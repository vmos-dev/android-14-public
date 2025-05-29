/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * VMAN -- Audio module
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 */

#include <hardware/hardware.h>
#include <log/log.h>
#include "AudioModule.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "vman_audio"
#endif

/*
 * Get volume value
 * @return int:
 * 	Current volume, range: 0~100
 */
static int get_speakerout_volume(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set volume value
 * @param value int:
 * 	Current volume, range: 0~100
 */
static void set_speakerout_volume(struct audio_hal_module *module, int value)
{
}

/*
 * Get mute status
 * @return boolean：
 *      mute status: true - mute; false - unmute;
 */
static int get_speakerout_mute(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set mute status
 * @param value boolean:
 *      mute status: true - mute; false - unmute;
 */
static void set_speakerout_mute(struct audio_hal_module *module, int value)
{
}

/*
 * Set the audio device type for the forced output
 * @param outputDevice: audio device type
 *
 * @return result, true: successfully; false: failure
 */
static bool set_force_use_outputdevice(struct audio_hal_module *module,
	enumaudiooutputdevicetype outputDevice)
{
	return 0;
}

/*
 * Get the audio device type for the forced output
 *
 * @return audio device type
 */
static enumaudiooutputdevicetype get_force_use_outputdevice(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Get the PA mute status
 * @param amptype
 * 	AudioCtrlManager.AMP_TYPE_TREBLE: treble 
 * 	AudioCtrlManager.AMP_TYPE_BASS: bass
 * @return mute status 
 *	AudioCtrlManager.AMP_UNMUTE: unmute
 *	AudioCtrlManager.AMP_MUTE:   mute
 */
static int get_audio_amp_mute(struct audio_hal_module *module, int amptype)
{
	return 0;
}

/*
 * Set the PA mute status
 * @param amptype
 * 	AudioCtrlManager.AMP_TYPE_TREBLE： treble
 * 	AudioCtrlManager.AMP_TYPE_BASS：	bass
 * @param status
 * 	AudioCtrlManager.AMP_UNMUTE：unmute   
 * 	AudioCtrlManager.AMP_MUTE：  mute
 * @return result：true - successfully; false - failure;
 */
static bool set_audio_amp_mute(struct audio_hal_module *module, int amptype, int status)
{
	return 0;
}

/*
 * Get LineOut/Headphone PA mute status
 * @return mute status 
 *	AudioCtrlManager.HEADPHONE_UNMUTE:	false = unmute
 *	AudioCtrlManager.HEADPHONE_MUTE：	true = mute
 */
static bool get_headphone_mute(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set LineOut/Headphone PA mute status
 * @param status
 *	AudioCtrlManager.HEADPHONE_UNMUTE:	false = unmute
 *	AudioCtrlManager.HEADPHONE_MUTE：	true =  mute
 * @return result：true - successfully; false - failure;
 */
static bool set_headphone_mute(struct audio_hal_module *module, bool status)
{
	return 0;
}

/*
 * Get DRC status
 * @return DRC status:  false - Disable DRC;	true - Enable DRC
 */
static bool get_drc_enable(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set DRC status
 * @param enable boolean
 * 	Set DRC :  false - Disable DRC;	true - Enable DRC
 * @return result：true - successfully; false - failure;
 */
static bool set_drc_enable(struct audio_hal_module *module, bool enable)
{
	return 0;
}

/*
 * Get EQ status
 * @return EQ status: false - DisableEQ;  true - EnableEQ;
 */
static bool get_eq_enable(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set EQ status
 * @param enable boolean
 * 	Set EQ:	false - DisableEQ;  true - EnableEQ;
 * @return result：true - successfully; false - failure;
 */
static bool set_eq_enable(struct audio_hal_module *module, bool enable)
{
	return 0;
}

/*
 * Get balance
 * @return balance
 */
static int get_balance(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set balance
 * @param balance value
 * @return result：true - successfully; false - failure;
 */
static bool set_balance(struct audio_hal_module *module, int balance)
{
	return 0;
}

/*
 * Get treble
 * @return treble
 */
static int get_treble(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set treble
 * @param treble : treble tytpe
 * @return result：true - successfully; false - failure;
 */
static bool set_treble(struct audio_hal_module *module, int treble)
{
	return 0;
}

/*
 * Get bass
 * @return bass
 */
static int get_bass(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set bass
 * @param bass ：bass value
 * @return result：true - successfully; false - failure;
 */
static bool set_bass(struct audio_hal_module *module, int bass)
{
	return 0;
}

/*
 * Get AudioOutMode
 * @return AudioOutMode
 * 	AudioCtrlManager.AUDIOOUT_MODE_HEADSET	:Headset 
 * 	AudioCtrlManager.AUDIOOUT_MODE_LINEOUT	:Lineout
 * 	AudioCtrlManager.AUDIOOUT_MODE_SPEAKER	:Speaker
 * 	AudioCtrlManager.AUDIOOUT_MODE_BOTH		  :Both
 */
static int get_audio_out_Mode(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set AudioOutMode
 * @param mode: audio ouput mode
 * 	AudioCtrlManager.AUDIOOUT_MODE_HEADSET	:Headset
 * 	AudioCtrlManager.AUDIOOUT_MODE_LINEOUT	:Lineout
 * 	AudioCtrlManager.AUDIOOUT_MODE_SPEAKER	:Speaker
 * 	AudioCtrlManager.AUDIOOUT_MODE_BOTH		  :Both
 * @return result：true - successfully; false - failure;
 */
static bool set_audio_out_mode(struct audio_hal_module *module, int mode)
{
	return 0;
}

/*
 * Get Sound mode
 * @return EQ mode:
 * 	0: Standard mode 1: Movie mode 2: Classroom mode 3: Conference mode 4: User mode
 */
static int get_sound_mode(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set Sound mode
 * @param mode:
 * 	0: Standard mode 1: Movie mode 2: Classroom mode 3: Conference mode 4: User mode
 * @return result：true - successfully; false - failure;
 */
static bool set_sound_mode(struct audio_hal_module *module, int mode)
{
	return 0;
}

/*
 * Set AEC status (Speaker and Headset output simultaneously)
 * 	Synchronous / Asynchronous: Synchronous
 *
 * @param enable :
 * 	bool，enable or disable
 */
static void set_auto_matic_echo_cancel(struct audio_hal_module *module, bool enable)
{
}

/*
 * Get AEC status
 * Synchronous / Asynchronous: Synchronous
 *
 * @return : bool，enable or disable
 */
static bool get_auto_matic_echo_cancel(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Get LineOut fix output
 * Synchronous / Asynchronous: Synchronous
 *
 * @return
 *	   true：LineOut fix output；false：The LineOut output follows the volume change
 */
static bool get_lineout_fix_output(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set LineOut fix output
 * Synchronous / Asynchronous: Synchronous
 *
 * @param fix
 *	   0 : The LineOut output follows the volume change	1 : LineOut fix output
 * @return result：true - successfully; false - failure;
 */
static bool set_lineout_fix_output(struct audio_hal_module *module, int fix)
{
	return 0;
}

/* 
 * Get delay time of sound output
 * Synchronous / Asynchronous: Synchronous
 *
 * @param channel
 *		The channel of Sound delay 
 * @return
 *		int: The time of sound delay
 */
 static int get_audio_delay(struct audio_hal_module *module, int channel)
{
	return 0;
}

/*
 * Set delay time of sound output
 * Synchronous / Asynchronous: Synchronous
 *
 * @param channel
 *		 The channel of Sound delay 
 * @param ms
 *		 The time of sound delay
 * @return result：true - successfully; false - failure;
 */
static bool set_audio_delay(struct audio_hal_module *module, int channel, int ms)
{
	return 0;
}

/*
 * Get GEQ value
 * Synchronous / Asynchronous: Synchronous
 *
 * @return
 *		int array, GEQ value of each band
 */
static int* get_geq(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set GEQ value of the specified band
 * Synchronous / Asynchronous: Synchronous
 *
 * @param band
 * 	index
 * @param value
 * 	GEQ value of the specified band
 * @return result：true - successfully; false - failure;
 */
static int set_geq(struct audio_hal_module *module, int band, int value)
{
	return 0;
}
 
/*
 * Get PEQ status
 * Synchronous / Asynchronous: Synchronous
 *
 * @return
 * 	 status true：enable，false：disable
 */ 
static bool get_peq_enable(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set PEQ status
 * 	Synchronous / Asynchronous: Synchronous
 *
 * @param enable :	true：enable，false：disable
 * @return result：true - successfully; false - failure;
 */ 
static bool set_peq_enable(struct audio_hal_module *module, bool enable)
{
	return 0;
}

/*
 * Get PEQ value
 * Synchronous / Asynchronous: Synchronous
 *
 * @param band
 * 	index
 * @return
 * 	PEQ value of the specified band
 */
static int* get_band_peq(struct audio_hal_module *module, int band)
{
	return 0;
}

/*
 * Set PEQ value of the specified band
 * Synchronous / Asynchronous: Synchronous
 *
 * @param band
 * 	index
 * @param size
 * 	peqvalue size
 * @param peqvalue
 * 	PEQ value
 * @return result：0 - successfully; -1 - failure;
 */
static int set_band_peq(struct audio_hal_module *module, int band, int size, int peqvalue[])
{
	return 0;
}

/*
 * Get DRC value
 * Synchronous / Asynchronous: Synchronous
 *
 * @param band
 *		 index
 * @return
 *		int，DRC value of the specified band
 */
 static int* get_band_drc(struct audio_hal_module *module, int band)
{
	return 0;
}

/*
 * Set DRC value of the specified band
 * Synchronous / Asynchronous: Synchronous
 *
 * @param band
 * 	index
 * @param size
 * 	drcvalue size
 * @param drcvalue
 * 	DRC value
 * @return result：0 - successfully; -1 - failure;
 */
static int set_band_drc(struct audio_hal_module *module, int band, int size, int drcvalue[])
{
	return 0;
}

/*
 * Get Prescale of sound channel
 * Synchronous / Asynchronous: Synchronous
 *
 * @return
 *		int Prescale value
 */
static int get_prescale(struct audio_hal_module *module)
{
	return 0;
}

/*
 * Set Prescale of sound channel
 * Synchronous / Asynchronous: Synchronous
 *
 * @param value
 *		 Prescale value
 * @return result：true - successfully; false - failure;
 */
static bool set_prescale(struct audio_hal_module *module, int value)
{
	return 0;
}

static struct hw_module_methods_t audio_hal_module_methods =
{
	.open = NULL,
};

struct audio_hal_module HAL_MODULE_INFO_SYM =
{
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = AUDIO_HAL_MODULE_API_VERSION_0_1,
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = AUDIO_HAL_HARDWARE_MODULE_ID,
		.name ="Default AUDIO HAL",
		.author = "The AUDIO Project",
		.methods = &audio_hal_module_methods,
	},
	.get_speakerout_volume = get_speakerout_volume,
	.set_speakerout_volume = set_speakerout_volume,

	.get_speakerout_mute = get_speakerout_mute,
	.set_speakerout_mute = set_speakerout_mute,

	.set_force_use_outputdevice = set_force_use_outputdevice,
	.get_force_use_outputdevice = get_force_use_outputdevice,

	.get_audio_amp_mute = get_audio_amp_mute,
	.set_audio_amp_mute = set_audio_amp_mute,
	.get_headphone_mute = get_headphone_mute,
	.set_headphone_mute = set_headphone_mute,

	.get_drc_enable = get_drc_enable,
	.set_drc_enable = set_drc_enable,
	.get_eq_enable = get_eq_enable,
	.set_eq_enable = set_eq_enable,

	.get_balance = get_balance,
	.set_balance = set_balance,
	.get_treble = get_treble,
	.set_treble = set_treble,
	.get_bass = get_bass,
	.set_bass = set_bass,
	.get_audio_out_Mode = get_audio_out_Mode,
	.set_audio_out_mode = set_audio_out_mode,
	.get_sound_mode = get_sound_mode,
	.set_sound_mode = set_sound_mode,

	.set_auto_matic_echo_cancel = set_auto_matic_echo_cancel,
	.get_auto_matic_echo_cancel = get_auto_matic_echo_cancel,
	.get_lineout_fix_output = get_lineout_fix_output,
	.set_lineout_fix_output = set_lineout_fix_output,

	.get_audio_delay = get_audio_delay,
	.set_audio_delay = set_audio_delay,
	.get_geq = get_geq,
	.set_geq = set_geq,
	.get_peq_enable = get_peq_enable,
	.set_peq_enable = set_peq_enable,
	.get_band_peq = get_band_peq,
	.set_band_peq = set_band_peq,
	.get_band_drc = get_band_drc,
	.set_band_drc = set_band_drc,
	.get_prescale = get_prescale,
	.set_prescale = set_prescale,
};
