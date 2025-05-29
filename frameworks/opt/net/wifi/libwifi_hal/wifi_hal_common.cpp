/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hardware_legacy/wifi.h"
#include "hardware_legacy/rk_wifi.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <android-base/logging.h>
#include <cutils/misc.h>
#include <cutils/properties.h>
#include <sys/syscall.h>

extern "C" int init_module(void *, unsigned long, const char *);
extern "C" int delete_module(const char *, unsigned int);
#define WIFI_MODULE_PATH                "/vendor/lib/modules/"
#define MLAN_DRIVER_MODULE_PATH          WIFI_MODULE_PATH"mlan.ko"
#define BCM_STATIC_BUF_MODULE_PATH	 WIFI_MODULE_PATH"dhd_static_buf.ko"
#define AIC8800_BSP_DRIVER_MODULE_PATH   WIFI_MODULE_PATH"aic8800_bsp.ko"
#define SKW6160_SDIO_MODULE_PATH	 WIFI_MODULE_PATH"skw_sdio.ko"
#define SKW6160_BOOTCOMS_MODULE_PATH	 WIFI_MODULE_PATH"skw_bootcoms.ko"
#define MVL_DRIVER_MODULE_NAME           "sd8xxx"
#define BCM_DRIVER_MODULE_NAME           "bcmdhd"
#define AIC8800_DRIVER_MODULE_NAME	 "aic8800"
#define SKW6160_DRIVER_MODULE_NAME       "skw"

#ifndef WIFI_DRIVER_FW_PATH_STA
#define WIFI_DRIVER_FW_PATH_STA NULL
#endif
#ifndef WIFI_DRIVER_FW_PATH_AP
#define WIFI_DRIVER_FW_PATH_AP NULL
#endif
#ifndef WIFI_DRIVER_FW_PATH_P2P
#define WIFI_DRIVER_FW_PATH_P2P NULL
#endif

#ifndef WIFI_DRIVER_MODULE_ARG
#define WIFI_DRIVER_MODULE_ARG ""
#endif

static const char DRIVER_PROP_NAME[] = "wlan.driver.status";
#ifndef WIFI_DRIVER_MODULE_PATH
#define WIFI_DRIVER_MODULE_PATH		"/vendor/lib/modules/none"
#endif
#ifndef WIFI_DRIVER_MODULE_NAME
#define WIFI_DRIVER_MODULE_NAME    "wlan"
#endif
#ifdef WIFI_DRIVER_MODULE_PATH
//static const char DRIVER_MODULE_NAME[] = WIFI_DRIVER_MODULE_NAME;
static const char DRIVER_MODULE_TAG[] = WIFI_DRIVER_MODULE_NAME " ";
//static const char DRIVER_MODULE_PATH[] = WIFI_DRIVER_MODULE_PATH;
//static const char DRIVER_MODULE_ARG[] = WIFI_DRIVER_MODULE_ARG;
static const char MODULE_FILE[] = "/proc/modules";
#endif

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
int kDriverStateAccessRetrySleepMillis = 200;
#endif

enum {
    KERNEL_VERSION_UNKNOWN = 1,
    KERNEL_VERSION_3_10,
    KERNEL_VERSION_4_4,
    KERNEL_VERSION_4_19,
    KERNEL_VERSION_5_10,
    KERNEL_VERSION_6_1,
};

static char wifi_type[64] = {0};
extern "C" int check_wifi_chip_type_string(char *type);

int get_kernel_version(void)
{
    int fd, version = 0;
    char buf[64];

    fd = open("/proc/version", O_RDONLY);
    if (fd < 0) {
        PLOG(ERROR) << "Can't open '/proc/version', errno = " << errno;
        goto fderror;
    }
    memset(buf, 0, 64);
    if( 0 == read(fd, buf, 63) ){
        PLOG(ERROR) << "read '/proc/version' failed";
        close(fd);
        goto fderror;
    }
    close(fd);
    if (strstr(buf, "Linux version 3.10") != NULL) {
        version = KERNEL_VERSION_3_10;
        PLOG(ERROR) << "Kernel version is 3.10.";
    } else if (strstr(buf, "Linux version 4.4") != NULL) {
	version = KERNEL_VERSION_4_4;
	PLOG(ERROR) << "Kernel version is 4.4.";
    } else if (strstr(buf, "Linux version 4.19") != NULL) {
	version = KERNEL_VERSION_4_19;
	PLOG(ERROR) << "Kernel version is 4.19.";
    } else if (strstr(buf, "Linux version 5.10") != NULL) {
	version = KERNEL_VERSION_5_10;
	PLOG(ERROR) << "Kernel version is 5.10";
    } else if (strstr(buf, "Linux version 6.1") != NULL) {
	version = KERNEL_VERSION_6_1;
	PLOG(ERROR) << "Kernel version is 6.1";
    } else {
        version = KERNEL_VERSION_UNKNOWN;
        PLOG(ERROR) << "Kernel version unknown.";
    }

    return version;

fderror:
    return -1;
}

/* 0 - not ready; 1 - ready. */
int check_wireless_ready(void)
{
	char line[1024];
	FILE *fp = NULL;

	if ((get_kernel_version() == KERNEL_VERSION_4_4)
			|| (get_kernel_version() == KERNEL_VERSION_4_19)
			|| (get_kernel_version() == KERNEL_VERSION_5_10)
			|| (get_kernel_version() == KERNEL_VERSION_6_1)) {
		fp = fopen("/proc/net/dev", "r");
		if (fp == NULL) {
			PLOG(ERROR) << "Couldn't open /proc/net/dev";
			return 0;
		}
	} else {
		fp = fopen("/proc/net/wireless", "r");
		if (fp == NULL) {
			PLOG(ERROR) << "Couldn't open /proc/net/wireless";
			return 0;
		}
	}

	while(fgets(line, 1024, fp)) {
		if ((strstr(line, "wlan0:") != NULL) || (strstr(line, "p2p0:") != NULL)) {
			PLOG(ERROR) << "Wifi driver is ready for now...";
			property_set(DRIVER_PROP_NAME, "ok");
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);

	PLOG(ERROR) << "Wifi driver is not ready.";
	return 0;
}

static int insmod(const char *filename, const char *args) {
  int ret;
  int fd;

  fd = TEMP_FAILURE_RETRY(open(filename, O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open " << filename;
    return -1;
  }

  ret = syscall(__NR_finit_module, fd, args, 0);

  close(fd);
  if (ret < 0) {
    PLOG(ERROR) << "finit_module return: " << ret;
  }

  return ret;
}

static int rmmod(const char *modname) {
  int ret = -1;
  int maxtry = 10;

  while (maxtry-- > 0) {
    ret = delete_module(modname, O_NONBLOCK | O_EXCL);
    if (ret < 0 && errno == EAGAIN)
      usleep(500000);
    else
      break;
  }

  if (ret != 0)
    PLOG(DEBUG) << "Unable to unload driver module '" << modname << "'";
  return ret;
}

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
int wifi_change_driver_state(const char *state) {
  int len;
  int fd;
  int ret = 0;
  struct timespec req;
  req.tv_sec = 0;
  req.tv_nsec = kDriverStateAccessRetrySleepMillis * 1000000L;
  int count = 5; /* wait at most 1 second for completion. */

  if (!state) return -1;
  do {
    if (access(WIFI_DRIVER_STATE_CTRL_PARAM, W_OK) == 0)
      break;
    nanosleep(&req, (struct timespec *)NULL);
  } while (--count > 0);
  if (count == 0) {
    PLOG(ERROR) << "Failed to access driver state control param "
                << strerror(errno) << ", " << errno;
    return -1;
  }
  fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_STATE_CTRL_PARAM, O_WRONLY));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open driver state control param";
    return -1;
  }
  len = strlen(state) + 1;
  if (TEMP_FAILURE_RETRY(write(fd, state, len)) != len) {
    PLOG(ERROR) << "Failed to write driver state control param";
    ret = -1;
  }
  close(fd);
  return ret;
}
#endif

int is_wifi_driver_loaded() {
#ifdef WIFI_DRIVER_MODULE_PATH
  FILE *proc;
  char line[sizeof(DRIVER_MODULE_TAG) + 10];
#endif
#if 0
  if (!property_get(DRIVER_PROP_NAME, driver_status, NULL)) {
    return 0; /* driver not loaded */
  }
#endif
if (check_wireless_ready() == 0) {
  /*
   * If the property says the driver is loaded, check to
   * make sure that the property setting isn't just left
   * over from a previous manual shutdown or a runtime
   * crash.
   */
  if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
    PLOG(WARNING) << "Could not open " << MODULE_FILE;
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
  }
  while ((fgets(line, sizeof(line), proc)) != NULL) {
    if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
      fclose(proc);
      return 1;
    }
  }
  fclose(proc);
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
} else {
  return 1;
}
}

int wifi_load_driver() {
#ifdef WIFI_DRIVER_MODULE_PATH
	const char* wifi_ko_path = get_wifi_module_path();
	const char* wifi_ko_arg = get_wifi_module_arg();
	int count = 100;
	if (is_wifi_driver_loaded()) {
		return 0;
	}
	if (wifi_ko_path == NULL) {
		PLOG(ERROR) << "falied to find wifi driver for type=" << wifi_type;
		return -1;
	}

	if (strstr(wifi_ko_path, MVL_DRIVER_MODULE_NAME)) {
		insmod(MLAN_DRIVER_MODULE_PATH, "");
	}

	if (strstr(wifi_ko_path, BCM_DRIVER_MODULE_NAME)) {
		insmod(BCM_STATIC_BUF_MODULE_PATH, "");
	}

	if (strstr(wifi_ko_path, AIC8800_DRIVER_MODULE_NAME)) {
		insmod(AIC8800_BSP_DRIVER_MODULE_PATH, "");
		usleep(200000);
	}

	if (strstr(wifi_ko_path, SKW6160_DRIVER_MODULE_NAME)) {
		insmod(SKW6160_SDIO_MODULE_PATH, "");
		insmod(SKW6160_BOOTCOMS_MODULE_PATH, "");
		usleep(200000);
	}

  if (insmod(wifi_ko_path, wifi_ko_arg) < 0) {
	  return -1;
  }
#endif

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (wifi_change_driver_state(WIFI_DRIVER_STATE_ON) < 0) return -1;
#endif
  while (count-- > 0) {
	  if (is_wifi_driver_loaded()) {
		property_set(DRIVER_PROP_NAME, "ok");
		return 0;
	  }
	  usleep(200000);
  }
  property_set(DRIVER_PROP_NAME, "timeout");
  return -1;
}

int wifi_unload_driver() {
#if 0
  if (!is_wifi_driver_loaded()) {
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
  }

  usleep(200000); /* allow to finish interface down */
#ifdef WIFI_DRIVER_MODULE_PATH
  const char* wifi_ko_name = get_wifi_driver_name();
  if (rmmod(wifi_ko_name) == 0) {
    int count = 20; /* wait at most 10 seconds for completion */
    while (count-- > 0) {
      if (!is_wifi_driver_loaded()) break;
      usleep(500000);
    }
    usleep(500000); /* allow card removal */
    if (count) {
      return 0;
    }
    return -1;
  } else
    return -1;
#else
#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    if (wifi_change_driver_state(WIFI_DRIVER_STATE_OFF) < 0) return -1;
  }
#endif
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
#endif
#endif
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
}

const char *wifi_get_fw_path(int fw_type) {
  switch (fw_type) {
    case WIFI_GET_FW_PATH_STA:
      return WIFI_DRIVER_FW_PATH_STA;
    case WIFI_GET_FW_PATH_AP:
      return WIFI_DRIVER_FW_PATH_AP;
    case WIFI_GET_FW_PATH_P2P:
      return WIFI_DRIVER_FW_PATH_P2P;
  }
  return NULL;
}

int wifi_change_fw_path(const char *fwpath) {
  int len;
  int fd;
  int ret = 0;

  if (wifi_type[0] == 0)
	check_wifi_chip_type_string(wifi_type);
  return ret;
  if (!fwpath) return ret;
  fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_FW_PATH_PARAM, O_WRONLY));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open wlan fw path param";
    return -1;
  }
  len = strlen(fwpath) + 1;
  if (TEMP_FAILURE_RETRY(write(fd, fwpath, len)) != len) {
    PLOG(ERROR) << "Failed to write wlan fw path param";
    ret = -1;
  }
  close(fd);
  return ret;
}
