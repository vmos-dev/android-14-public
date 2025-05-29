/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * @file    voice_eq_preprocess.c
 * @author  kevan.lan <kevan.lan@rock-chips.com>
 *          lsh <lsh@rock-chips.com>
 * @date    2020-04-18
 */

//#define LOG_NDEBUG 0

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <dlfcn.h>  // for dlopen/dlclose
#include <fcntl.h>
#include <sys/inotify.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <speex/speex_preprocess.h>
#include <speex/speex_resampler.h>

#include "voice_eq_preprocess.h"

#define LOG_TAG "voice_eq_process"

typedef struct rk_eq_drc_handle_ {
    rk_eq_drc_api *eq_drc_Api;
    void *eq_drc_LibHandle;
    unsigned long period_size;
    unsigned long frame_size;
    /* unsigned long frame_count; */
    char *param_name;
    char *debug_file; /* "/data/debug.pcm" */
#ifdef EQ_DRC_DUMP_AUDIO_DATA
    char *raw_file;
    char *processed_file;
    FILE *raw_fd;
    FILE *processed_fd;
#endif
#ifdef CONFIG_FILE_INOTIFY
    int inotify_fd;
    int wd;
    pthread_t inotify_thread;
    pthread_mutex_t wait_lock;
#endif
    void *presidue_audio;
    int presidue_length;
    struct AUDIOPOST_STRUCT *AudioPostHandle;
} rk_eq_drc_handle;

static rk_eq_drc_handle *eq_drc_handle = NULL;

#ifdef CONFIG_FILE_INOTIFY

int open_inotify_fd()
{
    int fd;

    fd = inotify_init();
    if (fd < 0) {
        ALOGE("inotify_init () = ");
    }
    return fd;
}

int close_inotify_fd(int fd)
{
    int ret;

    if ((ret = close (fd)) < 0) {
        ALOGE("close (fd) = ");
    }
    return ret;
}

int event_check(int fd)
{
    struct timeval seltime;
    fd_set rfds;
    int ret;

    seltime.tv_sec = 1;
    seltime.tv_usec = 0;
    FD_ZERO (&rfds);
    FD_SET (fd, &rfds);
    ret = select (fd + 1, &rfds, NULL, NULL, &seltime);
    if (0 == ret) {
        /* ALOGD("select time out"); */
    } else if (ret > 0) {
        ALOGD("select return %d", ret);
    } else {
        ALOGD("select error");
    }
    return ret;
}

void _do_update(void *_handle)
{
    FILE *fp;
    float reset_para[PARALEN] = {0};
    rk_eq_drc_handle *handle =  (rk_eq_drc_handle *)_handle;

    ALOGD("_do_update....");
    pthread_mutex_lock(&handle->wait_lock);
    if (handle->param_name) {
        fp = fopen(handle->param_name, "rb");
        if (fp != NULL) {
            fread(reset_para, sizeof(float), sizeof(reset_para)/sizeof(reset_para[0]), fp);
            handle->eq_drc_Api->pfAudioPost_SetPara(eq_drc_handle->AudioPostHandle,
                                                        reset_para, handle->period_size);
            ALOGD("AudioPost_SetPara %s, %d", handle->param_name, handle->period_size);
            fclose(fp);
        } else {
            ALOGE("_do_update fail");
        }
    }
    pthread_mutex_unlock(&handle->wait_lock);

}

void _inotify_event_handler(void *_handle, struct inotify_event *event)
{
    rk_eq_drc_handle *handle =  (rk_eq_drc_handle *)_handle;

    if (handle->wd == event->wd && (event->mask & IN_CREATE )) {
        ALOGD("_inotify_event_handler: IN_CREATE");
        return;
    }
    if (handle->wd == event->wd && (event->mask & IN_IGNORED )) {
        ALOGD("_inotify_event_handler: IN_IGNORED");
        if(inotify_rm_watch(handle->inotify_fd, handle->wd)) {
            ALOGE("_inotify_event_handler: rm_watch error %d\n", errno);
        }
        handle->wd = -1;
        return;
    }
    if (handle->wd == event->wd && (event->mask & IN_MODIFY)) {
        ALOGD("_inotify_event_handler: IN_MODIFY");
        _do_update(_handle);
    }
}

int read_events(void *handle, int fd)
{
    char buffer[1024];
    size_t buffer_i, ret, event_size;
    struct inotify_event *pevent;
    int count = 0;

    ret = read(fd, buffer, 1024);
    if (ret <= 0)
        return ret;
    buffer_i = 0;
    while (buffer_i < ret) {
        pevent = (struct inotify_event *)&buffer[buffer_i];
        event_size =  offsetof(struct inotify_event, name) + pevent->len;
        buffer_i += event_size;
        _inotify_event_handler(handle, pevent);
        count++;
    }
    return count;
}

int process_inotify_events(void *_handle, int fd)
{
    rk_eq_drc_handle *handle =  (rk_eq_drc_handle *)_handle;
    ALOGD("process_inotify_events...");

    while (1) {
        if (handle->wd < 0 ) {
            ALOGD("config file readd %d", handle->wd);
            handle->wd = inotify_add_watch(handle->inotify_fd, handle->param_name, IN_ALL_EVENTS);
            if (handle->wd >= 0) {
                /* Non-existed -> existed, reparse immediately.*/
                _do_update(_handle);
            }
        }
        if (event_check(fd) > 0) {
            int i = read_events(handle, fd);
        }
    }
    ALOGD("process_inotify_events exit");
    return 0;
}

static void* inotify_threadproc(void *args)
{
    rk_eq_drc_handle *handle = (rk_eq_drc_handle *)args;

    ALOGD("inotify_threadproc...");
    process_inotify_events(handle, handle->inotify_fd);
    return NULL;
}

#endif

int rk_eq_drc_destory()
{
#ifdef CONFIG_FILE_INOTIFY
    /* need some operates to free inotify resources, skip...*/
#endif
    if (eq_drc_handle == NULL) {
        ALOGD("rk_eq_drc_destory return");
        return 0;
    }
    /* AudioPost_Destroy(); */
    if (eq_drc_handle->eq_drc_Api) {
        eq_drc_handle->eq_drc_Api->pfAudioPost_Destroy(eq_drc_handle->AudioPostHandle);
        free(eq_drc_handle->eq_drc_Api);
    }
    if (eq_drc_handle->eq_drc_LibHandle) {
        dlclose(eq_drc_handle->eq_drc_LibHandle);
        eq_drc_handle->eq_drc_LibHandle = NULL;
    }
    free(eq_drc_handle);
    eq_drc_handle = NULL;
    ALOGD("rk_eq_drc_destory free and return");
    return 0;
}

int rk_eq_drc_process(const void *in_buffer, void *out_buffer,
                      unsigned long size, int pcm_channel, int bit_per_sample)
{
    rk_eq_drc_handle *handle =  eq_drc_handle;
    int frame_size, process_count;
    short *in16, *out16;
    char *in8, *out8;
    int x,y;

    frame_size = eq_drc_handle->period_size * eq_drc_handle->frame_size;
    if (size != frame_size) {
        ALOGE("input_size:%d must equal to frame_size:%d", size, frame_size);
    }
    process_count = 1;
#ifdef EQ_DRC_DUMP_AUDIO_DATA
    if (handle->raw_fd) { /* dump raw data */
        fwrite(in_buffer, size,1, handle->raw_fd);
    }
#endif

    switch (bit_per_sample) {
    case 8:
        in8 = in_buffer;
        out8 = out_buffer;
        break;
    case 16:/* S16_LE */
        for (x = 0; x < process_count; x++) { /* max frame count can be processed one time */
            in16 = &in_buffer[x*eq_drc_handle->frame_size];
            out16 = &out_buffer[x*eq_drc_handle->frame_size];
#ifdef CONFIG_FILE_INOTIFY
            pthread_mutex_lock(&handle->wait_lock);
            eq_drc_handle->eq_drc_Api->pfAudioPost_Process(eq_drc_handle->AudioPostHandle,
                                            in16, out16, pcm_channel, eq_drc_handle->period_size);
            pthread_mutex_unlock(&handle->wait_lock);
#else
            eq_drc_handle->eq_drc_Api->pfAudioPost_Process(eq_drc_handle->AudioPostHandle,
                                            in16, out16, pcm_channel, eq_drc_handle->period_size);
#endif
        }
        break;
    case 24:
            break;
    case 32:
            break;
    default:
            break;
    }

#ifdef EQ_DRC_DUMP_AUDIO_DATA
    if (handle->processed_fd) { /* dump processed data */
        fwrite(out_buffer, size,1, handle->processed_fd);
    }
#endif
    return 0;
}

int start_record()
{
    rk_eq_drc_handle *handle =  eq_drc_handle;

    ALOGD("start_record");
#ifdef EQ_DRC_DUMP_AUDIO_DATA
    if (handle->raw_file) {
        handle->raw_fd = fopen(handle->raw_file,"wb");
        if (!handle->raw_fd) {
            ALOGE("fopen %s failed", handle->raw_file);
        }
    }
    if (handle->processed_file) {
        handle->processed_fd = fopen(handle->processed_file,"wb");
        if (!handle->processed_fd) {
            ALOGE("fopen %s failed", handle->processed_file);
        }
    }
#endif
    return 0;
}

int stop_record()
{
    rk_eq_drc_handle *handle =  eq_drc_handle;

    ALOGD("stop_record");
#ifdef EQ_DRC_DUMP_AUDIO_DATA
    if (handle->raw_fd) {
        fclose(handle->raw_fd);
    }
    if (handle->processed_fd) {
        fclose(handle->processed_fd);
    }
#endif
    return 0;
}

rk_eq_drc_api *rk_eq_drc_create(const char *param_name,
                                unsigned long period_size, unsigned long frame_size)
{
    int rc;
    static char __param_name[100];
    char *so = "/vendor/lib/libRK_AudioProcess.so";

    if (eq_drc_handle) {
        ALOGW(" eq_drc_ handle has already opened, return");
        return eq_drc_handle->eq_drc_Api;
    }
    if (! period_size ) {
        ALOGE("wrong parameters param_name:%x size:%d", param_name, period_size);
        return NULL;
    }
    if (!param_name) {
        sprintf(__param_name, "%s", "/sdcard/Para.bin");
    }
    sprintf(__param_name, "%s", param_name);
    ALOGD("rk_eq_drc_create param_name:%s size:%d", __param_name, period_size);
    eq_drc_handle = (rk_eq_drc_handle *)malloc(sizeof(rk_eq_drc_handle));
    if (eq_drc_handle == NULL) {
        ALOGE("rk_eq_drc_handle malloc failed");
        return NULL;
    }
    eq_drc_handle->eq_drc_Api = (rk_eq_drc_api *)malloc(sizeof(rk_eq_drc_api));
    if (eq_drc_handle->eq_drc_Api == NULL) {
        ALOGE("rk_eq_drc_api malloc failed!");
        goto failed;
    }
    eq_drc_handle->param_name = __param_name;
    float reset_para[PARALEN] = {0};
    if (1) {
        FILE *fp;
        int rc, wc;

        fp = fopen(eq_drc_handle->param_name, "rb");
        if (fp != NULL) {
            wc = sizeof(reset_para)/sizeof(reset_para[0]);
            rc = fread(reset_para, sizeof(reset_para[0]), wc , fp);
            ALOGD("read %s:want count %d, read count %d", eq_drc_handle->param_name, wc, rc);
            fclose(fp);
        } else {
            ALOGD("fopen(%s, \"rb\") faile ", eq_drc_handle->param_name);
        }
        ALOGD("sampleRate is %f, bitRate is %f, nChan is %f, Link is %f",
            reset_para[0], reset_para[1], reset_para[2], reset_para[3]);
    }

    ALOGD("loading %s", so);
    eq_drc_handle->eq_drc_LibHandle = dlopen(so, RTLD_LAZY);
    if (!eq_drc_handle->eq_drc_LibHandle) {
        ALOGE("open so failed %s", dlerror());
        goto failed;
    }
    eq_drc_handle->eq_drc_Api->pfAudioPost_Init =
                (AudioPost_Init *)dlsym(eq_drc_handle->eq_drc_LibHandle, "AudioPost_Init");
    eq_drc_handle->eq_drc_Api->pfAudioPost_Destroy =
                (AudioPost_Destroy *)dlsym(eq_drc_handle->eq_drc_LibHandle, "AudioPost_Destroy");
    eq_drc_handle->eq_drc_Api->pfAudioPost_Process =
                (AudioPost_Process *)dlsym(eq_drc_handle->eq_drc_LibHandle, "AudioPost_Process");
    eq_drc_handle->eq_drc_Api->pfAudioPost_SetPara =
                (AudioPost_SetPara *)dlsym(eq_drc_handle->eq_drc_LibHandle, "AudioPost_SetPara");
    if ((!eq_drc_handle->eq_drc_Api->pfAudioPost_Init)||
        (!eq_drc_handle->eq_drc_Api->pfAudioPost_Destroy)||
        (!eq_drc_handle->eq_drc_Api->pfAudioPost_Process)||
        (!eq_drc_handle->eq_drc_Api->pfAudioPost_SetPara)) {
        ALOGE("load api failed!");
        goto failed;
    }

    eq_drc_handle->eq_drc_Api->rk_eq_drc_process = rk_eq_drc_process;
    eq_drc_handle->eq_drc_Api->start_record = start_record;
    eq_drc_handle->eq_drc_Api->stop_record = stop_record;
    eq_drc_handle->presidue_audio = NULL;
    eq_drc_handle->presidue_length = 0;
    eq_drc_handle->period_size = period_size;
    eq_drc_handle->frame_size = frame_size;
    /* eq_drc_handle->frame_count = period_size/frame_size;*/
#ifdef EQ_DRC_DUMP_AUDIO_DATA
    eq_drc_handle->raw_file = "/data/raw.pcm";
    eq_drc_handle->processed_file = "/data/processed.pcm";
#endif
    ALOGD("pfAudioPost_Init %s %d", eq_drc_handle->param_name, eq_drc_handle->period_size);
    eq_drc_handle->AudioPostHandle =
        eq_drc_handle->eq_drc_Api->pfAudioPost_Init(reset_para, eq_drc_handle->period_size);
    if (eq_drc_handle->AudioPostHandle == NULL) {
            ALOGD("Create audiopost handle fail...");
        }

#ifdef CONFIG_FILE_INOTIFY
    {
        char *notify_file;

        ALOGD("opening inotify fd....");
        notify_file = eq_drc_handle->param_name;
        eq_drc_handle->inotify_fd = open_inotify_fd();
        if (eq_drc_handle->inotify_fd > 0 ) {
            ALOGD("adding inotify file %s %d", notify_file, eq_drc_handle->inotify_fd);
            eq_drc_handle->wd =
                inotify_add_watch(eq_drc_handle->inotify_fd, notify_file,
                                  IN_ALL_EVENTS);
            if (eq_drc_handle->wd < 0) {
                ALOGE("inotify_add_watch fail %d", errno);
                close_inotify_fd(eq_drc_handle->inotify_fd);
                eq_drc_handle->inotify_fd = -1;
            } else {
                ALOGD("pthread_mutex_init");
                pthread_mutex_init(&eq_drc_handle->wait_lock, NULL);
                ALOGD("creating inotify thread");
                rc = pthread_create(&eq_drc_handle->inotify_thread, NULL,
                                    inotify_threadproc, eq_drc_handle);
                if (rc) {
                    ALOGE("inotify thread is not created.");
                    inotify_rm_watch(eq_drc_handle->inotify_fd, eq_drc_handle->wd);
                    close_inotify_fd(eq_drc_handle->inotify_fd);
                    eq_drc_handle->inotify_fd = -1;
                }
            }
        } else {
            ALOGE("opening inotify fd fail");
        }
    }
#endif

    return eq_drc_handle->eq_drc_Api;
failed:
    if (eq_drc_handle) {
        if (eq_drc_handle->eq_drc_LibHandle) {
            dlclose(eq_drc_handle->eq_drc_LibHandle);
            eq_drc_handle->eq_drc_LibHandle = NULL;
        }
        if (eq_drc_handle->eq_drc_Api) {
            free(eq_drc_handle->eq_drc_Api);
        }
        free(eq_drc_handle);
        eq_drc_handle = NULL;
    }
    return NULL;
}
