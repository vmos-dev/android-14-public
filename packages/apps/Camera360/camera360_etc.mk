# prebuilt for config xml files in /vendor/etc/camera or /system/etc/camera
ifneq ($(filter rk3588 rk3576, $(strip $(TARGET_BOARD_PLATFORM))), )
    PRODUCT_COPY_FILES += \
        $(call find-copy-subdir-files,*,$(TOP)/packages/apps/Camera360/AppData_rk/,$(TARGET_COPY_OUT_VENDOR)/etc/camera/AppData_rk/)
endif


