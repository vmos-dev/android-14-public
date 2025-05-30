/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <aidl/android/hardware/usb/gadget/BnUsbGadget.h>
#include <aidl/android/hardware/usb/gadget/BnUsbGadgetCallback.h>
#include <aidl/android/hardware/usb/gadget/GadgetFunction.h>
#include <aidl/android/hardware/usb/gadget/IUsbGadget.h>
#include <aidl/android/hardware/usb/gadget/IUsbGadgetCallback.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <utils/Log.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include <rkusb/UsbGadgetAidlCommon.h>

namespace aidl {
namespace android {
namespace hardware {
namespace usb {
namespace gadget {

using ::aidl::android::hardware::usb::gadget::GadgetFunction;
using ::aidl::android::hardware::usb::gadget::IUsbGadgetCallback;
using ::aidl::android::hardware::usb::gadget::IUsbGadget;
using ::aidl::android::hardware::usb::gadget::Status;
using ::aidl::android::hardware::usb::gadget::UsbSpeed;
using ::android::base::GetProperty;
using ::android::base::SetProperty;
using ::android::base::ParseUint;
using ::android::base::unique_fd;
using ::android::base::ReadFileToString;
using ::android::base::Trim;
using ::android::base::WriteStringToFile;
using ::ndk::ScopedAStatus;
using ::std::shared_ptr;
using ::std::string;

using ::aidl::android::hardware::usb::gadget::MonitorFfs;

constexpr char kGadgetNameProp[] = "sys.usb.controller";
constexpr char kProcInterruptsPath[] = "/proc/interrupts";
constexpr char kProcIrqPath[] = "/proc/irq/";
constexpr char kSmpAffinityList[] = "/smp_affinity_list";

#define BIG_CORE "6"
#define MEDIUM_CORE "4"

#define POWER_SUPPLY_PATH	"/sys/class/power_supply/usb/"
#define USB_PORT0_PATH		"/sys/class/typec/port0/"

#define CURRENT_USB_TYPE_PATH			POWER_SUPPLY_PATH	"usb_type"
#define CURRENT_USB_POWER_OPERATION_MODE_PATH	USB_PORT0_PATH		"power_operation_mode"

struct UsbGadget : public BnUsbGadget {
    UsbGadget();

    // Makes sure that only one request is processed at a time.
    std::mutex mLockSetCurrentFunction;
    std::string mGadgetIrqPath;
    std::string mUdcController;
    int64_t mCurrentUsbFunctions;
    bool mCurrentUsbFunctionsApplied;
    UsbSpeed mUsbSpeed;

    ScopedAStatus setCurrentUsbFunctions(int64_t functions,
            const shared_ptr<IUsbGadgetCallback> &callback,
            int64_t timeoutMs, int64_t in_transactionId) override;

    ScopedAStatus getCurrentUsbFunctions(const shared_ptr<IUsbGadgetCallback> &callback,
	    int64_t in_transactionId) override;

    ScopedAStatus reset(const shared_ptr<IUsbGadgetCallback> &callback,
            int64_t in_transactionId) override;

    ScopedAStatus getUsbSpeed(const shared_ptr<IUsbGadgetCallback> &callback,
	    int64_t in_transactionId) override;

    binder_status_t dump(int fd, const char **args, unsigned int numArgs) override;

  private:
    Status tearDownGadget();
    Status getUsbGadgetIrqPath();
    Status setupFunctions(int64_t functions, const shared_ptr<IUsbGadgetCallback> &callback,
            uint64_t timeout, int64_t in_transactionId);
};

}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android
}  // aidl
