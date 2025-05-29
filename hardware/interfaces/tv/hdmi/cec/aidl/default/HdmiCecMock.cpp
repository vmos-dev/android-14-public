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

#define LOG_TAG "android.hardware.tv.hdmi.cec"
#include <android-base/logging.h>
#include <fcntl.h>
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/hdmi_cec.h>
#include "HdmiCecMock.h"

using ndk::ScopedAStatus;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace cec {
namespace implementation {

std::shared_ptr<IHdmiCecCallback> HdmiCecMock::mCallback = nullptr;


void HdmiCecMock::serviceDied(void* cookie) {
    ALOGE("HdmiCecMock died");

    auto hdmiCecMock = static_cast<HdmiCecMock*>(cookie);
	mCallback = nullptr;
	rk_hdmi_cec_destroy(&(hdmiCecMock->rkdev));
}

ScopedAStatus HdmiCecMock::addLogicalAddress(CecLogicalAddress addr, Result* _aidl_return) {
	ALOGD("%s.", __FUNCTION__);

    int ret = hdmi_cec_add_logical_address(&rkdev, static_cast<cec_logical_address_t>(addr));
    switch (ret) {
        case 0:
			*_aidl_return = Result::SUCCESS;
			break;
        case -EINVAL:
			*_aidl_return = Result::FAILURE_INVALID_ARGS;
			break;
        case -ENOTSUP:
			*_aidl_return = Result::FAILURE_NOT_SUPPORTED;
			break;
        case -EBUSY:
			*_aidl_return = Result::FAILURE_BUSY;
			break;
        default:
			*_aidl_return = Result::FAILURE_UNKNOWN;
			break;
    }

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::clearLogicalAddress() {
	ALOGD("%s.", __FUNCTION__);
	hdmi_cec_clear_logical_address(&rkdev);

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::enableAudioReturnChannel(int32_t portId, bool enable) {
	ALOGD("%s.", __FUNCTION__);
	hdmi_cec_set_audio_return_channel(&rkdev, portId, enable ? 1 : 0);

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::getCecVersion(int32_t* _aidl_return) {
	ALOGD("%s.", __FUNCTION__);
    int version;
    hdmi_cec_get_version(&rkdev, &version);
	*_aidl_return = static_cast<int32_t>(version);

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::getPhysicalAddress(int32_t* _aidl_return) {
	ALOGD("%s.", __FUNCTION__);
    uint16_t addr = 0xFFFF;
    int ret = hdmi_cec_get_physical_address(&rkdev, &addr);
    switch (ret) {
        case 0:
			*_aidl_return = addr;
            break;
        case -EBADF:
			*_aidl_return = addr;
            break;
        default:
			*_aidl_return = addr;
            break;
    }

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::getVendorId(int32_t* _aidl_return) {
	ALOGD("%s.", __FUNCTION__);
    uint32_t vendor_id;
    hdmi_cec_get_vendor_id(&rkdev, &vendor_id);
	*_aidl_return = vendor_id;

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::sendMessage(const CecMessage& message, SendMessageResult* _aidl_return) {
    if (message.body.size() > CEC_MESSAGE_BODY_MAX_LENGTH) {
        ALOGW("message body is too long(%zu > %d)", message.body.size(), CEC_MESSAGE_BODY_MAX_LENGTH);
        *_aidl_return = SendMessageResult::FAIL;
    }
    else {
        //ALOGD("%s %s", __FUNCTION__, message.toString().c_str());
        cec_message_t legacyMessage{
          .initiator = static_cast<cec_logical_address_t>(message.initiator),
          .destination = static_cast<cec_logical_address_t>(message.destination),
          .length = message.body.size(),
        };
        if (message.body.size() > 0)
        {
            for (size_t i = 0; i < message.body.size(); ++i) {
                legacyMessage.body[i] = static_cast<unsigned char>(message.body[i]);
            }
        }
        *_aidl_return = static_cast<SendMessageResult>(hdmi_cec_send_message(&rkdev, &legacyMessage));
    }

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::setCallback(const std::shared_ptr<IHdmiCecCallback>& callback) {
	ALOGD("%s.", __FUNCTION__);
    // If callback is null, mCallback is also set to null so we do not call the old callback.
    mCallback = callback;

    if (callback != nullptr) {
        AIBinder_linkToDeath(this->asBinder().get(), mDeathRecipient.get(), 0 /* cookie */);

		hdmi_cec_register_event_callback(&rkdev, eventCallback, nullptr);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::setLanguage(const std::string& language) {
	ALOGD("%s.", __FUNCTION__);
    if (language.size() != 3) {
        LOG(ERROR) << "Wrong language code: expected 3 letters, but it was " << language.size()
                   << ".";
        return ScopedAStatus::ok();
    }

    const char *languageStr = language.c_str();
    int convertedLanguage = ((languageStr[0] & 0xFF) << 16)
            | ((languageStr[1] & 0xFF) << 8)
            | (languageStr[2] & 0xFF);
	hdmi_cec_set_option(&rkdev, HDMI_OPTION_SET_LANG, convertedLanguage);

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::enableWakeupByOtp(bool value) {
	ALOGD("%s.", __FUNCTION__);
    hdmi_cec_set_option(&rkdev, HDMI_OPTION_WAKEUP, value ? 1 : 0);

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::enableCec(bool value) {
	ALOGD("%s.", __FUNCTION__);
    hdmi_cec_set_option(&rkdev, HDMI_OPTION_ENABLE_CEC, value ? 1 : 0);

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCecMock::enableSystemCecControl(bool value) {
	ALOGD("%s.", __FUNCTION__);
    hdmi_cec_set_option(&rkdev, HDMI_OPTION_SYSTEM_CEC_CONTROL, value ? 1 : 0);

    return ScopedAStatus::ok();
}

void HdmiCecMock::printCecMsgBuf(const char* msg_buf, int len) {
    int i, size = 0;
    const int bufSize = CEC_MESSAGE_BODY_MAX_LENGTH * 3;
    // Use 2 characters for each byte in the message plus 1 space
    char buf[bufSize] = {0};

    // Messages longer than max length will be truncated.
    for (i = 0; i < len && size < bufSize; i++) {
        size += sprintf(buf + size, " %02x", msg_buf[i]);
    }
    ALOGD("[halimp_aidl] %s, msg:%.*s", __FUNCTION__, size, buf);
}


HdmiCecMock::HdmiCecMock() {
    ALOGD("Opening a RK CEC HAL AIDL Implementation.");

	rk_hdmi_cec_init(&rkdev);

    mCallback = nullptr;
    mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(AIBinder_DeathRecipient_new(serviceDied));
}

}  // namespace implementation
}  // namespace cec
}  // namespace hdmi
}  // namespace tv
}  // namespace hardware
}  // namespace android
