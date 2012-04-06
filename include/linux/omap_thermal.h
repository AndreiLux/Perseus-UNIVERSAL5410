#ifndef	OMAP_THERMAL_H
#define	OMAP_THERMAL_H

#define SENSOR_NAME_LEN	20
#define MAX_COOLING_DEVICE 5

struct thermal_sensor_conf {
	char	name[SENSOR_NAME_LEN];
	int	(*read_temperature)(void *data);
	void	*private_data;
	void	*sensor_data;
};

struct omap_thermal_zone {
	unsigned int idle_interval;
	unsigned int active_interval;
	struct thermal_zone_device *therm_dev;
	struct thermal_cooling_device *cool_dev[MAX_COOLING_DEVICE];
	unsigned int cool_dev_size;
	struct thermal_sensor_conf *sensor_conf;
	struct omap4_thermal_data *sensor_data;
};

/**
 * omap4_register_thermal: Register to the omap thermal interface.
 * @sensor_conf:   Structure containing temperature sensor information
 *
 * returns zero on success, else negative errno.
 */
struct omap_thermal_zone *omap4_register_thermal(
	struct thermal_sensor_conf *sensor_conf);

/**
 * omap4_unregister_thermal: Un-register from the omap thermal interface.
 *
 * return not applicable.
 */
void omap4_unregister_thermal(struct omap_thermal_zone *th_zone);

#endif
