/*
 * Copyright 2021, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HalImpl.h"

#include <aidl/android/hardware/graphics/composer3/IComposerCallback.h>
#include <android-base/logging.h>

#include "TranslateHwcAidl.h"
#include "Util.h"

//#include <drmhwctwo.h>

using namespace android;

namespace aidl::android::hardware::graphics::composer3::impl {

// open hwcomposer2 device, install an adapter if necessary
static hwc2_device_t* openDeviceWithAdapter(const hw_module_t* module, bool* outAdapted) {
    hw_device_t* device;
    int error = module->methods->open(module, HWC_HARDWARE_COMPOSER, &device);
    if (error) {
        ALOGE("failed to open hwcomposer device: %s", strerror(-error));
        return nullptr;
    }

    int major = (device->version >> 24) & 0xf;
    if (major != 2) {
        *outAdapted = true;
        ALOGE("hwcomposer 1.0 is not supported");
        return nullptr;
    }

    *outAdapted = false;
    return reinterpret_cast<hwc2_device_t*>(device);
}

// load hwcomposer2 module
static const hw_module_t* loadModule() {
    const hw_module_t* module;
    int error = hw_get_module(HWC_HARDWARE_MODULE_ID, &module);
    if (error) {
        ALOGE("failed to get hwcomposer module");
        return nullptr;
    }

    return module;
}

std::unique_ptr<IComposerHal> IComposerHal::create() {
    const hw_module_t* module = loadModule();
    if (!module) {
        return nullptr;
    }

    bool adapted;
    hwc2_device_t* device = openDeviceWithAdapter(module, &adapted);
    if (!device) {
        return nullptr;
    }
    auto hal = std::make_unique<HalImpl>();
    return hal->initWithDevice(std::move(device), !adapted) ? std::move(hal) : nullptr;
}

namespace hook {

void hotplug(hwc2_callback_data_t callbackData, hwc2_display_t hwcDisplay,
                        int32_t connected) {
    auto hal = static_cast<HalImpl*>(callbackData);
    int64_t display;

    h2a::translate(hwcDisplay, display);
    hal->getEventCallback()->onHotplug(display, connected == HWC2_CONNECTION_CONNECTED);
}

void refresh(hwc2_callback_data_t callbackData, hwc2_display_t hwcDisplay) {
    auto hal = static_cast<HalImpl*>(callbackData);
    int64_t display;

    h2a::translate(hwcDisplay, display);
    hal->getEventCallback()->onRefresh(display);
}

void vsync(hwc2_callback_data_t callbackData, hwc2_display_t hwcDisplay,
                           int64_t timestamp, hwc2_vsync_period_t hwcVsyncPeriodNanos) {
    auto hal = static_cast<HalImpl*>(callbackData);
    int64_t display;
    int32_t vsyncPeriodNanos;

    h2a::translate(hwcDisplay, display);
    h2a::translate(hwcVsyncPeriodNanos, vsyncPeriodNanos);
    hal->getEventCallback()->onVsync(display, timestamp, vsyncPeriodNanos);
}

void vsyncPeriodTimingChanged(hwc2_callback_data_t callbackData,
                                         hwc2_display_t hwcDisplay,
                                         hwc_vsync_period_change_timeline_t* hwcTimeline) {
    auto hal = static_cast<HalImpl*>(callbackData);
    int64_t display;
    VsyncPeriodChangeTimeline timeline;

    h2a::translate(hwcDisplay, display);
    h2a::translate(*hwcTimeline, timeline);
    hal->getEventCallback()->onVsyncPeriodTimingChanged(display, timeline);
}

void vsyncIdle(hwc2_callback_data_t callbackData, hwc2_display_t hwcDisplay) {
    auto hal = static_cast<HalImpl*>(callbackData);
    int64_t display;

    h2a::translate(hwcDisplay, display);
    hal->getEventCallback()->onVsyncIdle(display);
}

void seamlessPossible(hwc2_callback_data_t callbackData, hwc2_display_t hwcDisplay) {
    auto hal = static_cast<HalImpl*>(callbackData);
    int64_t display;

    h2a::translate(hwcDisplay, display);
    hal->getEventCallback()->onSeamlessPossible(display);
}

} // nampesapce hook

bool HalImpl::initWithDevice(hwc2_device_t* device, bool requireReliablePresentFence) {
    // we own the device from this point on
    mDevice = device;

    initCaps();
    if (requireReliablePresentFence &&
        hasCapability(Capability::PRESENT_FENCE_IS_NOT_RELIABLE)) {
        ALOGE("present fence must be reliable");
        mDevice->common.close(&mDevice->common);
        mDevice = nullptr;
        return false;
    }

    if (!initDispatch()) {
        mDevice->common.close(&mDevice->common);
        mDevice = nullptr;
        return false;
    }

    return true;
}

void HalImpl::initCaps() {
    uint32_t count = 0;
    mDevice->getCapabilities(mDevice, &count, nullptr);

    std::vector<int32_t> halCaps(count);
    mDevice->getCapabilities(mDevice, &count, halCaps.data());

    for (auto hwcCap : halCaps) {
        Capability cap;
        h2a::translate(hwcCap, cap);
        mCaps.insert(cap);
    }

    //mCaps.insert(Capability::BOOT_DISPLAY_CONFIG);
}

bool HalImpl::hasCapability(Capability cap) {
    return mCaps.find(cap) != mCaps.end();
}

void HalImpl::getCapabilities(std::vector<Capability>* caps) {
    caps->clear();
    caps->insert(caps->begin(), mCaps.begin(), mCaps.end());
}

void HalImpl::dumpDebugInfo(std::string* output) {
    if (output == nullptr) return;

    uint32_t len = 0;
    mDispatch.dump(mDevice, &len, nullptr);

    std::vector<char> buf(len + 1);
    mDispatch.dump(mDevice, &len, buf.data());
    buf.resize(len + 1);
    buf[len] = '\0';

    *output = std::string(buf.begin(), buf.end());
}

void HalImpl::registerEventCallback(EventCallback* callback) {
    mEventCallback = callback;

    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_HOTPLUG, this,
                              reinterpret_cast<hwc2_function_pointer_t>(hook::hotplug));
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_REFRESH, this,
                              reinterpret_cast<hwc2_function_pointer_t>(hook::refresh));
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_VSYNC_2_4, this,
                              reinterpret_cast<hwc2_function_pointer_t>(hook::vsync));

#if 0
    mDevice->registerCallback(HWC2_CALLBACK_VSYNC_PERIOD_TIMING_CHANGED, this,
                     reinterpret_cast<hwc2_function_pointer_t>(hook::vsyncPeriodTimingChanged));
    mDevice->registerCallback(HWC2_CALLBACK_SEAMLESS_POSSIBLE, this,
                     reinterpret_cast<hwc2_function_pointer_t>(hook::seamlessPossible));

    // register HWC3 Callback
    mDevice->registerHwc3Callback(IComposerCallback::TRANSACTION_onVsyncIdle, this,
                                  reinterpret_cast<hwc2_function_pointer_t>(hook::vsyncIdle));
#endif
}

void HalImpl::unregisterEventCallback() {
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_HOTPLUG, this, nullptr);
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_REFRESH, this, nullptr);
    mDispatch.registerCallback(mDevice, HWC2_CALLBACK_VSYNC_2_4, this, nullptr);

#if 0
    mDevice->registerCallback(HWC2_CALLBACK_VSYNC_PERIOD_TIMING_CHANGED, this, nullptr);
    mDevice->registerCallback(HWC2_CALLBACK_SEAMLESS_POSSIBLE, this, nullptr);

    // unregister HWC3 Callback
    mDevice->registerHwc3Callback(IComposerCallback::TRANSACTION_onVsyncIdle, this, nullptr);
#endif

    mEventCallback = nullptr;
}

int32_t HalImpl::acceptDisplayChanges(int64_t display) {
    int32_t err = mDispatch.acceptDisplayChanges(mDevice, display);

    return err;
}

int32_t HalImpl::createLayer(int64_t display, int64_t* outLayer) {
    hwc2_layer_t hwcLayer = 0;
    RET_IF_ERR(mDispatch.createLayer(mDevice, display, &hwcLayer));

    h2a::translate(hwcLayer, *outLayer);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::destroyLayer(int64_t display, int64_t layer) {
    hwc2_layer_t hwcLayer = 0;
    a2h::translate(layer, hwcLayer);
    RET_IF_ERR(mDispatch.destroyLayer(mDevice, display, hwcLayer));

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::createVirtualDisplay(uint32_t width, uint32_t height, AidlPixelFormat format,
                                      VirtualDisplay* outDisplay) {
    int32_t hwcFormat;
    a2h::translate(format, hwcFormat);

    hwc2_display_t hwcDisplay = getDisplayId(HWC_DISPLAY_VIRTUAL, 0);

    RET_IF_ERR(mDispatch.createVirtualDisplay(mDevice, width, height, &hwcFormat, &hwcDisplay));

    h2a::translate(hwcDisplay, outDisplay->display);
    h2a::translate(hwcFormat, outDisplay->format);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::destroyVirtualDisplay(int64_t display) {
    return mDispatch.destroyVirtualDisplay(mDevice, display);
}

int32_t HalImpl::getActiveConfig(int64_t display, int32_t* outConfig) {
    hwc2_config_t hwcConfig;
    RET_IF_ERR(mDispatch.getActiveConfig(mDevice, display, &hwcConfig));

    h2a::translate(hwcConfig, *outConfig);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getColorModes(int64_t display, std::vector<ColorMode>* outModes) {
    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getColorModes(mDevice, display, &count, nullptr));

    std::vector<int32_t> hwcModes(count);
    RET_IF_ERR(mDispatch.getColorModes(mDevice, display, &count, hwcModes.data()));

    h2a::translate(hwcModes, *outModes);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDataspaceSaturationMatrix([[maybe_unused]] common::Dataspace dataspace,
                                              std::vector<float>* matrix) {
    // Pixel HWC does not support dataspace saturation matrix, return unit matrix.
    std::vector<float> unitMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    *matrix = std::move(unitMatrix);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayAttribute(int64_t display, int32_t config,
                                     DisplayAttribute attribute, int32_t* outValue) {
    hwc2_config_t hwcConfig;
    int32_t hwcAttr;
    a2h::translate(config, hwcConfig);
    a2h::translate(attribute, hwcAttr);

    auto err = mDispatch.getDisplayAttribute(mDevice, display, hwcConfig, hwcAttr, outValue);
    if (err != HWC2_ERROR_NONE && *outValue == -1) {
        return HWC2_ERROR_BAD_PARAMETER;
    }
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayBrightnessSupport([[maybe_unused]] int64_t display, bool& outSupport) {

    if (!mDispatch.getDisplayBrightnessSupport) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    return mDispatch.getDisplayBrightnessSupport(mDevice, display, &outSupport);
}

int32_t HalImpl::getDisplayCapabilities([[maybe_unused]] int64_t display,
                                        std::vector<DisplayCapability>* caps) {
    if (!mDispatch.getDisplayCapabilities) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getDisplayCapabilities(mDevice, display, &count, nullptr));

    std::vector<uint32_t> hwcCaps(count);
    RET_IF_ERR(mDispatch.getDisplayCapabilities(mDevice, display, &count, hwcCaps.data()));

    h2a::translate(hwcCaps, *caps);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayConfigs(int64_t display, std::vector<int32_t>* configs) {
    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getDisplayConfigs(mDevice, display, &count, nullptr));

    std::vector<hwc2_config_t> hwcConfigs(count);
    RET_IF_ERR(mDispatch.getDisplayConfigs(mDevice, display, &count, hwcConfigs.data()));

    h2a::translate(hwcConfigs, *configs);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayConnectionType(int64_t display, DisplayConnectionType* outType) {
    if (!mDispatch.getDisplayConnectionType) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    uint32_t hwcType = HWC2_DISPLAY_CONNECTION_TYPE_INTERNAL;
    RET_IF_ERR(mDispatch.getDisplayConnectionType(mDevice, display, &hwcType));
    h2a::translate(hwcType, *outType);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayIdentificationData(int64_t display,
                                              DisplayIdentification *id) {
    if (!mDispatch.getDisplayIdentificationData) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    uint8_t port;
    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getDisplayIdentificationData(mDevice, display, &port, &count, nullptr));

    id->data.resize(count);
    RET_IF_ERR(mDispatch.getDisplayIdentificationData(mDevice, display, &port, &count, id->data.data()));

    h2a::translate(port, id->port);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayName(int64_t display, std::string* outName) {
    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getDisplayName(mDevice, display, &count, nullptr));

    outName->resize(count);
    RET_IF_ERR(mDispatch.getDisplayName(mDevice, display, &count, outName->data()));

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayVsyncPeriod(int64_t display, int32_t* outVsyncPeriod) {
    if (!mDispatch.getDisplayVsyncPeriod) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    hwc2_vsync_period_t hwcVsyncPeriod;
    RET_IF_ERR(mDispatch.getDisplayVsyncPeriod(mDevice, display, &hwcVsyncPeriod));

    h2a::translate(hwcVsyncPeriod, *outVsyncPeriod);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayedContentSample([[maybe_unused]] int64_t display,
                                           [[maybe_unused]] int64_t maxFrames,
                                           [[maybe_unused]] int64_t timestamp,
                                           [[maybe_unused]] DisplayContentSample* samples) {
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getDisplayedContentSamplingAttributes(
        [[maybe_unused]] int64_t display,
        [[maybe_unused]] DisplayContentSamplingAttributes* attrs) {
    return HWC2_ERROR_UNSUPPORTED;
}


int32_t HalImpl::getDisplayPhysicalOrientation([[maybe_unused]] int64_t display,
                                               [[maybe_unused]] common::Transform* orientation) {

    uint32_t hwcType = HWC2_DISPLAY_CONNECTION_TYPE_INTERNAL;
    /* just use this function to judge whether display is valid */
    if (mDispatch.getDisplayConnectionType(mDevice, display, &hwcType) != HWC2_ERROR_NONE)
        return HWC2_ERROR_BAD_DISPLAY;

    *orientation = common::Transform::NONE;

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDozeSupport(int64_t display, bool& support) {
    int32_t hwcSupport;
    RET_IF_ERR(mDispatch.getDozeSupport(mDevice, display, &hwcSupport));

    h2a::translate(hwcSupport, support);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getHdrCapabilities(int64_t display, HdrCapabilities* caps) {
    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getHdrCapabilities(mDevice, display, &count, nullptr,
                                              &caps->maxLuminance,
                                              &caps->maxAverageLuminance,
                                              &caps->minLuminance));

    std::vector<int32_t> hwcHdrTypes(count);
    RET_IF_ERR(mDispatch.getHdrCapabilities(mDevice, display, &count,
                                              hwcHdrTypes.data(),
                                              &caps->maxLuminance,
                                              &caps->maxAverageLuminance,
                                              &caps->minLuminance));

    h2a::translate(hwcHdrTypes, caps->types);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getOverlaySupport([[maybe_unused]] OverlayProperties* caps) {
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getMaxVirtualDisplayCount(int32_t* count) {
    uint32_t hwcCount = mDispatch.getMaxVirtualDisplayCount(mDevice);
    h2a::translate(hwcCount, *count);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getPerFrameMetadataKeys([[maybe_unused]] int64_t display,
                                         [[maybe_unused]] std::vector<PerFrameMetadataKey>* keys) {
/* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getReadbackBufferAttributes([[maybe_unused]] int64_t display,
                                             [[maybe_unused]] ReadbackBufferAttributes* attrs) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getReadbackBufferFence([[maybe_unused]] int64_t display,
                                        [[maybe_unused]] ndk::ScopedFileDescriptor* acquireFence) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getRenderIntents([[maybe_unused]] int64_t display, ColorMode mode,
                                  std::vector<RenderIntent>* intents) {
    if (!mDispatch.getRenderIntents) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    int32_t hwcMode;
    uint32_t count = 0;
    a2h::translate(mode, hwcMode);
    RET_IF_ERR(mDispatch.getRenderIntents(mDevice, display, hwcMode, &count, nullptr));

    std::vector<int32_t> hwcIntents(count);
    RET_IF_ERR(mDispatch.getRenderIntents(mDevice, display, hwcMode, &count, hwcIntents.data()));

    h2a::translate(hwcIntents, *intents);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getSupportedContentTypes([[maybe_unused]] int64_t display, std::vector<ContentType>* types) {
    if (!mDispatch.getSupportedContentTypes) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getSupportedContentTypes(mDevice, display, &count, nullptr));

    std::vector<uint32_t> hwcTypes(count);
    RET_IF_ERR(mDispatch.getSupportedContentTypes(mDevice, display, &count, hwcTypes.data()));

    h2a::translate(hwcTypes, *types);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::presentDisplay(int64_t display, ndk::ScopedFileDescriptor& fence,
                       std::vector<int64_t>* outLayers,
                       std::vector<ndk::ScopedFileDescriptor>* outReleaseFences) {
    int32_t hwcOutPresentFence = -1;
    RET_IF_ERR(mDispatch.presentDisplay(mDevice, display, &hwcOutPresentFence));
    h2a::translate(hwcOutPresentFence, fence);

    uint32_t count = 0;
    RET_IF_ERR(mDispatch.getReleaseFences(mDevice, display, &count, nullptr, nullptr));

    std::vector<hwc2_layer_t> hwcLayers(count);
    std::vector<int32_t> hwcReleaseFences(count, -1);
    RET_IF_ERR(mDispatch.getReleaseFences(mDevice, display, &count, hwcLayers.data(), hwcReleaseFences.data()));

    h2a::translate(hwcLayers, *outLayers);
    h2a::translate(hwcReleaseFences, *outReleaseFences);

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::setActiveConfig(int64_t display, int32_t config) {
    hwc2_config_t hwcConfig;
    a2h::translate(config, hwcConfig);
    return mDispatch.setActiveConfig(mDevice, display, hwcConfig);
}

int32_t HalImpl::setActiveConfigWithConstraints(
            int64_t display, int32_t config,
            const VsyncPeriodChangeConstraints& vsyncPeriodChangeConstraints,
            VsyncPeriodChangeTimeline* timeline) {
    hwc2_config_t hwcConfig;
    hwc_vsync_period_change_constraints_t hwcVsyncPeriodChangeConstraints;
    hwc_vsync_period_change_timeline_t hwcOutTimeline;

    a2h::translate(config, hwcConfig);
    a2h::translate(vsyncPeriodChangeConstraints, hwcVsyncPeriodChangeConstraints);

    RET_IF_ERR(mDispatch.setActiveConfigWithConstraints(mDevice, display, hwcConfig, &hwcVsyncPeriodChangeConstraints, &hwcOutTimeline));

    h2a::translate(hwcOutTimeline, *timeline);
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::setBootDisplayConfig([[maybe_unused]] int64_t display, [[maybe_unused]] int32_t config) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::clearBootDisplayConfig([[maybe_unused]] int64_t display) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getPreferredBootDisplayConfig([[maybe_unused]] int64_t display, [[maybe_unused]] int32_t* config) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::getHdrConversionCapabilities(std::vector<common::HdrConversionCapability>*) {
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setHdrConversionStrategy(const common::HdrConversionStrategy&, common::Hdr*) {
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setAutoLowLatencyMode([[maybe_unused]] int64_t display, [[maybe_unused]] bool on) {
    /* Drmhwc2 not support this feature */
    if (!mDispatch.getRenderIntents) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    return mDispatch.setAutoLowLatencyMode(mDevice, display, on);;
}

int32_t HalImpl::setClientTarget(int64_t display, buffer_handle_t target,
                                 const ndk::ScopedFileDescriptor& fence,
                                 common::Dataspace dataspace,
                                 const std::vector<common::Rect>& damage) {
    int32_t hwcAcquireFence = -1;
    int32_t hwcDataspace;
    std::vector<hwc_rect_t> hwcDamage;

    a2h::translate(fence, hwcAcquireFence);
    a2h::translate(dataspace, hwcDataspace);
    a2h::translate(damage, hwcDamage);
    hwc_region_t region = { hwcDamage.size(), hwcDamage.data() };

    return mDispatch.setClientTarget(mDevice, display, target, hwcAcquireFence, hwcDataspace, region);
}

int32_t HalImpl::setColorMode(int64_t display, ColorMode mode, RenderIntent intent) {
    int32_t hwcMode;
    int32_t hwcIntent;

    a2h::translate(mode, hwcMode);
    a2h::translate(intent, hwcIntent);

    if ((hwcMode < 0) || (hwcIntent < 0))
        return HWC2_ERROR_BAD_PARAMETER;

    return mDispatch.setColorMode(mDevice, display, hwcMode);
}

int32_t HalImpl::setColorTransform(int64_t display, const std::vector<float>& matrix) {
    // clang-format off
    constexpr std::array<float, 16> kIdentity = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    // clang-format on
    const bool isIdentity = (std::equal(matrix.begin(), matrix.end(), kIdentity.begin()));
    const common::ColorTransform hint = isIdentity ? common::ColorTransform::IDENTITY
                                                   : common::ColorTransform::ARBITRARY_MATRIX;

    int32_t hwcHint;
    a2h::translate(hint, hwcHint);
    return mDispatch.setColorTransform(mDevice, display, matrix.data(), hwcHint);
}

int32_t HalImpl::setContentType([[maybe_unused]] int64_t display, ContentType contentType) {
    if (!mDispatch.setContentType) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    int32_t type;
    a2h::translate(contentType, type);
    return mDispatch.setContentType(mDevice, display, type);
}

int32_t HalImpl::setDisplayBrightness([[maybe_unused]] int64_t display, [[maybe_unused]] float brightness) {
    if (!mDispatch.setDisplayBrightness) {
        return HWC2_ERROR_UNSUPPORTED;
    }
    return mDispatch.setDisplayBrightness(mDevice, display, brightness);;
}

int32_t HalImpl::setDisplayedContentSamplingEnabled(
        [[maybe_unused]] int64_t display,
        [[maybe_unused]] bool enable,
        [[maybe_unused]] FormatColorComponent componentMask,
        [[maybe_unused]] int64_t maxFrames) {
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setLayerBlendMode(int64_t display, int64_t layer, common::BlendMode mode) {
    int32_t hwcMode;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(mode, hwcMode);
    a2h::translate(layer, hwcLayer);
    return mDispatch.setLayerBlendMode(mDevice, display, hwcLayer, hwcMode);
}

int32_t HalImpl::setLayerBuffer(int64_t display, int64_t layer, buffer_handle_t buffer,
                                const ndk::ScopedFileDescriptor& acquireFence) {
    int32_t hwcAcquireFence = -1;
    hwc2_layer_t hwcLayer = 0;
    a2h::translate(acquireFence, hwcAcquireFence);
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerBuffer(mDevice, display, hwcLayer, buffer, hwcAcquireFence);
}

int32_t HalImpl::setLayerColor(int64_t display, int64_t layer, Color color) {
    hwc_color_t hwcColor;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(color, hwcColor);
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerColor(mDevice, display, hwcLayer, hwcColor);
}

int32_t HalImpl::setLayerColorTransform([[maybe_unused]] int64_t display, [[maybe_unused]] int64_t layer,
                                        [[maybe_unused]] const std::vector<float>& matrix) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setLayerCompositionType(int64_t display, int64_t layer, Composition type) {
    int32_t hwcType;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(type, hwcType);
    a2h::translate(layer, hwcLayer);

    if (type == Composition::DISPLAY_DECORATION) {
        ALOGW("%s: CompositionType %d can not support", __func__, hwcType);
        return HWC2_ERROR_UNSUPPORTED;
    }

    return mDispatch.setLayerCompositionType(mDevice, display, hwcLayer, hwcType);
}

int32_t HalImpl::setLayerCursorPosition(int64_t display, int64_t layer, int32_t x, int32_t y) {
    hwc2_layer_t hwcLayer = 0;
    a2h::translate(layer, hwcLayer);

    return mDispatch.setCursorPosition(mDevice, display, hwcLayer, x, y);
}

int32_t HalImpl::setLayerDataspace(int64_t display, int64_t layer, common::Dataspace dataspace) {
    int32_t hwcDataspace;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(dataspace, hwcDataspace);
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerDataspace(mDevice, display, hwcLayer, hwcDataspace);
}

int32_t HalImpl::setLayerDisplayFrame(int64_t display, int64_t layer, const common::Rect& frame) {
    hwc_rect_t hwcFrame;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(frame, hwcFrame);
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerDisplayFrame(mDevice, display, hwcLayer, hwcFrame);
}

int32_t HalImpl::setLayerPerFrameMetadata([[maybe_unused]] int64_t display, [[maybe_unused]] int64_t layer,
                           [[maybe_unused]] const std::vector<std::optional<PerFrameMetadata>>& metadata) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setLayerPerFrameMetadataBlobs([[maybe_unused]] int64_t display, [[maybe_unused]] int64_t layer,
                           [[maybe_unused]] const std::vector<std::optional<PerFrameMetadataBlob>>& blobs) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setLayerPlaneAlpha(int64_t display, int64_t layer, float alpha) {
    hwc2_layer_t hwcLayer = 0;
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerPlaneAlpha(mDevice, display, hwcLayer, alpha);
}

int32_t HalImpl::setLayerSidebandStream(int64_t display, int64_t layer,
                                        buffer_handle_t stream) {
    hwc2_layer_t hwcLayer = 0;
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerSidebandStream(mDevice, display, hwcLayer, stream);
}

int32_t HalImpl::setLayerSourceCrop(int64_t display, int64_t layer, const common::FRect& crop) {
    hwc_frect_t hwcCrop;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(crop, hwcCrop);
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerSourceCrop(mDevice, display, hwcLayer, hwcCrop);
}

int32_t HalImpl::setLayerSurfaceDamage(int64_t display, int64_t layer,
                                  const std::vector<std::optional<common::Rect>>& damage) {
    std::vector<hwc_rect_t> hwcDamage;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(damage, hwcDamage);
    hwc_region_t region = { hwcDamage.size(), hwcDamage.data() };

    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerSurfaceDamage(mDevice, display, hwcLayer, region);
}

int32_t HalImpl::setLayerTransform(int64_t display, int64_t layer, common::Transform transform) {
    int32_t hwcTransform;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(transform, hwcTransform);
    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerTransform(mDevice, display, hwcLayer, hwcTransform);
}

int32_t HalImpl::setLayerVisibleRegion(int64_t display, int64_t layer,
                               const std::vector<std::optional<common::Rect>>& visible) {
    std::vector<hwc_rect_t> hwcVisible;
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(visible, hwcVisible);
    hwc_region_t region = { hwcVisible.size(), hwcVisible.data() };

    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerVisibleRegion(mDevice, display, hwcLayer, region);
}

int32_t HalImpl::setLayerBrightness([[maybe_unused]] int64_t display, [[maybe_unused]] int64_t layer, float brightness) {
    if (!std::isfinite(brightness)) {
        ALOGW("%s layer brightness %f is not a valid floating value", __func__, brightness);
        return HWC2_ERROR_BAD_PARAMETER;
    }

    if (brightness > 1.f || brightness < 0.f) {
        ALOGW("%s Brightness is out of [0, 1] range: %f", __func__, brightness);
        return HWC2_ERROR_BAD_PARAMETER;
    }

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::setLayerZOrder(int64_t display, int64_t layer, uint32_t z) {
    hwc2_layer_t hwcLayer = 0;

    a2h::translate(layer, hwcLayer);

    return mDispatch.setLayerZOrder(mDevice, display, hwcLayer, z);
}

int32_t HalImpl::setOutputBuffer(int64_t display, buffer_handle_t buffer,
                                 const ndk::ScopedFileDescriptor& releaseFence) {
    int32_t hwcReleaseFence = -1;
    a2h::translate(releaseFence, hwcReleaseFence);

    auto err = mDispatch.setOutputBuffer(mDevice, display, buffer, hwcReleaseFence);
    // unlike in setClientTarget, releaseFence is owned by us
    if (err == HWC2_ERROR_NONE && hwcReleaseFence >= 0) {
        close(hwcReleaseFence);
    }

    return err;
}

int32_t HalImpl::setPowerMode(int64_t display, PowerMode mode) {
    if (mode == PowerMode::ON_SUSPEND || mode == PowerMode::DOZE_SUSPEND) {
        return HWC2_ERROR_UNSUPPORTED;
    }

    int32_t hwcMode;
    a2h::translate(mode, hwcMode);
    return mDispatch.setPowerMode(mDevice, display, hwcMode);
}

int32_t HalImpl::setReadbackBuffer([[maybe_unused]] int64_t display, [[maybe_unused]] buffer_handle_t buffer,
                                   [[maybe_unused]] const ndk::ScopedFileDescriptor& releaseFence) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setVsyncEnabled(int64_t display, bool enabled) {
    hwc2_vsync_t hwcEnable;
    a2h::translate(enabled, hwcEnable);
    return mDispatch.setVsyncEnabled(mDevice, display, hwcEnable);
}

int32_t HalImpl::setIdleTimerEnabled([[maybe_unused]] int64_t display, [[maybe_unused]] int32_t timeout) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t getClientTargetProperty(
                        hwc_client_target_property_t *outClientTargetProperty,
                        DimmingStage *outDimmingStage) {
    outClientTargetProperty->pixelFormat = HAL_PIXEL_FORMAT_RGBA_8888;
    outClientTargetProperty->dataspace = HAL_DATASPACE_UNKNOWN;
    if (outDimmingStage != nullptr)
        *outDimmingStage = DimmingStage::NONE;

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::validateDisplay(int64_t display, std::vector<int64_t>* outChangedLayers,
                                 std::vector<Composition>* outCompositionTypes,
                                 uint32_t* outDisplayRequestMask,
                                 std::vector<int64_t>* outRequestedLayers,
                                 std::vector<int32_t>* outRequestMasks,
                                 ClientTargetProperty* outClientTargetProperty,
                                 DimmingStage* outDimmingStage) {
    uint32_t typesCount = 0;
    uint32_t reqsCount = 0;
    auto err = mDispatch.validateDisplay(mDevice, display, &typesCount, &reqsCount);

    if (err != HWC2_ERROR_NONE && err != HWC2_ERROR_HAS_CHANGES) {
        return err;
    }

    std::vector<hwc2_layer_t> hwcChangedLayers(typesCount);
    std::vector<int32_t> hwcCompositionTypes(typesCount);
    RET_IF_ERR(mDispatch.getChangedCompositionTypes(mDevice, display,
                                                    &typesCount, hwcChangedLayers.data(),
                                                    hwcCompositionTypes.data()));

    int32_t displayReqs;

    std::vector<hwc2_layer_t> hwcRequestedLayers(reqsCount);
    outRequestMasks->resize(reqsCount);
    RET_IF_ERR(mDispatch.getDisplayRequests(mDevice, display, &displayReqs, &reqsCount,
                                              hwcRequestedLayers.data(), outRequestMasks->data()));

    h2a::translate(hwcChangedLayers, *outChangedLayers);
    h2a::translate(hwcCompositionTypes, *outCompositionTypes);
    *outDisplayRequestMask = displayReqs;
    h2a::translate(hwcRequestedLayers, *outRequestedLayers);

    hwc_client_target_property hwcProperty;
    if (!getClientTargetProperty(&hwcProperty, outDimmingStage))
        h2a::translate(hwcProperty, *outClientTargetProperty);
    // else ignore this error

    return HWC2_ERROR_NONE;
}

int HalImpl::setExpectedPresentTime(
        [[maybe_unused]] int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime) {
    if (!expectedPresentTime.has_value()) return HWC2_ERROR_NONE;

#if 0
    /* Drmhwc2 not support this feature */
    if (halDisplay->getPendingExpectedPresentTime() != 0) {
        ALOGW("HalImpl: set expected present time multiple times in one frame");
    }

    halDisplay->setExpectedPresentTime(expectedPresentTime->timestampNanos);
#endif

    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getRCDLayerSupport([[maybe_unused]] int64_t display, [[maybe_unused]] bool& outSupport) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

int32_t HalImpl::setLayerBlockingRegion(
        [[maybe_unused]] int64_t display, [[maybe_unused]] int64_t layer,
        [[maybe_unused]] const std::vector<std::optional<common::Rect>>& blockingRegion) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::getDisplayIdleTimerSupport([[maybe_unused]] int64_t display, [[maybe_unused]] bool& outSupport) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_NONE;
}

int32_t HalImpl::setRefreshRateChangedCallbackDebugEnabled([[maybe_unused]] int64_t display,
        [[maybe_unused]] bool enabled) {
    /* Drmhwc2 not support this feature */
    return HWC2_ERROR_UNSUPPORTED;
}

} // namespace aidl::android::hardware::graphics::composer3::impl
