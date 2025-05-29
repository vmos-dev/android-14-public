#pragma once

#include <aidl/android/hardware/sensors/BnSensors.h>

#include "Sensor.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

class AccelSensor : public Sensor {
  public:
    AccelSensor(int32_t sensorHandle, ISensorsEventCallback* callback);

  protected:
    float mCurEventData[3];
    float mLastEventData[3];

    virtual void readEventPayload(EventPayload& payload) override;
    virtual void driverActivate(bool enable) override;
    virtual void driverSetDelay(int64_t samplingPeriodNs) override;

    void processEvent(int code, int value);
};

} // sensors
} // hardware
} // android
} // aidl
