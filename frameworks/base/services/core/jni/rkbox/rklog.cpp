/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd
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

#define LOG_TAG "RKLOG"

#define LOG_NDEBUG 0
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <sys/inotify.h>
#include <cutils/properties.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <log/log.h>
#include <processgroup/sched_policy.h>

#include <vector>


#define DROBOX_PATH "/data/system/dropbox"
#define TOMBSTONE_PATH "/data/tombstones"
#define MAX_EPOLL_EVENTS 8
#define DROPBOX_MIN_REPEAT_DURATION 5*60 //2 generate a bugreport if there is a same dropbox every 5 minutes
#define TOMBSTONE_MIN_REPEAT_DURATION 2*60 //2 generate a bugreport if there is a same tombstone every 2 minutes
struct dropbox_info {
    unsigned int type;
    char* name;
    unsigned int hash;
    unsigned int repeat_times;
    unsigned int trig_times;
    long time;
};

//we check the backtrace of tombstone to judge whether this tombstone happened again
struct tombstone_info {
    unsigned int backtrace_hash;
    unsigned int repeat_times;
    unsigned int trig_times;
    long time;
};

enum {
    DROPBOX_SYSTEM_SERVER_CRASH = 1,
    DROPBOX_SYSTEM_SERVER_ANR = 2,
    DROPBOX_SYSTEM_SERVER_LOWMEM = 3,
    DROPBOX_SYSTEM_SERVER_WATCHDOG = 4,
    DROPBOX_SYSTEM_BOOT = 5,
    DROPBOX_SYSTEM_RESTART = 6,
    DROPBOX_SYSTEM_APP_CRASH = 7,
    DROPBOX_SYSTEM_APP_ANR = 8,
    DROPBOX_SYSTEM_SERVER_WTF = 9,
    DROPBOX_SYSTEM_APP_WTF = 10,
    DROPBOX_DATA_APP_ANR = 11,
    DROPBOX_DATA_APP_CRASH =12,
    DROPBOX_DATA_APP_WTF = 13,
    DROPBOX_MAX_ENTRY,
};

//default we only enable system level event and data_app_anr
//ignore data_app_crash and data_app_wtf
int dropboxLevel = DROPBOX_SYSTEM_APP_ANR;
const char *dropboxName[DROPBOX_MAX_ENTRY] = {
    "system_server_crash",
    "system_server_anr",
    "system_server_lowmem",
    "system_server_watchdog",
    "SYSTEM_BOOT",
    "SYSTEM_RESTART",
    "system_app_crash",
    "system_app_anr",
    "system_server_wtf",
    "system_app_wtf",
    "data_app_anr",
    "data_app_crash",
    "data_app_wtf",
};
std::vector<dropbox_info> dropboxData;
std::vector<tombstone_info> tombstoneData;
int inotify_fd = 0;
int dropbox_wd = 0;
int tombstone_wd = 0;
int epoll_fd = 0;
bool stop_thread = false;
pthread_t rklog_thread;

static unsigned int gen_file_hash(char *path)
{
    unsigned int hash = 5381;
    char buf[512];
    FILE *fd;

    fd = fopen(path, "rb");
    if (fd == NULL){
        ALOGE("can not open file: %s\n", path);
        return 0;
    }
    while(!feof(fd)){
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), fd) != NULL) {
            char *tmp = buf;
            //because the pid and process-runtime value is always different, so skip caculate the hash of this line
            if (strstr(buf, "PID") || strstr(buf, "Process-Runtime"))
                continue;
            while (*tmp)
                hash += (hash << 5) + (*tmp++);
        }
    }

    fclose(fd);
    return (hash & 0x7FFFFFFF);
}

static int add_to_epoll(int fd, int epoll_fd)
{
    int result;
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = fd;
    result = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);

    return result;
}

static long get_uptime()
{
    struct sysinfo info;

    if (sysinfo(&info)) {
        ALOGE("Failed to get sysinfo, errno:%d, reason %s", errno, strerror(errno));
        return 0;
    }

    return info.uptime;
}

static int trig_bugreport(const char * bugreport_reason, long time)
{
    int retry = 5;
    char prop[PROPERTY_VALUE_MAX];

    do{
        property_get("dumpstate.completed", prop, "0");
        if (!strcmp(prop, "1"))
            break;
        sleep(2);
    } while (retry--);

    property_set("dumpstate.completed", "0");
    property_set("sys.bugreport_reason", bugreport_reason);
    property_set("ctl.start", "simple_bugreportd");

    return 0;
}

static int store_dropbox_and_trig_bugreport(struct dropbox_info* info)
{
    int i;
    bool is_exist = false;

    for(i = 0; i < dropboxData.size(); i++) {
        if(dropboxData[i].hash == info->hash) {
            if (info->time - dropboxData[i].time <= DROPBOX_MIN_REPEAT_DURATION) {
                dropboxData[i].repeat_times++;
                ALOGD("%s hash[0x%x] is same as [%d], dont gen bugreport\n", info->name, info->hash, i);
                return -1;
            }
            dropboxData[i].repeat_times++;
            dropboxData[i].trig_times++;
            dropboxData[i].time = info->time;
            is_exist = true;
            break;
        }
    }

    if (!is_exist) {
        info->trig_times = 1;
        dropboxData.push_back(*info);
        ALOGD("=========generate dropbox bugreport: {%d, %s, 0x%x, %d, %d, %ld}\n",
              info->type, info->name, info->hash, info->repeat_times, info->trig_times, info->time);
    } else {
        ALOGD("=========generate dropbox bugreport[idx=%d]: {%d, %s, 0x%x, %d, %d, %ld}\n",
              i, dropboxData[i].type, dropboxData[i].name, dropboxData[i].hash,
              dropboxData[i].repeat_times, dropboxData[i].trig_times, dropboxData[i].time);
    }
    trig_bugreport(info->name, info->time);
    return 0;
}

static int parse_dropbox_event(struct inotify_event *event)
{
    char path[256] = {0};
    int i;
    struct dropbox_info info = {0, NULL, 0, 0, 0, 0};

    if (event->mask & IN_ISDIR)
        return 0;
    if (event->len) {
        snprintf(path, sizeof(path),"%s/%s", DROBOX_PATH, event->name);
        for (i = 0; i < DROPBOX_MAX_ENTRY - 1; i++) {
            if (strstr(event->name, dropboxName[i]))
                break;
        }

        if (i > dropboxLevel - 1) {
//            ALOGD("NOT found valid dropbox, %s\n", event->name);
            return 0;
        }
        info.hash = gen_file_hash(path);
        if (info.hash == 0) {
             ALOGE("Dropbox hash is zero, %s\n", path);
             return -1;
        }

        info.type = i;
        info.name = (char*)dropboxName[i];
        info.repeat_times = 1;
        info.time = get_uptime();
        store_dropbox_and_trig_bugreport(&info);
    }

    return 0;
}

/*
 * a tombstone example:
 * backtrace:
 *         #00 pc 000000000004facc  /apex/com.android.runtime/lib64/bionic/libc.so (abort+164) (BuildId: cd7952cb40d1a2deca6420c2da7910be)
 *         #01 pc 00000000000a2444  /system/lib64/libchrome.so (base::debug::BreakDebugger()+40) (BuildId: 54be86fb8ea0142c23d79fa581006a9a)
 *         #02 pc 00000000000bbd4c  /system/lib64/libchrome.so (logging::LogMessage::~LogMessage()+1308) (BuildId: 54be86fb8ea0142c23d79fa581006a9a)
 * we only use the strings after charactor "/" to generate a hash code
 */
static unsigned int gen_tombstone_hash(char *path)
{
    unsigned int hash = 5381;
    char buf[512];
    FILE *fd;
    bool bt_found = false;
    int max_check_bt = 5; // check 5 backtrace line to caculate the hash

    fd = fopen(path, "rb");
    if (fd == NULL){
        ALOGE("can not open file: %s\n", path);
        return 0;
    }
    while(!feof(fd)){
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), fd) != NULL) {
            if (!bt_found) {
                if (strstr(buf, "backtrace:"))
                    bt_found = true;
                continue;
            }
            if (max_check_bt-- <= 0)
                break;
            char *pos = strchr(buf, '/');
            if (pos == NULL)
                continue;
            while (*pos)
                hash += (hash << 5) + (*pos++);
        }
    }

    fclose(fd);
    return (hash & 0x7FFFFFFF);
}

static int store_tombstone_and_trig_bugreport(struct tombstone_info* info)
{
    int i;
    bool is_exist = false;
    const char* reason = "TOMBSTONE";

    for(i = 0; i < tombstoneData.size(); i++) {
        if(tombstoneData[i].backtrace_hash == info->backtrace_hash) {
        if (info->time - tombstoneData[i].time <= TOMBSTONE_MIN_REPEAT_DURATION) {
                tombstoneData[i].repeat_times++;
                ALOGD("TOMBSTONE hash[0x%x] is same as [%d], dont gen bugreport\n", info->backtrace_hash, i);
                return -1;
            }
            tombstoneData[i].repeat_times++;
            tombstoneData[i].trig_times++;
            tombstoneData[i].time = info->time;
            is_exist = true;
            break;
        }
    }

    if (!is_exist) {
        info->trig_times = 1;
        tombstoneData.push_back(*info);
        ALOGD("=========generate tombstone bugreport: {0x%x, %d, %d, %ld}\n",
              tombstoneData[i].backtrace_hash, tombstoneData[i].repeat_times,
              tombstoneData[i].trig_times, tombstoneData[i].time);
    } else {
        ALOGD("=========generate tombstone bugreport[idx=%d]: {0x%x, %d, %d, %ld}\n",
              i, info->backtrace_hash, tombstoneData[i].repeat_times,
              tombstoneData[i].trig_times, info->time);
    }
    trig_bugreport(reason, info->time);

    return 0;
}

static int parse_tombstone_event(struct inotify_event *event)
{
    char path[256] = {0};
    struct tombstone_info info = {0, 0, 0, 0};

    if (event->mask & IN_ISDIR || strstr(event->name, ".pb"))
        return 0;

    if (event->len) {
        snprintf(path, sizeof(path),"%s/%s", TOMBSTONE_PATH, event->name);
        info.time = get_uptime();
        info.backtrace_hash = gen_tombstone_hash(path);
        if (info.backtrace_hash == 0) {
            ALOGE("Tombstone hash is zero, %s\n", path);
            return -1;
        }
        info.repeat_times = 1;
        store_tombstone_and_trig_bugreport(&info);
    }

    return 0;
}

/*
 * Clear data if android is restarted.
 */
static void rklog_clr_data(void){
    if (inotify_fd) {
        if (dropbox_wd) {
            inotify_rm_watch(inotify_fd, dropbox_wd);
            dropbox_wd = 0;
        }
        if (tombstone_wd) {
            inotify_rm_watch(inotify_fd, tombstone_wd);
            tombstone_wd = 0;
        }
        if (epoll_fd) {
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, inotify_fd, nullptr) == -1)
                ALOGE("EPOLL_CTL_DEL failed: %s\n", strerror(errno));
            epoll_fd = 0;
        }
        dropboxData.clear();
        tombstoneData.clear();
        inotify_fd = 0;
        ALOGE("Clear data because android is restarted\n");
    }
}

static void *rklog_monitor_thread(void *arg) {
    struct epoll_event pending_evs[MAX_EPOLL_EVENTS];
    int len, ev_pos;
    char prop[PROPERTY_VALUE_MAX];
    char buf[512];
    int i;

    set_sched_policy(0, SP_BACKGROUND);
    if (cpusets_enabled())
        set_cpuset_policy(0, SP_BACKGROUND);

    while (!stop_thread) {
         int poll_res = epoll_wait(epoll_fd, pending_evs, MAX_EPOLL_EVENTS, -1);
         if (poll_res <= 0) {
             ALOGE("Epoll wait failed, errno = %s.\n", strerror(errno));
             continue;
         }
         if (property_get("persist.sys.rklog.level", prop, NULL) > 0)
             dropboxLevel = atoi(prop);
         if (dropboxLevel == 0) {
//             ALOGD("Don't trigger rkbugreport because persist.sys.rklog.level is 0\n");
             for (i = 0; i < poll_res; i++) {
                 read(pending_evs[i].data.fd, buf, sizeof(buf));
             }
             continue;
         }
         for (i = 0; i < poll_res; i++) {
             memset(buf, 0, sizeof(buf));
             if (pending_evs[i].data.fd == inotify_fd) {
                 struct inotify_event *event = (struct inotify_event *)buf;
                 ev_pos = 0;

                 len = read(inotify_fd, buf, sizeof(buf));
                 if (len <= (int)sizeof(*event)) {
                     ALOGE("read length[%d] less than [%d], errno = %s.\n",len , (int)sizeof(*event), strerror(errno));
                     continue;
                 }
                 while (len > sizeof(*event)) {
                     int event_size;
                     event = (struct inotify_event *)(buf + ev_pos);
                     event_size = sizeof(*event) + event->len;
                     ev_pos += event_size;
                     //exceed the buffer size, don't parse this event
                     if (ev_pos > sizeof(buf)) {
                         ALOGE("exceed the buffer size, event len %d\n", event->len);
                         break;
                     }
                     if (event->wd == dropbox_wd) {
                         parse_dropbox_event(event);
                     } else if (event->wd == tombstone_wd && strstr(event->name, "tombstone")) {
                         parse_tombstone_event(event);
                     }
                     len -= event_size;
                 }
             } else {
                 ALOGE("Reason: 0x%x\n", pending_evs[i].events);
                 read(pending_evs[i].data.fd, buf, sizeof(buf));
                 buf[511] = '\0';
                 ALOGE("get data: %s\n", buf);
             }
         }
    }

    //actually there is no need to release the resources because we want the rklog_monitor thread
    //always running. If system_server is crash(android will be restarted), then the resources of
    //rklog_monitor thread will be released automatically.
    rklog_clr_data();
    return NULL;
}

int stop_rklog(void)
{
    stop_thread = true;
    pthread_kill(rklog_thread, SIGINT);
    pthread_join(rklog_thread, NULL);

    ALOGE("%s", __FUNCTION__);
    return 0;
}

int start_rklog(void)
{
    int ret;

    ALOGE("%s", __FUNCTION__);
    epoll_fd = epoll_create(16);

    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        ALOGE("inotify_init failed, %s\n", strerror(errno));
        return -1;
    }
    dropbox_wd = inotify_add_watch(inotify_fd, DROBOX_PATH, IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF);
    if (dropbox_wd < 0) {
        ALOGE("Can't add watch for %s. and the errno = %s.\n", DROBOX_PATH, strerror(errno));
        return -1;
    }
    tombstone_wd = inotify_add_watch(inotify_fd, TOMBSTONE_PATH, IN_CREATE);
    if (tombstone_wd < 0) {
        ALOGE("Can't add watch for %s. and the errno = %s.\n", TOMBSTONE_PATH, strerror(errno));
        return -1;
    }
    add_to_epoll(inotify_fd, epoll_fd);
    stop_thread = false;

    ret = pthread_create(&rklog_thread, NULL, rklog_monitor_thread, NULL);
    if (ret != 0) {
        ALOGE("[ERROR]Cannot create monitor thread\n");
        return -1;
    }
    pthread_setname_np(rklog_thread, "rklog_monitor");

    return 0;
}

