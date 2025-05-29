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

#pragma once

#define TV_INPUT_AIDL_SERVICE_NAME "android.hardware.tv.input.ITvInput/default"

#include <aidl/android/hardware/tv/input/BnTvInputCallback.h>
#include <aidl/android/hardware/tv/input/CableConnectionStatus.h>
#include <aidl/android/hardware/tv/input/ITvInput.h>
#include <aidl/android/media/audio/common/AudioDevice.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <android/binder_manager.h>
#include <fmq/AidlMessageQueue.h>
#include <nativehelper/JNIHelp.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <utils/Log.h>
#include <utils/Looper.h>
#include <utils/NativeHandle.h>

//-----------------------rk code----------
#include <aidl/rockchip/hardware/tv/input/IRkTvInput.h>
#include <gui/Surface.h>
#include <hardware/tv_input.h>
//----------------------------------------

#include <iomanip>

//-----------------------rk code----------
//#include "BufferProducerThread.h"
//----------------------------------------
#include "TvInputHal_hidl.h"
#include "android_os_MessageQueue.h"
#include "android_runtime/AndroidRuntime.h"
#include "android_runtime/android_view_Surface.h"
#include "tvinput/jstruct.h"

using ::android::AidlMessageQueue;

using ::aidl::android::hardware::common::fmq::MQDescriptor;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::aidl::android::hardware::tv::input::BnTvInputCallback;
using ::aidl::android::hardware::tv::input::CableConnectionStatus;
using ::aidl::android::hardware::tv::input::TvInputEventType;
using ::aidl::android::hardware::tv::input::TvInputType;
using ::aidl::android::hardware::tv::input::TvMessageEvent;
using ::aidl::android::hardware::tv::input::TvMessageEventType;

using AidlAudioDevice = ::aidl::android::media::audio::common::AudioDevice;
using AidlAudioDeviceAddress = ::aidl::android::media::audio::common::AudioDeviceAddress;
using AidlAudioDeviceType = ::aidl::android::media::audio::common::AudioDeviceType;
using AidlITvInput = ::aidl::android::hardware::tv::input::ITvInput;
using AidlNativeHandle = ::aidl::android::hardware::common::NativeHandle;
using AidlTvInputDeviceInfo = ::aidl::android::hardware::tv::input::TvInputDeviceInfo;
using AidlTvInputEvent = ::aidl::android::hardware::tv::input::TvInputEvent;
using AidlTvMessage = ::aidl::android::hardware::tv::input::TvMessage;
using AidlTvMessageEvent = ::aidl::android::hardware::tv::input::TvMessageEvent;
using AidlTvMessageEventType = ::aidl::android::hardware::tv::input::TvMessageEventType;
using AidlTvStreamConfig = ::aidl::android::hardware::tv::input::TvStreamConfig;

using AidlMessageQueueMap = std::unordered_map<
        int,
        std::unordered_map<int, std::shared_ptr<AidlMessageQueue<int8_t, SynchronizedReadWrite>>>>;

//-----------------------rk code----------
using AidlIRkTvInput = ::aidl::rockchip::hardware::tv::input::IRkTvInput;
using AidlRkTvStreamConfig = ::aidl::rockchip::hardware::tv::input::RkTvStreamConfig;
using AidlRkTvPrivAppCmdInfo = ::aidl::rockchip::hardware::tv::input::RkTvPrivAppCmdInfo;
using AidlRkTvPrivAppCmdBundle = ::aidl::rockchip::hardware::tv::input::RkTvPrivAppCmdBundle;
//----------------------------------------

extern gBundleClassInfoType gBundleClassInfo;
extern gTvInputHalClassInfoType gTvInputHalClassInfo;
extern gTvStreamConfigClassInfoType gTvStreamConfigClassInfo;
extern gTvStreamConfigBuilderClassInfoType gTvStreamConfigBuilderClassInfo;
extern gTvInputHardwareInfoBuilderClassInfoType gTvInputHardwareInfoBuilderClassInfo;

namespace android {

//-----------------------rk code----------
enum {
    BUFF_STATUS_QUEUED,
    BUFF_STATUS_DEQUEUED,
    BUFF_STATUS_QUEUED_FAILED,
    BUFF_STATUS_DEQUEUED_FAILED,
};

struct PreviewBuffer {
    uint64_t bufferId;
    buffer_handle_t buffer;
};

typedef struct tvhal_preview_buff {
    uint64_t buffId;
    int bufferFenceFd;
    int buffStatus;
    sp<ANativeWindowBuffer_t> anwbPtr;
    sp<GraphicBuffer> mGraphicBuffer;
} tvhal_preview_buff_t;
//----------------------------------------

class JTvInputHal {
public:
    ~JTvInputHal();

    static JTvInputHal* createInstance(JNIEnv* env, jobject thiz, const sp<Looper>& looper);

    int addOrUpdateStream(int deviceId, int streamId, const sp<Surface>& surface);
    int setTvMessageEnabled(int deviceId, int streamId, int type, bool enabled);
    int removeStream(int deviceId, int streamId);
    const std::vector<AidlTvStreamConfig> getStreamConfigs(int deviceId);

    //-----------------------rk code----------
    int privCmdFromApp(AidlRkTvPrivAppCmdInfo info);

    std::vector<PreviewBuffer> mPreviewBuffer;
    //----------------------------------------

private:
    //-----------------------rk code----------
    class BufferProducerThread : public Thread {
    public:
        BufferProducerThread(JTvInputHal* hal, int deviceId/*, const tv_stream_config_t* stream*/);

        int initPreviewBuffPoll(const sp<Surface>& surface, const AidlRkTvStreamConfig* stream);
        void setSurface(const sp<Surface>& surface);
        void onCaptured(uint64_t buffId, uint32_t buffSeq, bool succeeded);
        void shutdown();

    private:
        Mutex mLock;
        Condition mCondition;
        sp<Surface> mSurface;
        JTvInputHal* mParentHal;
        int mDeviceId;
        AidlRkTvStreamConfig mStream;
        std::vector<tvhal_preview_buff_t> mTvHalPreviewBuff;
        sp<ANativeWindowBuffer_t> mBuffer;
        enum {
            CAPTURING,
            CAPTURED,
            RELEASED,
        } mBufferState;
        uint32_t mSeq;
        uint64_t mCurrBuffId;
        bool mFirstCaptured;
        bool mShutdown;
        bool mAlreadyShowSignal;

        virtual status_t readyToRun();
        virtual bool threadLoop();
        status_t configPreviewBuff();
        void surfaceBuffDestory();
    };
    //----------------------------------------

    // Connection between a surface and a stream.
    class Connection {
    public:
        Connection() {}

        sp<Surface> mSurface;
        tv_stream_type_t mStreamType;

        // Only valid when mStreamType == TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE
        sp<NativeHandle> mSourceHandle;
        //-----------------------rk code----------
        sp<NativeHandle> mCancelSourceHandle = NULL;
        //----------------------------------------
        // Only valid when mStreamType == TV_STREAM_TYPE_BUFFER_PRODUCER
        sp<BufferProducerThread> mThread;
    };

    class TvInputDeviceInfoWrapper {
    public:
        TvInputDeviceInfoWrapper() {}

        static TvInputDeviceInfoWrapper createDeviceInfoWrapper(
                const AidlTvInputDeviceInfo& aidlTvInputDeviceInfo);
        static TvInputDeviceInfoWrapper createDeviceInfoWrapper(
                const HidlTvInputDeviceInfo& hidlTvInputDeviceInfo);

        bool isHidl;
        int deviceId;
        TvInputType type;
        int portId;
        CableConnectionStatus cableConnectionStatus;
        AidlAudioDevice aidlAudioDevice;
        HidlAudioDevice hidlAudioType;
        ::android::hardware::hidl_array<uint8_t, 32> hidlAudioAddress;
    };

    class TvInputEventWrapper {
    public:
        TvInputEventWrapper() {}

        static TvInputEventWrapper createEventWrapper(const AidlTvInputEvent& aidlTvInputEvent);
        static TvInputEventWrapper createEventWrapper(const HidlTvInputEvent& hidlTvInputEvent);

        TvInputEventType type;
        TvInputDeviceInfoWrapper deviceInfo;
    };

    class TvMessageEventWrapper {
    public:
        TvMessageEventWrapper() {}

        static TvMessageEventWrapper createEventWrapper(
                const AidlTvMessageEvent& aidlTvMessageEvent);

        int streamId;
        int deviceId;
        std::vector<AidlTvMessage> messages;
        AidlTvMessageEventType type;
    };

    class NotifyHandler : public MessageHandler {
    public:
        NotifyHandler(JTvInputHal* hal, const TvInputEventWrapper& event);

        void handleMessage(const Message& message) override;

    private:
        TvInputEventWrapper mEvent;
        JTvInputHal* mHal;
    };

    class NotifyTvMessageHandler : public MessageHandler {
    public:
        NotifyTvMessageHandler(JTvInputHal* hal, const TvMessageEventWrapper& event);

        void handleMessage(const Message& message) override;

    private:
        TvMessageEventWrapper mEvent;
        JTvInputHal* mHal;
    };

    class TvInputCallback : public HidlITvInputCallback, public BnTvInputCallback {
    public:
        explicit TvInputCallback(JTvInputHal* hal);
        ::ndk::ScopedAStatus notify(const AidlTvInputEvent& event) override;
        ::ndk::ScopedAStatus notifyTvMessageEvent(const AidlTvMessageEvent& event) override;
        Return<void> notify(const HidlTvInputEvent& event) override;

    private:
        JTvInputHal* mHal;
    };

    class ITvInputWrapper {
    public:
        //-----------------------rk code----------
        ITvInputWrapper(std::shared_ptr<AidlITvInput>& aidlTvInput, std::shared_ptr<AidlIRkTvInput>& aidlRkTvInput);
        //----------------------------------------
        ITvInputWrapper(sp<HidlITvInput>& hidlTvInput);

        ::ndk::ScopedAStatus setCallback(const std::shared_ptr<TvInputCallback>& in_callback);
        ::ndk::ScopedAStatus getStreamConfigurations(int32_t in_deviceId,
                                                     std::vector<AidlTvStreamConfig>* _aidl_return);
        ::ndk::ScopedAStatus openStream(int32_t in_deviceId, int32_t in_streamId,
                                        AidlNativeHandle* _aidl_return);
        ::ndk::ScopedAStatus closeStream(int32_t in_deviceId, int32_t in_streamId);
        ::ndk::ScopedAStatus setTvMessageEnabled(int32_t deviceId, int32_t streamId,
                                                 TvMessageEventType in_type, bool enabled);
        ::ndk::ScopedAStatus getTvMessageQueueDesc(
                MQDescriptor<int8_t, SynchronizedReadWrite>* out_queue, int32_t in_deviceId,
                int32_t in_streamId);

        //-----------------------rk code----------
        ::ndk::ScopedAStatus privRkCmdFromApp(AidlRkTvPrivAppCmdInfo in_info);
        ::ndk::ScopedAStatus setRkCallback(const std::shared_ptr<TvInputCallback>& in_callback);
        ::ndk::ScopedAStatus getRkStreamConfigurations(int32_t in_deviceId,
                std::vector<AidlRkTvStreamConfig>* _aidl_return);
        ::ndk::ScopedAStatus openRkStream(int32_t in_deviceId, int32_t in_streamId,
                /*AidlNativeHandle* _aidl_return*/std::vector<AidlNativeHandle>* _aidl_return);
        ::ndk::ScopedAStatus closeRkStream(int32_t in_deviceId, int32_t in_streamId);
        ::ndk::ScopedAStatus setRkTvMessageEnabled(int32_t deviceId, int32_t streamId,
                TvMessageEventType in_type, bool enabled);
        ::ndk::ScopedAStatus getRkTvMessageQueueDesc(
                MQDescriptor<int8_t, SynchronizedReadWrite>* out_queue, int32_t in_deviceId,
                int32_t in_streamId);
        ::ndk::ScopedAStatus setRkPreviewInfo(int32_t       deviceId, int32_t streamId, int32_t initType);
        ::ndk::ScopedAStatus setSinglePreviewBuffer(uint64_t bufId, const native_handle* bufHandle);
        ::ndk::ScopedAStatus requestRkCapture(
                int32_t deviceId, int32_t streamId, uint64_t bufId, uint32_t seq);
        //----------------------------------------

    private:
        ::ndk::ScopedAStatus hidlSetCallback(const std::shared_ptr<TvInputCallback>& in_callback);
        ::ndk::ScopedAStatus hidlGetStreamConfigurations(
                int32_t in_deviceId, std::vector<AidlTvStreamConfig>* _aidl_return);
        ::ndk::ScopedAStatus hidlOpenStream(int32_t in_deviceId, int32_t in_streamId,
                                            AidlNativeHandle* _aidl_return);
        ::ndk::ScopedAStatus hidlCloseStream(int32_t in_deviceId, int32_t in_streamId);

        bool mIsHidl;
        sp<HidlITvInput> mHidlTvInput;
        std::shared_ptr<AidlITvInput> mAidlTvInput;
        //-----------------------rk code----------
        std::shared_ptr<AidlIRkTvInput> mAidlRkTvInput;
        //----------------------------------------
    };

    JTvInputHal(JNIEnv* env, jobject thiz, std::shared_ptr<ITvInputWrapper> tvInput,
                const sp<Looper>& looper);

    void hidlSetUpAudioInfo(JNIEnv* env, jobject& builder, const TvInputDeviceInfoWrapper& info);
    void onDeviceAvailable(const TvInputDeviceInfoWrapper& info);
    void onDeviceUnavailable(int deviceId);
    void onStreamConfigurationsChanged(int deviceId, int cableConnectionStatus);
    //-----------------------rk code----------
    //void onCaptured(int deviceId, int streamId, uint32_t seq, bool succeeded);
    void onCaptured(int deviceId, int streamId, uint64_t buffId, uint32_t buffSeq, bool succeeded);
    //----------------------------------------
    void onTvMessage(int deviceId, int streamId, AidlTvMessageEventType type,
                     AidlTvMessage& message, signed char data[], int dataLength);

    Mutex mLock;
    Mutex mStreamLock;
    jweak mThiz;
    sp<Looper> mLooper;
    AidlMessageQueueMap mQueueMap;

    KeyedVector<int, KeyedVector<int, Connection> > mConnections;

    std::shared_ptr<ITvInputWrapper> mTvInput;
    std::shared_ptr<TvInputCallback> mTvInputCallback;
};

} // namespace android
