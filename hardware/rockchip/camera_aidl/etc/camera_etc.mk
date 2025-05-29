CUR_PATH := $(TOP)/hardware/rockchip/camera_aidl/etc

ifeq ($(CAMERA_SUPPORT_VIRTUAL),true)
PRODUCT_COPY_FILES += \
	$(CUR_PATH)/1920x1080.yuv:$(TARGET_COPY_OUT_VENDOR)/etc/camera/1920x1080.yuv \
	$(CUR_PATH)/virtual_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/virtual_config.xml
endif
ifeq ($(CAMERA_SUPPORT_HDMI),true)
PRODUCT_COPY_FILES += \
    $(CUR_PATH)/hdmi_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/hdmi_config.xml
endif
ifeq ($(CAMERA_SUPPORT_OSD),true)
PRODUCT_COPY_FILES += \
    $(CUR_PATH)/osd_logo.png:$(TARGET_COPY_OUT_VENDOR)/etc/camera/osd_logo.png
endif
