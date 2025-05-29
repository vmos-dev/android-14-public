/*
 * Copyright (C) 2023 Fuzhou Rockchip Electronics Co.Ltd
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

#include <cutils/properties.h>
#include "rk_hwc_proxy_client.h"

namespace aidl::rockchip::hwc::proxy::aidl {

RkHwcProxyClient::RkHwcProxyClient() {
};

RkHwcProxyClient::~RkHwcProxyClient() {
};

int RkHwcProxyClient::setup() {
    const std::string instance = std::string() + IRkHwcProxyAidl::descriptor + "/default";
    client = IRkHwcProxyAidl::fromBinder(SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    if (client == nullptr) {
        ALOGE("Failed to get rkhwcproxy aidl service");
        return -1;
    }
      return 0;
}

void RkHwcProxyClient::run(const RkHwcProxyAidlRequest& req, RkHwcProxyAidlResponse* resp) {
    client->run(req, resp);
}

int RkHwcProxyClient::getResolutionInfo(int dpy, char* resolution) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_GET_RESOLUTION;
    std::vector<uint8_t> data(1);//dpy
    data[0] = dpy;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        for (int i = 0; i < 256; i++) {
            resolution[i] = response->value[i];
            ALOGD("getResolutionInfo: %s", resolution);
        }
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::setGamma(int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_SET_GAMMA;
    drm_gamma gamma;
    gamma.dpy = dpy;
    gamma.size = size;
    for (int i = 0; i < size; i++) {
        gamma.red[i] = r[i];
        gamma.green[i] = g[i];
        gamma.blue[i] = b[i];
    }
    request.req_len = sizeof(gamma);
    std::vector<uint8_t> data((uint8_t*)&gamma, (uint8_t*)&gamma + sizeof(gamma));
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ALOGD("setGamma ret %d", response->value[0]);
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::set3DLut(int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_SET_3DLUT;
    drm_cubic cubic;
    cubic.dpy = dpy;
    cubic.size = size;
    for (int i = 0; i < size; i++) {
        cubic.red[i] = r[i];
        cubic.green[i] = g[i];
        cubic.blue[i] = b[i];
    }
    request.req_len = sizeof(cubic);
    std::vector<uint8_t> data((uint8_t*)&cubic, (uint8_t*)&cubic + sizeof(cubic));
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ALOGD("set3DLut ret %d", response->value[0]);
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::set_hdcp_enable(int dpy, bool enable) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_SET_HDCP_ENABLE;
    std::vector<uint8_t> data(2);//dpy
    data[0] = dpy;
    data[1] = enable;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::get_hdcp_enable_status(int dpy) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_GET_HDCP_ENABLE_STATUS;
    std::vector<uint8_t> data(1);//dpy
    data[0] = dpy;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::set_hdcp_type(int dpy, int type) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_SET_HDCP_TYPE;
    std::vector<uint8_t> data(2);//dpy
    data[0] = dpy;
    data[1] = type;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::get_hdcp_encrypted_status(int dpy) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_GET_HDCP_ENABLE_STATUS;
    std::vector<uint8_t> data(1);//dpy
    data[0] = dpy;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::set_dvi_status(int dpy, int value) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_SET_DVI_STATUS;
    std::vector<uint8_t> data(2);//dpy
    data[0] = dpy;
    data[1] = value;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

int RkHwcProxyClient::get_dvi_status(int dpy) {
    int ret = 0;
    RkHwcProxyAidlRequest request;
    RkHwcProxyAidlResponse *response = new RkHwcProxyAidlResponse();
    request.id = CMD_GET_DVI_STATUS;
    std::vector<uint8_t> data(1);//dpy
    data[0] = dpy;
    request.data = data;
    run(request, response);
    if (response != nullptr && !response->value.empty() && response->id == request.id) {
        ret = response->value[0];
    } else {
        ret = -1;
    }
    return ret;
}

}
