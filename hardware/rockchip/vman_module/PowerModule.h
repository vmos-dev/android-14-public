#ifndef ANDROID_INCLUDE_HARDWARE_POWER_HAL_H
#define ANDROID_INCLUDE_HARDWARE_POWER_HAL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>

__BEGIN_DECLS

#define POWER_HAL_MODULE_API_VERSION           HARDWARE_MODULE_API_VERSION(0, 1)

/*
 *  The id of this module
 */
#define POWER_HAL_HARDWARE_MODULE_ID           "vman_power_hal"

typedef enum EnumPowerMode {
	/* Standby mode: Standby after power-on */
	POWER_MODE_OFF = 0,

	/* power-on mode: System starts after power-on */
	POWER_MODE_ON = 1,

	/* last mode */
	POWER_MODE_LAST = 2,
} ui_power_mode_t;

typedef struct power_hal_module {
	struct hw_module_t common;
	ui_power_mode_t (*get_power_mode)(struct power_hal_module *module);
	int (*set_power_mode)(struct power_hal_module *module, ui_power_mode_t value);
} power_hal_module_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_HARDWARE_POWER_HAL_H */
