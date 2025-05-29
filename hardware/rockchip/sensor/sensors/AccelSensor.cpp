/*
 * Copyright 2024 Rockchip Electronics Co., Ltd
 */

#include "sensors-impl/AccelSensor.h"
#include <cutils/properties.h>
#include <log/log.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#define GSENSOR_IOCTL_MAGIC             'a'
#define GSENSOR_IOCTL_START             _IO(GSENSOR_IOCTL_MAGIC, 0x03)
#define GSENSOR_IOCTL_CLOSE             _IO(GSENSOR_IOCTL_MAGIC, 0x02)
#define GSENSOR_IOCTL_APP_SET_RATE      _IOW(GSENSOR_IOCTL_MAGIC, 0x10, short)

#define ACCELERATION_RATIO_ANDROID_TO_HW        (9.80665f / 16384)
#define DBG_LOG(...)	\
    do {		\
        if (Debug_on)	\
	    ALOGI(__VA_ARGS__);	\
    } while (0);

int Debug_on = 0;

using namespace std;
namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

AccelSensor::AccelSensor(int32_t sensorHandle, ISensorsEventCallback* callback) : Sensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "Accel Sensor";
    mSensorInfo.vendor = "Rockchip Sensor";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::ACCELEROMETER;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 78.4f;  // +/- 8g
    mSensorInfo.resolution = 1.52e-5;
    mSensorInfo.power = 0.001f;          // mA
    mSensorInfo.minDelayUs = 10 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_DATA_INJECTION);
    mSensorName = "gsensor";
    openDevInput();
};

void AccelSensor::driverActivate(bool enable) {
    ALOGI("driverActivate: %s", mSensorName.c_str());
    int err = 0;
    std::string driverPath = "/dev/mma8452_daemon";
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
    if (enable) {
        if (0 > (err = ioctl(dev_fd, GSENSOR_IOCTL_START))) {
            ALOGE("Failed to start ioctl. %s", strerror(errno));
        }
    } else {
        if (0 > (err = ioctl(dev_fd, GSENSOR_IOCTL_CLOSE))) {
            ALOGE("Failed to close ioctl. %s", strerror(errno));
        }
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void AccelSensor::driverSetDelay(int64_t samplingPeriodNs) {
    ALOGI("driverSetDelay: %s -> %" PRId64"", mSensorName.c_str(), samplingPeriodNs);
    int err = 0;
    std::string driverPath = "/dev/mma8452_daemon";
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
    short delay = samplingPeriodNs/1000000;
    if (ioctl(dev_fd, GSENSOR_IOCTL_APP_SET_RATE, &delay)) {
        ALOGE("Failed to set delay ioctl. %s", strerror(errno));
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void AccelSensor::processEvent(int code, int value) {
    ALOGV("processEvent: code: %d, value: %d", code, value);
    switch (code) {
        case ABS_X:
            this->mCurEventData[0] = value * ACCELERATION_RATIO_ANDROID_TO_HW;
            break;
        case ABS_Y:
            this->mCurEventData[1] = value * ACCELERATION_RATIO_ANDROID_TO_HW;
            break;
        case ABS_Z:
            this->mCurEventData[2] = value * ACCELERATION_RATIO_ANDROID_TO_HW;
            break;
        default:
            break;
    }
}

void AccelSensor::readEventPayload(EventPayload& payload) {
    ssize_t n, dataAvailable;
    input_event const* event;
    EventPayload::Vec3 vec3 = {
            .x = 0,
            .y = 0,
            .z = 9.8,
            .status = SensorStatus::ACCURACY_HIGH,
    };

    n = mInputReader.fill(this->mInputFd);
    if (n < 0) {
        //Sensor::setCheckSelf();
        DBG_LOG("AccelSensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    dataAvailable = mInputReader.readEvent(&event);
    if (dataAvailable <= 0) {
        DBG_LOG("AccelSensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    while (dataAvailable) {
        int type = event->type;

        if ((type == EV_ABS) || (type == EV_REL) || (type == EV_KEY)) {
            ALOGV("Sensor: %s event {code %d value %d}",
                  mSensorName.c_str(), event->code, event->value);
            processEvent(event->code, event->value);
        } else if (type == EV_SYN) {
            this->mLastEventData[0] = this->mCurEventData[0];
            this->mLastEventData[1] = this->mCurEventData[1];
            this->mLastEventData[2] = this->mCurEventData[2];
            break;
        }
        dataAvailable = mInputReader.readEvent(&event);
    }

out:
    vec3.x = this->mLastEventData[0];
    vec3.y = this->mLastEventData[1];
    vec3.z = this->mLastEventData[2];

    payload.set<EventPayload::Tag::vec3>(vec3);
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
