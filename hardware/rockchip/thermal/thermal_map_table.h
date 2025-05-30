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

#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <log/log.h>
#include <hardware/thermal.h>
#include <array>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include "thermal_map_table_type.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

const char *CPU_ALL_LABEL[] = {"CPU0", "CPU1", "CPU2", "CPU3", "CPU4", "CPU5", "CPU6", "CPU7", "CPU8", "CPU9"};

TZ_DATA tz_data[TT_MAX] = {
    {CPU_TZ_NAME, "CPU", -1},/*CPU*/
    {GPU_TZ_NAME, "GPU", -1},/*GPU*/
    {BATTERY_TZ_NAME,"BATTERY", -1},/*BATTERY*/
    {SKIN_TZ_NAME,"SKIN", -1},/*SKIN*/
    {USB_PORT_TZ_NAME,"USB_PORT", -1},/*USB_PORT not support*/
    {POWER_AMPLIFIER_TZ_NAME,"POWER_AMPLIFIER", -1},/*POWER_AMPLIFIER*/
    {BCL_VOLTAGE_TZ_NAME,"BCL_VOLTAGE", -1},/*BCL_VOLTAGE not support*/
    {BCL_CURRENT_TZ_NAME,"BCL_CURRENT", -1},/*BCL_CURRENT not support*/
    {BCL_PERCENTAGE_TZ_NAME,"BCL_PERCENTAGE", -1},/*BCL_PERCENTAGE not support*/
    {NPU_TZ_NAME,"NPU", -1},/*NPU not support*/
    {TPU_TZ_NAME,"TPU", -1},/*TPU not support*/
    {DISPLAY_TZ_NAME,"DISPLAY", -1},/*DISPLAY not support*/
    {MODEM_TZ_NAME,"MODEM", -1},/*MODEM not support*/
    {SOC_TZ_NAME,"SOC", -1},/*SOC*/
};

COOLING_DATA cdata[MAX_COOLING] = {
    {
        {
            .type = CoolingType::CPU,
            .name = "thermal-cpufreq-0",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "thermal-cpufreq-1",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "thermal-cpufreq-2",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::COMPONENT,
            .name = "thermal-devfreq-0", // DDR
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::COMPONENT,
            .name = "thermal-devfreq-1",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::BATTERY,
            .name = "thermal-clock-0",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu00",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu01",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu02",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu03",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu04",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu05",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu06",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu07",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu08",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu09",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu10",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu11",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu12",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu13",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu14",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu15",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu16",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu17",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu18",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu19",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu20",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu21",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu22",
            .value = 0,
        },
        -1
    },
    {
        {
            .type = CoolingType::CPU,
            .name = "cpu23",
            .value = 0,
        },
        -1
    },
};
const TemperatureThreshold kRockchipTempThreshold[TT_MAX] = {
    {
        .type = TemperatureType::CPU,
        .name = CPU_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 70, NAN, NAN, 115}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::GPU,
        .name = GPU_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 70, NAN, NAN, 115}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::BATTERY,
        .name = BATTERY_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60.0}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::SKIN,
        .name = SKIN_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 90}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::USB_PORT,
        .name = USB_PORT_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::POWER_AMPLIFIER,
        .name = POWER_AMPLIFIER_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::BCL_VOLTAGE,
        .name = BCL_VOLTAGE_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::BCL_CURRENT,
        .name = BCL_CURRENT_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::BCL_PERCENTAGE,
        .name = BCL_PERCENTAGE_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 50, NAN, NAN, 60.0}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::NPU,
        .name = NPU_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::TPU,
        .name = TPU_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::DISPLAY,
        .name = DISPLAY_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::MODEM,
        .name = MODEM_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
    {
        .type = TemperatureType::SOC,
        .name = SOC_TZ_NAME,
        .hotThrottlingThresholds = {{NAN, NAN, NAN, 70, NAN, NAN, 115}},
        .coldThrottlingThresholds = {{NAN, NAN, NAN, NAN, NAN, NAN, NAN}},
    },
};


}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
