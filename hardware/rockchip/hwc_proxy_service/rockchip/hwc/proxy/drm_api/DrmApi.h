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
#include <errno.h>
#include <inttypes.h>
#include <log/log.h>
#include <stdint.h>
#include <string>
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "drm_fourcc.h"
#include "drm_mode.h"
#include <aidl/rockchip/hwc/proxy/aidl/IRkHwcProxyAidl.h>

using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlRequest;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlResponse;

typedef enum _drm_cmd {
    CMD_GET_RESOLUTION,
    CMD_SET_HDCP_ENABLE,
    CMD_GET_HDCP_ENABLE_STATUS,
    CMD_SET_HDCP_TYPE,
    CMD_GET_HDCP_ENCRYPTED_STATUS,
    CMD_SET_GAMMA,
    CMD_SET_3DLUT,
    CMD_SET_DVI_STATUS,
    CMD_GET_DVI_STATUS,
} drm_cmd;

struct drm_gamma {
    uint8_t dpy;
    uint32_t size;
    uint16_t red[1024];
    uint16_t green[1024];
    uint16_t blue[1024];
};

struct drm_cubic {
    uint8_t dpy;
    uint32_t size;
    uint16_t red[4913];
    uint16_t green[4913];
    uint16_t blue[4913];
};

class DrmApi {
    public:
    DrmApi();
    ~DrmApi();
    int drm_proc(int fd, const RkHwcProxyAidlRequest& request, RkHwcProxyAidlResponse* response);
    int get_resolution(int fd, int dpy, char* resolution);
    int set_gamma(int fd, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
    int set_3d_lut(int fd, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
    int set_hdcp_enable(int fd, int dpy, bool enable);
    int get_hdcp_enable_status(int fd, int dpy);
    int set_hdcp_type(int fd, int dpy, int type);
    int get_hdcp_encrypted_status(int fd, int dpy);
    int set_dvi_status(int fd, int dpy, int value);
    int get_dvi_status(int fd, int dpy);
private:
    drmModeConnectorPtr get_connector(int fd, int dpy);
    int get_property_id(int fd, drmModeObjectProperties *props, const char *name);
    int get_property_index(int fd, drmModeObjectProperties *props, const char *name);
};
