#ifndef ANDROID_INCLUDE_HARDWARE_PQ_HAL_H
#define ANDROID_INCLUDE_HARDWARE_PQ_HAL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>

__BEGIN_DECLS

#define PQ_HAL_MODULE_API_VERSION  HARDWARE_MODULE_API_VERSION(0, 1)

/*
 *  The id of this module
 */
#define PQ_HAL_HARDWARE_MODULE_ID      "vman_pq_hal"

typedef enum EnumVmanPQColorTemp {
	VmanPQ_COLOR_TEMP_COLD,
	VmanPQ_COLOR_TEMP_STANDARD,
	VmanPQ_COLOR_TEMP_WARM,
	VmanPQ_COLOR_TEMP_USER,
} ui_color_temp_t;

typedef enum EnumVmanPQPictureMode {
	VmanPQ_PICTURE_MODE_STANDARD,
	VmanPQ_PICTURE_MODE_USER,
	VmanPQ_PICTURE_MODE_DYNAMIC,
	VmanPQ_PICTURE_MODE_SOFT,
	VmanPQ_PICTURE_MODE_VIVID,
	VmanPQ_PICTURE_MODE_MOVIE,
	VmanPQ_PICTURE_MODE_ENERGY_SAVING,
} ui_picture_mode_t;

typedef enum EnumVmanPQAspect {
	VmanPQ_ASPECT_16X9,
	VmanPQ_ASPECT_4X3,
	VmanPQ_ASPECT_FULL,
	VmanPQ_ASPECT_ZOOM,
} ui_aspect_mode_t;

typedef enum EnumVmanPQColorRange {
	VmanPQ_COLOR_RANGE_AUTO,
	VmanPQ_COLOR_RANGE_LIMIT,
	VmanPQ_COLOR_RANGE_FULL,
} ui_color_range_t;

typedef struct pq_hal_module {
	struct hw_module_t common;
	int dpy;
	void (*init)(struct pq_hal_module *module);
	int (*get_brightness)(struct pq_hal_module *module);
	int (*set_brightness)(struct pq_hal_module *module, int value);
	int (*get_contrast)(struct pq_hal_module *module);
	int (*set_contrast)(struct pq_hal_module *module, int value);
	int (*get_saturation)(struct pq_hal_module *module);
	int (*set_saturation)(struct pq_hal_module *module, int value);
	int (*set_sharpness)(struct pq_hal_module *module, int value);
	int (*get_sharpness)(struct pq_hal_module *module);
	int (*get_hue)(struct pq_hal_module *module);
	int (*set_hue)(struct pq_hal_module *module, int value);
	int (*set_preset_color_temp_mode)(struct pq_hal_module *module, int path, int index, uint32_t rgain, uint32_t ggain, uint32_t bgain);
	int (*get_preset_color_temp_mode)(struct pq_hal_module *module, int path,int index, uint32_t* rgain, uint32_t* ggain, uint32_t* bgain);
	int (*set_color_temp_mode)(struct pq_hal_module *module, ui_color_temp_t value);
	ui_color_temp_t (*get_color_temp_mode)(struct pq_hal_module *module);
	int (*reset_color_temp)(struct pq_hal_module *module);
	int (*get_wb_r_gain)(struct pq_hal_module *module);
	int (*set_wb_r_gain)(struct pq_hal_module *module, int value);
	int (*get_wb_g_gain)(struct pq_hal_module *module);
	int (*set_wb_g_gain)(struct pq_hal_module *module, int value);
	int (*get_wb_b_gain)(struct pq_hal_module *module);
	int (*set_wb_b_gain)(struct pq_hal_module *module, int value);
	int (*get_wb_r_offset)(struct pq_hal_module *module);
	int (*set_wb_r_offset)(struct pq_hal_module *module, int value);
	int (*get_wb_g_offset)(struct pq_hal_module *module);
	int (*set_wb_g_offset)(struct pq_hal_module *module, int value);
	int (*get_wb_b_offset)(struct pq_hal_module *module);
	int (*set_wb_b_offset)(struct pq_hal_module *module, int value);
	int (*set_csc_enable)(struct pq_hal_module *module, int value);
	int (*get_csc_enable)(struct pq_hal_module *module);
	int (*set_dci_enable)(struct pq_hal_module *module, int value);
	int (*get_dci_enable)(struct pq_hal_module *module);
	int (*set_acm_enable)(struct pq_hal_module *module, int value);
	int (*get_acm_enable)(struct pq_hal_module *module);
	int (*set_sharp_enable)(struct pq_hal_module *module, int value);
	int (*get_sharp_enable)(struct pq_hal_module *module);
	int (*set_pq_enable)(struct pq_hal_module *module, int value);
	int (*get_pq_enable)(struct pq_hal_module *module);
	int (*set_hdr_enable)(struct pq_hal_module *module, int value);
	int (*get_hdr_enable)(struct pq_hal_module *module);
	int (*set_picture_mode)(struct pq_hal_module *module, ui_picture_mode_t value);
	ui_picture_mode_t (*get_picture_mode)(struct pq_hal_module *module);
	int (*set_gamma_mode)(struct pq_hal_module *module, int value);
	int (*get_gamma_mode)(struct pq_hal_module *module);
	int (*set_3dlut_mode)(struct pq_hal_module *module, int value);
	int (*get_3dlut_mode)(struct pq_hal_module *module);
	int (*set_3dlut_data_buff)(struct pq_hal_module *module, int index, char *data);
	int (*get_3dlut_data_buff)(struct pq_hal_module *module, int index, char *data);
	int (*reset_3dlut_data)(struct pq_hal_module *module);
	ui_aspect_mode_t (*get_aspect_mode)(struct pq_hal_module *module);
	int (*set_aspect_mode)(struct pq_hal_module *module, ui_aspect_mode_t value);
	ui_color_range_t (*get_color_range)(struct pq_hal_module *module);
	int (*set_color_range)(struct pq_hal_module *module, ui_color_range_t value);
	int (*get_backlight)(struct pq_hal_module *module);
	int (*set_backlight)(struct pq_hal_module *module, int value);
	int (*get_max_backlight)(struct pq_hal_module *module);
	int (*set_max_backlight)(struct pq_hal_module *module, int value);
	int (*set_acm_mode)(struct pq_hal_module *module, int mode);
	int (*get_acm_mode)(struct pq_hal_module *module);
	int (*set_dci_mode)(struct pq_hal_module *module, int mode);
	int (*get_dci_mode)(struct pq_hal_module *module);
	int (*set_preset_picture_mode)(struct pq_hal_module *module, int  path, int index, int brightness, int contrast, int saturation, int hue);
	int (*get_preset_picture_mode)(struct pq_hal_module *module, int  path, int index, int *brightness, int *contrast, int *saturation, int *hue);
	int (*set_preset_gamma_mode)(struct pq_hal_module *module, int path, int index, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
	int (*get_preset_gamma_mode)(struct pq_hal_module *module, int path, int index, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
	int (*set_preset_3dlut_mode)(struct pq_hal_module *module, int path, int index, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);
	int (*get_preset_3dlut_mode)(struct pq_hal_module *module, int path, int index, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b);

} pq_hal_module_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_HARDWARE_PQ_HAL_H */
