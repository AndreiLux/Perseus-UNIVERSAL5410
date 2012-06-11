/*
 * OMAP system control module header file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: J Keerthy <j-keerthy@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/*
 * The register offsets and but fields might change across
 * OMAP versions hence populating them in this structure.
 */

struct omap4460plus_temp_sensor_registers {
	u32	temp_sensor_ctrl;
	u32	bgap_tempsoff_mask;
	u32	bgap_soc_mask;
	u32	bgap_eocz_mask;
	u32	bgap_dtemp_mask;

	u32	bgap_mask_ctrl;
	u32	mask_hot_mask;
	u32	mask_cold_mask;

	u32	bgap_mode_ctrl;
	u32	mode_ctrl_mask;

	u32	bgap_counter;
	u32	counter_mask;

	u32	bgap_threshold;
	u32	threshold_thot_mask;
	u32	threshold_tcold_mask;

	u32	tshut_threshold;
	u32	tshut_hot_mask;
	u32	tshut_cold_mask;

	u32	bgap_status;
	u32	status_clean_stop_mask;
	u32	status_bgap_alert_mask;
	u32	status_hot_mask;
	u32	status_cold_mask;

	u32	bgap_efuse;
};

/*
 * The thresholds and limits for temperature sensors.
 */
struct omap4460plus_temp_sensor_data {
	u32	tshut_hot;
	u32	tshut_cold;
	u32	t_hot;
	u32	t_cold;
	u32	min_freq;
	u32	max_freq;
	int	max_temp;
	int	min_temp;
	int	hyst_val;
	u32	adc_start_val;
	u32	adc_end_val;
	u32	update_int1;
	u32	update_int2;
};

/* forward declaration */
struct scm;

/**
 * struct temp_sensor_hwmon - temperature sensor hwmon device structure
 * @scm_ptr: pointer to system control module structure
 * @pdev: platform device pointer for hwmon device
 * @name: Name of the hwmon temp sensor device
 */
struct temp_sensor_hwmon {
	struct scm		*scm_ptr;
	struct platform_device	*pdev;
	const char		*name;
};

/**
 * struct scm_regval - temperature sensor register values
 * @bg_mode_ctrl: temp sensor control register value
 * @bg_ctrl: bandgap ctrl register value
 * @bg_counter: bandgap counter value
 * @bg_threshold: bandgap threshold register value
 * @tshut_threshold: bandgap tshut register value
 */
struct scm_regval {
	u32			bg_mode_ctrl;
	u32			bg_ctrl;
	u32			bg_counter;
	u32			bg_threshold;
	u32			tshut_threshold;
};

/**
 * struct system control module - scm device structure
 * @dev: device pointer
 * @ts_data: Pointer to struct with thresholds, limits of temperature sensor
 * @registers: Pointer to the list of register offsets and bitfields
 * @therm_fw: Pointer to list of thermal devices
 * @regval: Pointer to list of register values of individual sensors
 * @fclock: pointer to functional clock of temperature sensor
 * @div_clk: pointer to parent clock of temperature sensor fclk
 * @tsh_ptr: pointer to temperature sesnor hwmon struct
 * @name: pointer to list of temperature sensor instance names is scmi
 * @conv_table: Pointer to adc to temperature conversion table
 * @scm_mutex: Mutex for sysfs, irq and PM
 * @irq: MPU Irq number for thermal alert
 * @gpio_tshut: GPIO number for tshut
 * @base: Base of the temp I/O
 * @clk_rate: Holds current clock rate
 * @cnt: count of temperature sensor device in scm
 * @rev: Revision of Temperature sensor
 * @Acc: Accuracy of the temperature
 */
struct scm {
	struct device			*dev;
	struct omap4460plus_temp_sensor_data **ts_data;
	struct omap4460plus_temp_sensor_registers **registers;
#if defined(CONFIG_THERMAL_FRAMEWORK)
	struct thermal_dev		**therm_fw;
#elif defined(CONFIG_CPU_THERMAL)
	struct thermal_sensor_conf	*cpu_therm;
#endif
	struct scm_regval		**regval;
	struct clk		*fclock;
	struct clk		*div_clk;
	struct temp_sensor_hwmon *tsh_ptr;
	const char		**name;
	int			*conv_table;
	struct mutex		scm_mutex; /* Mutex for sysfs, irq and PM */
	unsigned int		irq;
	unsigned int		gpio_tshut;
	void __iomem		*base;
	u32			clk_rate;
	u32			cnt;
	int			rev;
	bool			accurate;
};

extern struct omap4460plus_temp_sensor_registers
			omap4460_mpu_temp_sensor_registers;
extern struct omap4460plus_temp_sensor_registers
			omap5430_mpu_temp_sensor_registers;
extern struct omap4460plus_temp_sensor_registers
			omap5430_gpu_temp_sensor_registers;
extern struct omap4460plus_temp_sensor_registers
			omap5430_core_temp_sensor_registers;
extern int omap4460_adc_to_temp[403];
extern int omap5430_adc_to_temp[403];
extern struct omap4460plus_temp_sensor_data omap4460_mpu_temp_sensor_data;
extern struct omap4460plus_temp_sensor_data omap5430_mpu_temp_sensor_data;
extern struct omap4460plus_temp_sensor_data omap5430_gpu_temp_sensor_data;
extern struct omap4460plus_temp_sensor_data omap5430_core_temp_sensor_data;

u32 omap4plus_scm_readl(struct scm *scm_ptr, u32 reg);
void omap4plus_scm_writel(struct scm *scm_ptr, u32 val, u32 reg);
void temp_sensor_unmask_interrupts(struct scm *scm_ptr, int id, u32 t_hot,
					u32 t_cold);
int omap4460plus_temp_sensor_init(struct scm *scm_ptr);
void omap4460plus_temp_sensor_deinit(struct scm *scm_ptr);
int omap4460_tshut_init(struct scm *scm_ptr);
void omap4460_tshut_deinit(struct scm *scm_ptr);
int omap4460plus_scm_show_temp_max(struct scm *scm_ptr, int id);
int omap4460plus_scm_show_temp_max_hyst(struct scm *scm_ptr, int id);
int omap4460plus_scm_set_temp_max(struct scm *scm_ptr, int id, int val);
int omap4460plus_scm_set_temp_max_hyst(struct scm *scm_ptr,
							int id, int val);
int omap4460plus_scm_show_update_interval(struct scm *scm_ptr, int id);
void omap4460plus_scm_set_update_interval(struct scm *scm_ptr,
						u32 interval, int id);
int omap4460plus_scm_read_temp(struct scm *scm_ptr, int id);
int adc_to_temp_conversion(struct scm *scm_ptr, int id, int val);
