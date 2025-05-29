/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "sensors-impl/Sensor.h"

#include "utils/SystemClock.h"

#include <cmath>

#include <linux/input.h>

#include <log/log.h>
#include <dirent.h>

using ::ndk::ScopedAStatus;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

Sensor::Sensor(ISensorsEventCallback* callback)
    : mIsEnabled(false),
      mSamplingPeriodNs(0),
      mLastSampleTimeNs(0),
      mCallback(callback),
      mMode(OperationMode::NORMAL),
      mInputReader(16) {
    mRunThread = std::thread(startThread, this);
}

Sensor::~Sensor() {
    std::unique_lock<std::mutex> lock(mRunMutex);
    mStopThread = true;
    mIsEnabled = false;
    mWaitCV.notify_all();
    lock.release();
    mRunThread.join();
}

const SensorInfo& Sensor::getSensorInfo() const {
    return mSensorInfo;
}

void Sensor::batch(int64_t samplingPeriodNs) {
    if (samplingPeriodNs < mSensorInfo.minDelayUs * 1000LL) {
        samplingPeriodNs = mSensorInfo.minDelayUs * 1000LL;
    } else if (samplingPeriodNs > mSensorInfo.maxDelayUs * 1000LL) {
        samplingPeriodNs = mSensorInfo.maxDelayUs * 1000LL;
    }

    if (mSamplingPeriodNs != samplingPeriodNs) {
        mSamplingPeriodNs = samplingPeriodNs;
        // set delay
        driverSetDelay(mSamplingPeriodNs);
        // Wake up the 'run' thread to check if a new event should be generated now
        mWaitCV.notify_all();
    }
}

void Sensor::activate(bool enable) {
    if (mIsEnabled != enable) {
        // set enable
        driverActivate(enable);
        mIsEnabled = enable;
        mWaitCV.notify_all();
    }
}

ScopedAStatus Sensor::flush() {
    // Only generate a flush complete event if the sensor is enabled and if the sensor is not a
    // one-shot sensor.
    if (!mIsEnabled ||
        (mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ONE_SHOT_MODE))) {
        return ScopedAStatus::fromServiceSpecificError(
                static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
    }

    // Note: If a sensor supports batching, write all of the currently batched events for the sensor
    // to the Event FMQ prior to writing the flush complete event.
    Event ev;
    ev.sensorHandle = mSensorInfo.sensorHandle;
    ev.sensorType = SensorType::META_DATA;
    EventPayload::MetaData meta = {
            .what = MetaDataEventType::META_DATA_FLUSH_COMPLETE,
    };
    ev.payload.set<EventPayload::Tag::meta>(meta);
    std::vector<Event> evs{ev};
    mCallback->postEvents(evs, isWakeUpSensor());

    return ScopedAStatus::ok();
}

void Sensor::startThread(Sensor* sensor) {
    sensor->run();
}

void Sensor::run() {
    std::unique_lock<std::mutex> runLock(mRunMutex);
    constexpr int64_t kNanosecondsInSeconds = 1000 * 1000 * 1000;

    while (!mStopThread) {
        if (!mIsEnabled || mMode == OperationMode::DATA_INJECTION) {
            mWaitCV.wait(runLock, [&] {
                return ((mIsEnabled && mMode == OperationMode::NORMAL) || mStopThread);
            });
        } else {
            timespec curTime;
            clock_gettime(CLOCK_BOOTTIME, &curTime);
            int64_t now = (curTime.tv_sec * kNanosecondsInSeconds) + curTime.tv_nsec;
            int64_t nextSampleTime = mLastSampleTimeNs + mSamplingPeriodNs;

            if (now >= nextSampleTime) {
                mLastSampleTimeNs = now;
                nextSampleTime = mLastSampleTimeNs + mSamplingPeriodNs;
                mCallback->postEvents(readEvents(), isWakeUpSensor());
            }

            mWaitCV.wait_for(runLock, std::chrono::nanoseconds(nextSampleTime - now));
        }
    }
}

bool Sensor::isWakeUpSensor() {
    return mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_WAKE_UP);
}

std::vector<Event> Sensor::readEvents() {
    std::vector<Event> events;
    Event event;
    event.sensorHandle = mSensorInfo.sensorHandle;
    event.sensorType = mSensorInfo.type;
    event.timestamp = ::android::elapsedRealtimeNano();
    memset(&event.payload, 0, sizeof(event.payload));
    readEventPayload(event.payload);
    events.push_back(event);
    return events;
}

void Sensor::setOperationMode(OperationMode mode) {
    if (mMode != mode) {
        mMode = mode;
        mWaitCV.notify_all();
    }
}

void Sensor::openDevInput() {
    if ("" != mSensorName) {
        int clockid = CLOCK_BOOTTIME;
        mInputFd = openInput(mSensorName.c_str());
        if (ioctl(mInputFd, EVIOCSCLOCKID, &clockid))
            ALOGE("set EVIOCSCLOCKID failed");
    } else {
        ALOGE("Should never reach here: mSensorName is NULL!");
    }
    ALOGI("Successfully openDevInput for %s", mSensorName.c_str());
}

/*
 * 1: is target
 * 0: is not target
 * other: error
 */
static int is_target_dev(const char* fname, const char *inputName) {
    int ret = 0;
    int fd = -1;
    char devname[PATH_MAX];
    char name[80];

    sprintf(devname, "/sys/class/input/%s/device/name", fname);
    fd = open(devname, O_RDONLY);

    if (fd >= 0) {
        if (read(fd, name, 79) < 1)
            name[0] = '\0';

        if (!strncmp(name, inputName, strlen(inputName)))
            ret = 1;

        close(fd);
    }
    return ret;
}

int Sensor::openInput(const char *inputName) {
    int fd = -1;
    const char *dirname = "/dev/input";
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if (dir == NULL)
        return -1;

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';

    while ((de = readdir(dir))) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' ||
             (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        if (!is_target_dev(de->d_name, inputName))
            continue;

        strcpy(filename, de->d_name);
        fd = open(devname, O_RDONLY);

        ALOGI("path open %s", devname);
        if (fd >= 0) {
            char name[80];

            if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
                name[0] = '\0';
            }
            if (!strcmp(name, inputName)) {
                ALOGE("Found %s, path: %s", name, devname);
                break;
            } else {
                close(fd);
                fd = -1;
            }
        }
    }
    closedir(dir);
    if (fd < 0) ALOGE("couldn't find '%s' input device", inputName);

    return fd;
}

bool Sensor::supportsDataInjection() const {
    return mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_DATA_INJECTION);
}

ScopedAStatus Sensor::injectEvent(const Event& event) {
    if (event.sensorType == SensorType::ADDITIONAL_INFO) {
        return ScopedAStatus::ok();
        // When in OperationMode::NORMAL, SensorType::ADDITIONAL_INFO is used to push operation
        // environment data into the device.
    }

    if (!supportsDataInjection()) {
        return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    if (mMode == OperationMode::DATA_INJECTION) {
        mCallback->postEvents(std::vector<Event>{event}, isWakeUpSensor());
        return ScopedAStatus::ok();
    }

    return ScopedAStatus::fromServiceSpecificError(
            static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
}

OnChangeSensor::OnChangeSensor(ISensorsEventCallback* callback)
    : Sensor(callback), mPreviousEventSet(false) {}

void OnChangeSensor::activate(bool enable) {
    Sensor::activate(enable);
    if (!enable) {
        mPreviousEventSet = false;
    }
}

std::vector<Event> OnChangeSensor::readEvents() {
    std::vector<Event> events = Sensor::readEvents();
    std::vector<Event> outputEvents;

    for (auto iter = events.begin(); iter != events.end(); ++iter) {
        Event ev = *iter;
        if (!mPreviousEventSet ||
            memcmp(&mPreviousEvent.payload, &ev.payload, sizeof(ev.payload)) != 0) {
            outputEvents.push_back(ev);
            mPreviousEvent = ev;
            mPreviousEventSet = true;
        }
    }
    return outputEvents;
}

PressureSensor::PressureSensor(int32_t sensorHandle, ISensorsEventCallback* callback)
    : Sensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "Pressure Sensor";
    mSensorInfo.vendor = "Vendor String";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::PRESSURE;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 1100.0f;       // hPa
    mSensorInfo.resolution = 0.005f;      // hPa
    mSensorInfo.power = 0.001f;           // mA
    mSensorInfo.minDelayUs = 100 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = 0;
};

void PressureSensor::readEventPayload(EventPayload& payload) {
    payload.set<EventPayload::Tag::scalar>(1013.25f);
}

MagnetometerSensor::MagnetometerSensor(int32_t sensorHandle, ISensorsEventCallback* callback)
    : Sensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "Magnetic Field Sensor";
    mSensorInfo.vendor = "Vendor String";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::MAGNETIC_FIELD;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 1300.0f;
    mSensorInfo.resolution = 0.01f;
    mSensorInfo.power = 0.001f;          // mA
    mSensorInfo.minDelayUs = 20 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = 0;
};

void MagnetometerSensor::readEventPayload(EventPayload& payload) {
    EventPayload::Vec3 vec3 = {
            .x = 100.0,
            .y = 0,
            .z = 50.0,
            .status = SensorStatus::ACCURACY_HIGH,
    };
    payload.set<EventPayload::Tag::vec3>(vec3);
}

AmbientTempSensor::AmbientTempSensor(int32_t sensorHandle, ISensorsEventCallback* callback)
    : OnChangeSensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "Ambient Temp Sensor";
    mSensorInfo.vendor = "Vendor String";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::AMBIENT_TEMPERATURE;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 80.0f;
    mSensorInfo.resolution = 0.01f;
    mSensorInfo.power = 0.001f;
    mSensorInfo.minDelayUs = 40 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE);
};

void AmbientTempSensor::readEventPayload(EventPayload& payload) {
    payload.set<EventPayload::Tag::scalar>(40.0f);
}

RelativeHumiditySensor::RelativeHumiditySensor(int32_t sensorHandle,
                                               ISensorsEventCallback* callback)
    : OnChangeSensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "Relative Humidity Sensor";
    mSensorInfo.vendor = "Vendor String";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::RELATIVE_HUMIDITY;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 100.0f;
    mSensorInfo.resolution = 0.1f;
    mSensorInfo.power = 0.001f;
    mSensorInfo.minDelayUs = 40 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE);
}

void RelativeHumiditySensor::readEventPayload(EventPayload& payload) {
    payload.set<EventPayload::Tag::scalar>(50.0f);
}

HingeAngleSensor::HingeAngleSensor(int32_t sensorHandle, ISensorsEventCallback* callback)
    : OnChangeSensor(callback) {
    mSensorInfo.sensorHandle = sensorHandle;
    mSensorInfo.name = "Hinge Angle Sensor";
    mSensorInfo.vendor = "Vendor String";
    mSensorInfo.version = 1;
    mSensorInfo.type = SensorType::HINGE_ANGLE;
    mSensorInfo.typeAsString = "";
    mSensorInfo.maxRange = 360.0f;
    mSensorInfo.resolution = 1.0f;
    mSensorInfo.power = 0.001f;
    mSensorInfo.minDelayUs = 40 * 1000;  // microseconds
    mSensorInfo.maxDelayUs = kDefaultMaxDelayUs;
    mSensorInfo.fifoReservedEventCount = 0;
    mSensorInfo.fifoMaxEventCount = 0;
    mSensorInfo.requiredPermission = "";
    mSensorInfo.flags = static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE |
                                              SensorInfo::SENSOR_FLAG_BITS_WAKE_UP |
                                              SensorInfo::SENSOR_FLAG_BITS_DATA_INJECTION);
}

void HingeAngleSensor::readEventPayload(EventPayload& payload) {
    payload.set<EventPayload::Tag::scalar>(180.0f);
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
