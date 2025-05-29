/*
 * Copyright 2022 The Android Open Source Project
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

#include "JTvInputHal.h"

#include <nativehelper/ScopedLocalRef.h>

//-----------------------rk code----------
#include <cutils/properties.h>
//----------------------------------------

namespace android {

//-----------------------rk code----------
char SUBTYPE_HDMINOUT[] = "hdmiinout";
char SUBTYPE_STREAM_CAPTURE[] = "stream_capture";
//----------------------------------------

JTvInputHal::JTvInputHal(JNIEnv* env, jobject thiz, std::shared_ptr<ITvInputWrapper> tvInput,
                         const sp<Looper>& looper) {
    mThiz = env->NewWeakGlobalRef(thiz);
    mTvInput = tvInput;
    mLooper = looper;
    mTvInputCallback = ::ndk::SharedRefBase::make<TvInputCallback>(this);
    //-----------------------rk code----------
    //mTvInput->setCallback(mTvInputCallback);
    mTvInput->setRkCallback(mTvInputCallback);
    //----------------------------------------
}

JTvInputHal::~JTvInputHal() {
    mTvInput->setCallback(nullptr);
    JNIEnv* env = AndroidRuntime::getJNIEnv();
    env->DeleteWeakGlobalRef(mThiz);
    mThiz = NULL;
}

JTvInputHal* JTvInputHal::createInstance(JNIEnv* env, jobject thiz, const sp<Looper>& looper) {
    sp<HidlITvInput> hidlITvInput = HidlITvInput::getService();
    if (hidlITvInput != nullptr) {
        ALOGD("tv.input service is HIDL.");
        return new JTvInputHal(env, thiz,
                               std::shared_ptr<ITvInputWrapper>(new ITvInputWrapper(hidlITvInput)),
                               looper);
    }
    std::shared_ptr<AidlITvInput> aidlITvInput = nullptr;
    //-----------------------rk code----------
    if (AServiceManager_isDeclared(TV_INPUT_AIDL_SERVICE_NAME)) {
        ::ndk::SpAIBinder binder(AServiceManager_waitForService(TV_INPUT_AIDL_SERVICE_NAME));
        aidlITvInput = AidlITvInput::fromBinder(binder);
        if (aidlITvInput == nullptr) {
            ALOGE("Couldn't get tv.input service.");
            return nullptr;
        }
        std::shared_ptr<AidlIRkTvInput> aidlIRkTvInput = nullptr;
        ndk::SpAIBinder binderExt;
        binder_status_t status = AIBinder_getExtension(binder.get(), binderExt.getR());
        if (STATUS_OK == status) {
            aidlIRkTvInput = AidlIRkTvInput::fromBinder(binderExt);
            if (aidlIRkTvInput == nullptr) {
                ALOGE("Couldn't get rk tv.input service.");
            }
        } else {
            ALOGE("Couldn't get tv.input ext service.");
        }
        return new JTvInputHal(env, thiz,
                               std::shared_ptr<ITvInputWrapper>(new ITvInputWrapper(aidlITvInput, aidlIRkTvInput)),
                               looper);
    }
    return nullptr;
    //----------------------------------------
}

int JTvInputHal::addOrUpdateStream(int deviceId, int streamId, const sp<Surface>& surface) {
    //-----------------------rk modify this method----------
    char c_prop_value[PROPERTY_VALUE_MAX] = {0};
    property_get("tvinput.hdmiin.buff_type", c_prop_value, "0");
    int prop_value = (int)atoi(c_prop_value);
    ALOGW("%s deviceId=%d, streamId=%d, prop=%d", __FUNCTION__, deviceId, streamId, prop_value);

    Mutex::Autolock autoLock(&mStreamLock);
    KeyedVector<int, Connection>& connections = mConnections.editValueFor(deviceId);
    if (connections.indexOfKey(streamId) < 0) {
        connections.add(streamId, Connection());
    }
    Connection& connection = connections.editValueFor(streamId);
    if (connection.mSurface == surface) {
        // Nothing to do
        return NO_ERROR;
    }

    connection.mStreamType = (prop_value == 1) ? TV_STREAM_TYPE_BUFFER_PRODUCER : TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;

    // Clear the surface in the connection.
    if (connection.mSurface != NULL) {
        if (connection.mStreamType == TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE) {
            if (Surface::isValid(connection.mSurface)) {
                if (connection.mCancelSourceHandle != NULL) {
                    ALOGW("%s setSidebandStream cacnelBuffer", __FUNCTION__);
                    connection.mSurface->setSidebandStream(connection.mCancelSourceHandle);
                    connection.mCancelSourceHandle.clear();
                    connection.mCancelSourceHandle = NULL;
                } else {
                    ALOGW("%s setSidebandStream NULL because connection.mSurface invalid", __FUNCTION__);
                    connection.mSurface->setSidebandStream(NULL);
                }
            }
        }
        connection.mSurface.clear();
    }

    bool bSidebandFlow = connection.mStreamType == TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE ? true : false;
    if (connection.mSourceHandle == NULL /*TODO waha check why delete it && connection.mThread == NULL*/) {
        // Need to configure stream
        ::ndk::ScopedAStatus status;
        std::vector<AidlRkTvStreamConfig> list;
        status = mTvInput->getRkStreamConfigurations(deviceId, &list);
        if (!status.isOk()) {
            ALOGE("Couldn't get stream configs for device id:%d result:%d", deviceId,
                  status.getServiceSpecificError());
            return UNKNOWN_ERROR;
        }
        int configIndex = -1;
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].base.streamId == streamId) {
                configIndex = i;
                break;
            }
        }
        if (configIndex == -1) {
            ALOGE("Cannot find a config with given stream ID: %d", streamId);
            return BAD_VALUE;
        }
        status = mTvInput->setRkPreviewInfo(deviceId, streamId, connection.mStreamType);
        if (!status.isOk()) {
            ALOGE("initPreviewBuffPoll setPreviewInfo failed");
            return BAD_VALUE;
        }

        if (!bSidebandFlow) {
            //ALOGW("not need getStreamConfigurations_ext again after setPreviewInfo");
            int width = list[configIndex].width;
            int height = list[configIndex].height;
            int format = list[configIndex].format;//HAL_PIXEL_FORMAT_YCrCb_NV12;
            long usage = list[configIndex].usage;
            int buffCount = list[configIndex].buffCount;
            ALOGW("stream config info: width=%d, height=%d, format=%d, usage=%ld, buffCount=%d",
                width, height, format, usage, buffCount);
            if (connection.mThread == NULL) {
                connection.mThread = new BufferProducerThread(this, deviceId);
            }
            int buffSize = connection.mThread->initPreviewBuffPoll(surface, &list[configIndex]);
            int compareCount = buffCount + 1;
            if (buffSize != compareCount) {
                ALOGE("initPreviewBuffPoll size failed");
                return BAD_VALUE;
            }
        }

        std::vector<AidlNativeHandle> sidebandStreamList;
        status = mTvInput->openRkStream(deviceId, streamId, &sidebandStreamList);
        if (!status.isOk() && bSidebandFlow) {
            ALOGE("Couldn't open stream. device id:%d stream id:%d result:%d", deviceId, streamId,
                  status.getServiceSpecificError());
            return UNKNOWN_ERROR;
        } else if (bSidebandFlow) {
            connection.mSourceHandle = NativeHandle::create(dupFromAidl(sidebandStreamList[0]), true);
            if (sidebandStreamList.size() == 2) {
                ALOGW("set connection.mCancelSourceHandle");
                connection.mCancelSourceHandle = NativeHandle::create(dupFromAidl(sidebandStreamList[1]), true);
            }
        }
        mTvInput->setRkTvMessageEnabled(deviceId, streamId, AidlTvMessageEventType::OTHER, true);
    }
    connection.mSurface = surface;
    if (connection.mSurface != nullptr && bSidebandFlow) {
        ALOGW("%s prepared setSidebandStream", __FUNCTION__);
        connection.mSurface->setSidebandStream(connection.mSourceHandle);
    } else if (connection.mSurface != nullptr) {
        ALOGW("start TvInputBufferProducerThread");
        connection.mThread->run("TvInputBufferProducerThread");
        connection.mThread->setSurface(surface);
    }
    //----------------------------------------

    return NO_ERROR;
}

int JTvInputHal::removeStream(int deviceId, int streamId) {
    //-----------------------rk modify this method----------
    Mutex::Autolock autoLock(&mStreamLock);
    ALOGW("%s deviceId=%d, streamId=%d", __FUNCTION__, deviceId, streamId);
    KeyedVector<int, Connection>& connections = mConnections.editValueFor(deviceId);
    if (connections.indexOfKey(streamId) < 0) {
        ALOGW("%s connections.indexOfKey(streamId) < 0 streamId=%d", __FUNCTION__, streamId);
        //-----------------------rk code----------
        //return BAD_VALUE;
        return NO_ERROR;
        //----------------------------------------
    }
    Connection& connection = connections.editValueFor(streamId);
    if (connection.mSurface == NULL) {
        ALOGW("%s connection.mSurface == NULL streamId=%d", __FUNCTION__, streamId);
        // Nothing to do
        return NO_ERROR;
    }
    if (Surface::isValid(connection.mSurface)) {
        if (connection.mStreamType == TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE) {
            if (connection.mCancelSourceHandle != NULL) {
                ALOGW("%s setSidebandStream cacnelBuffer", __FUNCTION__);
                connection.mSurface->setSidebandStream(connection.mCancelSourceHandle);
            } else {
                 ALOGW("%s setSidebandStream NULL because connection.mSurface invalid", __FUNCTION__);
                 connection.mSurface->setSidebandStream(NULL);
            }
        }
    }
    ALOGW("%s start mTvInput->closeStream deviceId=%d, streamId=%d", __FUNCTION__, deviceId, streamId);
    if (!mTvInput->closeRkStream(deviceId, streamId).isOk()) {
        ALOGE("Couldn't close stream. device id:%d stream id:%d", deviceId, streamId);
        return BAD_VALUE;
    }
    if (connection.mThread != NULL) {
        ALOGW("%s exit connection.mThread", __FUNCTION__);
        connection.mThread->shutdown();
        connection.mThread->requestExit();
        connection.mThread->requestExitAndWait();
        connection.mThread = NULL;
    }
    connection.mSurface.clear();
    connection.mSurface = NULL;
    if (connection.mSourceHandle != NULL) {
        ALOGW("%s %d clear connection.mSourceHandle", __FUNCTION__, __LINE__);
        connection.mSourceHandle.clear();
        connection.mSourceHandle = NULL;
    }
    if (connection.mCancelSourceHandle != NULL) {
        connection.mCancelSourceHandle.clear();
        connection.mCancelSourceHandle = NULL;
    }
    //----------------------------------------
    ALOGW("%s end.", __FUNCTION__);
    return NO_ERROR;
}

int JTvInputHal::setTvMessageEnabled(int deviceId, int streamId, int type, bool enabled) {
    Mutex::Autolock autoLock(&mLock);
    if (!mTvInput->setTvMessageEnabled(deviceId, streamId,
                                       static_cast<AidlTvMessageEventType>(type), enabled)
                 .isOk()) {
        ALOGE("Error in setTvMessageEnabled. device id:%d stream id:%d", deviceId, streamId);
        return BAD_VALUE;
    }
    return NO_ERROR;
}

const std::vector<AidlTvStreamConfig> JTvInputHal::getStreamConfigs(int deviceId) {
    std::vector<AidlTvStreamConfig> list;
    //-----------------------rk code----------
    ALOGW("%s deviceId=%d", __FUNCTION__, deviceId);
    std::vector<AidlRkTvStreamConfig> rkList;
    ::ndk::ScopedAStatus status = mTvInput->getRkStreamConfigurations(deviceId, &rkList);
    if (status.isOk()) {
        if (!rkList.empty()) {
            for (int i = 0; i < rkList.size(); i++) {
                list.push_back(rkList[i].base);
            }
        }
    } else {
        ALOGW("%s mTvInput->getStreamConfigurations deviceId=%d", __FUNCTION__, deviceId);
        status = mTvInput->getStreamConfigurations(deviceId, &list);
        if (!status.isOk()) {
            ALOGE("Couldn't get stream configs for device id:%d result:%d", deviceId,
                  status.getServiceSpecificError());
            return std::vector<AidlTvStreamConfig>();
        }
    }
    //----------------------------------------
    return list;
}

void JTvInputHal::onDeviceAvailable(const TvInputDeviceInfoWrapper& info) {
    {
        Mutex::Autolock autoLock(&mLock);
        mConnections.add(info.deviceId, KeyedVector<int, Connection>());
    }
    JNIEnv* env = AndroidRuntime::getJNIEnv();
    jobject builder = env->NewObject(gTvInputHardwareInfoBuilderClassInfo.clazz,
                                     gTvInputHardwareInfoBuilderClassInfo.constructor);
    env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.deviceId, info.deviceId);
    env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.type, info.type);
    if (info.type == TvInputType::HDMI) {
        env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.hdmiPortId,
                              info.portId);
    }
    env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.cableConnectionStatus,
                          info.cableConnectionStatus);
    if (info.isHidl) {
        hidlSetUpAudioInfo(env, builder, info);
    } else {
        AidlAudioDeviceType audioType = info.aidlAudioDevice.type.type;
        env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.audioType, audioType);
        if (audioType != AidlAudioDeviceType::NONE) {
            std::stringstream ss;
            switch (info.aidlAudioDevice.address.getTag()) {
                case AidlAudioDeviceAddress::id:
                    ss << info.aidlAudioDevice.address.get<AidlAudioDeviceAddress::id>();
                    break;
                case AidlAudioDeviceAddress::mac: {
                    std::vector<uint8_t> addrList =
                            info.aidlAudioDevice.address.get<AidlAudioDeviceAddress::mac>();
                    for (int i = 0; i < addrList.size(); i++) {
                        if (i != 0) {
                            ss << ':';
                        }
                        ss << std::uppercase << std::setfill('0') << std::setw(2) << std::hex
                           << static_cast<int32_t>(addrList[i]);
                    }
                } break;
                case AidlAudioDeviceAddress::ipv4: {
                    std::vector<uint8_t> addrList =
                            info.aidlAudioDevice.address.get<AidlAudioDeviceAddress::ipv4>();
                    for (int i = 0; i < addrList.size(); i++) {
                        if (i != 0) {
                            ss << '.';
                        }
                        ss << static_cast<int32_t>(addrList[i]);
                    }
                } break;
                case AidlAudioDeviceAddress::ipv6: {
                    std::vector<int32_t> addrList =
                            info.aidlAudioDevice.address.get<AidlAudioDeviceAddress::ipv6>();
                    for (int i = 0; i < addrList.size(); i++) {
                        if (i != 0) {
                            ss << ':';
                        }
                        ss << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
                           << addrList[i];
                    }
                } break;
                case AidlAudioDeviceAddress::alsa: {
                    std::vector<int32_t> addrList =
                            info.aidlAudioDevice.address.get<AidlAudioDeviceAddress::alsa>();
                    ss << "card=" << addrList[0] << ";device=" << addrList[1];
                } break;
            }
            std::string bufferStr = ss.str();
            jstring audioAddress = env->NewStringUTF(bufferStr.c_str());
            env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.audioAddress,
                                  audioAddress);
            env->DeleteLocalRef(audioAddress);
        }
    }

    jobject infoObject = env->CallObjectMethod(builder, gTvInputHardwareInfoBuilderClassInfo.build);

    env->CallVoidMethod(mThiz, gTvInputHalClassInfo.deviceAvailable, infoObject);

    env->DeleteLocalRef(builder);
    env->DeleteLocalRef(infoObject);
}

void JTvInputHal::onDeviceUnavailable(int deviceId) {
    {
        Mutex::Autolock autoLock(&mLock);
        KeyedVector<int, Connection>& connections = mConnections.editValueFor(deviceId);
        for (size_t i = 0; i < connections.size(); ++i) {
            removeStream(deviceId, connections.keyAt(i));
        }
        connections.clear();
        mConnections.removeItem(deviceId);
    }
    JNIEnv* env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(mThiz, gTvInputHalClassInfo.deviceUnavailable, deviceId);
}

void JTvInputHal::onStreamConfigurationsChanged(int deviceId, int cableConnectionStatus) {
    {
        Mutex::Autolock autoLock(&mLock);
        KeyedVector<int, Connection>& connections = mConnections.editValueFor(deviceId);
        for (size_t i = 0; i < connections.size(); ++i) {
            removeStream(deviceId, connections.keyAt(i));
        }
        connections.clear();
    }
    JNIEnv* env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(mThiz, gTvInputHalClassInfo.streamConfigsChanged, deviceId,
                        cableConnectionStatus);
}

void JTvInputHal::onTvMessage(int deviceId, int streamId, AidlTvMessageEventType type,
                              AidlTvMessage& message, signed char data[], int dataLength) {
    JNIEnv* env = AndroidRuntime::getJNIEnv();
    ScopedLocalRef<jobject> bundle(env,
                                   env->NewObject(gBundleClassInfo.clazz,
                                                  gBundleClassInfo.constructor));
    ScopedLocalRef<jbyteArray> convertedData(env, env->NewByteArray(dataLength));
    env->SetByteArrayRegion(convertedData.get(), 0, dataLength, reinterpret_cast<jbyte*>(data));
    std::string key = "android.media.tv.TvInputManager.raw_data";
    ScopedLocalRef<jstring> jkey(env, env->NewStringUTF(key.c_str()));
    env->CallVoidMethod(bundle.get(), gBundleClassInfo.putByteArray, jkey.get(),
                        convertedData.get());
    ScopedLocalRef<jstring> subtype(env, env->NewStringUTF(message.subType.c_str()));
    key = "android.media.tv.TvInputManager.subtype";
    jkey = ScopedLocalRef<jstring>(env, env->NewStringUTF(key.c_str()));
    env->CallVoidMethod(bundle.get(), gBundleClassInfo.putString, jkey.get(), subtype.get());
    key = "android.media.tv.TvInputManager.group_id";
    jkey = ScopedLocalRef<jstring>(env, env->NewStringUTF(key.c_str()));
    env->CallVoidMethod(bundle.get(), gBundleClassInfo.putInt, jkey.get(), message.groupId);
    key = "android.media.tv.TvInputManager.stream_id";
    jkey = ScopedLocalRef<jstring>(env, env->NewStringUTF(key.c_str()));
    env->CallVoidMethod(bundle.get(), gBundleClassInfo.putInt, jkey.get(), streamId);
    env->CallVoidMethod(mThiz, gTvInputHalClassInfo.tvMessageReceived, deviceId,
                        static_cast<jint>(type), bundle.get());
}

//-----------------------rk code----------
//void JTvInputHal::onCaptured(int deviceId, int streamId, uint32_t seq, bool succeeded) {
void JTvInputHal::onCaptured(int deviceId, int streamId, uint64_t buffId, uint32_t seq, bool succeeded) {
//----------------------------------------
    sp<BufferProducerThread> thread;
    {
        Mutex::Autolock autoLock(&mLock);
        KeyedVector<int, Connection>& connections = mConnections.editValueFor(deviceId);
        Connection& connection = connections.editValueFor(streamId);
        if (connection.mThread == NULL) {
            ALOGE("capture thread not existing.");
            return;
        }
        thread = connection.mThread;
    }
    //-----------------------rk code----------
    thread->onCaptured(buffId, seq, succeeded);
    /*thread->onCaptured(seq, succeeded);
    if (seq == 0) {
        JNIEnv* env = AndroidRuntime::getJNIEnv();
        env->CallVoidMethod(mThiz, gTvInputHalClassInfo.firstFrameCaptured, deviceId, streamId);
    }*/
    //----------------------------------------
}

JTvInputHal::TvInputDeviceInfoWrapper
JTvInputHal::TvInputDeviceInfoWrapper::createDeviceInfoWrapper(
        const AidlTvInputDeviceInfo& aidlTvInputDeviceInfo) {
    TvInputDeviceInfoWrapper deviceInfo;
    deviceInfo.isHidl = false;
    deviceInfo.deviceId = aidlTvInputDeviceInfo.deviceId;
    deviceInfo.type = aidlTvInputDeviceInfo.type;
    deviceInfo.portId = aidlTvInputDeviceInfo.portId;
    deviceInfo.cableConnectionStatus = aidlTvInputDeviceInfo.cableConnectionStatus;
    deviceInfo.aidlAudioDevice = aidlTvInputDeviceInfo.audioDevice;
    return deviceInfo;
}

JTvInputHal::TvInputEventWrapper JTvInputHal::TvInputEventWrapper::createEventWrapper(
        const AidlTvInputEvent& aidlTvInputEvent) {
    TvInputEventWrapper event;
    event.type = aidlTvInputEvent.type;
    event.deviceInfo =
            TvInputDeviceInfoWrapper::createDeviceInfoWrapper(aidlTvInputEvent.deviceInfo);
    return event;
}

JTvInputHal::TvMessageEventWrapper JTvInputHal::TvMessageEventWrapper::createEventWrapper(
        const AidlTvMessageEvent& aidlTvMessageEvent) {
    TvMessageEventWrapper event;
    event.messages.insert(event.messages.begin(), std::begin(aidlTvMessageEvent.messages) + 1,
                          std::end(aidlTvMessageEvent.messages));
    event.streamId = aidlTvMessageEvent.streamId;
    event.deviceId = aidlTvMessageEvent.messages[0].groupId;
    event.type = aidlTvMessageEvent.type;
    return event;
}

JTvInputHal::NotifyHandler::NotifyHandler(JTvInputHal* hal, const TvInputEventWrapper& event) {
    mHal = hal;
    mEvent = event;
}

void JTvInputHal::NotifyHandler::handleMessage(const Message& message) {
    switch (mEvent.type) {
        case TvInputEventType::DEVICE_AVAILABLE: {
            //-----------------------rk code----------
            ALOGW("handleMessage DEVICE_AVAILABLE deviceId=%d, portId=%d",
                mEvent.deviceInfo.deviceId, mEvent.deviceInfo.portId);
            //----------------------------------------
            mHal->onDeviceAvailable(mEvent.deviceInfo);
        } break;
        case TvInputEventType::DEVICE_UNAVAILABLE: {
            //-----------------------rk code----------
            ALOGW("handleMessage DEVICE_UNAVAILABLE deviceId=%d, portId=%d",
                mEvent.deviceInfo.deviceId, mEvent.deviceInfo.portId);
            //----------------------------------------
            mHal->onDeviceUnavailable(mEvent.deviceInfo.deviceId);
        } break;
        case TvInputEventType::STREAM_CONFIGURATIONS_CHANGED: {
            //-----------------------rk code----------
            ALOGW("handleMessage STREAM_CONFIGURATIONS_CHANGED deviceId=%d, portId=%d",
                mEvent.deviceInfo.deviceId, mEvent.deviceInfo.portId);
            //----------------------------------------
            int cableConnectionStatus = static_cast<int>(mEvent.deviceInfo.cableConnectionStatus);
            mHal->onStreamConfigurationsChanged(mEvent.deviceInfo.deviceId, cableConnectionStatus);
        } break;
        default:
            ALOGE("Unrecognizable event");
    }
}

JTvInputHal::NotifyTvMessageHandler::NotifyTvMessageHandler(JTvInputHal* hal,
                                                            const TvMessageEventWrapper& event) {
    mHal = hal;
    mEvent = event;
}

void JTvInputHal::NotifyTvMessageHandler::handleMessage(const Message& message) {
    std::shared_ptr<AidlMessageQueue<int8_t, SynchronizedReadWrite>> queue =
            mHal->mQueueMap[mEvent.deviceId][mEvent.streamId];
    for (AidlTvMessage item : mEvent.messages) {
        if (queue == NULL || !queue->isValid() || queue->availableToRead() < item.dataLengthBytes) {
            MQDescriptor<int8_t, SynchronizedReadWrite> queueDesc;
            //-----------------------rk code----------
            if (mHal->mTvInput->getRkTvMessageQueueDesc(&queueDesc, mEvent.deviceId, mEvent.streamId)
                        .isOk()) {
            //----------------------------------------
                queue = std::make_shared<AidlMessageQueue<int8_t, SynchronizedReadWrite>>(queueDesc,
                                                                                          false);
            }
            if (queue == NULL || !queue->isValid() ||
                queue->availableToRead() < item.dataLengthBytes) {
                ALOGE("Incomplete TvMessageQueue data or missing queue");
                return;
            }
            mHal->mQueueMap[mEvent.deviceId][mEvent.streamId] = queue;
        }
        signed char* buffer = new signed char[item.dataLengthBytes];
        if (queue->read(buffer, item.dataLengthBytes)) {
            //-----------------------rk code----------
            if (strcmp(item.subType.c_str(), SUBTYPE_STREAM_CAPTURE) == 0) {
                uint64_t buffId = item.groupId;
                ALOGW("%s read %s buffId=%" PRIu64, __FUNCTION__, buffer, buffId);
                mHal->onCaptured(mEvent.deviceId, mEvent.streamId,
                    buffId, 0, buffId != -1);
            } else {
                /*if (strcmp(item.subType.c_str(), SUBTYPE_HDMINOUT) == 0) {
                    ALOGW("%s read %s", __FUNCTION__, buffer);
                }*/
                mHal->onTvMessage(mEvent.deviceId, mEvent.streamId, mEvent.type, item, buffer,
                              item.dataLengthBytes);
            }
            //----------------------------------------
        } else {
            ALOGE("Failed to read from TvMessageQueue");
        }
        delete[] buffer;
    }
}

JTvInputHal::TvInputCallback::TvInputCallback(JTvInputHal* hal) {
    mHal = hal;
}

::ndk::ScopedAStatus JTvInputHal::TvInputCallback::notify(const AidlTvInputEvent& event) {
    mHal->mLooper->sendMessage(new NotifyHandler(mHal,
                                                 TvInputEventWrapper::createEventWrapper(event)),
                               static_cast<int>(event.type));
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus JTvInputHal::TvInputCallback::notifyTvMessageEvent(
        const AidlTvMessageEvent& event) {
    const std::string DEVICE_ID_SUBTYPE = "device_id";
    if (event.messages.size() > 1 && event.messages[0].subType == DEVICE_ID_SUBTYPE) {
        mHal->mLooper
                ->sendMessage(new NotifyTvMessageHandler(mHal,
                                                         TvMessageEventWrapper::createEventWrapper(
                                                                 event)),
                              static_cast<int>(event.type));
    }

    return ::ndk::ScopedAStatus::ok();
}

//-----------------------rk code----------
int JTvInputHal::privCmdFromApp(AidlRkTvPrivAppCmdInfo info) {
    ::ndk::ScopedAStatus status = mTvInput->privRkCmdFromApp(info);
    if (!status.isOk()) {
        ALOGE("%s result:%d", __FUNCTION__, status.getServiceSpecificError());
        return UNKNOWN_ERROR;
    }
    ALOGW("%s end.", __FUNCTION__);
    return NO_ERROR;
}

JTvInputHal::ITvInputWrapper::ITvInputWrapper(std::shared_ptr<AidlITvInput>& aidlTvInput,
                                                    std::shared_ptr<AidlIRkTvInput>& aidlRkTvInput)
      : mIsHidl(false), mAidlTvInput(aidlTvInput), mAidlRkTvInput(aidlRkTvInput) {}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::privRkCmdFromApp(AidlRkTvPrivAppCmdInfo in_info) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->privRkCmdFromApp(in_info);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::setRkCallback(
        const std::shared_ptr<TvInputCallback>& in_callback) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->setRkCallback(in_callback);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::getRkStreamConfigurations(
        int32_t in_deviceId, std::vector<AidlRkTvStreamConfig>* _aidl_return) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->getRkStreamConfigurations(in_deviceId, _aidl_return);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::openRkStream(int32_t in_deviceId,
                                                              int32_t in_streamId,
                                                              /*AidlNativeHandle* _aidl_return*/std::vector<AidlNativeHandle>* _aidl_return) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->openRkStream(in_deviceId, in_streamId, _aidl_return);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::closeRkStream(int32_t in_deviceId,
                                                               int32_t in_streamId) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->closeRkStream(in_deviceId, in_streamId);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::setRkTvMessageEnabled(int32_t deviceId,
                                                                       int32_t streamId,
                                                                       TvMessageEventType in_type,
                                                                       bool enabled) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->setRkTvMessageEnabled(deviceId, streamId, in_type, enabled);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::getRkTvMessageQueueDesc(
        MQDescriptor<int8_t, SynchronizedReadWrite>* out_queue, int32_t in_deviceId,
        int32_t in_streamId) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->getRkTvMessageQueueDesc(out_queue, in_deviceId, in_streamId);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::setRkPreviewInfo(
        int32_t deviceId, int32_t streamId, int32_t initType) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->setRkPreviewInfo(deviceId, streamId, initType);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::setSinglePreviewBuffer(uint64_t bufId, const native_handle* bufHandle) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->setSinglePreviewBuffer(bufId, makeToAidl(bufHandle));
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::requestRkCapture(
        int32_t deviceId, int32_t streamId, uint64_t bufId, uint32_t seq) {
    if (mIsHidl || mAidlRkTvInput == nullptr) {
        ALOGE("%s err status mIsHidl=%d or mAidlRkTvInput is null", __FUNCTION__, mIsHidl);
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlRkTvInput->requestRkCapture(deviceId, streamId, bufId, seq);
    }
}

JTvInputHal::BufferProducerThread::BufferProducerThread(
        JTvInputHal* hal, int deviceId)
    : Thread(false),
      mParentHal(hal) {
}

status_t JTvInputHal::BufferProducerThread::readyToRun() {
      mBuffer = NULL;
      mBufferState = RELEASED;
      mSeq = 0;
      mCurrBuffId = 0;
      mFirstCaptured = true;
      mShutdown = false;
      mAlreadyShowSignal = false;
      return OK;
}

status_t JTvInputHal::BufferProducerThread::configPreviewBuff() {
    sp<ANativeWindow> anw(mSurface);
    ALOGW("%s in", __FUNCTION__);
    status_t err = native_window_api_connect(anw.get(), NATIVE_WINDOW_API_CAMERA);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_api_connect failed. err=%d", __FUNCTION__, err);
        return err;
    } else {
        ALOGW("native_window_api_connect succeed");
    }

    int consumerUsage = 0;
    err = anw.get()->query(anw.get(), NATIVE_WINDOW_CONSUMER_USAGE_BITS, &consumerUsage);
    if (err != NO_ERROR) {
        ALOGW("failed to get consumer usage bits. ignoring");
        err = NO_ERROR;
    }
    ALOGW("consumerUsage=%d, usage_project=%d", consumerUsage, (consumerUsage & GRALLOC_USAGE_PROTECTED));

    err = native_window_set_usage(anw.get(), (mStream.usage|consumerUsage));
    if (err != NO_ERROR) {
        ALOGE("%s native_window_set_usage failed.", __FUNCTION__);
        return err;
    }

    // dequeueBuffer cannot time out
    mSurface->setDequeueTimeout(-1);

    err = native_window_set_buffers_dimensions(
            anw.get(), mStream.width, mStream.height);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_set_buffers_dimensions failed.", __FUNCTION__);
        return err;
    }
    err = native_window_set_buffers_format(anw.get(), 0x15/*mStream.format*/);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_set_buffers_format failed.", __FUNCTION__);
        return err;
    }
    err = native_window_set_scaling_mode(anw.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_set_scaling_mode failed.", __FUNCTION__);
        return err;
    }
    int transform = 0;
    err = native_window_set_buffers_transform(anw.get(), transform);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_set_buffers_transform failed.", __FUNCTION__);
        return err;
    }

    int minUndequeuedBufs = 0;
    err = anw.get()->query(anw.get(),
            NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBufs);
    if (err != NO_ERROR) {
        ALOGE("error pushing blank frames: MIN_UNDEQUEUED_BUFFERS query "
                "failed: %s (%d)", strerror(-err), -err);
        return err;
    }
    ALOGW("minUndequeuedBufs = %d", minUndequeuedBufs);
    err = native_window_set_buffer_count(anw.get(), mStream.buffCount + 1);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_set_buffer_count failed.", __FUNCTION__);
        return err;
    }
    return NO_ERROR;
}

int JTvInputHal::BufferProducerThread::initPreviewBuffPoll(const sp<Surface>& surface,
        const AidlRkTvStreamConfig* stream) {
    Mutex::Autolock autoLock(&mLock);
    ALOGW("%s in", __FUNCTION__);
    status_t err = NO_ERROR;

    mSurface = surface;
    mDeviceId = stream->deviceId;
    memcpy(&mStream, stream, sizeof(mStream));

    if(configPreviewBuff() != NO_ERROR) {
        ALOGE("failed configPreviewBuff, please check your config.");
        return -1;
    }

    sp<ANativeWindow> anw(mSurface);
    int buffCount = mStream.buffCount + 1;
    mTvHalPreviewBuff.resize(buffCount);
    mParentHal->mPreviewBuffer.resize(buffCount);
    static_cast<Surface*>(anw.get())->getIGraphicBufferProducer()->allowAllocation(true);
    for (int i=0; i<buffCount; i++) {
        ANativeWindowBuffer_t* buffer = NULL;
        err = native_window_dequeue_buffer_and_wait(anw.get(), &buffer);
        // err = anw->dequeueBuffer(anw.get(), &buffer, &mTvHalPreviewBuff[i].bufferFenceFd);
        if (err != NO_ERROR) {
            ALOGE("error %d while dequeueing buffer to surface", err);
            return -1;
        }
        sp<GraphicBuffer> gbPtr(GraphicBuffer::from(buffer));
        ALOGW("%s handle = %p, buffId = %" PRIu64, __FUNCTION__, buffer->handle, gbPtr->getId());
        mTvHalPreviewBuff[i].buffId = gbPtr->getId();
        mTvHalPreviewBuff[i].buffStatus = BUFF_STATUS_DEQUEUED;
        mTvHalPreviewBuff[i].mGraphicBuffer = gbPtr;
        mTvHalPreviewBuff[i].anwbPtr = buffer;
        mParentHal->mPreviewBuffer[i].bufferId = gbPtr->getId();
        mParentHal->mPreviewBuffer[i].buffer = buffer->handle;
        if (i == buffCount - 1) {
            ALOGW("%s index=%d, signal handle = %p, buffId = %" PRIu64,
                __FUNCTION__, i, buffer->handle, gbPtr->getId());
        }
        mParentHal->mTvInput->setSinglePreviewBuffer(mParentHal->mPreviewBuffer[i].bufferId,
            mParentHal->mPreviewBuffer[i].buffer);
        buffer = NULL;
        gbPtr = NULL;
    }
    static_cast<Surface*>(anw.get())->getIGraphicBufferProducer()->allowAllocation(false);
    return mTvHalPreviewBuff.size();
}

void JTvInputHal::BufferProducerThread::setSurface(const sp<Surface>& surface) {
    Mutex::Autolock autoLock(&mLock);
    ALOGW("%s in", __FUNCTION__);
    mCondition.broadcast();
}

void JTvInputHal::BufferProducerThread::surfaceBuffDestory() {
    status_t err = NO_ERROR;
    sp<ANativeWindow> anw(mSurface);
    if (!mTvHalPreviewBuff.empty()) {
        for (int i=0; i< mTvHalPreviewBuff.size(); i++) {
            ALOGW("buff[%d], bufferId[%" PRIu64"] buffStatus = %d", i, mTvHalPreviewBuff[i].buffId, mTvHalPreviewBuff[i].buffStatus);
            if (mTvHalPreviewBuff[i].buffStatus == BUFF_STATUS_DEQUEUED) {
                err = anw->cancelBuffer(anw.get(), mTvHalPreviewBuff[i].mGraphicBuffer.get(), -1);
                if (err != NO_ERROR) {
                    ALOGE("%s cancelBuffer failed.", __FUNCTION__);
                }
            }
            mTvHalPreviewBuff[i].mGraphicBuffer.clear();
            mTvHalPreviewBuff[i].mGraphicBuffer = NULL;
            mTvHalPreviewBuff[i].anwbPtr.clear();
            mTvHalPreviewBuff[i].anwbPtr = NULL;

            mParentHal->mPreviewBuffer[i].buffer = NULL;
        }
        mTvHalPreviewBuff.clear();
        mParentHal->mPreviewBuffer.clear();
    }
    mBuffer.clear();
    mBuffer = NULL;
    err = native_window_api_disconnect(anw.get(), NATIVE_WINDOW_API_CAMERA);
    if (err != NO_ERROR) {
        ALOGE("%s native_window_api_disconnect failed.", __FUNCTION__);
    } else {
        ALOGW("%s native_window_api_disconnect succeeful.", __FUNCTION__);
    }
    mSurface.clear();
    mSurface = NULL;
}

void JTvInputHal::BufferProducerThread::onCaptured(uint64_t buffId, uint32_t buffSeq, bool succeeded) {
    status_t err = NO_ERROR;
    if (mShutdown) {
        return;
    }
    sp<ANativeWindow> anw(mSurface);
    if (buffSeq != mSeq) {
        //ALOGW("Incorrect sequence value: expected %u actual %u", mSeq, buffSeq);
        mSeq = buffSeq;
    }
    // mCondition.broadcast();
    if (succeeded && anw != NULL) {
        //ALOGV("%s buffSeq=%d, buffId = %" PRIu64, __FUNCTION__, buffSeq, buffId);
        if (0 == buffId && !mAlreadyShowSignal) {
            mAlreadyShowSignal = true;
            ALOGW("%s show signal", __FUNCTION__);
            err = anw->queueBuffer(anw.get(), mTvHalPreviewBuff[mTvHalPreviewBuff.size()-1].mGraphicBuffer.get(), -1);
            return;
        }
        if (mAlreadyShowSignal) {
            ALOGW("%s mAlreadyShowSignal", __FUNCTION__);
            return;
        }
        for (int i=0; i<mTvHalPreviewBuff.size(); i++) {
            if (buffId == mTvHalPreviewBuff[i].buffId) {
                err = anw->queueBuffer(anw.get(), mTvHalPreviewBuff[i].mGraphicBuffer.get(), -1);
                if (err != NO_ERROR) {
                    ALOGE("error %d while queueing buffer to surface", err);
                    return;
                } else {
                    //ALOGV("queueBuffer succeed mFirstCaptured=%d buff i=%d id = %" PRIu64,
                    //    mFirstCaptured, i, mTvHalPreviewBuff[i].buffId);
                    mTvHalPreviewBuff[i].buffStatus = BUFF_STATUS_QUEUED;
                    mCurrBuffId = buffId;
                    if (!mFirstCaptured) {
                        mBufferState = RELEASED;
                        mCondition.broadcast();
                    } else {
                        mFirstCaptured = false;
                        mBufferState = CAPTURING;
                    }
                    break;
                }
            }
        }
    }
}

void JTvInputHal::BufferProducerThread::shutdown() {
    Mutex::Autolock autoLock(&mLock);
    mShutdown = true;
    surfaceBuffDestory();
}

bool JTvInputHal::BufferProducerThread::threadLoop() {
    Mutex::Autolock autoLock(&mLock);
    if (mShutdown) {
        return false;
    }
    status_t err = NO_ERROR;
    if (mSurface == NULL) {
        err = mCondition.waitRelative(mLock, s2ns(1));
        // It's OK to time out here.
        if (err != NO_ERROR && err != TIMED_OUT) {
            ALOGE("error %d while wating for non-null surface to be set", err);
            return false;
        }
        return true;
    }
    sp<ANativeWindow> anw(mSurface);
    if ((mBuffer == NULL || mBufferState == RELEASED) && !mShutdown && anw != NULL) {
        ANativeWindowBuffer_t* buffer = NULL;
        if (mCurrBuffId != 0) {
            int index = -1;
            uint64_t dequeueBuffId = 0;
            //err = native_window_dequeue_buffer_and_wait(anw.get(), &buffer);
            int fenceFd = -1;
            err = anw->dequeueBuffer(anw.get(), &buffer, &fenceFd);
            if (err != NO_ERROR) {
                ALOGE("error %d while dequeueing buffer to surface", err);
                return false;
            } else {
                dequeueBuffId = GraphicBuffer::from(buffer)->getId();
                //ALOGV("%s native_window_dequeue_buffer_and_wait succeeded buffId=%" PRIu64, __FUNCTION__, dequeueBuffId);
            }
            for (int i=0; i<mTvHalPreviewBuff.size()-1; i++) {
                //ALOGV("i=%d, buffId=%" PRIu64 ", buffhandle=%p", i, mTvHalPreviewBuff[i].buffId, mTvHalPreviewBuff[i].mGraphicBuffer->getNativeBuffer());
                if (dequeueBuffId == mTvHalPreviewBuff[i].buffId) {
                    index = i;
                    mBuffer = mTvHalPreviewBuff.at(i).anwbPtr;
                    mCurrBuffId = mTvHalPreviewBuff.at(i).buffId;
                    mTvHalPreviewBuff[i].buffStatus = BUFF_STATUS_DEQUEUED;
                    //ALOGV("find the %d right buff %" PRIu64,i, mCurrBuffId);
                    break;
                }
            }
            sp<Fence> hwcFence(new Fence(fenceFd));
            //ALOGE("wait fence start %" PRIu64 , dequeueBuffId);
            //err = hwcFence->waitForever("dequeueBuffer_tv");
            err = hwcFence->wait(100);
            //ALOGE("wait fence end %" PRIu64 , dequeueBuffId);
            if (err != OK) {
                ALOGE("Fence wait err fenceFd %d err=%d, dequeueBuffId %" PRIx64, fenceFd, err, dequeueBuffId);
                //anw->cancelBuffer(anw.get(), buffer, fenceFd);
                //return false;
            }
            if (index == -1) {
                ALOGE("ERROR hapened, after dequeueing, there is no right buff id");
                return false;
            }
        } else {
            mBuffer = mTvHalPreviewBuff.front().anwbPtr;
            mCurrBuffId = mTvHalPreviewBuff.front().buffId;
        }
        if (mBuffer == NULL) {
            ALOGE("no buff dequeue");
            return false;
        }
        //ALOGV("before request: capture hanele=%p mCurrBuffId=%" PRIu64, mBuffer.get()->handle, mCurrBuffId);
        mBufferState = CAPTURING;
        mParentHal->mTvInput->requestRkCapture(mDeviceId, mStream.base.streamId, mCurrBuffId, /*mBuffer.get()->handle,*/ ++mSeq);
        buffer = NULL;
    }

    return true;
}

//----------------------------------------

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::setCallback(
        const std::shared_ptr<TvInputCallback>& in_callback) {
    if (mIsHidl) {
        return hidlSetCallback(in_callback);
    } else {
        return mAidlTvInput->setCallback(in_callback);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::getStreamConfigurations(
        int32_t in_deviceId, std::vector<AidlTvStreamConfig>* _aidl_return) {
    if (mIsHidl) {
        return hidlGetStreamConfigurations(in_deviceId, _aidl_return);
    } else {
        return mAidlTvInput->getStreamConfigurations(in_deviceId, _aidl_return);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::openStream(int32_t in_deviceId,
                                                              int32_t in_streamId,
                                                              AidlNativeHandle* _aidl_return) {
    if (mIsHidl) {
        return hidlOpenStream(in_deviceId, in_streamId, _aidl_return);
    } else {
        return mAidlTvInput->openStream(in_deviceId, in_streamId, _aidl_return);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::closeStream(int32_t in_deviceId,
                                                               int32_t in_streamId) {
    if (mIsHidl) {
        return hidlCloseStream(in_deviceId, in_streamId);
    } else {
        return mAidlTvInput->closeStream(in_deviceId, in_streamId);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::setTvMessageEnabled(int32_t deviceId,
                                                                       int32_t streamId,
                                                                       TvMessageEventType in_type,
                                                                       bool enabled) {
    if (mIsHidl) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlTvInput->setTvMessageEnabled(deviceId, streamId, in_type, enabled);
    }
}

::ndk::ScopedAStatus JTvInputHal::ITvInputWrapper::getTvMessageQueueDesc(
        MQDescriptor<int8_t, SynchronizedReadWrite>* out_queue, int32_t in_deviceId,
        int32_t in_streamId) {
    if (mIsHidl) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        return mAidlTvInput->getTvMessageQueueDesc(out_queue, in_deviceId, in_streamId);
    }
}

} // namespace android
