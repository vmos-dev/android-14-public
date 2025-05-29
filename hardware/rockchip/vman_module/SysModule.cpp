/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * VMAN -- SysModule
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <log/log.h>
#include <rockchip/hardware/outputmanager/1.0/IRkOutputManager.h>
#include <dirent.h>
#include "SysModule.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "vman_ays"
#endif
/*********** Private definition *************/
#define UN_USED(x) (void)(x)
#define DEFAULT_CSI_PATH    "/dev/v4l-subdev2"
#define DEFAULT_HDMIRX_PATH "/dev/video20"
#define RKMODULE_GET_HDMI_MODE       \
        _IOR('V', BASE_VIDIOC_PRIVATE + 34, __u32)

/*********** Private variable *************/
std::string gDevicePath = DEFAULT_CSI_PATH;
const int kMaxDevicePathLen = 256;
const char* kDevicePath = "/dev/";
char v4l2SubDevPath[kMaxDevicePathLen];
char kPrefix[] = "video";
const int kPrefixLen = sizeof(kPrefix) - 1;
const char kCsiPrefix[] = "v4l-subdev";
const int kCsiPrefixLen = sizeof(kCsiPrefix) - 1;
const char kHdmiNodeName[] = "rk_hdmirx";
/******************************************/
/*
 * SysModule init
 * hdmi-in find device path, check hdmirx or mipi-csi.
 * @param data: ui_device_config_t
 * @return int 1: success, 0: fail
 */
int sysmodule_init(ui_device_type eType)
{
	int ret, videofd;
	DIR* devdir = opendir(kDevicePath);
	struct dirent* de;

	if(devdir == 0) {
		ALOGE("%s: cannot open %s! Exiting threadloop", __FUNCTION__, kDevicePath);
		return -1;
	}

	while ((de = readdir(devdir)) != 0) {
		if (eType == HDMIIN_TYPE_HDMIRX && !strncmp(kPrefix, de->d_name, kPrefixLen)) {
			ALOGI("v4l device %s found\n", de->d_name);
			char v4l2DevicePath[kMaxDevicePathLen];
			char v4l2DeviceDriver[16];
			char gadget_video[100] = {0};

			sprintf(gadget_video, "/sys/class/video4linux/%s/function_name\n", de->d_name);
			if (access(gadget_video, F_OK) == 0) {
				ALOGI("/dev/%s is uvc gadget device, don't open it!\n", de->d_name);
				continue;
			}
			snprintf(v4l2DevicePath, kMaxDevicePathLen,"%s%s", kDevicePath, de->d_name);
			videofd = open(v4l2DevicePath, O_RDWR);

			struct v4l2_capability cap;
			if (videofd < 0){
				ALOGE("%s open v4l2DevicePath:%x failed!\n", __FUNCTION__, videofd);
				continue;
			} else {
				ALOGI("%s open device %s successful.\n", __FUNCTION__, v4l2DevicePath);
				ret = ioctl(videofd, VIDIOC_QUERYCAP, &cap);
				if (ret < 0) {
					ALOGE("VIDIOC_QUERYCAP Failed, error\n");
					close(videofd);
					continue;
				}
			}
			snprintf(v4l2DeviceDriver, 16,"%s",cap.driver);
			ALOGI("VIDIOC_QUERYCAP driver=%s,%s\n", cap.driver,v4l2DeviceDriver);
			ALOGI("VIDIOC_QUERYCAP card=%s\n", cap.card);

			if(!strncmp(kHdmiNodeName, v4l2DeviceDriver, sizeof(kHdmiNodeName)-1)){
				gDevicePath = v4l2DevicePath;
				close(videofd);
				return 0;
			} else {
				close(videofd);
				ALOGI("isnot hdmirx,VIDIOC_QUERYCAP driver=%s\n", cap.driver);
			}
		} else if (eType == HDMIIN_TYPE_MIPICSI && !strncmp(kCsiPrefix, de->d_name, kCsiPrefixLen)) {
			ALOGI(" v4l device %s found", de->d_name);
			snprintf(v4l2SubDevPath, kMaxDevicePathLen,"%s%s", kDevicePath, de->d_name);
			videofd = open(v4l2SubDevPath, O_RDWR);
			if (videofd < 0) {
				ALOGE("[%s %d] mHinDevEventHandle:%x\n", __FUNCTION__, __LINE__, videofd);
				continue;
			} else {
				ALOGI("%s open device %s successful.\n", __FUNCTION__, v4l2SubDevPath);
				uint32_t ishdmi = 0;
				ret = ioctl(videofd, RKMODULE_GET_HDMI_MODE, (void*)&ishdmi);
				if (ret < 0 || !ishdmi) {
					ALOGD("RKMODULE_GET_HDMI_MODE %s failed, ret=%d, ishdmi=%d\n", v4l2SubDevPath, ret, ishdmi);
					close(videofd);
				} else {
					gDevicePath = v4l2SubDevPath;
					ALOGI("find success dev: %s\n", v4l2SubDevPath);
					close(videofd);
					return 0;
				}
			}
		}
	}
	return -1;
}

#define PROPERTY_MEAN_LUMA "vendor.tvinput.rkpq.mean.luma"
#define PIXEL_SHIFT_PATH "/sys/class/drm/card0/video_port%d/pixel_shift"
#define DEFAULT_OVERSCAN_VALUE 100

using namespace rockchip::hardware::outputmanager::V1_0;

using ::rockchip::hardware::outputmanager::V1_0::IRkOutputManager;
using ::rockchip::hardware::outputmanager::V1_0::Result;
using android::hardware::hidl_handle;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::Void;
using ::android::sp;

sp<IRkOutputManager> mComposer = nullptr;

/*
 * Get input timing information
 * @param data: ui_stream_config_t
 * @return Getresult [0: successfully, <0: failure]
 */

static void get_service() {
	if (mComposer == nullptr) {
		mComposer = IRkOutputManager::getService();
	}
}

static int get_input_stream_config(struct sys_hal_module *module, ui_stream_config_t* data)
{
    int ret, fd = -1;
    int interlace = 0, fps = 0;
    char *dev = const_cast<char*>(gDevicePath.c_str());
    struct v4l2_dv_timings dv_timings;

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, VIDIOC_SUBDEV_QUERY_DV_TIMINGS, &dv_timings);
    if (ret < 0) {
            ALOGE("VIDIOC_SUBDEV_QUERY_DV_TIMINGS fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    ret = ioctl(fd, RK_HDMIRX_CMD_GET_SCAN_MODE, &interlace);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_SCAN_MODE fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    ret = ioctl(fd, RK_HDMIRX_CMD_GET_FPS, &fps);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_FPS fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    data->width = dv_timings.bt.width;
    data->height = dv_timings.bt.height;
    data->interlace = interlace;
    data->initFps = fps;
    data->Vfreq = fps;
    data->Hfreq = 0;    //not api from driver
    close(fd);

    return 0;
}

/*
 * Get input signal status
 * @return int 1:Normal, 0: No Signal
 */
static int get_cur_signal_status(struct sys_hal_module *module)
{
    int ret, fd = -1;
    int state = 0;
    char *dev = const_cast<char*>(gDevicePath.c_str());

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, RK_HDMIRX_CMD_GET_SIGNAL_STABLE_STATUS, &state);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_SIGNAL_STABLE_STATUS fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    close(fd);

	return (state > 0)? 1 : 0;
}

/*
 * Get input Interlaced status
 * @return int 1:I-Signal, 0:P-Signal
 */
static int get_cur_Source_Interlaced(struct sys_hal_module *module)
{
    int ret, fd = -1;
    int state = 0;
    char *dev = const_cast<char*>(gDevicePath.c_str());

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, RK_HDMIRX_CMD_GET_SCAN_MODE, &state);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_SCAN_MODE fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    close(fd);

    return (state > 0)? 1 : 0;
}

/*
 * Get input signal type, DVI or HDMI
 * @return int 1:DVI, 0:HDMI
 */
static int get_cur_source_dvi_mode(struct sys_hal_module *module)
{
    int ret, fd = -1;
    int state = 0;
    char *dev = const_cast<char*>(gDevicePath.c_str());

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, RK_HDMIRX_CMD_GET_INPUT_MODE, &state);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_INPUT_MODE fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    close(fd);

    return (state > 0)? 1 : 0;
}

/*
 * Get the hdcp encryption status of the input signal
 * @return int 1:encrypte, 0:no encrypte
 */
static int get_cur_source_hdcp_encrypted(struct sys_hal_module *module)
{
    int ret, fd = -1;
    int state = 0;
    char *dev = const_cast<char*>(gDevicePath.c_str());

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, RK_HDMIRX_CMD_GET_HDCP_STATUS, &state);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_HDCP_STATUS fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    close(fd);

    return (state > 0)? 1 : 0;
}

/*
 * Get CEC status
 * @return CEC status, 0:OFF, 1:ON
 */
static int get_cec_enable(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set CEC status
 * @param value: 0:OFF, 1:ON
 * @return result [0: successfully, <0: failure]
 */
static int set_cec_enable(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get ARC status
 * @return ARC status, 0:OFF, 1:ON
 */
static int get_arc_enable(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set ARC status
 * @param value: 0:OFF, 1:ON
 * @return result [0: successfully, <0: failure]
 */
static int set_arc_enable(struct sys_hal_module *module, int value)
{
	return 0;
}


/*
 * Get overscan value
 * @param data：overscan value
 * @return Getresult [0: successfully, <0: failure]
 */
static int get_overscan(struct sys_hal_module *module, ui_overscan_data_t *data)
{
	data->left = DEFAULT_OVERSCAN_VALUE;
	data->top = DEFAULT_OVERSCAN_VALUE;
	data->right = DEFAULT_OVERSCAN_VALUE;
	data->bottom = DEFAULT_OVERSCAN_VALUE;
	get_service();
	if (mComposer != nullptr && module != nullptr) {
		hidl_vec<uint32_t> hidlOverscan;
		if (mComposer != nullptr && module != nullptr) {
			mComposer->getOverscan(module->dpy, [&](const auto& tmpResult, const auto& tmpOverscan) {
				if (tmpResult == Result::OK) {
					hidlOverscan = tmpOverscan;
				}
			});
		}
		if (hidlOverscan.size() == 4) {
			data->left = hidlOverscan[0];
			data->top = hidlOverscan[1];
			data->right = hidlOverscan[2];
			data->bottom = hidlOverscan[3];
		}
	}
	return 0;
}

/*
 * Set overscan value
 * @param data：overscan value
 * @return Getresult [0: successfully, <0: failure]
 */
static int set_overscan(struct sys_hal_module *module, ui_overscan_data_t data)
{
	get_service();
	if (mComposer != nullptr && module != nullptr) {
		mComposer->setScreenScale(module->dpy, 0, data.left);
		mComposer->setScreenScale(module->dpy, 1, data.top);
		mComposer->setScreenScale(module->dpy, 2, data.right);
		mComposer->setScreenScale(module->dpy, 3, data.bottom);
	}
	return 0;
}

 /*
 * Set pattern
 * @param int red, int green, int blue: RGB, range[0-255]
 * @return result [0: successfully, <0: failure]
 */
static int set_screen_pattern(struct sys_hal_module *module, int red, int green, int blue)
{
	return 0;
}


 /*
 * Set half screen mode
 * @param value: 0:OFF, 1:ON
 * @return result [0: successfully, <0: failure]
 */
static int set_half_screen_mode(struct sys_hal_module *module, int value)
{
	return 0;
}

 /*
 * Get half screen mode
 * @param value: 0:OFF, 1:ON
 * @return result [0: successfully, <0: failure]
 */
static int get_half_screen_mode(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Get DCR status
 * @return status, range[0-1]
 */
int get_dcr_enable(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set DCR status
 * @param value: 0:OFF, 1:ON
 * @return result [0: successfully, <0: failure]
 */
int set_dcr_enable(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get DBC MAX backlight
 * @return MAX backlight, range[0-100]
 */
int get_dbc_max_backlight(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set DBC MAX backlight
 * @param value: range[0-100]
 * @return result [0: successfully, <0: failure]
 */
int set_dbc_max_backlight(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get DBC MIN backlight
 * @return MIN backlight, range[0-100]
 */
int get_dbc_min_backlight(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set DBC MIN backlight
 * @param value: range[0-100]
 * @return result [0: successfully, <0: failure]
 */
int set_dbc_min_backlight(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get DBC backlight step time(ms)
 * @return setp time, range[0-1000]
 */
int get_dbc_step_time(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set DBC backlight step time(ms)
 * @param value: range[0-1000]
 * @return result [0: successfully, <0: failure]
 */
int set_dbc_step_time(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get DBC backlight step value
 * @return backlight step value, range[0-100]
 */
int get_dbc_step_value(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set DBC backlight step value
 * @param value: range[0-100]
 * @return result [0: successfully, <0: failure]
 */
int set_dbc_step_value(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get DBC status
 * @return status, range[0-1]
 */
int get_dbc_enable(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set DBC status
 * @param value: 0:OFF, 1:ON
 * @return result [0: successfully, <0: failure]
 */
int set_dbc_enable(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get panel partition(V-by-One)
 * @return panel partition, range[ui_panel_division_t]
 */
ui_panel_division_t get_panel_division(struct sys_hal_module *module)
{
	return PANEL_DIVISION_1;
}

/*
 * Set panel partition(V-by-One)
 * @param value: range[ui_panel_division_t]
 * @return result [0: successfully, <0: failure]
 */
int set_panel_division(struct sys_hal_module *module, ui_panel_division_t value)
{
	return 0;
}

/*
* Get VBO swing
* @return  VBO swing, range[0-255]
 */
int get_panel_swing(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set  VBO swing
 * @param value: range[0-255]
 * @return result [0: successfully, <0: failure]
 */
int set_panel_swing(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get VBO preemphasis
 * @return  VBO preemphasis, range[0-255]
 */
int get_panel_preemphasis(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set  VBO preemphasis
 * @param value: range[0-255]
 * @return result [0: successfully, <0: failure]
 */
int set_panel_preemphasis(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get freerun status
 * @return  freerun status, range[0-1]
 */
int get_panel_freerun(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set  freerun preemphasis
 * @param value: range[0-1]
 * @return result [0: successfully, <0: failure]
 */
int set_panel_freerun(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get shake screen status
 * @return status, range[0-1]
 */
int get_shake_screen_mode(struct sys_hal_module *module)
{
	return 0;
}

/*
 * Set shake screen status
 * @param value: range[0-1]
 * @return result [0: successfully, <0: failure]
 */
int set_shake_screen_mode(struct sys_hal_module *module, int value)
{
	return 0;
}

/*
 * Get average brightness
 * @return average brightness, range[0-255]
 */
int get_average_brightness(struct sys_hal_module *module)
{
	return property_get_int32(PROPERTY_MEAN_LUMA, 0);
}

/*
 * Get HDMI EDID
 * @return EDID, range[ui_edid_mode_t]
 */
int get_hdmi_edid_mode(struct sys_hal_module *module)
{
    int ret, fd = -1;
    ui_edid_mode_t edid_mode = EDID_MODE_4K60HZ_YUV444;
    char *dev = const_cast<char*>(gDevicePath.c_str());

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, RK_HDMIRX_CMD_GET_EDID_MODE, &edid_mode);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_GET_EDID_MODE fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    close(fd);

    return edid_mode;
}

/*
 * Set HDMI EDID
 * @param value: range[ui_edid_mode_t]
 * @return result [0: successfully, <0: failure]
 */
int set_hdmi_edid_mode(struct sys_hal_module *module, ui_edid_mode_t value)
{
    int ret, fd = -1;
    ui_edid_mode_t edid_mode = value;
    char *dev = const_cast<char*>(gDevicePath.c_str());

    UN_USED(module);
    fd = open(dev, O_RDWR, 0);
    if (fd < 0) {
            ALOGE("%s, fd is err\n", __func__);
            return -1;
    }

    ret = ioctl(fd, RK_HDMIRX_CMD_SET_EDID_MODE, &edid_mode);
    if (ret < 0) {
            ALOGE("RK_HDMIRX_CMD_SET_EDID_MODE fail, ret: %d\n", ret);
            close(fd);
            return -1;
    }
    close(fd);

    return 0;
}

/*
 * Get PC EDID
 * @return PC EDID, range[ui_edid_mode_t]
 */
ui_edid_mode_t get_pc_edid_mode(struct sys_hal_module *module)
{
	return EDID_MODE_2K60HZ_YUV444;
}

/*
 * Set PC EDID
 * @param value: range[ui_edid_mode_t]
 * @return result [0: successfully, <0: failure]
 */
int set_pc_edid_mode(struct sys_hal_module *module, ui_edid_mode_t value)
{
	return 0;
}

int set_screen_pixel_shift(struct sys_hal_module *module, int x, int y)
{
	char path[128];
	char buf[16];
	int fd = 0;
	int ret = 0;
	for (int i = 0; i < 4; i++) {
		sprintf(path, PIXEL_SHIFT_PATH, i);
		fd = open(path, O_WRONLY);
		if (fd >= 0) {
			break;
		}
	}
	if (fd < 0) {
		return -1;
	}
	sprintf(buf, "%d %d", x, y);
	int len = write(fd, buf, strlen(buf));
	if (len < 0) {
		ALOGE("Error writing to %s: %s\n", path, buf);
		ret = -1;
	} else {
		ret = 0;
	}
	close(fd);
	return ret;
}

int get_screen_pixel_shift(struct sys_hal_module *module, int *x, int *y)
{
	char path[128];
	char buf[64];
	int fd = 0;
	int ret = 0;
	for (int i = 0; i < 4; i++) {
		sprintf(path, PIXEL_SHIFT_PATH, i);
		fd = open(path, O_WRONLY);
		if (fd >= 0) {
			break;
		}
	}
	if (fd < 0) {
		return -1;
	}
	int len = read(fd, buf, 64);
	if (len < 0) {
		ALOGE("Error read %s: len %d\n", path, len);
		ret = -1;
	} else {
		sscanf(buf, "shift_x:%d, shift_y:%d", x, y);
		ret = 0;
	}
	close(fd);
	return ret;
}

static struct hw_module_methods_t sys_hal_module_methods =
{
	.open = NULL,
};

struct sys_hal_module HAL_MODULE_INFO_SYM =
{
    .common ={
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = SYS_HAL_MODULE_API_VERSION,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = SYS_HAL_HARDWARE_MODULE_ID,
        .name ="Default SYS HAL",
        .author = "The SYS Project",
        .methods = &sys_hal_module_methods,
   },
    .sysmodule_init = sysmodule_init,
    .get_input_stream_config = get_input_stream_config,
    .get_cur_signal_status = get_cur_signal_status,
    .get_cur_Source_Interlaced = get_cur_Source_Interlaced,
    .get_cur_source_dvi_mode = get_cur_source_dvi_mode,
    .get_cur_source_hdcp_encrypted = get_cur_source_hdcp_encrypted,
    .get_overscan = get_overscan,
    .set_overscan = set_overscan,
    .get_cec_enable = get_cec_enable,
    .set_cec_enable = set_cec_enable,
    .get_arc_enable = get_arc_enable,
    .set_arc_enable = set_arc_enable,
    .set_screen_pattern = set_screen_pattern,
    .set_half_screen_mode = set_half_screen_mode,
    .get_half_screen_mode = get_half_screen_mode,
    .get_average_brightness = get_average_brightness,
    .get_dcr_enable = get_dcr_enable,
    .set_dcr_enable = set_dcr_enable,
    .get_dbc_max_backlight = get_dbc_max_backlight,
    .set_dbc_max_backlight = set_dbc_max_backlight,
    .get_dbc_min_backlight = get_dbc_min_backlight,
    .set_dbc_min_backlight = set_dbc_min_backlight,
    .get_dbc_step_time = get_dbc_step_time,
    .set_dbc_step_time = set_dbc_step_time,
    .get_dbc_step_value = get_dbc_step_value,
    .set_dbc_step_value = set_dbc_step_value,
    .get_dbc_enable = get_dbc_enable,
    .set_dbc_enable = set_dbc_enable,
    .get_panel_division = get_panel_division,
    .set_panel_division = set_panel_division,
    .get_panel_swing = get_panel_swing,
    .set_panel_swing = set_panel_swing,
    .get_panel_preemphasis = get_panel_preemphasis,
    .set_panel_preemphasis = set_panel_preemphasis,
    .get_panel_freerun = get_panel_freerun,
    .set_panel_freerun = set_panel_freerun,
    .get_shake_screen_mode = get_shake_screen_mode,
    .set_shake_screen_mode = set_shake_screen_mode,
    .get_hdmi_edid_mode = get_hdmi_edid_mode,
    .set_hdmi_edid_mode = set_hdmi_edid_mode,
    .get_pc_edid_mode = get_pc_edid_mode,
    .set_pc_edid_mode = set_pc_edid_mode,
    .set_screen_pixel_shift = set_screen_pixel_shift,
    .get_screen_pixel_shift = get_screen_pixel_shift,
};
