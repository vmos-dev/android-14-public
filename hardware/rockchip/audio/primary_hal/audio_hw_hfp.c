/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_hw_hfp"

/*#define LOG_NDEBUG 0*/
#define LOG_NDDEBUG 0

#include "audio_hw.h"
#include "audio_hw_hfp.h"

/* global variable */
unsigned int bt_card;
unsigned int bt_device;
unsigned int soc_card;
unsigned int soc_device;

static struct pcm *dl_in, *dl_out;
static struct pcm *up_in, *up_out;

rk_process_api *rk_effect;

#ifdef AUDIO_HFP_DUMP_DATA
static FILE *dl_in_fp, *dl_out_fp;
static FILE *up_in_fp, *up_out_fp;
#endif

/* For HFP BT Call */
struct pcm_config config_hfp_downlink_in = {    //BT in
    .channels = AUDIO_HFP_CHANNEL,
    .rate = AUDIO_HFP_RATE_8K,
    .period_size = AUDIO_HFP_RATE_8K * AUDIO_HFP_PERIOD_DURATION / 1000,
    .period_count = AUDIO_HFP_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config config_hfp_uplink_out = {     // BT out
    .channels = AUDIO_HFP_CHANNEL,
    .rate = AUDIO_HFP_RATE_8K,
    .period_size = AUDIO_HFP_RATE_8K * AUDIO_HFP_PERIOD_DURATION / 1000,
    .period_count = AUDIO_HFP_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config config_hfp_downlink_out = {   // speak out
    .channels = AUDIO_HFP_CHANNEL,
    .rate = AUDIO_SPEAKER_RATE_48K,
    .period_size = AUDIO_SPEAKER_RATE_48K * AUDIO_HFP_PERIOD_DURATION /1000,
    .period_count = AUDIO_HFP_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config config_hfp_uplink_in = {      // mic in
    .channels = AUDIO_HFP_CHANNEL,
    .rate = AUDIO_SPEAKER_RATE_48K,
    .period_size = AUDIO_SPEAKER_RATE_48K * AUDIO_HFP_PERIOD_DURATION /1000,
    .period_count = AUDIO_HFP_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

/**
  * @brief stereo_lr_mix
  * left & right channel right mix data
  *
  * @param  indata bytes
  * @author alvis
  */
#define INT16_MIN (-32768)
#define INT16_MAX (32767)
void stereo_lr_mix(void * indata, int bytes)
{
   int i = 0;
   float channell = 0.0;
   float channelr = 0.0;
   float channelmix = 0.0;
   short *buffer = (short *)indata;

   for (i = 0; i < bytes / 2 ; i = i + 2) {
       channell = (float)(((short *)buffer)[i]);
       channelr = (float)(((short *)buffer)[i + 1]);
       if ((channell > 0.0) && (channelr > 0.0)) {
            channelmix = (channell + channelr) - ((channell * channelr) / INT16_MAX);
       } else if ((channell < 0.0) && (channelr < 0.0)) {
            channelmix = (channell + channelr) - ((channell * channelr) / INT16_MIN);
       } else {
            channelmix = channell + channelr;
       }
            ((short *)buffer)[i] = ((short *)buffer)[i + 1] = (short)channelmix;
    }
}

/* BT out <-- rk soc in */
static void* audio_hfp_run_uplink(void * args)
{
    int ret;
    struct audio_device * adev = (struct audio_device *)args;
    void *raw_data = NULL, *resampler_data = NULL;
    unsigned int resampler_in_bytes;
    unsigned int resampler_out_bytes;
    struct resampler_itfe *resampler;
    snd_pcm_uframes_t resampler_in_frame;
    snd_pcm_uframes_t resampler_out_frame;

    resampler_in_bytes =
        config_hfp_uplink_in.period_size *
        config_hfp_uplink_in.channels *
        pcm_format_to_bits(config_hfp_uplink_in.format) / 8;

    resampler_out_bytes =
        config_hfp_uplink_out.period_size *
        config_hfp_uplink_out.channels *
        pcm_format_to_bits(config_hfp_uplink_out.format) / 8;

    resampler_in_frame = config_hfp_uplink_in.period_size;
    resampler_out_frame = config_hfp_uplink_out.period_size;

    raw_data = malloc(resampler_in_bytes);
    if (raw_data == NULL) {
        ALOGE("%s: failed to allocate %d", __func__, resampler_in_bytes);
        goto fail;
    }

    resampler_data = malloc(resampler_out_bytes);
    if (resampler_data == NULL) {
        ALOGE("%s: failed to allocate %d", __func__, resampler_out_bytes);
        goto fail;
    }

    ALOGD("%s, up_in: rate: %d, period byte: %d, period frame: %ld", __func__,
        config_hfp_uplink_in.rate, resampler_in_bytes, resampler_in_frame);

    ALOGD("%s, up_out: rate: %d, period byte: %d, period frame: %ld", __func__,
        config_hfp_uplink_out.rate, resampler_out_bytes, resampler_out_frame);

    ret = create_resampler(
            config_hfp_uplink_in.rate,
            config_hfp_uplink_out.rate,
            2, RESAMPLER_QUALITY_DEFAULT, NULL, &resampler);
    if (ret != 0) {
        resampler = NULL;
        ALOGE("%s: () failure to create resampler %d", __func__, ret);
        goto fail;
    }

    ALOGD("%s open uplink in, card: %d, device: %d", __func__, soc_card, soc_device);
    up_in = pcm_open(soc_card, soc_device, PCM_IN, &config_hfp_uplink_in);
    if (up_in == 0) {
        ALOGE("%s: failed to allocate memory for PCM up_in", __func__);
        goto fail;
    } else if (!pcm_is_ready(up_in)){
        ALOGE("%s: failed to open PCM up_in", __func__);
        goto fail;
    }

    ALOGD("%s open uplink out, card: %d, device: %d", __func__, bt_card, bt_device);
    up_out = pcm_open(bt_card, bt_device, PCM_OUT, &config_hfp_uplink_out);
    if (up_out == 0) {
        ALOGE("%s: failed to allocate memory for up_out", __func__);
        goto fail;
    } else if (!pcm_is_ready(up_out)){
        ALOGE("%s: failed to open PCM up_out", __func__);
        goto fail;
    }

    ALOGD("%s uplink start data transfer!", __func__);
    while (adev->hfp_enable) {

        /* rk soc in --> BT out */
        ret = pcm_read(up_in, raw_data, resampler_in_bytes);
        if (ret != 0){
            ALOGE("%s up_in pcm_read failed! error:%d", __func__, ret);
            break;
        }

#ifdef AUDIO_HFP_DUMP_DATA
       if (up_in_fp == NULL) {
            if ((up_in_fp=fopen("/data/misc/audioserver/up_in.pcm","wb+")) == NULL) {
                ALOGE("%s Open /data/misc/audioserver/up_in.pcm failed! reason:%s \n",
                    __func__, strerror(errno));
            }
        } else {
            fwrite(raw_data,resampler_in_bytes, 1, up_in_fp);
            fflush(up_in_fp);
       }
#endif
       //volume_process(framebuf_far_out, frame_of_far_out_totol_bytes, 4.0);
       stereo_lr_mix(raw_data, resampler_in_bytes);

       ret = rk_effect->quueCaputureBuffer(raw_data, resampler_in_bytes);
       if (ret < 0) {
           break;
       }

       ret = rk_effect->getCapureBuffer(raw_data, resampler_in_bytes);
       if (ret < 0)
           memset(raw_data, 0x00, resampler_out_bytes);

       /* 48KHz resampling 16KHz*/
       resampler->resample_from_input(
                                resampler,
                                (int16_t *)raw_data,
                                (size_t *)&resampler_in_frame,
                                (int16_t *)resampler_data,
                                (size_t *)&resampler_out_frame);

        ret = pcm_write(up_out, resampler_data, resampler_out_bytes);
        if (ret != 0){
            ALOGE("%s up_out pcm_write failed! error:%d", __func__, ret);
            break;
        }

#ifdef AUDIO_HFP_DUMP_DATA
        if (up_out_fp == NULL) {
            if ((up_out_fp = fopen("/data/misc/audioserver/up_out.pcm","wb+")) == NULL) {
                ALOGE("%s open /data/misc/audioserver/up_out.pcm failed! reason:%s \n", 
                    __func__, strerror(errno));
            }
        } else {
            fwrite(resampler_data, resampler_out_bytes, 1, up_out_fp);
            fflush(up_out_fp);
        }
#endif
    }
    ALOGD("%s: uplink exit the data transfer cycle!", __func__);

fail:
    if (up_in != 0)
        pcm_close(up_in);

    if (up_out != 0)
        pcm_close(up_out);

    if (raw_data) {
        free(raw_data);
        raw_data = NULL;
    }
    if (resampler_data) {
        free(resampler_data);
        resampler_data = NULL;
    }

#ifdef AUDIO_HFP_DUMP_DATA
        if (up_in_fp != 0) {
            fclose(up_in_fp);
            up_in_fp = NULL;
        }


        if (up_out_fp != 0) {
            fclose(up_out_fp);
            up_out_fp = NULL;
        }
#endif

    ALOGD("%s out\n", __func__);
    return NULL;
}

/* BT in --> rk soc out */
static void* audio_hfp_run_downlink(void * args)
{
    int ret;
    struct audio_device * adev = (struct audio_device *)args;
    void *raw_data = NULL, *resampler_data = NULL;
    unsigned int resampler_in_bytes;
    unsigned int resampler_out_bytes;
    struct resampler_itfe *resampler;
    snd_pcm_uframes_t resampler_in_frame;
    snd_pcm_uframes_t resampler_out_frame;

    resampler_in_bytes =
        config_hfp_downlink_in.period_size *
        config_hfp_downlink_in.channels *
        pcm_format_to_bits(config_hfp_downlink_in.format) / 8;

    resampler_out_bytes =
        config_hfp_downlink_out.period_size *
        config_hfp_downlink_out.channels *
        pcm_format_to_bits(config_hfp_downlink_out.format) / 8;

    resampler_in_frame = config_hfp_downlink_in.period_size;
    resampler_out_frame = config_hfp_downlink_out.period_size;

    raw_data = malloc(resampler_in_bytes);
    if (raw_data == NULL) {
        ALOGE("%s: failed to allocate %d", __func__, resampler_in_bytes);
        goto fail;
    }

    resampler_data = malloc(resampler_out_bytes);
    if (resampler_data == NULL) {
        ALOGE("%s: failed to allocate %d", __func__, resampler_out_bytes);
        goto fail;
    }

    ALOGD("%s, dl_in: rate: %d, period byte: %d, period frame: %ld", __func__,
        config_hfp_downlink_in.rate, resampler_in_bytes, resampler_in_frame);

    ALOGD("%s, dl_out: rate: %d, period byte: %d, period frame: %ld", __func__,
        config_hfp_downlink_out.rate, resampler_out_bytes, resampler_out_frame);

    ret = create_resampler(
            config_hfp_downlink_in.rate,
            config_hfp_downlink_out.rate,
            2, RESAMPLER_QUALITY_DEFAULT, NULL, &resampler);
    if (ret != 0) {
        resampler = NULL;
        ALOGE("%s: () failure to create resampler %d", __func__, ret);
        goto fail;
    }

    ALOGD("%s open downlink in, card: %d, device: %d", __func__, bt_card, bt_device);
    dl_in = pcm_open(bt_card, bt_device, PCM_IN, &config_hfp_downlink_in);
    if (dl_in == 0) {
        ALOGE("%s: failed to allocate memory for PCM dl_in", __func__);
        goto fail;
    } else if (!pcm_is_ready(dl_in)) {
        ALOGE("%s: failed to open PCM dl_in", __func__);
        goto fail;
    }

    ALOGD("%s open downlink out, card: %d, device: %d", __func__, soc_card, soc_device);
    dl_out = pcm_open(soc_card, soc_device, PCM_OUT, &config_hfp_downlink_out);
    if (dl_out == 0) {
        ALOGE("%s: failed to allocate memory for PCM dl_out", __func__);
        goto fail;
    } else if (!pcm_is_ready(dl_out)) {
        ALOGE("%s: failed to open PCM dl_out", __func__);
        goto fail;
    }

    ALOGD("%s downlink start data transfer!", __func__);
    while (adev->hfp_enable) {

        /* BT in --> rk soc out */
        ret = pcm_read(dl_in, raw_data, resampler_in_bytes);
        if (ret != 0) {
            ALOGE("%s dl_in pcm_read failed! error:%d", __func__, ret);
            break;
        }

#ifdef AUDIO_HFP_DUMP_DATA
        if (dl_in_fp == NULL) {
            if ((dl_in_fp = fopen("/data/misc/audioserver/dl_in.pcm","wb+")) == NULL) {
                ALOGE("%s open /data/misc/audioserver/dl_in.pcm failed! reason:%s \n", __func__, strerror(errno));
            }
        } else {
            fwrite(raw_data,resampler_in_bytes, 1, dl_in_fp);
            fflush(dl_in_fp);
        }
#endif

        stereo_lr_mix(raw_data, resampler_in_bytes);

        /* 16KHz resampling 48KHz*/
        resampler->resample_from_input(
                                    resampler,
                                    (int16_t *)raw_data,
                                    (size_t *)&resampler_in_frame,
                                    (int16_t *)resampler_data,
                                    (size_t *)&resampler_out_frame);

        ret = rk_effect->queuePlaybackBuffer(resampler_data, resampler_out_bytes);
        if (ret < 0) {
            break;
        }

        ret = rk_effect->getPlaybackBuffer(resampler_data, resampler_out_bytes);
        if (ret < 0)
            memset(resampler_data, 0x00, resampler_out_bytes);

        ret = pcm_write(dl_out, resampler_data, resampler_out_bytes);
        if (ret != 0) {
            ALOGE("%s dl_out pcm_write failed! error:%d", __func__, ret);
            break;
        }

#ifdef AUDIO_HFP_DUMP_DATA
        if (dl_out_fp == NULL) {
            if ((dl_out_fp = fopen("/data/misc/audioserver/dl_out.pcm","wb+")) == NULL) {
                ALOGE("%s open /data/misc/audioserver/dl_out.pcm failed! reason:%s \n", __func__, strerror(errno));
            }
        } else {
            fwrite(resampler_data,resampler_out_bytes, 1, dl_out_fp);
            fflush(dl_out_fp);
        }
#endif
    }
    ALOGD("%s: downlink exit data transfer cycle!", __func__);

fail:
    if (dl_in != 0)
        pcm_close(dl_in);

    if (dl_out != 0)
        pcm_close(dl_out);

    if (raw_data){
        free(raw_data);
        raw_data = NULL;
    }
    if (resampler_data) {
        free(resampler_data);
        resampler_data = NULL;
    }

#ifdef AUDIO_HFP_DUMP_DATA
    if (dl_in_fp != 0) {
        fclose(dl_in_fp);
        dl_in_fp = NULL;
    }

    if (dl_out_fp != 0) {
        fclose(dl_out_fp);
        dl_out_fp = NULL;
    }

#endif

    ALOGD("%s out\n", __func__);
    return NULL;
}

void audio_hfp_run(struct audio_device *adev)
{
    adev->hfp_enable = true;

    bt_card = AUDIO_HFP_BT_CARD; bt_device = AUDIO_HFP_BT_DEV;
    soc_card = AUDIO_HFP_SOC_CARD; soc_device = AUDIO_HFP_SOC_DEV;

    ALOGD("3a process has opened, try to create 3a process!");
    rk_effect = rk_voiceprocess_create(config_hfp_downlink_out.rate,
                                    config_hfp_downlink_out.channels,
                                    config_hfp_uplink_in.rate,
                                    config_hfp_uplink_in.channels);
    if (rk_effect == NULL) {
        ALOGE("crate voice process failed!");
        return ;
    }

    rk_effect->start();

    pthread_create(&adev->hfp_downlink_thread, NULL, &audio_hfp_run_downlink, adev);
    pthread_create(&adev->hfp_uplink_thread, NULL, &audio_hfp_run_uplink, adev);
    return ;
}

void audio_hfp_stop(struct audio_device *adev)
{
    adev->hfp_enable = false;
    return ;
}

void audio_hfp_set_sample_rate(unsigned int rate)
{

    ALOGD("%s: rate: %d\n", __func__, rate);
    switch(rate) {
        case AUDIO_HFP_RATE_8K: {
            // soc Fixed resampling to 48k

            config_hfp_downlink_in.rate = AUDIO_HFP_RATE_8K;
            config_hfp_downlink_in.period_size =
                        AUDIO_HFP_RATE_8K * AUDIO_HFP_PERIOD_DURATION / 1000;

            config_hfp_uplink_out.rate = AUDIO_HFP_RATE_8K;
            config_hfp_uplink_out.period_size =
                        AUDIO_HFP_RATE_8K * AUDIO_HFP_PERIOD_DURATION / 1000;
        } break;

        case AUDIO_HFP_RATE_16K: {
            // soc Fixed resampling to 48k

            config_hfp_downlink_in.rate = AUDIO_HFP_RATE_16K;
            config_hfp_downlink_in.period_size =
                        AUDIO_HFP_RATE_16K * AUDIO_HFP_PERIOD_DURATION / 1000;

            config_hfp_uplink_out.rate = AUDIO_HFP_RATE_16K;
            config_hfp_uplink_out.period_size =
                        AUDIO_HFP_RATE_16K * AUDIO_HFP_PERIOD_DURATION / 1000;
        } break;

        default:
            ALOGE("Unsupported rate..");
            break;
    }

    return ;
}

void audio_hfp_set_volume(struct audio_device *adev, float value)
{
    ALOGD("%s: %f\n", __func__, value);
}

