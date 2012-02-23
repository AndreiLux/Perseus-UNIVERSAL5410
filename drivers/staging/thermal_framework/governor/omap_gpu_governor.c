/*
 * drivers/thermal/omap_gpu_governor.c
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/err.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/thermal_framework.h>

/* GPU Zone information */
#define FATAL_ZONE	5
#define PANIC_ZONE	4
#define ALERT_ZONE	3
#define MONITOR_ZONE	2
#define SAFE_ZONE	1
#define NO_ACTION	0
#define OMAP_FATAL_TEMP 125000
#define OMAP_PANIC_TEMP 110000
#define OMAP_ALERT_TEMP 100000
#define OMAP_MONITOR_TEMP 85000


/* TODO: Define this via a configurable file */
#define HYSTERESIS_VALUE 5000
#define NORMAL_TEMP_MONITORING_RATE 1000
#define FAST_TEMP_MONITORING_RATE 250

static int omap_gpu_slope;
static int omap_gpu_const;

struct omap_gpu_governor {
	struct thermal_dev *temp_sensor;
	void (*update_temp_thresh) (struct thermal_dev *, int min, int max);
	int report_rate;
	int panic_zone_reached;
	int cooling_level;
};

static struct thermal_dev *therm_fw;
static struct omap_gpu_governor *omap_gov;

static LIST_HEAD(cooling_agents);

/**
 * DOC: Introduction
 * =================
 * The OMAP GPU Temperature governor maintains the policy for the GPU
 * on gpu temperature sensor.  The main goal of the governor is to receive
 * a temperature report from a thermal sensor and calculate the current
 * thermal zone.  Each zone will sort through a list of cooling agents
 * passed in to determine the correct cooling stategy that will cool the
 * device appropriately for that zone.
 *
 * The temperature that is reported by the temperature sensor is first
 * calculated to an OMAP hot spot temperature.
 * This takes care of the existing temperature gpu between
 * the OMAP hot spot and the on-gpu temp sensor.
 * Note: The "slope" parameter is multiplied by 1000 in the configuration
 * file to avoid using floating values.
 * Note: The "offset" is defined in milli-celsius degrees.
 *
 * Next the hot spot temperature is then compared to thresholds to determine
 * the proper zone.
 *
 * There are 5 zones identified:
 *
 * FATAL_ZONE: This zone indicates that the on-gpu temperature has reached
 * a point where the device needs to be rebooted and allow ROM or the bootloader
 * to run to allow the device to cool.
 *
 * PANIC_ZONE: This zone indicates a near fatal temperature is approaching
 * and should impart all neccessary cooling agent to bring the temperature
 * down to an acceptable level.
 *
 * ALERT_ZONE: This zone indicates that gpu is at a level that may need more
 * agressive cooling agents to keep or lower the temperature.
 *
 * MONITOR_ZONE: This zone is used as a monitoring zone and may or may not use
 * cooling agents to hold the current temperature.
 *
 * SAFE_ZONE: This zone is optimal thermal zone.  It allows the device to
 * run at max levels without imparting any cooling agent strategies.
 *
 * NO_ACTION: Means just that.  There was no action taken based on the current
 * temperature sent in.
*/

/**
 * omap_update_report_rate() - Updates the temperature sensor monitoring rate
 *
 * @new_rate: New measurement rate for the temp sensor
 *
 * No return
 */
static void omap_update_report_rate(struct thermal_dev *temp_sensor,
				int new_rate)
{
	if (omap_gov->report_rate == -EOPNOTSUPP) {
		pr_err("%s:Updating the report rate is not supported\n",
			__func__);
		return;
	}

	if (omap_gov->report_rate != new_rate)
		omap_gov->report_rate =
			thermal_update_temp_rate(temp_sensor, new_rate);
}

/*
 * convert_omap_sensor_temp_to_hotspot_temp() -Convert the temperature from the
 *		OMAP on-gpu temp sensor into OMAP hot spot temperature.
 *		This takes care of the existing temperature gpu between
 *		the OMAP hot spot and the on-gpu temp sensor.
 *
 * @sensor_temp: Raw temperature reported by the OMAP gpu temp sensor
 *
 * Returns the calculated hot spot temperature for the zone calculation
 */
static signed int convert_omap_sensor_temp_to_hotspot_temp(int sensor_temp)
{
	int absolute_delta;

	absolute_delta = ((sensor_temp * omap_gpu_slope / 1000) +
			omap_gpu_const);
	return sensor_temp + absolute_delta;
}

/*
 * hotspot_temp_to_omap_sensor_temp() - Convert the temperature from
 *		the OMAP hot spot temperature into the OMAP on-gpu temp sensor.
 *		This is useful to configure the thresholds at OMAP on-gpu
 *		sensor level. This takes care of the existing temperature
 *		gpu between the OMAP hot spot and the on-gpu temp sensor.
 *
 * @hot_spot_temp: Hot spot temperature to the be calculated to GPU on-gpu
 *		temperature value.
 *
 * Returns the calculated gpu on-gpu temperature.
 */

static signed hotspot_temp_to_sensor_temp(int hot_spot_temp)
{
	return ((hot_spot_temp - omap_gpu_const) * 1000) /
			(1000 + omap_gpu_slope);
}

/**
 * omap_safe_zone() - THERMAL "Safe Zone" definition:
 *  - No constraint about Max GPU frequency
 *  - No constraint about GPU freq governor
 *  - Normal temperature monitoring rate
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @gpu_temp:	The current adjusted GPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_safe_zone(struct list_head *cooling_list, int gpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int gpu_temp_upper = 0;
	int gpu_temp_lower = 0;
	pr_debug("%s:hot spot temp %d\n", __func__, gpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		omap_gov->cooling_level = 0;
		thermal_cooling_set_level(&cooling_agents,
					omap_gov->cooling_level);
		list_del_init(&cooling_agents);
		gpu_temp_lower = hotspot_temp_to_sensor_temp(
			OMAP_MONITOR_TEMP - HYSTERESIS_VALUE);
		gpu_temp_upper = hotspot_temp_to_sensor_temp(OMAP_MONITOR_TEMP);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			gpu_temp_lower, gpu_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			NORMAL_TEMP_MONITORING_RATE);
		omap_gov->panic_zone_reached = 0;
	}

	return 0;
}

/**
 * omap_monitor_zone() - Current device is in a situation that requires
 *			monitoring and may impose agents to keep the device
 *			at a steady state temperature or moderately cool the
 *			device.
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @gpu_temp:	The current adjusted GPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_monitor_zone(struct list_head *cooling_list, int gpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int gpu_temp_upper = 0;
	int gpu_temp_lower = 0;

	pr_debug("%s:hot spot temp %d\n", __func__, gpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		omap_gov->cooling_level = 0;
		thermal_cooling_set_level(&cooling_agents,
					omap_gov->cooling_level);
		list_del_init(&cooling_agents);
		gpu_temp_lower = hotspot_temp_to_sensor_temp(
			OMAP_MONITOR_TEMP - HYSTERESIS_VALUE);
		gpu_temp_upper =
			hotspot_temp_to_sensor_temp(OMAP_ALERT_TEMP);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			gpu_temp_lower, gpu_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			FAST_TEMP_MONITORING_RATE);
		omap_gov->panic_zone_reached = 0;
	}

	return 0;
}
/**
 * omap_alert_zone() - "Alert Zone" definition:
 *	- If the Panic Zone has never been reached, then
 *	- Define constraint about Max GPU frequency
 *		if Current frequency < Max frequency,
 *		then Max frequency = Current frequency
 *	- Else keep the constraints set previously until
 *		temperature falls to safe zone
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @gpu_temp:	The current adjusted GPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_alert_zone(struct list_head *cooling_list, int gpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int gpu_temp_upper = 0;
	int gpu_temp_lower = 0;

	pr_debug("%s:hot spot temp %d\n", __func__, gpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		if (omap_gov->panic_zone_reached == 0) {
			/* Temperature rises and enters into alert zone */
			omap_gov->cooling_level = 0;
			thermal_cooling_set_level(&cooling_agents,
						omap_gov->cooling_level);
		}

		list_del_init(&cooling_agents);
		gpu_temp_lower = hotspot_temp_to_sensor_temp(
			OMAP_ALERT_TEMP - HYSTERESIS_VALUE);
		gpu_temp_upper = hotspot_temp_to_sensor_temp(
			OMAP_PANIC_TEMP);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			gpu_temp_lower, gpu_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			FAST_TEMP_MONITORING_RATE);
	}

	return 0;
}

/**
 * omap_panic_zone() - Force GPU frequency to a "safe frequency"
 *     . Force the GPU frequency to a “safe” frequency
 *     . Limit max GPU frequency to the “safe” frequency
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @gpu_temp:	The current adjusted GPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_panic_zone(struct list_head *cooling_list, int gpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int gpu_temp_upper = 0;
	int gpu_temp_lower = 0;

	pr_debug("%s:hot spot temp %d\n", __func__, gpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		omap_gov->cooling_level++;
		omap_gov->panic_zone_reached++;
		pr_debug("%s: Panic zone reached %i times\n",
			__func__, omap_gov->panic_zone_reached);
		thermal_cooling_set_level(&cooling_agents,
					omap_gov->cooling_level);
		list_del_init(&cooling_agents);
		gpu_temp_lower = hotspot_temp_to_sensor_temp(
			OMAP_PANIC_TEMP - HYSTERESIS_VALUE);

		/* Set the threshold window to below fatal.  This way the
		 * governor can manage the thermal if the temp should rise
		 * while throttling.  We need to be agressive with throttling
		 * should we reach this zone. */
		gpu_temp_upper = (((OMAP_FATAL_TEMP - OMAP_PANIC_TEMP) / 4) *
				omap_gov->panic_zone_reached) + OMAP_PANIC_TEMP;
		if (gpu_temp_upper >= OMAP_FATAL_TEMP)
			gpu_temp_upper = OMAP_FATAL_TEMP;

		gpu_temp_upper = hotspot_temp_to_sensor_temp(gpu_temp_upper);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			gpu_temp_lower, gpu_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			FAST_TEMP_MONITORING_RATE);
	}

	return 0;
}

/**
 * omap_fatal_zone() - Shut-down the system to ensure OMAP Junction
 *			temperature decreases enough
 *
 * @gpu_temp:	The current adjusted GPU temperature
 *
 * No return forces a restart of the system
 */
static void omap_fatal_zone(int gpu_temp)
{
	pr_emerg("%s:FATAL ZONE (hot spot temp: %i)\n", __func__, gpu_temp);

	kernel_restart(NULL);
}

static int omap_gpu_thermal_manager(struct list_head *cooling_list, int temp)
{
	int gpu_temp;

	gpu_temp = convert_omap_sensor_temp_to_hotspot_temp(temp);
	if (gpu_temp >= OMAP_FATAL_TEMP) {
		omap_fatal_zone(gpu_temp);
		return FATAL_ZONE;
	} else if (gpu_temp >= OMAP_PANIC_TEMP) {
		omap_panic_zone(cooling_list, gpu_temp);
		return PANIC_ZONE;
	} else if (gpu_temp < (OMAP_PANIC_TEMP - HYSTERESIS_VALUE)) {
		if (gpu_temp >= OMAP_ALERT_TEMP) {
			omap_alert_zone(cooling_list, gpu_temp);
			return ALERT_ZONE;
		} else if (gpu_temp < (OMAP_ALERT_TEMP - HYSTERESIS_VALUE)) {
			if (gpu_temp >= OMAP_MONITOR_TEMP) {
				omap_monitor_zone(cooling_list, gpu_temp);
				return MONITOR_ZONE;
			} else {
				/*
				 * this includes the case where :
				 * (OMAP_MONITOR_TEMP - HYSTERESIS_VALUE) <= T
				 * && T < OMAP_MONITOR_TEMP
				 */
				omap_safe_zone(cooling_list, gpu_temp);
				return SAFE_ZONE;
			}
		} else {
			/*
			 * this includes the case where :
			 * (OMAP_ALERT_TEMP - HYSTERESIS_VALUE) <=
			 * T < OMAP_ALERT_TEMP
			 */
			omap_monitor_zone(cooling_list, gpu_temp);
			return MONITOR_ZONE;
		}
	} else {
		/*
		 * this includes the case where :
		 * (OMAP_PANIC_TEMP - HYSTERESIS_VALUE) <= T < OMAP_PANIC_TEMP
		 */
		omap_alert_zone(cooling_list, gpu_temp);
		return ALERT_ZONE;
	}

	return NO_ACTION;

}

static int omap_process_gpu_temp(struct list_head *cooling_list,
				struct thermal_dev *temp_sensor,
				int temp)
{
	pr_debug("%s: Received temp %i\n", __func__, temp);
	omap_gov->temp_sensor = temp_sensor;
	return omap_gpu_thermal_manager(cooling_list, temp);
}

static struct thermal_dev_ops omap_gov_ops = {
	.process_temp = omap_process_gpu_temp,
};

static int __init omap_gpu_governor_init(void)
{
	struct thermal_dev *thermal_fw;
	omap_gov = kzalloc(sizeof(struct omap_gpu_governor), GFP_KERNEL);
	if (!omap_gov) {
		pr_err("%s:Cannot allocate memory\n", __func__);
		return -ENOMEM;
	}

	thermal_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (thermal_fw) {
		thermal_fw->name = "omap_ongpu_governor";
		thermal_fw->domain_name = "gpu";
		thermal_fw->dev_ops = &omap_gov_ops;
		thermal_governor_dev_register(thermal_fw);
		therm_fw = thermal_fw;
	} else {
		pr_err("%s: Cannot allocate memory\n", __func__);
		kfree(omap_gov);
		return -ENOMEM;
	}

	omap_gpu_slope = thermal_get_slope(thermal_fw);
	omap_gpu_const = thermal_get_offset(thermal_fw);

	return 0;
}

static void __exit omap_gpu_governor_exit(void)
{
	thermal_governor_dev_unregister(therm_fw);
	kfree(therm_fw);
	kfree(omap_gov);
}

module_init(omap_gpu_governor_init);
module_exit(omap_gpu_governor_exit);

MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
MODULE_DESCRIPTION("OMAP on-gpu thermal governor");
MODULE_LICENSE("GPL");
