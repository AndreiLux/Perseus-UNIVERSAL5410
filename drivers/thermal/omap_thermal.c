#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/omap_thermal.h>
#include <linux/platform_data/omap4_thermal_data.h>

static int omap_get_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode *mode)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;

	if (th_zone->sensor_conf) {
		pr_info("Temperature sensor not initialised\n");
		*mode = THERMAL_DEVICE_DISABLED;
	} else
		*mode = THERMAL_DEVICE_ENABLED;
	return 0;
}

static int omap_set_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode mode)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;

	if (!th_zone->therm_dev) {
		pr_notice("thermal zone not registered\n");
		return 0;
	}
	if (mode == THERMAL_DEVICE_ENABLED)
		th_zone->therm_dev->polling_delay =
				th_zone->active_interval*1000;
	else
		th_zone->therm_dev->polling_delay =
				th_zone->idle_interval*1000;

	thermal_zone_device_update(th_zone->therm_dev);
	pr_info("thermal polling set for duration=%d sec\n",
				th_zone->therm_dev->polling_delay/1000);
	return 0;
}

static int omap_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	if (trip >= 0 && trip < (thermal->trips - 2))
		*type = THERMAL_TRIP_STATE_ACTIVE;
	else if (trip == (thermal->trips - 1))
		*type = THERMAL_TRIP_CRITICAL;
	else
		return -EINVAL;

	return 0;
}

static int omap_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				 unsigned long *temp)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;

	if (trip >= 0 && trip < thermal->trips)
		*temp = th_zone->sensor_data->trigger_levels[trip];
	else
		return -EINVAL;
	return 0;
}

static int omap_get_crit_temp(struct thermal_zone_device *thermal,
				 unsigned long *temp)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;

	/*Panic zone*/
	*temp = th_zone->sensor_data->trigger_levels[thermal->trips-1];
	return 0;
}
static int omap_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;
	unsigned int i;

	/* if the cooling device is the one from omap4 bind it */
	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (cdev == th_zone->cool_dev[i]) {
			if (thermal_zone_bind_cooling_device(thermal,
								i, cdev)) {
				pr_err("error binding cooling dev\n");
				return -EINVAL;
			}
			break;
		}
	}
	if (i >= th_zone->cool_dev_size) {
		pr_err("error binding cooling dev\n");
		return -EINVAL;
	}

	return 0;
}

static int omap_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;
	unsigned int i;

	/* if the cooling device is the one from omap4 unbind it */
	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (cdev == th_zone->cool_dev[i]) {
			if (thermal_zone_unbind_cooling_device(thermal,
								i, cdev)) {
				pr_err("error unbinding cooling dev\n");
				return -EINVAL;
			}
			break;
		}
	}
	if (i >= th_zone->cool_dev_size) {
		pr_err("error unbinding cooling dev\n");
		return -EINVAL;
	}

	return 0;
}

static int omap_get_temp(struct thermal_zone_device *thermal,
			       unsigned long *temp)
{
	struct omap_thermal_zone *th_zone = thermal->devdata;
	void *data;

	if (!th_zone->sensor_conf) {
		pr_info("Temperature sensor not initialised\n");
		return -EINVAL;
	}
	data = th_zone->sensor_conf->private_data;
	*temp = th_zone->sensor_conf->read_temperature(data);
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops omap_dev_ops = {
	.bind = omap_bind,
	.unbind = omap_unbind,
	.get_temp = omap_get_temp,
	.get_mode = omap_get_mode,
	.set_mode = omap_set_mode,
	.get_trip_type = omap_get_trip_type,
	.get_trip_temp = omap_get_trip_temp,
	.get_crit_temp = omap_get_crit_temp,
};

void omap4_unregister_thermal(struct omap_thermal_zone *th_zone)
{
	unsigned int i;

	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (th_zone && th_zone->cool_dev[i])
			cpufreq_cooling_unregister(th_zone->cool_dev[i]);
	}

	if (th_zone && th_zone->therm_dev)
		thermal_zone_device_unregister(th_zone->therm_dev);

	kfree(th_zone);

	pr_info("omap: Kernel Thermal management unregistered\n");
}
EXPORT_SYMBOL(omap4_unregister_thermal);

struct omap_thermal_zone *omap4_register_thermal(
	struct thermal_sensor_conf *sensor_conf)
{
	int ret = 0, count, tab_size;
	struct freq_pctg_table *tab_ptr;
	struct omap_thermal_zone *th_zone;

	if (!sensor_conf) {
		pr_err("Temperature sensor not initialised\n");
		return ERR_PTR(-EINVAL);
	}

	th_zone = kzalloc(sizeof(struct omap_thermal_zone), GFP_KERNEL);
	if (!th_zone) {
		ret = -ENOMEM;
		goto err_unregister;
	}

	th_zone->sensor_conf = sensor_conf;

	th_zone->sensor_data = sensor_conf->sensor_data;
	if (!th_zone->sensor_data) {
		pr_err("Temperature sensor data not initialised\n");
		ret = -EINVAL;
		goto err_unregister;
	}
	tab_ptr = (struct freq_pctg_table *)th_zone->sensor_data->freq_tab;
	tab_size = th_zone->sensor_data->freq_tab_count;
	/*set the cpu for the thermal clipping if not set*/
	for (count = 0; count < tab_size; count++) {
		th_zone->cool_dev[count] = cpufreq_cooling_register(
			(struct freq_pctg_table *)&(tab_ptr[count]),
			1, cpumask_of(0));
		if (IS_ERR(th_zone->cool_dev[count])) {
			pr_err("Failed to register cpufreq cooling device\n");
			ret = -EINVAL;
			goto err_unregister;
		}
	}
	th_zone->cool_dev_size = tab_size;

	th_zone->therm_dev = thermal_zone_device_register(sensor_conf->name,
			tab_size, th_zone, &omap_dev_ops, 1, 1, 4000, 4000);
	if (IS_ERR(th_zone->therm_dev)) {
		pr_err("Failed to register thermal zone device\n");
		ret = -EINVAL;
		goto err_unregister;
	}

	th_zone->active_interval = 1;
	th_zone->idle_interval = 5;

	omap_set_mode(th_zone->therm_dev, THERMAL_DEVICE_DISABLED);

	pr_info("omap: Kernel Thermal management registered\n");

	return th_zone;

err_unregister:
	omap4_unregister_thermal(th_zone);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(omap4_register_thermal);
