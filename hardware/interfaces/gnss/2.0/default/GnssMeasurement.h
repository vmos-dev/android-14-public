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

#ifndef android_hardware_gnss_V2_0_GnssMeasurement_H_
#define android_hardware_gnss_V2_0_GnssMeasurement_H_

#include <ThreadCreationWrapper.h>
#include <android/hardware/gnss/2.0/IGnssMeasurement.h>
#include <hidl/Status.h>
#include <hardware/gps.h>
#include <thread>
#include <utils/SystemClock.h>

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;

using LegacyGnssData = ::GnssData;

/*
 * Extended interface for GNSS Measurements support. Also contains wrapper methods to allow methods
 * from IGnssMeasurementCallback interface to be passed into the conventional implementation of the
 * GNSS HAL.
 */
struct GnssMeasurement : public IGnssMeasurement {
    GnssMeasurement(const GpsMeasurementInterface* gpsMeasurementIface);

    /*
     * Methods from ::android::hardware::gnss::V1_0::IGnssMeasurement follow.
     * These declarations were generated from IGnssMeasurement.hal.
     */
    Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> setCallback(
        const sp<V1_0::IGnssMeasurementCallback>& callback) override;
    Return<void> close() override;

    // Methods from V1_1::IGnssMeasurement follow.
    Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> setCallback_1_1(
        const sp<V1_1::IGnssMeasurementCallback>& callback, bool enableFullTracking) override;

    // Methods from V2_0::IGnssMeasurement follow.
    Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> setCallback_2_0(
        const sp<V2_0::IGnssMeasurementCallback>& callback, bool enableFullTracking) override;


    /*
     * Callback methods to be passed into the conventional GNSS HAL by the default
     * implementation. These methods are not part of the IGnssMeasurement base class.
     */
     static  void gnssMeasurementCb(LegacyGnssData* data);
     /*
      * Deprecated callback added for backward compatibity for devices that do
      * not support GnssData measurements.
      */
    static void gpsMeasurementCb(GpsData* data);


    /*
     * Holds function pointers to the callback methods.
     */
    static GpsMeasurementCallbacks sGnssMeasurementCbs;

 private:
        void start();
        void stop();
        V2_0::IGnssMeasurementCallback::GnssData getMockMeasurement();
    const GpsMeasurementInterface* mGnssMeasureIface = nullptr;
    static sp<V1_0::IGnssMeasurementCallback> sGnssMeasureCbIface;
    static sp<V1_1::IGnssMeasurementCallback> sGnssMeasureCbIface_1_1;
    static sp<V2_0::IGnssMeasurementCallback> sGnssMeasureCbIface_2_0;
    std::atomic<bool> mIsActive;
    std::thread mThread;
    int mock_count;

 };

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android

#endif  // android_hardware_gnss_V1_0_GnssMeasurement_H_
