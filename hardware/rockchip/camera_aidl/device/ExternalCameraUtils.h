/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef HARDWARE_INTERFACES_CAMERA_DEVICE_DEFAULT_EXTERNALCAMERAUTILS_H_
#define HARDWARE_INTERFACES_CAMERA_DEVICE_DEFAULT_EXTERNALCAMERAUTILS_H_

#include <CameraMetadata.h>
#include <HandleImporter.h>
#include <aidl/android/hardware/camera/common/Status.h>
#include <aidl/android/hardware/camera/device/CaptureResult.h>
#include <aidl/android/hardware/camera/device/ErrorCode.h>
#include <aidl/android/hardware/camera/device/NotifyMsg.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>

#ifdef RK_DEVICE
#include "rk_type.h"
#include "vpu_api.h"
#endif

using ::aidl::android::hardware::camera::common::Status;
using ::aidl::android::hardware::camera::device::CaptureResult;
using ::aidl::android::hardware::camera::device::ErrorCode;
using ::aidl::android::hardware::camera::device::NotifyMsg;
using ::aidl::android::hardware::graphics::common::BufferUsage;
using ::aidl::android::hardware::graphics::common::PixelFormat;
using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;
using ::android::hardware::camera::common::V1_0::helper::HandleImporter;

namespace android {
namespace hardware {
namespace camera {

namespace external {
namespace common {

struct Size {
    int32_t width;
    int32_t height;

    bool operator==(const Size& other) const {
        return (width == other.width && height == other.height);
    }
};

struct SizeHasher {
    size_t operator()(const Size& sz) const {
        size_t result = 1;
        result = 31 * result + sz.width;
        result = 31 * result + sz.height;
        return result;
    }
};

struct ExternalCameraConfig {
    static const char* kDefaultCfgPath;
    static ExternalCameraConfig loadFromCfg(const char* cfgPath = kDefaultCfgPath);

    // CameraId base offset for numerical representation
    uint32_t cameraIdOffset;

    // List of internal V4L2 video nodes external camera HAL must ignore.
    std::unordered_set<std::string> mInternalDevices;

    // Maximal size of a JPEG buffer, in bytes
    int32_t maxJpegBufSize;

    // Maximum Size that can sustain 30fps streaming
    Size maxVideoSize;

    // Size of v4l2 buffer queue when streaming <= kMaxVideoSize
    uint32_t numVideoBuffers;

    // Size of v4l2 buffer queue when streaming > kMaxVideoSize
    uint32_t numStillBuffers;

    // Indication that the device connected supports depth output
    bool depthEnabled;

    struct FpsLimitation {
        Size size;
        double fpsUpperBound;
    };
    std::vector<FpsLimitation> fpsLimits;
    std::vector<FpsLimitation> depthFpsLimits;

    // Minimum output stream size
    Size minStreamSize;

    // The value of android.sensor.orientation
    int32_t orientation;

  private:
    ExternalCameraConfig();
    static bool updateFpsList(tinyxml2::XMLElement* fpsList, std::vector<FpsLimitation>& fpsLimits);
};

}  // namespace common
}  // namespace external

namespace device {
namespace implementation {

struct SupportedV4L2Format {
    int32_t width;
    int32_t height;
    uint32_t fourcc;
    // All supported frame rate for this w/h/fourcc combination
    struct FrameRate {
        // Duration (in seconds) of a single frame.
        // Numerator and denominator of the frame duration are stored separately.
        // For ex. a frame lasting 1/30 of a second will be stored as {1, 30}
        uint32_t durationNumerator;         // frame duration numerator.   Ex: 1
        uint32_t durationDenominator;       // frame duration denominator. Ex: 30
        double getFramesPerSecond() const;  // FPS as double.        Ex: 30.0
    };
    double maxFramerate;
    std::vector<FrameRate> frameRates;
};

// A Base class with basic information about a frame
struct Frame : public std::enable_shared_from_this<Frame> {
  public:
    Frame(uint32_t width, uint32_t height, uint32_t fourcc);
    virtual ~Frame();
    const int32_t mWidth;
    const int32_t mHeight;
    const uint32_t mFourcc;

    // getData might involve map/allocation
    virtual int getData(uint8_t** outData, size_t* dataSize) = 0;
};

// A class provide access to a dequeued V4L2 frame buffer (mostly in MJPG format)
// Also contains necessary information to enqueue the buffer back to V4L2 buffer queue
class V4L2Frame : public Frame {
  public:
    V4L2Frame(uint32_t w, uint32_t h, uint32_t fourcc, int bufIdx, int fd, uint32_t dataSize,
              uint64_t offset, uint32_t buffds[]);
    virtual ~V4L2Frame();

    virtual int getData(uint8_t** outData, size_t* dataSize) override;
    int getFd();

    const int mBufferIndex;  // for later enqueue
    int map(uint8_t** data, size_t* dataSize);
    int unmap();

  private:
    std::mutex mLock;
    const int mFd;  // used for mmap but doesn't claim ownership
    const size_t mDataSize;
    const uint64_t mOffset;  // used for mmap
    uint8_t* mData = nullptr;
    bool mMapped = false;
    uint32_t* mBufFd = nullptr;
};

// A RAII class representing a CPU allocated YUV frame used as intermediate buffers
// when generating output images.
class AllocatedFrame : public Frame {
  public:
    AllocatedFrame(uint32_t w, uint32_t h);  // only support V4L2_PIX_FMT_YUV420 for now
    ~AllocatedFrame() override;

    virtual int getData(uint8_t** outData, size_t* dataSize) override;

    int allocate(YCbCrLayout* out = nullptr);
    int getLayout(YCbCrLayout* out);
    int getCroppedLayout(const IMapper::Rect&, YCbCrLayout* out);  // return non-zero for bad input
  private:
    std::mutex mLock;
    std::vector<uint8_t> mData;
    size_t mBufferSize;  // size of mData before padding. Actual size of mData might be slightly
                         // bigger to horizontally pad the frame for jpeglib.
};

enum CroppingType { HORIZONTAL = 0, VERTICAL = 1 };

// Aspect ratio is defined as width/height here and ExternalCameraDevice
// will guarantee all supported sizes has width >= height (so aspect ratio >= 1.0)
#define ASPECT_RATIO(sz) (static_cast<float>((sz).width) / (sz).height)
const float kMaxAspectRatio = std::numeric_limits<float>::max();
const float kMinAspectRatio = 1.f;

bool isAspectRatioClose(float ar1, float ar2);

struct HalStreamBuffer {
    int32_t streamId;
    int64_t bufferId;
    int32_t width;
    int32_t height;
    ::aidl::android::hardware::graphics::common::PixelFormat format;
    ::aidl::android::hardware::graphics::common::BufferUsage usage;
    buffer_handle_t* bufPtr;
    int acquireFence;
    bool fenceTimeout;
};

struct HalRequest {
    int32_t frameNumber;
    common::V1_0::helper::CameraMetadata setting;
    std::shared_ptr<Frame> frameIn;
    nsecs_t shutterTs;
    std::vector<HalStreamBuffer> buffers;

    //rk
    //sp<YuvFrame> yuvframeIn;
#ifdef RK_DEVICE
    MppFrame MppFrame;
    VPU_FRAME vframe;
    int index;
    unsigned long mShareFd;
    unsigned long mVirAddr;
    uint8_t* inData;
    size_t inDataSize;
    std::string cameraId;
#endif
};

static const uint64_t BUFFER_ID_NO_BUFFER = 0;

// buffers currently circulating between HAL and camera service
// key: bufferId sent via HIDL interface
// value: imported buffer_handle_t
// Buffer will be imported during processCaptureRequest (or requestStreamBuffer
// in the case of HAL buffer manager is enabled) and will be freed
// when the stream is deleted or camera device session is closed
typedef std::unordered_map<uint64_t, buffer_handle_t> CirculatingBuffers;

aidl::android::hardware::camera::common::Status importBufferImpl(
        /*inout*/ std::map<int, CirculatingBuffers>& circulatingBuffers,
        /*inout*/ HandleImporter& handleImporter, int32_t streamId, uint64_t bufId,
        buffer_handle_t buf,
        /*out*/ buffer_handle_t** outBufPtr);

static const uint32_t FLEX_YUV_GENERIC =
        static_cast<uint32_t>('F') | static_cast<uint32_t>('L') << 8 |
        static_cast<uint32_t>('E') << 16 | static_cast<uint32_t>('X') << 24;

// returns FLEX_YUV_GENERIC for formats other than YV12/YU12/NV12/NV21
uint32_t getFourCcFromLayout(const YCbCrLayout&);

using ::android::hardware::camera::external::common::Size;
int getCropRect(CroppingType ct, const Size& inSize, const Size& outSize, IMapper::Rect* out);

int formatConvert(const YCbCrLayout& in, const YCbCrLayout& out, Size sz, uint32_t format);

int encodeJpegYU12(const Size& inSz, const YCbCrLayout& inLayout, int jpegQuality,
                   const void* app1Buffer, size_t app1Size, void* out, size_t maxOutSize,
                   size_t& actualCodeSize);

Size getMaxThumbnailResolution(const common::V1_0::helper::CameraMetadata&);

void freeReleaseFences(std::vector<CaptureResult>&);

status_t fillCaptureResultCommon(common::V1_0::helper::CameraMetadata& md, nsecs_t timestamp,
                                 camera_metadata_ro_entry& activeArraySize);

// Interface for OutputThread calling back to parent
struct OutputThreadInterface {
    virtual ~OutputThreadInterface() {}
    virtual aidl::android::hardware::camera::common::Status importBuffer(
            int32_t streamId, uint64_t bufId, buffer_handle_t buf,
            /*out*/ buffer_handle_t** outBufPtr) = 0;

    virtual void notifyError(int32_t frameNumber, int32_t streamId, ErrorCode ec) = 0;

    // Callbacks are fired within the method if msgs/results are nullptr.
    // Otherwise the callbacks will be returned and caller is responsible to
    // fire the callback later
    virtual aidl::android::hardware::camera::common::Status processCaptureRequestError(
            const std::shared_ptr<HalRequest>&,
            /*out*/ std::vector<NotifyMsg>* msgs,
            /*out*/ std::vector<CaptureResult>* results) = 0;

    virtual aidl::android::hardware::camera::common::Status processCaptureRequestError(
            const std::shared_ptr<HalRequest>& reqs) final {
        return processCaptureRequestError(reqs, nullptr, nullptr);
    }

    virtual aidl::android::hardware::camera::common::Status processCaptureResult(
            std::shared_ptr<HalRequest>&) = 0;

    virtual ssize_t getJpegBufferSize(int32_t width, int32_t height) const = 0;
};

// A CPU copy of a mapped V4L2Frame. Will map the input V4L2 frame.
class AllocatedV4L2Frame : public Frame {
  public:
    AllocatedV4L2Frame(std::shared_ptr<V4L2Frame> frameIn);
    ~AllocatedV4L2Frame() override;
    virtual int getData(uint8_t** outData, size_t* dataSize) override;

  private:
    std::vector<uint8_t> mData;
};

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_INTERFACES_CAMERA_DEVICE_DEFAULT_EXTERNALCAMERAUTILS_H_
