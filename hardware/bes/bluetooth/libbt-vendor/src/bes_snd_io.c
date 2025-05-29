#include <utils/Log.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "tinyalsa/asoundlib.h"
#include "bes_snd_io.h"

enum{
    TINYALSA_PCM_IN = 0x00,
    TINYALSA_PCM_OUT = 0x01,
    TINYALSA_PCM_MAX
};

static bes_tiny_mixer_para_t bes_tiny_mixer_para[]= {
    {"test_snd_vol", "3"}
};

static bes_tinyalsa_para_t tinyalsa_para = {0};

static void tinymix_set_byte_ctl(struct mixer_ctl *ctl,
    char **values, unsigned int num_values)
{
    int ret;
    char *buf;
    char *end;
    unsigned int i;
    long n;
    unsigned int *tlv, tlv_size;
    unsigned int tlv_header_size = 0;

    if (mixer_ctl_is_access_tlv_rw(ctl)) {
        tlv_header_size = TLV_HEADER_SIZE;
    }

    tlv_size = num_values + tlv_header_size;

    buf = calloc(1, tlv_size);
    if (buf == NULL) {
        ALOGE("set_byte_ctl: Failed to alloc mem for bytes %d\n", num_values);
        return;
    }

    tlv = (unsigned int *)buf;
    tlv[0] = 0;
    tlv[1] = num_values;

    for (i = 0; i < num_values; i++) {
        errno = 0;
        n = strtol(values[i], &end, 0);
        if (*end) {
            ALOGE("%s not an integer\n", values[i]);
            goto fail;
        }
        if (errno) {
            ALOGE("strtol: %s: %s\n", values[i],
                strerror(errno));
            goto fail;
        }
        if (n < 0 || n > 0xff) {
            ALOGE("%s should be between [0, 0xff]\n",
                values[i]);
            goto fail;
        }
        /* start filling after the TLV header */
        buf[i + tlv_header_size] = n;
    }

    ret = mixer_ctl_set_array(ctl, buf, tlv_size);
    if (ret < 0) {
        ALOGE("Failed to set binary control\n");
        goto fail;
    }

    free(buf);
    return;

fail:
    free(buf);
    return;
}

static int tinymix_set_value(struct mixer *mixer, const char *control,
                             char **values, unsigned int num_values)
{
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_ctl_values;
    unsigned int i;

    if (isdigit(control[0]))
        ctl = mixer_get_ctl(mixer, atoi(control));
    else
        ctl = mixer_get_ctl_by_name(mixer, control);

    if (!ctl) {
        ALOGE("Invalid mixer control: %s\n", control);
        return ENOENT;
    }

    type = mixer_ctl_get_type(ctl);
    num_ctl_values = mixer_ctl_get_num_values(ctl);

    if (type == MIXER_CTL_TYPE_BYTE) {
        tinymix_set_byte_ctl(ctl, values, num_values);
        return ENOENT;
    }

    if (isdigit(values[0][0])) {
        if (num_values == 1) {
            /* Set all values the same */
            int value = atoi(values[0]);

            for (i = 0; i < num_ctl_values; i++) {
                if (mixer_ctl_set_value(ctl, i, value)) {
                    ALOGE("Error: invalid value\n");
                    return EINVAL;
                }
            }
        } else {
            /* Set multiple values */
            if (num_values > num_ctl_values) {
                ALOGE("Error: %u values given, but control only takes %u\n",
                        num_values, num_ctl_values);
                return EINVAL;
            }
            for (i = 0; i < num_values; i++) {
                if (mixer_ctl_set_value(ctl, i, atoi(values[i]))) {
                    ALOGE("Error: invalid value for index %d\n", i);
                    return EINVAL;
                }
            }
        }
    } else {
        if (type == MIXER_CTL_TYPE_ENUM) {
            if (num_values != 1) {
                ALOGE("Enclose strings in quotes and try again\n");
                return EINVAL;
            }
            if (mixer_ctl_set_enum_by_string(ctl, values[0])) {
                ALOGE("Error: invalid enum value\n");
                return EINVAL;
            }
        } else {
            ALOGE("Error: only enum types can be set with strings\n");
            return EINVAL;
        }
    }

    return 0;
}


bool bes_set_snd_path(void)
{
    struct mixer *mixer;
    int card = 0;
    int ret = true;
    
    mixer = mixer_open(card);
    
    if (!mixer) {
        ALOGE("Failed to open mixer\n");
        return ENODEV;
    }

    for(int i = 0; i < sizeof(bes_tiny_mixer_para)/sizeof(bes_tiny_mixer_para_t); i++){
        ret = tinymix_set_value(mixer, 
                                (const char *)bes_tiny_mixer_para[i].value_name,
                                (char **)&bes_tiny_mixer_para[i].value,
                                1);
        if(!ret){
            ALOGE("bes_set_snd_path error:%s", bes_tiny_mixer_para[i].value_name);
            break;
        }
    }
    return ret;
}

static int check_param(struct pcm_params *params, unsigned int param, unsigned int value,
                 char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        ALOGE("%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        ALOGE("%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}

static int sample_is_playable(unsigned int card, unsigned int device, unsigned int channels,
                        unsigned int rate, unsigned int bits, unsigned int period_size,
                        unsigned int period_count)
{
    struct pcm_params *params;
    int can_play;

    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        ALOGE("Unable to open PCM device %u.\n", device);
        return 0;
    }

    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", " frames");
    can_play &= check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", " periods");

    pcm_params_free(params);

    return can_play;
}

static struct pcm *tinyalsa_pcm[TINYALSA_PCM_MAX] = {NULL};
bool bes_snd_tinyalsa_open(bes_tinyalsa_para_t * alsa_param, bool pcm_out)
{
    uint32_t pcm_direction = 0;
    struct pcm_config config;
    struct pcm ** s_pcm = NULL;
    
    if(!alsa_param){
        ALOGE("invalide alsa_param == NULL");
        return false;
    }

    if(pcm_out){
        if (!sample_is_playable(alsa_param->card, 
                                alsa_param->device, 
                                alsa_param->channels, 
                                alsa_param->rate, 
                                alsa_param->bits, 
                                alsa_param->period_size, 
                                alsa_param->period_count)) {
            return false;
        }
    }

    memset(&config, 0, sizeof(config));
    config.channels = alsa_param->channels;
    config.rate = alsa_param->rate;
    config.period_size = alsa_param->period_size;
    config.period_count = alsa_param->period_count;
    if (alsa_param->bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (alsa_param->bits == 24)
        config.format = PCM_FORMAT_S24_3LE;
    else if (alsa_param->bits == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    if(pcm_out){
        pcm_direction = PCM_OUT;
        s_pcm = &tinyalsa_pcm[TINYALSA_PCM_OUT];
    }
    else{
        pcm_direction = PCM_IN;
        s_pcm = &tinyalsa_pcm[TINYALSA_PCM_IN];
    }
    
    *s_pcm = pcm_open(alsa_param->card, alsa_param->device, pcm_direction, &config);
    if (!(*s_pcm) || !pcm_is_ready((*s_pcm))) {
        ALOGE("Unable to open PCM device %u (%s) d(%d) \n",
                alsa_param->device, pcm_get_error((*s_pcm)), pcm_direction);
        return false;
    }

    return true;
}

int bes_snd_pcm_play(char * buffer, uint32_t buffer_size)
{
    int ret = -1;
    
    if(!tinyalsa_pcm[TINYALSA_PCM_OUT])
        return -1;

    ret = pcm_write(tinyalsa_pcm[TINYALSA_PCM_OUT], 
                    (const char *) buffer,
                    buffer_size);

    return ret;
}

int bes_snd_pcm_cap(char * buffer, uint32_t buffer_size)
{
    int ret = -1;
    
    if(!tinyalsa_pcm[TINYALSA_PCM_IN])
        return -1;

    ret = pcm_read(tinyalsa_pcm[TINYALSA_PCM_IN],
                    buffer,
                    buffer_size);

    return ret;
}

void bes_snd_tinyalsa_close(bool pcm_out)
{
    struct pcm ** s_pcm = NULL;
    if(pcm_out){
        s_pcm = &tinyalsa_pcm[TINYALSA_PCM_OUT];
    }
    else{
        s_pcm = &tinyalsa_pcm[TINYALSA_PCM_IN];
    }

    if(*s_pcm){
        pcm_close(*s_pcm);
        *s_pcm = NULL;
    }
}
