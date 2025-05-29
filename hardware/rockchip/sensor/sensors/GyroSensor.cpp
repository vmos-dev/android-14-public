/*
 * Copyright 2024 Rockchip Electronics Co., Ltd
 */

#include "sensors-impl/GyroSensor.h"
#include <cutils/properties.h>
#include <log/log.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#define L3G4200D_IOCTL_BASE 77

#define L3G4200D_IOCTL_SET_DELAY _IOW(L3G4200D_IOCTL_BASE, 0, int)
#define L3G4200D_IOCTL_GET_DELAY _IOR(L3G4200D_IOCTL_BASE, 1, int)
#define L3G4200D_IOCTL_SET_ENABLE _IOW(L3G4200D_IOCTL_BASE, 2, int)
#define L3G4200D_IOCTL_GET_ENABLE _IOR(L3G4200D_IOCTL_BASE, 3, int)
#define L3G4200D_IOCTL_GET_CALIBRATION _IOR(L3G4200D_IOCTL_BASE, 4, int[3])

#define EVENT_TYPE_GYRO_X          REL_RX
#define EVENT_TYPE_GYRO_Y          REL_RY
#define EVENT_TYPE_GYRO_Z          REL_RZ
#define CONVERT_GYRO                (0.001065264)
#define CONVERT_GYRO_X              (CONVERT_GYRO)
#define CONVERT_GYRO_Y              (CONVERT_GYRO)
#define CONVERT_GYRO_Z              (CONVERT_GYRO)

#ifndef M_PI
#define M_PI            3.14159265358979323846  // matches value in gcc v2 math.h
#endif

#define RANGE_GYRO                  (2000.0f*(float)M_PI/180.0f)
#define GYROERATION_RATIO_ANDROID_TO_HW        (9.80665f / 16384)
//#define DBG_LOG(...)	\
//    do {		\
//        if (Debug_on)	\
//	    ALOGI(__VA_ARGS__);	\
//    } while (0);
//
//int Debug_on;

using namespace std;
namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

GyroSensor::GyroSensor(int32_t sensorHandle, ISensorsEventCallback* callback) : Sensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "gyro";
    mSensorInfo.vendor = "Rockchip Sensor";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::GYROSCOPE;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = RANGE_GYRO;
    mSensorInfo.resolution = CONVERT_GYRO;
    mSensorInfo.power = 6.1f;
    mSensorInfo.minDelayUs = 7000;
    mSensorInfo.maxDelayUs = 200000;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = 0;
    mSensorName = "gyro";
    openDevInput();
};

void GyroSensor::driverActivate(bool enable) {
    ALOGI("driverActivate: %s", mSensorName.c_str());
    int err = 0;
    int flag = enable ? 1 : 0;
    std::string driverPath = "/dev/gyrosensor";
    int dev_fd = open(driverPath.c_str(), O_RDONLY);

    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
    if (enable) {
        if (0 > (err = ioctl(dev_fd, L3G4200D_IOCTL_SET_ENABLE, &flag))) {
            ALOGE("Failed to start ioctl. %s", strerror(errno));
        }
    } else {
        if (0 > (err = ioctl(dev_fd, L3G4200D_IOCTL_SET_ENABLE, &flag))) {
            ALOGE("Failed to close ioctl. %s", strerror(errno));
        }
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void GyroSensor::driverSetDelay(int64_t samplingPeriodNs) {
    ALOGI("driverSetDelay: %s -> %" PRId64"", mSensorName.c_str(), samplingPeriodNs);
    int err = 0;
    std::string driverPath = "/dev/gyrosensor";
    int dev_fd = open(driverPath.c_str(), O_RDONLY);
    if (dev_fd < 0) {
        ALOGE("Failed to open %s", driverPath.c_str());
        return;
    }
    short delay = samplingPeriodNs/1000000;
    if (ioctl(dev_fd, L3G4200D_IOCTL_SET_DELAY, &delay)) {
        ALOGE("Failed to set delay ioctl. %s", strerror(errno));
    }
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

void GyroSensor::processEvent(int code, int value) {
    ALOGV("processEvent: code: %d, value: %d", code, value);
    switch (code) {
        case EVENT_TYPE_GYRO_X:
	    ALOGV("sensor EVENT_TYPE_GYRO_X\n");
            this->mCurEventData[0] = value * CONVERT_GYRO_X;
            break;
        case EVENT_TYPE_GYRO_Y:
	    ALOGV("sensor EVENT_TYPE_GYRO_Y\n");
            this->mCurEventData[1] = value * CONVERT_GYRO_Y;
            break;
        case EVENT_TYPE_GYRO_Z:
	    ALOGV("sensor EVENT_TYPE_GYRO_Z\n");
            this->mCurEventData[2] = value * CONVERT_GYRO_Z;
            break;
        default:
	    ALOGV("sensor EVENT_TYPE_GYRO_ERR\n");
            break;
    }
}

void GyroSensor::readEventPayload(EventPayload& payload) {
    ssize_t n, dataAvailable;
    input_event const* event;
    EventPayload::Vec3 vec3 = {
            .x = 0,
            .y = 0,
            .z = 0,
            .status = SensorStatus::ACCURACY_HIGH,
    };

    n = mInputReader.fill(this->mInputFd);
    if (n < 0) {
        //Sensor::setCheckSelf();
        ALOGD("GyroSensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    dataAvailable = mInputReader.readEvent(&event);
    if (dataAvailable <= 0) {
        ALOGD("GyroSensor %s readEventPayload fill data failed", mSensorName.c_str());
        goto out;
    }
    while (dataAvailable) {
        int type = event->type;

        if ((type == EV_ABS) || (type == EV_REL)) {
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
    ALOGV("Sensor: payload.set\n");
    payload.set<EventPayload::Tag::vec3>(vec3);
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
