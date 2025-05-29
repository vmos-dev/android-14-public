PRODUCT_PACKAGES += \
        libpq \
        rkpq_tool_server

ifneq (,$(filter rk3588, $(strip $(TARGET_BOARD_PLATFORM))))
    PRODUCT_PACKAGES += \
        librkswpq \
        pq_init
endif

ifneq (,$(filter rk3576, $(strip $(TARGET_BOARD_PLATFORM))))
    PRODUCT_PACKAGES += \
        librkhwpq \
        libvdpp
endif

# SELinux配置
BOARD_SEPOLICY_DIRS += hardware/rockchip/libpq/pq_init/sepolicy
