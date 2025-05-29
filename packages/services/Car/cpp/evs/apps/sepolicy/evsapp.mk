# Selinux policies for the sample EVS application
PRODUCT_PUBLIC_SEPOLICY_DIRS += packages/services/Car/cpp/evs/apps/sepolicy/public
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/cpp/evs/apps/sepolicy/private

ifeq ($(ENABLE_CARTELEMETRY_SERVICE), true)
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/cpp/evs/apps/sepolicy/cartelemetry
endif
