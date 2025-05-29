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
#define LOG_TAG "RKBOX"

#define LOG_NDEBUG 0

#include <nativehelper/JNIHelp.h>
#include "jni.h"
#include "android_runtime/AndroidRuntime.h"
#include "rkbox/rklog.h"


namespace android {

class JRKBox {
public:
    JRKBox();
    ~JRKBox();
private:
};

JRKBox* mRKBox = nullptr;

JRKBox::JRKBox() {
    ALOGD("%s", __FUNCTION__);
    start_rklog();
}

JRKBox::~JRKBox() {
    ALOGD("%s", __FUNCTION__);
}


static int nativeInit(JNIEnv* env, jobject obj) {
    int ret = 0;
    ALOGD("%s", __FUNCTION__);

    mRKBox = new JRKBox();
    return static_cast<jint>(ret);
}

// ----------------------------------------------------------------------------

static const JNINativeMethod sRKBoxManagementServiceMethods[] = {
    /* name, signature, funcPtr */
    { "nativeInit", "()I",
            (void*) nativeInit},
};

#define FIND_CLASS(var, className) \
        var = env->FindClass(className); \
        LOG_FATAL_IF(! (var), "Unable to find class " className)

#define GET_METHOD_ID(var, clazz, methodName, fieldDescriptor) \
        var = env->GetMethodID(clazz, methodName, fieldDescriptor); \
        LOG_FATAL_IF(! (var), "Unable to find method" methodName)

int register_com_android_server_RKBoxManagementService(JNIEnv* env) {
    int res = jniRegisterNativeMethods(env, "com/android/server/RKBoxManagementService",
            sRKBoxManagementServiceMethods, NELEM(sRKBoxManagementServiceMethods));
    LOG_FATAL_IF(res < 0, "Unable to register native methods.");
    (void)res; // Don't complain about unused variable in the LOG_NDEBUG case

    jclass clazz;
    FIND_CLASS(clazz, "com/android/server/RKBoxManagementService");
    return 0;
}

} /* namespace android */
