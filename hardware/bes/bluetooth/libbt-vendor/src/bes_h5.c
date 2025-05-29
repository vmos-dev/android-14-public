#define LOG_TAG "bes_h5"
#include <utils/Log.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "bes_utils.h"
#include "bes_list.h"
#include "userial_vendor.h"

/* HCI data types */
#define HCI_COMMAND_PKT		0x01
#define HCI_ACLDATA_PKT		0x02
#define HCI_SCODATA_PKT		0x03
#define HCI_EVENT_PKT		0x04
#define HCI_DIAG_PKT		0xf0
#define HCI_VENDOR_PKT		0xff
#define HCI_3WIRE_ACK_PKT	0x00
#define HCI_3WIRE_LINK_PKT	0x0f

// HCI data types //
#define H5_RELIABLE_PKT         0x01
#define H5_UNRELIABLE_PKT       0x00

#define H5_EVENT_RX                    0x0001
#define H5_EVENT_EXIT                  0x0200

/*
 * Maximum Three-wire packet:
 *     4 byte header + max value for 12-bit length + 2 bytes for CRC
 */
#define H5_MAX_LEN (4 + 0x400 + 2)
#define H5_WINDOW_SIZE (7)

/* Convenience macros for reading Three-wire header values */
#define H5_HDR_SEQ(hdr)		((hdr)[0] & 0x07)
#define H5_HDR_ACK(hdr)		(((hdr)[0] >> 3) & 0x07)
#define H5_HDR_CRC(hdr)		(((hdr)[0] >> 6) & 0x01)
#define H5_HDR_RELIABLE(hdr)	(((hdr)[0] >> 7) & 0x01)
#define H5_HDR_PKT_TYPE(hdr)	((hdr)[1] & 0x0f)
#define H5_HDR_LEN(hdr)		((((hdr)[1] >> 4) & 0x0f) + ((hdr)[2] << 4))
#define H5_HDR_SIZE             4

#define SLIP_DELIMITER	0xc0
#define SLIP_ESC	0xdb
#define SLIP_ESC_DELIM	0xdc
#define SLIP_ESC_ESC	0xdd

/* H5 state flags */
enum {
    H5_RX_ESC,	/* SLIP escape mode */
    H5_TX_ACK_REQ,	/* Pending ack to send */
};

typedef enum H5_RX_STATE
{
    H5_W4_PKT_DELIMITER,
    H5_W4_PKT_START,
    H5_W4_HDR,
    H5_W4_DATA,
    H5_W4_CRC
} tH5_RX_STATE;

typedef enum H5_RX_ESC_STATE
{
    H5_ESCSTATE_NOESC,
    H5_ESCSTATE_ESC
} tH5_RX_ESC_STATE;

typedef struct bes_h5_packet{
    uint8_t index;
    uint8_t   seq_num;
    uint16_t data_len;
    uint8_t data[H5_MAX_LEN];
}bes_h5_packet_t;

struct bes_h5 {
    bes_list_t * unack;
    bes_list_t * free_packet_list;
    unsigned long flags;
    tH5_RX_STATE        rx_state;
    tH5_RX_ESC_STATE    rx_esc_state;
    uint32_t      rx_pending;	/* Expecting more bytes */
    uint8_t        rx_ack;		/* Last ack number received */    
    uint8_t		tx_seq;		/* Next seq number to send */
    uint8_t		rxseq_txack;/*expected rx SeqNumber*/
    bool           is_txack_req;
    bool           recv_valid_ack;
    bool           use_crc;
    uint16_t      message_crc;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    pthread_t       thread_data_retrans;
    bool           retrans_timer_enable;
    timer_t       retrans_timer;
    int (*rx_func)(struct bes_h5 * h5, uint8_t c);
    bes_h5_packet_t rx_packet;
    bes_h5_packet_t tx_packet;
    bool           h5_ack_enable;
}bes_h5_t;

#define H5_MAX_RETRY_COUNT (30)
#define H5_RETRANS_TIMEOUT_VALUE (50)

void h5_send_ack(void);
void h5_retransfer_signal_event(uint16_t event);
static void h5_retransfer_timeout_handler(union sigval sigev_value);
static int create_data_retransfer_thread();

static volatile uint8_t h5_retransfer_running = 0;
static volatile uint16_t h5_ready_events = 0;
static volatile uint8_t h5_data_ready_running = 0;
static struct bes_h5 st_bes_h5 = { 0};
static bes_h5_packet_t h5_packet[H5_WINDOW_SIZE];
static pthread_mutex_t h5_thread_tx_mutex = PTHREAD_MUTEX_INITIALIZER;

static const uint16_t crc_table[] =
{
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f
};

// bite reverse in bytes
// 00000001 -> 10000000
// 00000100 -> 00100000
const uint8_t byte_rev_table[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};


// Initialise the crc calculator
#define H5_CRC_INIT(x) x = 0xffff

// reverse bit
static __inline uint8_t bit_rev8(uint8_t byte)
{
    return byte_rev_table[byte];
}

// reverse bit
static __inline uint16_t bit_rev16(uint16_t x)
{
    return (bit_rev8(x & 0xff) << 8) | bit_rev8(x >> 8);
}


void h5_init(void)
{
    memset(&st_bes_h5, 0, sizeof(st_bes_h5));
    st_bes_h5.unack = bes_list_new(NULL);
    st_bes_h5.free_packet_list = bes_list_new(NULL);

    st_bes_h5.rx_state = H5_W4_PKT_DELIMITER;
    st_bes_h5.rx_esc_state = H5_ESCSTATE_NOESC;
    st_bes_h5.use_crc = true;
    st_bes_h5.recv_valid_ack = false;
    st_bes_h5.h5_ack_enable = true;

    st_bes_h5.retrans_timer = OsAllocateTimer(h5_retransfer_timeout_handler, &st_bes_h5);
    st_bes_h5.retrans_timer_enable = false;
    for(int i = 0; i < sizeof(h5_packet)/sizeof(bes_h5_packet_t); i ++){
        bes_list_append(st_bes_h5.free_packet_list, &h5_packet[i]);
    }
    create_data_retransfer_thread();
    ALOGD("h5_init:%x", (uint32_t)st_bes_h5.free_packet_list);
}

void h5_close(void)
{
    if(h5_retransfer_running){
        h5_retransfer_running = 0;
        h5_retransfer_signal_event(H5_EVENT_EXIT);
    }

    if(st_bes_h5.retrans_timer_enable){
        OsStopTimer(st_bes_h5.retrans_timer);
        st_bes_h5.retrans_timer_enable = false;
    }
    OsFreeTimer(st_bes_h5.retrans_timer);
    ALOGD("h5_close");
}

static inline void h5_init_packet(bes_h5_packet_t * h5_packet)
{
    h5_packet->data_len = 0;
}

static inline void h5_put_data_in_packet(bes_h5_packet_t * h5_packet, uint8_t data)
{
    h5_packet->data[h5_packet->data_len ++] = data;
}

static void h5_slip_delim(bes_h5_packet_t * h5_packet)
{
    const char delim = SLIP_DELIMITER;
    
    h5_packet->data[h5_packet->data_len ++] = delim;
}

static void h5_slip_one_byte(bes_h5_packet_t * h5_packet, uint8_t c)
{
    const char esc_delim[2] = { SLIP_ESC, SLIP_ESC_DELIM };
    const char esc_esc[2] = { SLIP_ESC, SLIP_ESC_ESC };

    switch (c) {
    case SLIP_DELIMITER:
        h5_packet->data[h5_packet->data_len ++] = esc_delim[0];
        h5_packet->data[h5_packet->data_len ++] = esc_delim[1];
        break;
    case SLIP_ESC:
        h5_packet->data[h5_packet->data_len ++] = esc_esc[0];
        h5_packet->data[h5_packet->data_len ++] = esc_esc[1];
        break;
    default:
        h5_packet->data[h5_packet->data_len ++] = c;
    }
}

/**
* Add "d" into crc scope, caculate the new crc value
*
* @param crc crc data
* @param d one byte data
*/
static void h5_crc_update(uint16_t *crc, uint8_t d)
{
    uint16_t reg = *crc;

    reg = (reg >> 4) ^ crc_table[(reg ^ d) & 0x000f];
    reg = (reg >> 4) ^ crc_table[(reg ^ (d >> 4)) & 0x000f];

    *crc = reg;
}

/**
* Get crc data.
*
* @param h5 bes h5 struct
* @return crc data
*/
static uint16_t h5_get_crc(struct bes_h5* h5)
{
   uint16_t crc = 0;
   uint8_t * data = h5->rx_packet.data + h5->rx_packet.data_len - 2;
   crc = data[1] + (data[0] << 8);
   return crc;
}

/**
* Decode one byte in h5 proto, as follows:
* 0xdb, 0xdc -> 0xc0
* 0xdb, 0xdd -> 0xdb
* 0xdb, 0xde -> 0x11
* 0xdb, 0xdf -> 0x13
* others will not change
*
* @param h5 bes h5 struct
* @byte pure data in the one byte
*/
static void h5_unslip_one_byte( struct bes_h5*h5, unsigned char byte)
{
    const uint8_t c0 = 0xc0, db = 0xdb;
    const uint8_t oof1 = 0x11, oof2 = 0x13;
    uint8_t *hdr = h5->rx_packet.data;

    if (H5_ESCSTATE_NOESC == h5->rx_esc_state)
    {
        if (0xdb == byte)
        {
            h5->rx_esc_state = H5_ESCSTATE_ESC;
        }
        else
        {
            h5_put_data_in_packet(&h5->rx_packet, byte);
            //Check Pkt Header's CRC enable bit
            if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
            {
                h5_crc_update(&h5->message_crc, byte);
            }
            h5->rx_pending--;
        }
    }
    else if(H5_ESCSTATE_ESC == h5->rx_esc_state)
    {
        switch (byte)
        {
        case 0xdc:
            h5_put_data_in_packet(&h5->rx_packet, c0);
            if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                h5_crc_update(&h5->message_crc, 0xc0);
            h5->rx_esc_state = H5_ESCSTATE_NOESC;
            h5->rx_pending--;
            break;
        case 0xdd:
            h5_put_data_in_packet(&h5->rx_packet, db);
             if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                h5_crc_update(&h5->message_crc, 0xdb);
            h5->rx_esc_state = H5_ESCSTATE_NOESC;
            h5->rx_pending--;
            break;
        case 0xde:
            h5_put_data_in_packet(&h5->rx_packet, oof1);
            if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                h5_crc_update(&h5->message_crc, oof1);
            h5->rx_esc_state = H5_ESCSTATE_NOESC;
            h5->rx_pending--;
            break;
        case 0xdf:
            h5_put_data_in_packet(&h5->rx_packet, oof2);
            if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                h5_crc_update(&h5->message_crc, oof2);
            h5->rx_esc_state = H5_ESCSTATE_NOESC;
            h5->rx_pending--;
            break;
        default:
            ALOGE("Error: Invalid byte %02x after esc byte", byte);
            h5_init_packet(&h5->rx_packet);
            h5->rx_state = H5_W4_PKT_DELIMITER;
            h5->rx_pending = 0;
            break;
        }
    }
}

/**
* Removed controller acked packet from Host's unacked lists
*
* @param h5 bes h5 struct
*/
static void h5_remove_acked_pkt(struct bes_h5* h5)
{
    bes_h5_packet_t * bes_packet = NULL;

    int pkts_to_be_removed = 0;
    int seqno = 0;
    int i = 0;
    bool valid_ack = false;
    int list_num = 0;

    seqno = h5->tx_seq;
    list_num = bes_list_length(h5->unack);

    ALOGD("h5_ack:%d %d %d", seqno, h5->rx_ack, list_num);
    while (pkts_to_be_removed < list_num)
    {
        // tx seq should increment first as saed tx_seq is not acked
        seqno = (seqno + 1) & 0x07;    
        pkts_to_be_removed ++;
        if (h5->rx_ack == seqno){
            valid_ack = true;
            break;
        }       
    }

    if (!valid_ack)
    {
        ALOGD("acked invalid packet");
    }

    if(valid_ack){
        // remove ack'ed packet from unack queue
        for(int i = 0; i < pkts_to_be_removed;  i ++){
            bes_packet = bes_list_front(h5->unack);
            bes_list_remove(h5->unack, bes_packet);
            h5_init_packet(&h5->rx_packet);
            bes_list_append(h5->free_packet_list, bes_packet);
            //should increment TX start seq number 
            ++(h5->tx_seq);
            h5->tx_seq = (h5->tx_seq) & 0x07;
        }
    }

    if(bes_list_length(h5->unack) == 0){
        //should stop retransmit timer
        st_bes_h5.retrans_timer_enable = false;
        OsStopTimer(st_bes_h5.retrans_timer);
    }

    if (valid_ack)
    {
        ALOGD("Removed only (%u) out of (%u) pkts", i, pkts_to_be_removed);
    }
}

static uint8_t h5_complete_rx_pkt(struct bes_h5*h5)
{
    int pass_up = 1;
    uint16_t eventtype = 0;
    uint16_t payload_len = 0;
    uint8_t *h5_hdr = NULL;
    uint8_t pkt_type = 0;
    uint8_t status = 0;

    pthread_mutex_lock(&h5_thread_tx_mutex);
    h5_hdr = h5->rx_packet.data;
    ALOGD("h5_complete_rx_pkt seq(%d), ack(%d)", H5_HDR_SEQ(h5_hdr), H5_HDR_ACK(h5_hdr));

    if (H5_HDR_RELIABLE(h5_hdr))
    {
        ALOGD("Received reliable seqno %u from card", h5->rxseq_txack);

        h5->rxseq_txack = H5_HDR_SEQ(h5_hdr) + 1;
        h5->rxseq_txack %= 8;
        h5->is_txack_req = true;
        h5_send_ack();
    }

    payload_len = H5_HDR_LEN(h5_hdr);
    h5->rx_ack = H5_HDR_ACK(h5_hdr);
    pkt_type = H5_HDR_PKT_TYPE(h5_hdr);
    st_bes_h5.recv_valid_ack = true;
    switch (pkt_type)
    {
        case HCI_ACLDATA_PKT:
            pass_up = 1;
        break;

        case HCI_EVENT_PKT:
            pass_up = 1;
            break;

        case HCI_SCODATA_PKT:
            pass_up = 1;
            break;
        case HCI_COMMAND_PKT:
            pass_up = 1;
            break;

        case HCI_3WIRE_LINK_PKT:
            pass_up = 0;
        break;

        case HCI_3WIRE_ACK_PKT:
            pass_up = 0;
            break;

        default:
          ALOGE("Unknown pkt type(%d)", H5_HDR_PKT_TYPE(h5_hdr));
          pass_up = 0;
          break;
    }

    // remove h5 header and send packet to hci
    if(st_bes_h5.h5_ack_enable)
        h5_remove_acked_pkt(h5);
    pthread_mutex_unlock(&h5_thread_tx_mutex);
    if(pass_up){
        userial_recv_uart_rawdata(h5->rx_packet.data + H5_HDR_SIZE, payload_len);
    }
    
    h5_init_packet(&h5->rx_packet);
    h5->rx_state = H5_W4_PKT_DELIMITER;   
    return pkt_type;
}

/**
* Parse the receive data in h5 proto.
*
* @param h5 bes h5 struct
* @param data point to data received before parse
* @param count num of data
* @return reserved count
*/
bool bes_h5_recv(uint8_t *data, int count)
{
    uint8_t *ptr;
    uint8_t *hdr = NULL;
    bool complete_packet = false;
    struct bes_h5*h5 = &st_bes_h5;

    ptr = (uint8_t *)data;

    while(count){
        if(h5->rx_pending > 0){
            if (*ptr == SLIP_DELIMITER)
            {
                ALOGE("short h5 packet");
                h5_init_packet(&h5->rx_packet);
                h5->rx_state = H5_W4_PKT_START;
                h5->rx_pending = 0;
            } else
                h5_unslip_one_byte(h5, *ptr);

            ptr++; count--;
            continue;
        }
        
        //H5LogMsg("h5_recv rx_state=%d", h5->rx_state);
        switch (h5->rx_state)
        {
        case H5_W4_HDR:
            // check header checksum. see Core Spec V4 "3-wire uart" page 67
            hdr = h5->rx_packet.data;

            if ((0xff & (uint8_t) ~ (hdr[0] + hdr[1] +
                                   hdr[2])) != hdr[3])
            {
                ALOGE("h5 hdr checksum error!!!");
                h5_init_packet(&h5->rx_packet);
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_pending = 0;
                continue;
            }

            if (H5_HDR_RELIABLE(hdr)
                && (H5_HDR_SEQ(hdr) != h5->rxseq_txack))
            {
                ALOGE("Out-of-order packet arrived, got(%u)expected(%u)",
                   H5_HDR_SEQ(hdr), h5->rxseq_txack);
                h5->is_txack_req = true;

                h5_init_packet(&h5->rx_packet);
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_pending = 0;

                continue;
            }
            h5->rx_state = H5_W4_DATA;
            //payload length: May be 0
            h5->rx_pending = H5_HDR_LEN(hdr);
            continue;
        case H5_W4_DATA:
            hdr = h5->rx_packet.data;;
            if (H5_HDR_CRC(hdr))
            {   // pkt with crc /
                h5->rx_state = H5_W4_CRC;
                h5->rx_pending = 2;
            }
            else
            {
                //no crc complete packet
                h5_complete_rx_pkt(h5); //Send ACK
                complete_packet = true;
                h5->rx_pending = 0;
                h5->rx_state = H5_W4_PKT_DELIMITER;
                ALOGD("--------> H5_W4_DATA ACK\n");
            }
            continue;

        case H5_W4_CRC:
            if (bit_rev16(h5->message_crc) != h5_get_crc(h5))
            {
                ALOGE("Checksum failed, computed(%04x)received(%04x)",
                    bit_rev16(h5->message_crc), h5_get_crc(h5));
                h5_init_packet(&h5->rx_packet);
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_pending = 0;
                continue;
            }
            //remove crc
            h5->rx_packet.data_len -= 2;
            h5_complete_rx_pkt(h5);
            complete_packet = true;
            continue;

        case H5_W4_PKT_DELIMITER:
            switch (*ptr)
            {
                case SLIP_DELIMITER:
                    h5->rx_state = H5_W4_PKT_START;
                    break;
                default:
                    break;
            }
            ptr++; count--;
            break;

        case H5_W4_PKT_START:
            switch (*ptr)
            {
                case SLIP_DELIMITER:
                    ptr++; count--;
                    break;
                default:
                    h5->rx_state = H5_W4_HDR;
                    h5->rx_pending = 4;
                    h5->rx_esc_state = H5_ESCSTATE_NOESC;
                    H5_CRC_INIT(h5->message_crc);
                    break;
            }
            break;
        }
    }

    return complete_packet;
}

/**
* Prepare h5 packet, packet format as follow:
*  | LSB 4 octets  | 0 ~1024| 2 MSB
*  |packet header | payload | data integrity check |
*
* pakcket header fromat is show below:
*  | LSB 3 bits         | 3 bits             | 1 bits                       | 1 bits          |
*  | 4 bits     | 12 bits        | 8 bits MSB
*  |sequence number | acknowledgement number | data integrity check present | reliable packet |
*  |packet type | payload length | header checksum
*
* @param h5 bes h5 struct
* @param data_out  h5 data to send
* @param data pure data
* @param len the length of data
* @param pkt_type packet type
* @return length of whole packet should send to btc
*/
static uint32_t h5_prepare_pkt(bes_h5_packet_t *tx_packet, uint8_t *data, signed long len, uint8_t pkt_type, uint8_t is_reliable)
{
    uint8_t hdr[4] = {0}; 
    uint16_t H5_CRC_INIT(h5_txmsg_crc);
    int i;
    struct bes_h5*h5 = &st_bes_h5;

    switch (pkt_type)
    {
        case HCI_ACLDATA_PKT:
        case HCI_COMMAND_PKT:
        case HCI_EVENT_PKT:
        case HCI_3WIRE_ACK_PKT:
        case HCI_VENDOR_PKT:
        case HCI_3WIRE_LINK_PKT:
        break;
        default:
        ALOGE("Unknown packet type");
        return 0;
    }
    h5_init_packet(tx_packet);
    //Add SLIP start byte: 0xc0
    tx_packet->data[0] = SLIP_DELIMITER;
    tx_packet->data_len = 1;
    // set AckNumber in SlipHeader
    hdr[0] = h5->rxseq_txack << 3;
    h5->is_txack_req = false;

    ALOGD("Sending packet with seqno %u and wait %u pkt_len:%d", h5->tx_seq, h5->rxseq_txack, (int)len);
    if (H5_RELIABLE_PKT == is_reliable)
    {
        // set reliable pkt bit and SeqNumber
        hdr[0] |= 0x80 + h5->tx_seq;
        tx_packet->seq_num = h5->tx_seq;
        ALOGD("Sending packet with seqno(%u)", h5->tx_seq);
        ++(h5->tx_seq);
        h5->tx_seq = (h5->tx_seq) & 0x07;
    }

    // set DicPresent bit
    if (h5->use_crc)
        hdr[0] |= 0x40;

    // set packet type and payload length
    hdr[1] = ((len << 4) & 0xff) | pkt_type;
    hdr[2] = (uint8_t)(len >> 4);
    // set checksum
    hdr[3] = ~(hdr[0] + hdr[1] + hdr[2]);

    // Put h5 header */
    for (i = 0; i < 4; i++)
    {
        h5_slip_one_byte(tx_packet, hdr[i]);

        if (h5->use_crc)
            h5_crc_update(&h5_txmsg_crc, hdr[i]);
    }

    // Put payload */
    for (i = 0; i < len; i++)
    {
        h5_slip_one_byte(tx_packet, data[i]);

       if (h5->use_crc)
       h5_crc_update(&h5_txmsg_crc, data[i]);
    }

    // Put CRC */
    if (h5->use_crc)
    {
        h5_txmsg_crc = bit_rev16(h5_txmsg_crc);
        h5_slip_one_byte(tx_packet, (uint8_t) ((h5_txmsg_crc >> 8) & 0x00ff));
        h5_slip_one_byte(tx_packet, (uint8_t) (h5_txmsg_crc & 0x00ff));
    }

    // Add SLIP end byte: 0xc0
    tx_packet->data[tx_packet->data_len ++] = SLIP_DELIMITER; 
    return tx_packet->data_len;
}

void h5_send_ack(void)
{
    if(st_bes_h5.is_txack_req){
        st_bes_h5.is_txack_req = false;
        h5_prepare_pkt(&st_bes_h5.tx_packet, NULL, 0, HCI_3WIRE_ACK_PKT, H5_UNRELIABLE_PKT);
        userial_vendor_transmit_data_to_btc(st_bes_h5.tx_packet.data, st_bes_h5.tx_packet.data_len);
    }
}

uint16_t h5_transmit_data_to_btc(uint8_t *data, uint16_t total_length)
{
    uint8_t hci_type = data[0];
    uint8_t pkt_type = H5_UNRELIABLE_PKT;
    bes_h5_packet_t* h5_packet = NULL;
    bes_h5_packet_t* tx_packet = NULL;
    uint32_t transmit_length = 0;

    if(st_bes_h5.h5_ack_enable){
        switch(hci_type){
            case HCI_COMMAND_PKT:
            case HCI_ACLDATA_PKT:
            {
                pkt_type = H5_RELIABLE_PKT;
                break;
            }

            case HCI_3WIRE_ACK_PKT:
            {
                pkt_type = H5_UNRELIABLE_PKT;
                break;
            }
                
            default:
                ASSERT(0);
                break;
        }
    }

    pthread_mutex_lock(&h5_thread_tx_mutex);
    if(pkt_type == H5_RELIABLE_PKT){
        uint32_t list_num = bes_list_length(st_bes_h5.free_packet_list);
        ALOGD("st_bes_h5.free_packet_list:%x free num:%d", (uint32_t)st_bes_h5.free_packet_list, list_num);
        if(bes_list_length(st_bes_h5.free_packet_list) == 0){
            ALOGE(0, "h5 free list full");
            pthread_mutex_unlock(&h5_thread_tx_mutex);
            return 0;
        }
        
        h5_packet = (bes_h5_packet_t*)bes_list_front(st_bes_h5.free_packet_list);
        bes_list_remove(st_bes_h5.free_packet_list, h5_packet);
        tx_packet = h5_packet;
         //save raw data for re-send
        tx_packet->data_len = total_length;
        memcpy(tx_packet->data, data, total_length);
        bes_list_append(st_bes_h5.unack, tx_packet);
        h5_retransfer_signal_event(H5_EVENT_RX);    
    }
    else{
        //unreliable packet send directly
        tx_packet = &st_bes_h5.tx_packet;
        h5_prepare_pkt(tx_packet, data, total_length, hci_type, H5_UNRELIABLE_PKT);
        transmit_length = userial_vendor_transmit_data_to_btc(tx_packet->data, tx_packet->data_len);
    }
    pthread_mutex_unlock(&h5_thread_tx_mutex);

    return total_length;
}

void h5_retransfer_signal_event(uint16_t event)
{
    h5_ready_events |= event;
    pthread_cond_signal(&st_bes_h5.cond);
}

static void data_retransfer_thread(void *arg)
{
    uint16_t events;
    bes_list_node_t * list_node = NULL;
    bes_h5_packet_t* tx_packet = NULL;
    uint16_t tx_seq = 0;

    ALOGD("data_retransfer_thread create");
    while (h5_retransfer_running)
    {
        pthread_mutex_lock(&st_bes_h5.mutex);
        while (h5_ready_events == 0)
        {
            pthread_cond_wait(&st_bes_h5.cond, &st_bes_h5.mutex);
        }
        events = h5_ready_events;
        h5_ready_events = 0;
        pthread_mutex_unlock(&st_bes_h5.mutex);

        if (events & H5_EVENT_RX){
            uint32_t send_count = 0;
             pthread_mutex_lock(&h5_thread_tx_mutex);
             //save tx_seq number
             tx_seq = st_bes_h5.tx_seq;
             for(list_node = bes_list_begin(st_bes_h5.unack); list_node != bes_list_end(st_bes_h5.unack); list_node = bes_list_next(list_node)){
                tx_packet = (bes_h5_packet_t*)list_node->data;
                h5_prepare_pkt(&st_bes_h5.tx_packet, tx_packet->data, tx_packet->data_len, tx_packet->data[0], H5_RELIABLE_PKT);
                userial_vendor_transmit_data_to_btc(st_bes_h5.tx_packet.data, st_bes_h5.tx_packet.data_len);
                send_count ++;
                if(st_bes_h5.recv_valid_ack){
                    st_bes_h5.recv_valid_ack = false;
                    break;
                }
             }
             //restore tx_seq
             st_bes_h5.tx_seq = tx_seq;
             pthread_mutex_unlock(&h5_thread_tx_mutex);
             if(!st_bes_h5.retrans_timer_enable && (send_count > 0)){
                st_bes_h5.retrans_timer_enable = true;
                OsStartTimer(st_bes_h5.retrans_timer, H5_RETRANS_TIMEOUT_VALUE, 0);
             }
             ALOGD("re-send count:%d", send_count);
        }
        else  if (events & H5_EVENT_EXIT){
            break;
        }
    }
    
    pthread_mutex_destroy(&st_bes_h5.mutex);
    pthread_cond_destroy(&st_bes_h5.cond);    
    ALOGD("data_retransfer_thread exiting");
    h5_retransfer_running = 0;
    pthread_exit(NULL);    
}

static int create_data_retransfer_thread()
{
    pthread_attr_t thread_attr;

    if (h5_retransfer_running)
    {
        ALOGW("create_data_retransfer_thread has been called repeatedly without calling cleanup ?");
    }

    h5_retransfer_running = 1;
    h5_ready_events = 0;

    pthread_attr_init(&thread_attr);
    pthread_mutex_init(&st_bes_h5.mutex, NULL);
    pthread_cond_init(&st_bes_h5.cond, NULL);

    if (pthread_create(&st_bes_h5.thread_data_retrans, &thread_attr, \
               (void*)data_retransfer_thread, NULL) != 0)
    {
        ALOGE("pthread_create thread_data_retrans failed!");
        h5_retransfer_running = 0;
        return -1 ;
    }

    return 0;
}

static void h5_retransfer_timeout_handler(union sigval sigev_value)
{
    ALOGD("h5_retransfer_timeout_handler");
    st_bes_h5.retrans_timer_enable = false;
    h5_retransfer_signal_event(H5_EVENT_RX);
}

