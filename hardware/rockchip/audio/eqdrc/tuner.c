/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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

#define LOG_TAG "eqdrc-tuner"
// #define LOG_NDEBUG 0

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <cutils/log.h>
#include "profile.h"

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

struct tuner {
    int fd;
    int wd;
    pthread_t thread;
    bool initialized;
    bool synced;
    char fname[PATH_MAX];
    char events[BUF_LEN];
};

static struct tuner tuner = {
    -1,
    -1,
    -1,
    false,
    false,
    { 0 },
    { 0 }
};

static void *tuner_thread(void *args __unused)
{
    struct inotify_event *event;
    char *buf;
    char fname[PATH_MAX];
    int ret;

    while (true) {
        ret = read(tuner.fd, tuner.events, sizeof(tuner.events));
        if (ret <= 0)
            continue;

        for (buf = tuner.events; buf < (tuner.events + ret);) {
            event = (struct inotify_event *)buf;
            buf += sizeof(struct inotify_event) + event->len;

            /* Not a file. */
            if (event->mask & IN_ISDIR)
                continue;

            snprintf(fname, sizeof(fname), "/vendor/etc/%s", event->name);
            /* Not the binary file pushed by our util. */
            if (strncmp(fname, PROFILE_PATH_PREFIX, strlen(PROFILE_PATH_PREFIX)))
                continue;

            if (event->mask & IN_MODIFY) {
                strncpy(tuner.fname, fname, PATH_MAX);
                tuner.synced = true;
            }
        }
    }

    return NULL;
}

bool tuner_initialized(void)
{
    return tuner.initialized;
}

bool tuner_synced(void)
{
    return tuner.synced;
}

int tuner_init(void)
{
    int ret;

    tuner.fd = inotify_init();
    if (tuner.fd < 0) {
        ALOGE("failed to initialized inotify");
        return -errno;
    }

    tuner.wd = inotify_add_watch(tuner.fd, "/vendor/etc", IN_MODIFY);
    if (tuner.wd < 0) {
        ALOGE("failed to add watch");
        close(tuner.fd);
        return -errno;
    }

    ret = pthread_create(&tuner.thread, NULL, tuner_thread, &tuner);
    if (ret) {
        inotify_rm_watch(tuner.fd, tuner.wd);
        close(tuner.fd);
        return ret;
    }

    tuner.initialized = true;

    return 0;
}

int tuner_sync_profile(struct profile *profile, unsigned int sampling_rate, unsigned channels)
{
    char fname[PATH_MAX];
    int ret;

    snprintf(fname, sizeof(fname), PROFILE_PATH_FMT, sampling_rate, channels);

    /* Not the expected profile. */
    if (strcmp(tuner.fname, fname)) {
        ALOGE("%s: got profile %s, expected %s", __func__, tuner.fname, fname);
        ret = -EAGAIN;
        goto out;
    }

    ret = profile_read(profile, fname);
    if (ret)
        goto out;

    ALOGV("%s: houston we are good to go", __func__);

out:
    /* Clear the flag. */
    tuner.synced = false;

    return ret;
}
