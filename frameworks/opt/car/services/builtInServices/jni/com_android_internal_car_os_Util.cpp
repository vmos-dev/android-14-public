/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define LOG_TAG "OsUtil"

#include "core_jni_helpers.h"

#include <processgroup/processgroup.h>
#include <utils/Log.h>

#include <nativehelper/JNIHelp.h>

using namespace android;

void com_android_internal_car_os_Util_setProcessProfile(JNIEnv* env, jobject /*clazz*/, jint pid,
    jint uid, jstring profile) {
    if (profile == nullptr) {
        jniThrowNullPointerException(env, NULL);
        return;
    }

    const char* profileStr = env->GetStringUTFChars(profile, nullptr);
    if (profileStr == nullptr) {
        jniThrowNullPointerException(env, NULL);
        return;
    }
    bool success = SetProcessProfiles(uid, pid, {profileStr});
    env->ReleaseStringUTFChars(profile, profileStr);

    if (!success) {
        jniThrowExceptionFmt(env, "java/lang/IllegalArgumentException",
            "setProcessProfile for pid %d, uid %d, profile %s failed", pid, uid, profileStr);
    }
}

static const JNINativeMethod methods[] = {
        {"setProcessProfile", "(IILjava/lang/String;)V",
         (void*)com_android_internal_car_os_Util_setProcessProfile},
};

int register_com_android_internal_car_os_Util(JNIEnv* env)
{
    return RegisterMethodsOrDie(env, "com/android/internal/car/os/Util", methods, NELEM(methods));
}
