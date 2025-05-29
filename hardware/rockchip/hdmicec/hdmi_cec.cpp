/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/hdmi_cec.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <stdlib.h>
#include <string.h>
#include <hdmicec.h>
#include <unistd.h>
#include <cutils/properties.h>
#include "rk_hdmi_cec.h"
static int logicaddr_to_type(cec_logical_address_t addr)
{
	int type;

	ALOGI("%s", __func__);
	switch (addr) {
	case CEC_ADDR_TV:
		type = CEC_LOG_ADDR_TYPE_TV;
	break;
	case CEC_ADDR_RECORDER_1:
	case CEC_ADDR_RECORDER_2:
	case CEC_ADDR_RECORDER_3:
		type = CEC_LOG_ADDR_TYPE_RECORD;
	break;
	case CEC_ADDR_TUNER_1:
	case CEC_ADDR_TUNER_2:
	case CEC_ADDR_TUNER_3:
	case CEC_ADDR_TUNER_4:
		type = CEC_LOG_ADDR_TYPE_TUNER;
	break;
	case CEC_ADDR_PLAYBACK_1:
	case CEC_ADDR_PLAYBACK_2:
	case CEC_ADDR_PLAYBACK_3:
		type = CEC_LOG_ADDR_TYPE_PLAYBACK;
	break;
	case CEC_ADDR_AUDIO_SYSTEM:
		type = CEC_LOG_ADDR_TYPE_AUDIOSYSTEM;
	break;
	default:
		type = -1;
	}

	return type;
}

static int latype_to_devtype(int latype)
{
	int devtype;

	ALOGI("%s", __func__);
	switch (latype) {
	case CEC_LOG_ADDR_TYPE_TV:
		devtype = CEC_OP_PRIM_DEVTYPE_TV;
	break;
	case CEC_LOG_ADDR_TYPE_RECORD:
		devtype = CEC_OP_PRIM_DEVTYPE_RECORD;
	break;
	case CEC_LOG_ADDR_TYPE_TUNER:
		devtype = CEC_OP_PRIM_DEVTYPE_TUNER;
	break;
	case CEC_LOG_ADDR_TYPE_PLAYBACK:
		devtype = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
	break;
	case CEC_LOG_ADDR_TYPE_AUDIOSYSTEM:
		devtype = CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM;
	break;
	default:
		devtype = -1;
	}

	return devtype;
}

static int set_kernel_logical_address(struct hdmi_cec_context_t* ctx, cec_logical_address_t addr)
{
	int ret, la_type, dev_type, retry_num = 100;
	int mode = CEC_MODE_INITIATOR | CEC_MODE_EXCL_FOLLOWER_PASSTHRU;
	struct cec_log_addrs log_addr;

	ALOGD("%s, logic address:%02x\n", __func__, addr);

	if (ctx->fd < 0) {
		ALOGE("%s open error", __func__);
		return -ENOENT;
	}

	la_type = logicaddr_to_type(addr);
	if (la_type < 0) {
		ALOGE("%s invalid logic type\n", __func__);
		return -EINVAL;
	}

	dev_type = latype_to_devtype(la_type);
	if (dev_type < 0) {
		ALOGE("%s invalid device type\n", __func__);
		return -EINVAL;
	}

	ret = ioctl(ctx->fd, CEC_S_MODE, &mode);
	if (ret) {
		ALOGE("CEC set mode error!\n");
		return ret;
	}

	ret = ioctl(ctx->fd, CEC_ADAP_G_LOG_ADDRS, &log_addr);
	if (ret) {
		ALOGE("%s get logic address err ret:%d\n", __func__, ret);
		return -EINVAL;
	}

	ALOGI("primary_device_type:%02x,log_addr_type:%02x,log_addr[0]:%02x\n",
	      log_addr.primary_device_type[0], log_addr.log_addr_type[0], log_addr.log_addr[0]);
	if (log_addr.log_addr[0] != CEC_LOG_ADDR_INVALID && log_addr.log_addr[0]) {
		ALOGI("LA is existing, not need to set logic addr\n");
		return 0;
	}

	log_addr.cec_version = HDMI_CEC_VERSION;
	log_addr.num_log_addrs = 1;
	log_addr.log_addr[0] = addr;
	log_addr.vendor_id = HDMI_CEC_VENDOR_ID;
	log_addr.osd_name[0] = 'R';
	log_addr.osd_name[1] = 'K';
	log_addr.primary_device_type[0] = dev_type;
	log_addr.log_addr_type[0] = la_type;

la_retry:
	ret = ioctl(ctx->fd, CEC_ADAP_S_LOG_ADDRS, &log_addr);
	if (ret) {
		ALOGE("%s set logic address ioctl err ret:%d %s\n", __func__, ret, strerror(errno));
		if ((errno == -EBUSY) && retry_num) {
			usleep(10000);
			retry_num--;
			goto la_retry;
		}
		return -EBUSY;
	} else if (log_addr.log_addr[0] == 0xff) {
		if (retry_num) {
			usleep(10000);
			retry_num--;
			goto la_retry;
		}
		ALOGE("%s set logic address claim err la:0xff\n", __func__);
		return -EINVAL;
	}

	ALOGI("%s claim la success\n", __func__);
	return 0;
}

int hdmi_cec_add_logical_address(struct hdmi_cec_context_t* ctx ,cec_logical_address_t addr)
{
	ALOGI("%s", __func__);
	return set_kernel_logical_address(ctx, addr);
}

void hdmi_cec_clear_logical_address(struct hdmi_cec_context_t* ctx)
{
	int ret;
	struct cec_log_addrs log_addr;

	ALOGI("%s", __func__);
	if (ctx->fd < 0) {
		ALOGE("%s open error!", __func__);
		return;
	}

	if (!ctx->cec_init) {
		ALOGI("%s cec is not init!", __func__);
		return;
	}

	log_addr.num_log_addrs = 0;
	ret = ioctl(ctx->fd, CEC_ADAP_S_LOG_ADDRS, &log_addr);
	if (ret) {
		ALOGE("%s set logic address err ret:%d\n", __func__, ret);
		return;
	}
}

int hdmi_cec_get_physical_address(struct hdmi_cec_context_t* ctx, uint16_t* addr)
{
    int i = 0;
	uint16_t val = 0;

	ALOGI("%s", __func__);
	if (addr == NULL) {
		ALOGE("%s addr is null", __func__);
		return -ENXIO;
	}

	if (ctx->fd < 0) {
		ALOGE("%s open error!", __func__);
		return -ENOENT;
	}

	for (i = 0; i < 5; i++) {
		int ret = ioctl(ctx->fd, CEC_ADAP_G_PHYS_ADDR, &val);
		if (ret) {
			ALOGE("CEC read physical addr error! ret:%d\n", ret);
			return ret;
		}

		if(val != 0xffff) {
			break;
		}
		usleep(20000);
	}

	if (i == 5) {
		ALOGE("get phy addr err!:%x\n", val);
		return -EINVAL;
	}

	*addr = val;
	ALOGI("%s val = %x", __func__, val);

	return 0;
}

int hdmi_cec_is_connected(struct hdmi_cec_context_t* ctx, int port_id)
{
	(void)port_id;

	ALOGI("%s", __func__);
	if (ctx->hotplug)
		return HDMI_CONNECTED;
	else
		return HDMI_NOT_CONNECTED;
}

int hdmi_cec_send_message(struct hdmi_cec_context_t* ctx, const cec_message_t* message)
{
	struct cec_msg cecframe;
	int i = 0;
    int ret = 0;

	//ALOGI("%s", __func__);
	if (!ctx->enable) {
		ALOGE("%s cec disabled\n", __func__);
		return -EPERM;
	}

	if (ctx->fd < 0) {
		ALOGE("%s open error", __func__);
		return -ENOENT;
	}

	// if(!ctx->hotplug)
	// 	return -EPERM;

	memset(&cecframe, 0, sizeof(struct cec_msg));
	if (message->initiator == message->destination) {
		struct cec_log_addrs log_addr;

		ret = ioctl(ctx->fd, CEC_ADAP_G_LOG_ADDRS, &log_addr);
		if (ret) {
			ALOGE("%s get logic address err ret:%d\n", __func__, ret);
			return -EINVAL;
		}

		ALOGD("kernel logic addr:%02x, preferred logic addr:%02x",
		      log_addr.log_addr[0], message->initiator);
		if (log_addr.log_addr[0] != CEC_LOG_ADDR_INVALID && log_addr.log_addr[0]) {
			ALOGI("kernel logaddr is existing\n");
			if (log_addr.log_addr[0] == message->initiator) {
				ALOGI("kernel logaddr is preferred logaddr\n");
				return HDMI_RESULT_NACK;
			} else {
				ALOGI("preferred log addr is not kernel log addr\n");
				return HDMI_RESULT_SUCCESS;
			}
		} else {
			ALOGI("kernel logaddr is not existing\n");
			if(!set_kernel_logical_address(ctx, message->initiator)) {
				for (i = 0; i < 5; i++) {
					if (!ctx->phy_addr || ctx->phy_addr == 0xffff) {
						ALOGE("phy addr not ready\n");
						usleep(200000);
					} else {
						break;
					}
				}
			}
			if (i == 5) {
				ALOGE("can't make kernel addr done\n");
				return HDMI_RESULT_FAIL;
			} else {

				return HDMI_RESULT_NACK;
			}
		}
	}

	cecframe.msg[0] = (message->initiator << 4) | message->destination;
	cecframe.len = message->length + 1;
	cecframe.msg[1] = message->body[0];
	ALOGI("send msg LEN:%d,opcode:%02x,addr:%02x\n",
	      cecframe.len ,cecframe.msg[1],cecframe.msg[0]);
	if (cecframe.len > 16)
		cecframe.len = 0;
	for (ret = 0; ret < cecframe.len; ret++)
		cecframe.msg[ret + 2] = message->body[ret + 1];

	i = 10;
retry:
	ret = ioctl(ctx->fd, CEC_TRANSMIT, &cecframe);

 	if (ret < 0) {
		ALOGE("ioctl err:%d %s\n", ret, strerror(errno));
		return HDMI_RESULT_FAIL;
	}

	if (cecframe.tx_status & CEC_TX_STATUS_NACK) {
		ALOGW("HDMI_RESULT_NACK\n");
		return HDMI_RESULT_NACK;
	}
	else if (cecframe.tx_status & CEC_TX_STATUS_OK) {
		ALOGD("HDMI_RESULT_SUCCESS\n");
		return HDMI_RESULT_SUCCESS;
	}
	else if (cecframe.tx_status & CEC_TX_STATUS_ERROR) {
		ALOGW("HDMI_RESULT_BUSY\n");
		if (i) {
			i--;
			usleep(10000);
			goto retry;
		}
		return HDMI_RESULT_BUSY;
	}
	return HDMI_RESULT_FAIL;
}

void hdmi_cec_register_event_callback(struct hdmi_cec_context_t* ctx, event_callback_t callback, void* arg)
{
	ALOGI("%s", __func__);
	ctx->event_callback = callback;
	ctx->cec_arg = arg;
}

void hdmi_cec_get_version(struct hdmi_cec_context_t* ctx, int* version)
{
//	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	(void)ctx;

	ALOGI("%s", __func__);
	*version = HDMI_CEC_VERSION;
}

void hdmi_cec_get_vendor_id(struct hdmi_cec_context_t* ctx, uint32_t* vendor_id)
{
//	struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	(void)ctx;


	ALOGI("%s", __func__);
	*vendor_id = HDMI_CEC_VENDOR_ID;
}

static int set_kernel_cec_standy(struct hdmi_cec_context_t* ctx, bool enable)
{
	int ret = 0;
	int fd = 0;

	ALOGI("%s", __func__);
        fd = open(HDMI_WAKE_PATH, O_RDWR);
        if (fd < 0) {
                ALOGE("%s open error!", __func__);
                ALOGE("cec %s\n", strerror(errno));
		return errno;
        }

	ret = ioctl(fd, CEC_STANDBY, &enable);
	if (ret) {
		ALOGE("%s set kernel cec standby err %s\n", __func__, strerror(errno));
		close(fd);
		return ret;
	}

	close(fd);

	return 0;
}

#define CEC_ENABLE	0
#define CEC_WAKE	1

static int set_kernel_cec_wake_enable(struct hdmi_cec_context_t* ctx, int mask, bool enable)
{
	int ret, fd;

	ALOGI("%s", __func__);
	fd = open(HDMI_WAKE_PATH, O_RDWR);
	if (fd < 0) {
		ALOGE("%s open error!", __func__);
		ALOGE("cec %s\n", strerror(errno));
		return errno;
	}

	if (enable)
		ctx->en_mask |= (enable << mask);
	else
		ctx->en_mask &= ~(1 << mask);

	ALOGI("mask:%d, enable:%d, en_mask:%d\n", mask, enable, ctx->en_mask);
	ret = ioctl(fd, CEC_FUNC_EN, &ctx->en_mask);
	if (ret) {
		ALOGE("%s set kernel cec enable err ret:%d %s\n", __func__, ret, strerror(errno));
		close(fd);
		return ret;
	}

	close(fd);

	return 0;
}

void hdmi_cec_set_option(struct hdmi_cec_context_t* ctx, int flag, int value)
{
	int ret =0;

	ALOGI("%s", __func__);
	if (ctx->fd < 0) {
		ALOGE("%s open error", __func__);
		return;
	}

	switch (flag) {
		case HDMI_OPTION_WAKEUP:
			ALOGI("%s: Wakeup: value: %d", __FUNCTION__, value);
			set_kernel_cec_wake_enable(ctx, CEC_WAKE, !!value);
			break;
		case HDMI_OPTION_ENABLE_CEC:
			ALOGI("%s: Enable CEC: value: %d", __FUNCTION__, value);
			ctx->enable = !!value;
			set_kernel_cec_wake_enable(ctx, CEC_ENABLE, value);
			break;
		case HDMI_OPTION_SYSTEM_CEC_CONTROL:
			ALOGI("%s: system_control: value: %d",
			      __FUNCTION__, value);
			ctx->system_control = !!value;
			set_kernel_cec_standy(ctx, ctx->system_control);
			break;
	}
}

void hdmi_cec_set_audio_return_channel(struct hdmi_cec_context_t* ctx, int port_id, int flag)
{
	//struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	(void)ctx;(void)flag;

	ALOGI("%s %d", __func__, port_id);
}

int rk_hdmi_cec_destroy(struct hdmi_cec_context_t *ctx)
{
	ALOGI("rk_hdmi_cec_destroy.");

	if (ctx) {
		ctx->enable = false;
		ctx->phy_addr = 0;
		close(ctx->fd);
		//free(ctx);
	}

	return 0;
}

int rk_hdmi_cec_init(struct hdmi_cec_context_t *dev)
{
	ALOGI("rk_hdmi_cec_init.");
	if(NULL == dev){
		ALOGE("%s rk_hdmi_cec_init error!", __func__);
		return 0;
	}
	/* initialize our state here */
	memset(dev, 0, sizeof(struct hdmi_cec_context_t));

	dev->enable = true;
	dev->system_control = false;
	dev->cec_init = false;

	dev->phy_addr = 0;
	dev->en_mask = CEC_WAKE | CEC_ENABLE;
	dev->fd = open(HDMI_DEV_PATH,O_RDWR | O_CLOEXEC,0);
	ALOGD("%s open %s", __func__, HDMI_DEV_PATH);
	if (dev->fd < 0) {
		ALOGE("%s open error!", __func__);
		ALOGE("cec %s\n", strerror(errno));
	}
	ALOGI("%s dev->fd = %d", __func__, dev->fd);
	property_set("vendor.sys.hdmicec.version",HDMI_CEC_HAL_VERSION);

	init_uevent_thread(dev);

	ALOGI("rockchip hdmi cec modules loaded AIDL implementation library.");
	return 0;
}
