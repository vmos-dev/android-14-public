/*
 * Copyright 2024 Rockchip Electronics Co., Ltd
 */
#ifndef ANDROID_LIGHT_SENSOR_H
#define ANDROID_LIGHT_SENSOR_H

#pragma once

#include <aidl/android/hardware/sensors/BnSensors.h>

#include "Sensor.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

class LightSensor : public Sensor {
  public:
    LightSensor(int32_t sensorHandle, ISensorsEventCallback* callback);

  protected:
    float mCurEventData;
    float mLastEventData;

    virtual void readEventPayload(EventPayload& payload) override;
    virtual void driverActivate(bool enable) override;
    virtual void driverSetDelay(int64_t samplingPeriodNs) override;

    void processEvent(int code, int value);
};

} // sensors
} // hardware
} // android
} // aidl

#endif
