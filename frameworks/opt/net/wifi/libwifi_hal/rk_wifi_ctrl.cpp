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

//----rk-code-------
#include "hardware_legacy/rk_wifi.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

#include <android-base/logging.h>
#include <cutils/misc.h>
#include <cutils/properties.h>
//#include <private/android_filesystem_config.h>

static int identify_sucess = -1;
static char recoginze_wifi_chip[64];
static const char USB_DIR[] = "/sys/bus/usb/devices";
static const char SDIO_DIR[]= "/sys/bus/sdio/devices";
static const char PCIE_DIR[]= "/sys/bus/pci/devices";
static const char PREFIX_SDIO[] = "SDIO_ID=";
static const char PREFIX_PCIE[] = "PCI_ID=";
static const char PREFIX_USB[] = "PRODUCT=";

static int invalid_wifi_device_id = -1;
static char wifi_type[64] = {0};

#define WIFI_MODULE_PATH                "/vendor/lib/modules/"
#define RTL8188EU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8188eu.ko"
#define RTL8723BU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8723bu.ko"
#define RTL8723BS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8723bs.ko"
#define RTL8723BS_VQ0_DRIVER_MODULE_PATH WIFI_MODULE_PATH"8723bs-vq0.ko"
#define RTL8723CS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8723cs.ko"
#define RTL8723DS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8723ds.ko"
#define RTL8188FU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8188fu.ko"
#define RTL8822BU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822bu.ko"
#define RTL8822BS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822bs.ko"
#define RTL8189ES_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8189es.ko"
#define RTL8189FS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8189fs.ko"
#define RTL8192DU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8192du.ko"
#define RTL8812AU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8812au.ko"
#define RTL8822BE_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822be.ko"
#define RTL8821CS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8821cs.ko"
#define RTL8822CU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822cu.ko"
#define RTL8822CS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822cs.ko"
#define SSV6051_DRIVER_MODULE_PATH       WIFI_MODULE_PATH"ssv6051.ko"
#define ESP8089_DRIVER_MODULE_PATH       WIFI_MODULE_PATH"esp8089.ko"
#define BCM_DRIVER_MODULE_PATH           WIFI_MODULE_PATH"bcmdhd.ko"
#define MLAN_DRIVER_MODULE_PATH          WIFI_MODULE_PATH"mlan.ko"
#define MVL_DRIVER_MODULE_PATH           WIFI_MODULE_PATH"sd8xxx.ko"
#define RK912_DRIVER_MODULE_PATH         WIFI_MODULE_PATH"rk912.ko"
#define SPRDWL_DRIVER_MODULE_PATH        WIFI_MODULE_PATH"sprdwl_ng.ko"
#define BES2600_DRIVER_MODULE_PATH        WIFI_MODULE_PATH"bes2600.ko"
#define AIC8800_DRIVER_MODULE_PATH        WIFI_MODULE_PATH"aic8800_fdrv.ko"
#define SKW6160_DRIVER_MODULE_PATH       WIFI_MODULE_PATH"skw.ko"
#define CYW88459_DRIVER_MODULE_PATH      WIFI_MODULE_PATH"cyw88459.ko"

#define RTL8188EU_DRIVER_MODULE_NAME     "8188eu"
#define RTL8723BU_DRIVER_MODULE_NAME     "8723bu"
#define RTL8723BS_DRIVER_MODULE_NAME     "8723bs"
#define RTL8723BS_VQ0_DRIVER_MODULE_NAME "8723bs-vq0"
#define RTL8723CS_DRIVER_MODULE_NAME     "8723cs"
#define RTL8723DS_DRIVER_MODULE_NAME     "8723ds"
#define RTL8188FU_DRIVER_MODULE_NAME     "8188fu"
#define RTL8822BU_DRIVER_MODULE_NAME     "8822bu"
#define RTL8822BS_DRIVER_MODULE_NAME     "8822bs"
#define RTL8189ES_DRIVER_MODULE_NAME     "8189es"
#define RTL8189FS_DRIVER_MODULE_NAME     "8189fs"
#define RTL8192DU_DRIVER_MODULE_NAME     "8192du"
#define RTL8812AU_DRIVER_MODULE_NAME     "8812au"
#define RTL8822BE_DRIVER_MODULE_NAME     "8822be"
#define RTL8821CS_DRIVER_MODULE_NAME     "8821cs"
#define RTL8822CU_DRIVER_MODULE_NAME     "8822cu"
#define RTL8822CS_DRIVER_MODULE_NAME     "8822cs"
#define SSV6051_DRIVER_MODULE_NAME       "ssv6051"
#define ESP8089_DRIVER_MODULE_NAME       "esp8089"
#define BCM_DRIVER_MODULE_NAME           "bcmdhd"
#define MVL_DRIVER_MODULE_NAME           "sd8xxx"
#define RK912_DRIVER_MODULE_NAME         "rk912"
#define SPRDWL_DRIVER_MODULE_NAME        "sprdwl"
#define BES2600_DRIVER_MODULE_NAME       "bes2600"
#define AIC8800_DRIVER_MODULE_NAME        "aic8800"
#define SKW6160_DRIVER_MODULE_NAME       "skw"
#define CYW88459_DRIVER_MODULE_NAME       "cyw88459"

#define UNKOWN_DRIVER_MODULE_ARG ""
#define SSV6051_DRIVER_MODULE_ARG "stacfgpath=/vendor/etc/firmware/ssv6051-wifi.cfg"
#define MVL88W8977_DRIVER_MODULE_ARG "drv_mode=1 fw_name=mrvl/sd8977_wlan_v2.bin cal_data_cfg=none cfg80211_wext=0xf"

#define BROADCOM_WIFI_HAL "libwifi-hal-bcm.so"
#define REALTEK_WIFI_HAL "libwifi-hal-rtk.so"
#define SPRD_WIFI_HAL "libwifi-hal-sprd.so"
#define BES_WIFI_HAL "libwifi-hal-bes.so"
#define AIC_WIFI_HAL "libwifi-hal-aic.so"
#define SKW_WIFI_HAL "libwifi-hal-skw.so"

typedef struct _wifi_devices
{
  char wifi_name[64];
  char wifi_vid_pid[64];
} wifi_device;

typedef struct _wifi_file_name
{
  char wifi_name[64];
  char wifi_driver_name[64];
  char wifi_module_path[128];
  char wifi_module_arg[128];
  char wifi_hal_name[64];
}wifi_file_name;

static wifi_device supported_wifi_devices[] = {
	{"RTL8188EU",	"0bda:8179"},
	{"RTL8188EU",	"0bda:0179"},
	{"RTL8723BU",	"0bda:b720"},
	{"RTL8723BS",	"024c:b723"},
	{"RTL8822BS",	"024c:b822"},
	{"RTL8723CS",	"024c:b703"},
	{"RTL8723DS",	"024c:d723"},
	{"RTL8188FU",	"0bda:f179"},
	{"RTL8822BU",	"0bda:b82c"},
	{"RTL8189ES",	"024c:8179"},
	{"RTL8189FS",	"024c:f179"},
	{"RTL8192DU",	"0bda:8194"},
	{"RTL8812AU",	"0bda:8812"},
	{"RTL8821CS",	"024c:c821"},
	{"RTL8822BE",   "10ec:b822"},
        {"RTL8822CU",   "0bda:c82c"},
	{"RTL8822CS",   "024c:c822"},
	{"SSV6051",	"3030:3030"},
	{"ESP8089",	"6666:1111"},
	{"AP6354",	"02d0:4354"},
	{"AP6330",	"02d0:4330"},
	{"AP6356S",	"02d0:4356"},
	{"AP6335",	"02d0:4335"},
	{"AP6212",      "02d0:a9a6"},
	{"AP6255",      "02d0:a9bf"},
	{"AP6275S",     "02d0:aae8"},
	{"AP6275P",     "14e4:449d"},
	{"AP6276P",     "14e4:44a0"},
	{"AP6398S",     "02d0:4359"},
	{"AP6611S",     "06CB:AABF"},
	{"MVL88W8977",	"02df:9145"},
	//{"SPRDWL",	"0000:0000"},
	{"BES2600",	"be57:2002"},
	{"AIC8800",	"5449:0145"},
	{"AIC8800",     "c8a1:0082"},
	{"SKW6160",     "0483:5720"},
	{"SKW6160",     "0483:5721"},
	{"SKW6160",     "0000:0000"},
	{"SKW6160",     "3607:6160"},
	{"CYW88459",    "14e4:4415"},

};

const wifi_file_name module_list[] =
{
	{"RTL8723BU", RTL8723BU_DRIVER_MODULE_NAME, RTL8723BU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8188EU", RTL8188EU_DRIVER_MODULE_NAME, RTL8188EU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8192DU", RTL8192DU_DRIVER_MODULE_NAME, RTL8192DU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8822BU", RTL8822BU_DRIVER_MODULE_NAME, RTL8822BU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8822BS", RTL8822BS_DRIVER_MODULE_NAME, RTL8822BS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8188FU", RTL8188FU_DRIVER_MODULE_NAME, RTL8188FU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8189ES", RTL8189ES_DRIVER_MODULE_NAME, RTL8189ES_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8723BS", RTL8723BS_DRIVER_MODULE_NAME, RTL8723BS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8723CS", RTL8723CS_DRIVER_MODULE_NAME, RTL8723CS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8723DS", RTL8723DS_DRIVER_MODULE_NAME, RTL8723DS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8812AU", RTL8812AU_DRIVER_MODULE_NAME, RTL8812AU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8189FS", RTL8189FS_DRIVER_MODULE_NAME, RTL8189FS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8822BE", RTL8822BE_DRIVER_MODULE_NAME, RTL8822BE_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8821CS", RTL8821CS_DRIVER_MODULE_NAME, RTL8821CS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8822CU", RTL8822CU_DRIVER_MODULE_NAME, RTL8822CU_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"RTL8822CS", RTL8822CS_DRIVER_MODULE_NAME, RTL8822CS_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, REALTEK_WIFI_HAL},
	{"SSV6051",     SSV6051_DRIVER_MODULE_NAME,   SSV6051_DRIVER_MODULE_PATH, SSV6051_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"ESP8089",     ESP8089_DRIVER_MODULE_NAME,   ESP8089_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6335",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6330",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6354",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6356S",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6212",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6255",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6275S",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6275P",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6276P",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6398S",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"AP6611S",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"APXXX",           BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"MVL88W8977",      MVL_DRIVER_MODULE_NAME,       MVL_DRIVER_MODULE_PATH, MVL88W8977_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"RK912",         RK912_DRIVER_MODULE_NAME,     RK912_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
	{"SPRDWL",          SPRDWL_DRIVER_MODULE_NAME, SPRDWL_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, SPRD_WIFI_HAL},
	{"BES2600",          BES2600_DRIVER_MODULE_NAME, BES2600_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BES_WIFI_HAL},
	{"AIC8800",          AIC8800_DRIVER_MODULE_NAME, AIC8800_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, AIC_WIFI_HAL},
	{"SKW6160",     SKW6160_DRIVER_MODULE_NAME,   SKW6160_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, SKW_WIFI_HAL},
	{"CYW88459",   CYW88459_DRIVER_MODULE_NAME,  CYW88459_DRIVER_MODULE_PATH, UNKOWN_DRIVER_MODULE_ARG, BROADCOM_WIFI_HAL},
};

int strncmp_case_insensitive(const char *str1, const char *str2, size_t n) {
	char buf1[n + 1];
	char buf2[n + 1];
	size_t i;

	strncpy(buf1, str1, n);
	strncpy(buf2, str2, n);

	for (i = 0; i < n; ++i) {
		buf1[i] = tolower(buf1[i]);
		buf2[i] = tolower(buf2[i]);
	}

	buf1[n] = '\0';
	buf2[n] = '\0';

	return strncmp(buf1, buf2, n);
}

int get_wifi_device_id(const char *bus_dir, const char *prefix)
{
	int idnum;
	int i = 0;
	int ret = invalid_wifi_device_id;
	DIR *dir;
	struct dirent *next;
	FILE *fp = NULL;
	idnum = sizeof(supported_wifi_devices) / sizeof(supported_wifi_devices[0]);
	dir = opendir(bus_dir);
	if (!dir) {
		PLOG(ERROR) << "open dir failed:" << strerror(errno);
		return invalid_wifi_device_id;
	}

	while ((next = readdir(dir)) != NULL) {
		char line[256];
		char uevent_file[256] = {0};
		sprintf(uevent_file, "%s/%s/uevent", bus_dir, next->d_name);
		PLOG(INFO) << "uevent path:" << uevent_file;
		fp = fopen(uevent_file, "r");
		if (NULL == fp) {
			continue;
		}

		while (fgets(line, sizeof(line), fp)) {
			char *pos = NULL;
			int product_vid = 0;
			int product_did = 0;
			int producd_bcddev = 0;
			char temp[10] = {0};
			pos = strstr(line, prefix);
			PLOG(DEBUG) << "line:" << line << ", prefix:" << prefix << ".";
			if (pos != NULL) {
				if (strncmp(bus_dir, USB_DIR, sizeof(USB_DIR)) == 0)
					sscanf(pos + 8, "%x/%x/%x", &product_vid, &product_did, &producd_bcddev);
				else if (strncmp(bus_dir, SDIO_DIR, sizeof(SDIO_DIR)) == 0)
					sscanf(pos + 8, "%x:%x", &product_vid, &product_did);
				else if (strncmp(bus_dir, PCIE_DIR, sizeof(PCIE_DIR)) == 0)
					sscanf(pos + 7, "%x:%x", &product_vid, &product_did);
				else
					return invalid_wifi_device_id;

				sprintf(temp, "%04x:%04x", product_vid, product_did);
				PLOG(ERROR) << "pid:vid :" << temp;
				for (i = 0; i < idnum; i++) {
					if (0 == strncmp_case_insensitive(temp, supported_wifi_devices[i].wifi_vid_pid, 9)) {
						PLOG(ERROR) << "found device pid:vid :" << temp;
						strcpy(recoginze_wifi_chip, supported_wifi_devices[i].wifi_name);
						identify_sucess = 1 ;
						ret = 0;
						fclose(fp);
						goto ready;
					}
				}
			}
		}
		fclose(fp);
	}

	ret = invalid_wifi_device_id;
ready:
	closedir(dir);
	PLOG(DEBUG) << "wifi detectd return ret:" << ret;
	return ret;
}
int check_wifi_chip_type_string(char *type)
{
	if (identify_sucess == -1) {
		if (get_wifi_device_id(SDIO_DIR, PREFIX_SDIO) == 0)
			PLOG(INFO) << "SDIO WIFI identify sucess";
		else if (get_wifi_device_id(USB_DIR, PREFIX_USB) == 0)
			PLOG(INFO) << "USB WIFI identify sucess";
		else if (get_wifi_device_id(PCIE_DIR, PREFIX_PCIE) == 0)
			PLOG(INFO) << "PCIE WIFI identify sucess";
		else {
			PLOG(ERROR) << "maybe there is no usb wifi or sdio or pcie wifi,set default wifi module Brocom APXXX";
			strcpy(recoginze_wifi_chip, "APXXX");
			identify_sucess = 1 ;
		}
	}

	strcpy(type, recoginze_wifi_chip);
	PLOG(ERROR) << "check_wifi_chip_type_string : " << type;
	return 0;
}

const char *get_wifi_driver_name(void)
{
	if (wifi_type[0] == 0) {
		check_wifi_chip_type_string(wifi_type);
	}
	for (int i = 0; i< (int)(sizeof(module_list) / sizeof(module_list[0])); i++) {
		if (strncmp(wifi_type, module_list[i].wifi_name, strlen(wifi_type)) == 0) {
			PLOG(ERROR) << "matched wifi_driver_name: " << module_list[i].wifi_driver_name;
			return module_list[i].wifi_driver_name;
		}
	}
	return NULL;
}

const char *get_wifi_module_path(void)
{
	if (wifi_type[0] == 0) {
		check_wifi_chip_type_string(wifi_type);
	}

	for (int i = 0; i< (int)(sizeof(module_list) / sizeof(module_list[0])); i++) {
		if (strncmp(wifi_type, module_list[i].wifi_name, strlen(wifi_type)) == 0) {
			PLOG(ERROR) << "matched wifi_module_path: " << module_list[i].wifi_module_path;
			return module_list[i].wifi_module_path;
		}
	}
	return NULL;
}

const char *get_wifi_module_arg(void)
{
	if (wifi_type[0] == 0) {
		check_wifi_chip_type_string(wifi_type);
	}
	for (int i = 0; i< (int)(sizeof(module_list) / sizeof(module_list[0])); i++) {
		if (strncmp(wifi_type, module_list[i].wifi_name, strlen(wifi_type)) == 0) {
			PLOG(ERROR) << "matched wifi_module_arg: " << module_list[i].wifi_module_arg;
			return module_list[i].wifi_module_arg;
		}
	}
	return NULL;
}

const char *get_wifi_hal_name(void)
{
	if (wifi_type[0] == 0) {
		check_wifi_chip_type_string(wifi_type);
	}
	for (int i = 0; i< (int)(sizeof(module_list) / sizeof(module_list[0])); i++) {
		if (strncmp(wifi_type, module_list[i].wifi_name, strlen(wifi_type)) == 0) {
			PLOG(ERROR) << "matched wifi_hal_name: " << module_list[i].wifi_hal_name;
			return module_list[i].wifi_hal_name;
		}
	}
	return NULL;
}
//----rk-code-------
