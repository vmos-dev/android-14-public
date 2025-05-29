/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "GnssHAL_GnssInterface"

#include <Gnss.h>
#include <GnssUtils.h>
#include <utils/SystemClock.h>

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

using ::android::hardware::gnss::measurement_corrections::V1_0::implementation::
        GnssMeasurementCorrections;
using ::android::hardware::gnss::visibility_control::V1_0::implementation::GnssVisibilityControl;

sp<V2_0::IGnssCallback> Gnss::sGnssCallback_2_0 = nullptr;
sp<V1_1::IGnssCallback> Gnss::sGnssCallback_1_1 = nullptr;


std::vector<std::unique_ptr<ThreadFuncArgs>> Gnss::sThreadFuncArgsList;
sp<V1_0::IGnssCallback> Gnss::sGnssCbIface = nullptr;
bool Gnss::sInterfaceExists = false;
bool Gnss::sWakelockHeldGnss = false;
bool Gnss::sWakelockHeldFused = false;

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

uint32_t Gnss::sCapabilitiesCached = 0;
uint16_t Gnss::sYearOfHwCached = 0;

Gnss::Gnss(gps_device_t* gnssDevice) :
        mDeathRecipient(new GnssHidlDeathRecipient(this)) {
    /* Error out if an instance of the interface already exists. */
    LOG_ALWAYS_FATAL_IF(sInterfaceExists);
    sInterfaceExists = true;

    if (gnssDevice == nullptr) {
        ALOGE("%s: Invalid device_t handle", __func__);
        return;
    }

    mGnssIface = gnssDevice->get_gps_interface(gnssDevice);
}

Gnss::~Gnss() {
    sInterfaceExists = false;
    sThreadFuncArgsList.clear();
}

void Gnss::locationCb(GpsLocation* location) {
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (location == nullptr) {
        ALOGE("%s: Invalid location from GNSS HAL", __func__);
        return;
    }

    android::hardware::gnss::V1_0::GnssLocation gnssLocation_1_0 = V1_0::implementation::convertToGnssLocation(location);

    if(sGnssCallback_2_0 != nullptr) {
        //ALOGE("%s: ceshi is here0", __func__);
            const ElapsedRealtime timestamp = {
            .flags = ElapsedRealtimeFlags::HAS_TIMESTAMP_NS |
                     ElapsedRealtimeFlags::HAS_TIME_UNCERTAINTY_NS,
            .timestampNs = static_cast<uint64_t>(::android::elapsedRealtimeNano()),
            .timeUncertaintyNs = 1000000};

            V2_0::GnssLocation gnssLocation = {.v1_0 = gnssLocation_1_0, .elapsedRealtime = timestamp};

        auto ret = sGnssCallback_2_0->gnssLocationCb_2_0(gnssLocation);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        //ALOGE("%s: ceshi is here1", __func__);
        auto ret = sGnssCallback_1_1->gnssLocationCb(gnssLocation_1_0);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        //ALOGE("%s: ceshi is here2", __func__);
        auto ret = sGnssCbIface->gnssLocationCb(gnssLocation_1_0);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }

}

void Gnss::statusCb(GpsStatus* gnssStatus) {
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (gnssStatus == nullptr) {
        ALOGE("%s: Invalid GpsStatus from GNSS HAL", __func__);
        return;
    }

    IGnssCallback::GnssStatusValue status = static_cast<IGnssCallback::GnssStatusValue>(gnssStatus->status);
    if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssStatusCb(status);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssStatusCb(status);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssStatusCb(status);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

void Gnss::gnssSvStatusCb(GnssSvStatus* status) {
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (status == nullptr) {
        ALOGE("Invalid status from GNSS HAL %s", __func__);
        return;
    }

    IGnssCallback::GnssSvStatus svStatus;
    svStatus.numSvs = status->num_svs;

    if (svStatus.numSvs > static_cast<uint32_t>(V1_0::GnssMax::SVS_COUNT)) {
        ALOGW("Too many satellites %u. Clamps to %d.", svStatus.numSvs, V1_0::GnssMax::SVS_COUNT);
        svStatus.numSvs = static_cast<uint32_t>(V1_0::GnssMax::SVS_COUNT);
    }

    for (size_t i = 0; i < svStatus.numSvs; i++) {
        auto svInfo = status->gnss_sv_list[i];
        V1_0::IGnssCallback::GnssSvInfo gnssSvInfo = {
            .svid = svInfo.svid,
            .constellation = static_cast<
                android::hardware::gnss::V1_0::GnssConstellationType>(
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
            .carrierFrequencyHz = 1111*1e6F};
        svStatus.gnssSvList[i] = gnssSvInfo;
    }

    if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssSvStatusCb(svStatus);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssSvStatusCb(svStatus);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssSvStatusCb(svStatus);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
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
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (svInfo == nullptr) {
        ALOGE("Invalid status from GNSS HAL %s", __func__);
        return;
    }

    IGnssCallback::GnssSvStatus svStatus;
    svStatus.numSvs = svInfo->num_svs;
    /*
     * Clamp the list size since GnssSvStatus can support a maximum of
     * GnssMax::SVS_COUNT entries.
     */
    if (svStatus.numSvs > static_cast<uint32_t>(V1_0::GnssMax::SVS_COUNT)) {
        ALOGW("Too many satellites %u. Clamps to %d.", svStatus.numSvs, V1_0::GnssMax::SVS_COUNT);
        svStatus.numSvs = static_cast<uint32_t>(V1_0::GnssMax::SVS_COUNT);
    }

    uint32_t ephemerisMask = svInfo->ephemeris_mask;
    uint32_t almanacMask = svInfo->almanac_mask;
    uint32_t usedInFixMask = svInfo->used_in_fix_mask;
    /*
     * Conversion from GpsSvInfo to IGnssCallback::GnssSvInfo happens below.
     */
    for (size_t i = 0; i < svStatus.numSvs; i++) {
        V1_0::IGnssCallback::GnssSvInfo& info = svStatus.gnssSvList[i];
        info.svid = svInfo->sv_list[i].prn;
        if (info.svid >= 1 && info.svid <= 32) {
            info.constellation = V1_0::GnssConstellationType::GPS;
        } else if (info.svid > GLONASS_SVID_OFFSET &&
                   info.svid <= GLONASS_SVID_OFFSET + GLONASS_SVID_COUNT) {
            info.constellation = V1_0::GnssConstellationType::GLONASS;
            info.svid -= GLONASS_SVID_OFFSET;
        } else if (info.svid > BEIDOU_SVID_OFFSET &&
                 info.svid <= BEIDOU_SVID_OFFSET + BEIDOU_SVID_COUNT) {
            info.constellation = V1_0::GnssConstellationType::BEIDOU;
            info.svid -= BEIDOU_SVID_OFFSET;
        } else if (info.svid >= SBAS_SVID_MIN && info.svid <= SBAS_SVID_MAX) {
            info.constellation = V1_0::GnssConstellationType::SBAS;
            info.svid += SBAS_SVID_ADD;
        } else if (info.svid >= QZSS_SVID_MIN && info.svid <= QZSS_SVID_MAX) {
            info.constellation = V1_0::GnssConstellationType::QZSS;
        } else {
            ALOGD("Unknown constellation type with Svid = %d.", info.svid);
            info.constellation = V1_0::GnssConstellationType::UNKNOWN;
        }

        info.cN0Dbhz = svInfo->sv_list[i].snr;
        info.elevationDegrees = svInfo->sv_list[i].elevation;
        info.azimuthDegrees = svInfo->sv_list[i].azimuth;
        // TODO: b/31702236
        info.svFlag = static_cast<uint8_t>(V1_0::IGnssCallback::GnssSvFlags::NONE);

        /*
         * Only GPS info is valid for these fields, as these masks are just 32
         * bits, by GPS prn.
         */
        if (info.constellation == V1_0::GnssConstellationType::GPS) {
            int32_t svidMask = (1 << (info.svid - 1));
            if ((ephemerisMask & svidMask) != 0) {
                info.svFlag |= V1_0::IGnssCallback::GnssSvFlags::HAS_EPHEMERIS_DATA;
            }
            if ((almanacMask & svidMask) != 0) {
                info.svFlag |= V1_0::IGnssCallback::GnssSvFlags::HAS_ALMANAC_DATA;
            }
            if ((usedInFixMask & svidMask) != 0) {
                info.svFlag |= V1_0::IGnssCallback::GnssSvFlags::USED_IN_FIX;
            }
        }
    }

    if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssSvStatusCb(svStatus);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssSvStatusCb(svStatus);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssSvStatusCb(svStatus);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

void Gnss::nmeaCb(GpsUtcTime timestamp, const char* nmea, int length) {
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    android::hardware::hidl_string nmeaString;
    nmeaString.setToExternal(nmea, length);
     if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssNmeaCb(timestamp, nmeaString);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssNmeaCb(timestamp, nmeaString);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssNmeaCb(timestamp, nmeaString);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

void Gnss::setCapabilitiesCb(uint32_t capabilities) {
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssSetCapabilitesCb(capabilities);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssSetCapabilitesCb(capabilities);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssSetCapabilitesCb(capabilities);
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

    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if (sWakelockHeldGnss || sWakelockHeldFused) {
        if (!sWakelockHeld) {
            ALOGI("%s: GNSS HAL Wakelock acquired due to gps: %d, fused: %d", __func__,
                    sWakelockHeldGnss, sWakelockHeldFused);
            sWakelockHeld = true;
            if(sGnssCallback_2_0 != nullptr) {
                    auto ret = sGnssCallback_2_0->gnssAcquireWakelockCb();
                   if (!ret.isOk()) {
                     ALOGE("%s: Unable to invoke callback", __func__);
                   }
                  } else if(sGnssCallback_1_1 != nullptr) {
                    auto ret = sGnssCallback_1_1->gnssAcquireWakelockCb();
                   if (!ret.isOk()) {
                    ALOGE("%s: Unable to invoke callback", __func__);
                   }
                } else if(sGnssCbIface != nullptr) {
                  auto ret = sGnssCbIface->gnssAcquireWakelockCb();
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
        if(sGnssCallback_2_0 != nullptr) {
           auto ret = sGnssCallback_2_0->gnssReleaseWakelockCb();
            if (!ret.isOk()) {
              ALOGE("%s: Unable to invoke callback", __func__);
           }
         } else if(sGnssCallback_1_1 != nullptr) {
           auto ret = sGnssCallback_1_1->gnssReleaseWakelockCb();
           if (!ret.isOk()) {
              ALOGE("%s: Unable to invoke callback", __func__);
           }
           } else if(sGnssCbIface != nullptr) {
            auto ret = sGnssCbIface->gnssReleaseWakelockCb();
           if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
           }
       }
    }
}

void Gnss::requestUtcTimeCb() {
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
        ALOGE("%s: GNSS Callback Interface configured incorrectly", __func__);
        return;
    }

    if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssRequestTimeCb();
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssRequestTimeCb();
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssRequestTimeCb();
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

pthread_t Gnss::createThreadCb(const char* name, void (*start)(void*), void* arg) {
    return createPthread(name, start, arg, &sThreadFuncArgsList);
}

void Gnss::setSystemInfoCb(const LegacyGnssSystemInfo* info) {
        
    if (sGnssCbIface == nullptr && sGnssCallback_1_1 == nullptr && sGnssCallback_2_0 == nullptr) {
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

    if(sGnssCallback_2_0 != nullptr) {
        auto ret = sGnssCallback_2_0->gnssSetSystemInfoCb(gnssInfo);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCallback_1_1 != nullptr) {
        auto ret = sGnssCallback_1_1->gnssSetSystemInfoCb(gnssInfo);
        if (!ret.isOk()) {
             ALOGE("%s: Unable to invoke callback", __func__);
        }
     } else if(sGnssCbIface != nullptr) {
        auto ret = sGnssCbIface->gnssSetSystemInfoCb(gnssInfo);
        if (!ret.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
    // Save for reconnection when some legacy hal's don't resend this info
    sYearOfHwCached = info->year_of_hw;
}


// Methods from ::android::hardware::gnss::V1_0::IGnss follow.
Return<bool> Gnss::setCallback(const sp<V1_0::IGnssCallback>& callback)  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    if (callback == nullptr)  {
        ALOGE("%s: Null callback ignored", __func__);
        return false;
    }



    if (sGnssCallback_1_1 != nullptr) {
        sGnssCallback_1_1->unlinkToDeath(mDeathRecipient);
        sGnssCallback_1_1 = nullptr;
    }
    if (sGnssCallback_2_0 != nullptr) {
        sGnssCallback_2_0->unlinkToDeath(mDeathRecipient);
         sGnssCallback_2_0 = nullptr;
    }

    if (sGnssCbIface != nullptr) {
        sGnssCbIface ->unlinkToDeath(mDeathRecipient);
    }
    sGnssCbIface  = callback;
    if (sGnssCbIface  != nullptr) {
        sGnssCbIface->linkToDeath(mDeathRecipient, 0 /*cookie*/);
    }


    //if (sGnssCbIface != NULL) {
    //    ALOGW("%s called more than once. Unexpected unless test.", __func__);
    //    sGnssCbIface->unlinkToDeath(mDeathRecipient);
    //}

    //sGnssCbIface = callback;
     //callback->linkToDeath(mDeathRecipient, 0 /*cookie*/);

    // If this was received in the past, send it up again to refresh caller.
    // mGnssIface will override after init() is called below, if needed
    // (though it's unlikely the gps.h capabilities or system info will change.)
    if (sCapabilitiesCached != 0) {
        setCapabilitiesCb(sCapabilitiesCached);
    }
    if (sYearOfHwCached != 0) {
        LegacyGnssSystemInfo info;
        info.year_of_hw = sYearOfHwCached;
        setSystemInfoCb(&info);
    }

    return (mGnssIface->init(&sGnssCb) == 0);
}

Return<bool> Gnss::start()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    return (mGnssIface->start() == 0);
}

Return<bool> Gnss::stop()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    return (mGnssIface->stop() == 0);
}

Return<void> Gnss::cleanup()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
    } else {
        mGnssIface->cleanup();
    }
    return Void();
}

Return<bool> Gnss::injectLocation(double latitudeDegrees,
                                  double longitudeDegrees,
                                  float accuracyMeters)  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    return (mGnssIface->inject_location(latitudeDegrees, longitudeDegrees, accuracyMeters) == 0);
}

Return<bool> Gnss::injectTime(int64_t timeMs, int64_t timeReferenceMs,
                              int32_t uncertaintyMs) {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    return (mGnssIface->inject_time(timeMs, timeReferenceMs, uncertaintyMs) == 0);
}

Return<void> Gnss::deleteAidingData(V1_0::IGnss::GnssAidingData aidingDataFlags)  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
    } else {
        mGnssIface->delete_aiding_data(static_cast<GpsAidingData>(aidingDataFlags));
    }
    return Void();
}

Return<bool> Gnss::setPositionMode(V1_0::IGnss::GnssPositionMode mode,
                                   IGnss::GnssPositionRecurrence recurrence,
                                   uint32_t minIntervalMs,
                                   uint32_t preferredAccuracyMeters,
                                   uint32_t preferredTimeMs)  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    return (mGnssIface->set_position_mode(static_cast<GpsPositionMode>(mode),
                                          static_cast<GpsPositionRecurrence>(recurrence),
                                          minIntervalMs,
                                          preferredAccuracyMeters,
                                          preferredTimeMs) == 0);
}

Return<sp<V1_0::IAGnssRil>> Gnss::getExtensionAGnssRil()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssRil == nullptr) {
        const AGpsRilInterface* agpsRilIface = static_cast<const AGpsRilInterface*>(
                mGnssIface->get_extension(AGPS_RIL_INTERFACE));
        if (agpsRilIface == nullptr) {
            ALOGI("%s: GnssRil interface not implemented by HAL", __func__);
        } else {
            mGnssRil = new AGnssRil(agpsRilIface);
        }
    }
    return mGnssRil;
}

Return<sp<V1_0::IGnssConfiguration>> Gnss::getExtensionGnssConfiguration()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssConfig == nullptr) {
        const GnssConfigurationInterface* gnssConfigIface =
                static_cast<const GnssConfigurationInterface*>(
                        mGnssIface->get_extension(GNSS_CONFIGURATION_INTERFACE));

        if (gnssConfigIface == nullptr) {
            ALOGE("%s: GnssConfiguration interface not implemented by HAL", __func__);
        } else {
            mGnssConfig = new GnssConfiguration(gnssConfigIface);
        }
    }
    return mGnssConfig;
}

Return<sp<V1_0::IGnssGeofencing>> Gnss::getExtensionGnssGeofencing()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssGeofencingIface == nullptr) {
        const GpsGeofencingInterface* gpsGeofencingIface =
                static_cast<const GpsGeofencingInterface*>(
                        mGnssIface->get_extension(GPS_GEOFENCING_INTERFACE));

        if (gpsGeofencingIface == nullptr) {
            ALOGE("%s: GnssGeofencing interface not implemented by HAL", __func__);
        } else {
            mGnssGeofencingIface = new GnssGeofencing(gpsGeofencingIface);
        }
    }

    return mGnssGeofencingIface;
}

Return<sp<V1_0::IAGnss>> Gnss::getExtensionAGnss()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mAGnssIface == nullptr) {
        const AGpsInterface* agpsIface = static_cast<const AGpsInterface*>(
                mGnssIface->get_extension(AGPS_INTERFACE));
        if (agpsIface == nullptr) {
            ALOGE("%s: AGnss interface not implemented by HAL", __func__);
        } else {
            mAGnssIface = new AGnss(agpsIface);
        }
    }
    return mAGnssIface;
}

Return<sp<V1_0::IGnssNi>> Gnss::getExtensionGnssNi()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }
    return nullptr;
/*
    if (mGnssNi == nullptr) {
        const GpsNiInterface* gpsNiIface = static_cast<const GpsNiInterface*>(
                mGnssIface->get_extension(GPS_NI_INTERFACE));
        if (gpsNiIface == nullptr) {
            ALOGI("%s: GnssNi interface not implemented by HAL", __func__);
        } else {
            mGnssNi = new GnssNi(gpsNiIface);
        }
    }
    return mGnssNi;
*/
}


Return<sp<V1_0::IGnssMeasurement>> Gnss::getExtensionGnssMeasurement() {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssMeasurement == nullptr) {
        const GpsMeasurementInterface* gpsMeasurementIface =
                static_cast<const GpsMeasurementInterface*>(
                        mGnssIface->get_extension(GPS_MEASUREMENT_INTERFACE));

        if (gpsMeasurementIface == nullptr) {
            ALOGE("%s: GnssMeasurement interface not implemented by HAL", __func__);
        } else {
            mGnssMeasurement = new GnssMeasurement(gpsMeasurementIface);
        }
    }
    return mGnssMeasurement;
}

Return<sp<V1_0::IGnssXtra>> Gnss::getExtensionXtra()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssXtraIface == nullptr) {
        const GpsXtraInterface* gpsXtraIface = static_cast<const GpsXtraInterface*>(
                mGnssIface->get_extension(GPS_XTRA_INTERFACE));

        if (gpsXtraIface == nullptr) {
            ALOGI("%s: GnssXtra interface not implemented by HAL", __func__);
        } else {
            mGnssXtraIface = new GnssXtra(gpsXtraIface);
        }
    }

    return mGnssXtraIface;
}


Return<sp<V1_0::IGnssNavigationMessage>> Gnss::getExtensionGnssNavigationMessage() {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssNavigationMessage == nullptr) {
        const GpsNavigationMessageInterface* gpsNavigationMessageIface =
                static_cast<const GpsNavigationMessageInterface*>(
                        mGnssIface->get_extension(GPS_NAVIGATION_MESSAGE_INTERFACE));

        if (gpsNavigationMessageIface == nullptr) {
            ALOGI("%s: GnssNavigationMessage interface not implemented by HAL", __func__);
        } else {
            mGnssNavigationMessage = new GnssNavigationMessage(gpsNavigationMessageIface);
        }
    }

    return mGnssNavigationMessage;
}


Return<sp<V1_0::IGnssDebug>> Gnss::getExtensionGnssDebug()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssDebug == nullptr) {
        const GpsDebugInterface* gpsDebugIface = static_cast<const GpsDebugInterface*>(
                mGnssIface->get_extension(GPS_DEBUG_INTERFACE));

        if (gpsDebugIface == nullptr) {
            ALOGI("%s: GnssDebug interface not implemented by HAL", __func__);
        } else {
            mGnssDebug = new GnssDebug(gpsDebugIface);
        }
    }

    return mGnssDebug;
}


Return<sp<V1_0::IGnssBatching>> Gnss::getExtensionGnssBatching()  {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssBatching == nullptr) {
        hw_module_t* module;
        const FlpLocationInterface* flpLocationIface = nullptr;
        int err = hw_get_module(FUSED_LOCATION_HARDWARE_MODULE_ID, (hw_module_t const**)&module);

        if (err != 0) {
            ALOGE("gnss flp hw_get_module failed: %d", err);
        } else if (module == nullptr) {
            ALOGE("Fused Location hw_get_module returned null module");
        } else if (module->methods == nullptr) {
            ALOGE("Fused Location hw_get_module returned null methods");
        } else {
            hw_device_t* device;
            err = module->methods->open(module, FUSED_LOCATION_HARDWARE_MODULE_ID, &device);
            if (err != 0) {
                ALOGE("flpDevice open failed: %d", err);
            } else {
                flp_device_t * flpDevice = reinterpret_cast<flp_device_t*>(device);
                flpLocationIface = flpDevice->get_flp_interface(flpDevice);
            }
        }

        if (flpLocationIface == nullptr) {
            ALOGE("%s: GnssBatching interface is not implemented by HAL", __func__);
        } else {
            mGnssBatching = new GnssBatching(flpLocationIface);
        }
    }
    return mGnssBatching;
}


Return<sp<measurement_corrections::V1_0::IMeasurementCorrections>>
Gnss::getExtensionMeasurementCorrections() {
    ALOGD("Gnss::getExtensionMeasurementCorrections");
    return new GnssMeasurementCorrections();
}

Return<sp<visibility_control::V1_0::IGnssVisibilityControl>> Gnss::getExtensionVisibilityControl() {
    ALOGD("Gnss::getExtensionVisibilityControl");
    return new GnssVisibilityControl();
}


// Methods from V1_1::IGnss follow.
Return<bool> Gnss::setCallback_1_1(const sp<V1_1::IGnssCallback>& callback) {
    ALOGD("Gnss::setCallback_1_1");

    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    if (callback == nullptr) {
        ALOGE("%s: Null callback ignored", __func__);
        return false;
    }

    if (sGnssCbIface != nullptr) {
        sGnssCbIface->unlinkToDeath(mDeathRecipient);
        sGnssCbIface = nullptr;
    }
    if (sGnssCallback_2_0 != nullptr) {
        sGnssCallback_2_0->unlinkToDeath(mDeathRecipient);
         sGnssCallback_2_0 = nullptr;
    }

    if (sGnssCallback_1_1 != nullptr) {
        sGnssCallback_1_1->unlinkToDeath(mDeathRecipient);
    }
    sGnssCallback_1_1 = callback;
    if (sGnssCallback_1_1 != nullptr) {
        sGnssCallback_1_1->linkToDeath(mDeathRecipient, 0 /*cookie*/);
    }

    auto gnssName = "Google Mock GNSS Implementation v2.0";
    auto ret = sGnssCallback_1_1->gnssNameCb(gnssName);
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }

    return (mGnssIface->init(&sGnssCb) == 0);
}

Return<bool> Gnss::setPositionMode_1_1(V1_0::IGnss::GnssPositionMode,
                                       V1_0::IGnss::GnssPositionRecurrence, uint32_t, uint32_t,
                                       uint32_t, bool) {
    return true;
}

Return<sp<V1_1::IGnssConfiguration>> Gnss::getExtensionGnssConfiguration_1_1() {
    // TODO(b/124012850): Implement function.
    return sp<V1_1::IGnssConfiguration>{};
}

Return<sp<V1_1::IGnssMeasurement>> Gnss::getExtensionGnssMeasurement_1_1() {
    ALOGD("Gnss::getExtensionGnssMeasurement_1_1");

    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssMeasurement_1_1 == nullptr) {
        const GpsMeasurementInterface* gpsMeasurementIface =
                static_cast<const GpsMeasurementInterface*>(
                        mGnssIface->get_extension(GPS_MEASUREMENT_INTERFACE));

        if (gpsMeasurementIface == nullptr) {
            ALOGE("%s: GnssMeasurement interface not implemented by HAL", __func__);
        } else {
            mGnssMeasurement_1_1 = new GnssMeasurement(gpsMeasurementIface);
        }
    }

    return mGnssMeasurement_1_1;
}

Return<bool> Gnss::injectBestLocation(const V1_0::GnssLocation&) {
    // TODO(b/124012850): Implement function.
    //return bool{};
        return true;
}

Return<bool> Gnss::setCallback_2_0(const sp<V2_0::IGnssCallback>& callback) {
    ALOGD("Gnss::setCallback_2_0");
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return false;
    }

    if (callback == nullptr)  {
        ALOGE("%s: Null callback ignored", __func__);
        return false;
    }

    //if (sGnssCbIface != NULL) {
    //    ALOGW("%s called more than once. Unexpected unless test.", __func__);
    //    sGnssCbIface->unlinkToDeath(mDeathRecipient);
    //}

    if (sGnssCbIface != nullptr) {
        sGnssCbIface->unlinkToDeath(mDeathRecipient);
        sGnssCbIface = nullptr;
    }
    if (sGnssCallback_1_1 != nullptr) {
        sGnssCallback_1_1->unlinkToDeath(mDeathRecipient);
         sGnssCallback_1_1 = nullptr;
    }

    if (sGnssCallback_2_0 != nullptr) {
        sGnssCallback_2_0->unlinkToDeath(mDeathRecipient);
    }
    sGnssCallback_2_0 = callback;
    if (sGnssCallback_2_0 != nullptr) {
        sGnssCallback_2_0->linkToDeath(mDeathRecipient, 0 /*cookie*/);
    }


    //sGnssCbIface = callback;
    //sGnssCallback_2_0 = callback;
    auto gnssName = "Google Mock GNSS Implementation v2.0";
    auto ret = sGnssCallback_2_0->gnssNameCb(gnssName);
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }

    //callback->linkToDeath(mDeathRecipient, 0 /*cookie*/);

    return (mGnssIface->init(&sGnssCb) == 0);
}


// Methods from V2_0::IGnss follow.
Return<sp<V2_0::IGnssConfiguration>> Gnss::getExtensionGnssConfiguration_2_0() {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssConfig_2_0  == nullptr) {
        const GnssConfigurationInterface* gnssConfigIface =
        static_cast<const GnssConfigurationInterface*>(mGnssIface->get_extension(GNSS_CONFIGURATION_INTERFACE));

        if (gnssConfigIface == nullptr) {
            ALOGE("%s: GnssConfiguration interface not implemented by HAL", __func__);
                        mGnssConfig_2_0 = new GnssConfiguration();
        } else {
            mGnssConfig_2_0 = new GnssConfiguration(gnssConfigIface);
        }
    }
    return mGnssConfig_2_0;

}

Return<sp<V2_0::IGnssDebug>> Gnss::getExtensionGnssDebug_2_0() {
    // TODO(b/124012850): Implement function.
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssDebug_2_0 == nullptr) {
        const GpsDebugInterface* gpsDebugIface = static_cast<const GpsDebugInterface*>(
                mGnssIface->get_extension(GPS_DEBUG_INTERFACE));

        if (gpsDebugIface == nullptr) {
            ALOGI("%s: GnssDebug interface not implemented by HAL", __func__);
        } else {
            mGnssDebug_2_0 = new GnssDebug(gpsDebugIface);
        }
    }

    return mGnssDebug_2_0;
}

Return<sp<V2_0::IAGnss>> Gnss::getExtensionAGnss_2_0() {
     if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mAGnssIface_2_0 == nullptr) {
        const AGpsInterface* agpsIface = static_cast<const AGpsInterface*>(
                mGnssIface->get_extension(AGPS_INTERFACE));
        if (agpsIface == nullptr) {
            ALOGE("%s: AGnss interface not implemented by HAL", __func__);
        } else {
            mAGnssIface_2_0 = new AGnss(agpsIface);
        }
    }
    return mAGnssIface_2_0;

}

Return<sp<V2_0::IAGnssRil>> Gnss::getExtensionAGnssRil_2_0() {
     if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssRil_2_0 == nullptr) {
        const AGpsRilInterface* agpsRilIface = static_cast<const AGpsRilInterface*>(
                mGnssIface->get_extension(AGPS_RIL_INTERFACE));
        if (agpsRilIface == nullptr) {
            ALOGI("%s: GnssRil interface not implemented by HAL", __func__);
        } else {
            mGnssRil_2_0 = new AGnssRil(agpsRilIface);
        }
    }
    return mGnssRil_2_0;

}

Return<sp<V2_0::IGnssMeasurement>> Gnss::getExtensionGnssMeasurement_2_0() {
    ALOGD("Gnss::getExtensionGnssMeasurement_2_0");
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssMeasurement_2_0 == nullptr) {
        const GpsMeasurementInterface* gpsMeasurementIface =
                static_cast<const GpsMeasurementInterface*>(
                        mGnssIface->get_extension(GPS_MEASUREMENT_INTERFACE));

        if (gpsMeasurementIface == nullptr) {
            ALOGE("%s: GnssMeasurement interface not implemented by HAL", __func__);
        } else {
            mGnssMeasurement_2_0 = new GnssMeasurement(gpsMeasurementIface);
        }
    }

    return mGnssMeasurement_2_0;

}

Return<sp<V2_0::IGnssBatching>> Gnss::getExtensionGnssBatching_2_0() {
    if (mGnssIface == nullptr) {
        ALOGE("%s: Gnss interface is unavailable", __func__);
        return nullptr;
    }

    if (mGnssBatching_2_0 == nullptr) {
        hw_module_t* module;
        const FlpLocationInterface* flpLocationIface = nullptr;
        int err = hw_get_module(FUSED_LOCATION_HARDWARE_MODULE_ID, (hw_module_t const**)&module);

        if (err != 0) {
            ALOGE("gnss flp hw_get_module failed: %d", err);
        } else if (module == nullptr) {
            ALOGE("Fused Location hw_get_module returned null module");
        } else if (module->methods == nullptr) {
            ALOGE("Fused Location hw_get_module returned null methods");
        } else {
            hw_device_t* device;
            err = module->methods->open(module, FUSED_LOCATION_HARDWARE_MODULE_ID, &device);
            if (err != 0) {
                ALOGE("flpDevice open failed: %d", err);
            } else {
                flp_device_t * flpDevice = reinterpret_cast<flp_device_t*>(device);
                flpLocationIface = flpDevice->get_flp_interface(flpDevice);
            }
        }

        if (flpLocationIface == nullptr) {
            ALOGE("%s: GnssBatching interface is not implemented by HAL", __func__);
        } else {
            mGnssBatching_2_0 = new GnssBatching(flpLocationIface);
        }
    }
    return mGnssBatching_2_0;

}

Return<bool> Gnss::injectBestLocation_2_0(const V2_0::GnssLocation&) {
    // TODO(b/124012850): Implement function.
    //return bool{};
    return true;
}


void Gnss::handleHidlDeath() {
    ALOGW("GNSS service noticed HIDL death. Stopping all GNSS operations.");

    // commands down to the HAL implementation
    stop(); // stop ongoing GPS tracking
    if (mGnssMeasurement != nullptr) {
        mGnssMeasurement->close();
    }


    if (mGnssNavigationMessage != nullptr) {
        mGnssNavigationMessage->close();
    }

    if (mGnssBatching != nullptr) {
        mGnssBatching->stop();
        mGnssBatching->cleanup();
    }

    cleanup();

    /*
     * This has died, so close it off in case (race condition) callbacks happen
     * before HAL processes above messages.
     */
    sGnssCbIface = nullptr;
}

IGnss* HIDL_FETCH_IGnss(const char* /* hal */) {
    hw_module_t* module;
    IGnss* iface = nullptr;
    int err = hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module, GPS_HARDWARE_MODULE_ID, &device);
        if (err == 0) {
            iface = new Gnss(reinterpret_cast<gps_device_t*>(device));
        } else {
            ALOGE("gnssDevice open %s failed: %d", GPS_HARDWARE_MODULE_ID, err);
        }
    } else {
      ALOGE("gnss hw_get_module %s failed: %d", GPS_HARDWARE_MODULE_ID, err);
    }
    return iface;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
