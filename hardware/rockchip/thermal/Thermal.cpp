/*
 * Copyright (C) 2023 Rockchip Electronics Co.,Ltd.
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

#define ATRACE_TAG (ATRACE_TAG_THERMAL | ATRACE_TAG_HAL)

#include "Thermal.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <utils/Trace.h>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

namespace {

ndk::ScopedAStatus initErrorStatus() {
    return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_STATE,
                                                            "ThermalHAL not initialized properly.");
}

ndk::ScopedAStatus readErrorStatus() {
    return ndk::ScopedAStatus::fromExceptionCodeWithMessage(
            EX_ILLEGAL_STATE, "ThermalHal cannot read any sensor data");
}

bool interfacesEqual(const std::shared_ptr<::ndk::ICInterface> left,
                     const std::shared_ptr<::ndk::ICInterface> right) {
    if (left == nullptr || right == nullptr || !left->isRemote() || !right->isRemote()) {
        return left == right;
    }
    return left->asBinder() == right->asBinder();
}

}  // namespace

Thermal::Thermal()
    : thermal_impl_(
              std::bind(&Thermal::sendThermalChangedCallback, this, std::placeholders::_1)) {}

ndk::ScopedAStatus Thermal::getTemperatures(std::vector<Temperature> *_aidl_return) {
    return getFilteredTemperatures(false, TemperatureType::UNKNOWN, _aidl_return);
}

ndk::ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType type,
                                                    std::vector<Temperature> *_aidl_return) {
    return getFilteredTemperatures(true, type, _aidl_return);
}

ndk::ScopedAStatus Thermal::getFilteredTemperatures(bool filterType, TemperatureType type,
                                                    std::vector<Temperature> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_impl_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_impl_.fill_temperatures(filterType, false, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice> *_aidl_return) {
    return getFilteredCoolingDevices(false, CoolingType::BATTERY, _aidl_return);
}

ndk::ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType type,
                                                      std::vector<CoolingDevice> *_aidl_return) {
    return getFilteredCoolingDevices(true, type, _aidl_return);
}

ndk::ScopedAStatus Thermal::getFilteredCoolingDevices(bool filterType, CoolingType type,
                                                      std::vector<CoolingDevice> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_impl_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_impl_.fill_cooling_devices(filterType, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold> *_aidl_return) {
    *_aidl_return = {};
    return getFilteredTemperatureThresholds(false, TemperatureType::UNKNOWN, _aidl_return);
}

ndk::ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return) {
    return getFilteredTemperatureThresholds(true, type, _aidl_return);
}

ndk::ScopedAStatus Thermal::getFilteredTemperatureThresholds(
        bool filterType, TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_impl_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_impl_.fill_thresholds(filterType, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback) {
    ATRACE_CALL();
    return registerThermalChangedCallback(callback, false, TemperatureType::UNKNOWN);
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback> &callback, TemperatureType type) {
    ATRACE_CALL();
    return registerThermalChangedCallback(callback, true, type);
}

ndk::ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback) {
    if (callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    bool removed = false;
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    callbacks_.erase(
            std::remove_if(
                    callbacks_.begin(), callbacks_.end(),
                    [&](const CallbackSetting &c) {
                        if (interfacesEqual(c.callback, callback)) {
                            LOG(INFO)
                                    << "a callback has been unregistered to ThermalHAL, isFilter: "
                                    << c.is_filter_type << " Type: " << toString(c.type);
                            removed = true;
                            return true;
                        }
                        return false;
                    }),
            callbacks_.end());
    if (!removed) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback wasn't registered");
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback, bool filterType,
        TemperatureType type) {
    ATRACE_CALL();
    if (callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    if (!thermal_impl_.isInitializedOk()) {
        return initErrorStatus();
    }
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    if (std::any_of(callbacks_.begin(), callbacks_.end(), [&](const CallbackSetting &c) {
            return interfacesEqual(c.callback, callback);
        })) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback already registered");
    }
    auto c = callbacks_.emplace_back(callback, filterType, type);
    LOG(INFO) << "a callback has been registered to ThermalHAL, isFilter: " << c.is_filter_type
              << " Type: " << toString(c.type);
    // Send notification right away after successful thermal callback registration
    std::function<void()> handler = [this, c, filterType, type]() {
        std::vector<Temperature> temperatures;
        if (thermal_impl_.fill_temperatures(filterType, true, type, &temperatures)) {
            for (const auto &t : temperatures) {
                if (!filterType || t.type == type) {
                    LOG(INFO) << "Sending notification: "
                              << " Type: " << toString(t.type) << " Name: " << t.name
                              << " CurrentValue: " << t.value
                              << " ThrottlingStatus: " << toString(t.throttlingStatus);
                    c.callback->notifyThrottling(t);
                }
            }
        }
    };
    looper_.addEvent(Looper::Event{handler});
    return ndk::ScopedAStatus::ok();
}

void Thermal::sendThermalChangedCallback(const Temperature &t) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    LOG(VERBOSE) << "Sending notification: "
                 << " Type: " << toString(t.type) << " Name: " << t.name
                 << " CurrentValue: " << t.value
                 << " ThrottlingStatus: " << toString(t.throttlingStatus);

    callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
                                    [&](const CallbackSetting &c) {
                                        if (!c.is_filter_type || t.type == c.type) {
                                            ::ndk::ScopedAStatus ret =
                                                    c.callback->notifyThrottling(t);
                                            if (!ret.isOk()) {
                                                LOG(ERROR) << "a Thermal callback is dead, removed "
                                                              "from callback list.";
                                                return true;
                                            }
                                            return false;
                                        }
                                        return false;
                                    }),
                     callbacks_.end());
}

/*
void Thermal::dumpThrottlingInfo(std::ostringstream *dump_buf) {
    *dump_buf << "getThrottlingInfo:" << std::endl;
    const auto &map = thermal_impl_.GetSensorInfoMap();
    const auto &thermal_throttling_status_map = thermal_impl_.GetThermalThrottlingStatusMap();
    for (const auto &name_info_pair : map) {
        if (name_info_pair.second.throttling_info == nullptr) {
            continue;
        }
        if (name_info_pair.second.throttling_info->binded_cdev_info_map.size()) {
            if (thermal_throttling_status_map.find(name_info_pair.first) ==
                thermal_throttling_status_map.end()) {
                continue;
            }
            *dump_buf << " Name: " << name_info_pair.first << std::endl;
            if (thermal_throttling_status_map.at(name_info_pair.first)
                        .pid_power_budget_map.size()) {
                *dump_buf << "  PID Info:" << std::endl;
                *dump_buf << "   K_po: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_po[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   K_pu: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_pu[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   K_i: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_i[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   K_d: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_d[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   i_max: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->i_max[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   max_alloc_power: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->max_alloc_power[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   min_alloc_power: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->min_alloc_power[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   s_power: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->s_power[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   i_cutoff: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->i_cutoff[i] << " ";
                }
                *dump_buf << "]" << std::endl;
            }
            *dump_buf << "  Binded CDEV Info:" << std::endl;
            for (const auto &binded_cdev_info_pair :
                 name_info_pair.second.throttling_info->binded_cdev_info_map) {
                *dump_buf << "   Cooling device name: " << binded_cdev_info_pair.first << std::endl;
                if (thermal_throttling_status_map.at(name_info_pair.first)
                            .pid_power_budget_map.size()) {
                    *dump_buf << "    WeightForPID: [";
                    for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                        *dump_buf << binded_cdev_info_pair.second.cdev_weight_for_pid[i] << " ";
                    }
                    *dump_buf << "]" << std::endl;
                }
                *dump_buf << "    Ceiling: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << binded_cdev_info_pair.second.cdev_ceiling[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "    Hard limit: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << binded_cdev_info_pair.second.limit_info[i] << " ";
                }
                *dump_buf << "]" << std::endl;

                if (!binded_cdev_info_pair.second.power_rail.empty()) {
                    *dump_buf << "    Binded power rail: "
                              << binded_cdev_info_pair.second.power_rail << std::endl;
                    *dump_buf << "    Power threshold: [";
                    for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                        *dump_buf << binded_cdev_info_pair.second.power_thresholds[i] << " ";
                    }
                    *dump_buf << "]" << std::endl;
                    *dump_buf << "    Floor with PowerLink: [";
                    for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                        *dump_buf << binded_cdev_info_pair.second.cdev_floor_with_power_link[i]
                                  << " ";
                    }
                    *dump_buf << "]" << std::endl;
                    *dump_buf << "    Release logic: ";
                    switch (binded_cdev_info_pair.second.release_logic) {
                        case ReleaseLogic::INCREASE:
                            *dump_buf << "INCREASE";
                            break;
                        case ReleaseLogic::DECREASE:
                            *dump_buf << "DECREASE";
                            break;
                        case ReleaseLogic::STEPWISE:
                            *dump_buf << "STEPWISE";
                            break;
                        case ReleaseLogic::RELEASE_TO_FLOOR:
                            *dump_buf << "RELEASE_TO_FLOOR";
                            break;
                        default:
                            *dump_buf << "NONE";
                            break;
                    }
                    *dump_buf << std::endl;
                    *dump_buf << "    high_power_check: " << std::boolalpha
                              << binded_cdev_info_pair.second.high_power_check << std::endl;
                    *dump_buf << "    throttling_with_power_link: " << std::boolalpha
                              << binded_cdev_info_pair.second.throttling_with_power_link
                              << std::endl;
                }
            }
        }
    }
}
*/
void Thermal::dumpThermalData(int fd) {
    std::ostringstream dump_buf;

    if (!thermal_impl_.isInitializedOk()) {
        dump_buf << "ThermalHAL not initialized properly." << std::endl;
    } else {
        /*{
            const auto &map = thermal_impl_.GetSensorInfoMap();
            dump_buf << "getCurrentTemperatures:" << std::endl;
            Temperature temp_2_0;
            for (const auto &name_info_pair : map) {
                thermal_impl_.read_temperature(name_info_pair.first, &temp_2_0, nullptr, true);
                dump_buf << " Type: " << toString(temp_2_0.type)
                         << " Name: " << name_info_pair.first << " CurrentValue: " << temp_2_0.value
                         << " ThrottlingStatus: " << toString(temp_2_0.throttlingStatus)
                         << std::endl;
            }
            dump_buf << "getTemperatureThresholds:" << std::endl;
            for (const auto &name_info_pair : map) {
                if (!name_info_pair.second.is_watch) {
                    continue;
                }
                dump_buf << " Type: " << toString(name_info_pair.second.type)
                         << " Name: " << name_info_pair.first;
                dump_buf << " hotThrottlingThreshold: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    dump_buf << name_info_pair.second.hot_thresholds[i] << " ";
                }
                dump_buf << "] coldThrottlingThreshold: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    dump_buf << name_info_pair.second.cold_thresholds[i] << " ";
                }
                dump_buf << "] vrThrottlingThreshold: " << name_info_pair.second.vr_threshold;
                dump_buf << std::endl;
            }
        }*/
        {
            dump_buf << "getCurrentCoolingDevices:" << std::endl;
            std::vector<CoolingDevice> cooling_devices;
            if (!thermal_impl_.fill_cooling_devices(false, CoolingType::CPU,
                                                           &cooling_devices)) {
                dump_buf << " Failed to getCurrentCoolingDevices." << std::endl;
            }

            for (const auto &c : cooling_devices) {
                dump_buf << " Type: " << toString(c.type) << " Name: " << c.name
                         << " CurrentValue: " << c.value << std::endl;
            }
        }
        {
            dump_buf << "getCallbacks:" << std::endl;
            dump_buf << " Total: " << callbacks_.size() << std::endl;
            for (const auto &c : callbacks_) {
                dump_buf << " IsFilter: " << c.is_filter_type << " Type: " << toString(c.type)
                         << std::endl;
            }
        }
        {
            /*dump_buf << "sendCallback:" << std::endl;
            dump_buf << "  Enabled List: ";
            const auto &map = thermal_impl_.GetSensorInfoMap();
            for (const auto &name_info_pair : map) {
                if (name_info_pair.second.send_cb) {
                    dump_buf << name_info_pair.first << " ";
                }
            }
            dump_buf << std::endl;
            */
        }
 //       dumpThrottlingInfo(&dump_buf);
    }
    std::string buf = dump_buf.str();
    if (!::android::base::WriteStringToFd(buf, fd)) {
        PLOG(ERROR) << "Failed to dump state to fd";
    }
    fsync(fd);
}

binder_status_t Thermal::dump(int fd, const char **args, uint32_t numArgs) {
    if (numArgs == 0 || std::string(args[0]) == "-a") {
        dumpThermalData(fd);
        return STATUS_OK;
    }
    return STATUS_BAD_VALUE;
}

void Thermal::Looper::addEvent(const Thermal::Looper::Event &e) {
    std::unique_lock<std::mutex> lock(mutex_);
    events_.push(e);
    cv_.notify_all();
}

void Thermal::Looper::loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return !events_.empty(); });
        Event event = events_.front();
        events_.pop();
        lock.unlock();
        event.handler();
    }
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
