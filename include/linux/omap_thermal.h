#ifndef	OMAP_THERMAL_H
#define	OMAP_THERMAL_H

#define SENSOR_NAME_LEN	20

struct thermal_sensor_conf {
	char	name[SENSOR_NAME_LEN];
	int	(*read_temperature)(void *data);
	void	*private_data;
	void	*sensor_data;
};

/**
 * omap4_register_thermal: Register to the omap thermal interface.
 * @sensor_conf:   Structure containing temperature sensor information
 *
 * returns zero on success, else negative errno.
 */
int omap4_register_thermal(struct thermal_sensor_conf *sensor_conf);

/**
 * omap4_unregister_thermal: Un-register from the omap thermal interface.
 *
 * return not applicable.
 */
void omap4_unregister_thermal(void);

#endif
