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

#define LOG_TAG "GnssHAL_GnssMeasurementInterface"

#include "GnssMeasurement.h"


namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

sp<V1_0::IGnssMeasurementCallback> GnssMeasurement::sGnssMeasureCbIface = nullptr;
sp<V1_1::IGnssMeasurementCallback> GnssMeasurement::sGnssMeasureCbIface_1_1 = nullptr;
sp<V2_0::IGnssMeasurementCallback> GnssMeasurement::sGnssMeasureCbIface_2_0 = nullptr;


GpsMeasurementCallbacks GnssMeasurement::sGnssMeasurementCbs = {
    .size = sizeof(GpsMeasurementCallbacks),
    .measurement_callback = gpsMeasurementCb,
    .gnss_measurement_callback = gnssMeasurementCb
};

/*
static void convertGnssData_2_0(LegacyGnssData* legacyGnssData,
        V2_0::IGnssMeasurementCallback::GnssData& out)
{
    
        size_t count = std::min(legacyGnssData->measurement_count, static_cast<size_t>(V1_0::GnssMax::SVS_COUNT));
        out.measurements.resize(count);

    for (size_t i = 0; i < count; i++) {
        auto entry = legacyGnssData->measurements[i];
        auto state = static_cast<GnssMeasurementState>(entry.state);
        if (state & V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_TOW_DECODED) {
          state |= V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_TOW_KNOWN;
        }
        if (state & V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_GLO_TOD_DECODED) {
          state |= V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_GLO_TOD_KNOWN;
        }
        out.measurements[i].v1_1.v1_0 = {
            .flags = entry.flags,
            .svid = entry.svid,
            .constellation = static_cast<V1_0::GnssConstellationType>(entry.constellation),
            .timeOffsetNs = entry.time_offset_ns,
            .state = state,
            .receivedSvTimeInNs = entry.received_sv_time_in_ns,
            .receivedSvTimeUncertaintyInNs = entry.received_sv_time_uncertainty_in_ns,
            .cN0DbHz = entry.c_n0_dbhz,
            .pseudorangeRateMps = entry.pseudorange_rate_mps,
            .pseudorangeRateUncertaintyMps = entry.pseudorange_rate_uncertainty_mps,
            .accumulatedDeltaRangeState = entry.accumulated_delta_range_state,
            .accumulatedDeltaRangeM = entry.accumulated_delta_range_m,
            .accumulatedDeltaRangeUncertaintyM = entry.accumulated_delta_range_uncertainty_m,
            .carrierFrequencyHz = entry.carrier_frequency_hz,
            .carrierCycles = entry.carrier_cycles,
            .carrierPhase = entry.carrier_phase,
            .carrierPhaseUncertainty = entry.carrier_phase_uncertainty,
            .multipathIndicator = static_cast<V1_0::IGnssMeasurementCallback::GnssMultipathIndicator>(
                    entry.multipath_indicator),
            .snrDb = entry.snr_db
        };
                out.measurements[i].constellation = static_cast<V2_0::GnssConstellationType>(entry.constellation);
                out.measurements[i].codeType = "C";
                     out.measurements[i].v1_1.accumulatedDeltaRangeState = entry.accumulated_delta_range_state;
                     out.measurements[i].state = state;   
         }

     auto clockVal = legacyGnssData->clock;
     out.clock = {
        .gnssClockFlags = clockVal.flags,
        .leapSecond = clockVal.leap_second,
        .timeNs = clockVal.time_ns,
        .timeUncertaintyNs = clockVal.time_uncertainty_ns,
        .fullBiasNs = clockVal.full_bias_ns,
        .biasNs = clockVal.bias_ns,
        .biasUncertaintyNs = clockVal.bias_uncertainty_ns,
        .driftNsps = clockVal.drift_nsps,
        .driftUncertaintyNsps = clockVal.drift_uncertainty_nsps,
        .hwClockDiscontinuityCount = clockVal.hw_clock_discontinuity_count
    };

}
*/

static void convertGnssData(LegacyGnssData* legacyGnssData,
        V1_0::IGnssMeasurementCallback::GnssData& gnssData)
{
    
    gnssData.measurementCount = std::min(legacyGnssData->measurement_count,
                                         static_cast<size_t>(V1_0::GnssMax::SVS_COUNT));

    for (size_t i = 0; i < gnssData.measurementCount; i++) {
        auto entry = legacyGnssData->measurements[i];
        auto state = static_cast<GnssMeasurementState>(entry.state);
        if (state & V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_TOW_DECODED) {
          state |= V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_TOW_KNOWN;
        }
        if (state & V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_GLO_TOD_DECODED) {
          state |= V1_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_GLO_TOD_KNOWN;
        }
        gnssData.measurements[i] = {
            .flags = entry.flags,
            .svid = entry.svid,
            .constellation = static_cast<V1_0::GnssConstellationType>(entry.constellation),
            .timeOffsetNs = entry.time_offset_ns,
            .state = state,
            .receivedSvTimeInNs = entry.received_sv_time_in_ns,
            .receivedSvTimeUncertaintyInNs = entry.received_sv_time_uncertainty_in_ns,
            .cN0DbHz = entry.c_n0_dbhz,
            .pseudorangeRateMps = entry.pseudorange_rate_mps,
            .pseudorangeRateUncertaintyMps = entry.pseudorange_rate_uncertainty_mps,
            .accumulatedDeltaRangeState = entry.accumulated_delta_range_state,
            .accumulatedDeltaRangeM = entry.accumulated_delta_range_m,
            .accumulatedDeltaRangeUncertaintyM = entry.accumulated_delta_range_uncertainty_m,
            .carrierFrequencyHz = entry.carrier_frequency_hz,
            .carrierCycles = entry.carrier_cycles,
            .carrierPhase = entry.carrier_phase,
            .carrierPhaseUncertainty = entry.carrier_phase_uncertainty,
            .multipathIndicator = static_cast<V1_0::IGnssMeasurementCallback::GnssMultipathIndicator>(
                    entry.multipath_indicator),
            .snrDb = entry.snr_db
        };
    }

    auto clockVal = legacyGnssData->clock;
    gnssData.clock = {
        .gnssClockFlags = clockVal.flags,
        .leapSecond = clockVal.leap_second,
        .timeNs = clockVal.time_ns,
        .timeUncertaintyNs = clockVal.time_uncertainty_ns,
        .fullBiasNs = clockVal.full_bias_ns,
        .biasNs = clockVal.bias_ns,
        .biasUncertaintyNs = clockVal.bias_uncertainty_ns,
        .driftNsps = clockVal.drift_nsps,
        .driftUncertaintyNsps = clockVal.drift_uncertainty_nsps,
        .hwClockDiscontinuityCount = clockVal.hw_clock_discontinuity_count
    };

}

V2_0::IGnssMeasurementCallback::GnssData GnssMeasurement::getMockMeasurement() {
    V1_0::IGnssMeasurementCallback::GnssMeasurement measurement_1_0 = {
            .flags = (uint32_t)V1_0::IGnssMeasurementCallback::GnssMeasurementFlags::HAS_CARRIER_FREQUENCY,
            .svid = (int16_t)6,
            .constellation = V1_0::GnssConstellationType::UNKNOWN,
            .timeOffsetNs = 0.0,
            .receivedSvTimeInNs = 8195997131077,
            .receivedSvTimeUncertaintyInNs = 15,
            .cN0DbHz = 30.0,
            .pseudorangeRateMps = -484.13739013671875,
            .pseudorangeRateUncertaintyMps = 0.1005345,
            .accumulatedDeltaRangeState = (uint32_t)V1_0::IGnssMeasurementCallback::GnssAccumulatedDeltaRangeState::ADR_STATE_UNKNOWN,
            .accumulatedDeltaRangeM = 0.0,
            .accumulatedDeltaRangeUncertaintyM = 0.0,
            .carrierFrequencyHz = 1.59975e+09,
            .multipathIndicator =
                    V1_0::IGnssMeasurementCallback::GnssMultipathIndicator::INDICATOR_UNKNOWN};
    V1_1::IGnssMeasurementCallback::GnssMeasurement measurement_1_1 = {.v1_0 = measurement_1_0};
    V2_0::IGnssMeasurementCallback::GnssMeasurement measurement_2_0 = {
            .v1_1 = measurement_1_1,
            .codeType = "C",
            .constellation = GnssConstellationType::GLONASS,
            .state = V2_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_CODE_LOCK | V2_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_BIT_SYNC |
                     V2_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_SUBFRAME_SYNC |
                     V2_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_TOW_DECODED |
                     V2_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_GLO_STRING_SYNC |
                     V2_0::IGnssMeasurementCallback::GnssMeasurementState::STATE_GLO_TOD_DECODED};

    hidl_vec<IGnssMeasurementCallback::GnssMeasurement> measurements(1);
    measurements[0] = measurement_2_0;
    V1_0::IGnssMeasurementCallback::GnssClock clock = {.timeNs = 2713545000000,
                                                       .fullBiasNs = -1226701900521857520,
                                                       .biasNs = 0.59689998626708984,
                                                       .biasUncertaintyNs = 47514.989972114563,
                                                       .driftNsps = -51.757811607455452,
                                                       .driftUncertaintyNsps = 310.64968328491528,
                                                       .hwClockDiscontinuityCount = 1};

    ElapsedRealtime timestamp = {
            .flags = ElapsedRealtimeFlags::HAS_TIMESTAMP_NS |
                     ElapsedRealtimeFlags::HAS_TIME_UNCERTAINTY_NS,
            .timestampNs = static_cast<uint64_t>(::android::elapsedRealtimeNano()),
            // This is an hardcoded value indicating a 1ms of uncertainty between the two clocks.
            // In an actual implementation provide an estimate of the synchronization uncertainty
            // or don't set the field.
            .timeUncertaintyNs = 1000000};

    V2_0::IGnssMeasurementCallback::GnssData gnssData = {
            .measurements = measurements,
            .clock = clock,
            .elapsedRealtime = timestamp
       };
    return gnssData;
}

void GnssMeasurement::start() {
    ALOGD("start");
    mIsActive = true;
    mThread = std::thread([this]() {
        while (mIsActive == true && mock_count++ < 3) {
            auto measurement = getMockMeasurement();
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            ALOGD("MockMeasurement report");
            if(sGnssMeasureCbIface_2_0 != nullptr)
                 sGnssMeasureCbIface_2_0->gnssMeasurementCb_2_0(measurement);
        }
    });
}

void GnssMeasurement::stop() {
    ALOGD("stop");
    mIsActive = false;
    if (mThread.joinable()) {
        mThread.join();
    }
}


GnssMeasurement::GnssMeasurement(const GpsMeasurementInterface* gpsMeasurementIface)
    : mGnssMeasureIface(gpsMeasurementIface) {}

void GnssMeasurement::gnssMeasurementCb(LegacyGnssData* legacyGnssData) {
    if (sGnssMeasureCbIface == nullptr &&  sGnssMeasureCbIface_2_0 == nullptr && sGnssMeasureCbIface_1_1 == nullptr) {
        ALOGE("%s: GNSSMeasurement Callback Interface configured incorrectly", __func__);
        return;
    }

    if (legacyGnssData == nullptr) {
        ALOGE("%s: Invalid GnssData from GNSS HAL", __func__);
        return;
    }

        if (sGnssMeasureCbIface_2_0 != nullptr) {
            V1_0::IGnssMeasurementCallback::GnssData gnssData;
            //convertGnssData_2_0(legacyGnssData, gnssData);
            //auto r = sGnssMeasureCbIface_2_0->gnssMeasurementCb_2_0(gnssData);
            convertGnssData(legacyGnssData, gnssData);
            auto r = sGnssMeasureCbIface_2_0->GnssMeasurementCb(gnssData);
            if (!r.isOk()) {
                ALOGE("%s] Error from gnssMeasurementCb description=%s",
                    __func__, r.description().c_str());
            }
        } else if (sGnssMeasureCbIface_1_1 != nullptr) {
              ALOGE("%s] gnssMeasurementCbIface_1_1 is null", __func__);
        } else if (sGnssMeasureCbIface != nullptr) {
            V1_0::IGnssMeasurementCallback::GnssData gnssData;
            convertGnssData(legacyGnssData, gnssData);
            auto r = sGnssMeasureCbIface->GnssMeasurementCb(gnssData);
            if (!r.isOk()) {
                ALOGE("%s] Error from GnssMeasurementCb description=%s",
                    __func__, r.description().c_str());
            }
      }
}

/*
 * The code in the following method has been moved here from GnssLocationProvider.
 * It converts GpsData to GnssData. This code is no longer required in
 * GnssLocationProvider since GpsData is deprecated and no longer part of the
 * GNSS interface.
 */
void GnssMeasurement::gpsMeasurementCb(GpsData* gpsData) {
    if (sGnssMeasureCbIface == nullptr) {
        ALOGE("%s: GNSSMeasurement Callback Interface configured incorrectly", __func__);
        return;
    }

    if (gpsData == nullptr) {
        ALOGE("%s: Invalid GpsData from GNSS HAL", __func__);
        return;
    }

    V1_0::IGnssMeasurementCallback::GnssData gnssData;
    gnssData.measurementCount = std::min(gpsData->measurement_count,
                                         static_cast<size_t>(V1_0::GnssMax::SVS_COUNT));


    for (size_t i = 0; i < gnssData.measurementCount; i++) {
        auto entry = gpsData->measurements[i];
        gnssData.measurements[i].flags = entry.flags;
        gnssData.measurements[i].svid = static_cast<int32_t>(entry.prn);
        if (entry.prn >= 1 && entry.prn <= 32) {
            gnssData.measurements[i].constellation = V1_0::GnssConstellationType::GPS;
        } else {
            gnssData.measurements[i].constellation =
                  V1_0::GnssConstellationType::UNKNOWN;
        }

        gnssData.measurements[i].timeOffsetNs = entry.time_offset_ns;
        gnssData.measurements[i].state = entry.state;
        gnssData.measurements[i].receivedSvTimeInNs = entry.received_gps_tow_ns;
        gnssData.measurements[i].receivedSvTimeUncertaintyInNs =
            entry.received_gps_tow_uncertainty_ns;
        gnssData.measurements[i].cN0DbHz = entry.c_n0_dbhz;
        gnssData.measurements[i].pseudorangeRateMps = entry.pseudorange_rate_mps;
        gnssData.measurements[i].pseudorangeRateUncertaintyMps =
                entry.pseudorange_rate_uncertainty_mps;
        gnssData.measurements[i].accumulatedDeltaRangeState =
                entry.accumulated_delta_range_state;
        gnssData.measurements[i].accumulatedDeltaRangeM =
                entry.accumulated_delta_range_m;
        gnssData.measurements[i].accumulatedDeltaRangeUncertaintyM =
                entry.accumulated_delta_range_uncertainty_m;

        if (entry.flags & GNSS_MEASUREMENT_HAS_CARRIER_FREQUENCY) {
            gnssData.measurements[i].carrierFrequencyHz = entry.carrier_frequency_hz;
        } else {
            gnssData.measurements[i].carrierFrequencyHz = 0;
        }

        if (entry.flags & GNSS_MEASUREMENT_HAS_CARRIER_PHASE) {
            gnssData.measurements[i].carrierPhase = entry.carrier_phase;
        } else {
            gnssData.measurements[i].carrierPhase = 0;
        }

        if (entry.flags & GNSS_MEASUREMENT_HAS_CARRIER_PHASE_UNCERTAINTY) {
            gnssData.measurements[i].carrierPhaseUncertainty = entry.carrier_phase_uncertainty;
        } else {
            gnssData.measurements[i].carrierPhaseUncertainty = 0;
        }

        gnssData.measurements[i].multipathIndicator =
                static_cast<V1_0::IGnssMeasurementCallback::GnssMultipathIndicator>(
                        entry.multipath_indicator);

        if (entry.flags & GNSS_MEASUREMENT_HAS_SNR) {
            gnssData.measurements[i].snrDb = entry.snr_db;
        } else {
            gnssData.measurements[i].snrDb = 0;
        }
    }

    auto clockVal = gpsData->clock;
    static uint32_t discontinuity_count_to_handle_old_clock_type = 0;

    gnssData.clock.leapSecond = clockVal.leap_second;
    /*
     * GnssClock only supports the more effective HW_CLOCK type, so type
     * handling and documentation complexity has been removed.  To convert the
     * old GPS_CLOCK types (active only in a limited number of older devices),
     * the GPS time information is handled as an always discontinuous HW clock,
     * with the GPS time information put into the full_bias_ns instead - so that
     * time_ns - full_bias_ns = local estimate of GPS time. Additionally, the
     * sign of full_bias_ns and bias_ns has flipped between GpsClock &
     * GnssClock, so that is also handled below.
     */
    switch (clockVal.type) {
        case GPS_CLOCK_TYPE_UNKNOWN:
            // Clock type unsupported.
            ALOGE("Unknown clock type provided.");
            break;
        case GPS_CLOCK_TYPE_LOCAL_HW_TIME:
            // Already local hardware time. No need to do anything.
            break;
        case GPS_CLOCK_TYPE_GPS_TIME:
            // GPS time, need to convert.
            clockVal.flags |= GPS_CLOCK_HAS_FULL_BIAS;
            clockVal.full_bias_ns = clockVal.time_ns;
            clockVal.time_ns = 0;
            gnssData.clock.hwClockDiscontinuityCount =
                    discontinuity_count_to_handle_old_clock_type++;
            break;
    }

    gnssData.clock.timeNs = clockVal.time_ns;
    gnssData.clock.timeUncertaintyNs = clockVal.time_uncertainty_ns;
    /*
     * Definition of sign for full_bias_ns & bias_ns has been changed since N,
     * so flip signs here.
     */
    gnssData.clock.fullBiasNs = -(clockVal.full_bias_ns);
    gnssData.clock.biasNs = -(clockVal.bias_ns);
    gnssData.clock.biasUncertaintyNs = clockVal.bias_uncertainty_ns;
    gnssData.clock.driftNsps = clockVal.drift_nsps;
    gnssData.clock.driftUncertaintyNsps = clockVal.drift_uncertainty_nsps;
    gnssData.clock.gnssClockFlags = clockVal.flags;

    auto ret = sGnssMeasureCbIface->GnssMeasurementCb(gnssData);
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
}

// Methods from ::android::hardware::gnss::V1_0::IGnssMeasurement follow.
Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> GnssMeasurement::setCallback(
        const sp<V1_0::IGnssMeasurementCallback>& callback)  {
    if (mGnssMeasureIface == nullptr) {
        ALOGE("%s: GnssMeasure interface is unavailable", __func__);
        return V1_0::IGnssMeasurement::GnssMeasurementStatus::ERROR_GENERIC;
    }
    sGnssMeasureCbIface = callback;

    return static_cast<V1_0::IGnssMeasurement::GnssMeasurementStatus>(
            mGnssMeasureIface->init(&sGnssMeasurementCbs));
}

// Methods from V1_1::IGnssMeasurement follow.
Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> GnssMeasurement::setCallback_1_1(
    const sp<V1_1::IGnssMeasurementCallback>& callback, bool enableFullTracking) {
    // TODO implement
        bool parm = false;
        parm = enableFullTracking;
    if (mGnssMeasureIface == nullptr) {
        ALOGE("%s: GnssMeasure interface is unavailable", __func__);
        return V1_0::IGnssMeasurement::GnssMeasurementStatus::ERROR_GENERIC;
    }
    sGnssMeasureCbIface_1_1 = callback;
    return static_cast<V1_0::IGnssMeasurement::GnssMeasurementStatus>(
            mGnssMeasureIface->init(&sGnssMeasurementCbs));

}


// Methods from V2_0::IGnssMeasurement follow.
Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> GnssMeasurement::setCallback_2_0(
    const sp<V2_0::IGnssMeasurementCallback>& callback, bool enableFullTracking) {
    bool parm = false;
  
    parm = enableFullTracking;
    if (mGnssMeasureIface == nullptr) {
        ALOGE("%s: GnssMeasure interface is unavailable", __func__);
        return V1_0::IGnssMeasurement::GnssMeasurementStatus::ERROR_GENERIC;
    }

    sGnssMeasureCbIface_2_0 = callback;
    if (mIsActive) {
        ALOGW("GnssMeasurement callback already set. Resetting the callback...");
        stop();
    }
    mock_count = 0;
    start();
    return static_cast<V1_0::IGnssMeasurement::GnssMeasurementStatus>(
            mGnssMeasureIface->init(&sGnssMeasurementCbs));
}


Return<void> GnssMeasurement::close()  {
   if(sGnssMeasureCbIface != nullptr)
        sGnssMeasureCbIface = nullptr;
   if(sGnssMeasureCbIface_1_1 != nullptr)
       sGnssMeasureCbIface_1_1 = nullptr;
   if(sGnssMeasureCbIface_2_0 != nullptr)
      sGnssMeasureCbIface_2_0 = nullptr;

    if (mGnssMeasureIface == nullptr) {
        ALOGE("%s: GnssMeasure interface is unavailable", __func__);
    } else {
        mGnssMeasureIface->close();
    }
    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
