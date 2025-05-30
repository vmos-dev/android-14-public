/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <aidl/android/hardware/tv/hdmi/cec/BnHdmiCec.h>
#include <algorithm>
#include <vector>
#include "rk_hdmi_cec.h"


using namespace std;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace cec {
namespace implementation {

using ::aidl::android::hardware::tv::hdmi::cec::BnHdmiCec;
using ::aidl::android::hardware::tv::hdmi::cec::CecLogicalAddress;
using ::aidl::android::hardware::tv::hdmi::cec::CecMessage;
using ::aidl::android::hardware::tv::hdmi::cec::IHdmiCec;
using ::aidl::android::hardware::tv::hdmi::cec::IHdmiCecCallback;
using ::aidl::android::hardware::tv::hdmi::cec::Result;
using ::aidl::android::hardware::tv::hdmi::cec::SendMessageResult;


struct HdmiCecMock : public BnHdmiCec {
    HdmiCecMock();
    ::ndk::ScopedAStatus addLogicalAddress(CecLogicalAddress addr, Result* _aidl_return) override;
    ::ndk::ScopedAStatus clearLogicalAddress() override;
    ::ndk::ScopedAStatus enableAudioReturnChannel(int32_t portId, bool enable) override;
    ::ndk::ScopedAStatus getCecVersion(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus getPhysicalAddress(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus getVendorId(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus sendMessage(const CecMessage& message,
                                     SendMessageResult* _aidl_return) override;
    ::ndk::ScopedAStatus setCallback(const std::shared_ptr<IHdmiCecCallback>& callback) override;
    ::ndk::ScopedAStatus setLanguage(const std::string& language) override;
    ::ndk::ScopedAStatus enableWakeupByOtp(bool value) override;
    ::ndk::ScopedAStatus enableCec(bool value) override;
    ::ndk::ScopedAStatus enableSystemCecControl(bool value) override;
    void printCecMsgBuf(const char* msg_buf, int len);
    static void eventCallback(const hdmi_event_t* event, void* /* arg */){
        if (mCallback != nullptr && event != nullptr) {
            if (event->type == HDMI_EVENT_CEC_MESSAGE) {
                size_t length = std::min(event->cec.length,
                        static_cast<size_t>(CEC_MESSAGE_BODY_MAX_LENGTH));
                CecMessage cecMessage {
                    .initiator = static_cast<CecLogicalAddress>(event->cec.initiator),
                    .destination = static_cast<CecLogicalAddress>(event->cec.destination),
                };
                cecMessage.body.resize(length);
                for (size_t i = 0; i < length; ++i) {
                    cecMessage.body[i] = static_cast<uint8_t>(event->cec.body[i]);
                }
                mCallback->onCecMessage(cecMessage);
            } else if (event->type == HDMI_EVENT_HOT_PLUG) {
                /*
                hotplug_event hotplugEvent {
                    .connected = event->hotplug.connected > 0,
                    .port_id = static_cast<int>(event->hotplug.port_id)
                };
                mCallback->onHotplugEvent(hotplugEvent);
              */
            }
        }
    }


  private:
    static void serviceDied(void* cookie);
    static std::shared_ptr<IHdmiCecCallback> mCallback;

    //Variables for RK implementation
    struct hdmi_cec_context_t rkdev;

    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
};
}  // namespace implementation
}  // namespace cec
}  // namespace hdmi
}  // namespace tv
}  // namespace hardware
}  // namespace android
