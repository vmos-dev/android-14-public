# 把aidl service和测试的client bin编译到固件中
PRODUCT_PACKAGES += \
    rockchip.hwc.proxy-service \
    librkhwcproxy

# 在system中注册这个服务
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += hardware/rockchip/hwc_proxy_service/default/frameworks_rockchip.hwc.proxy-service.xml

# SELinux配置
BOARD_SEPOLICY_DIRS += hardware/rockchip/hwc_proxy_service/default/sepolicy
