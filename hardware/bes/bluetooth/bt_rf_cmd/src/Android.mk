LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := bt_nonsignal.c rf_cmd.c rf_uart.c
LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../include
LOCAL_MODULE := btrf
LOCAL_CFLAGS :=
LOCAL_CFLAGS += -O2 -g -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
LOCAL_CFLAGS += -Wno-unused-parameter

#LOCAL_32_BIT_ONLY := true

include $(BUILD_EXECUTABLE)
