//
// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "vendor_interface.h"

#define LOG_TAG "android.hardware.bluetooth@1.0-impl"
#include <cutils/properties.h>
#include <utils/Log.h>

#include <dlfcn.h>
#include <fcntl.h>

#include "bluetooth_address.h"
#include "h4_protocol.h"
#include "mct_protocol.h"

static const char* VENDOR_LIBRARY_NAME = "libbt-vendor.so";
static const char* VENDOR_REALTEK_LIBRARY_NAME = "libbt-vendor-realtek.so";
static const char* VENDOR_LIBRARY_SYMBOL_NAME =
    "BLUETOOTH_VENDOR_LIB_INTERFACE";

static const int INVALID_FD = -1;
std::mutex vendor_mutex_;
std::mutex initcb_mutex_;

namespace {

using android::hardware::hidl_vec;
using android::hardware::bluetooth::V1_0::implementation::VendorInterface;

struct {
  tINT_CMD_CBACK cb;
  uint16_t opcode;
} internal_command;

enum {
  VENDOR_STATE_INIT = 1,
  VENDOR_STATE_OPENING,	/* during opening */
  VENDOR_STATE_OPENED,	/* open in fops_open */
  VENDOR_STATE_CLOSING,	/* during closing */
  VENDOR_STATE_CLOSED,	/* closed */

  VENDOR_STATE_MSG_NUM
} ;

uint8_t vstate = VENDOR_STATE_INIT;

// True when LPM is not enabled yet or wake is not asserted.
bool lpm_wake_deasserted;
uint32_t lpm_timeout_ms;
bool recent_activity_flag;

VendorInterface* g_vendor_interface = nullptr;
static VendorInterface vendor_interface;

HC_BT_HDR* WrapPacketAndCopy(uint16_t event, const hidl_vec<uint8_t>& data) {
  size_t packet_size = data.size() + sizeof(HC_BT_HDR);
  HC_BT_HDR* packet = reinterpret_cast<HC_BT_HDR*>(new uint8_t[packet_size]);
  packet->offset = 0;
  packet->len = data.size();
  packet->layer_specific = 0;
  packet->event = event;
  // TODO(eisenbach): Avoid copy here; if BT_HDR->data can be ensured to
  // be the only way the data is accessed, a pointer could be passed here...
  memcpy(packet->data, data.data(), data.size());
  return packet;
}

bool internal_command_event_match(const hidl_vec<uint8_t>& packet) {
  uint8_t event_code = packet[0];
  if (event_code != HCI_COMMAND_COMPLETE_EVENT) {
    ALOGE("%s: Unhandled event type %02X", __func__, event_code);
    return false;
  }

  size_t opcode_offset = HCI_EVENT_PREAMBLE_SIZE + 1;  // Skip num packets.

  uint16_t opcode = packet[opcode_offset] | (packet[opcode_offset + 1] << 8);

  ALOGV("%s internal_command.opcode = %04X opcode = %04x", __func__,
        internal_command.opcode, opcode);
  return opcode == internal_command.opcode;
}

uint8_t transmit_cb(uint16_t opcode, void* buffer, tINT_CMD_CBACK callback) {
  ALOGV("%s opcode: 0x%04x, ptr: %p, cb: %p", __func__, opcode, buffer,
        callback);
  internal_command.cb = callback;
  internal_command.opcode = opcode;
  uint8_t type = HCI_PACKET_TYPE_COMMAND;
  HC_BT_HDR* bt_hdr = reinterpret_cast<HC_BT_HDR*>(buffer);
  VendorInterface::get()->Send(type, bt_hdr->data, bt_hdr->len);
  delete[] reinterpret_cast<uint8_t*>(buffer);
  return true;
}

void firmware_config_cb(bt_vendor_op_result_t result) {
  ALOGV("%s result: %d", __func__, result);
  VendorInterface::get()->OnFirmwareConfigured(result);
}

void sco_config_cb(bt_vendor_op_result_t result) {
  ALOGD("%s result: %d", __func__, result);
}

void low_power_mode_cb(bt_vendor_op_result_t result) {
  ALOGD("%s result: %d", __func__, result);
}

void sco_audiostate_cb(bt_vendor_op_result_t result) {
  ALOGD("%s result: %d", __func__, result);
}

void* buffer_alloc_cb(int size) {
  void* p = new uint8_t[size];
  ALOGV("%s pts: %p, size: %d", __func__, p, size);
  return p;
}

void buffer_free_cb(void* buffer) {
  ALOGV("%s ptr: %p", __func__, buffer);
  delete[] reinterpret_cast<uint8_t*>(buffer);
}

void epilog_cb(bt_vendor_op_result_t result) {
  ALOGD("%s result: %d", __func__, result);
}

void a2dp_offload_cb(bt_vendor_op_result_t result, bt_vendor_opcode_t op,
                     uint8_t av_handle) {
  ALOGD("%s result: %d, op: %d, handle: %d", __func__, result, op, av_handle);
}

const bt_vendor_callbacks_t lib_callbacks = {
    sizeof(lib_callbacks), firmware_config_cb, sco_config_cb,
    low_power_mode_cb,     sco_audiostate_cb,  buffer_alloc_cb,
    buffer_free_cb,        transmit_cb,        epilog_cb,
    a2dp_offload_cb};

}  // namespace

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

class FirmwareStartupTimer {
 public:
  FirmwareStartupTimer() : start_time_(std::chrono::steady_clock::now()) {}

  ~FirmwareStartupTimer() {
    std::chrono::duration<double> duration =
        std::chrono::steady_clock::now() - start_time_;
    double s = duration.count();
    if (s == 0) return;
    ALOGI("Firmware configured in %.3fs", s);
  }

 private:
  std::chrono::steady_clock::time_point start_time_;
};

bool VendorInterface::Initialize(
    InitializeCompleteCallback initialize_complete_cb,
    PacketReadCallback event_cb, PacketReadCallback acl_cb,
    PacketReadCallback sco_cb, PacketReadCallback iso_cb) {
  ALOGI("%s: VendorInterface::Initialize", __func__);
  g_vendor_interface = &vendor_interface;
  return g_vendor_interface->Open(initialize_complete_cb, event_cb, acl_cb,
                                  sco_cb, iso_cb);
}

void VendorInterface::Shutdown() {
  LOG_ALWAYS_FATAL_IF(!g_vendor_interface, "%s: No Vendor interface!",
                      __func__);
  ALOGI("%s: VendorInterface::Shutdown", __func__);
  g_vendor_interface->Close();
}

VendorInterface* VendorInterface::get() { return g_vendor_interface; }

static char wifi_type[64] = {0};
extern "C" int check_wifi_chip_type_string(char *type);


bool VendorInterface::Open(InitializeCompleteCallback initialize_complete_cb,
                           PacketReadCallback event_cb,
                           PacketReadCallback acl_cb,
                           PacketReadCallback sco_cb,
                           PacketReadCallback iso_cb) {
  {
    //std::unique_lock<std::mutex> guard(vendor_mutex_);
    if (vstate == VENDOR_STATE_OPENED) {
      ALOGW("VendorInterface opened!");
      return true;
    }

    if ((vstate == VENDOR_STATE_CLOSING) ||
        (vstate == VENDOR_STATE_OPENING)) {
      ALOGW("VendorInterface open/close is on-going !");
      return true;
    }

    vstate = VENDOR_STATE_OPENING;
    ALOGI("%s: VendorInterface::Open", __func__);

    initialize_complete_cb_ = initialize_complete_cb;
	char vendor_lib_name[32];
    // Initialize vendor interface
  if (wifi_type[0] == 0)
    check_wifi_chip_type_string(wifi_type);
  if ((0 == strncmp(wifi_type, "RTL", 3)) ||
      (0 == strncmp(wifi_type, "SSV", 3))) { // for ssv6051 wifi + rtl8761 bt
    strcpy(vendor_lib_name, VENDOR_REALTEK_LIBRARY_NAME);
  } else {
    strcpy(vendor_lib_name, VENDOR_LIBRARY_NAME);
  }

  ALOGD("%s: %s", __func__, vendor_lib_name);
    lib_handle_ = dlopen(vendor_lib_name, RTLD_NOW);
    if (!lib_handle_) {
      ALOGE("%s unable to open %s (%s)", __func__, VENDOR_LIBRARY_NAME,
            dlerror());
      return false;
    }

    lib_interface_ = reinterpret_cast<bt_vendor_interface_t*>(
        dlsym(lib_handle_, VENDOR_LIBRARY_SYMBOL_NAME));
    if (!lib_interface_) {
      ALOGE("%s unable to find symbol %s in %s (%s)", __func__,
            VENDOR_LIBRARY_SYMBOL_NAME, vendor_lib_name, dlerror());
      return false;
    }

  // Get the local BD address

    uint8_t local_bda[BluetoothAddress::kBytes] = {0, 0, 0, 0, 0, 0};
    if (!BluetoothAddress::get_local_address(local_bda)) {
      // BT driver will get BD address from NVRAM for MTK solution
      ALOGW("%s: No pre-set Bluetooth Address!", __func__);
    }
    int status = lib_interface_->init(&lib_callbacks, (unsigned char*)local_bda);
    if (status) {
      ALOGE("%s unable to initialize vendor library: %d", __func__, status);
      return false;
    }

    ALOGD("%s vendor library loaded", __func__);

  // Power on the controller

    int power_state = BT_VND_PWR_ON;
    lib_interface_->op(BT_VND_OP_POWER_CTRL, &power_state);

  // Get the UART socket(s)

    int fd_list[CH_MAX] = {0};
    int fd_count = lib_interface_->op(BT_VND_OP_USERIAL_OPEN, &fd_list);

    if (fd_count < 1 || fd_count > CH_MAX - 1) {
      ALOGE("%s: fd_count %d is invalid!", __func__, fd_count);
      return false;
    }

    for (int i = 0; i < fd_count; i++) {
      if (fd_list[i] == INVALID_FD) {
        ALOGE("%s: fd %d is invalid!", __func__, fd_list[i]);
        return false;
      }
    }

    event_cb_ = event_cb;
    PacketReadCallback intercept_events = [this](const hidl_vec<uint8_t>& event) {
      HandleIncomingEvent(event);
    };

    if (fd_count == 1) {
      hci::H4Protocol* h4_hci =
          new hci::H4Protocol(fd_list[0], intercept_events, acl_cb, sco_cb, iso_cb);
      fd_watcher_.WatchFdForNonBlockingReads(
          fd_list[0], [h4_hci](int fd) { h4_hci->OnDataReady(fd); });
      hci_ = h4_hci;
    } else {
      hci::MctProtocol* mct_hci =
          new hci::MctProtocol(fd_list, intercept_events, acl_cb);
      fd_watcher_.WatchFdForNonBlockingReads(
          fd_list[CH_EVT], [mct_hci](int fd) { mct_hci->OnEventDataReady(fd); });
      fd_watcher_.WatchFdForNonBlockingReads(
          fd_list[CH_ACL_IN],
          [mct_hci](int fd) { mct_hci->OnAclDataReady(fd); });
      hci_ = mct_hci;
    }

    vstate = VENDOR_STATE_OPENED;
    ALOGI("%s: VendorInterface::Open done!!!", __func__);

  // Initially, the power management is off.
    lpm_wake_deasserted = true;

  // Start configuring the firmware
    firmware_startup_timer_ = new FirmwareStartupTimer();
    lib_interface_->op(BT_VND_OP_FW_CFG, nullptr);


  }  // vendor_mutex_ done
  return true;
}

void VendorInterface::Close() {
  // These callbacks may send HCI events (vendor-dependent), so make sure to
  // StopWatching the file descriptor after this.

  if (vstate != VENDOR_STATE_OPENED) {
    ALOGW("VendorInterface is not allow close(%d)", vstate);
    return;
  }
  vstate = VENDOR_STATE_CLOSING;
  ALOGI("%s: VendorInterface::Close", __func__);

  if (lib_interface_ != nullptr) {
    lib_interface_->cleanup();
    bt_vendor_lpm_mode_t mode = BT_VND_LPM_DISABLE;
    lib_interface_->op(BT_VND_OP_LPM_SET_MODE, &mode);
  }

  {
    std::unique_lock<std::mutex> guard(vendor_mutex_);

    fd_watcher_.StopWatchingFileDescriptors();
    if (hci_ != nullptr) {
      delete hci_;
      hci_ = nullptr;
    }

    if (lib_interface_ != nullptr) {
      lib_interface_->op(BT_VND_OP_USERIAL_CLOSE, nullptr);

      int power_state = BT_VND_PWR_OFF;
      lib_interface_->op(BT_VND_OP_POWER_CTRL, &power_state);

      lib_interface_ = nullptr;
    }

    if (lib_handle_ != nullptr) {
      dlclose(lib_handle_);
      lib_handle_ = nullptr;
    }

    if (firmware_startup_timer_ != nullptr) {
      delete firmware_startup_timer_;
      firmware_startup_timer_ = nullptr;
    }
    vstate = VENDOR_STATE_CLOSED;
  }  // vendor_mutex_ done
  ALOGI("%s: VendorInterface::Close done!!!", __func__);
}

size_t VendorInterface::Send(uint8_t type, const uint8_t* data, size_t length) {
  {
    std::unique_lock<std::mutex> guard(vendor_mutex_);

    if (vstate != VENDOR_STATE_OPENED) {
      ALOGW("VendorInterface is not open yet(%d)!", vstate);
      return 0;
    }
    ALOGV("%s: VendorInterface::Send", __func__);

    if (lib_interface_ == nullptr) {
      ALOGE("lib_interface_ is null");
      return 0;
    }
    recent_activity_flag = true;
    if (lpm_wake_deasserted == true) {
      // Restart the timer.
      fd_watcher_.ConfigureTimeout(std::chrono::milliseconds(lpm_timeout_ms),
                                 [this]() { OnTimeout(); });
      // Assert wake.
      lpm_wake_deasserted = false;
      bt_vendor_lpm_wake_state_t wakeState = BT_VND_LPM_WAKE_ASSERT;
      lib_interface_->op(BT_VND_OP_LPM_WAKE_SET_STATE, &wakeState);
      ALOGV("%s: Sent wake before (%02x)", __func__, data[0] | (data[1] << 8));
    }

    return hci_ ? hci_->Send(type, data, length) : 0;
  }  // vendor_mutex_ done
}

void VendorInterface::OnFirmwareConfigured(uint8_t result) {
  ALOGD("%s result: %d", __func__, result);

  if (firmware_startup_timer_ != nullptr) {
    delete firmware_startup_timer_;
    firmware_startup_timer_ = nullptr;
  }

  {
    std::unique_lock<std::mutex> guard(initcb_mutex_);
    ALOGD("%s OnFirmwareConfigured get lock", __func__);
    if (initialize_complete_cb_ != nullptr) {
      LOG_ALWAYS_FATAL_IF((result != 0),
          "%s: Failed to init firmware!", __func__);
      initialize_complete_cb_(result == 0);
    }
  }  // initcb_mutex_ done

  if (lib_interface_ != nullptr) {
    lib_interface_->op(BT_VND_OP_GET_LPM_IDLE_TIMEOUT, &lpm_timeout_ms);
    ALOGI("%s: lpm_timeout_ms %d", __func__, lpm_timeout_ms);

    bt_vendor_lpm_mode_t mode = BT_VND_LPM_ENABLE;
    lib_interface_->op(BT_VND_OP_LPM_SET_MODE, &mode);

    ALOGD("%s Calling StartLowPowerWatchdog()", __func__);
    fd_watcher_.ConfigureTimeout(std::chrono::milliseconds(lpm_timeout_ms),
                                 [this]() { OnTimeout(); });
  }
  else {
    ALOGE("lib_interface_ is null");
  }

  initialize_complete_cb_ = nullptr;
}

void VendorInterface::OnTimeout() {
  ALOGV("%s", __func__);
  if (recent_activity_flag == false) {
    lpm_wake_deasserted = true;
    bt_vendor_lpm_wake_state_t wakeState = BT_VND_LPM_WAKE_DEASSERT;
    lib_interface_->op(BT_VND_OP_LPM_WAKE_SET_STATE, &wakeState);
    fd_watcher_.ConfigureTimeout(std::chrono::seconds(0), []() {
      ALOGE("Zero timeout! Should never happen.");
    });
  }
  recent_activity_flag = false;
}

void VendorInterface::HandleIncomingEvent(const hidl_vec<uint8_t>& hci_packet) {
  if (internal_command.cb != nullptr &&
      internal_command_event_match(hci_packet)) {
    HC_BT_HDR* bt_hdr = WrapPacketAndCopy(HCI_PACKET_TYPE_EVENT, hci_packet);

    // The callbacks can send new commands, so don't zero after calling.
    tINT_CMD_CBACK saved_cb = internal_command.cb;
    internal_command.cb = nullptr;
    saved_cb(bt_hdr);
  } else {
    event_cb_(hci_packet);
  }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
