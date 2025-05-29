#ifndef ANDROID_INCLUDE_AUDIO_HAL_H
#define ANDROID_INCLUDE_AUDIO_HAL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>

__BEGIN_DECLS

#define AUDIO_HAL_MODULE_API_VERSION_0_1  HARDWARE_MODULE_API_VERSION(0, 1)

/*
 *  The id ot this module
 */
#define AUDIO_HAL_HARDWARE_MODULE_ID "vman_audio_hal"

typedef enum EnumAudioOutputDeviceType {
	DEVICE_OUTPUT_AUTO,
	DEVICE_OUTPUT_SPEAKER,
	DEVICE_OUTPUT_HDMI,
	DEVICE_OUTPUT_HEASET,
	DEVICE_OUTPUT_COUNT_MAX
} enumaudiooutputdevicetype;

typedef struct audio_hal_module {
	struct hw_module_t common;
	int (*get_speakerout_volume)(struct audio_hal_module *module);
	void (*set_speakerout_volume)(struct audio_hal_module *module, int value);
	int (*get_speakerout_mute)(struct audio_hal_module *module);
	void (*set_speakerout_mute)(struct audio_hal_module *module, int value);
	bool (*set_force_use_outputdevice)(struct audio_hal_module *module,
		enumaudiooutputdevicetype outputDevice);
	enumaudiooutputdevicetype (*get_force_use_outputdevice)(struct audio_hal_module *module);
	int (*get_audio_amp_mute)(struct audio_hal_module *module,  int amptype);
	bool (*set_audio_amp_mute)(struct audio_hal_module *module, int amptype, int status);
	bool (*get_headphone_mute)(struct audio_hal_module *module);
	bool (*set_headphone_mute)(struct audio_hal_module *module, bool status);
	bool (*get_drc_enable)(struct audio_hal_module *module);
	bool (*set_drc_enable)(struct audio_hal_module *module, bool enable);
	bool (*get_eq_enable)(struct audio_hal_module *module);
	bool (*set_eq_enable)(struct audio_hal_module *module, bool enable);
	int (*get_balance)(struct audio_hal_module *module);
	bool (*set_balance)(struct audio_hal_module *module, int balance);
	int (*get_treble)(struct audio_hal_module *module);
	bool (*set_treble)(struct audio_hal_module *module, int treble);
	int (*get_bass)(struct audio_hal_module *module);
	bool (*set_bass)(struct audio_hal_module *module, int bass);
	int (*get_audio_out_Mode)(struct audio_hal_module *module);
	bool (*set_audio_out_mode)(struct audio_hal_module *module, int mode);
	int (*get_sound_mode)(struct audio_hal_module *module);
	bool (*set_sound_mode)(struct audio_hal_module *module, int mode);
	void (*set_auto_matic_echo_cancel)(struct audio_hal_module *module, bool enable);
	bool (*get_auto_matic_echo_cancel)(struct audio_hal_module *module);
	bool (*get_lineout_fix_output)(struct audio_hal_module *module);
	bool (*set_lineout_fix_output)(struct audio_hal_module *module, int fix);
	int (*get_audio_delay)(struct audio_hal_module *module, int channel);
	bool (*set_audio_delay)(struct audio_hal_module *module, int channel, int ms);
	int* (*get_geq)(struct audio_hal_module *module);
	int (*set_geq)(struct audio_hal_module *module, int band, int value);
	bool (*get_peq_enable)(struct audio_hal_module *module);
	bool (*set_peq_enable)(struct audio_hal_module *module, bool enable);
	int* (*get_band_peq)(struct audio_hal_module *module, int band);
	int (*set_band_peq)(struct audio_hal_module *module, int band, int size, int peqvalue[]);
	int* (*get_band_drc)(struct audio_hal_module *module, int band);
	int (*set_band_drc)(struct audio_hal_module *module, int band, int size, int drcvalue[]);
	int (*get_prescale)(struct audio_hal_module *module);
	bool (*set_prescale)(struct audio_hal_module *module, int value);
} audio_hal_module_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_AUDIO_HAL_H */
