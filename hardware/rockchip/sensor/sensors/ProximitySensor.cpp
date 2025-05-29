/*
 * Copyright 2024 Rockchip Electronics Co., Ltd
 */
#include "sensors-impl/ProximitySensor.h"
#include <cutils/properties.h>
#include <log/log.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#define PSENSOR_IOCTL_MAGIC 'p'
#define PSENSOR_IOCTL_GET_ENABLED               _IOR(PSENSOR_IOCTL_MAGIC, 1, int *)
#define PSENSOR_IOCTL_ENABLE                    _IOW(PSENSOR_IOCTL_MAGIC, 2, int *)
#define PSENSOR_IOCTL_DISABLE                   _IOW(PSENSOR_IOCTL_MAGIC, 3, int *)
#define EVENT_TYPE_PROXIMITY                    ABS_DISTANCE
#define PROXIMITY_THRESHOLD_CM                  9.0f

//#define DBG_LOG(...)	\
//    do {		\
//        if (Debug_on)	\
//	    ALOGI(__VA_ARGS__);	\
//    } while (0);

//int Debug_on;

using namespace std;
namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

ProximitySensor::ProximitySensor(int32_t sensorHandle, ISensorsEventCallback* callback) : Sensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "proximity";
    mSensorInfo.vendor = "Rockchip Sensor";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::PROXIMITY;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 5.0f;
    mSensorInfo.resolution = 1.0f;
    mSensorInfo.power = 0.012f;          // mA
    mSensorInfo.minDelayUs = 200 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE |
                                              SensorInfo::SENSOR_FLAG_BITS_WAKE_UP);
    mSensorName = "proximity";
    openDevInput();
};

void ProximitySensor::driverActivate(bool enable) {
    ALOGI("driverActivate: %s", mSensorName.c_str());
    int err = 0;
    std::string driverPath = "/dev/psensor";
    int flag = enable ? 1 : 0;
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
    if (enable) {
        if (0 > (err = ioctl(dev_fd, PSENSOR_IOCTL_ENABLE, &flag))) {
            ALOGE("Failed to start ioctl. %s", strerror(errno));
        }
    } else {
        if (0 > (err = ioctl(dev_fd, PSENSOR_IOCTL_ENABLE, &flag))) {
            ALOGE("Failed to close ioctl. %s", strerror(errno));
        }
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void ProximitySensor::driverSetDelay(int64_t samplingPeriodNs) {
    ALOGI("driverSetDelay: %s -> %" PRId64"", mSensorName.c_str(), samplingPeriodNs);
    int err = 0;
    std::string driverPath = "/dev/psensor";
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
/*
    short delay = samplingPeriodNs/1000000;
    if (ioctl(dev_fd, PSENSOR_IOCTL_APP_SET_RATE, &delay)) {
        ALOGE("Failed to set delay ioctl. %s", strerror(errno));
    }
*/
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void ProximitySensor::processEvent(int code, int index) {
    ALOGI("processEvent: code: %d, value: %d", code, index);
    if (code == EVENT_TYPE_PROXIMITY)
        this->mCurEventData = index * PROXIMITY_THRESHOLD_CM;
}

void ProximitySensor::readEventPayload(EventPayload& payload) {
    ssize_t n, dataAvailable;
    input_event const* event;

    n = mInputReader.fill(this->mInputFd);
    if (n < 0) {
        //Sensor::setCheckSelf();
        ALOGD("ProximitySensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    dataAvailable = mInputReader.readEvent(&event);
    if (dataAvailable <= 0) {
        ALOGD("ProximitySensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    while (dataAvailable) {
        int type = event->type;

        if (type == EV_ABS) {
            ALOGV("Sensor: %s event {code %d value %d}",
                  mSensorName.c_str(), event->code, event->value);
            processEvent(event->code, event->value);
        } else if (type == EV_SYN) {
            this->mLastEventData = this->mCurEventData;
            break;
        }
        dataAvailable = mInputReader.readEvent(&event);
    }

out:
    payload.set<EventPayload::Tag::scalar>(this->mCurEventData);
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl

