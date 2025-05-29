LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

ROAM_OFFLOAD := false

LOCAL_SRC_FILES := besphy_rf.c cmd.c event.c
LOCAL_MODULE := besphy
LOCAL_CFLAGS := -I$(gettop)/external/libnl/include
LOCAL_CFLAGS += -O2 -g -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
LOCAL_CFLAGS += -Wno-unused-parameter


LOCAL_CFLAGS += -DCONFIG_LIBNL30

ifeq ($(ROAM_OFFLOAD),true)
LOCAL_CFLAGS += -DROAM_OFFLOAD
endif

#LOCAL_MODULE_PATH  := $(TARGET_OUT_VENDOR)/bin

LOCAL_SHARED_LIBRARIES := libnl

include $(BUILD_EXECUTABLE)