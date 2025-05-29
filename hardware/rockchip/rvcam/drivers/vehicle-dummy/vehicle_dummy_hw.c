/*
 * Copyright 2018 NXP
 * Copyright 2024 Rockchip Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include "vehicle_core.h"

/* Miscellaneous external connector */
#define EXTCON_VEHICLE_RPMSG_REGISTER 58
#define EXTCON_VEHICLE_RPMSG_EVENT 59

//VEHICLE is in state parking
#define GEAR_0 1
//VEHICLE is in state reverse
#define GEAR_1 2
//VEHICLE is in state neutral
#define GEAR_3 3
//VEHICLE is in state driver
#define GEAR_2 4
// no turn signal
#define TURN_0 0
// left turn signal
#define TURN_1 1
// right turn signal
#define TURN_2 2

#define POWER_REQ_STATE_ON 0
#define POWER_REQ_STATE_SHUTDOWN_PREPARE 1
#define POWER_REQ_STATE_CANCEL_SHUTDOWN 2
#define POWER_REQ_STATE_FINISHED 3

#define POWER_REQ_PARAM_SHUTDOWN_IMMEDIATELY 1
#define POWER_REQ_PARAM_CAN_SLEEP 2
#define POWER_REQ_PARAM_SHUTDOWN_ONLY 3
#define POWER_REQ_PARAM_SLEEP_IMMEDIATELY 4
#define POWER_REQ_PARAM_HIBERNATE_IMMEDIATELY 5
#define POWER_REQ_PARAM_CAN_HIBERNATE 6

// temperature set from hardware on Android OREO and PIE uses below indexs
#define AC_TEMP_LEFT_INDEX 1
#define AC_TEMP_RIGHT_INDEX 4

// temperature set from APP on Android PIE uses below indexs
#define PIE_AC_TEMP_LEFT_INDEX 49
#define PIE_AC_TEMP_RIGHT_INDEX 68

enum vehicle_dummy_type {
	VEHICLE_TYPE_DUMMY,
	VEHICLE_TYPE_GPIO,
	VEHICLE_TYPE_ADC,
};

struct vehicle_dummy_drvdata {
	struct device *dev;
	struct delayed_work handler;

	enum vehicle_dummy_type type;

	int irq;

	// RK3588 GPIO
	struct gpio_desc *gear_reverse;
	struct gpio_desc *gear_park;

	// RK3576 SARADC
	struct iio_channel *gear_channel;
	struct iio_channel *turn_left_channel;
	struct iio_channel *turn_right_channel;

#ifdef CONFIG_EXTCON
	struct extcon_dev *rg_edev;
	struct extcon_dev *ev_edev;
#endif

	u32 gear_button;
	u32 turn_button;

	u32 gear;
	u32 turn;
	u32 temp_right;
	u32 temp_left;
	u32 fan_direction;
	u32 fan_speed;
	u32 defrost_left;
	u32 defrost_right;
	u32 ac_on;
	u32 auto_on;
	u32 hvac_on;
	u32 recirc_on;
	u32 power_req_state;
	u32 power_req_param;
	u32 seat_temp_left;
	u32 seat_temp_right;
};

#ifdef CONFIG_EXTCON
static const unsigned int rvcam_vehicle_dummy_extcon_register_cables[] = {
	EXTCON_VEHICLE_RPMSG_REGISTER,
	EXTCON_NONE,
};
static const unsigned int rvcam_vehicle_dummy_extcon_event_cables[] = {
	EXTCON_VEHICLE_RPMSG_EVENT,
	EXTCON_NONE,
};
#endif

extern void vehicle_hw_prop_ops_register(const struct hw_prop_ops *prop_ops);
extern void vehicle_hal_set_property(u16 prop, u8 index, u32 value, u32 param);
static struct vehicle_dummy_drvdata *vehicle_dummy;
static struct class *vehicle_dummy_class;

void mcu_set_control_commands(u32 prop, u32 area, u32 value)
{
	pr_debug("%s: prop %d, area %d, value %d\n", __func__, prop, area,
		value);
	switch (prop) {
	case HVAC_FAN_SPEED:
		pr_info("set fan speed with value %d\n", value);
		vehicle_dummy->fan_speed = value;
		break;
	case HVAC_FAN_DIRECTION:
		pr_info("set fan direction with value %d\n", value);
		vehicle_dummy->fan_direction = value;
		break;
	case HVAC_AUTO_ON:
		pr_info("set fan auto on with value %d\n", value);
		vehicle_dummy->auto_on = value;
		break;
	case HVAC_AC_ON:
		pr_info("set fan ac on with value %d\n", value);
		vehicle_dummy->ac_on = value;
		break;
	case HVAC_RECIRC_ON:
		pr_info("set fan recirc on with value %d\n", value);
		vehicle_dummy->recirc_on = value;
		break;
	case HVAC_DEFROSTER:
		pr_info("set defroster index %d with value %d\n", area, value);
		if (area == 1)
			vehicle_dummy->defrost_left = value;
		else
			vehicle_dummy->defrost_right = value;
		break;
	case HVAC_TEMPERATURE_SET:
		pr_info("set temp index %d with value %d\n", area, value);
#ifdef CONFIG_VEHICLE_DRIVER_OREO
		if (area == AC_TEMP_LEFT_INDEX)
			vehicle_dummy->temp_left = value;
		else if (area == AC_TEMP_RIGHT_INDEX)
			vehicle_dummy->temp_right = value;
#else
		if (area == PIE_AC_TEMP_LEFT_INDEX)
			vehicle_dummy->temp_left = value;
		else if (area == PIE_AC_TEMP_RIGHT_INDEX)
			vehicle_dummy->temp_right = value;
#endif
		break;
	case HVAC_POWER_ON:
		pr_info("set hvac power on with value %d\n", value);
		vehicle_dummy->hvac_on = value;
		break;
	case AP_POWER_STATE_REPORT:
		pr_info("receive power state report with value %d\n", value);
		break;
	case AP_POWER_STATE_REQ:
		// Proper action is TBD
		break;
	case WATCHDOG_ALIVE:
	case DISPLAY_BRIGHTNESS:
		// Proper action is TBD
		break;
	case HVAC_SEAT_TEMPERATURE:
		pr_info("set seat temperature index %d with value %d\n", area,
			value);
		if (area == SEAT_TEMP_LEFT_INDEX)
			vehicle_dummy->seat_temp_left = value;
		else if (area == SEAT_TEMP_RIGHT_INDEX)
			vehicle_dummy->seat_temp_right = value;
		else
			pr_err("unknown index: %d:%d:%d!\n", prop, area, value);
		break;
	case GEAR_SELECTION:
	case TURN_SIGNAL_STATE:
	case CURRENT_GEAR:
		// GEAR is handled in mxc vehicle driver
		break;
	case INFO_MAKE:
	case INFO_MODEL:
	case POWER_POLICY_REQ:
	case FUEL_DOOR_OPEN:
	case EV_CHARGE_PORT_OPEN:
	case EV_CHARGE_PORT_CONNECTED:
	case PARKING_BRAKE_ON:
	case FUEL_LEVEL_LOW:
	case NIGHT_MODE:
	case ABS_ACTIVE:
	case TRACTION_CONTROL_ACTIVE:
	case SUPPORT_CUSTOMIZE_VENDOR_PERMISSION:
	case INFO_MODEL_YEAR:
	case INFO_FUEL_DOOR_LOCATION:
	case INFO_EV_PORT_LOCATION:
	case ENGINE_OIL_LEVEL:
	case IGNITION_STATE:
	case HVAC_STEERING_WHEEL_HEAT:
	case HVAC_TEMPERATURE_DISPLAY_UNITS:
	case DISTANCE_DISPLAY_UNITS:
	case TIRE_PRESSURE_DISPLAY_UNITS:
	case VEHICLE_SPEED_DISPLAY_UNITS:
	case HEADLIGHTS_STATE:
	case HIGH_BEAM_LIGHTS_STATE:
	case FOG_LIGHTS_STATE:
	case PARKING_BRAKE_AUTO_APPLY:
	case HAZARD_LIGHTS_STATE:
	case HEADLIGHTS_SWITCH:
	case HIGH_BEAM_LIGHTS_SWITCH:
	case FOG_LIGHTS_SWITCH:
	case HAZARD_LIGHTS_SWITCH:
	case CLUSTER_SWITCH_UI:
	case CLUSTER_REQUEST_DISPLAY:
	case POWER_POLICY_GROUP_REQ:
	case CURRENT_POWER_POLICY:
	case ELECTRONIC_TOLL_COLLECTION_CARD_TYPE:
	case ELECTRONIC_TOLL_COLLECTION_CARD_STATUS:
	case INFO_FUEL_TYPE:
	case INFO_EV_CONNECTOR_TYPE:
	case INFO_DRIVER_SEAT:
	case INFO_EXTERIOR_DIMENSIONS:
	case INFO_MULTI_EV_PORT_LOCATIONS:
	case HW_KEY_INPUT:
	case HW_ROTARY_INPUT:
	case HW_CUSTOM_INPUT:
	case EVS_SERVICE_REQUEST:
	case CLUSTER_DISPLAY_STATE:
	case EPOCH_TIME:
	case VHAL_HEARTBEAT:
	case WHEEL_TICK:
	case INFO_FUEL_CAPACITY:
	case INFO_EV_BATTERY_CAPACITY:
	case PERF_ODOMETER:
	case PERF_VEHICLE_SPEED:
	case PERF_STEERING_ANGLE:
	case PERF_REAR_STEERING_ANGLE:
	case ENGINE_OIL_TEMP:
	case ENGINE_RPM:
	case FUEL_LEVEL:
	case RANGE_REMAINING:
	case EV_BATTERY_LEVEL:
	case EV_BATTERY_INSTANTANEOUS_CHARGE_RATE:
	case ENV_OUTSIDE_TEMPERATURE:
	case HVAC_TEMPERATURE_VALUE_SUGGESTION:
	case STORAGE_ENCRYPTION_BINDING_SEED:
	case CLUSTER_NAVIGATION_STATE:
	case INITIAL_USER_INFO:
	case SWITCH_USER:
	case CREATE_USER:
	case REMOVE_USER:
	case USER_IDENTIFICATION_ASSOCIATION:
	case WATCHDOG_TERMINATED_PROCESS:
	case CLUSTER_REPORT_STATE:
	case WINDOW_LOCK:
	case VEHICLE_MAP_SERVICE:
	case WINDOW_POS:
	case HVAC_MAX_AC_ON:
	case HVAC_MAX_DEFROST_ON:
	case HVAC_DUAL_ON:
	case HVAC_AUTO_RECIRC_ON:
	case HVAC_SEAT_VENTILATION:
	case HVAC_ELECTRIC_DEFROSTER_ON:
	case SEAT_OCCUPANCY:
	case HVAC_FAN_DIRECTION_AVAILABLE:
	case DOOR_LOCK:
	case DOOR_POS:
	case TIRE_PRESSURE:
	case CRITICALLY_LOW_TIRE_PRESSURE:
	case FRONT_FOG_LIGHTS_STATE:
	case FRONT_FOG_LIGHTS_SWITCH:
	case REAR_FOG_LIGHTS_STATE:
	case REAR_FOG_LIGHTS_SWITCH:
	case EV_CHARGE_CURRENT_DRAW_LIMIT:
	case EV_CHARGE_PERCENT_LIMIT:
	case EV_CHARGE_STATE:
	case EV_CHARGE_SWITCH:
	case EV_CHARGE_TIME_REMAINING:
	case EV_REGENERATIVE_BRAKING_STATE:
	case TRAILER_PRESENT:
	case VEHICLE_CURB_WEIGHT:
		// Proper action is TBD
		break;
	case VENDOR_EXTENSION_STRING_PROPERTY:
	case VENDOR_EXTENSION_BOOLEAN_PROPERTY:
	case VENDOR_EXTENSION_FLOAT_PROPERTY:
	case VENDOR_EXTENSION_INT_PROPERTY:
	case kMixedTypePropertyForTest:
		// Proper action is TBD
		break;
	default:
		pr_err("this type is not correct: %d:%d:%d!\n", prop, area,
		       value);
	}
}

static ssize_t turn_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->turn);
}

/* echo 0/1/2(none/left/right) > /sys/devices/platform/vehicle-dummy/turn*/
static ssize_t turn_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	u32 turn;

	if (!size)
		return -EINVAL;
	turn = simple_strtoul(buf, NULL, 10);
	if (turn != TURN_0 && turn != TURN_1 && turn != TURN_2) {
		pr_err("input value is not correct, please type correct one\n");
		return -EINVAL;
	}
	if (turn != vehicle_dummy->turn) {
		vehicle_dummy->turn = turn;
		vehicle_hal_set_property(VEHICLE_TURN_SIGNAL, 0, turn, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(turn);

static ssize_t gear_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->gear);
}

/*echo 1/2/4(parking/reverse/drive) > /sys/devices/platform/vehicle-dummy/gear*/
static ssize_t gear_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	u32 gear;

	if (!size)
		return -EINVAL;
	gear = simple_strtoul(buf, NULL, 10);
	if (gear != vehicle_dummy->gear) {
		vehicle_dummy->gear = gear;
		vehicle_hal_set_property(VEHICLE_GEAR, 0, gear, 0);
		if (gear != GEAR_0 && gear != GEAR_1 && gear != GEAR_2 &&
		    gear != GEAR_3) {
			pr_err("input value is not correct, please type correct one\n");
			return -EINVAL;
		}
#ifdef CONFIG_EXTCON
		if (gear == VEHICLE_GEAR_DRIVE)
			extcon_set_state_sync(vehicle_dummy->ev_edev,
					      EXTCON_VEHICLE_RPMSG_EVENT, 0);
		else if (gear == VEHICLE_GEAR_REVERSE)
			extcon_set_state_sync(vehicle_dummy->ev_edev,
					      EXTCON_VEHICLE_RPMSG_EVENT, 1);
#endif
	}
	return size;
}
static DEVICE_ATTR_RW(gear);

static ssize_t temp_left_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", vehicle_dummy->temp_left);
}

/*echo 1100713529 > /sys/devices/platform/vehicle-dummy/temp_left*/
static ssize_t temp_left_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	u32 temp;

	if (!size)
		return -EINVAL;
	temp = simple_strtoul(buf, NULL, 10);
	if (temp != vehicle_dummy->temp_left) {
		vehicle_dummy->temp_left = temp;
#ifdef CONFIG_VEHICLE_DRIVER_OREO
		vehicle_hal_set_property(VEHICLE_AC_TEMP, AC_TEMP_LEFT_INDEX,
					 temp, 0);
#else
		vehicle_hal_set_property(VEHICLE_AC_TEMP,
					 PIE_AC_TEMP_LEFT_INDEX, temp, 0);
#endif
	}
	return size;
}
static DEVICE_ATTR_RW(temp_left);

static ssize_t temp_right_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vehicle_dummy->temp_right);
}

/*echo 1100713529 > /sys/devices/platform/vehicle-dummy/temp_right*/
static ssize_t temp_right_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	u32 temp;

	if (!size)
		return -EINVAL;
	temp = simple_strtoul(buf, NULL, 10);
	if (temp != vehicle_dummy->temp_right) {
		vehicle_dummy->temp_right = temp;
#ifdef CONFIG_VEHICLE_DRIVER_OREO
		vehicle_hal_set_property(VEHICLE_AC_TEMP, AC_TEMP_RIGHT_INDEX,
					 temp, 0);
#else
		vehicle_hal_set_property(VEHICLE_AC_TEMP,
					 PIE_AC_TEMP_RIGHT_INDEX, temp, 0);
#endif
	}
	return size;
}

static DEVICE_ATTR_RW(temp_right);

static ssize_t seat_temp_left_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vehicle_dummy->seat_temp_left);
}

/*echo 0/1/2/3 > /sys/devices/platform/vehicle-dummy/seat_temp_left*/
static ssize_t seat_temp_left_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	u32 seat_temp;

	if (!size)
		return -EINVAL;
	seat_temp = simple_strtoul(buf, NULL, 10);

	if (seat_temp != vehicle_dummy->seat_temp_left) {
		vehicle_dummy->seat_temp_left = seat_temp;
		vehicle_hal_set_property(VEHICLE_SEAT_TEMPERATURE,
					 SEAT_TEMP_LEFT_INDEX, seat_temp, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(seat_temp_left);

static ssize_t seat_temp_right_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vehicle_dummy->seat_temp_right);
}

/*echo 0/1/2/3 > /sys/devices/platform/vehicle-dummy/seat_temp_right*/
static ssize_t seat_temp_right_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	u32 seat_temp;

	if (!size)
		return -EINVAL;
	seat_temp = simple_strtoul(buf, NULL, 10);
	if (seat_temp != SEAT_TEMP_0 && seat_temp != SEAT_TEMP_1 &&
	    seat_temp != SEAT_TEMP_2 && seat_temp != SEAT_TEMP_3) {
		pr_err("input value is not correct, please type correct one\n");
		return -EINVAL;
	}
	if (seat_temp != vehicle_dummy->seat_temp_right) {
		vehicle_dummy->seat_temp_right = seat_temp;
		vehicle_hal_set_property(VEHICLE_SEAT_TEMPERATURE,
					 SEAT_TEMP_RIGHT_INDEX, seat_temp, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(seat_temp_right);

static ssize_t fan_direction_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->fan_direction);
}

/*echo 1/2/3/6 > /sys/devices/platform/vehicle-dummy/fan_direction*/
static ssize_t fan_direction_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	u32 fan_direction;

	if (!size)
		return -EINVAL;
	fan_direction = simple_strtoul(buf, NULL, 10);

	if (fan_direction != vehicle_dummy->fan_direction) {
		vehicle_dummy->fan_direction = fan_direction;
		vehicle_hal_set_property(VEHICLE_FAN_DIRECTION, 0,
					 fan_direction, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(fan_direction);

static ssize_t fan_speed_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->fan_speed);
}

/*echo 1/2/3/4/5/6 > /sys/devices/platform/vehicle-dummy/fan_speed*/
static ssize_t fan_speed_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	u32 fan_speed;

	if (!size)
		return -EINVAL;
	fan_speed = simple_strtoul(buf, NULL, 10);

	if (fan_speed != vehicle_dummy->fan_speed) {
		vehicle_dummy->fan_speed = fan_speed;
		vehicle_hal_set_property(VEHICLE_FAN_SPEED, 0, fan_speed, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(fan_speed);

static ssize_t defrost_left_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->defrost_left);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/defrost_left*/
static ssize_t defrost_left_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	u32 defrost;

	if (!size)
		return -EINVAL;

	defrost = simple_strtoul(buf, NULL, 10);

	if (defrost != vehicle_dummy->defrost_left) {
		vehicle_dummy->defrost_left = defrost;
		vehicle_hal_set_property(VEHICLE_DEFROST, 1, defrost, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(defrost_left);

static ssize_t defrost_right_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->defrost_right);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/defrost_right*/
static ssize_t defrost_right_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	u32 defrost;

	if (!size)
		return -EINVAL;

	defrost = simple_strtoul(buf, NULL, 10);
	if (defrost != DEFROST_ON && defrost != DEFROST_OFF) {
		pr_err("input value is not correct, please type correct one\n");
		return -EINVAL;
	}
	if (defrost != vehicle_dummy->defrost_right) {
		vehicle_dummy->defrost_right = defrost;
		vehicle_hal_set_property(VEHICLE_DEFROST, 2, defrost, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(defrost_right);

static ssize_t ac_on_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->ac_on);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/ac_on*/
static ssize_t ac_on_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	u32 ac_on;

	if (!size)
		return -EINVAL;

	ac_on = simple_strtoul(buf, NULL, 10);

	if (ac_on != vehicle_dummy->ac_on) {
		vehicle_dummy->ac_on = ac_on;
		vehicle_hal_set_property(VEHICLE_AC, 0, ac_on, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(ac_on);

static ssize_t auto_on_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->auto_on);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/auto_on*/
static ssize_t auto_on_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	u32 auto_on;

	if (!size)
		return -EINVAL;

	auto_on = simple_strtoul(buf, NULL, 10);

	if (auto_on != vehicle_dummy->auto_on) {
		vehicle_dummy->auto_on = auto_on;
		vehicle_hal_set_property(VEHICLE_AUTO_ON, 0, auto_on, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(auto_on);

static ssize_t hvac_on_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->hvac_on);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/hvac_on*/
static ssize_t hvac_on_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	u32 hvac_on;

	if (!size)
		return -EINVAL;

	hvac_on = simple_strtoul(buf, NULL, 10);

	if (hvac_on != vehicle_dummy->hvac_on) {
		vehicle_dummy->hvac_on = hvac_on;
		vehicle_hal_set_property(VEHICLE_HVAC_POWER_ON, 0, hvac_on, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(hvac_on);

static ssize_t recirc_on_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%u\n", vehicle_dummy->recirc_on);
}

/* echo 0/1 > /sys/devices/platform/vehicle-dummy/recirc_on*/
static ssize_t recirc_on_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	u32 recirc_on;

	if (!size)
		return -EINVAL;

	recirc_on = simple_strtoul(buf, NULL, 10);

	if (recirc_on != vehicle_dummy->recirc_on) {
		vehicle_dummy->recirc_on = recirc_on;
		vehicle_hal_set_property(VEHICLE_RECIRC_ON, 0, recirc_on, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(recirc_on);

static ssize_t power_req_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%u %u\n", vehicle_dummy->power_req_state,
		       vehicle_dummy->power_req_param);
}

/* echo "1 1" > /sys/devices/platform/vehicle-dummy/power_req */
static ssize_t power_req_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	char *ret;
	u32 state;
	u32 param;

	if (size < 4) {
		pr_err("input command format is not correct, please type command like this: \"1 1\"\n");
		return -EINVAL;
	}

	state = simple_strtoul(buf, NULL, 10);
	ret = strrchr(buf, ' ');
	param = simple_strtoul(ret + 1, NULL, 10);

	if (state != POWER_REQ_STATE_ON &&
	    state != POWER_REQ_STATE_SHUTDOWN_PREPARE &&
	    state != POWER_REQ_STATE_CANCEL_SHUTDOWN &&
	    state != POWER_REQ_STATE_FINISHED) {
		pr_err("input power request state is not correct, please type correct one\n");
		return -EINVAL;
	}

	if (param != POWER_REQ_PARAM_SHUTDOWN_IMMEDIATELY &&
	    param != POWER_REQ_PARAM_CAN_SLEEP &&
	    param != POWER_REQ_PARAM_SHUTDOWN_ONLY &&
	    param != POWER_REQ_PARAM_SLEEP_IMMEDIATELY &&
	    param != POWER_REQ_PARAM_HIBERNATE_IMMEDIATELY &&
	    param != POWER_REQ_PARAM_CAN_HIBERNATE) {
		pr_err("input power request param is not correct, please type correct one\n");
		return -EINVAL;
	}

	vehicle_dummy->power_req_state = state;
	vehicle_dummy->power_req_param = param;
	vehicle_hal_set_property(VEHICLE_POWER_STATE_REQ, 0, state, param);
	pr_info("power control with state: %d, param: %d\n", state, param);
	return size;
}

static DEVICE_ATTR_RW(power_req);

static struct vehicle_dummy_drvdata *
vehicle_get_devtree_pdata(struct device *dev)
{
	struct vehicle_dummy_drvdata *ddata;

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return ERR_PTR(-ENOMEM);

	return ddata;
}

static const struct hw_prop_ops hw_prop_mcu_ops = {
	.set_control_commands = mcu_set_control_commands,
};

static int vehicle_dummy_init_gpio(struct device *dev,
				   struct vehicle_dummy_drvdata *ddata)
{
	int err = 0;
	ddata->gear_reverse = devm_gpiod_get_optional(dev, "reverse", GPIOD_IN);
	if (IS_ERR(ddata->gear_reverse)) {
		dev_err(dev, "failed to get gpio reverse\n");
		err = PTR_ERR(ddata->gear_reverse);
	}

	ddata->gear_park = devm_gpiod_get_optional(dev, "park", GPIOD_IN);
	if (IS_ERR(ddata->gear_park)) {
		dev_err(dev, "failed to get gpio park\n");
		err = PTR_ERR(ddata->gear_park);
	}

	return err;
}

static int vehicle_dummy_init_iio(struct device *dev,
				  struct iio_channel **channel,
				  const char *name)
{
	int err = 0;
	enum iio_chan_type type;

	struct iio_channel *tmp_channel = devm_iio_channel_get(dev, name);
	if (IS_ERR(tmp_channel))
		return PTR_ERR(tmp_channel);

	if (!tmp_channel->indio_dev)
		return -ENXIO;

	err = iio_get_channel_type(tmp_channel, &type);
	if (err < 0)
		return err;

	if (type != IIO_VOLTAGE) {
		dev_err(dev, "Incompatible channel type %d\n", type);
		return -EINVAL;
	}

	*channel = tmp_channel;

	return err;
}

static int vehicle_dummy_get_adc_gear(struct device *dev,
				      struct vehicle_dummy_drvdata *ddata)
{
	int gear = 0;
	int val;

	if (ddata->gear_channel) {
		if (iio_read_channel_processed(ddata->gear_channel, &val) < 0) {
			dev_err(dev, "Failed to read ADC value\n");
		}
		if (val < 200) {
			gear = GEAR_0;
		} else if (val > 200 && val < 600) {
			gear = GEAR_1;
		} else if (val > 600 && val < 1000) {
			gear = GEAR_3;
		} else if (val > 1000 && val < 2200) {
			gear = GEAR_2;
		}
	}

	return gear;
}

static int vehicle_dummy_get_adc_turn(struct device *dev,
				      struct vehicle_dummy_drvdata *ddata)
{
	int turn = TURN_0;
	int val;

	if (ddata->turn_left_channel) {
		if (iio_read_channel_raw(ddata->turn_left_channel, &val) < 0) {
			dev_err(dev, "Failed to read ADC value\n");
		}
		if (val > 50) {
			turn = TURN_1;
		}
	}

	if (ddata->turn_right_channel) {
		if (iio_read_channel_raw(ddata->turn_right_channel, &val) < 0) {
			dev_err(dev, "Failed to read ADC value\n");
		}
		if (val > 50 && turn == TURN_1) {
			turn = TURN_0;
		} else if (val > 50) {
			turn = TURN_2;
		}
	}

	return turn;
}

static int vehicle_dummy_get_gpio_gear(struct device *dev,
				       struct vehicle_dummy_drvdata *ddata)
{
	int gear = 0;
	int park_value = 0;
	int reverse_value = 0;

	if (ddata->gear_park) {
		park_value = !!gpiod_get_value(ddata->gear_park);
	}

	if (ddata->gear_reverse) {
		reverse_value = !!gpiod_get_value(ddata->gear_reverse);
	}
	dev_dbg(dev, "vehicle dummy gpio %d %d\n", park_value, reverse_value);
	if (park_value && reverse_value) {
		gear = GEAR_2;
	} else if (!park_value && reverse_value) {
		gear = GEAR_1;
	} else if (park_value && !reverse_value) {
		gear = GEAR_0;
	} else {
		gear = GEAR_3;
	}

	return gear;
}

static void vehicle_dummy_work_func(struct work_struct *work)
{
	struct vehicle_dummy_drvdata *ddata = container_of(
		to_delayed_work(work), struct vehicle_dummy_drvdata, handler);
	struct device *dev = ddata->dev;

	if (ddata->type == VEHICLE_TYPE_GPIO) {
		ddata->gear_button =
			vehicle_dummy_get_gpio_gear(ddata->dev, ddata);
	} else if (ddata->type == VEHICLE_TYPE_ADC) {
		ddata->gear_button =
			vehicle_dummy_get_adc_gear(ddata->dev, ddata);
		ddata->turn_button =
			vehicle_dummy_get_adc_turn(ddata->dev, ddata);
	}
	if (ddata->gear == 0 && ddata->gear_button > 0) {
		vehicle_hal_set_property(VEHICLE_GEAR, 0, ddata->gear_button,
					 0);
		if (ddata->gear_button != GEAR_0 &&
		    ddata->gear_button != GEAR_1 &&
		    ddata->gear_button != GEAR_3 &&
		    ddata->gear_button != GEAR_2) {
			dev_err(dev, "GPIO gear value is not correct\n");
		}
#ifdef CONFIG_EXTCON
		if (ddata->gear_button == VEHICLE_GEAR_DRIVE)
			extcon_set_state_sync(ddata->ev_edev,
					      EXTCON_VEHICLE_RPMSG_EVENT, 0);
		else if (ddata->gear_button == VEHICLE_GEAR_REVERSE)
			extcon_set_state_sync(ddata->ev_edev,
					      EXTCON_VEHICLE_RPMSG_EVENT, 1);
#endif
	}

	if (ddata->turn == 0) {
		vehicle_hal_set_property(VEHICLE_TURN_SIGNAL, 0,
					 ddata->turn_button, 0);
	}
	dev_dbg(dev, "gear %u turn %u\n", ddata->gear_button,
		 ddata->turn_button);
	schedule_delayed_work(&ddata->handler, msecs_to_jiffies(1000));
}

static irqreturn_t vehicle_dummy_irq_thread(int irq, void *_data)
{
	struct vehicle_dummy_drvdata *data = _data;

	queue_delayed_work(system_power_efficient_wq, &data->handler,
			   msecs_to_jiffies(5));

	return IRQ_HANDLED;
}

static const struct of_device_id rockchip_vehicle_id[] = {
	{
		.compatible = "rockchip,vehicle-dummy",
		.data = (void *)VEHICLE_TYPE_DUMMY,
	},
	{
		.compatible = "rockchip,vehicle-dummy-gpio",
		.data = (void *)VEHICLE_TYPE_GPIO,
	},
	{
		.compatible = "rockchip,vehicle-dummy-adc",
		.data = (void *)VEHICLE_TYPE_ADC,
	},
	{},
};

static int vehicle_dummy_hw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vehicle_dummy_drvdata *ddata;
	enum vehicle_dummy_type type = VEHICLE_TYPE_DUMMY;
	const struct of_device_id *match;
	int err;

	dev_info(dev, "probe\n");

	match = of_match_device(rockchip_vehicle_id, &pdev->dev);
	if (!match) {
		dev_err(dev, "No configuration found\n");
	} else {
		dev->platform_data = (void *)match->data;
		type = (enum vehicle_dummy_type)dev->platform_data;
	}

	ddata = vehicle_get_devtree_pdata(dev);
	if (IS_ERR(ddata))
		return PTR_ERR(ddata);

	ddata->dev = dev;
	ddata->type = type;

	if (type == VEHICLE_TYPE_ADC) {
		dev_info(dev, "type is ADC\n");
		err = vehicle_dummy_init_iio(dev, &ddata->gear_channel, "gear");
		if (err) {
			dev_err(dev, "failed to get gear adc channel.\n");
			type = VEHICLE_TYPE_DUMMY;
		} else {
			err = vehicle_dummy_init_iio(dev,
						     &ddata->turn_left_channel,
						     "turn_left") ||
			      vehicle_dummy_init_iio(dev,
						     &ddata->turn_right_channel,
						     "turn_right");
			if (err) {
				dev_err(dev,
					"failed to get turn adc channel.\n");
				type = VEHICLE_TYPE_DUMMY;
			}
		}
	} else if (type == VEHICLE_TYPE_GPIO) {
		dev_info(dev, "type is GPIO\n");
		err = vehicle_dummy_init_gpio(dev, ddata);
		if (err) {
			dev_err(dev, "failed to get gear gpio.\n");
			type = VEHICLE_TYPE_DUMMY;
		}
	}

	if (type != VEHICLE_TYPE_DUMMY) {
		INIT_DEFERRABLE_WORK(&ddata->handler, vehicle_dummy_work_func);
	}

	vehicle_dummy = ddata;
	platform_set_drvdata(pdev, ddata);

	vehicle_dummy_class = class_create(THIS_MODULE, "vehicle_dummy_hw");
	if (IS_ERR(vehicle_dummy_class)) {
		dev_err(dev, "failed to create class.\n");
		return PTR_ERR(vehicle_dummy_class);
	}

	err = device_create_file(dev, &dev_attr_recirc_on) ||
	      device_create_file(dev, &dev_attr_hvac_on) ||
	      device_create_file(dev, &dev_attr_auto_on) ||
	      device_create_file(dev, &dev_attr_ac_on) ||
	      device_create_file(dev, &dev_attr_defrost_right) ||
	      device_create_file(dev, &dev_attr_defrost_left) ||
	      device_create_file(dev, &dev_attr_fan_speed) ||
	      device_create_file(dev, &dev_attr_fan_direction) ||
	      device_create_file(dev, &dev_attr_temp_left) ||
	      device_create_file(dev, &dev_attr_temp_right) ||
	      device_create_file(dev, &dev_attr_gear) ||
	      device_create_file(dev, &dev_attr_power_req) ||
	      device_create_file(dev, &dev_attr_seat_temp_left) ||
	      device_create_file(dev, &dev_attr_seat_temp_right) ||
	      device_create_file(dev, &dev_attr_turn);
	if (err) {
		dev_err(dev, "failed to create sysfs files\n");
		return err;
	}

	vehicle_hw_prop_ops_register(&hw_prop_mcu_ops);

#ifdef CONFIG_EXTCON
	ddata->rg_edev = devm_extcon_dev_allocate(
		dev, rvcam_vehicle_dummy_extcon_register_cables);
	if (IS_ERR(ddata->rg_edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return PTR_ERR(ddata->rg_edev);
	}
	err = devm_extcon_dev_register(dev, ddata->rg_edev);
	if (err < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return err;
	}
	ddata->ev_edev = devm_extcon_dev_allocate(
		dev, rvcam_vehicle_dummy_extcon_event_cables);
	if (IS_ERR(ddata->ev_edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return PTR_ERR(ddata->ev_edev);
	}
	err = devm_extcon_dev_register(dev, ddata->ev_edev);
	if (err < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return err;
	}
#endif

	if (type == VEHICLE_TYPE_ADC) {
#if 0
		ddata->irq = platform_get_irq(pdev, 0);
		if (ddata->irq < 0) {
			dev_err(dev, "failed to get irq\n");
			return -ENODEV;
		} else {
			err = request_any_context_irq(ddata->irq,
						      vehicle_dummy_irq_thread,
						      IRQF_TRIGGER_HIGH,
						      dev_name(dev), ddata);

			if (err < 0) {
				dev_err(dev, "error: irq %d\n", ddata->irq);
				return err;
			}
		}
#endif
	} else if (type == VEHICLE_TYPE_GPIO) {
		if (ddata->gear_reverse) {
			int irq = gpiod_to_irq(ddata->gear_reverse);
			int flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
			err = request_any_context_irq(irq,
						      vehicle_dummy_irq_thread,
						      flags, dev_name(dev),
						      ddata);

			if (err < 0) {
				dev_err(dev, "error: irq for reverse gpio\n");
				return err;
			}
		}
		if (ddata->gear_park) {
			int irq = gpiod_to_irq(ddata->gear_park);
			int flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
			err = request_any_context_irq(irq,
						      vehicle_dummy_irq_thread,
						      flags, dev_name(dev),
						      ddata);

			if (err < 0) {
				dev_err(dev, "error: irq for park gpio\n");
				return err;
			}
		}
	}

	if (type != VEHICLE_TYPE_DUMMY) {
		schedule_delayed_work(&ddata->handler, msecs_to_jiffies(200));
	}

	return 0;
}

static int vehicle_dummy_hw_remove(struct platform_device *pdev)
{
	struct vehicle_dummy_drvdata *ddata = platform_get_drvdata(pdev);

	if (ddata->type != VEHICLE_TYPE_DUMMY) {
		free_irq(ddata->irq, ddata);
		cancel_work_sync(&ddata->handler.work);
	}
	class_destroy(vehicle_dummy_class);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vehicle_dummy_suspend(struct device *dev)
{
	struct vehicle_dummy_drvdata *ddata = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&ddata->handler);
	if (device_may_wakeup(ddata->dev))
		enable_irq_wake(ddata->irq);

	return 0;
}

static int vehicle_dummy_resume(struct device *dev)
{
	struct vehicle_dummy_drvdata *ddata = dev_get_drvdata(dev);

	if (device_may_wakeup(ddata->dev))
		disable_irq_wake(ddata->irq);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(vehicle_dummy_pm_ops, vehicle_dummy_suspend,
			 vehicle_dummy_resume);

static struct platform_driver vehicle_dummy_hw_driver = {
    .probe = vehicle_dummy_hw_probe,
    .remove = vehicle_dummy_hw_remove,
    .driver = {
        .name = "vehicle-dummy",
        .pm = &vehicle_dummy_pm_ops,
        .of_match_table = rockchip_vehicle_id,
    },
};

static struct platform_device *dummy_pdev;

static int vehicle_dummy_init(void)
{
	int err;

	err = platform_driver_register(&vehicle_dummy_hw_driver);
	if (err) {
		pr_err("Failed to register dummy vehicle driver\n");
		platform_device_unregister(dummy_pdev);
		return err;
	}
	return 0;
}

static void __exit vehicle_dummy_exit(void)
{
	platform_driver_unregister(&vehicle_dummy_hw_driver);
}

late_initcall(vehicle_dummy_init);
module_exit(vehicle_dummy_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductor");
MODULE_DESCRIPTION("VEHICLE DUMMY HW");
