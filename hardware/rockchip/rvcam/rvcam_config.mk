## Basic config settings
## One can use function like this to change the values
## $(call soong_config_set,rvcam,soc,rk3568)
## $(call soong_config_set,rvcam,board,evb_v22)
SOONG_CONFIG_NAMESPACES += rvcam
SOONG_CONFIG_rvcam += \
	soc \
	board \
	has_hal3 \
	has_evs \
	has_vhal \
	autcam_hal_version \
	evs_hal_version \
	vhal_hal_version \
	has_hwjpeg

SOONG_CONFIG_rvcam_soc ?= rk3588
SOONG_CONFIG_rvcam_board ?= evb_v22
SOONG_CONFIG_rvcam_has_hal3 ?= true
SOONG_CONFIG_rvcam_has_evs ?= true
SOONG_CONFIG_rvcam_has_vhal ?= true


ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \>= 14)))
  SOONG_CONFIG_rvcam_autcam_hal_version ?= aidl_V1
  SOONG_CONFIG_rvcam_evs_hal_version ?= aidl_V1
  SOONG_CONFIG_rvcam_vhal_hal_version ?= aidl_V1
else
  SOONG_CONFIG_rvcam_autcam_hal_version ?= hidl_2_4
  SOONG_CONFIG_rvcam_evs_hal_version ?= hidl_1_1
  SOONG_CONFIG_rvcam_vhal_hal_version ?= hidl_2_0
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 30)))
	SOONG_CONFIG_rvcam_has_hwjpeg ?= true
else
	SOONG_CONFIG_rvcam_has_hwjpeg ?= false
endif

ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \= 14)))
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
	hardware/rockchip/rvcam/hal/fcm/rvcam_compatibility_matrix_u.xml
else
ifeq (1, $(strip $(shell expr $(PLATFORM_VERSION) \= 12)))
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
	hardware/rockchip/rvcam/hal/fcm/rvcam_compatibility_matrix_s.xml
endif
endif
