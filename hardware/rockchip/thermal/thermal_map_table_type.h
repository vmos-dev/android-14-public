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

#include <aidl/android/hardware/thermal/IThermal.h>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

#define CPU_USAGE_FILE          "/proc/stat"
#define CORENUM_PATH "/sys/devices/system/cpu/possible"

#define TZPATH_LENGTH 40
#define TZPATH_PREFIX "/sys/class/thermal/thermal_zone"
#define TZNAME_SZ 20

#define CDPATH_LENGTH 50
#define CDPATH_PREFIX "/sys/class/thermal/cooling_device"
#define CDNAME_SZ 25

#define CPU_TZ_NAME "soc-thermal"
#define GPU_TZ_NAME "gpu-thermal"
#define BATTERY_TZ_NAME "battery"
#define TEST_BATTERY_NAME "test_battery"
#define SKIN_TZ_NAME "notsupport"
#define USB_PORT_TZ_NAME "notsupport"
#define POWER_AMPLIFIER_TZ_NAME "notsupport"
#define BCL_VOLTAGE_TZ_NAME "notsupport"
#define BCL_CURRENT_TZ_NAME "notsupport"
#define BCL_PERCENTAGE_TZ_NAME "notsupport"
#define NPU_TZ_NAME "notsupport"
#define TPU_TZ_NAME "notsupport"
#define DISPLAY_TZ_NAME "notsupport"
#define MODEM_TZ_NAME "notsupport"
#define SOC_TZ_NAME "soc-thermal"

enum TempType {
    TT_UNKNOWN = -1,
    TT_CPU = 0,
    TT_GPU = 1,
    TT_BATTERY = 2,
    TT_SKIN = 3,
    TT_USB_PORT = 4,
    TT_POWER_AMPLIFIER = 5,

    /** Battery Charge Limit - virtual thermal sensors */
    TT_BCL_VOLTAGE = 6,
    TT_BCL_CURRENT = 7,
    TT_BCL_PERCENTAGE = 8,

    /** Neural Processing Unit */
    TT_NPU = 9,
    /** Add aidl definitions */
    TT_TPU = 10,
    TT_DISPLAY = 11,
    TT_MODEM = 12,
    TT_SOC = 13,
    /** aidl end */
    TT_MAX = 14,
};

typedef struct _TZ_DATA {
    const char* tzName;
    const char* label;
    int tz_idx;
} TZ_DATA;


#define MAX_COOLING 127

typedef struct _COOLING_DATA {
    CoolingDevice cl_2_0;
    int cl_idx; /* /sys/class/thermal/cooling_device{$cl_idx} */
} COOLING_DATA;

/** Device cooling device types */
enum coolingType {
    CT_UNKNOWN = -1,
    CT_FAN = 0,
    CT_BATTERY = 1,
    CT_CPU = 2,
    CT_GPU = 3,
    CT_MODEM = 4,
    CT_NPU = 5,
    CT_COMPONENT = 6,
    /** Add aidl definitions */
    CT_TPU = 7,
    CT_POWER_AMPLIFIER = 8,
    CT_DISPLAY = 9,
    CT_SPEAKER = 10,
    /** aidl end */
    CT_MAX = 11,
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
