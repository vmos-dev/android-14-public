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

#pragma once

#include <array>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <map>

#include <aidl/android/hardware/thermal/IThermal.h>

#include "ThermalWatcher.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::sp;

using NotificationCallback = std::function<void(const Temperature &t)>;

struct SensorStatus {
	ThrottlingSeverity severity;
	ThrottlingSeverity prev_hot_severity;
	ThrottlingSeverity prev_cold_severity;
};

class ThermalImpl {
  public:
	ThermalImpl(const NotificationCallback &cb);
	~ThermalImpl() = default;
	// Dissallow copy and assign.
	ThermalImpl(const ThermalImpl &) = delete;
	void operator=(const ThermalImpl &) = delete;

	bool isInitializedOk() const { return is_initialized_; }
	ThrottlingSeverity getSeverityFromThresholds(float value, TemperatureType type);
	bool fill_temperatures(bool filterType, bool filterCallback,
                        TemperatureType type,
                        std::vector<Temperature> *temperatures);
	bool fill_thresholds(bool filterType, TemperatureType type,
            std::vector<TemperatureThreshold> *thresholds) const;
	bool fill_cooling_devices(bool filterType, CoolingType type,
            std::vector<CoolingDevice> *coolingdevices);
	void init_tz_path(void);
	bool init_cl_path(void);
	bool read_temperature(int type, Temperature *ret_temp);
	int get_tz_num() const { return thermal_zone_num; };
	int get_cooling_num() const { return cooling_device_num; }
	bool is_tz_path_valided(int type);
	bool is_cooling_path_valided() const;
  private:
	// For thermal_watcher_'s polling thread
	bool thermalWatcherCallbackFunc(const std::set<std::string> &uevent_sensors);
	sp<ThermalWatcher> thermal_watcher_;
	bool is_initialized_;
	const NotificationCallback cb_;

	mutable std::shared_mutex sensor_status_map_mutex_;
	std::map<std::string, SensorStatus> sensor_status_map_;
	int thermal_zone_num;
	int cooling_device_num;
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
