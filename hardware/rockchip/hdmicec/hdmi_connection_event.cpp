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

#include <hardware_legacy/uevent.h>
#include <utils/Log.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>
#include <hardware/hdmi_cec.h>
#include <errno.h>
#include <hdmicec.h>
#include <unistd.h>
#include <pthread.h>
#include "rk_hdmi_connection.h"
#define HDMI_CONNECTION_UEVENT_THREAD_NAME "HdmiConnectionThread"

static int validcecmessage(hdmi_event_t cec_event)
{
	int ret = 0;

	if (cec_event.cec.length > 15)
		ret = -E2BIG;
	return ret;
}

/*
 * Kernel can't report initial HPD state, because when HDMI
 * driver is initialized HAL has not yet created cec driver's
 * adapter devnode. HDMI HPD state can't be reported without
 * adapter devnode, so we should get HDMI initial HPD state
 * through HDMI HPD state node when cec adapter just finished
 * initialization.
 */
static bool connection_get_hpd_state_from_node(hdmi_connection_context_t* ctx)
{
	int ret, fd;
	char statebuf[20];

	ALOGI("%s", __func__);
	memset(statebuf, 0, sizeof(statebuf));
	fd = open(HDMI_STATE_PATH, O_RDONLY);
	if (fd < 0)
		return -ENOENT;
	ret = read(fd, statebuf, sizeof(statebuf));
	close(fd);
	if (ret < 0) {
		ALOGE("read hdmi state err\n");
		return -EINVAL;
	}

	if (!strcmp(statebuf, "connected\n")) {
		return HDMI_CONNECTED;
	} else if (!strcmp(statebuf, "disconnected\n")) {
		return HDMI_NOT_CONNECTED;
	}

	ALOGE("%s can't get hdmi status HDMI_NOT_CONNECTED\n", __func__);
	return HDMI_NOT_CONNECTED;
}

static void connection_report_hdp_event(hdmi_connection_context_t* ctx, bool hpd)
{
	hdmi_event_t cec_event;

	cec_event.type = HDMI_EVENT_HOT_PLUG;
	//cec_event.dev = &ctx->device;
	if (hpd)
		cec_event.hotplug.connected = HDMI_CONNECTED;
	else
		cec_event.hotplug.connected = HDMI_NOT_CONNECTED;
	cec_event.hotplug.port_id = HDMI_CEC_PORT_ID;
	if (ctx->connection_event_callback)
		ctx->connection_event_callback(&cec_event, ctx->connection_arg);
}

static void *connection_uevent_loop(void *param)
{
	hdmi_connection_context_t * ctx = reinterpret_cast<hdmi_connection_context_t *>(param);
	char thread_name[64] = HDMI_CONNECTION_UEVENT_THREAD_NAME;
	hdmi_event_t cec_event;
	struct pollfd pfd[2];
	int fd[2];
	int ret, i;

	prctl(PR_SET_NAME, (unsigned long) &thread_name, 0, 0, 0);
	setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

	fd[0] = ctx->fd;
	if (fd[0] < 0) {
		ALOGE ("%s:not able to open cec state node", __func__);
		return NULL;
	}

	pfd[0].fd = fd[0];
	if (pfd[0].fd >= 0)
		pfd[0].events = POLLIN | POLLRDNORM | POLLPRI;

	while (true) {
                usleep(1000);
		int err = poll(&pfd[0], 1, 20);

		if (!err) {
			continue;
		} else if(err > 0) {
			#if 0
			if (!ctx->enable || !ctx->system_control)
				continue;
			#endif
			ALOGI("poll revent:%02x\n", pfd[0].revents);
			memset(&cec_event, 0, sizeof(hdmi_event_t));

			if (pfd[0].revents & (POLLPRI)) {
				int state = -1;
				struct cec_event event;

				//ALOGI("poll receive event\n");
				ret = ioctl(pfd[0].fd, CEC_DQEVENT, &event);
				if (!ret) {
					ALOGI("event:%d", event.event);
					if (event.event == CEC_EVENT_PIN_HPD_LOW) {
						ALOGI("CEC_EVENT_PIN_HPD_LOW -");
						ctx->hotplug = false;
						cec_event.type = HDMI_EVENT_HOT_PLUG;
						//cec_event.dev = &ctx->device;
						cec_event.hotplug.connected = HDMI_NOT_CONNECTED;
						cec_event.hotplug.port_id = HDMI_CEC_PORT_ID;
						if (ctx->connection_event_callback)
							ctx->connection_event_callback(&cec_event, ctx->connection_arg);
					} else if (event.event == CEC_EVENT_PIN_HPD_HIGH) {
						ALOGI("CEC_EVENT_PIN_HPD_HIGH +");
						ctx->hotplug = true;
						cec_event.type = HDMI_EVENT_HOT_PLUG;
						//cec_event.dev = &ctx->device;
						cec_event.hotplug.connected = HDMI_CONNECTED;
						cec_event.hotplug.port_id = HDMI_CEC_PORT_ID;
						if (ctx->connection_event_callback)
							ctx->connection_event_callback(&cec_event, ctx->connection_arg);
					} else if (event.event == CEC_EVENT_STATE_CHANGE) {
						ALOGD("adapt state change,phy_addr:%x,flags:%x\n", event.state_change.phys_addr, event.flags);

						/*
						 * Before cec HAL is initialized, hdmi hpd state may be
						 * changed. So we should confirm the hpd status
						 * after cec is initialized(Kernel will report
						 * CEC_EVENT_FL_INITIAL_STATE to notify HAL that
						 * initialization is done).
						 */
						if (event.flags & CEC_EVENT_FL_INITIAL_STATE) {
							ALOGD("cec adapter init complete, get connect state");
							ctx->hotplug = connection_get_hpd_state_from_node(ctx);
							ctx->cec_init = true;

							/*
							 * Framework will start la polling when box turn on,
							 * In addition, as soon as framewrok receives hdmi
							 * plug in, it will start la polling immediately.
							 * There is not need to report plug in event if hdmi
							 * is connecting when box turn on. So we should report
							 * hdmi plug out only.
							 */
							if (!ctx->hotplug)
								connection_report_hdp_event(ctx, ctx->hotplug);
						}
						ctx->phy_addr = event.state_change.phys_addr;
					}
				} else {
					ALOGE("%s cec event get err, ret:%d", __func__, ret);
				}
			}
		} else {
			ALOGE("%s: cec poll failed errno: %s", __FUNCTION__,
			strerror(errno));
			continue;
		}
	}
	return NULL;
}

void init_connection_uevent_thread(struct hdmi_connection_context_t* ctx)
{
	pthread_t uevent_thread;
	int ret;

	ALOGI("Initializing CONNECTION UEVENT Thread");
	ret = pthread_create(&uevent_thread, NULL, connection_uevent_loop, (void*) ctx);
	if (ret) {
		ALOGE("%s: failed to create %s: %s", __FUNCTION__,
		      HDMI_CONNECTION_UEVENT_THREAD_NAME, strerror(ret));
	}
}

