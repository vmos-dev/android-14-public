#ifndef __BESPHY_RF_H
#define __BESPHY_RF_H

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "nl80211.h"

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

#define ETH_ALEN 6
#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof(ar[0]))


#if (!defined CONFIG_LIBNL20) && (!defined CONFIG_LIBNL30)
#  define nl_sock nl_handle
#endif

/**
 * input parameter type
 */
enum ARGV_PARSE_ATTR {
    BESPHY_USAGE,
    BESPHY_TESTMODE,
    BESPHY_MONITOR,
    INVLID_ARGC,
};

/*
 * copied from the kernel -- keep in sync
 * and correspond with enum bes_nl80211_testmode_data_attributes from wifi-linux
 */
enum besphy_testmode_attr {
    __BESPHY_TM_ATTR_INVALID = 0,
    BESPHY_TM_MSG_ID,	/* u32 type containing the BES message ID */
    BESPHY_TM_MSG_DATA,	/* message payload */

    /* keep last */
    __BESPHY_TM_ATTR_AFTER_LAST,
    BESPHY_TM_ATTR_MAX = __BESPHY_TM_ATTR_AFTER_LAST - 1
};


#define CONFIG_BES2600_TESTMODE
/*
 * copied enum bes_nl80211_testmode_data_attributes from wifi-linux
 */
enum bes_msg_id {
    BES_MSG_TEST = 0,	/* for test purposes */
    BES_MSG_EVENT_TEST,	/* for test purposes, event ID used by the driver */
    BES_MSG_SET_SNAP_FRAME,  /*  snap_frame  */
    BES_MSG_EVENT_FRAME_DATA,  /* event ID used by the driver */
#ifdef CONFIG_BES2600_TESTMODE
    BES_MSG_GET_TX_POWER_LEVEL,       /* get_tx_power_level */
    BES_MSG_GET_TX_POWER_RANGE,       /* get_tx_power_range */
    BES_MSG_SET_ADVANCE_SCAN_ELEMS,   /* set_advance_scan_elems */
    BES_MSG_SET_TX_QUEUE_PARAMS,      /* set_tx_queue_params */
    BES_MSG_START_STOP_TSM,           /* start_stop_tsm */
    BES_MSG_GET_TSM_PARAMS,           /* get_tsm_params */
    BES_MSG_GET_ROAM_DELAY,           /* get_roam_delay */
#endif /*CONFIG_BES2600_TESTMODE*/
    BES_MSG_SET_POWER_SAVE,           /* set_power_save */
#ifdef ROAM_OFFLOAD
    BES_MSG_NEW_SCAN_RESULTS,         /* event ID used by the driver */
#endif /* ROAM_OFFLOAD */
    BES_MSG_ADD_IP_OFFLOAD,
    BES_MSG_DEL_IP_OFFLOAD,
    BES_MSG_SET_IP_OFFLOAD_PERIOD,
    BES_MSG_VENDOR_RF_CMD,            /* rf signaling cmd & nosignaling cmd */
    /* Add new IDs here */

    BES_MSG_ID_MAX,
};

enum vendor_rf_cmd_type {
    VENDOR_RF_SIGNALING_CMD = 0,
    VENDOR_RF_NOSIGNALING_CMD,
    VENDOR_RF_SAVE_CMD,
    VENDOR_RF_GET_SAVE_FREQOFFSET_CMD,
    VENDOR_RF_GET_SAVE_POWERLEVEL_CMD,
    VENDOR_RF_SAVE_FREQOFFSET_CMD,
    VENDOR_RF_SAVE_POWERLEVEL_CMD,
    /* add new here */

    VENDOR_RF_CMD_MAX,

    VENDOR_RF_SIG_NOSIG_MIX = 0xFFFFFFFF,

};

struct print_event_args {
    struct timeval ts; /* internal */
    bool have_ts; /* must be set false */
    bool frame, time, reltime;
};

struct wait_event {
    int n_cmds;
    const u32 *cmds;
    u32 cmd;
    struct print_event_args *pargs;
};

struct nl80211_state {
    struct nl_sock *nl_sock;
    struct nl_cache *nl_cache;
    struct genl_family *nl80211;
};

void usage(void);
int do_commands(struct nl_cb *cb, struct nl_msg *msg, const int argc, char **argv);
u32 listen_events(struct nl80211_state *state, struct nl_cb *cb);
int prepare_listen_events(struct nl80211_state *state, struct nl_cb **cb);

#endif /* __BESPHY_RF_H */
