LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_STATIC_JAVA_LIBRARIES := android-support-v4
LOCAL_DEX_PREOPT := false
LOCAL_PRIVATE_PLATFORM_APIS:= true
LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res
LOCAL_CERTIFICATE := platform
LOCAL_PACKAGE_NAME := Camera360

LOCAL_PREBUILT_JNI_LIBS := jni/lib64/libc++_shared.so
LOCAL_PREBUILT_JNI_LIBS := jni/lib64/libassimp.so
LOCAL_PREBUILT_JNI_LIBS += jni/lib64/lib_render_3d.so
LOCAL_PREBUILT_JNI_LIBS += jni/lib64/libutils.so
LOCAL_PREBUILT_JNI_LIBS += jni/lib64/libxml2.so
LOCAL_PREBUILT_JNI_LIBS += jni/lib64/libopencv_core.so
LOCAL_PREBUILT_JNI_LIBS += jni/lib64/libopencv_imgcodecs.so
LOCAL_PREBUILT_JNI_LIBS += jni/lib64/libopencv_imgproc.so

LOCAL_JNI_SHARED_LIBRARIES := librkaroundcamera

LOCAL_MODULE_INCLUDE_LIBRARY := true

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
