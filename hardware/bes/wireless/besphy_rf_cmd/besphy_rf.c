/*
 * nl80211 test tool
 *
 * Copyright 2007-2009    Johannes Berg <johannes@sipsolutions.net>
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "besphy_rf.h"


#if (!defined CONFIG_LIBNL20) && (!defined CONFIG_LIBNL30)
/* libnl 2.0 && libnl 3.0 compatibility code */

static inline struct nl_handle *nl_socket_alloc(void)
{
    return nl_handle_alloc();
}

static inline void nl_socket_free(struct nl_sock *h)
{
    nl_handle_destroy(h);
}

static inline int __genl_ctrl_alloc_cache(struct nl_sock *h, struct nl_cache **cache)
{
    struct nl_cache *tmp = genl_ctrl_alloc_cache(h);
    if (!tmp)
        return -ENOMEM;
    *cache = tmp;
    return 0;
}
#define genl_ctrl_alloc_cache __genl_ctrl_alloc_cache
#endif /* CONFIG_LIBNL20 && CONFIG_LIBNL30*/

static int nl80211_init(struct nl80211_state *state)
{
    int err;

    state->nl_sock = nl_socket_alloc();
    if (!state->nl_sock) {
        fprintf(stderr, "Failed to allocate netlink socket.\n");
        return -ENOMEM;
    }

    if (genl_connect(state->nl_sock)) {
        fprintf(stderr, "Failed to connect to generic netlink.\n");
        err = -ENOLINK;
        goto out_handle_destroy;
    }

    if (genl_ctrl_alloc_cache(state->nl_sock, &state->nl_cache)) {
        fprintf(stderr, "Failed to allocate generic netlink cache.\n");
        err = -ENOMEM;
        goto out_handle_destroy;
    }

    state->nl80211 = genl_ctrl_search_by_name(state->nl_cache, "nl80211");
    if (!state->nl80211) {
        fprintf(stderr, "nl80211 not found.\n");
        err = -ENOENT;
        goto out_cache_free;
    }

    /*
     * Enable peek mode so drivers can send large amounts
     * of data in blobs without problems.
     */
    nl_socket_enable_msg_peek(state->nl_sock);

    return 0;

 out_cache_free:
    nl_cache_free(state->nl_cache);
 out_handle_destroy:
    nl_socket_free(state->nl_sock);
    return err;
}

static void nl80211_cleanup(struct nl80211_state *state)
{
    genl_family_put(state->nl80211);
    nl_cache_free(state->nl_cache);
    nl_socket_free(state->nl_sock);
}

static int phy_lookup(char *name)
{
    char buf[200];
    int fd, pos;

    snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

    fd = open(buf, O_RDONLY);
    if (fd < 0)
        return -1;
    pos = read(fd, buf, sizeof(buf) - 1);
    if (pos < 0)
        return -1;
    buf[pos] = '\0';
    return atoi(buf);
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
             void *arg)
{
    int *ret = arg;
    *ret = err->error;
    printf("error handler NL_STOP = %d\n", NL_STOP);
    return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    printf("finish handler NL_SKIP = %d\n", NL_SKIP);
    *ret = 0;
    return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    printf("ack handler NL_STOP = %d\n", NL_STOP);
    *ret = 0;
    return NL_STOP;
}

static char wlan0_vector[6] = {'w','l','a','n','0','\0'};
static void wlan_add(int argc, char **argv)
{
    int i;
    char *tmp[argc + 1];
    tmp[0] = wlan0_vector;
    char **argv_tmp = argv;
    for (i = 1; i <= argc; i++) {
        tmp[i] = *argv_tmp++;
    }
    for (i = 0; i <= argc; i++) {
        *argv++ = tmp[i];
    }
}

static int handle(struct nl80211_state *state, int argc, char **argv)
{
    struct nl_cb *cb;
    struct nl_msg *msg;
    struct nlattr *nest;
    int devidx = 0;
    int err;

    if (!argc) {
        printf("no dev/phy given\n");
        return 1;
    }

    /* CHANGE HERE: you may need to allocate larger messages! */
    msg = nlmsg_alloc();
    if (!msg) {
        fprintf(stderr, "failed to allocate netlink message\n");
        return 2;
    }

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        fprintf(stderr, "failed to allocate netlink callbacks\n");
        err = 2;
        goto out_free_msg;
    }

    genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0, 0, NL80211_CMD_TESTMODE, 0);

    //genlmsg_put(msg, 0, 0, state->nl80211, 0, 0, NL80211_CMD_TESTMODE, 0);
    //genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
    //        0, NL80211_CMD_TRIGGER_SCAN, 0);
    devidx = if_nametoindex(*argv);
    printf("dev = %s, devidx=%d\n", *argv, devidx);
    if (devidx) {
        NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
    } else {
        devidx = phy_lookup(*argv);
        if (devidx < 0) {
            printf("Device not found\nCheck phyname/wlanname\n");
            return 1;
        }
        printf("phy lookup index=%d\n", devidx);
        NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, devidx);
    }

    nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
    if (!nest)
        return 4;

    err = do_commands(cb, msg, argc, argv);
    nla_nest_end(msg, nest);

    if (err) {
        printf("do command error = %d\n", err);
        goto out;

    }

    err = nl_send_auto_complete(state->nl_sock, msg);
    if (err < 0) {
        printf("nl_send_auto_complete err = %d\n", err);
        goto out;

    }

    err = 1;

    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

    while (err > 0) {
        //printf("while err = %d\n", err);
        nl_recvmsgs(state->nl_sock, cb);
    }
 out:
    nl_cb_put(cb);
 out_free_msg:
    nlmsg_free(msg);
    return err;
 nla_put_failure:
    fprintf(stderr, "building message failed\n");
    return 2;
}

static struct nl80211_state nl_state;
static struct nl_cb *cb_event = NULL;


static void besphy_remove(void)
{
    if (cb_event)
        nl_cb_put(cb_event);
    nl80211_cleanup(&nl_state);
}

/**
 * force exit program when event listening.
 * receive the CTRL C forced exit signal, and then exit the program.
 */
void signal_handler(int signum)
{
    besphy_remove();
    printf("\n");
    exit(signum);
}

void usage(void)
{
    printf("besphy usage:\n"
           "RF nosignaling commands:\n"
           "\tbesphy [wlanname/phyname] start\n"
           "\tbesphy [wlanname/phyname] bandwidth <value(20/40)>\n"
           "\tbesphy [wlanname/phyname] rate <value>\n"
           "\tbesphy [wlanname/phyname] get_rate\n"
           "\tbesphy [wlanname/phyname] tx\n"
           "\tbesphy [wlanname/phyname] tx_stop\n"
           "\tbesphy [wlanname/phyname] rx\n"
           "\tbesphy [wlanname/phyname] rx_ackSet\n"
           "\tbesphy [wlanname/phyname] rx_ackRead\n"
           "\tbesphy [wlanname/phyname] rx_stop\n"
           "\tbesphy [wlanname/phyname] channel <value>\n"
           "\tbesphy [wlanname/phyname] get_channel\n"
           "\tbesphy [wlanname/phyname] powerlevel <value(hex)>\n"
           "\tbesphy [wlanname/phyname] get_powerlevel\n"
           "\tbesphy [wlanname/phyname] save_powerlevel\n"
           "\tbesphy [wlanname/phyname] get_save_powerlevel\n"
           "\tbesphy [wlanname/phyname] freqOffset <value(hex)>\n"
           "\tbesphy [wlanname/phyname] get_freqOffset\n"
           "\tbesphy [wlanname/phyname] save_freqoffset\n"
           "\tbesphy [wlanname/phyname] get_save_freqoffset\n"
           "\tbesphy [wlanname/phyname] save\n"
           "\tbesphy [wlanname/phyname] wifi_stop\n"
           "Help commands:\n"
           "\tbesphy\n"
           "\tbesphy -h\n"
           "\tbesphy help\n"
           "\nBy default, wlan0 is supported. When the device is not wlan0, "
           "enter phyname/wlanname.\n\n");
}


static enum ARGV_PARSE_ATTR argc_parse(int argc, char **argv)
{
    /* enter at least one command.(phyname/wlanname and one cmd) */
    if (argc < 2)
        return INVLID_ARGC;

    argc--;
    argv++;

    enum ARGV_PARSE_ATTR ret = INVLID_ARGC;

    if (argc == 1 && (!strcmp(argv[0], "-h") || !strcmp(argv[0], "help"))) {
        ret = BESPHY_USAGE;
    } else if (!strcmp(argv[0], "monitor") && argc == 1) {
            ret = BESPHY_MONITOR;
    } else {
        ret = BESPHY_TESTMODE;
    }

    return ret;

}


int main(int argc, char **argv)
{
    int err;
    enum ARGV_PARSE_ATTR cmd_type;

    /* strip off self */
    argc--;
    argv++;

    if (argc == 0) {
        usage();
        return 0;
    }

    /* if phy* or wlan* is not inputed, add phy0 as the default parameter. */
    if (!(!strncmp(*argv,"phy", 3) || !strncmp(*argv,"wlan", 4))) {
        wlan_add(argc, argv);
        argc++;
    }

    cmd_type = argc_parse(argc, argv);
    if (cmd_type == BESPHY_USAGE) {
        usage();
        return 0;
    }

    err = nl80211_init(&nl_state);
    if (err)
        return 1;

    switch (cmd_type) {
    case BESPHY_USAGE:
        err = 0;
        break;
    case BESPHY_TESTMODE:
        err = handle(&nl_state, argc, argv);
        if (err > 0)
            printf("error!\n");
        else if (err < 0)
            printf("error: %d (%s)\n", err, strerror(-err));
        if (err == -EINVAL)
            printf("besphy: bad parameters (see \"besphy -h\")\n");

        /** 
         * If need to monitor the event after sending the test command,
         * enter the monitoring immediately.
         * If not listening, exit the program.
         */
        if (err == 0 && !strcmp(argv[1], "set_snap_frame"))
            printf("start snap format frame listening...\n");
        else
            break;
    case BESPHY_MONITOR:
        err = prepare_listen_events(&nl_state, &cb_event);
        if (err) {
            printf("join multicast group failed, err: %d\n", err);
            break;
        }
        printf("listen testmode event start...\n");
        /* Initialize signal exit function. */
        signal(SIGINT, signal_handler);
        listen_events(&nl_state, cb_event);
        break;
    case INVLID_ARGC:
    default:
        printf("besphy: bad parameters (see \"besphy -h\")\n");
        err = 1;
        break;
    }

    besphy_remove();

    return err;
}
