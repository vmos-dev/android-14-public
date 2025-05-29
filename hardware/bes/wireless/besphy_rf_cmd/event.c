/*
 * This ought to be provided by libnl
 */

#include <asm/errno.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <linux/genetlink.h>
#include <stdarg.h>
#include <time.h>

#include "besphy_rf.h"


#define NL80211_GENL_NAME "nl80211"

/**
 * nl80211 supports the following multicast groups.
 * Different multicast groups receive different msg.
 */
#define NL80211_MULTICAST_GROUP_CONFIG		"config"
#define NL80211_MULTICAST_GROUP_SCAN		"scan"
#define NL80211_MULTICAST_GROUP_REG		"regulatory"
#define NL80211_MULTICAST_GROUP_MLME		"mlme"
#define NL80211_MULTICAST_GROUP_VENDOR		"vendor"
#define NL80211_MULTICAST_GROUP_NAN		"nan"
#define NL80211_MULTICAST_GROUP_TESTMODE	"testmode"


static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}


static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
					     void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

struct handler_args {
	const char *group;
	int id;
};

static int family_handler(struct nl_msg *msg, void *arg)
{
	struct handler_args *grp = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int rem_mcgrp;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
		struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
			  nla_data(mcgrp), nla_len(mcgrp), NULL);

		if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
			!tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
			continue;
		if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
				grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])))
			continue;
		grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int ret, ctrlid;
	struct handler_args grp = {
		.group = group,
		.id = -ENOENT,
	};

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		ret = -ENOMEM;
		goto out_fail_cb;
	}

	ctrlid = genl_ctrl_resolve(sock, "nlctrl");

	genlmsg_put(msg, 0, 0, ctrlid, 0,
		    0, CTRL_CMD_GETFAMILY, 0);

	ret = -ENOBUFS;
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = nl_send_auto_complete(sock, msg);
	if (ret < 0)
		goto out;

	ret = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

	while (ret > 0)
		nl_recvmsgs(sock, cb);

	if (ret == 0)
		ret = grp.id;
 nla_put_failure:
 out:
	nl_cb_put(cb);
 out_fail_cb:
	nlmsg_free(msg);
	return ret;
}


static void get_cur_time(char *cur_time)
{
	time_t timep;
	const char *month[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
					   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	struct tm *tm;
	time(&timep);
	tm = localtime(&timep);
	snprintf(cur_time, 16, "%s %02d %02d:%02d:%02d", month[tm->tm_mon],
	tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

}


static void frame_hexdump(char *prefix, u8 *data, int len)
{
	int i;

	printf("%s hexdump:\n", prefix);
	for (i = 0; i < len; i++) {
		if (i + 10 < len) {
			printf("%.1X %.1X %.1X %.1X " \
				"%.1X %.1X %.1X %.1X %.1X %.1X\n",
				data[i], data[i+1], data[i+2],
				data[i+3], data[i+4], data[i+5],
				data[i+6], data[i+7], data[i+8],
				data[i+9]);
			i += 9;
		} else {
			printf("%.1X ", data[i]);
		}
	}
	printf("\n");
}


static int print_event(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 2];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[BESPHY_TM_ATTR_MAX + 2];
	char cur_time[16];

	get_cur_time(cur_time);

	/* get reply message for all nlmsg */
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA] || !tb[NL80211_ATTR_WIPHY]) {
		printf("%s linsten event: no data!\n", cur_time);
		return NL_SKIP;
	}

	/* get reply message for rf test cmd */
	nla_parse(td, BESPHY_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		  nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	if (!td[BESPHY_TM_MSG_ID] || !td[BESPHY_TM_MSG_DATA]) {
		printf("%s linsten event: no recvmsgs info\n", cur_time);
		return NL_SKIP;
	}


	switch (nla_get_u32(td[BESPHY_TM_MSG_ID])) {
	case BES_MSG_EVENT_TEST:
		printf("%s phy#%d, msg_test event:\ndummy = %d\n\n", cur_time,
		nla_get_u32(tb[NL80211_ATTR_WIPHY]),
		nla_get_u32(td[BESPHY_TM_MSG_DATA]));
		break;
	case BES_MSG_EVENT_FRAME_DATA:
		printf("%s phy#%d, frame_data event, ", cur_time, nla_get_u32(tb[NL80211_ATTR_WIPHY]));
		frame_hexdump("FRAME 802.3,", nla_data(td[BESPHY_TM_MSG_DATA]),nla_len(td[BESPHY_TM_MSG_DATA]));
		break;
#ifdef ROAM_OFFLOAD
	case BES_MSG_NEW_SCAN_RESULTS:
		printf("%s phy#%d, msg_test event:\nscan finished\n\n", cur_time, nla_get_u32(tb[NL80211_ATTR_WIPHY]));
		break;
#endif /* ROAM_OFFLOAD */
	default:
		break;
	}

	fflush(stdout);
	return NL_SKIP;
}

/**
 * Get the multicast group number and join.
 * For CONFIG_BES2600_TESTMODE, just join the nl80211 testmode multicast group.
*/
int prepare_listen_events(struct nl80211_state *state, struct nl_cb **cb)
{
	int mcid, ret;

	/* join in testmode multicast group */
	mcid = nl_get_multicast_id(state->nl_sock, "nl80211", "testmode");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state->nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* cb_event callback */
	*cb = nl_cb_alloc(NL_CB_DEFAULT);

	if (!(*cb)) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		return -ENOMEM;
	}

	return 0;
}


u32 __do_listen_events(struct nl80211_state *state, struct nl_cb *cb)
{
	/* no sequence checking for multicast messages */
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);

	/* Print the result of the listener without exiting the listener. */
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_event, NULL);

	while (1)
		nl_recvmsgs(state->nl_sock, cb);

	nl_cb_put(cb);

	return 0;
}


u32 listen_events(struct nl80211_state *state, struct nl_cb *cb)
{
	return __do_listen_events(state, cb);
}