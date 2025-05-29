# prebuilt for config xml files in /system/etc/automotive/evs/RKAVM
    PRODUCT_COPY_FILES += \
        $(call find-copy-subdir-files,*,packages/services/Car/cpp/evs/apps/default/res/RKAVM/,system/etc/automotive/evs/RKAVM/)


