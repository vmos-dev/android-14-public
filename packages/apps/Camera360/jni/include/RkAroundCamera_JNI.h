#ifndef RkAroundCamera_JNI_H
#define RkAroundCamera_JNI_H

#include <jni.h>
#include "android/log.h"

static const char *TAG = "NDK_RkAroundCamera";

#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, "%s:line %d | " fmt, __func__, __LINE__, ##args)
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO,  TAG, "%s:line %d | " fmt, __func__, __LINE__, ##args)
#define LOGW(fmt, args...) __android_log_print(ANDROID_LOG_WARN,  TAG, "%s:line %d | " fmt, __func__, __LINE__, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, "%s:line %d | " fmt, __func__, __LINE__, ##args)


#endif
