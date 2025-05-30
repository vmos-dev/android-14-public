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

#define LOG_TAG "APM::AudioOutputDescriptor"
//#define LOG_NDEBUG 0

#include <android-base/stringprintf.h>

#include <AudioPolicyInterface.h>
#include "AudioOutputDescriptor.h"
#include "AudioPolicyMix.h"
#include "IOProfile.h"
#include "Volume.h"
#include "HwModule.h"
#include "TypeConverter.h"
#include "policy.h"
#include <media/AudioGain.h>
#include <media/AudioParameter.h>
#include <media/AudioPolicy.h>

// A device mask for all audio output devices that are considered "remote" when evaluating
// active output devices in isStreamActiveRemotely()

namespace android {

static const DeviceTypeSet& getAllOutRemoteDevices() {
    static const DeviceTypeSet allOutRemoteDevices = {AUDIO_DEVICE_OUT_REMOTE_SUBMIX};
    return allOutRemoteDevices;
}

AudioOutputDescriptor::AudioOutputDescriptor(const sp<PolicyAudioPort>& policyAudioPort,
                                             AudioPolicyClientInterface *clientInterface)
    : mPolicyAudioPort(policyAudioPort), mClientInterface(clientInterface)
{
    if (mPolicyAudioPort.get() != nullptr) {
        mPolicyAudioPort->pickAudioProfile(mSamplingRate, mChannelMask, mFormat);
        if (mPolicyAudioPort->asAudioPort()->getGains().size() > 0) {
            mPolicyAudioPort->asAudioPort()->getGains()[0]->getDefaultConfig(&mGain);
        }
    }
}

audio_config_base_t AudioOutputDescriptor::getConfig() const
{
    const audio_config_base_t config = { .sample_rate = mSamplingRate, .channel_mask = mChannelMask,
            .format = mFormat };
    return config;
}

audio_module_handle_t AudioOutputDescriptor::getModuleHandle() const
{
    return mPolicyAudioPort.get() != nullptr ?
            mPolicyAudioPort->getModuleHandle() : AUDIO_MODULE_HANDLE_NONE;
}

audio_patch_handle_t AudioOutputDescriptor::getPatchHandle() const
{
    return mPatchHandle;
}

void AudioOutputDescriptor::setPatchHandle(audio_patch_handle_t handle)
{
    mPatchHandle = handle;
}

bool AudioOutputDescriptor::sharesHwModuleWith(
        const sp<AudioOutputDescriptor>& outputDesc)
{
    return hasSameHwModuleAs(outputDesc);
}

void AudioOutputDescriptor::setStopTime(const sp<TrackClientDescriptor>& client, nsecs_t sysTime)
{
    mVolumeActivities[client->volumeSource()].setStopTime(sysTime);
    mRoutingActivities[client->strategy()].setStopTime(sysTime);
}

void AudioOutputDescriptor::setClientActive(const sp<TrackClientDescriptor>& client, bool active)
{
    auto clientIter = std::find(begin(mActiveClients), end(mActiveClients), client);
    if (active == (clientIter != end(mActiveClients))) {
        ALOGW("%s(%s): ignored active: %d, current stream count %d", __func__,
              client->toShortString().c_str(), active,
              mRoutingActivities.at(client->strategy()).getActivityCount());
        return;
    }
    if (active) {
        mActiveClients.push_back(client);
    } else {
        mActiveClients.erase(clientIter);
    }
    const int delta = active ? 1 : -1;
    // If ps is unknown, it is time to track it!
    mRoutingActivities[client->strategy()].changeActivityCount(delta);
    mVolumeActivities[client->volumeSource()].changeActivityCount(delta);

    // Handle non-client-specific activity ref count
    int32_t oldGlobalActiveCount = mGlobalActiveCount;
    if (!active && mGlobalActiveCount < 1) {
        ALOGW("%s(%s): invalid deactivation with globalRefCount %d",
              __func__, client->toShortString().c_str(), mGlobalActiveCount);
        mGlobalActiveCount = 1;
    }
    mGlobalActiveCount += delta;

    sp<AudioPolicyMix> policyMix = mPolicyMix.promote();
    if ((policyMix != NULL) && ((policyMix->mCbFlags & AudioMix::kCbFlagNotifyActivity) != 0)) {
        if ((oldGlobalActiveCount == 0) || (mGlobalActiveCount == 0)) {
            mClientInterface->onDynamicPolicyMixStateUpdate(policyMix->mDeviceAddress,
                mGlobalActiveCount > 0 ? MIX_STATE_MIXING : MIX_STATE_IDLE);
        }
    }
    client->setActive(active);
}

bool AudioOutputDescriptor::isClientActive(const sp<TrackClientDescriptor>& client)
{
    return client != nullptr &&
            std::find(begin(mActiveClients), end(mActiveClients), client) != end(mActiveClients);
}

bool AudioOutputDescriptor::isActive(VolumeSource vs, uint32_t inPastMs, nsecs_t sysTime) const
{
    return (vs == VOLUME_SOURCE_NONE) ?
                isActive(inPastMs) : (mVolumeActivities.find(vs) != std::end(mVolumeActivities)?
                mVolumeActivities.at(vs).isActive(inPastMs, sysTime) : false);
}

bool AudioOutputDescriptor::isActive(uint32_t inPastMs) const
{
    nsecs_t sysTime = 0;
    if (inPastMs != 0) {
        sysTime = systemTime();
    }
    for (const auto &iter : mVolumeActivities) {
        if (iter.second.isActive(inPastMs, sysTime)) {
            return true;
        }
    }
    return false;
}

bool AudioOutputDescriptor::isFixedVolume(const DeviceTypeSet& deviceTypes __unused)
{
    return false;
}

bool AudioOutputDescriptor::setVolume(float volumeDb, bool /*muted*/,
                                      VolumeSource volumeSource,
                                      const StreamTypeVector &/*streams*/,
                                      const DeviceTypeSet& deviceTypes,
                                      uint32_t delayMs,
                                      bool force,
                                      bool isVoiceVolSrc)
{

    if (!supportedDevices().containsDeviceAmongTypes(deviceTypes)) {
        ALOGV("%s output ID %d unsupported device %s",
                __func__, getId(), toString(deviceTypes).c_str());
        return false;
    }
    // We actually change the volume if:
    // - the float value returned by computeVolume() changed
    // - the force flag is set
    if (volumeDb != getCurVolume(volumeSource) || force) {
        ALOGV("%s for volumeSrc %d, volume %f, delay %d", __func__, volumeSource, volumeDb, delayMs);
        setCurVolume(volumeSource, volumeDb, isVoiceVolSrc);
        return true;
    }
    return false;
}

status_t AudioOutputDescriptor::applyAudioPortConfig(const struct audio_port_config *config,
                                                     audio_port_config *backupConfig)
{
    struct audio_port_config localBackupConfig = { .config_mask = config->config_mask };
    status_t status = NO_ERROR;

    toAudioPortConfig(&localBackupConfig);
    if ((status = validationBeforeApplyConfig(config)) == NO_ERROR) {
        AudioPortConfig::applyAudioPortConfig(config, backupConfig);
    }

    if (backupConfig != NULL) {
        *backupConfig = localBackupConfig;
    }
    return status;
}


void AudioOutputDescriptor::toAudioPortConfig(struct audio_port_config *dstConfig,
                                              const struct audio_port_config *srcConfig) const
{
    dstConfig->config_mask = AUDIO_PORT_CONFIG_ALL;
    if (srcConfig != NULL) {
        dstConfig->config_mask |= srcConfig->config_mask;
    }
    AudioPortConfig::toAudioPortConfig(dstConfig, srcConfig);

    dstConfig->role = AUDIO_PORT_ROLE_SOURCE;
    dstConfig->type = AUDIO_PORT_TYPE_MIX;
    dstConfig->ext.mix.hw_module = getModuleHandle();
    dstConfig->ext.mix.usecase.stream = AUDIO_STREAM_DEFAULT;
}

void AudioOutputDescriptor::toAudioPort(struct audio_port_v7 *port) const
{
    // Should not be called for duplicated ports, see SwAudioOutputDescriptor::toAudioPortConfig.
    mPolicyAudioPort->asAudioPort()->toAudioPort(port);
    port->id = mId;
    port->ext.mix.hw_module = getModuleHandle();
}

TrackClientVector AudioOutputDescriptor::clientsList(bool activeOnly, product_strategy_t strategy,
                                                     bool preferredDeviceOnly) const
{
    TrackClientVector clients;
    for (const auto &client : getClientIterable()) {
        if ((!activeOnly || client->active())
            && (strategy == PRODUCT_STRATEGY_NONE || strategy == client->strategy())
            && (!preferredDeviceOnly ||
                (client->hasPreferredDevice() && !client->isPreferredDeviceForExclusiveUse()))) {
            clients.push_back(client);
        }
    }
    return clients;
}

size_t AudioOutputDescriptor::sameExclusivePreferredDevicesCount() const
{
    audio_port_handle_t deviceId = AUDIO_PORT_HANDLE_NONE;
    size_t count = 0;
    for (const auto &client : getClientIterable()) {
        if (client->active()) {
            if (!(client->hasPreferredDevice() &&
                    client->isPreferredDeviceForExclusiveUse())) {
                return 0;
            }
            if (deviceId == AUDIO_PORT_HANDLE_NONE) {
                deviceId = client->preferredDeviceId();
            } else if (deviceId != client->preferredDeviceId()) {
                return 0;
            }
            count++;
        }
    }
    return count;
}

bool AudioOutputDescriptor::isAnyActive(VolumeSource volumeSourceToIgnore) const
{
    return std::find_if(begin(mActiveClients), end(mActiveClients),
                        [&volumeSourceToIgnore](const auto &client) {
        return client->volumeSource() != volumeSourceToIgnore; }) != end(mActiveClients);
}

void AudioOutputDescriptor::dump(String8 *dst, int spaces, const char* extraInfo) const
{
    dst->appendFormat("Port ID: %d%s%s\n",
            mId, extraInfo != nullptr ? "; " : "", extraInfo != nullptr ? extraInfo : "");
    dst->appendFormat("%*s%s; %d; Channel mask: 0x%x\n", spaces, "",
            audio_format_to_string(mFormat), mSamplingRate, mChannelMask);
    dst->appendFormat("%*sDevices: %s\n", spaces, "",
            devices().toString(true /*includeSensitiveInfo*/).c_str());
    dst->appendFormat("%*sGlobal active count: %u\n", spaces, "", mGlobalActiveCount);
    if (!mRoutingActivities.empty()) {
        dst->appendFormat("%*s- Product Strategies (%zu):\n", spaces - 2, "",
                mRoutingActivities.size());
        for (const auto &iter : mRoutingActivities) {
            dst->appendFormat("%*sid %d: ", spaces + 1, "", iter.first);
            iter.second.dump(dst, 0);
        }
    }
    if (!mVolumeActivities.empty()) {
        dst->appendFormat("%*s- Volume Activities (%zu):\n", spaces - 2, "",
                mVolumeActivities.size());
        for (const auto &iter : mVolumeActivities) {
            dst->appendFormat("%*sid %d: ", spaces + 1, "", iter.first);
            iter.second.dump(dst, 0);
        }
    }
    if (getClientCount() != 0) {
        dst->appendFormat("%*s- AudioTrack clients (%zu):\n", spaces - 2, "", getClientCount());
        ClientMapHandler<TrackClientDescriptor>::dump(dst, spaces);
    }
    if (!mActiveClients.empty()) {
        dst->appendFormat("%*s- AudioTrack active (stream) clients (%zu):\n", spaces - 2, "",
                mActiveClients.size());
        size_t index = 0;
        for (const auto& client : mActiveClients) {
            const std::string prefix = base::StringPrintf(
                    "%*sid %zu: ", spaces + 1, "", index + 1);
            dst->appendFormat("%s", prefix.c_str());
            client->dump(dst, prefix.size());
        }
    }
}

void AudioOutputDescriptor::log(const char* indent)
{
    ALOGI("%sID: %d,0x%X, [rt:%d fmt:0x%X ch:0x%X]",
          indent, mId, mId, mSamplingRate, mFormat, mChannelMask);
}

// SwAudioOutputDescriptor implementation
SwAudioOutputDescriptor::SwAudioOutputDescriptor(const sp<IOProfile>& profile,
                                                 AudioPolicyClientInterface *clientInterface)
    : AudioOutputDescriptor(profile, clientInterface),
    mProfile(profile), mIoHandle(AUDIO_IO_HANDLE_NONE), mLatency(0),
    mOutput1(0), mOutput2(0), mDirectOpenCount(0),
    mDirectClientSession(AUDIO_SESSION_NONE)
{
    if (profile != NULL) {
        // By default, opening the output without immutable flags, the bit-perfect flags should be
        // applied when the apps explicitly request.
        mFlags = (audio_output_flags_t)(profile->getFlags() & (~AUDIO_OUTPUT_FLAG_BIT_PERFECT));
    }
}

void SwAudioOutputDescriptor::dump(String8 *dst, int spaces, const char* extraInfo) const
{
    String8 allExtraInfo;
    if (extraInfo != nullptr) {
        allExtraInfo.appendFormat("%s; ", extraInfo);
    }
    if (mProfile != nullptr) {
        allExtraInfo.appendFormat("IOProfile name:%s; ", mProfile->getName().c_str());
    }
    std::string flagsLiteral = toString(mFlags);
    allExtraInfo.appendFormat("Latency: %d; 0x%04x", mLatency, mFlags);
    if (!flagsLiteral.empty()) {
        allExtraInfo.appendFormat(" (%s)", flagsLiteral.c_str());
    }
    AudioOutputDescriptor::dump(dst, spaces, allExtraInfo.c_str());
}

DeviceVector SwAudioOutputDescriptor::devices() const
{
    if (isDuplicated()) {
        DeviceVector devices = mOutput1->devices();
        devices.merge(mOutput2->devices());
        return devices;
    }
    return mDevices;
}

bool SwAudioOutputDescriptor::sharesHwModuleWith(
        const sp<SwAudioOutputDescriptor>& outputDesc)
{
    if (isDuplicated()) {
        return mOutput1->sharesHwModuleWith(outputDesc) || mOutput2->sharesHwModuleWith(outputDesc);
    } else if (outputDesc->isDuplicated()){
        return sharesHwModuleWith(outputDesc->subOutput1()) ||
                    sharesHwModuleWith(outputDesc->subOutput2());
    } else {
        return AudioOutputDescriptor::sharesHwModuleWith(outputDesc);
    }
}

DeviceVector SwAudioOutputDescriptor::supportedDevices() const
{
    if (isDuplicated()) {
        DeviceVector supportedDevices = mOutput1->supportedDevices();
        supportedDevices.merge(mOutput2->supportedDevices());
        return supportedDevices;
    }
    return mProfile->getSupportedDevices();
}

bool SwAudioOutputDescriptor::supportsDevice(const sp<DeviceDescriptor> &device) const
{
    return supportedDevices().contains(device);
}

bool SwAudioOutputDescriptor::supportsAllDevices(const DeviceVector &devices) const
{
    return supportedDevices().containsAllDevices(devices);
}

bool SwAudioOutputDescriptor::supportsAtLeastOne(const DeviceVector &devices) const
{
    return filterSupportedDevices(devices).size() > 0;
}

bool SwAudioOutputDescriptor::supportsDevicesForPlayback(const DeviceVector &devices) const
{
    // No considering duplicated output
    // TODO: need to verify if the profile supports the devices combo for playback.
    return !isDuplicated() && supportsAllDevices(devices);
}

DeviceVector SwAudioOutputDescriptor::filterSupportedDevices(const DeviceVector &devices) const
{
    DeviceVector filteredDevices = supportedDevices();
    return filteredDevices.filter(devices);
}

bool SwAudioOutputDescriptor::devicesSupportEncodedFormats(const DeviceTypeSet& deviceTypes)
{
    if (isDuplicated()) {
        return (mOutput1->devicesSupportEncodedFormats(deviceTypes)
                    || mOutput2->devicesSupportEncodedFormats(deviceTypes));
    } else {
       return mProfile->devicesSupportEncodedFormats(deviceTypes);
    }
}

bool SwAudioOutputDescriptor::containsSingleDeviceSupportingEncodedFormats(
        const sp<DeviceDescriptor>& device) const
{
    if (isDuplicated()) {
        return (mOutput1->containsSingleDeviceSupportingEncodedFormats(device) &&
                mOutput2->containsSingleDeviceSupportingEncodedFormats(device));
    }
    return mProfile->containsSingleDeviceSupportingEncodedFormats(device);
}

uint32_t SwAudioOutputDescriptor::latency()
{
    if (isDuplicated()) {
        return (mOutput1->mLatency > mOutput2->mLatency) ? mOutput1->mLatency : mOutput2->mLatency;
    } else {
        return mLatency;
    }
}

void SwAudioOutputDescriptor::setClientActive(const sp<TrackClientDescriptor>& client, bool active)
{
    // forward usage count change to attached outputs
    if (isDuplicated()) {
        mOutput1->setClientActive(client, active);
        mOutput2->setClientActive(client, active);
    }
    AudioOutputDescriptor::setClientActive(client, active);
}

bool SwAudioOutputDescriptor::isFixedVolume(const DeviceTypeSet& deviceTypes)
{
    // unit gain if rerouting to external policy
    if (isSingleDeviceType(deviceTypes, AUDIO_DEVICE_OUT_REMOTE_SUBMIX)) {
        if (mPolicyMix != NULL) {
            ALOGV("max gain when rerouting for output=%d", mIoHandle);
            return true;
        }
    }
    if (isSingleDeviceType(deviceTypes, AUDIO_DEVICE_OUT_TELEPHONY_TX)) {
        ALOGV("max gain when output device is telephony tx");
        return true;
    }
    return false;
}

void SwAudioOutputDescriptor::toAudioPortConfig(
                                                 struct audio_port_config *dstConfig,
                                                 const struct audio_port_config *srcConfig) const
{

    ALOG_ASSERT(!isDuplicated(), "toAudioPortConfig() called on duplicated output %d", mIoHandle);
    AudioOutputDescriptor::toAudioPortConfig(dstConfig, srcConfig);

    dstConfig->ext.mix.handle = mIoHandle;
}

void SwAudioOutputDescriptor::toAudioPort(struct audio_port_v7 *port) const
{
    ALOG_ASSERT(!isDuplicated(), "toAudioPort() called on duplicated output %d", mIoHandle);

    AudioOutputDescriptor::toAudioPort(port);

    toAudioPortConfig(&port->active_config);
    port->ext.mix.handle = mIoHandle;
    port->ext.mix.latency_class =
            mFlags & AUDIO_OUTPUT_FLAG_FAST ? AUDIO_LATENCY_LOW : AUDIO_LATENCY_NORMAL;
}

void SwAudioOutputDescriptor::setSwMute(
        bool muted, VolumeSource vs, const StreamTypeVector &streamTypes,
        const DeviceTypeSet& deviceTypes, uint32_t delayMs) {
    // volume source active and more than one volume source is active, otherwise, no-op or let
    // setVolume controlling SW and/or HW Gains
    if (!streamTypes.empty() && isActive(vs) && (getActiveVolumeSources().size() > 1)) {
        for (const auto& devicePort : devices()) {
            if (isSingleDeviceType(deviceTypes, devicePort->type()) &&
                    devicePort->hasGainController(true /*canUseForVolume*/)) {
                float volumeAmpl = muted ? 0.0f : Volume::DbToAmpl(0);
                ALOGV("%s: output: %d, vs: %d, muted: %d, active vs count: %zu", __func__,
                      mIoHandle, vs, muted, getActiveVolumeSources().size());
                for (const auto &stream : streamTypes) {
                    mClientInterface->setStreamVolume(stream, volumeAmpl, mIoHandle, delayMs);
                }
                return;
            }
        }
    }
}

bool SwAudioOutputDescriptor::setVolume(float volumeDb, bool muted,
                                        VolumeSource vs, const StreamTypeVector &streamTypes,
                                        const DeviceTypeSet& deviceTypes,
                                        uint32_t delayMs,
                                        bool force,
                                        bool isVoiceVolSrc)
{
    StreamTypeVector streams = streamTypes;
    if (!AudioOutputDescriptor::setVolume(
            volumeDb, muted, vs, streamTypes, deviceTypes, delayMs, force, isVoiceVolSrc)) {
        return false;
    }
    if (streams.empty()) {
        streams.push_back(AUDIO_STREAM_MUSIC);
    }
    for (const auto& devicePort : devices()) {
        // APM loops on all group, so filter on active group to set the port gain,
        // let the other groups set the stream volume as per legacy
        // TODO: Pass in the device address and check against it.
        if (isSingleDeviceType(deviceTypes, devicePort->type()) &&
                devicePort->hasGainController(true) && isActive(vs)) {
            ALOGV("%s: device %s has gain controller", __func__, devicePort->toString().c_str());
            // @todo: here we might be in trouble if the SwOutput has several active clients with
            // different Volume Source (or if we allow several curves within same volume group)
            //
            // @todo: default stream volume to max (0) when using HW Port gain?
            // Allows to set SW Gain on AudioFlinger if:
            //    -volume group has explicit stream(s) associated
            //    -volume group with no explicit stream(s) is the only active source on this output
            // Allows to mute SW Gain on AudioFlinger only for volume group with explicit stream(s)
            if (!streamTypes.empty() || (getActiveVolumeSources().size() == 1)) {
                const bool canMute = muted && (volumeDb != 0.0f) && !streamTypes.empty();
                float volumeAmpl = canMute ? 0.0f : Volume::DbToAmpl(0);
                for (const auto &stream : streams) {
                    mClientInterface->setStreamVolume(stream, volumeAmpl, mIoHandle, delayMs);
                }
            }
            AudioGains gains = devicePort->getGains();
            int gainMinValueInMb = gains[0]->getMinValueInMb();
            int gainMaxValueInMb = gains[0]->getMaxValueInMb();
            int gainStepValueInMb = gains[0]->getStepValueInMb();
            int gainValueMb = ((volumeDb * 100)/ gainStepValueInMb) * gainStepValueInMb;
            gainValueMb = std::max(gainMinValueInMb, std::min(gainValueMb, gainMaxValueInMb));

            audio_port_config config = {};
            devicePort->toAudioPortConfig(&config);
            config.config_mask = AUDIO_PORT_CONFIG_GAIN;
            config.gain.values[0] = gainValueMb;
            //-----rk-code-----//
            config.gain.mode = AUDIO_GAIN_MODE_JOINT;
            //----------------//
            return mClientInterface->setAudioPortConfig(&config, 0) == NO_ERROR;
        }
    }
    // Force VOICE_CALL to track BLUETOOTH_SCO stream volume when bluetooth audio is enabled
    float volumeAmpl = Volume::DbToAmpl(getCurVolume(vs));
    if (hasStream(streams, AUDIO_STREAM_BLUETOOTH_SCO)) {
        mClientInterface->setStreamVolume(AUDIO_STREAM_VOICE_CALL, volumeAmpl, mIoHandle, delayMs);
        VolumeSource callVolSrc = getVoiceSource();
        if (callVolSrc != VOLUME_SOURCE_NONE) {
            setCurVolume(callVolSrc, getCurVolume(vs), true);
        }
    }
    for (const auto &stream : streams) {
        ALOGV("%s output %d for volumeSource %d, volume %f, delay %d stream=%s", __func__,
              mIoHandle, vs, volumeDb, delayMs, toString(stream).c_str());
        mClientInterface->setStreamVolume(stream, volumeAmpl, mIoHandle, delayMs);
    }
    return true;
}

status_t SwAudioOutputDescriptor::open(const audio_config_t *halConfig,
                                       const audio_config_base_t *mixerConfig,
                                       const DeviceVector &devices,
                                       audio_stream_type_t stream,
                                       audio_output_flags_t flags,
                                       audio_io_handle_t *output)
{
    mDevices = devices;
    sp<DeviceDescriptor> device = devices.getDeviceForOpening();
    LOG_ALWAYS_FATAL_IF(device == nullptr,
                        "%s failed to get device descriptor for opening "
                        "with the requested devices, all device types: %s",
                        __func__, dumpDeviceTypes(devices.types()).c_str());

    audio_config_t lHalConfig;
    if (halConfig == nullptr) {
        lHalConfig = AUDIO_CONFIG_INITIALIZER;
        lHalConfig.sample_rate = mSamplingRate;
        lHalConfig.channel_mask = mChannelMask;
        lHalConfig.format = mFormat;
    } else {
        lHalConfig = *halConfig;
    }

    // if the selected profile is offloaded and no offload info was specified,
    // create a default one
    if ((mProfile->getFlags() & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) &&
            lHalConfig.offload_info.format == AUDIO_FORMAT_DEFAULT) {
        flags = (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD);
        lHalConfig.offload_info = AUDIO_INFO_INITIALIZER;
        lHalConfig.offload_info.sample_rate = lHalConfig.sample_rate;
        lHalConfig.offload_info.channel_mask = lHalConfig.channel_mask;
        lHalConfig.offload_info.format = lHalConfig.format;
        lHalConfig.offload_info.stream_type = stream;
    }

    audio_config_base_t lMixerConfig;
    if (mixerConfig == nullptr) {
        lMixerConfig = AUDIO_CONFIG_BASE_INITIALIZER;
        lMixerConfig.sample_rate = lHalConfig.sample_rate;
        lMixerConfig.channel_mask = lHalConfig.channel_mask;
        lMixerConfig.format = lHalConfig.format;
    } else {
        lMixerConfig = *mixerConfig;
    }

    mFlags = (audio_output_flags_t)(mFlags | flags);

    // If no mixer config is specified for a spatializer output, default to 5.1 for proper
    // configuration of the final downmixer or spatializer
    if ((mFlags & AUDIO_OUTPUT_FLAG_SPATIALIZER) != 0
            && mixerConfig == nullptr) {
        lMixerConfig.channel_mask = AUDIO_CHANNEL_OUT_5POINT1;
    }

    ALOGV("opening output for device %s profile %p name %s",
          mDevices.toString().c_str(), mProfile.get(), mProfile->getName().c_str());

    status_t status = mClientInterface->openOutput(mProfile->getModuleHandle(),
                                                   output,
                                                   &lHalConfig,
                                                   &lMixerConfig,
                                                   device,
                                                   &mLatency,
                                                   mFlags);

    if (status == NO_ERROR) {
        LOG_ALWAYS_FATAL_IF(*output == AUDIO_IO_HANDLE_NONE,
                            "%s openOutput returned output handle %d for device %s, "
                            "selected device %s for opening",
                            __FUNCTION__, *output, devices.toString().c_str(),
                            device->toString().c_str());
        mSamplingRate = lHalConfig.sample_rate;
        mChannelMask = lHalConfig.channel_mask;
        mFormat = lHalConfig.format;
        mMixerChannelMask = lMixerConfig.channel_mask;
        mId = PolicyAudioPort::getNextUniqueId();
        mIoHandle = *output;
        mProfile->curOpenCount++;
    }

    return status;
}

status_t SwAudioOutputDescriptor::start()
{
    if (isDuplicated()) {
        status_t status = mOutput1->start();
        if (status != NO_ERROR) {
            return status;
        }
        status = mOutput2->start();
        if (status != NO_ERROR) {
            mOutput1->stop();
            return status;
        }
        return NO_ERROR;
    }
    if (!isActive()) {
        if (!mProfile->canStartNewIo()) {
            return INVALID_OPERATION;
        }
        mProfile->curActiveCount++;
    }
    return NO_ERROR;
}

void SwAudioOutputDescriptor::stop()
{
    if (isDuplicated()) {
        mOutput1->stop();
        mOutput2->stop();
        return;
    }

    if (!isActive()) {
        LOG_ALWAYS_FATAL_IF(mProfile->curActiveCount < 1,
                            "%s invalid profile active count %u",
                            __func__, mProfile->curActiveCount);
        mProfile->curActiveCount--;
    }
}

void SwAudioOutputDescriptor::close()
{
    if (mIoHandle != AUDIO_IO_HANDLE_NONE) {
        // clean up active clients if any (can happen if close() is called to force
        // clients to reconnect
        for (const auto &client : getClientIterable()) {
            if (client->active()) {
                ALOGW("%s client with port ID %d still active on output %d",
                      __func__, client->portId(), mId);
                setClientActive(client, false);
                stop();
            }
        }

        mClientInterface->closeOutput(mIoHandle);

        LOG_ALWAYS_FATAL_IF(mProfile->curOpenCount < 1, "%s profile open count %u",
                            __FUNCTION__, mProfile->curOpenCount);
        mProfile->curOpenCount--;
        mIoHandle = AUDIO_IO_HANDLE_NONE;
    }
}

status_t SwAudioOutputDescriptor::openDuplicating(const sp<SwAudioOutputDescriptor>& output1,
                                                  const sp<SwAudioOutputDescriptor>& output2,
                                                  audio_io_handle_t *ioHandle)
{
    // open a duplicating output thread for the new output and the primary output
    // Note: openDuplicateOutput() API expects the output handles in the reverse order from the
    // numbering in SwAudioOutputDescriptor mOutput1 and mOutput2
    *ioHandle = mClientInterface->openDuplicateOutput(output2->mIoHandle, output1->mIoHandle);
    if (*ioHandle == AUDIO_IO_HANDLE_NONE) {
        return INVALID_OPERATION;
    }

    mId = PolicyAudioPort::getNextUniqueId();
    mIoHandle = *ioHandle;
    mOutput1 = output1;
    mOutput2 = output2;
    mSamplingRate = output2->mSamplingRate;
    mFormat = output2->mFormat;
    mChannelMask = output2->mChannelMask;
    mLatency = output2->mLatency;

    return NO_ERROR;
}

uint32_t SwAudioOutputDescriptor::getRecommendedMuteDurationMs() const
{
    if (isDuplicated()) {
        return std::max(mOutput1->getRecommendedMuteDurationMs(),
                mOutput2->getRecommendedMuteDurationMs());
    }
    return mProfile->recommendedMuteDurationMs;
}

void SwAudioOutputDescriptor::setTracksInvalidatedStatusByStrategy(product_strategy_t strategy) {
    for (const auto &client : getClientIterable()) {
        if (strategy == client->strategy()) {
            client->setIsInvalid();
        }
    }
}

// HwAudioOutputDescriptor implementation
HwAudioOutputDescriptor::HwAudioOutputDescriptor(const sp<SourceClientDescriptor>& source,
                                                 AudioPolicyClientInterface *clientInterface)
    : AudioOutputDescriptor(source->srcDevice(), clientInterface),
      mSource(source)
{
}

void HwAudioOutputDescriptor::dump(String8 *dst, int spaces, const char* extraInfo) const
{
    AudioOutputDescriptor::dump(dst, spaces, extraInfo);
    dst->appendFormat("%*sSource:\n", spaces, "");
    mSource->dump(dst, spaces);
}

void HwAudioOutputDescriptor::toAudioPortConfig(
                                                 struct audio_port_config *dstConfig,
                                                 const struct audio_port_config *srcConfig) const
{
    mSource->srcDevice()->toAudioPortConfig(dstConfig, srcConfig);
}

void HwAudioOutputDescriptor::toAudioPort(struct audio_port_v7 *port) const
{
    mSource->srcDevice()->toAudioPort(port);
}


bool HwAudioOutputDescriptor::setVolume(float volumeDb, bool muted,
                                        VolumeSource volumeSource, const StreamTypeVector &streams,
                                        const DeviceTypeSet& deviceTypes,
                                        uint32_t delayMs,
                                        bool force,
                                        bool isVoiceVolSrc)
{
    bool changed = AudioOutputDescriptor::setVolume(
            volumeDb, muted, volumeSource, streams, deviceTypes, delayMs, force, isVoiceVolSrc);

    if (changed) {
      // TODO: use gain controller on source device if any to adjust volume
    }
    return changed;
}

// SwAudioOutputCollection implementation
bool SwAudioOutputCollection::isActive(VolumeSource volumeSource, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < this->size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = this->valueAt(i);
        if (outputDesc->isActive(volumeSource, inPastMs, sysTime)) {
            return true;
        }
    }
    return false;
}

bool SwAudioOutputCollection::isActiveLocally(VolumeSource volumeSource, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < this->size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = this->valueAt(i);
        if (outputDesc->isActive(volumeSource, inPastMs, sysTime)
                && (!(outputDesc->devices()
                        .containsDeviceAmongTypes(getAllOutRemoteDevices())
                        || outputDesc->devices()
                            .onlyContainsDevicesWithType(AUDIO_DEVICE_OUT_TELEPHONY_TX)))) {
            return true;
        }
    }
    return false;
}

bool SwAudioOutputCollection::isActiveRemotely(VolumeSource volumeSource, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->devices().containsDeviceAmongTypes(getAllOutRemoteDevices()) &&
                outputDesc->isActive(volumeSource, inPastMs, sysTime)) {
            // do not consider re routing (when the output is going to a dynamic policy)
            // as "remote playback"
            if (outputDesc->mPolicyMix == NULL) {
                return true;
            }
        }
    }
    return false;
}

bool SwAudioOutputCollection::isStrategyActiveOnSameModule(product_strategy_t ps,
                                                           const sp<SwAudioOutputDescriptor>& desc,
                                                           uint32_t inPastMs, nsecs_t sysTime) const
{
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> otherDesc = valueAt(i);
        if (desc->sharesHwModuleWith(otherDesc) &&
                otherDesc->isStrategyActive(ps, inPastMs, sysTime)) {
            if (desc == otherDesc
                    || !otherDesc->devices()
                            .onlyContainsDevicesWithType(AUDIO_DEVICE_OUT_TELEPHONY_TX)) {
                return true;
            }
        }
    }
    return false;
}

bool SwAudioOutputCollection::isStrategyActive(product_strategy_t ps) const
{
    for (size_t i = 0; i < size(); i++) {
        if (valueAt(i)->isStrategyActive(ps)) {
            return true;
        }
    }
    return false;
}

audio_io_handle_t SwAudioOutputCollection::getA2dpOutput() const
{
    for (size_t i = 0; i < size(); i++) {
        sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (!outputDesc->isDuplicated() &&
             outputDesc->devices().containsDeviceAmongTypes(getAudioDeviceOutAllA2dpSet()) &&
             outputDesc->devicesSupportEncodedFormats(getAudioDeviceOutAllA2dpSet())) {
            return this->keyAt(i);
        }
    }
    return 0;
}

bool SwAudioOutputCollection::isA2dpOffloadedOnPrimary() const
{
    sp<SwAudioOutputDescriptor> primaryOutput = getPrimaryOutput();

    if ((primaryOutput != NULL) && (primaryOutput->mProfile != NULL)
        && (primaryOutput->mProfile->getModule() != NULL)) {
        sp<HwModule> primaryHwModule = primaryOutput->mProfile->getModule();

        for (const auto &outputProfile : primaryHwModule->getOutputProfiles()) {
            if (outputProfile->supportsDeviceTypes(getAudioDeviceOutAllA2dpSet())) {
                return true;
            }
        }
    }
    return false;
}

sp<SwAudioOutputDescriptor> SwAudioOutputCollection::getPrimaryOutput() const
{
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->mFlags & AUDIO_OUTPUT_FLAG_PRIMARY) {
            return outputDesc;
        }
    }
    return NULL;
}

sp<SwAudioOutputDescriptor> SwAudioOutputCollection::getOutputFromId(audio_port_handle_t id) const
{
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->getId() == id) {
            return outputDesc;
        }
    }
    return NULL;
}

sp<SwAudioOutputDescriptor> SwAudioOutputCollection::getOutputForClient(audio_port_handle_t portId)
{
    for (size_t i = 0; i < size(); i++) {
        sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->getClient(portId) != nullptr) {
            return outputDesc;
        }
    }
    return 0;
}

void SwAudioOutputCollection::clearSessionRoutesForDevice(
        const sp<DeviceDescriptor> &disconnectedDevice)
{
    for (size_t i = 0; i < size(); i++) {
        sp<AudioOutputDescriptor> outputDesc = valueAt(i);
        for (const auto& client : outputDesc->getClientIterable()) {
            if (client->preferredDeviceId() == disconnectedDevice->getId()) {
                client->setPreferredDeviceId(AUDIO_PORT_HANDLE_NONE);
            }
        }
    }
}
bool SwAudioOutputCollection::isAnyDeviceTypeActive(const DeviceTypeSet& deviceTypes) const {
    for (size_t i = 0; i < size(); i++) {
        const sp<SwAudioOutputDescriptor> outputDesc = valueAt(i);
        if (outputDesc->isActive()
                && outputDesc->devices().containsDeviceAmongTypes(deviceTypes)) {
            return true;
        }
    }
    return false;
}

bool SwAudioOutputDescriptor::isConfigurationMatched(const audio_config_base_t &config,
                                                     audio_output_flags_t flags) {
    const uint32_t mustMatchOutputFlags =
            AUDIO_OUTPUT_FLAG_DIRECT|AUDIO_OUTPUT_FLAG_HW_AV_SYNC|AUDIO_OUTPUT_FLAG_MMAP_NOIRQ;
    return audio_output_flags_is_subset(AudioOutputDescriptor::mFlags, flags, mustMatchOutputFlags)
            && mSamplingRate == config.sample_rate
            && mChannelMask == config.channel_mask
            && mFormat == config.format;
}

PortHandleVector SwAudioOutputDescriptor::getClientsForStream(
        audio_stream_type_t streamType) const {
    PortHandleVector clientsForStream;
    for (const auto& client : getClientIterable()) {
        if (client->stream() == streamType) {
            clientsForStream.push_back(client->portId());
        }
    }
    return clientsForStream;
}

void SwAudioOutputCollection::dump(String8 *dst) const
{
    dst->appendFormat("\n Outputs (%zu):\n", size());
    for (size_t i = 0; i < size(); i++) {
        const std::string prefix = base::StringPrintf("  %zu. ", i + 1);
        const std::string extraInfo = base::StringPrintf("I/O handle: %d", keyAt(i));
        dst->appendFormat("%s", prefix.c_str());
        valueAt(i)->dump(dst, prefix.size(), extraInfo.c_str());
    }
}

// HwAudioOutputCollection implementation
bool HwAudioOutputCollection::isActive(VolumeSource volumeSource, uint32_t inPastMs) const
{
    nsecs_t sysTime = systemTime();
    for (size_t i = 0; i < this->size(); i++) {
        const sp<HwAudioOutputDescriptor> outputDesc = this->valueAt(i);
        if (outputDesc->isActive(volumeSource, inPastMs, sysTime)) {
            return true;
        }
    }
    return false;
}

void HwAudioOutputCollection::dump(String8 *dst) const
{
    dst->appendFormat("\n Outputs (%zu):\n", size());
    for (size_t i = 0; i < size(); i++) {
        const std::string prefix = base::StringPrintf("  %zu. ", i + 1);
        const std::string extraInfo = base::StringPrintf("I/O handle: %d", keyAt(i));
        dst->appendFormat("%s", prefix.c_str());
        valueAt(i)->dump(dst, prefix.size(), extraInfo.c_str());
    }
}

}; //namespace android
