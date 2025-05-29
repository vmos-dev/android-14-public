/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution, Apache license notifications and license are
 * retained for attribution purposes only.

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
#include <unistd.h>
#include <cutils/properties.h>
#include "hdmicec.h"
#include "rk_hdmi_connection.h"

#define SIGNAL_HDMI_HPD_PHYSICAL	0
#define SIGNAL_HDMI_HPD_STATUS_BIT	1


void hdmi_connection_get_port_info(struct hdmi_connection_context_t* ctx, struct hdmi_port_info* list[], int* total)
{
	ALOGI("%s", __func__);
#ifdef KER_CONFIG_HDMI_CEC
	int ret, val, fd, i;
	fd = ctx->fd;
	ALOGI("%s -- %s", __func__, HDMI_CONNECT_PATH);
	ret = ioctl(fd, RK_CEC_G_PORT_NUM, total);
	if (ret) {
		ALOGE("%s RK_CEC_G_PORT_NUM error:%s", __func__, strerror(errno));
		//close(fd);
	}
	if (*total > MAX_PORT)
		*total = MAX_PORT;

	if (*total == 0) {
		//close(fd);
	}

	if (NULL != ctx->port)
		delete[] ctx->port;
	ctx->port = new hdmi_port_info[*total];
	if (!ctx->port) {
		ALOGE("alloc port_data failed");
		*total = 0;
		//close(fd);
	}
	ret = ioctl(fd, RK_CEC_G_PORT_INFO, ctx->port);
	if (ret) {
		ALOGE("%s RK_CEC_G_PORT_INFO error:%s", __func__, strerror(errno));
		//close(fd);
	}

	for (i = 0; i < *total; i++) {
		if (ctx->port[i].type == HDMI_OUTPUT && ctx->fd > 0) {
			ret = ioctl(ctx->fd, CEC_ADAP_G_PHYS_ADDR, &val);
			if (!ret) {
				ALOGE("%s get port phy addr %x\n", __func__, val);
				if (val != 0xffff)
					ctx->port[i].physical_address = val;
			}
		}
		ALOGI("portId: %d, type:%s, cec support:%d, arc support:%d, physical address:%x",
			ctx->port[i].port_id,
			ctx->port[i].type ? "output" : "input",
			ctx->port[i].cec_supported,
			ctx->port[i].arc_supported,
			ctx->port[i].physical_address);
	}
	*list = ctx->port;
#else
	for (int i = 0;i < HDMI_PORT_NUM;i++)
	{
#if HDMI_PORT_TYPE == in
		ctx->port[i].type = HDMI_INPUT;
#else
		ctx->port[i].type = HDMI_OUTPUT;
#endif
		ctx->port[i].port_id = i + 1;//start from 1
		ctx->port[i].cec_supported = 1;
		ctx->port[i].arc_supported = 0;
		ctx->port[i].physical_address = 0x1000*(i+1);
	}
#if HDMI_PORT_TYPE == in
	//update port which support ARC
	//ctx->port[0].arc_supported = 1;
#endif
	*list = &ctx->port[0];
	*total = HDMI_PORT_NUM;
#ifdef DEBUG_HDMI_HAL
	for (int i = 0; i < *total; i++)
		ALOGI("port %d:type=%d,port_id=%d,PA:0x%04x", i, ctx->port[i].type, ctx->port[i].port_id, ctx->port[i].physical_address);
#endif
	ALOGI("%s,*total=%d", __func__, *total);
#endif
}

void hdmi_connection_register_event_callback(struct hdmi_connection_context_t* ctx, event_callback_t callback, void* arg)
{
	ALOGI("%s", __func__);
	ctx->connection_event_callback = callback;
	ctx->connection_arg = arg;
}

int hdmi_connection_is_connected(struct hdmi_connection_context_t* ctx, int port_id)
{
	int connected = HDMI_NOT_CONNECTED;
	ALOGI("%s port_id:%d", __func__, port_id);
#ifdef KER_CONFIG_HDMI_CEC
	return connected;
#else
	connected = ctx->hotplug ? HDMI_CONNECTED : HDMI_NOT_CONNECTED;
	ALOGI("%s port_id:%d is connected:%d", __func__, port_id, connected);
	return connected;
#endif
}

int hdmi_connection_set_phd_signal(struct hdmi_connection_context_t* ctx, int port_id, int signal)
{
	//struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	(void)ctx;(void)port_id;(void)signal;

	ALOGI("%s port_id:%d", __func__, port_id);
	return 0;
}

int hdmi_connection_get_phd_signal(struct hdmi_connection_context_t* ctx, int port_id)
{
	//struct hdmi_cec_context_t* ctx = (struct hdmi_cec_context_t*)dev;
	(void)ctx;(void)port_id;(void)signal;

	ALOGI("%s port_id:%d", __func__, port_id);
	return SIGNAL_HDMI_HPD_PHYSICAL;
}



int rk_hdmi_connection_destroy(struct hdmi_connection_context_t* ctx)
{
	ALOGI("rk_hdmi_connection_destroy.");

	if (ctx) {
		ctx->phy_addr = 0;
#ifndef KER_CONFIG_HDMI_CEC
		if (ctx->port != NULL)
			delete ctx->port;
#endif
		close(ctx->fd);
		//free(ctx);
	}

	return 0;
}


int rk_hdmi_connection_init(struct hdmi_connection_context_t *dev)
{
	ALOGI("rk_hdmi_connection_init.");
	if(NULL == dev){
		ALOGE("%s rk_hdmi_connection_init error!", __func__);
		return 0;
	}
	/* initialize our state here */
	memset(dev, 0, sizeof(struct hdmi_connection_context_t));

	dev->cec_init = false;
	dev->phy_addr = 0;
	dev->fd = open(HDMI_CONNECT_PATH, O_RDWR | O_CLOEXEC, 0);
	ALOGD("%s open %s", __func__, HDMI_CONNECT_PATH);
	if (dev->fd < 0) {
		ALOGE("%s open error!", __func__);
		ALOGE("hdmi connection errno:%d, %s\n", errno, strerror(errno));
	}
	ALOGI("%s dev->fd = %d", __func__, dev->fd);
	//property_set("vendor.sys.hdmicec.version",HDMI_CEC_HAL_VERSION);
#ifndef KER_CONFIG_HDMI_CEC
	if (dev->port == NULL)
		dev->port = new hdmi_port_info[HDMI_PORT_NUM];
#endif
	init_connection_uevent_thread(dev);

	ALOGI("rockchip hdmi connection modules loaded AIDL implementation library.");
	return 0;
}



