# Rockchip Vehicle Camera Packages
ifeq ($(strip $(BOARD_CAMERA_SUPPORT_AUTOMOTIVE)), true)

# RVCAM middleware packages
ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \>= 14)))
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rvcam.u.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rvcam.rc
else
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rvcam.s.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rvcam.rc
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/ueventd.rvcam.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/ueventd.rvcam.rc \
    $(LOCAL_PATH)/components/config/xml/rvcam_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/rvcam_config.xml

PRODUCT_PACKAGES += \
    vendor.rockchip.automotive.camera.rvcam@1.0 \
    vendor.rockchip.automotive.camera.rvcam@1.0-service \
    librvcam_client@1.0

PRODUCT_PRODUCT_PROPERTIES += persist.vendor.rockchip.rvcam.loglevel=1

include hardware/rockchip/rvcam/hal/hidl/rvcam/server/sepolicy.mk

# EVS packages
ifeq ($(strip $(SOONG_CONFIG_rvcam_has_evs)), true)

ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \>= 14)))
LOCAL_EVS_PROPERTIES ?= persist.automotive.evs.mode=1
PRODUCT_PRODUCT_PROPERTIES += $(LOCAL_EVS_PROPERTIES)
PRODUCT_PACKAGES += \
    evs_app \
    evsmanagerd \
    android.hardware.automotive.evs-rvcam \
    cardisplayproxyd

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/hal/evs/app/config.json:$(TARGET_COPY_OUT_VENDOR)/etc/automotive/evs/config_override.json \
    $(LOCAL_PATH)/hal/evs/app/evs_configuration_override.xml:$(TARGET_COPY_OUT_VENDOR)/etc/automotive/evs/evs_configuration_override.xml
include hardware/rockchip/rvcam/hal/evs/aidl/sepolicy/evsdriver.mk

else
LOCAL_EVS_PROPERTIES ?= persist.automotive.evs.mode=1
PRODUCT_PRODUCT_PROPERTIES += $(LOCAL_EVS_PROPERTIES)
PRODUCT_PACKAGES += \
    evs_app \
    android.hardware.automotive.evs@1.1-rvcam \
    android.frameworks.automotive.display@1.0-service

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/hal/evs/app/evs_app_config.json:$(TARGET_COPY_OUT_SYSTEM)/etc/automotive/evs/config_override.json \
    $(LOCAL_PATH)/hal/evs/app/evs_configuration_override.xml:$(TARGET_COPY_OUT_VENDOR)/etc/automotive/evs/evs_configuration/evs_configuration_override.xml

include hardware/rockchip/rvcam/hal/evs/1.1/sepolicy/evsdriver.mk
endif

endif

include packages/services/Car/cpp/evs/apps/sepolicy/evsapp.mk

# Camera HAL3 packages
ifeq ($(strip $(SOONG_CONFIG_rvcam_has_hal3)), true)

ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \>= 14)))
PRODUCT_PACKAGES += \
    android.hardware.camera.provider-V1-automotive-impl \
    android.hardware.camera.provider-V1-automotive-service
else
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-impl-automotive \
    android.hardware.camera.provider@2.4-automotive-service
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/hal/provider/config/automotive_camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/automotive_camera_config.xml

endif

# VHAL pacakges
ifeq ($(strip $(SOONG_CONFIG_rvcam_has_vhal)), true)
ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \>= 14)))
PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@V1-rvcam-service

include hardware/rockchip/rvcam/hal/vehicle/aidl/sepolicy/vhal.mk

BOARD_VENDOR_KERNEL_MODULES += hardware/rockchip/rvcam/drivers/prebuilts/6.1/vehicle-core.ko
BOARD_VENDOR_KERNEL_MODULES += hardware/rockchip/rvcam/drivers/prebuilts/6.1/vehicle-dummy-hw.ko

else
PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@2.0-rvcam-service

include hardware/rockchip/rvcam/hal/vehicle/2.0/sepolicy/vhal.mk

BOARD_VENDOR_KERNEL_MODULES += hardware/rockchip/rvcam/drivers/prebuilts/5.10/vehicle-core.ko
BOARD_VENDOR_KERNEL_MODULES += hardware/rockchip/rvcam/drivers/prebuilts/5.10/vehicle-dummy-hw.ko

endif
endif

endif
