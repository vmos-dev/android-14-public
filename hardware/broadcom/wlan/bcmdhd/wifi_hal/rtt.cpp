/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Portions copyright (C) 2023 Broadcom Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink-private/object-api.h>
#include <netlink-private/types.h>

#include "nl80211_copy.h"

#include "sync.h"

#define LOG_TAG  "WifiHAL"

#include <log/log.h>
#include <utils/String8.h>

#include <hardware_legacy/wifi_hal.h>
#include "common.h"
#include "cpp_bindings.h"

using namespace android;
#define RTT_RESULT_V2_SIZE (sizeof(wifi_rtt_result_v2))
#define RTT_RESULT_V1_SIZE (sizeof(wifi_rtt_result))
typedef enum {

    RTT_SUBCMD_SET_CONFIG = ANDROID_NL80211_SUBCMD_RTT_RANGE_START,
    RTT_SUBCMD_CANCEL_CONFIG,
    RTT_SUBCMD_GETCAPABILITY,
    RTT_SUBCMD_GETAVAILCHANNEL,
    RTT_SUBCMD_SET_RESPONDER,
    RTT_SUBCMD_CANCEL_RESPONDER,
} RTT_SUB_COMMAND;

typedef enum {
    RTT_ATTRIBUTE_TARGET_INVALID            = 0,
    RTT_ATTRIBUTE_TARGET_CNT                = 1,
    RTT_ATTRIBUTE_TARGET_INFO               = 2,
    RTT_ATTRIBUTE_TARGET_MAC                = 3,
    RTT_ATTRIBUTE_TARGET_TYPE               = 4,
    RTT_ATTRIBUTE_TARGET_PEER               = 5,
    RTT_ATTRIBUTE_TARGET_CHAN               = 6,
    RTT_ATTRIBUTE_TARGET_PERIOD             = 7,
    RTT_ATTRIBUTE_TARGET_NUM_BURST          = 8,
    RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST      = 9,
    RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM      = 10,
    RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR     = 11,
    RTT_ATTRIBUTE_TARGET_LCI                = 12,
    RTT_ATTRIBUTE_TARGET_LCR                = 13,
    RTT_ATTRIBUTE_TARGET_BURST_DURATION     = 14,
    RTT_ATTRIBUTE_TARGET_PREAMBLE           = 15,
    RTT_ATTRIBUTE_TARGET_BW                 = 16,
    RTT_ATTRIBUTE_RESULTS_COMPLETE          = 30,
    RTT_ATTRIBUTE_RESULTS_PER_TARGET        = 31,
    RTT_ATTRIBUTE_RESULT_CNT                = 32,
    RTT_ATTRIBUTE_RESULT                    = 33,
    RTT_ATTRIBUTE_RESUTL_DETAIL             = 34,
    RTT_ATTRIBUTE_RESULT_FREQ               = 35,
    RTT_ATTRIBUTE_RESULT_BW                 = 36,
    /* Add any new RTT_ATTRIBUTE prior to RTT_ATTRIBUTE_MAX */
    RTT_ATTRIBUTE_MAX
} RTT_ATTRIBUTE;
typedef struct strmap_entry {
    int			id;
    String8		text;
} strmap_entry_t;
struct dot11_rm_ie {
    u8 id;
    u8 len;
    u8 token;
    u8 mode;
    u8 type;
} __attribute__ ((packed));
typedef struct dot11_rm_ie dot11_rm_ie_t;
#define DOT11_HDR_LEN 2
#define DOT11_RM_IE_LEN       5
#define DOT11_MNG_MEASURE_REQUEST_ID		38	/* 11H MeasurementRequest */
#define DOT11_MNG_MEASURE_REPORT_ID		39	/* 11H MeasurementResponse */
#define DOT11_MEASURE_TYPE_LCI		8   /* d11 measurement LCI type */
#define DOT11_MEASURE_TYPE_CIVICLOC	11  /* d11 measurement location civic */

static const strmap_entry_t err_info[] = {
    {RTT_STATUS_SUCCESS, String8("Success")},
    {RTT_STATUS_FAILURE, String8("Failure")},
    {RTT_STATUS_FAIL_NO_RSP, String8("No reponse")},
    {RTT_STATUS_FAIL_INVALID_TS, String8("Invalid Timestamp")},
    {RTT_STATUS_FAIL_PROTOCOL, String8("Protocol error")},
    {RTT_STATUS_FAIL_REJECTED, String8("Rejected")},
    {RTT_STATUS_FAIL_NOT_SCHEDULED_YET, String8("not scheduled")},
    {RTT_STATUS_FAIL_SCHEDULE,  String8("schedule failed")},
    {RTT_STATUS_FAIL_TM_TIMEOUT, String8("timeout")},
    {RTT_STATUS_FAIL_AP_ON_DIFF_CHANNEL, String8("AP is on difference channel")},
    {RTT_STATUS_FAIL_NO_CAPABILITY, String8("no capability")},
    {RTT_STATUS_FAIL_BUSY_TRY_LATER, String8("busy and try later")},
    {RTT_STATUS_ABORTED, String8("aborted")}
};

    static const char*
get_err_info(int status)
{
    int i;
    const strmap_entry_t *p_entry;
    int num_entries = sizeof(err_info)/ sizeof(err_info[0]);
    /* scan thru the table till end */
    p_entry = err_info;
    for (i = 0; i < (int) num_entries; i++)
    {
        if (p_entry->id == status)
            return p_entry->text;
        p_entry++;		/* next entry */
    }
    return "unknown error";			/* not found */
}

class GetRttCapabilitiesCommand : public WifiCommand
{
    wifi_rtt_capabilities *mCapabilities;
public:
    GetRttCapabilitiesCommand(wifi_interface_handle iface, wifi_rtt_capabilities *capabitlites)
        : WifiCommand("GetRttCapabilitiesCommand", iface, 0), mCapabilities(capabitlites)
    {
        memset(mCapabilities, 0, sizeof(*mCapabilities));
    }

    virtual int create() {
        ALOGD("Creating message to get scan capablities; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_GETCAPABILITY);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGD("In GetRttCapabilitiesCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        void *data = reply.get_vendor_data();
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d, expected len = %d", id, subcmd, len,
                sizeof(*mCapabilities));

        memcpy(mCapabilities, data, min(len, (int) sizeof(*mCapabilities)));

        return NL_OK;
    }
};


class GetRttResponderInfoCommand : public WifiCommand
{
    wifi_rtt_responder* mResponderInfo;
public:
    GetRttResponderInfoCommand(wifi_interface_handle iface, wifi_rtt_responder *responderInfo)
        : WifiCommand("GetRttResponderInfoCommand", iface, 0), mResponderInfo(responderInfo)
    {
        memset(mResponderInfo, 0 , sizeof(*mResponderInfo));

    }

    virtual int create() {
        ALOGD("Creating message to get responder info ; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_GETAVAILCHANNEL);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGD("In GetRttResponderInfoCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        void *data = reply.get_vendor_data();
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d, expected len = %d", id, subcmd, len,
                sizeof(*mResponderInfo));

        memcpy(mResponderInfo, data, min(len, (int) sizeof(*mResponderInfo)));

        return NL_OK;
    }
};


class EnableResponderCommand : public WifiCommand
{
    wifi_channel_info  mChannelInfo;
    wifi_rtt_responder* mResponderInfo;
    unsigned m_max_duration_sec;
public:
    EnableResponderCommand(wifi_interface_handle iface, int id, wifi_channel_info channel_hint,
            unsigned max_duration_seconds, wifi_rtt_responder *responderInfo)
            : WifiCommand("EnableResponderCommand", iface, 0), mChannelInfo(channel_hint),
            m_max_duration_sec(max_duration_seconds), mResponderInfo(responderInfo)
    {
        memset(mResponderInfo, 0, sizeof(*mResponderInfo));
    }

    virtual int create() {
        ALOGD("Creating message to set responder ; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_SET_RESPONDER);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGD("In EnableResponderCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        void *data = reply.get_vendor_data();
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d, expected len = %d", id, subcmd, len,
                sizeof(*mResponderInfo));

        memcpy(mResponderInfo, data, min(len, (int) sizeof(*mResponderInfo)));

        return NL_OK;
    }
};


class CancelResponderCommand : public WifiCommand
{

public:
    CancelResponderCommand(wifi_interface_handle iface, int id)
        : WifiCommand("CancelResponderCommand", iface, 0)/*, mChannelInfo(channel)*/
    {

    }

    virtual int create() {
        ALOGD("Creating message to cancel responder ; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_CANCEL_RESPONDER);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {
        /* Nothing to do on response! */
        return NL_SKIP;
    }

};


class RttCommand : public WifiCommand
{
    unsigned numRttParams;
    int mCompleted;
    int currentIdx;
    int currDtlIdx;
    int totalCnt;
    static const int MAX_RESULTS = 1024;
    wifi_rtt_result_v2 *rttResults[MAX_RESULTS];
    wifi_rtt_config *rttParams;
    wifi_rtt_event_handler rttHandler;
    int nextidx = 0;
    wifi_rtt_result *rttResultsV1[MAX_RESULTS];
    wifi_channel channel;
    wifi_rtt_bw bw = WIFI_RTT_BW_UNSPECIFIED;
    int result_size;
    int opt_result_size;
public:
    RttCommand(wifi_interface_handle iface, int id, unsigned num_rtt_config,
            wifi_rtt_config rtt_config[], wifi_rtt_event_handler handler)
        : WifiCommand("RttCommand", iface, id), numRttParams(num_rtt_config), rttParams(rtt_config),
        rttHandler(handler)
    {
        memset(rttResults, 0, sizeof(rttResults));
        memset(rttResultsV1, 0, sizeof(rttResultsV1));
        currentIdx = 0;
        mCompleted = 0;
        totalCnt = 0;
        currDtlIdx = 0;
    }

    RttCommand(wifi_interface_handle iface, int id)
        : WifiCommand("RttCommand", iface, id)
    {
        currentIdx = 0;
        mCompleted = 0;
        totalCnt = 0;
        currDtlIdx = 0;
        numRttParams = 0;
        memset(rttResults, 0, sizeof(rttResults));
        memset(rttResultsV1, 0, sizeof(rttResultsV1));
        rttParams = NULL;
        rttHandler.on_rtt_results_v2 = NULL;
    }

    int createSetupRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, RTT_SUBCMD_SET_CONFIG);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        result = request.put_u8(RTT_ATTRIBUTE_TARGET_CNT, numRttParams);
        if (result < 0) {
            return result;
        }
        nlattr *rtt_config = request.attr_start(RTT_ATTRIBUTE_TARGET_INFO);
        for (unsigned i = 0; i < numRttParams; i++) {
            nlattr *attr2 = request.attr_start(i);
            if (attr2 == NULL) {
                return WIFI_ERROR_OUT_OF_MEMORY;
            }

            result = request.put_addr(RTT_ATTRIBUTE_TARGET_MAC, rttParams[i].addr);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_TYPE, rttParams[i].type);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_PEER, rttParams[i].peer);
            if (result < 0) {
                return result;
            }

            result = request.put(RTT_ATTRIBUTE_TARGET_CHAN, &rttParams[i].channel,
                    sizeof(wifi_channel_info));
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_BURST, rttParams[i].num_burst);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST,
                    rttParams[i].num_frames_per_burst);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM,
                    rttParams[i].num_retries_per_rtt_frame);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR,
                    rttParams[i].num_retries_per_ftmr);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_PERIOD,
                    rttParams[i].burst_period);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_BURST_DURATION,
                    rttParams[i].burst_duration);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_LCI,
                    rttParams[i].LCI_request);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_LCR,
                    rttParams[i].LCR_request);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_BW,
                    rttParams[i].bw);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_PREAMBLE,
                    rttParams[i].preamble);
            if (result < 0) {
                return result;
            }
            request.attr_end(attr2);
        }

        request.attr_end(rtt_config);
        request.attr_end(data);
        return WIFI_SUCCESS;
    }

    int createTeardownRequest(WifiRequest& request, unsigned num_devices, mac_addr addr[]) {
        int result = request.create(GOOGLE_OUI, RTT_SUBCMD_CANCEL_CONFIG);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
	result = request.put_u8(RTT_ATTRIBUTE_TARGET_CNT, num_devices);

	if (result < 0) {
		return result;
	}
        for(unsigned i = 0; i < num_devices; i++) {
            result = request.put_addr(RTT_ATTRIBUTE_TARGET_MAC, addr[i]);
            if (result < 0) {
                return result;
            }
        }
        request.attr_end(data);
        return result;
    }
    int start() {
        ALOGD("Setting RTT configuration");
        WifiRequest request(familyId(), ifaceId());
        int result = createSetupRequest(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create setup request; result = %d", result);
            return result;
        }

        registerVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
            ALOGE("failed to configure RTT setup; result = %d", result);
            return result;
        }

        ALOGI("Successfully started RTT operation");
        return result;
    }

    virtual int cancel() {
        ALOGD("Stopping RTT");

        WifiRequest request(familyId(), ifaceId());
        int result = createTeardownRequest(request, 0, NULL);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create stop request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop scan; result = %d", result);
            }
        }

        unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
        return WIFI_SUCCESS;
    }

    int cancel_specific(unsigned num_devices, mac_addr addr[]) {
        ALOGE("Stopping RTT");

        WifiRequest request(familyId(), ifaceId());
        int result = createTeardownRequest(request, num_devices, addr);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create stop request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop RTT; result = %d", result);
            }
        }

        unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
        return WIFI_SUCCESS;
    }

    virtual int handleResponse(WifiEvent& reply) {
        /* Nothing to do on response! */
        return NL_SKIP;
    }

    virtual int handleEvent(WifiEvent& event) {
        ALOGI("Got an RTT event");
        nlattr *vendor_data = event.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = event.get_vendor_data_len();
        if (vendor_data == NULL || len == 0) {
            ALOGI("No rtt results found");
            return NL_STOP;
        }
        for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
            if (it.get_type() == RTT_ATTRIBUTE_RESULTS_COMPLETE) {
                mCompleted = it.get_u32();
                ALOGI("retrieved completed flag : %d\n", mCompleted);
            } else if (it.get_type() == RTT_ATTRIBUTE_RESULTS_PER_TARGET) {
                int result_cnt = 0;
                mac_addr bssid;
                for (nl_iterator it2(it.get()); it2.has_next(); it2.next()) {
                    if (it2.get_type() == RTT_ATTRIBUTE_TARGET_MAC) {
                        memcpy(bssid, it2.get_data(), sizeof(mac_addr));
                        ALOGI("retrived target mac : %02x:%02x:%02x:%02x:%02x:%02x\n",
                                bssid[0],
                                bssid[1],
                                bssid[2],
                                bssid[3],
                                bssid[4],
                                bssid[5]);
                    } else if (it2.get_type() == RTT_ATTRIBUTE_RESULT_FREQ) {
                        channel = it2.get_u32();
                        if (rttResults[currentIdx] == NULL) {
                            ALOGE("Not allocated, currentIdx %d\n", currentIdx);
                            break;
                        }
                        if (!channel) {
                            rttResults[currentIdx]->frequency = UNSPECIFIED;
                        } else {
                            rttResults[currentIdx]->frequency = channel;
                        }
                        ALOGI("retrieved rtt_result : \n\tchannel :%d",
                            rttResults[currentIdx]->frequency);
                    } else if (it2.get_type() == RTT_ATTRIBUTE_RESULT_BW) {
                        bw = (wifi_rtt_bw)it2.get_u32();
                        if (rttResults[currentIdx] == NULL) {
                            ALOGE("Not allocated, currentIdx %d\n", currentIdx);
                            break;
                        }
                        rttResults[currentIdx]->packet_bw = bw;
                        ALOGI("retrieved rtt_result : \n\tpacket_bw :%d",
                            rttResults[currentIdx]->packet_bw);
                    } else if (it2.get_type() == RTT_ATTRIBUTE_RESULT_CNT) {
                        result_cnt = it2.get_u32();
                        ALOGI("retrieved result_cnt : %d\n", result_cnt);
                    } else if (it2.get_type() == RTT_ATTRIBUTE_RESULT) {
                        currentIdx = nextidx;
                        int result_len = it2.get_len();
                        rttResultsV1[currentIdx] =
                            (wifi_rtt_result *)malloc(it2.get_len());
                        wifi_rtt_result *rtt_results_v1 = rttResultsV1[currentIdx];
                        if (rtt_results_v1 == NULL) {
                            mCompleted = 1;
                            ALOGE("failed to allocate the wifi_result_v1\n");
                            break;
                        }

                        /* Populate to the rtt_results_v1 struct */
                        memcpy(rtt_results_v1, it2.get_data(), it2.get_len());

                        /* handle the optional data */
                        result_len -= RTT_RESULT_V1_SIZE;
                        if (result_len > 0) {
                            dot11_rm_ie_t *ele_1;
                            dot11_rm_ie_t *ele_2;
                            /* The result has LCI or LCR element */
                            ele_1 = (dot11_rm_ie_t *)(rtt_results_v1 + 1);
                            if (ele_1->id == DOT11_MNG_MEASURE_REPORT_ID) {
                                if (ele_1->type == DOT11_MEASURE_TYPE_LCI) {
                                    rtt_results_v1->LCI = (wifi_information_element *)ele_1;
                                    result_len -= (ele_1->len + DOT11_HDR_LEN);
                                    opt_result_size += (ele_1->len + DOT11_HDR_LEN);
                                    /* get a next rm ie */
                                    if (result_len > 0) {
                                        ele_2 = (dot11_rm_ie_t *)((char *)ele_1 +
                                            (ele_1->len + DOT11_HDR_LEN));
                                        if ((ele_2->id == DOT11_MNG_MEASURE_REPORT_ID) &&
                                                (ele_2->type == DOT11_MEASURE_TYPE_CIVICLOC)) {
                                            rtt_results_v1->LCR = (wifi_information_element *)ele_2;
                                        }
                                    }
                                } else if (ele_1->type == DOT11_MEASURE_TYPE_CIVICLOC){
                                    rtt_results_v1->LCR = (wifi_information_element *)ele_1;
                                    result_len -= (ele_1->len + DOT11_HDR_LEN);
                                    opt_result_size += (ele_1->len + DOT11_HDR_LEN);
                                    /* get a next rm ie */
                                    if (result_len > 0) {
                                        ele_2 = (dot11_rm_ie_t *)((char *)ele_1 +
                                                (ele_1->len + DOT11_HDR_LEN));
                                        if ((ele_2->id == DOT11_MNG_MEASURE_REPORT_ID) &&
                                                (ele_2->type == DOT11_MEASURE_TYPE_LCI)) {
                                            rtt_results_v1->LCI = (wifi_information_element *)ele_2;
                                        }
                                    }
                                }
                            }
                        }

                        /* Alloc new struct including new elements, reserve for new elements */
                        rttResults[currentIdx] =
                            (wifi_rtt_result_v2 *)malloc(RTT_RESULT_V2_SIZE + opt_result_size);
                        wifi_rtt_result_v2 *rtt_result_v2 = rttResults[currentIdx];
                        if (rtt_result_v2 == NULL) {
                            ALOGE("failed to allocate the rtt_result\n");
                            break;
                        }

                        /* Populate the new struct as per the legacy struct elements */
                        memcpy(&rtt_result_v2->rtt_result,
                            (wifi_rtt_result *)rtt_results_v1, RTT_RESULT_V1_SIZE);
                        if (!channel) {
                            rtt_result_v2->frequency = UNSPECIFIED;
                        }

                        /* Copy the optional data to new struct */
                        if (opt_result_size &&
                            (opt_result_size == (it2.get_len() - RTT_RESULT_V1_SIZE))) {

                            wifi_rtt_result_v2 *opt_rtt_result = NULL;
                            /* Intersect the optional data from legacy rtt result struct */
                            wifi_rtt_result *opt_legacy_rtt_result =
                                (wifi_rtt_result *)(rtt_results_v1 + RTT_RESULT_V1_SIZE);

                            /* shift dest buf by size of new rtt result struct */
                            opt_rtt_result =
                                (wifi_rtt_result_v2 *)rtt_result_v2 + RTT_RESULT_V2_SIZE;

                            /* Append optional rtt_result_v1 data to rtt_result_v2 */
                            memcpy(opt_rtt_result, opt_legacy_rtt_result,
                                (it2.get_len() - RTT_RESULT_V1_SIZE));
                        } else {
                           ALOGI("Optional rtt result elements missing, skip processing\n");
                        }

                        totalCnt++;
                        ALOGI("retrieved rtt_result : \n\tburst_num :%d, measurement_number : %d"
                                ", success_number : %d \tnumber_per_burst_peer : %d, status : %s,"
                                " retry_after_duration : %d s\n \trssi : %d dbm,"
                                " rx_rate : %d Kbps, rtt : %lu ns, rtt_sd : %lu\n"
                                "\tdistance : %d cm, burst_duration : %d ms,"
                                " negotiated_burst_num : %d\n",
                                rtt_results_v1->burst_num, rtt_results_v1->measurement_number,
                                rtt_results_v1->success_number,
                                rtt_results_v1->number_per_burst_peer,
                                get_err_info(rtt_results_v1->status),
                                rtt_results_v1->retry_after_duration,
                                rtt_results_v1->rssi, rtt_results_v1->rx_rate.bitrate * 100,
                                (unsigned long)rtt_results_v1->rtt/1000,
                                (unsigned long)rtt_results_v1->rtt_sd,
                                rtt_results_v1->distance_mm / 10,
                                rtt_results_v1->burst_duration,
                                rtt_results_v1->negotiated_burst_num);
                        nextidx = currentIdx;
                        nextidx++;
                    }
                }
            }
        }
        if (mCompleted) {
            unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
            {
                if (*rttHandler.on_rtt_results_v2) {
                    (*rttHandler.on_rtt_results_v2)(id(), totalCnt, rttResults);
                }
            }
            for (int i = 0; i < currentIdx; i++) {
                free(rttResults[i]);
                rttResults[i] = NULL;

                free(rttResultsV1[i]);
                rttResultsV1[i] = NULL;
            }
            totalCnt = currentIdx = nextidx = 0;
            WifiCommand *cmd = wifi_unregister_cmd(wifiHandle(), id());
            if (cmd)
                cmd->releaseRef();
        }
        return NL_SKIP;
    }
};


/* API to request RTT measurement */
wifi_error wifi_rtt_range_request(wifi_request_id id, wifi_interface_handle iface,
        unsigned num_rtt_config, wifi_rtt_config rtt_config[], wifi_rtt_event_handler handler)
{
    if (iface == NULL) {
        ALOGE("wifi_rtt_range_request: NULL iface pointer provided."
            " Exit.");
        return WIFI_ERROR_INVALID_ARGS;
    }

    wifi_handle handle = getWifiHandle(iface);
    if (handle == NULL) {
        ALOGE("wifi_rtt_range_request: NULL handle pointer provided."
            " Exit.");
        return WIFI_ERROR_INVALID_ARGS;
    }

    ALOGI("Rtt range_request; id = %d", id);
    RttCommand *cmd = new RttCommand(iface, id, num_rtt_config, rtt_config, handler);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = wifi_register_cmd(handle, id, cmd);
    if (result != WIFI_SUCCESS) {
        cmd->releaseRef();
        return result;
    }
    result = (wifi_error)cmd->start();
    if (result != WIFI_SUCCESS) {
        wifi_unregister_cmd(handle, id);
        cmd->releaseRef();
        return result;
    }
    return result;
}

/* API to cancel RTT measurements */
wifi_error wifi_rtt_range_cancel(wifi_request_id id,  wifi_interface_handle iface,
        unsigned num_devices, mac_addr addr[])
{
   if (iface == NULL) {
	ALOGE("wifi_rtt_range_cancel: NULL iface pointer provided."
		" Exit.");
	return WIFI_ERROR_INVALID_ARGS;
   }

    wifi_handle handle = getWifiHandle(iface);
    if (handle == NULL) {
	ALOGE("wifi_rtt_range_cancel: NULL handle pointer provided."
		" Exit.");
	return WIFI_ERROR_INVALID_ARGS;
    }

    ALOGI("Rtt range_cancel_request; id = %d", id);
    RttCommand *cmd = new RttCommand(iface, id);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    cmd->cancel_specific(num_devices, addr);
    wifi_unregister_cmd(handle, id);
    cmd->releaseRef();
    return WIFI_SUCCESS;
}

/* API to get RTT capability */
wifi_error wifi_get_rtt_capabilities(wifi_interface_handle iface,
        wifi_rtt_capabilities *capabilities)
{
    if (iface == NULL) {
	ALOGE("wifi_get_rtt_capabilities: NULL iface pointer provided."
		" Exit.");
	return WIFI_ERROR_INVALID_ARGS;
    }

    if (capabilities == NULL) {
	ALOGE("wifi_get_rtt_capabilities: NULL capabilities pointer provided."
		" Exit.");
	return WIFI_ERROR_INVALID_ARGS;
    }

    GetRttCapabilitiesCommand command(iface, capabilities);
    return (wifi_error) command.requestResponse();
}

/* API to get the responder information */
wifi_error wifi_rtt_get_responder_info(wifi_interface_handle iface,
        wifi_rtt_responder* responderInfo)
{
    if (iface == NULL) {
	ALOGE("wifi_rtt_get_responder_info: NULL iface pointer provided."
		" Exit.");
	return WIFI_ERROR_INVALID_ARGS;
    }

    GetRttResponderInfoCommand command(iface, responderInfo);
    return (wifi_error) command.requestResponse();

}

/**
 * Enable RTT responder mode.
 * channel_hint - hint of the channel information where RTT responder should be enabled on.
 * max_duration_seconds - timeout of responder mode.
 * wifi_rtt_responder - information for RTT responder e.g. channel used and preamble supported.
 */
wifi_error wifi_enable_responder(wifi_request_id id, wifi_interface_handle iface,
                                wifi_channel_info channel_hint, unsigned max_duration_seconds,
                                wifi_rtt_responder* responderInfo)
{
    EnableResponderCommand command(iface, id, channel_hint, max_duration_seconds, responderInfo);
    return (wifi_error) command.requestResponse();
}

/**
 * Disable RTT responder mode.
 */
wifi_error wifi_disable_responder(wifi_request_id id, wifi_interface_handle iface)
{
    CancelResponderCommand command(iface, id);
    return (wifi_error) command.requestResponse();
}

