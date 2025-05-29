/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
*/
/**
 * @file rk3308_config.h
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date 2024-02-27
 */

#ifndef _RK3308_CONFIG_H_
#define _RK3308_CONFIG_H_

#define BUS_NAME_MEDIA              "bus0_media_out"
#define BUS_NAME_NAVIGATION         "bus1_navigation_out"
#define BUS_NAME_VOICE              "bus2_voice_command_out"
#define BUS_NAME_RING               "bus3_call_ring_out"
#define BUS_NAME_PHONE              "bus4_call_out"
#define BUS_NAME_SYS_NOTIFICATION   "bus6_notification_out"

#define BUS_NAME_ZONE_1             "bus100_audio_zone_1"
#define BUS_NAME_ZONE_2             "bus200_audio_zone_2"
#define BUS_NAME_ZONE_3             "bus300_audio_zone_3"

#define CTL_CHN0_PLAYBACK_VOLUME    "CHN0 Playback Vol"
#define CTL_CHN1_PLAYBACK_VOLUME    "CHN1 Playback Vol"
#define CTL_CHN2_PLAYBACK_VOLUME    "CHN2 Playback Vol"
#define CTL_CHN3_PLAYBACK_VOLUME    "CHN3 Playback Vol"
#define CTL_CHN4_PLAYBACK_VOLUME    "CHN4 Playback Vol"
#define CTL_CHN5_PLAYBACK_VOLUME    "CHN5 Playback Vol"
#define CTL_CHN6_PLAYBACK_VOLUME    "CHN6 Playback Vol"
#define CTL_CHN7_PLAYBACK_VOLUME    "CHN7 Playback Vol"

#define CTL_CHN0_PLAYBACK_MUTE      "CHN0 Playback Mute"
#define CTL_CHN1_PLAYBACK_MUTE      "CHN1 Playback Mute"
#define CTL_CHN2_PLAYBACK_MUTE      "CHN2 Playback Mute"
#define CTL_CHN3_PLAYBACK_MUTE      "CHN3 Playback Mute"
#define CTL_CHN4_PLAYBACK_MUTE      "CHN4 Playback Mute"
#define CTL_CHN5_PLAYBACK_MUTE      "CHN5 Playback Mute"
#define CTL_CHN6_PLAYBACK_MUTE      "CHN6 Playback Mute"
#define CTL_CHN7_PLAYBACK_MUTE      "CHN7 Playback Mute"

#define on 1
#define off 0

struct config_control
{
    const char *ctl_name; //name of control.
    char *str_val; //value of control, which type is stream.
    int int_val[2]; //left and right value of control, which type are int.
};

struct audio_bus_ctls
{
    const char *bus_name;
    struct config_control *controls;
    const unsigned controls_count;
};

struct config_control rk3308_speaker_media_controls[] = {
    {
        .ctl_name = CTL_CHN0_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN0_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_nav_controls[] = {
    {
        .ctl_name = CTL_CHN2_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN2_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_voice_controls[] = {
    {
        .ctl_name = CTL_CHN4_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN4_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_phone_controls[] = {
    {
        .ctl_name = CTL_CHN6_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN6_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_notif_controls[] = {
    {
        .ctl_name = CTL_CHN1_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN1_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_zone_1_controls[] = {
    {
        .ctl_name = CTL_CHN3_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN3_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_zone_2_controls[] = {
    {
        .ctl_name = CTL_CHN5_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN5_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct config_control rk3308_speaker_zone_3_controls[] = {
    {
        .ctl_name = CTL_CHN7_PLAYBACK_VOLUME,
        .int_val = {0, 0},
    },
    {
        .ctl_name = CTL_CHN7_PLAYBACK_MUTE,
        .int_val = {off},
    },
};

struct audio_bus_ctls bus_table[] = {
    {
        .bus_name = BUS_NAME_MEDIA,
        .controls = rk3308_speaker_media_controls,
        .controls_count = sizeof(rk3308_speaker_media_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_NAVIGATION,
        .controls = rk3308_speaker_nav_controls,
        .controls_count = sizeof(rk3308_speaker_nav_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_VOICE,
        .controls = rk3308_speaker_voice_controls,
        .controls_count = sizeof(rk3308_speaker_voice_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_PHONE,
        .controls = rk3308_speaker_phone_controls,
        .controls_count = sizeof(rk3308_speaker_phone_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_SYS_NOTIFICATION,
        .controls = rk3308_speaker_notif_controls,
        .controls_count = sizeof(rk3308_speaker_notif_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_ZONE_1,
        .controls = rk3308_speaker_zone_1_controls,
        .controls_count = sizeof(rk3308_speaker_zone_1_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_ZONE_2,
        .controls = rk3308_speaker_zone_2_controls,
        .controls_count = sizeof(rk3308_speaker_zone_2_controls) / sizeof(struct config_control),
    },
    {
        .bus_name = BUS_NAME_ZONE_3,
        .controls = rk3308_speaker_zone_3_controls,
        .controls_count = sizeof(rk3308_speaker_zone_3_controls) / sizeof(struct config_control),
    },
};

#endif //_RK3308_CONFIG_H_
