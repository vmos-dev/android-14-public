/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd
 */
#include "sensors-impl/LightSensor.h"
#include <cutils/properties.h>
#include <log/log.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#define LIGHTSENSOR_IOCTL_MAGIC 'l'
#define LIGHTSENSOR_IOCTL_GET_ENABLED		_IOR(LIGHTSENSOR_IOCTL_MAGIC, 1, int *)
#define LIGHTSENSOR_IOCTL_ENABLE		_IOW(LIGHTSENSOR_IOCTL_MAGIC, 2, int *)
#define LIGHTSENSOR_IOCTL_SET_RATE		_IOW(LIGHTSENSOR_IOCTL_MAGIC, 3, short)
#define EVENT_TYPE_LIGHT			ABS_MISC

using namespace std;
namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

LightSensor::LightSensor(int32_t sensorHandle, ISensorsEventCallback* callback) : Sensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "LightSensor";
    mSensorInfo.vendor = "Rockchip Sensor";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::LIGHT;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 43000.0f;
    mSensorInfo.resolution = 10.0f;
    mSensorInfo.power = 0.001f;          // mA
    mSensorInfo.minDelayUs = 200 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE);
    mSensorName = "lightsensor-level";
    openDevInput();
};

void LightSensor::driverActivate(bool enable) {
    ALOGI("driverActivate: %s", mSensorName.c_str());
    int err = 0;
    std::string driverPath = "/dev/lightsensor";
    int flag = enable ? 1 : 0;
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
    if (enable) {
        if (0 > (err = ioctl(dev_fd, LIGHTSENSOR_IOCTL_ENABLE, &flag))) {
            ALOGE("Failed to start ioctl. %s", strerror(errno));
        }
    } else {
        if (0 > (err = ioctl(dev_fd, LIGHTSENSOR_IOCTL_ENABLE, &flag))) {
            ALOGE("Failed to close ioctl. %s", strerror(errno));
        }
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void LightSensor::driverSetDelay(int64_t samplingPeriodNs) {
    ALOGI("driverSetDelay: %s -> %" PRId64"", mSensorName.c_str(), samplingPeriodNs);
    int err = 0;
    std::string driverPath = "/dev/lightsensor";
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }

    short delay = samplingPeriodNs/1000000;
    if (ioctl(dev_fd, LIGHTSENSOR_IOCTL_SET_RATE, &delay)) {
        ALOGE("Failed to set delay ioctl. %s", strerror(errno));
    }

    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void LightSensor::processEvent(int code, int index) {
    ALOGI("processEvent: code: %d, value: %d", code, index);
    static const float luxValues[8] = {
            10.0, 160.0, 225.0, 320.0,
            640.0, 1280.0, 2600.0, 10240.0
    };
    const size_t maxIndex = sizeof(luxValues)/sizeof(*luxValues) - 1;
    if (index > maxIndex)
	index = maxIndex;
    if (code == EVENT_TYPE_LIGHT)
        this->mCurEventData = luxValues[index];
}

void LightSensor::readEventPayload(EventPayload& payload) {
    ssize_t n, dataAvailable;
    input_event const* event;

    n = mInputReader.fill(this->mInputFd);
    if (n < 0) {
        //Sensor::setCheckSelf();
        ALOGD("LightSensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    dataAvailable = mInputReader.readEvent(&event);
    if (dataAvailable <= 0) {
        ALOGD("LightSensor %s readEventPayload fill data failed", mSensorName.c_str());
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

