#legacy audio hal is just for debuging, and we use tinyalsa in all rk product
#use AUDIO_FORCE_LEGACY to choose which you need.

MY_LOCAL_PATH := $(call my-dir)

AUDIO_FORCE_LEGACY=false

ifeq ($(strip $(AUDIO_FORCE_LEGACY)), true)
    include $(MY_LOCAL_PATH)/legacy_hal/Android.mk
else ifeq ($(strip $(BOARD_ROCKCHIP_VEHICLE)), true)
    include $(MY_LOCAL_PATH)/primary_hal/Android.mk
else
    include $(MY_LOCAL_PATH)/tinyalsa_hal/Android.mk
    include $(MY_LOCAL_PATH)/eqdrc/Android.mk
    include $(MY_LOCAL_PATH)/preprocess/Android.mk
endif

