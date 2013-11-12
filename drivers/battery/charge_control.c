/*
 * charge_control.c - Battery charge control for MAX77803 on UNIVERSAL5410
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: August 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/battery/sec_battery.h>

static struct sec_battery_info *info;

enum charge_control_type {
	INPUT = 0, CHARGE, OTHER
};

struct charge_control {
	const struct device_attribute	attribute;
	enum charge_control_type	type;
	unsigned int			index;
};

static ssize_t show_charge_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t store_charge_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

#define _charge(name_, type_, index_)	\
{ 							\
	.attribute = {					\
			.attr = {			\
				  .name = name_,	\
				  .mode = 0664,		\
				},			\
			.show = show_charge_property,	\
			.store = store_charge_property,	\
		     },					\
	.type 	= type_ ,				\
	.index 	= index_ ,				\
}

struct charge_control charge_controls[] = {
	_charge("ac_input_curr"	, INPUT	, POWER_SUPPLY_TYPE_MAINS),
	_charge("ac_chrg_curr"	, CHARGE, POWER_SUPPLY_TYPE_MAINS),

	_charge("sdp_input_curr", INPUT	, POWER_SUPPLY_TYPE_USB),
	_charge("sdp_chrg_curr"	, CHARGE, POWER_SUPPLY_TYPE_USB),

	_charge("dcp_input_curr", INPUT	, POWER_SUPPLY_TYPE_USB_DCP),
	_charge("dcp_chrg_curr"	, CHARGE, POWER_SUPPLY_TYPE_USB_DCP),

	_charge("cdp_input_curr", INPUT	, POWER_SUPPLY_TYPE_USB_CDP),
	_charge("cdp_chrg_curr"	, CHARGE, POWER_SUPPLY_TYPE_USB_CDP),

	_charge("aca_input_curr", INPUT	, POWER_SUPPLY_TYPE_USB_ACA),
	_charge("aca_chrg_curr"	, CHARGE, POWER_SUPPLY_TYPE_USB_ACA),

	_charge("misc_input_curr", INPUT, POWER_SUPPLY_TYPE_MISC),
	_charge("misc_chrg_curr", CHARGE, POWER_SUPPLY_TYPE_MISC),

	_charge("wpc_input_curr", INPUT	, POWER_SUPPLY_TYPE_WPC),
	_charge("wpc_chrg_curr"	, CHARGE, POWER_SUPPLY_TYPE_WPC),
};

#define curr(index) info->pdata->charging_current[index]

static ssize_t show_charge_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct charge_control *control = (struct charge_control*)(attr);
	int val = 0;

	switch (control->type) {
		case INPUT:	
			val = curr(control->index).input_current_limit;
			break;
		case CHARGE:	
			val = curr(control->index).fast_charging_current;
			break;
		default:
			break;
	}

	return sprintf(buf, "%d", val);
}

static ssize_t store_charge_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct charge_control *control = (struct charge_control*)(attr);
	unsigned int val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	switch (control->type) {
		case INPUT:
			curr(control->index).input_current_limit = val;
			break;
		case CHARGE:
			curr(control->index).fast_charging_current = val;
			break;
		default:
			break;
	}

	return count;
}

extern int SIOP_INPUT_LIMIT_CURRENT;
extern int SIOP_CHARGING_LIMIT_CURRENT;
extern int mhl_class_usb;
extern int mhl_class_500;
extern int mhl_class_900;
extern int mhl_class_1500;

extern bool unstable_power_detection;

#define CHARGE_INT_ATTR(_name, _mode, _var) \
	{ __ATTR(_name, _mode, device_show_int, device_store_int), &(_var) }

struct dev_ext_attribute static_controls[] = {
	CHARGE_INT_ATTR(siop_input_limit, 0644, SIOP_INPUT_LIMIT_CURRENT),
	CHARGE_INT_ATTR(siop_charge_limit, 0644, SIOP_CHARGING_LIMIT_CURRENT),
	CHARGE_INT_ATTR(mhl_usb_curr, 0644, mhl_class_usb),
	CHARGE_INT_ATTR(mhl_500_curr, 0644, mhl_class_500),
	CHARGE_INT_ATTR(mhl_900_curr, 0644, mhl_class_900),
	CHARGE_INT_ATTR(mhl_1500_curr, 0644, mhl_class_1500),
	CHARGE_INT_ATTR(unstable_power_detection, 0644, unstable_power_detection)
};

void charger_control_init(struct sec_battery_info *sec_info)
{
	int i;

	info = sec_info;

	for (i = 0; i < ARRAY_SIZE(charge_controls); i++)
		if (device_create_file(info->dev, &charge_controls[i].attribute))
			;;

	for (i = 0; i < ARRAY_SIZE(static_controls); i++)
		if (device_create_file(info->dev, &static_controls[i].attr))
			;;
}
