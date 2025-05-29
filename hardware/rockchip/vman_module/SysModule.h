#ifndef ANDROID_INCLUDE_HARDWARE_SYS_HAL_H
#define ANDROID_INCLUDE_HARDWARE_SYS_HAL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/v4l2-subdev.h>
#include <rk_hdmirx_config.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <string.h>

__BEGIN_DECLS

#define SYS_HAL_MODULE_API_VERSION  HARDWARE_MODULE_API_VERSION(0, 1)

/*
 *  The id of this module
 */
#define SYS_HAL_HARDWARE_MODULE_ID      "vman_sys_hal"

typedef enum EnumPanelDivision {
	PANEL_DIVISION_1,
	PANEL_DIVISION_2,
	PANEL_DIVISION_4,
} ui_panel_division_t;

typedef enum EnumEdidMode {
	EDID_MODE_2K60HZ_YUV444,
	EDID_MODE_4K30HZ_YUV444,
	EDID_MODE_4K60HZ_YUV444,
	EDID_MODE_4K60HZ_YUV420,
	EDID_MODE_ERR,
} ui_edid_mode_t;

typedef struct DataOverScan {
	int left;
	int top;
	int right;
	int bottom;
} ui_overscan_data_t;

typedef struct StreamConfig {
	int width;
	int height;
	int interlace;
	int initFps;
	int Hfreq;
	int Vfreq;
} ui_stream_config_t;

typedef enum HdmiInType {
	HDMIIN_TYPE_HDMIRX = 0x0,
	HDMIIN_TYPE_MIPICSI = 0x1,
} ui_device_type;

typedef struct sys_hal_module {
	struct hw_module_t common;
	int dpy;
	int (*sysmodule_init)(ui_device_type eType);
	int (*get_input_stream_config)(struct sys_hal_module *module, ui_stream_config_t* data);
	int (*get_cur_signal_status)(struct sys_hal_module *module);
	int (*get_cur_Source_Interlaced)(struct sys_hal_module *module);
	int (*get_cur_source_dvi_mode)(struct sys_hal_module *module);
	int (*get_cur_source_hdcp_encrypted)(struct sys_hal_module *module);
	int (*get_overscan)(struct sys_hal_module *module, ui_overscan_data_t *data);
	int (*set_overscan)(struct sys_hal_module *module, ui_overscan_data_t data);
	int (*get_cec_enable)(struct sys_hal_module *module);
	int (*set_cec_enable)(struct sys_hal_module *module, int value);
	int (*get_arc_enable)(struct sys_hal_module *module);
	int (*set_arc_enable)(struct sys_hal_module *module, int value);
	int (*set_screen_pattern)(struct sys_hal_module *module, int red, int green, int blue);
	int (*set_half_screen_mode)(struct sys_hal_module *module, int value);
	int (*get_half_screen_mode)(struct sys_hal_module *module);
	int (*get_dcr_enable)(struct sys_hal_module *module);
	int (*set_dcr_enable)(struct sys_hal_module *module, int value);
	int (*get_dbc_max_backlight)(struct sys_hal_module *module);
	int (*set_dbc_max_backlight)(struct sys_hal_module *module, int value);
	int (*get_dbc_min_backlight)(struct sys_hal_module *module);
	int (*set_dbc_min_backlight)(struct sys_hal_module *module, int value);
	int (*get_dbc_step_time)(struct sys_hal_module *module);
	int (*set_dbc_step_time)(struct sys_hal_module *module, int value);
	int (*get_dbc_step_value)(struct sys_hal_module *module);
	int (*set_dbc_step_value)(struct sys_hal_module *module, int value);
	int (*get_dbc_enable)(struct sys_hal_module *module);
	int (*set_dbc_enable)(struct sys_hal_module *module, int value);
	ui_panel_division_t (*get_panel_division)(struct sys_hal_module *module);
	int (*set_panel_division)(struct sys_hal_module *module, ui_panel_division_t value);
	int (*get_panel_swing)(struct sys_hal_module *module);
	int (*set_panel_swing)(struct sys_hal_module *module, int value);
	int (*get_panel_preemphasis)(struct sys_hal_module *module);
	int (*set_panel_preemphasis)(struct sys_hal_module *module, int value);
	int (*get_panel_freerun)(struct sys_hal_module *module);
	int (*set_panel_freerun)(struct sys_hal_module *module, int value);
	int (*get_shake_screen_mode)(struct sys_hal_module *module);
	int (*set_shake_screen_mode)(struct sys_hal_module *module, int value);
	int (*get_average_brightness)(struct sys_hal_module *module);
	int (*get_hdmi_edid_mode)(struct sys_hal_module *module);
	int (*set_hdmi_edid_mode)(struct sys_hal_module *module, ui_edid_mode_t value);
	ui_edid_mode_t (*get_pc_edid_mode)(struct sys_hal_module *module);
	int (*set_pc_edid_mode)(struct sys_hal_module *module, ui_edid_mode_t value);
	int (*set_screen_pixel_shift)(struct sys_hal_module *module, int x, int y);
	int (*get_screen_pixel_shift)(struct sys_hal_module *module, int *x, int *y);
} sys_hal_module_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_HARDWARE_SYS_HAL_H */
