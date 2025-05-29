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

#include <aidl/android/hardware/tv/hdmi/connection/BnHdmiConnection.h>
#include <aidl/android/hardware/tv/hdmi/connection/Result.h>
#include <algorithm>
#include <vector>
#include "rk_hdmi_connection.h"


using namespace std;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace connection {
namespace implementation {

using ::aidl::android::hardware::tv::hdmi::connection::BnHdmiConnection;
using ::aidl::android::hardware::tv::hdmi::connection::HdmiPortInfo;
using ::aidl::android::hardware::tv::hdmi::connection::HdmiPortType;
using ::aidl::android::hardware::tv::hdmi::connection::HpdSignal;
using ::aidl::android::hardware::tv::hdmi::connection::IHdmiConnection;
using ::aidl::android::hardware::tv::hdmi::connection::IHdmiConnectionCallback;
using ::aidl::android::hardware::tv::hdmi::connection::Result;


#define MESSAGE_BODY_MAX_LENGTH 4

struct HdmiConnectionMock : public BnHdmiConnection {
    HdmiConnectionMock();

    ::ndk::ScopedAStatus getPortInfo(std::vector<HdmiPortInfo>* _aidl_return) override;
    ::ndk::ScopedAStatus isConnected(int32_t portId, bool* _aidl_return) override;
    ::ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IHdmiConnectionCallback>& callback) override;
    ::ndk::ScopedAStatus setHpdSignal(HpdSignal signal, int32_t portId) override;
    ::ndk::ScopedAStatus getHpdSignal(int32_t portId, HpdSignal* _aidl_return) override;

    void printEventBuf(const char* msg_buf, int len);
    static void eventCallback(const hdmi_event_t* event, void* /* arg */){
        if (mCallback != nullptr && event != nullptr) {
            if (event->type == HDMI_EVENT_CEC_MESSAGE) {
				#if 0
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
				#endif
            } else if (event->type == HDMI_EVENT_HOT_PLUG) {
                hotplug_event hotplugEvent {
                    .connected = event->hotplug.connected > 0,
                    .port_id = static_cast<int>(event->hotplug.port_id)
                };
                mCallback->onHotplugEvent(hotplugEvent.connected, hotplugEvent.port_id);
				//mCallback->onHotplugEvent(hotplugEvent);
            }
        }
    }


  private:
    static void serviceDied(void* cookie);
    static std::shared_ptr<IHdmiConnectionCallback> mCallback;

	//Variables for RK implementation
    struct hdmi_connection_context_t rkdev;

    // Variables for the virtual HDMI hal impl
    std::vector<HdmiPortInfo> mPortInfos;
    //std::vector<bool> mPortConnectionStatus;

    // Port configuration
    //uint16_t mPhysicalAddress = 0xFFFF;
    //will update by hdmi HAL(or KERENL) config
    int mTotalPorts = 1;

    // HPD Signal being used
    //std::vector<HpdSignal> mHpdSignal;


    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
};
}  // namespace implementation
}  // namespace connection
}  // namespace hdmi
}  // Namespace tv
}  // namespace hardware
}  // namespace android
