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

#include <aidl/rockchip/hwc/proxy/aidl/IRkHwcProxyAidl.h>
#include <aidl/rockchip/hwc/proxy/aidl/BnRkHwcProxyAidlCallback.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <stdlib.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

#include <DrmApi.h>

using ::aidl::rockchip::hwc::proxy::aidl::IRkHwcProxyAidl;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlRequest;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlResponse;
using ::aidl::rockchip::hwc::proxy::aidl::IRkHwcProxyAidlCallback;

using ::ndk::SpAIBinder;
using namespace std;

using ndk::SharedRefBase;
using ndk::ScopedAStatus;

namespace aidl::rockchip::hwc::proxy::aidl {
    class RkHwcProxyClient {
    public:
        class RkHwcProxyAidlCB : public BnRkHwcProxyAidlCallback {
        public:
            RkHwcProxyClient& client_;
            RkHwcProxyAidlCB(RkHwcProxyClient& client):client_(client){};
            ::ndk::ScopedAStatus asyncDone(const RkHwcProxyAidlResponse& response) override {
              //ALOGD("Received %s", response.toString().c_str());
              return ScopedAStatus::ok();
            }

              ::ndk::ScopedAStatus onCallback(const RkHwcProxyAidlRequest& request, RkHwcProxyAidlResponse* response) override {
              return ScopedAStatus::ok();
              }
        };
        RkHwcProxyClient();
         ~RkHwcProxyClient();
        int setup();
        int getResolutionInfo(int dpy, char* resolution);
        int setGamma(int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
        int set3DLut(int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
        int set_hdcp_enable(int dpy, bool enable);
        int get_hdcp_enable_status(int dpy);
        int set_hdcp_type(int dpy, int type);
        int get_hdcp_encrypted_status(int dpy);
        int set_dvi_status(int dpy, int value);
        int get_dvi_status(int dpy);
    private:
        std::shared_ptr<IRkHwcProxyAidl> client;
        void run(const RkHwcProxyAidlRequest& req, RkHwcProxyAidlResponse* resp);

    };
}
