/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "GnssAidl"

#include "Gnss.h"
#include <inttypes.h>
#include <log/log.h>
#include <utils/Timers.h>
#include "AGnss.h"
#include "AGnssRil.h"
#include "DeviceFileReader.h"
#include "FixLocationParser.h"
#include "GnssAntennaInfo.h"
#include "GnssBatching.h"
#include "GnssConfiguration.h"
#include "GnssDebug.h"
#include "GnssGeofence.h"
#include "GnssNavigationMessageInterface.h"
#include "GnssPsds.h"
#include "GnssVisibilityControl.h"
#include "MeasurementCorrectionsInterface.h"
#include "Utils.h"
#include <hardware/gps.h>


namespace aidl::android::hardware::gnss {
using ::android::hardware::gnss::common::Utils;

using ndk::ScopedAStatus;
using GnssSvInfo = IGnssCallback::GnssSvInfo;


std::vector<std::unique_ptr<ThreadFuncArgs>> Gnss::sThreadFuncArgsList;
std::shared_ptr<IGnssCallback> Gnss::sGnssCallback = nullptr;
const GpsInterface* mGnssIface = nullptr;
GpsCallbacks Gnss::sGnssCb = {
    .size = sizeof(GpsCallbacks),
    .location_cb = locationCb,
    .status_cb = statusCb,
    .sv_status_cb = gpsSvStatusCb,
    .nmea_cb = nmeaCb,
    .set_capabilities_cb = setCapabilitiesCb,
    .acquire_wakelock_cb = acquireWakelockCb,
    .release_wakelock_cb = releaseWakelockCb,
    .create_thread_cb = createThreadCb,
    .request_utc_time_cb = requestUtcTimeCb,
    .set_system_info_cb = setSystemInfoCb,
    .gnss_sv_status_cb = gnssSvStatusCb,
};

bool Gnss::sWakelockHeldGnss = false;
bool Gnss::sWakelockHeldFused = false;
uint32_t Gnss::sCapabilitiesCached = 0;
uint16_t Gnss::sYearOfHwCached = 0;

GnssLocation convertToGnssLocation(GpsLocation* location) {
    GnssLocation gnssLocation ;
    if (location != nullptr) {
            // Bit operation AND with 1f below is needed to clear vertical accuracy,
            // speed accuracy and bearing accuracy flags as some vendors are found
            // to be setting these bits in pre-Android-O devices
            gnssLocation.gnssLocationFlags = static_cast<uint16_t>(location->flags & 0x1f);
            gnssLocation.latitudeDegrees = location->latitude;
            gnssLocation.longitudeDegrees = location->longitude;
            gnssLocation.altitudeMeters = location->altitude;
            gnssLocation.speedMetersPerSec = location->speed;
            gnssLocation.bearingDegrees = location->bearing;
            gnssLocation.horizontalAccuracyMeters = location->accuracy;
            // Older chipsets do not provide the following 3 fields, hence the flags
            // HAS_VERTICAL_ACCURACY, HAS_SPEED_ACCURACY and HAS_BEARING_ACCURACY are
            // not set and the field are set to zeros.
            gnssLocation.verticalAccuracyMeters = 0;
            gnssLocation.speedAccuracyMetersPerSecond = 0;
            gnssLocation.bearingAccuracyDegrees = 0;
            gnssLocation.timestampMillis = location->timestamp;
     }
    return gnssLocation;
}

Gnss::Gnss() : mMinIntervalMs(1000), mFirstFixReceived(false) {
   ALOGD("make Gnss");
   hw_module_t* module;
   int err = hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
   if (err == 0) {
      hw_device_t* device;
      gps_device_t* gnssDevice;
      err = module->methods->open(module, GPS_HARDWARE_MODULE_ID, &device);
      if (err == 0) {
         ALOGD("gnss hw_get_module ok");
         gnssDevice =(gps_device_t *)device;
         mGnssIface = gnssDevice->get_gps_interface(gnssDevice);
      } else {
         ALOGE("gnssDevice open %s failed: %d", GPS_HARDWARE_MODULE_ID, err);
      }
   } else {
      ALOGE("gnss hw_get_module %s failed: %d", GPS_HARDWARE_MODULE_ID, err);
   }
}
void Gnss::locationCb(GpsLocation* location) {
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (location == nullptr) {
        ALOGE("%s: Invalid location from GNSS HAL", __func__);
        return;
    }

    GnssLocation gnssLocation = convertToGnssLocation(location);

    if (sGnssCallback == nullptr) {
        ALOGE("%s: GnssCallback is null.", __func__);
        return;
    }
    auto status = sGnssCallback->gnssLocationCb(gnssLocation);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke gnssLocationCb", __func__);
    }
}

void Gnss::statusCb(GpsStatus* gnssStatus) {
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (gnssStatus == nullptr) {
        ALOGE("%s: Invalid GpsStatus from GNSS HAL", __func__);
        return;
    }

    IGnssCallback::GnssStatusValue status = static_cast<IGnssCallback::GnssStatusValue>(gnssStatus->status);
    if (sGnssCallback == nullptr) {
        ALOGE("%s: sGnssCallback is null.", __func__);
        return;
    }
    auto status1 = sGnssCallback->gnssStatusCb(status);
    if (!status1.isOk()) {
        ALOGE("%s: Unable to invoke gnssStatusCb", __func__);
    }
}

void Gnss::gnssSvStatusCb(GnssSvStatus* status) {
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (status == nullptr) {
        ALOGE("Invalid status from GNSS HAL %s", __func__);
        return;
    }


    int numSvs = status->num_svs;
    std::vector<GnssSvInfo> gnssSvInfoList;

    if (numSvs > 64) {
        ALOGW("Too many satellites %u. Clamps to 64.", numSvs);
        numSvs = 64;
    }

    for (int i = 0; i < numSvs; i++) {
        auto svInfo = status->gnss_sv_list[i];
        GnssSvInfo gnssSvInfo = {
            .svid = svInfo.svid,
            .constellation = static_cast<
                GnssConstellationType>(
                svInfo.constellation),
            .cN0Dbhz = svInfo.c_n0_dbhz,
            .elevationDegrees = svInfo.elevation,
            .azimuthDegrees = svInfo.azimuth,
            // Older chipsets do not provide carrier frequency, hence
            // HAS_CARRIER_FREQUENCY flag and the carrierFrequencyHz fields
            // are not set. So we are resetting both fields here.
            //.svFlag = static_cast<uint8_t>(
            //    svInfo.flags &= ~(static_cast<uint8_t>(
            //        V1_0::IGnssCallback::GnssSvFlags::HAS_CARRIER_FREQUENCY))),
            .svFlag = svInfo.flags,
            .carrierFrequencyHz = 1575420000};
        gnssSvInfoList.push_back(gnssSvInfo);
    }

   if (sGnssCallback == nullptr) {
        ALOGE("%s: sGnssCallback is null.", __func__);
        return;
    }
    auto res = sGnssCallback->gnssSvStatusCb(gnssSvInfoList);
    if (!res.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
}

/*
 * This enum is used by gpsSvStatusCb() method below to convert GpsSvStatus
 * to GnssSvStatus for backward compatibility. It is only used by the default
 * implementation and is not part of the GNSS interface.
 */
enum SvidValues : uint16_t {
    GLONASS_SVID_OFFSET = 64,
    GLONASS_SVID_COUNT = 24,
    BEIDOU_SVID_OFFSET = 200,
    BEIDOU_SVID_COUNT = 35,
    SBAS_SVID_MIN = 33,
    SBAS_SVID_MAX = 64,
    SBAS_SVID_ADD = 87,
    QZSS_SVID_MIN = 193,
    QZSS_SVID_MAX = 200
};

/*
 * The following code that converts GpsSvStatus to GnssSvStatus is moved here from
 * GnssLocationProvider. GnssLocationProvider does not require it anymore since GpsSvStatus is
 * being deprecated and is no longer part of the GNSS interface.
 */
void Gnss::gpsSvStatusCb(GpsSvStatus* svInfo) {
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (svInfo == nullptr) {
        ALOGE("Invalid status from GNSS HAL %s", __func__);
        return;
    }

    int numSvs = svInfo->num_svs;
    std::vector<GnssSvInfo> gnssSvInfoList;

    /*
     * Clamp the list size since GnssSvStatus can support a maximum of
     * GnssMax::SVS_COUNT entries.
     */
    if (numSvs > 64) {
        ALOGW("Too many satellites %u. Clamps to 64.", numSvs);
        numSvs = 64;
    }

    uint32_t ephemerisMask = svInfo->ephemeris_mask;
    uint32_t almanacMask = svInfo->almanac_mask;
    uint32_t usedInFixMask = svInfo->used_in_fix_mask;
    /*
     * Conversion from GpsSvInfo to IGnssCallback::GnssSvInfo happens below.
     */
    for (int i = 0; i < numSvs; i++) {
        GnssSvInfo info;
        info.svid = svInfo->sv_list[i].prn;
        if (info.svid >= 1 && info.svid <= 32) {
            info.constellation = GnssConstellationType::GPS;
        } else if (info.svid > GLONASS_SVID_OFFSET &&
                   info.svid <= GLONASS_SVID_OFFSET + GLONASS_SVID_COUNT) {
            info.constellation = GnssConstellationType::GLONASS;
            info.svid -= GLONASS_SVID_OFFSET;
        } else if (info.svid > BEIDOU_SVID_OFFSET &&
                 info.svid <= BEIDOU_SVID_OFFSET + BEIDOU_SVID_COUNT) {
            info.constellation = GnssConstellationType::BEIDOU;
            info.svid -= BEIDOU_SVID_OFFSET;
        } else if (info.svid >= SBAS_SVID_MIN && info.svid <= SBAS_SVID_MAX) {
            info.constellation = GnssConstellationType::SBAS;
            info.svid += SBAS_SVID_ADD;
        } else if (info.svid >= QZSS_SVID_MIN && info.svid <= QZSS_SVID_MAX) {
            info.constellation = GnssConstellationType::QZSS;
        } else {
            ALOGD("Unknown constellation type with Svid = %d.", info.svid);
            info.constellation = GnssConstellationType::UNKNOWN;
        }

        info.cN0Dbhz = svInfo->sv_list[i].snr;
        info.elevationDegrees = svInfo->sv_list[i].elevation;
        info.azimuthDegrees = svInfo->sv_list[i].azimuth;
        // TODO: b/31702236
        info.svFlag = static_cast<uint8_t>(IGnssCallback::GnssSvFlags::NONE);

        /*
         * Only GPS info is valid for these fields, as these masks are just 32
         * bits, by GPS prn.
         */
        if (info.constellation == GnssConstellationType::GPS) {
            int32_t svidMask = (1 << (info.svid - 1));
            if ((ephemerisMask & svidMask) != 0) {
                info.svFlag |= (int)IGnssCallback::GnssSvFlags::HAS_EPHEMERIS_DATA;
            }
            if ((almanacMask & svidMask) != 0) {
                info.svFlag |= (int)IGnssCallback::GnssSvFlags::HAS_ALMANAC_DATA;
            }
            if ((usedInFixMask & svidMask) != 0) {
                info.svFlag |= (int)IGnssCallback::GnssSvFlags::USED_IN_FIX;
            }
        }
        gnssSvInfoList.push_back(info);
    }

    if (sGnssCallback == nullptr) {
        ALOGE("%s: sGnssCallback is null.", __func__);
        return;
    }
    auto status = sGnssCallback->gnssSvStatusCb(gnssSvInfoList);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }

}

void Gnss::nmeaCb(GpsUtcTime timestamp, const char* nmea, int length) {
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    char  nmeaString[300]={0};
    memcpy(nmeaString,nmea,length);

   if(sGnssCallback != nullptr) {
        auto ret = sGnssCallback->gnssNmeaCb(timestamp, nmeaString);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

void Gnss::setCapabilitiesCb(uint32_t capabilities) {
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

   if(sGnssCallback != nullptr) {
        auto ret = sGnssCallback->gnssSetCapabilitiesCb(capabilities);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }

    // Save for reconnection when some legacy hal's don't resend this info
    sCapabilitiesCached = capabilities;
}

void Gnss::acquireWakelockCb() {
    acquireWakelockGnss();
}

void Gnss::releaseWakelockCb() {
    releaseWakelockGnss();
}


void Gnss::acquireWakelockGnss() {
    sWakelockHeldGnss = true;
    updateWakelock();
}

void Gnss::releaseWakelockGnss() {
    sWakelockHeldGnss = false;
    updateWakelock();
}

void Gnss::acquireWakelockFused() {
    sWakelockHeldFused = true;
    updateWakelock();
}

void Gnss::releaseWakelockFused() {
    sWakelockHeldFused = false;
    updateWakelock();
}

void Gnss::updateWakelock() {
    // Track the state of the last request - in case the wake lock in the layer above is reference
    // counted.
    static bool sWakelockHeld = false;

    if (sGnssCallback == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (sWakelockHeldGnss || sWakelockHeldFused) {
        if (!sWakelockHeld) {
            ALOGI("%s: GNSS HAL Wakelock acquired due to gps: %d, fused: %d", __func__,
                    sWakelockHeldGnss, sWakelockHeldFused);
            sWakelockHeld = true;
            if(sGnssCallback != nullptr) {
                  auto ret = sGnssCallback->gnssAcquireWakelockCb();
                 if (!ret.isOk()) {
                  ALOGE("%s: Unable to invoke callback", __func__);
                }
            }
        }
    } else {
        if (sWakelockHeld) {
            ALOGI("%s: GNSS HAL Wakelock released", __func__);
        } else  {
            // To avoid burning power, always release, even if logic got here with sWakelock false
            // which it shouldn't, unless underlying *.h implementation makes duplicate requests.
            ALOGW("%s: GNSS HAL Wakelock released, duplicate request", __func__);
        }
        sWakelockHeld = false;
        if(sGnssCallback != nullptr) {
            auto ret = sGnssCallback->gnssReleaseWakelockCb();
           if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
           }
       }
    }
}

void Gnss::requestUtcTimeCb() {
    if (sGnssCallback == nullptr ) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

   if(sGnssCallback != nullptr) {
        auto ret = sGnssCallback->gnssRequestTimeCb();
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

pthread_t Gnss::createThreadCb(const char* name, void (*start)(void*), void* arg) {
    return createPthread(name, start, arg, &sThreadFuncArgsList);
}

void Gnss::setSystemInfoCb(const LegacyGnssSystemInfo* info) {
    if (sGnssCallback == nullptr ) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (info == nullptr) {
        ALOGE("Invalid GnssSystemInfo from GNSS HAL %s", __func__);
        return;
    }

    IGnssCallback::GnssSystemInfo gnssInfo = {
        .yearOfHw = info->year_of_hw
    };

   if(sGnssCallback != nullptr) {
        auto ret = sGnssCallback->gnssSetSystemInfoCb(gnssInfo);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
    // Save for reconnection when some legacy hal's don't resend this info
    sYearOfHwCached = info->year_of_hw;
}

ScopedAStatus Gnss::setCallback(const std::shared_ptr<IGnssCallback>& callback) {
    ALOGD("setCallback");
	if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    if (callback == nullptr) {
        ALOGE("%s: Null callback ignored", __func__);
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    sGnssCallback = callback;

    int capabilities =
            (int)(IGnssCallback::CAPABILITY_MEASUREMENTS | IGnssCallback::CAPABILITY_SCHEDULING |
                  IGnssCallback::CAPABILITY_SATELLITE_BLOCKLIST |
                  IGnssCallback::CAPABILITY_SATELLITE_PVT |
                  IGnssCallback::CAPABILITY_CORRELATION_VECTOR |
                  IGnssCallback::CAPABILITY_ANTENNA_INFO |
                  IGnssCallback::CAPABILITY_ACCUMULATED_DELTA_RANGE);
    auto status = sGnssCallback->gnssSetCapabilitiesCb(capabilities);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback.gnssSetCapabilitiesCb", __func__);
    }

    IGnssCallback::GnssSystemInfo systemInfo = {
            .yearOfHw = 2022,
            .name = "Google, Cuttlefish, AIDL v3",
    };
    status = sGnssCallback->gnssSetSystemInfoCb(systemInfo);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback.gnssSetSystemInfoCb", __func__);
    }
    GnssSignalType signalType1 = {
            .constellation = GnssConstellationType::GPS,
            .carrierFrequencyHz = 1.57542e+09,
            .codeType = GnssSignalType::CODE_TYPE_C,
    };
    GnssSignalType signalType2 = {
            .constellation = GnssConstellationType::GLONASS,
            .carrierFrequencyHz = 1.5980625e+09,
            .codeType = GnssSignalType::CODE_TYPE_C,
    };
    status = sGnssCallback->gnssSetSignalTypeCapabilitiesCb(
            std::vector<GnssSignalType>({signalType1, signalType2}));
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback.gnssSetSignalTypeCapabilitiesCb", __func__);
    }
    if (sCapabilitiesCached != 0) {
        setCapabilitiesCb(sCapabilitiesCached);
    }
    if (sYearOfHwCached != 0) {
        LegacyGnssSystemInfo info;
        info.year_of_hw = sYearOfHwCached;
        setSystemInfoCb(&info);
    }
    if(mGnssIface->init(&sGnssCb)){
		ALOGE("%s: Unable to init gps module", __func__);
	}
    return ScopedAStatus::ok();
}

std::unique_ptr<GnssLocation> Gnss::getLocationFromHW() {
    if (!::android::hardware::gnss::common::ReplayUtils::hasFixedLocationDeviceFile()) {
        return nullptr;
    }
    std::string inputStr =
            ::android::hardware::gnss::common::DeviceFileReader::Instance().getLocationData();
    return ::android::hardware::gnss::common::FixLocationParser::getLocationFromInputStr(inputStr);
}

ScopedAStatus Gnss::start() {
    ALOGD("start()");
    if (mIsActive) {
        ALOGW("Gnss has started. Restarting...");
        stop();
    }

    mIsActive = true;
    mThreadBlocker.reset();
    // notify measurement engine to update measurement interval
    //mGnssMeasurementInterface->setLocationEnabled(true);
    this->reportGnssStatusValue(IGnssCallback::GnssStatusValue::SESSION_BEGIN);

    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    mGnssIface->start() ;

    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stop() {
    ALOGD("stop");
    mIsActive = false;
    mGnssMeasurementInterface->setLocationEnabled(false);
    this->reportGnssStatusValue(IGnssCallback::GnssStatusValue::SESSION_END);

    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    mGnssIface->stop();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::close() {
    ALOGD("close");
    sGnssCallback = nullptr;
    return ScopedAStatus::ok();
}

void Gnss::reportLocation(const GnssLocation& location) const {
    std::unique_lock<std::mutex> lock(mMutex);
    if (sGnssCallback == nullptr) {
        ALOGE("%s: GnssCallback is null.", __func__);
        return;
    }
    auto status = sGnssCallback->gnssLocationCb(location);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke gnssLocationCb", __func__);
    }
    return;
}

void Gnss::reportSvStatus() const {
    if (mIsSvStatusActive) {
        auto svStatus = filterBlocklistedSatellites(Utils::getMockSvInfoList());
        reportSvStatus(svStatus);
    }
}

void Gnss::reportSvStatus(const std::vector<GnssSvInfo>& svInfoList) const {
    std::unique_lock<std::mutex> lock(mMutex);
    if (sGnssCallback == nullptr) {
        ALOGE("%s: sGnssCallback is null.", __func__);
        return;
    }
    auto status = sGnssCallback->gnssSvStatusCb(svInfoList);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
}

std::vector<GnssSvInfo> Gnss::filterBlocklistedSatellites(
        std::vector<GnssSvInfo> gnssSvInfoList) const {
    for (uint32_t i = 0; i < gnssSvInfoList.size(); i++) {
        if (mGnssConfiguration->isBlocklisted(gnssSvInfoList[i])) {
            gnssSvInfoList[i].svFlag &= ~(uint32_t)IGnssCallback::GnssSvFlags::USED_IN_FIX;
        }
    }
    return gnssSvInfoList;
}

void Gnss::reportGnssStatusValue(const IGnssCallback::GnssStatusValue gnssStatusValue) const {
    std::unique_lock<std::mutex> lock(mMutex);
    if (sGnssCallback == nullptr) {
        ALOGE("%s: sGnssCallback is null.", __func__);
        return;
    }
    auto status = sGnssCallback->gnssStatusCb(gnssStatusValue);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke gnssStatusCb", __func__);
    }
}

void Gnss::reportNmea() const {
    if (mIsNmeaActive) {
        std::unique_lock<std::mutex> lock(mMutex);
        if (sGnssCallback == nullptr) {
            ALOGE("%s: sGnssCallback is null.", __func__);
            return;
        }
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        auto status = sGnssCallback->gnssNmeaCb(now, "$TEST,0,1,2,3,4,5");
        if (!status.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

ScopedAStatus Gnss::startSvStatus() {
    ALOGD("startSvStatus");
    mIsSvStatusActive = true;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stopSvStatus() {
    ALOGD("stopSvStatus");
    mIsSvStatusActive = false;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::startNmea() {
    ALOGD("startNmea");
    mIsNmeaActive = true;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::stopNmea() {
    ALOGD("stopNmea");
    mIsNmeaActive = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionAGnss(std::shared_ptr<IAGnss>* iAGnss) {
    ALOGD("Gnss::getExtensionAGnss");
    *iAGnss = SharedRefBase::make<AGnss>();
    return ndk::ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectTime(int64_t timeMs, int64_t timeReferenceMs, int uncertaintyMs) {
    ALOGD("injectTime. timeMs:%" PRId64 ", timeReferenceMs:%" PRId64 ", uncertaintyMs:%d", timeMs,
          timeReferenceMs, uncertaintyMs);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* iAGnssRil) {
    ALOGD("Gnss::getExtensionAGnssRil");
    *iAGnssRil = SharedRefBase::make<AGnssRil>();
    return ndk::ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectLocation(const GnssLocation& location) {
    ALOGD("injectLocation. lat:%lf, lng:%lf, acc:%f", location.latitudeDegrees,
          location.longitudeDegrees, location.horizontalAccuracyMeters);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectBestLocation(const GnssLocation& location) {
    ALOGD("injectBestLocation. lat:%lf, lng:%lf, acc:%f", location.latitudeDegrees,
          location.longitudeDegrees, location.horizontalAccuracyMeters);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::deleteAidingData(GnssAidingData aidingDataFlags) {
    ALOGD("deleteAidingData. flags:%d", (int)aidingDataFlags);
    mFirstFixReceived = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::setPositionMode(const PositionModeOptions& options) {
    ALOGD("setPositionMode. minIntervalMs:%d, lowPowerMode:%d", options.minIntervalMs,
          (int)options.lowPowerMode);
    mMinIntervalMs = std::max(1000, options.minIntervalMs);
    mGnssMeasurementInterface->setLocationInterval(mMinIntervalMs);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionPsds(std::shared_ptr<IGnssPsds>* iGnssPsds) {
    ALOGD("getExtensionPsds");
    *iGnssPsds = SharedRefBase::make<GnssPsds>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssConfiguration(
        std::shared_ptr<IGnssConfiguration>* iGnssConfiguration) {
    ALOGD("getExtensionGnssConfiguration");
    if (mGnssConfiguration == nullptr) {
        mGnssConfiguration = SharedRefBase::make<GnssConfiguration>();
    }
    *iGnssConfiguration = mGnssConfiguration;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssPowerIndication(
        std::shared_ptr<IGnssPowerIndication>* iGnssPowerIndication) {
    ALOGD("getExtensionGnssPowerIndication");
    if (mGnssPowerIndication == nullptr) {
        mGnssPowerIndication = SharedRefBase::make<GnssPowerIndication>();
    }

    *iGnssPowerIndication = mGnssPowerIndication;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssMeasurement(
        std::shared_ptr<IGnssMeasurementInterface>* iGnssMeasurement) {
    ALOGD("getExtensionGnssMeasurement");
    if (mGnssMeasurementInterface == nullptr) {
        mGnssMeasurementInterface = SharedRefBase::make<GnssMeasurementInterface>();
        mGnssMeasurementInterface->setGnssInterface(static_cast<std::shared_ptr<Gnss>>(this));
    }
    *iGnssMeasurement = mGnssMeasurementInterface;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssBatching(std::shared_ptr<IGnssBatching>* iGnssBatching) {
    ALOGD("getExtensionGnssBatching");

    *iGnssBatching = SharedRefBase::make<GnssBatching>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssGeofence(std::shared_ptr<IGnssGeofence>* iGnssGeofence) {
    ALOGD("getExtensionGnssGeofence");

    *iGnssGeofence = SharedRefBase::make<GnssGeofence>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssNavigationMessage(
        std::shared_ptr<IGnssNavigationMessageInterface>* iGnssNavigationMessage) {
    ALOGD("getExtensionGnssNavigationMessage");

    *iGnssNavigationMessage = SharedRefBase::make<GnssNavigationMessageInterface>();
    return ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* iGnssDebug) {
    ALOGD("Gnss::getExtensionGnssDebug");

    *iGnssDebug = SharedRefBase::make<GnssDebug>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssVisibilityControl(
        std::shared_ptr<visibility_control::IGnssVisibilityControl>* iGnssVisibilityControl) {
    ALOGD("Gnss::getExtensionGnssVisibilityControl");

    *iGnssVisibilityControl = SharedRefBase::make<visibility_control::GnssVisibilityControl>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssAntennaInfo(
        std::shared_ptr<IGnssAntennaInfo>* iGnssAntennaInfo) {
    ALOGD("Gnss::getExtensionGnssAntennaInfo");

    *iGnssAntennaInfo = SharedRefBase::make<GnssAntennaInfo>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionMeasurementCorrections(
        std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>*
                iMeasurementCorrections) {
    ALOGD("Gnss::getExtensionMeasurementCorrections");

    *iMeasurementCorrections =
            SharedRefBase::make<measurement_corrections::MeasurementCorrectionsInterface>();
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
