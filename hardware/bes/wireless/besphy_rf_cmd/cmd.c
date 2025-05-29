/*
 * Command implementations
 */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "besphy_rf.h"


#define U8_MAX  255
#define U16_MAX 65535


/* rf cmd length cannot exceed 640 bytes */
#define RF_LEN_MAX 640
struct vendor_rf_cmd_t {
    u32 cmd_type;
    u32 cmd_argc;
    u32 cmd_len;
    u8  cmd[0];
};

/* ID - BES_MSG_TEST */
struct bes_msg_test_t {
    int dummy;
};

/**
 * ID -BES_MSG_SET_SNAP_FRAME
 * bes_msg_set_snap_frame - set SNAP frame format
 * @len: length of SNAP frame, if 0 SNAP frame disabled
 * @frame: SNAP frame format
 *
 * In this structure is difference between user space because
 * format and length have to be hidden
 *
 */
struct bes_msg_set_snap_frame_t {
    u8 len;
    u8 frame[0];
};

struct nl_data {
    size_t   d_size;
    void    *d_data;
};

/* ID - BES_MSG_TEST */
struct advance_scan_elems {
    u8 scanMode;
    u16 duration;
};


/**
 * ID - BES_MSG_SET_TX_QUEUE_PARAMS
 * bes_msg_set_txqueue_params - store Tx queue params
 * @user_priority: User priority for which TSPEC negotiated
 * @medium_time: Allowed medium time
 * @expiry_time: The expiry time of MSDU
 *
 */
struct bes_msg_set_txqueue_params {
    u8 user_priority;
    u16 medium_time;
    u16 expiry_time;
};

/**
 * ID - BES_MSG_START_STOP_TSM
 * bes_msg_set_start_stop_tsm - To start or stop collecting TSM metrics in
 * bes2600 driver
 * @start: To start or stop collecting TSM metrics
 * @up: up for which metrics to be collected
 * @packetization_delay: Packetization period for this TID
 *
 */
struct bes_msg_start_stop_tsm_t {
    u8 start;	/*1: To start, 0: To stop*/
    u8 up;
    u16 packetization_delay;
};

/**
 * ID - BES_MSG_SET_POWER_SAVE
 * power_save_elems - To enable/disable legacy power Save
 */
struct power_save_elems {
    int powerSave;
};

/**
 * add_ip_offload_t - Parameters related to tcp alive: port, payload length, payload
 */
struct add_ip_offload_t {
    uint8_t proto;
    uint16_t dest_port;
    uint16_t payload_len;
    uint8_t payload[0];
};

/**
 * ip_alive_iac_idx - idx of tcp & udp alive stream
 */
struct ip_alive_iac_idx {
    int idx;
};

/**
 * ip_alive_period - tcp & udp alive period
 */
struct ip_alive_period {
    int period;
};

/**
 * bes_tsm_stats - To retrieve the Transmit Stream Measurement stats
 * @actual_msrmt_start_time: The TSF at the time at which the measurement
 * started
 * @msrmt_duration: Duration for measurement
 * @peer_sta_addr: Peer STA address
 * @tid: TID for which measurements were made
 * @reporting_reason: Reason for report sent
 * @txed_msdu_count: The number of MSDUs transmitted for the specified TID
 * @msdu_discarded_count: The number of discarded MSDUs for the specified TID
 * @msdu_failed_count: The number of failed MSDUs for the specified TID
 * @multi_retry_count: The number of MSDUs which were retried
 * @qos_cfpolls_lost_count: The number of QOS CF polls frames lost
 * @avg_q_delay: Average queue delay
 * @avg_transmit_delay: Average transmit delay
 * @bin0_range: Delay range of the first bin (Bin 0)
 * @bin0: bin0 transmit delay histogram
 * @bin1: bin1 transmit delay histogram
 * @bin2: bin2 transmit delay histogram
 * @bin3: bin3 transmit delay histogram
 * @bin4: bin4 transmit delay histogram
 * @bin5: bin5 transmit delay histogram
 *
 */
struct bes_tsm_stats {
    u64 actual_msrmt_start_time;
    u16 msrmt_duration;
    u8 peer_sta_addr[6];
    u8 tid;
    u8 reporting_reason;
    u32 txed_msdu_count;
    u32 msdu_discarded_count;
    u32 msdu_failed_count;
    u32 multi_retry_count;
    u32 qos_cfpolls_lost_count;
    u32 avg_q_delay;
    u32 avg_transmit_delay;
    u8 bin0_range;
    u32 bin0;
    u32 bin1;
    u32 bin2;
    u32 bin3;
    u32 bin4;
    u32 bin5;
} __packed;

struct wsm_tx_power_range {
    int min_power_level;
    int max_power_level;
    u32 stepping;
};

struct vendor_rf_cmd_reply {
    u32 id;
    u32 len;
    char msg[0];
};

struct wifi_power_cali_save_t {
    u16 mode;
    u16 band;
    u16 ch;
    u16 power_cali;
    u16 status; /* 0: fail, 1: success */
};

struct wifi_freq_cali_save_t {
    u16 freq_cali;
    u16 status; /* 0: fail, 1: success */
};

/* code to do everything */

static void print_dump(const char *fmt, unsigned int size,
                       unsigned int count, const void *buffer)
{
    int i = 0;
    int rowsize = 16;

    switch(size) {
    case sizeof(uint32_t):
        while(i < count) {
            printf(fmt, *(uint32_t *)((uint32_t *)buffer + i));
            if (i % rowsize == rowsize - 1)
                printf("\n");
            i++;
        }
        break;
    case sizeof(uint16_t):
        while(i < count) {
            printf(fmt, *(uint16_t *)((uint16_t *)buffer + i));
            if (i % rowsize == rowsize - 1)
                printf("\n");
            i++;
        }
        break;
    case sizeof(uint8_t):
    default:
        while(i < count) {
            printf(fmt, *(uint8_t *)((uint8_t *)buffer + i));
            if (i % rowsize == rowsize - 1)
                printf("\n");
            i++;
        }
        break;
    }
    printf("\n");
}

static int print_recvmsgs(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 2];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));  /* Gets the payload of the msg */
    struct nlattr *td[BESPHY_TM_ATTR_MAX + 2];
    struct bes_tsm_stats tsm_stats;
    struct wsm_tx_power_range  power_range[2];
    int cmd_id = *((int *)arg);
    struct vendor_rf_cmd_reply *rf_cmd_msg;
    struct wifi_power_cali_save_t *wifi_power_cali;
    struct wifi_freq_cali_save_t *wifi_freq_cali;

    /* get reply message for all nlmsg */
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb[NL80211_ATTR_TESTDATA] || !tb[NL80211_ATTR_WIPHY]) {
        printf("no data!\n");
        return NL_SKIP;
    }

    /* get reply message for rf test cmd */
    nla_parse(td, BESPHY_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
          nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

    if (!td[BESPHY_TM_MSG_DATA]) {
        printf("no recvmsgs info\n");
        return NL_SKIP;
    }

    switch (cmd_id) {
    case BES_MSG_TEST:
        printf("\nphy#%d, the cmd msg_test recvmsgs is:\ndummy = %d\n\n",
        nla_get_u32(tb[NL80211_ATTR_WIPHY]),
        (*((int *)nla_data(td[BESPHY_TM_MSG_DATA]))));
        break;
    case BES_MSG_GET_TX_POWER_LEVEL:
        printf("\nphy#%d, the cmd get_tx_power_level recvmsgs is:\npower = %d\n\n",
        nla_get_u32(tb[NL80211_ATTR_WIPHY]),
        (*((int *)nla_data(td[BESPHY_TM_MSG_DATA]))));
        break;
    case BES_MSG_GET_TX_POWER_RANGE:
        memcpy(power_range, (struct wsm_tx_power_range *)nla_data(td[BESPHY_TM_MSG_DATA]),
        sizeof(power_range));
        printf("\nphy#%d, the cmd get_tx_power_range recvmsgs is:\
        \ntxPowerRange[0]:\nmin_power_level = %d, max_power_level = %d, stepping = %u\n",
        nla_get_u32(tb[NL80211_ATTR_WIPHY]),
        power_range[0].min_power_level, power_range[0].max_power_level,
        power_range[0].stepping);
        printf("txPowerRange[1]:\nmin_power_level = %d, max_power_level = %d, stepping = %u\n",
        power_range[1].min_power_level,
        power_range[1].max_power_level, power_range[1].stepping);
        break;
    case BES_MSG_GET_TSM_PARAMS:
        memcpy(&tsm_stats, (struct bes_tsm_stats *)nla_data(td[BESPHY_TM_MSG_DATA]), sizeof(tsm_stats));
        printf("\nphy#%d, the cmd get_tsm_params recvmsgs is:\n",
        nla_get_u32(tb[NL80211_ATTR_WIPHY]));
        #if __WORDSIZE == 64
        printf("actual_msrmt_start_time = %lu\n", tsm_stats.actual_msrmt_start_time);
        #else
        printf("actual_msrmt_start_time = %llu\n", tsm_stats.actual_msrmt_start_time);
        #endif
        printf("msrmt_duration = %d\n"
        "peer_sta_addr = %02x:%02x:%02x:%02x:%02x:%02x\ntid = %d\n"
        "reporting_reason = %d\ntxed_msdu_count = %u\n"
        "msdu_discarded_count = %u\nmsdu_failed_count = %u\n"
        "multi_retry_count = %u\nqos_cfpolls_lost_count = %u\n"
        "avg_q_delay = %u\navg_transmit_delay = %u\n"
        "bin0_range = %d\nbin0 = %u\nbin1 = %u\nbin1 = %u\n"
        "bin3 = %u\nbin4 = %u\nbin5 = %u\n\n",
        tsm_stats.msrmt_duration, tsm_stats.peer_sta_addr[0], tsm_stats.peer_sta_addr[1],
        tsm_stats.peer_sta_addr[2], tsm_stats.peer_sta_addr[3], tsm_stats.peer_sta_addr[4],
        tsm_stats.peer_sta_addr[5], tsm_stats.tid, tsm_stats.reporting_reason,
        tsm_stats.txed_msdu_count, tsm_stats.msdu_discarded_count, tsm_stats.msdu_failed_count,
        tsm_stats.multi_retry_count, tsm_stats.qos_cfpolls_lost_count, tsm_stats.avg_q_delay,
        tsm_stats.avg_transmit_delay, tsm_stats.bin0_range, tsm_stats.bin0, tsm_stats.bin1,
        tsm_stats.bin2, tsm_stats.bin3, tsm_stats.bin4, tsm_stats.bin5);
        break;
    case BES_MSG_GET_ROAM_DELAY:
        printf("\nphy#%d, the cmd get_roam_delay recvmsgs is:\nroam_delay = %d\n\n",
        nla_get_u32(tb[NL80211_ATTR_WIPHY]),
        (*((int *)nla_data(td[BESPHY_TM_MSG_DATA]))));
        break;
    case BES_MSG_ADD_IP_OFFLOAD:
        printf("\nphy#%d, the keep_alive_cfg recvmsgs is:\nkeep_alive_iac_idx = %d\n\n",
        nla_get_u32(tb[NL80211_ATTR_WIPHY]),
        (*((int *)nla_data(td[BESPHY_TM_MSG_DATA]))));
        break;
    case BES_MSG_VENDOR_RF_CMD:
        rf_cmd_msg = (struct vendor_rf_cmd_reply *)nla_data(td[BESPHY_TM_MSG_DATA]);
        switch (rf_cmd_msg->id) {
        case VENDOR_RF_SIGNALING_CMD:
            printf("\nphy#%d, the vendor signaling cmd recvmsgs is:\n%s\n\n",
            nla_get_u32(tb[NL80211_ATTR_WIPHY]), rf_cmd_msg->msg);
            break;
        case VENDOR_RF_NOSIGNALING_CMD:
            printf("\nphy#%d, the rf cmd recvmsgs is:\n%s\n\n",
            nla_get_u32(tb[NL80211_ATTR_WIPHY]), rf_cmd_msg->msg);
            break;
        case VENDOR_RF_SAVE_CMD:
            printf("\nphy#%d, save success.\n\n",
            nla_get_u32(tb[NL80211_ATTR_WIPHY]));
            break;
        case VENDOR_RF_GET_SAVE_FREQOFFSET_CMD:
            printf("\nphy#%d, freq clai: 0x%04x\n\n",
            nla_get_u32(tb[NL80211_ATTR_WIPHY]),
            ((uint16_t *)rf_cmd_msg->msg)[0]);
            break;
        case VENDOR_RF_GET_SAVE_POWERLEVEL_CMD:
            printf("\nphy#%d, power cali 2g:\n", nla_get_u32(tb[NL80211_ATTR_WIPHY]));
            print_dump("0x%04x ", 2, 3, (uint16_t *)rf_cmd_msg->msg);
            printf("power cali 5g:\n");
            print_dump("0x%04x ", 2, 13, (uint16_t *)rf_cmd_msg->msg + 3);
            break;
        case VENDOR_RF_SAVE_FREQOFFSET_CMD:
            wifi_freq_cali = (struct wifi_freq_cali_save_t *)(rf_cmd_msg->msg);
            printf("\nphy#%d, freqOffset save msg:\n",
            nla_get_u32(tb[NL80211_ATTR_WIPHY]));
            if (wifi_freq_cali->status)
                printf("success to save freq cali.\n");
            else
                printf("fail to save freq cali.\n");
            printf("freq cali = 0x%04x\n", wifi_freq_cali->freq_cali);
            break;
        case VENDOR_RF_SAVE_POWERLEVEL_CMD:
            wifi_power_cali = (struct wifi_power_cali_save_t *)(rf_cmd_msg->msg);
            printf("\nphy#%d, powerlevel save msg:\n",
            nla_get_u32(tb[NL80211_ATTR_WIPHY]));
            if (wifi_power_cali->status)
                printf("success to save power cali.\n");
            else
                printf("fail to save power cali.\n");
            printf("mode = %u, band = %u, ch = %u, powerlevel = 0x%04x\n",
                    wifi_power_cali->mode, wifi_power_cali->band,
                    wifi_power_cali->ch, wifi_power_cali->power_cali);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return NL_SKIP;
}

/**
 * rf_itoa() converts rf cmd related numbers to characters.
 * NUM_BIT represents the number of bits occupied
 * by cmd_num and cmd_len after they are converted into strings.
 */
#if 0
#define NUM_BIT  3
static int rf_itoa(char *rf_cmd, int num)
{
    int i, j;
    int len;
    char tmp[NUM_BIT + 1];
    if (num > pow(10, NUM_BIT) - 1 || num < 1)
        return -1;
    sprintf(tmp, "%d", num);
    len = strlen(tmp);
    if (len > NUM_BIT || len < 1)
        return -1;
    for (i = 0; i < NUM_BIT -len; i++) {
        rf_cmd[i] = '0';
    }
    for (j = 0; i < NUM_BIT && j < len; i++, j++) {
        rf_cmd[i] = tmp[j];
    }
    return 0;
}
#endif


/* Merge all parameters into one pointer. */
static void str_join(u8 *cat_cmd, const int argc, char **argv)
{
    char *argv_tmp;
    int i;
    for (i = 0; i < argc; argv++, i++) {
        argv_tmp = *argv;
        while (*argv_tmp != '\0') {
            *cat_cmd++ = *argv_tmp++;
        }
        if (i + 1 == argc) {
            *cat_cmd = '\0';
        } else {
            *cat_cmd++ = ' ';
        }
    }
}

/* Gets the length of all parameters. */
static u32 get_cmd_len(const int argc, char **argv)
{
    int i;
    u32 ret = 0;
    if (argc <= 1 || !argv)
        return 0;
    for (i = 0; i < argc; argv++, i++) {
        if (*argv != NULL) {
            ret += strlen(*argv) + 1;
        } else {
            return 0;
        }
    }
    return ret;
}


static int do_vendor_rf_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                char **argv)
{
    u32 cmd_len;
    static int id_type = BES_MSG_VENDOR_RF_CMD;

    cmd_len = get_cmd_len(argc, argv);
    if (!cmd_len || cmd_len > RF_LEN_MAX) {
        goto invalid_para;
    }

    struct vendor_rf_cmd_t *vendor_cmd = (struct vendor_rf_cmd_t *) malloc(cmd_len + 3 * sizeof(u32));
    if (!vendor_cmd) {
        return -ENOMEM;
    }

    if (!strcmp(argv[1], "save"))
        vendor_cmd->cmd_type = VENDOR_RF_SAVE_CMD;
    else if (!strcmp(argv[1], "get_save_freqOffset"))
        vendor_cmd->cmd_type = VENDOR_RF_GET_SAVE_FREQOFFSET_CMD;
    else if (!strcmp(argv[1], "get_save_powerlevel"))
        vendor_cmd->cmd_type = VENDOR_RF_GET_SAVE_POWERLEVEL_CMD;
    else if (!strcmp(argv[1], "save_powerlevel"))
        vendor_cmd->cmd_type = VENDOR_RF_SAVE_POWERLEVEL_CMD;
    else if (!strcmp(argv[1], "save_freqOffset"))
        vendor_cmd->cmd_type = VENDOR_RF_SAVE_FREQOFFSET_CMD;
    else
        /* distinguish between signaling or non-signaling in the driver.*/
        vendor_cmd->cmd_type = VENDOR_RF_SIG_NOSIG_MIX;

    vendor_cmd->cmd_argc = argc;
    vendor_cmd->cmd_len = cmd_len;
    str_join(vendor_cmd->cmd, argc, argv);

    struct nl_data *data_vendor_cmd = (struct nl_data *) malloc(sizeof(struct nl_data));
    if (!data_vendor_cmd) {
        return -ENOMEM;
    }
    data_vendor_cmd->d_size = cmd_len + 3 * sizeof(u32);
    data_vendor_cmd->d_data = vendor_cmd;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, id_type);
    NLA_PUT_DATA(msg, BESPHY_TM_MSG_DATA, data_vendor_cmd);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    free(vendor_cmd);
    vendor_cmd = NULL;
    free(data_vendor_cmd);
    data_vendor_cmd = NULL;

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

/**
 * Convert integer numeric string to a int number,
 * otherwise return false.
 * The number of converted numbers does not exceed the size range of INT.
 */
#define ATOI_MAX_BIT 10
static bool msg_tsm_atoi(const char *str, int *ret)
{
    if (!str || strlen(str) == 0)
        return false;

    int i, j;
    int len, sum_tmp;
    bool negative = false;
    int inx_end = 0;
    unsigned int sum = 0;
    unsigned int num_max = pow(2, 31) - 1;
    len = strlen(str);

    if (str[0] == '-') {
        if (len > 1) {
            negative = true;
            inx_end = 1;
            num_max++;
        } else {
            return false;
        }
    }

    int multiple[ATOI_MAX_BIT];
    multiple[ATOI_MAX_BIT - 1] = 1;
    for (i = ATOI_MAX_BIT -2; i >= 0; i--) {
        multiple[i] = 10 * multiple[i + 1];
    }

    for (i = len - 1, j = ATOI_MAX_BIT -1; i >= inx_end && j >= 0; i--, j--) {
        if (str[i] >= '0' && str[i] <= '9') {
            sum_tmp = (str[i] - '0') * multiple[j];
            if (sum <= sum + sum_tmp && sum + sum_tmp <= num_max)
                sum += sum_tmp;
            else
                return false;
        } else {
            return false;
        }
    }

    if (negative) {
        sum *= -1;
    }

    if (i >= inx_end) {
        return false;
    } else {
        *ret = sum;
        return true;
    }
}

static int do_msg_test_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                           char **argv)
{
    if (argc != 3) {
        goto invalid_para;
    }

    int dummy_tmp;
    static int id_type = BES_MSG_TEST;
    struct bes_msg_test_t msg_test;
    if (!msg_tsm_atoi(argv[2], &dummy_tmp))
        goto invalid_para;

    msg_test.dummy = dummy_tmp;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_TEST);
    NLA_PUT_TYPE(msg, struct bes_msg_test_t, BESPHY_TM_MSG_DATA, msg_test);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

/**
 * Converts hexadecimal strings to a single byte.
 * The input format is, and each parameter contains one
 * or two hexadecimal characters.
 * such as 25 ab Ac AF 2 3 36
 */
static bool hex_str_to_byte(int argc, char **argv, unsigned char *ret)
{
    if (!ret)
        return false;

    int i, len;
    for (i = 0; i < argc; i++) {
        if (!argv[i])
            return false;

        len = strlen(argv[i]);
        if (len > 2 || len < 1)
            return false;

        /* convert lowercase to uppercase. */
        if(argv[i][len - 1] >= 'a' && argv[i][len - 1] <= 'f')
            argv[i][len - 1] = argv[i][len - 1] & ~0x20;

        /* convert hexadecimal characters to numbers. */
        if (argv[i][len - 1] >= 'A' && argv[i][len - 1] <= 'F')
            ret[i] = argv[i][len - 1] - 'A' + 10;
        else if (argv[i][len - 1] >= '0' && argv[i][len - 1] <= '9')
            ret[i] = argv[i][len - 1] & ~0x30;
        else
            return false;

        if (len == 2) {
            if(argv[i][0] >= 'a' && argv[i][0] <= 'f')
                argv[i][0] = argv[i][0] & ~0x20;

            if (argv[i][0] >= 'A' && argv[i][0] <= 'F')
                ret[i] |= (argv[i][0] - 'A' + 10) << 4;
            else if (argv[i][0] >= '0' && argv[i][0] <= '9')
                ret[i] |= (argv[i][0] & ~0x30) << 4;
            else
                return false;
        }
    }
    return true;
}


static int do_set_snap_frame_cmd(struct nl_cb *cb, struct nl_msg *msg, int argc,
                             char **argv)
{
    /* argc contains phy and command name */
    if (argc > U8_MAX + 2 || argc < 3) {
        goto invalid_para;
    }

    argc -= 2;
    argv += 2;

    struct bes_msg_set_snap_frame_t *snap_frame = (struct bes_msg_set_snap_frame_t *) malloc(argc + 1);
    if (!snap_frame) {
        return -ENOMEM;
    }
    snap_frame->len = (u8)argc;
    if (!hex_str_to_byte(argc, argv, snap_frame->frame))
        goto invalid_para;

    struct nl_data *data_snap_frame = (struct nl_data *) malloc(sizeof(struct nl_data));
    if (!data_snap_frame) {
        return -ENOMEM;
    }
    data_snap_frame->d_size = argc + 1;
    data_snap_frame->d_data = snap_frame;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_SET_SNAP_FRAME);
    NLA_PUT_DATA(msg, BESPHY_TM_MSG_DATA, data_snap_frame);

    free(snap_frame);
    snap_frame = NULL;
    free(data_snap_frame);
    data_snap_frame = NULL;

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_get_tx_power_level_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                    char **argv)
{
    if (argc != 2) {
        goto invalid_para;
    }
    static int id_type = BES_MSG_GET_TX_POWER_LEVEL;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_GET_TX_POWER_LEVEL);
    NLA_PUT_U32(msg, BESPHY_TM_MSG_DATA, 0);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_get_tx_power_range_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                    char **argv)
{
    if (argc != 2) {
        goto invalid_para;
    }
    static int id_type = BES_MSG_GET_TX_POWER_RANGE;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_GET_TX_POWER_RANGE);
    NLA_PUT_U32(msg, BESPHY_TM_MSG_DATA, 0);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_set_advance_scan_elems_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                        char **argv)
{
    if (argc != 4) {
        goto invalid_para;
    }

    int duration_tmp, len_tmp;
    struct advance_scan_elems scan_elems;

    if (!msg_tsm_atoi(argv[2], &len_tmp) || !msg_tsm_atoi(argv[3], &duration_tmp))
        goto invalid_para;

    if (duration_tmp > U16_MAX || duration_tmp < 0) {
        goto invalid_para;
    }
    if (len_tmp > U8_MAX || len_tmp < 0) {
        goto invalid_para;
    }

    scan_elems.scanMode = (u8)len_tmp;
    scan_elems.duration = (u16)duration_tmp;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_SET_ADVANCE_SCAN_ELEMS);
    NLA_PUT_TYPE(msg, struct advance_scan_elems, BESPHY_TM_MSG_DATA, scan_elems);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_set_tx_queue_params_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                     char **argv)
{
    if (argc != 5) {
        goto invalid_para;
    }

    int user_priority_tmp;
    int medium_time_tmp;
    int expiry_time_tmp;
    struct bes_msg_set_txqueue_params txqueue_params;

    if (!msg_tsm_atoi(argv[2], &user_priority_tmp) ||
        !msg_tsm_atoi(argv[3], &medium_time_tmp)   ||
        !msg_tsm_atoi(argv[4], &expiry_time_tmp))
        goto invalid_para;

    if (user_priority_tmp > U8_MAX || user_priority_tmp < 0)
        goto invalid_para;

    if (medium_time_tmp > U16_MAX || medium_time_tmp < 0)
        goto invalid_para;

    if (expiry_time_tmp > U16_MAX || expiry_time_tmp < 0)
        goto invalid_para;

    txqueue_params.user_priority = (u8)user_priority_tmp;
    txqueue_params.medium_time = (u16)medium_time_tmp;
    txqueue_params.expiry_time = (u16)expiry_time_tmp;


    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_SET_TX_QUEUE_PARAMS);
    NLA_PUT_TYPE(msg, struct bes_msg_set_txqueue_params, BESPHY_TM_MSG_DATA, txqueue_params);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_start_stop_tsm_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                char **argv)
{
    if (argc != 5) {
        goto invalid_para;
    }

    struct bes_msg_start_stop_tsm_t  start_stop_tsm;
    int start_tmp;
    int up_tmp;
    int packetization_delay_tmp;

    if (!msg_tsm_atoi(argv[2], &start_tmp) ||
        !msg_tsm_atoi(argv[3], &up_tmp)    ||
        !msg_tsm_atoi(argv[4], &packetization_delay_tmp))
        goto invalid_para;

    if (start_tmp > U8_MAX || start_tmp < 0)
        goto invalid_para;

    if (up_tmp > U8_MAX || up_tmp < 0)
        goto invalid_para;

    if (packetization_delay_tmp > U16_MAX || packetization_delay_tmp < 0)
        goto invalid_para;

    start_stop_tsm.start = (u8)start_tmp;
    start_stop_tsm.up = (u8)up_tmp;
    start_stop_tsm.packetization_delay = (u16)packetization_delay_tmp;


    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_START_STOP_TSM);
    NLA_PUT_TYPE(msg, struct bes_msg_start_stop_tsm_t, BESPHY_TM_MSG_DATA, start_stop_tsm);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_get_tsm_params_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                char **argv)
{
    if (argc != 2) {
        goto invalid_para;
    }
    static int id_type = BES_MSG_GET_TSM_PARAMS;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_GET_TSM_PARAMS);
    NLA_PUT_U32(msg, BESPHY_TM_MSG_DATA, 0);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_get_roam_delay_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                char **argv)
{
    if (argc != 2) {
        goto invalid_para;
    }
    static int id_type = BES_MSG_GET_ROAM_DELAY;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_GET_ROAM_DELAY);
    NLA_PUT_U32(msg, BESPHY_TM_MSG_DATA, 0);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_set_power_save_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                char **argv)
{
    if (argc != 3) {
        goto invalid_para;
    }

    int powerSave_tmp;
    struct power_save_elems  power_elems;

    if (!msg_tsm_atoi(argv[2], &powerSave_tmp))
        goto invalid_para;

    power_elems.powerSave = powerSave_tmp;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_SET_POWER_SAVE);
    NLA_PUT_TYPE(msg, struct power_save_elems, BESPHY_TM_MSG_DATA, power_elems);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_add_ip_offload_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc, char **argv)
{
    if (argc != 5) {
        goto invalid_para;
    }

    u8 proto_temp;
    int dest_port_temp;
    int payload_len_temp;
    int data_len;
    static int id_type = BES_MSG_ADD_IP_OFFLOAD;

    if (!strcmp(argv[2], "udp"))
        proto_temp = 0;
    else if (!strcmp(argv[2], "tcp"))
        proto_temp = 1;
    else
        goto invalid_para;

    if (!msg_tsm_atoi(argv[3], &dest_port_temp))
        goto invalid_para;

    if (dest_port_temp > U16_MAX || dest_port_temp < 0)
        goto invalid_para;

    payload_len_temp = strlen(argv[4]);
    if (payload_len_temp > U16_MAX || payload_len_temp < 0)
        goto invalid_para;

    data_len = 3 * sizeof(uint16_t) + payload_len_temp + 1;
    struct add_ip_offload_t *add_ip_offload = (struct add_ip_offload_t *) malloc(data_len);

    if (!add_ip_offload) {
        return -ENOMEM;
    }
    add_ip_offload->proto = proto_temp;
    add_ip_offload->dest_port = (u16)dest_port_temp;
    add_ip_offload->payload_len = (u16)payload_len_temp;
    memcpy(add_ip_offload->payload, argv[4], payload_len_temp + 1);

    struct nl_data *data_add_ip_offload = (struct nl_data *) malloc(sizeof(struct nl_data));
    if (!data_add_ip_offload) {
        return -ENOMEM;
    }
    data_add_ip_offload->d_size = data_len;
    data_add_ip_offload->d_data = add_ip_offload;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_ADD_IP_OFFLOAD);
    NLA_PUT_DATA(msg, BESPHY_TM_MSG_DATA, data_add_ip_offload);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_recvmsgs, &id_type);

    free(add_ip_offload);
    add_ip_offload = NULL;
    free(data_add_ip_offload);
    data_add_ip_offload = NULL;

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_del_ip_offload_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                 char **argv)
{
    if (argc != 3) {
        goto invalid_para;
    }

    if (strncmp(argv[0],"wlan", 4)) {
        printf("%s para error, input wlanname\n", argv[0]);
        goto invalid_para;
    }

    int idx_tmp;
    struct ip_alive_iac_idx iac_idx;

    if (!msg_tsm_atoi(argv[2], &idx_tmp))
        goto invalid_para;

    iac_idx.idx = idx_tmp;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_DEL_IP_OFFLOAD);
    NLA_PUT_TYPE(msg, struct ip_alive_iac_idx, BESPHY_TM_MSG_DATA, iac_idx);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}

static int do_set_ip_offload_period_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                                        char **argv)
{
    if (argc != 3) {
        goto invalid_para;
    }

    if (strncmp(argv[0],"wlan", 4)) {
        printf("%s para error, input wlanname\n", argv[0]);
        goto invalid_para;
    }

    int period_tmp;
    struct ip_alive_period alive_period;

    if (!msg_tsm_atoi(argv[2], &period_tmp))
        goto invalid_para;

    alive_period.period = period_tmp;

    NLA_PUT_U32(msg, BESPHY_TM_MSG_ID, BES_MSG_SET_IP_OFFLOAD_PERIOD);
    NLA_PUT_TYPE(msg, struct ip_alive_period, BESPHY_TM_MSG_DATA, alive_period);

    return 0;

invalid_para:
    return -EINVAL;

nla_put_failure:
    return -ENOBUFS;

}


/*
 * Print the currently executed test command.
 * And fill command to genlmsg.
 */
static int do_besphy_test_cmd(struct nl_cb *cb, struct nl_msg *msg, const int argc,
                              char **argv)
{
    int i, ret;
    if (argv[1]) {
        printf("do_%s: argc=%d, argv: %s ",argv[1], argc - 1, argv[1]);
    } else {
        goto para_input_error;
    }
    for (i = 2; i < argc; i++) {
        if (argv[i]) {
            printf("%s ",argv[i]);
        } else {
            printf("\n");
            goto para_input_error;
        }
    }
    printf("\n");


    if (!strcmp(argv[1], "msg_test")) {
        ret = do_msg_test_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "set_snap_frame")) {
        ret = do_set_snap_frame_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "get_tx_power_level")) {
        ret = do_get_tx_power_level_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "get_tx_power_range")) {
        ret = do_get_tx_power_range_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "set_advance_scan_elems")) {
        ret = do_set_advance_scan_elems_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "set_tx_queue_params")) {
        ret = do_set_tx_queue_params_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "start_stop_tsm")) {
        ret = do_start_stop_tsm_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "get_tsm_params")) {
        ret = do_get_tsm_params_cmd(cb, msg, argc, argv);

    } else if (!strcmp(argv[1], "get_roam_delay")) {
        ret = do_get_roam_delay_cmd(cb, msg, argc, argv);

    } else if(!strcmp(argv[1], "set_power_save")) {
        ret = do_set_power_save_cmd(cb, msg, argc, argv);

    } else if(!strcmp(argv[1], "keep_alive_cfg")) {
        ret = do_add_ip_offload_cmd(cb, msg, argc, argv);

    } else if(!strcmp(argv[1], "del_keep_alive")) {
        ret = do_del_ip_offload_cmd(cb, msg, argc, argv);

    } else if(!strcmp(argv[1], "keep_alive_peroid")) {
        ret = do_set_ip_offload_period_cmd(cb, msg, argc, argv);

    } else {
        /**
         * always keep the vendor rf cmd at the end.
         * including signaling and nosignaling
         */
        ret = do_vendor_rf_cmd(cb, msg, argc, argv);
    }

    return ret;

para_input_error:
    printf("The number of input param is not equal to argc.\n");
    return -EINVAL;

}


int do_commands(struct nl_cb *cb, struct nl_msg *msg, const int argc, char **argv)
{
    int ret = 0;
    if (argc <= 1) {
        return -EINVAL;
    }

    ret = do_besphy_test_cmd(cb, msg, argc, argv);

    return ret;
}

