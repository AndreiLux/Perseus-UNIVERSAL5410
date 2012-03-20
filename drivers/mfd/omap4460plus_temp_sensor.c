/*
 * OMAP4 system control module driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: J Keerthy <j-keerthy@ti.com>
 * Author: Moiz Sonasath <m-sonasath@ti.com>
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <plat/omap_device.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <plat/scm.h>
#include <linux/mfd/omap4_scm.h>
#include <mach/ctrl_module_core_44xx.h>

#ifdef CONFIG_THERMAL_FRAMEWORK
#include <linux/thermal_framework.h>
#elif defined(CONFIG_CPU_THERMAL)
#include <linux/omap_thermal.h>
#include <linux/platform_data/omap4_thermal_data.h>
#endif
/* Offsets from the base of temperature sensor registers */

#define OMAP4460_TEMP_SENSOR_CTRL_OFFSET	0x32C
#define OMAP4460_BGAP_CTRL_OFFSET		0x378
#define OMAP4460_BGAP_COUNTER_OFFSET		0x37C
#define OMAP4460_BGAP_THRESHOLD_OFFSET		0x380
#define OMAP4460_BGAP_TSHUT_OFFSET		0x384
#define OMAP4460_BGAP_STATUS_OFFSET		0x388
#define OMAP4460_FUSE_OPP_BGAP			0x260

#define OMAP5430_TEMP_SENSOR_MPU_OFFSET		0x32C
#define OMAP5430_BGAP_CTRL_OFFSET		0x380
#define OMAP5430_BGAP_COUNTER_MPU_OFFSET	0x39C
#define OMAP5430_BGAP_THRESHOLD_MPU_OFFSET	0x384
#define OMAP5430_BGAP_TSHUT_MPU_OFFSET		0x390
#define OMAP5430_BGAP_STATUS_OFFSET		0x3A8
#define OMAP5430_FUSE_OPP_BGAP_MPU		0x1E4

#define OMAP5430_TEMP_SENSOR_GPU_OFFSET		0x330
#define OMAP5430_BGAP_COUNTER_GPU_OFFSET	0x3A0
#define OMAP5430_BGAP_THRESHOLD_GPU_OFFSET	0x388
#define OMAP5430_BGAP_TSHUT_GPU_OFFSET		0x394
#define OMAP5430_FUSE_OPP_BGAP_GPU		0x1E0

#define OMAP5430_TEMP_SENSOR_CORE_OFFSET	0x334
#define OMAP5430_BGAP_COUNTER_CORE_OFFSET	0x3A4
#define OMAP5430_BGAP_THRESHOLD_CORE_OFFSET	0x38C
#define OMAP5430_BGAP_TSHUT_CORE_OFFSET		0x398
#define OMAP5430_FUSE_OPP_BGAP_CORE		0x1E8

#define OMAP4460_TSHUT_HOT		900	/* 122 deg C */
#define OMAP4460_TSHUT_COLD		895	/* 100 deg C */
#define OMAP4460_T_HOT			800	/* 73 deg C */
#define OMAP4460_T_COLD			795	/* 71 deg C */
#define OMAP4460_MAX_FREQ		1500000
#define OMAP4460_MIN_FREQ		1000000
#define OMAP4460_MIN_TEMP		-40000
#define OMAP4460_MAX_TEMP		123000
#define OMAP4460_HYST_VAL		5000
#define OMAP4460_ADC_START_VALUE	530
#define OMAP4460_ADC_END_VALUE		932

#define OMAP5430_MPU_TSHUT_HOT		915
#define OMAP5430_MPU_TSHUT_COLD		900
#define OMAP5430_MPU_T_HOT		800
#define OMAP5430_MPU_T_COLD		795
#define OMAP5430_MPU_MAX_FREQ		1500000
#define OMAP5430_MPU_MIN_FREQ		1000000
#define OMAP5430_MPU_MIN_TEMP		-40000
#define OMAP5430_MPU_MAX_TEMP		125000
#define OMAP5430_MPU_HYST_VAL		5000
#define OMAP5430_ADC_START_VALUE	532
#define OMAP5430_ADC_END_VALUE		934

#define OMAP5430_GPU_TSHUT_HOT		915
#define OMAP5430_GPU_TSHUT_COLD		900
#define OMAP5430_GPU_T_HOT		800
#define OMAP5430_GPU_T_COLD		795
#define OMAP5430_GPU_MAX_FREQ		1500000
#define OMAP5430_GPU_MIN_FREQ		1000000
#define OMAP5430_GPU_MIN_TEMP		-40000
#define OMAP5430_GPU_MAX_TEMP		125000
#define OMAP5430_GPU_HYST_VAL		5000

#define OMAP5430_CORE_TSHUT_HOT		915
#define OMAP5430_CORE_TSHUT_COLD	900
#define OMAP5430_CORE_T_HOT		800
#define OMAP5430_CORE_T_COLD		795
#define OMAP5430_CORE_MAX_FREQ		1500000
#define OMAP5430_CORE_MIN_FREQ		1000000
#define OMAP5430_CORE_MIN_TEMP		-40000
#define OMAP5430_CORE_MAX_TEMP		125000
#define OMAP5430_CORE_HYST_VAL		5000

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
struct omap4460plus_temp_sensor_registers omap4460_mpu_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP4460_TEMP_SENSOR_CTRL_OFFSET,
	.bgap_tempsoff_mask = OMAP4460_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP4460_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP4460_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP4460_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP4460_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP4460_MASK_HOT_MASK,
	.mask_cold_mask = OMAP4460_MASK_COLD_MASK,

	.bgap_mode_ctrl = OMAP4460_BGAP_CTRL_OFFSET,
	.mode_ctrl_mask = OMAP4460_SINGLE_MODE_MASK,

	.bgap_counter = OMAP4460_BGAP_COUNTER_OFFSET,
	.counter_mask = OMAP4460_COUNTER_MASK,

	.bgap_threshold = OMAP4460_BGAP_THRESHOLD_OFFSET,
	.threshold_thot_mask = OMAP4460_T_HOT_MASK,
	.threshold_tcold_mask = OMAP4460_T_COLD_MASK,

	.tshut_threshold = OMAP4460_BGAP_TSHUT_OFFSET,
	.tshut_hot_mask = OMAP4460_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP4460_TSHUT_COLD_MASK,

	.bgap_status = OMAP4460_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = OMAP4460_CLEAN_STOP_MASK,
	.status_bgap_alert_mask = OMAP4460_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP4460_HOT_FLAG_MASK,
	.status_cold_mask = OMAP4460_COLD_FLAG_MASK,

	.bgap_efuse = OMAP4460_FUSE_OPP_BGAP,
};

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
struct omap4460plus_temp_sensor_registers omap5430_mpu_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP5430_TEMP_SENSOR_MPU_OFFSET,
	.bgap_tempsoff_mask = OMAP5430_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP5430_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP5430_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP5430_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP5430_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP5430_MASK_HOT_MPU_MASK,
	.mask_cold_mask = OMAP5430_MASK_COLD_MPU_MASK,

	.bgap_mode_ctrl = OMAP5430_BGAP_COUNTER_MPU_OFFSET,
	.mode_ctrl_mask = OMAP5430_REPEAT_MODE_MASK,

	.bgap_counter = OMAP5430_BGAP_COUNTER_MPU_OFFSET,
	.counter_mask = OMAP5430_COUNTER_MASK,

	.bgap_threshold = OMAP5430_BGAP_THRESHOLD_MPU_OFFSET,
	.threshold_thot_mask = OMAP5430_T_HOT_MASK,
	.threshold_tcold_mask = OMAP5430_T_COLD_MASK,

	.tshut_threshold = OMAP5430_BGAP_TSHUT_MPU_OFFSET,
	.tshut_hot_mask = OMAP5430_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP5430_TSHUT_COLD_MASK,

	.bgap_status = OMAP5430_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = 0x0,
	.status_bgap_alert_mask = OMAP5430_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP5430_HOT_MPU_FLAG_MASK,
	.status_cold_mask = OMAP5430_COLD_MPU_FLAG_MASK,

	.bgap_efuse = OMAP5430_FUSE_OPP_BGAP_MPU,
};

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
struct omap4460plus_temp_sensor_registers omap5430_gpu_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP5430_TEMP_SENSOR_GPU_OFFSET,
	.bgap_tempsoff_mask = OMAP5430_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP5430_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP5430_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP5430_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP5430_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP5430_MASK_HOT_MM_MASK,
	.mask_cold_mask = OMAP5430_MASK_COLD_MM_MASK,

	.bgap_mode_ctrl = OMAP5430_BGAP_COUNTER_GPU_OFFSET,
	.mode_ctrl_mask = OMAP5430_REPEAT_MODE_MASK,

	.bgap_counter = OMAP5430_BGAP_COUNTER_GPU_OFFSET,
	.counter_mask = OMAP5430_COUNTER_MASK,

	.bgap_threshold = OMAP5430_BGAP_THRESHOLD_GPU_OFFSET,
	.threshold_thot_mask = OMAP5430_T_HOT_MASK,
	.threshold_tcold_mask = OMAP5430_T_COLD_MASK,

	.tshut_threshold = OMAP5430_BGAP_TSHUT_GPU_OFFSET,
	.tshut_hot_mask = OMAP5430_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP5430_TSHUT_COLD_MASK,

	.bgap_status = OMAP5430_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = 0x0,
	.status_bgap_alert_mask = OMAP5430_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP5430_HOT_MM_FLAG_MASK,
	.status_cold_mask = OMAP5430_COLD_MM_FLAG_MASK,

	.bgap_efuse = OMAP5430_FUSE_OPP_BGAP_GPU,
};

/*
 * OMAP4460 has one instance of thermal sensor for MPU
 * need to describe the individual bit fields
 */
struct omap4460plus_temp_sensor_registers
				omap5430_core_temp_sensor_registers = {
	.temp_sensor_ctrl = OMAP5430_TEMP_SENSOR_CORE_OFFSET,
	.bgap_tempsoff_mask = OMAP5430_BGAP_TEMPSOFF_MASK,
	.bgap_soc_mask = OMAP5430_BGAP_TEMP_SENSOR_SOC_MASK,
	.bgap_eocz_mask = OMAP5430_BGAP_TEMP_SENSOR_EOCZ_MASK,
	.bgap_dtemp_mask = OMAP5430_BGAP_TEMP_SENSOR_DTEMP_MASK,

	.bgap_mask_ctrl = OMAP5430_BGAP_CTRL_OFFSET,
	.mask_hot_mask = OMAP5430_MASK_HOT_CORE_MASK,
	.mask_cold_mask = OMAP5430_MASK_COLD_CORE_MASK,

	.bgap_mode_ctrl = OMAP5430_BGAP_COUNTER_CORE_OFFSET,
	.mode_ctrl_mask = OMAP5430_REPEAT_MODE_MASK,

	.bgap_counter = OMAP5430_BGAP_COUNTER_CORE_OFFSET,
	.counter_mask = OMAP5430_COUNTER_MASK,

	.bgap_threshold = OMAP5430_BGAP_THRESHOLD_CORE_OFFSET,
	.threshold_thot_mask = OMAP5430_T_HOT_MASK,
	.threshold_tcold_mask = OMAP5430_T_COLD_MASK,

	.tshut_threshold = OMAP5430_BGAP_TSHUT_CORE_OFFSET,
	.tshut_hot_mask = OMAP5430_TSHUT_HOT_MASK,
	.tshut_cold_mask = OMAP5430_TSHUT_COLD_MASK,

	.bgap_status = OMAP5430_BGAP_STATUS_OFFSET,
	.status_clean_stop_mask = 0x0,
	.status_bgap_alert_mask = OMAP5430_BGAP_ALERT_MASK,
	.status_hot_mask = OMAP5430_HOT_CORE_FLAG_MASK,
	.status_cold_mask = OMAP5430_COLD_CORE_FLAG_MASK,

	.bgap_efuse = OMAP5430_FUSE_OPP_BGAP_CORE,
};

/* Thresholds and limits for OMAP4460 MPU temperature sensor */
struct omap4460plus_temp_sensor_data omap4460_mpu_temp_sensor_data = {
	.tshut_hot = OMAP4460_TSHUT_HOT,
	.tshut_cold = OMAP4460_TSHUT_COLD,
	.t_hot = OMAP4460_T_HOT,
	.t_cold = OMAP4460_T_COLD,
	.min_freq = OMAP4460_MIN_FREQ,
	.max_freq = OMAP4460_MAX_FREQ,
	.max_temp = OMAP4460_MAX_TEMP,
	.min_temp = OMAP4460_MIN_TEMP,
	.hyst_val = OMAP4460_HYST_VAL,
	.adc_start_val = OMAP4460_ADC_START_VALUE,
	.adc_end_val = OMAP4460_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/* Thresholds and limits for OMAP5430 MPU temperature sensor */
struct omap4460plus_temp_sensor_data omap5430_mpu_temp_sensor_data = {
	.tshut_hot = OMAP5430_MPU_TSHUT_HOT,
	.tshut_cold = OMAP5430_MPU_TSHUT_COLD,
	.t_hot = OMAP5430_MPU_T_HOT,
	.t_cold = OMAP5430_MPU_T_COLD,
	.min_freq = OMAP5430_MPU_MIN_FREQ,
	.max_freq = OMAP5430_MPU_MAX_FREQ,
	.max_temp = OMAP5430_MPU_MAX_TEMP,
	.min_temp = OMAP5430_MPU_MIN_TEMP,
	.hyst_val = OMAP5430_MPU_HYST_VAL,
	.adc_start_val = OMAP5430_ADC_START_VALUE,
	.adc_end_val = OMAP5430_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/* Thresholds and limits for OMAP5430 GPU temperature sensor */
struct omap4460plus_temp_sensor_data omap5430_gpu_temp_sensor_data = {
	.tshut_hot = OMAP5430_GPU_TSHUT_HOT,
	.tshut_cold = OMAP5430_GPU_TSHUT_COLD,
	.t_hot = OMAP5430_GPU_T_HOT,
	.t_cold = OMAP5430_GPU_T_COLD,
	.min_freq = OMAP5430_GPU_MIN_FREQ,
	.max_freq = OMAP5430_GPU_MAX_FREQ,
	.max_temp = OMAP5430_GPU_MAX_TEMP,
	.min_temp = OMAP5430_GPU_MIN_TEMP,
	.hyst_val = OMAP5430_GPU_HYST_VAL,
	.adc_start_val = OMAP5430_ADC_START_VALUE,
	.adc_end_val = OMAP5430_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/* Thresholds and limits for OMAP5430 CORE temperature sensor */
struct omap4460plus_temp_sensor_data omap5430_core_temp_sensor_data = {
	.tshut_hot = OMAP5430_CORE_TSHUT_HOT,
	.tshut_cold = OMAP5430_CORE_TSHUT_COLD,
	.t_hot = OMAP5430_CORE_T_HOT,
	.t_cold = OMAP5430_CORE_T_COLD,
	.min_freq = OMAP5430_CORE_MIN_FREQ,
	.max_freq = OMAP5430_CORE_MAX_FREQ,
	.max_temp = OMAP5430_CORE_MAX_TEMP,
	.min_temp = OMAP5430_CORE_MIN_TEMP,
	.hyst_val = OMAP5430_CORE_HYST_VAL,
	.adc_start_val = OMAP5430_ADC_START_VALUE,
	.adc_end_val = OMAP5430_ADC_END_VALUE,
	.update_int1 = 1000,
	.update_int2 = 2000,
};

/*
 * Temperature values in milli degree celsius
 * ADC code values from 530 to 923
 */
int omap4460_adc_to_temp[OMAP4460_ADC_END_VALUE - OMAP4460_ADC_START_VALUE + 1]
	= {
	-40000, -40000, -40000, -40000, -39800, -39400, -39000, -38600, -38200,
	-37800, -37300, -36800, -36400, -36000, -35600, -35200, -34800,
	-34300, -33800, -33400, -33000, -32600, -32200, -31800, -31300,
	-30800, -30400, -30000, -29600, -29200, -28700, -28200, -27800,
	-27400, -27000, -26600, -26200, -25700, -25200, -24800, -24400,
	-24000, -23600, -23200, -22700, -22200, -21800, -21400, -21000,
	-20600, -20200, -19700, -19200, -18800, -18400, -18000, -17600,
	-17200, -16700, -16200, -15800, -15400, -15000, -14600, -14200,
	-13700, -13200, -12800, -12400, -12000, -11600, -11200, -10700,
	-10200, -9800, -9400, -9000, -8600, -8200, -7700, -7200, -6800,
	-6400, -6000, -5600, -5200, -4800, -4300, -3800, -3400, -3000,
	-2600, -2200, -1800, -1300, -800, -400, 0, 400, 800, 1200, 1600,
	2100, 2600, 3000, 3400, 3800, 4200, 4600, 5100, 5600, 6000, 6400,
	6800, 7200, 7600, 8000, 8500, 9000, 9400, 9800, 10200, 10600, 11000,
	11400, 11900, 12400, 12800, 13200, 13600, 14000, 14400, 14800,
	15300, 15800, 16200, 16600, 17000, 17400, 17800, 18200, 18700,
	19200, 19600, 20000, 20400, 20800, 21200, 21600, 22100, 22600,
	23000, 23400, 23800, 24200, 24600, 25000, 25400, 25900, 26400,
	26800, 27200, 27600, 28000, 28400, 28800, 29300, 29800, 30200,
	30600, 31000, 31400, 31800, 32200, 32600, 33100, 33600, 34000,
	34400, 34800, 35200, 35600, 36000, 36400, 36800, 37300, 37800,
	38200, 38600, 39000, 39400, 39800, 40200, 40600, 41100, 41600,
	42000, 42400, 42800, 43200, 43600, 44000, 44400, 44800, 45300,
	45800, 46200, 46600, 47000, 47400, 47800, 48200, 48600, 49000,
	49500, 50000, 50400, 50800, 51200, 51600, 52000, 52400, 52800,
	53200, 53700, 54200, 54600, 55000, 55400, 55800, 56200, 56600,
	57000, 57400, 57800, 58200, 58700, 59200, 59600, 60000, 60400,
	60800, 61200, 61600, 62000, 62400, 62800, 63300, 63800, 64200,
	64600, 65000, 65400, 65800, 66200, 66600, 67000, 67400, 67800,
	68200, 68700, 69200, 69600, 70000, 70400, 70800, 71200, 71600,
	72000, 72400, 72800, 73200, 73600, 74100, 74600, 75000, 75400,
	75800, 76200, 76600, 77000, 77400, 77800, 78200, 78600, 79000,
	79400, 79800, 80300, 80800, 81200, 81600, 82000, 82400, 82800,
	83200, 83600, 84000, 84400, 84800, 85200, 85600, 86000, 86400,
	86800, 87300, 87800, 88200, 88600, 89000, 89400, 89800, 90200,
	90600, 91000, 91400, 91800, 92200, 92600, 93000, 93400, 93800,
	94200, 94600, 95000, 95500, 96000, 96400, 96800, 97200, 97600,
	98000, 98400, 98800, 99200, 99600, 100000, 100400, 100800, 101200,
	101600, 102000, 102400, 102800, 103200, 103600, 104000, 104400,
	104800, 105200, 105600, 106100, 106600, 107000, 107400, 107800,
	108200, 108600, 109000, 109400, 109800, 110200, 110600, 111000,
	111400, 111800, 112200, 112600, 113000, 113400, 113800, 114200,
	114600, 115000, 115400, 115800, 116200, 116600, 117000, 117400,
	117800, 118200, 118600, 119000, 119400, 119800, 120200, 120600,
	121000, 121400, 121800, 122200, 122600, 123000, 123400, 123800, 124200,
	124600, 124900, 125000, 125000, 125000, 125000
};

int omap5430_adc_to_temp[OMAP5430_ADC_END_VALUE - OMAP5430_ADC_START_VALUE + 1]
	= {
	-40000, -40000, -40000, -40000, -39800, -39400, -39000, -38600,
	-38200, -37800, -37300, -36800,
	-36400, -36000, -35600, -35200, -34800, -34300, -33800, -33400, -33000,
	-32600,
	-32200, -31800, -31300, -30800, -30400, -30000, -29600, -29200, -28700,
	-28200, -27800, -27400, -27000, -26600, -26200, -25700, -25200, -24800,
	-24400, -24000, -23600, -23200, -22700, -22200, -21800, -21400, -21000,
	-20600, -20200, -19700, -19200, -9300, -18400, -18000, -17600, -17200,
	-16700, -16200, -15800, -15400, -15000, -14600, -14200, -13700, -13200,
	-12800, -12400, -12000, -11600, -11200, -10700, -10200, -9800, -9400,
	-9000,
	-8600, -8200, -7700, -7200, -6800, -6400, -6000, -5600, -5200, -4800,
	-4300,
	-3800, -3400, -3000, -2600, -2200, -1800, -1300, -800, -400, 0, 400,
	800,
	1200, 1600, 2100, 2600, 3000, 3400, 3800, 4200, 4600, 5100, 5600, 6000,
	6400, 6800, 7200, 7600, 8000, 8500, 9000, 9400, 9800, 10200, 10800,
	11100,
	11400, 11900, 12400, 12800, 13200, 13600, 14000, 14400, 14800, 15300,
	15800,
	16200, 16600, 17000, 17400, 17800, 18200, 18700, 19200, 19600, 20000,
	20400,
	20800, 21200, 21600, 22100, 22600, 23000, 23400, 23800, 24200, 24600,
	25000,
	25400, 25900, 26400, 26800, 27200, 27600, 28000, 28400, 28800, 29300,
	29800,
	30200, 30600, 31000, 31400, 31800, 32200, 32600, 33100, 33600, 34000,
	34400,
	34800, 35200, 35600, 36000, 36400, 36800, 37300, 37800, 38200, 38600,
	39000,
	39400, 39800, 40200, 40600, 41100, 41600, 42000, 42400, 42800, 43200,
	43600,
	44000, 44400, 44800, 45300, 45800, 46200, 46600, 47000, 47400, 47800,
	48200,
	48600, 49000, 49500, 50000, 50400, 50800, 51200, 51600, 52000, 52400,
	52800,
	53200, 53700, 54200, 54600, 55000, 55400, 55800, 56200, 56600, 57000,
	57400,
	57800, 58200, 58700, 59200, 59600, 60000, 60400, 60800, 61200, 61600,
	62000,
	62400, 62800, 63300, 63800, 64200, 64600, 65000, 65400, 65800, 66200,
	66600,
	67000, 67400, 67800, 68200, 68700, 69200, 69600, 70000, 70400, 70800,
	71200,
	71600, 72000, 72400, 72800, 73200, 73600, 74100, 74600, 75000, 75400,
	75800,
	76200, 76600, 77000, 77400, 77800, 78200, 78600, 79000, 79400, 79800,
	80300,
	80800, 81200, 81600, 82000, 82400, 82800, 83200, 83600, 84000, 84400,
	84800,
	85200, 85600, 86000, 86400, 86800, 87300, 87800, 88200, 88600, 89000,
	89400,
	89800, 90200, 90600, 91000, 91400, 91800, 92200, 92600, 93000, 93400,
	93800,
	94200, 94600, 95000, 95500, 96000, 96400, 96800, 97200, 97600, 98000,
	98400,
	98800, 99200, 99600, 100000, 100400, 100800, 101200, 101600, 102000,
	102400,
	102800, 103200, 103600, 104000, 104400, 104800, 105200, 105600, 106100,
	106600, 107000, 107400, 107800, 108200, 108600, 109000, 109400, 109800,
	110200, 110600, 111000, 111400, 111800, 112200, 112600, 113000, 113400,
	113800, 114200, 114600, 115000, 115400, 115800, 116200, 116600, 117000,
	117400, 117800, 118200, 118600, 119000, 119400, 119800, 120200, 120600,
	121000, 121400, 121800, 122200, 122600, 123000, 123400, 123800, 124200,
	124600, 124900, 125000, 125000, 125000, 125000,
};

int adc_to_temp_conversion(struct scm *scm_ptr, int id, int adc_val)
{
	return scm_ptr->conv_table[adc_val -
				   scm_ptr->ts_data[id]->adc_start_val];
}

static int temp_to_adc_conversion(long temp, struct scm *scm_ptr, int i)
{
	int high, low, mid;

	if (temp < scm_ptr->conv_table[0] ||
	    temp > scm_ptr->conv_table[scm_ptr->ts_data[i]->adc_end_val
				       - scm_ptr->ts_data[i]->adc_start_val])
		return -EINVAL;

	high = scm_ptr->ts_data[i]->adc_end_val -
	    scm_ptr->ts_data[i]->adc_start_val;
	low = 0;
	mid = (high + low) / 2;

	while (low < high) {
		if (temp < scm_ptr->conv_table[mid])
			high = mid - 1;
		else
			low = mid + 1;
		mid = (low + high) / 2;
	}

	return scm_ptr->ts_data[i]->adc_start_val + low;
}

void temp_sensor_unmask_interrupts(struct scm *scm_ptr, int id,
				   u32 t_hot, u32 t_cold)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	u32 temp, reg_val;

	/* Read the current on die temperature */
	tsr = scm_ptr->registers[id];
	temp = omap4plus_scm_readl(scm_ptr, tsr->temp_sensor_ctrl);
	temp &= tsr->bgap_dtemp_mask;

	reg_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_mask_ctrl);
	if (temp < t_hot)
		reg_val |= tsr->mask_hot_mask;
	else
		reg_val &= ~tsr->mask_hot_mask;

	if (t_cold < temp)
		reg_val |= tsr->mask_cold_mask;
	else
		reg_val &= ~tsr->mask_cold_mask;
	omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_mask_ctrl);
}

static int add_hyst(int adc_val, int hyst_val, struct scm *scm_ptr, int i)
{
	int temp = adc_to_temp_conversion(scm_ptr, i, adc_val);

	temp += hyst_val;

	return temp_to_adc_conversion(temp, scm_ptr, i);
}

static void temp_sensor_configure_thot(struct scm *scm_ptr, int id, int t_hot)
{
	int cold, thresh_val;
	struct omap4460plus_temp_sensor_registers *tsr;
	u32 reg_val;

	tsr = scm_ptr->registers[id];
	/* obtain the T cold value */
	thresh_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);
	cold = (thresh_val & tsr->threshold_tcold_mask) >>
	    __ffs(tsr->threshold_tcold_mask);
	if (t_hot <= cold) {
		/* change the t_cold to t_hot - 5000 millidegrees */
		cold = add_hyst(t_hot, -scm_ptr->ts_data[id]->hyst_val,
				scm_ptr, id);
		/* write the new t_cold value */
		reg_val = thresh_val & (~tsr->threshold_tcold_mask);
		reg_val |= cold << __ffs(tsr->threshold_tcold_mask);
		omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_threshold);
		thresh_val = reg_val;
	}

	/* write the new t_hot value */
	reg_val = thresh_val & ~tsr->threshold_thot_mask;
	reg_val |= (t_hot << __ffs(tsr->threshold_thot_mask));
	omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_threshold);
	temp_sensor_unmask_interrupts(scm_ptr, id, t_hot, cold);
}

static void temp_sensor_init_talert_thresholds(struct scm *scm_ptr, int id, int
			t_hot, int t_cold)
{
	u32 reg_val, thresh_val;
	struct omap4460plus_temp_sensor_registers *tsr;

	tsr = scm_ptr->registers[id];
	thresh_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);

	/* write the new t_cold value */
	reg_val = thresh_val & ~tsr->threshold_tcold_mask;
	reg_val |= (t_cold << __ffs(tsr->threshold_tcold_mask));
	omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_threshold);

	thresh_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);

	/* write the new t_hot value */
	reg_val = thresh_val & ~tsr->threshold_thot_mask;
	reg_val |= (t_hot << __ffs(tsr->threshold_thot_mask));
	omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_threshold);

	reg_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_mask_ctrl);
	reg_val |= tsr->mask_hot_mask;
	reg_val |= tsr->mask_cold_mask;
	omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_mask_ctrl);
}

static void temp_sensor_configure_tcold(struct scm *scm_ptr, int id, int t_cold)
{
	int hot, thresh_val;
	u32 reg_val;
	struct omap4460plus_temp_sensor_registers *tsr;

	tsr = scm_ptr->registers[id];
	/* obtain the T cold value */
	thresh_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);
	hot = (thresh_val & tsr->threshold_thot_mask) >>
	    __ffs(tsr->threshold_thot_mask);

	if (t_cold >= hot) {
		/* change the t_hot to t_cold + 5000 millidegrees */
		hot = add_hyst(t_cold, scm_ptr->ts_data[id]->hyst_val,
			       scm_ptr, id);
		/* write the new t_hot value */
		reg_val = thresh_val & (~tsr->threshold_thot_mask);
		reg_val |= hot << __ffs(tsr->threshold_thot_mask);
		omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_threshold);
		thresh_val = reg_val;
	}

	/* write the new t_cold value */
	reg_val = thresh_val & ~tsr->threshold_tcold_mask;
	reg_val |= (t_cold << __ffs(tsr->threshold_tcold_mask));
	omap4plus_scm_writel(scm_ptr, reg_val, tsr->bgap_threshold);
	temp_sensor_unmask_interrupts(scm_ptr, id, hot, t_cold);
}

static void temp_sensor_configure_tshut_hot(struct scm *scm_ptr,
					    int id, int tshut_hot)
{
	u32 reg_val;
	struct omap4460plus_temp_sensor_registers *tsr;

	tsr = scm_ptr->registers[id];
	reg_val = omap4plus_scm_readl(scm_ptr, tsr->tshut_threshold);
	reg_val &= ~scm_ptr->registers[id]->tshut_hot_mask;
	reg_val |= tshut_hot << __ffs(scm_ptr->registers[id]->tshut_hot_mask);
	omap4plus_scm_writel(scm_ptr, reg_val,
			     scm_ptr->registers[id]->tshut_threshold);
}

static void temp_sensor_configure_tshut_cold(struct scm *scm_ptr,
					     int id, int tshut_cold)
{
	u32 reg_val;
	struct omap4460plus_temp_sensor_registers *tsr;

	tsr = scm_ptr->registers[id];
	reg_val = omap4plus_scm_readl(scm_ptr, tsr->tshut_threshold);
	reg_val &= ~scm_ptr->registers[id]->tshut_cold_mask;
	reg_val |= tshut_cold << __ffs(scm_ptr->registers[id]->tshut_cold_mask);
	omap4plus_scm_writel(scm_ptr, reg_val,
			     scm_ptr->registers[id]->tshut_threshold);
}

static void configure_temp_sensor_counter(struct scm *scm_ptr, int id,
					  u32 counter)
{
	u32 val;

	val = omap4plus_scm_readl(scm_ptr,
				  scm_ptr->registers[id]->bgap_counter);
	val &= ~scm_ptr->registers[id]->counter_mask;
	val |= counter << __ffs(scm_ptr->registers[id]->counter_mask);
	omap4plus_scm_writel(scm_ptr, val,
			     scm_ptr->registers[id]->bgap_counter);

}

int omap4460plus_scm_show_temp_max(struct scm *scm_ptr, int id)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	int temp;

	tsr = scm_ptr->registers[id];
	temp = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);
	temp = (temp & tsr->threshold_thot_mask)
	    >> __ffs(tsr->threshold_thot_mask);
	temp = adc_to_temp_conversion(scm_ptr, id, temp);

	return temp;
}
EXPORT_SYMBOL(omap4460plus_scm_show_temp_max);

int omap4460plus_scm_set_temp_max(struct scm *scm_ptr, int id, int val)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	u32 t_hot;
	tsr = scm_ptr->registers[id];

	if (val < scm_ptr->ts_data[id]->min_temp +
	    scm_ptr->ts_data[id]->hyst_val)
		return -EINVAL;
	t_hot = temp_to_adc_conversion(val, scm_ptr, id);
	if (t_hot < 0)
		return t_hot;

	mutex_lock(&scm_ptr->scm_mutex);
	temp_sensor_configure_thot(scm_ptr, id, t_hot);
	mutex_unlock(&scm_ptr->scm_mutex);

	return 0;
}
EXPORT_SYMBOL(omap4460plus_scm_set_temp_max);

int omap4460plus_scm_show_temp_max_hyst(struct scm *scm_ptr, int id)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	int temp;

	tsr = scm_ptr->registers[id];
	temp = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);
	temp = (temp & tsr->threshold_tcold_mask)
	    >> __ffs(tsr->threshold_tcold_mask);
	temp = adc_to_temp_conversion(scm_ptr, id, temp);

	return temp;
}
EXPORT_SYMBOL(omap4460plus_scm_show_temp_max_hyst);

int omap4460plus_scm_set_temp_max_hyst(struct scm *scm_ptr, int id, int val)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	u32 t_cold;
	tsr = scm_ptr->registers[id];
	if (val > scm_ptr->ts_data[id]->max_temp +
	    scm_ptr->ts_data[id]->hyst_val)
		return -EINVAL;

	t_cold = temp_to_adc_conversion(val, scm_ptr, id);
	if (t_cold < 0)
		return t_cold;

	mutex_lock(&scm_ptr->scm_mutex);
	temp_sensor_configure_tcold(scm_ptr, id, t_cold);
	mutex_unlock(&scm_ptr->scm_mutex);

	return 0;
}
EXPORT_SYMBOL(omap4460plus_scm_set_temp_max_hyst);

void omap4460plus_scm_set_update_interval(struct scm *scm_ptr,
					  u32 interval, int id)
{
	interval = interval * scm_ptr->clk_rate / 1000;
	mutex_lock(&scm_ptr->scm_mutex);
	configure_temp_sensor_counter(scm_ptr, id, interval);
	mutex_unlock(&scm_ptr->scm_mutex);
}
EXPORT_SYMBOL(omap4460plus_scm_set_update_interval);

int omap4460plus_scm_show_update_interval(struct scm *scm_ptr, int id)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	int time;
	tsr = scm_ptr->registers[id];

	time = omap4plus_scm_readl(scm_ptr, tsr->bgap_counter);
	time = (time & tsr->counter_mask) >> __ffs(tsr->counter_mask);
	time = time * 1000 / scm_ptr->clk_rate;

	return time;
}
EXPORT_SYMBOL(omap4460plus_scm_show_update_interval);

int omap4460plus_scm_read_temp(struct scm *scm_ptr, int id)
{
	struct omap4460plus_temp_sensor_registers *tsr;
	int temp;
	tsr = scm_ptr->registers[id];

	temp = omap4plus_scm_readl(scm_ptr, tsr->temp_sensor_ctrl);
	temp &= tsr->bgap_dtemp_mask;
	/* look up for temperature in the table and return the temperature */
	if (temp < scm_ptr->ts_data[id]->adc_start_val ||
	    temp > scm_ptr->ts_data[id]->adc_end_val)
		return -EIO;

	return adc_to_temp_conversion(scm_ptr, id, temp);
}
EXPORT_SYMBOL(omap4460plus_scm_read_temp);

/**
 * enable_continuous_mode() - One time enbaling of continuous conversion mode
 * @scm_ptr - pointer to scm instance
 */
static void enable_continuous_mode(struct scm *scm_ptr)
{
	u32 val;
	int i;

	for (i = 0; i < scm_ptr->cnt; i++) {
		val = omap4plus_scm_readl(scm_ptr,
					  scm_ptr->
					  registers[i]->bgap_mode_ctrl);

		val |= 1 << __ffs(scm_ptr->registers[i]->mode_ctrl_mask);

		omap4plus_scm_writel(scm_ptr, val,
				     scm_ptr->registers[i]->bgap_mode_ctrl);
	}
}

int omap4460_tshut_init(struct scm *scm_ptr)
{
	int status, gpio_nr = 86;

	/* Request for gpio_86 line */
	status = gpio_request(gpio_nr, "tshut");
	if (status < 0) {
		pr_err("Could not request for TSHUT GPIO:%i\n", 86);
		return status;
	}
	status = gpio_direction_input(gpio_nr);
	if (status) {
		pr_err("Cannot set input TSHUT GPIO %d\n", gpio_nr);
		return status;
	}

	return 0;
}

#ifdef CONFIG_THERMAL_FRAMEWORK
static int omap_report_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct scm *scm_ptr = platform_get_drvdata(pdev);
	int id = tdev->sen_id;

	scm_ptr->therm_fw[id]->current_temp =
	    omap4460plus_scm_read_temp(scm_ptr, id);

	if (scm_ptr->therm_fw[id]->current_temp != -EINVAL) {
		thermal_sensor_set_temp(scm_ptr->therm_fw[id]);
		kobject_uevent(&scm_ptr->tsh_ptr[id].pdev->dev.kobj,
			       KOBJ_CHANGE);
	}

	return scm_ptr->therm_fw[id]->current_temp;
}

static int omap_set_temp_thresh(struct thermal_dev *tdev, int min, int max)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct scm *scm_ptr = platform_get_drvdata(pdev);
	int ret, t_cold, t_hot, thresh_val, hot, cold;
	int id = tdev->sen_id;
	struct omap4460plus_temp_sensor_registers *tsr;

	tsr = scm_ptr->registers[id];
	t_cold = temp_to_adc_conversion(min, scm_ptr, id);
	t_hot = temp_to_adc_conversion(max, scm_ptr, id);

	thresh_val = omap4plus_scm_readl(scm_ptr, tsr->bgap_threshold);
	hot = (thresh_val & tsr->threshold_thot_mask) >>
		__ffs(tsr->threshold_thot_mask);
	cold = (thresh_val & tsr->threshold_tcold_mask) >>
			__ffs(tsr->threshold_tcold_mask);

	if (t_hot < cold) {
		ret = omap4460plus_scm_set_temp_max_hyst(scm_ptr, id, min);
		ret = omap4460plus_scm_set_temp_max(scm_ptr, id, max);
	} else {
		ret = omap4460plus_scm_set_temp_max(scm_ptr, id, max);
		ret = omap4460plus_scm_set_temp_max_hyst(scm_ptr, id, min);
	}

	return ret;
}

static int omap_set_measuring_rate(struct thermal_dev *tdev, int rate)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct scm *scm_ptr = platform_get_drvdata(pdev);
	int id = tdev->sen_id;

	omap4460plus_scm_set_update_interval(scm_ptr, rate, id);

	return rate;
}

static int omap_report_slope(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct scm *scm_ptr = platform_get_drvdata(pdev);
	int id = tdev->sen_id;
	return scm_ptr->therm_fw[id]->slope;
}

static int omap_report_offset(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct scm *scm_ptr = platform_get_drvdata(pdev);
	int id = tdev->sen_id;

	return scm_ptr->therm_fw[id]->constant_offset;
}

static struct thermal_dev_ops omap_sensor_ops = {
	.report_temp = omap_report_temp,
	.set_temp_thresh = omap_set_temp_thresh,
	.set_temp_report_rate = omap_set_measuring_rate,
	.init_slope = omap_report_slope,
	.init_offset = omap_report_offset
};
#elif defined(CONFIG_CPU_THERMAL)

/* temp/frequency table for 1.20GHz chip */
static struct omap4_thermal_data omap4_1200mhz_bandgap_data = {
	.trigger_levels[0] = 73000,
	.trigger_levels[1] = 79000,
	.trigger_levels[2] = 86000,
	.trigger_levels[3] = 93000,
	/* 920Mhz */
	.freq_tab[0] = {
		.freq_clip_pctg = 24,
		.polling_interval = 4,
		},
	/* 700MHz */
	.freq_tab[1] = {
		.freq_clip_pctg = 41,
		.polling_interval = 2,
		},
	/* 350MHz */
	.freq_tab[2] = {
		.freq_clip_pctg = 70,
		.polling_interval = 1,
		},
	/* Shutdown */
	.freq_tab[3] = {
		.freq_clip_pctg = 99,
		},
	.freq_tab_count = 4,
};

static int omap_read_temp(void *private_data)
{
	struct scm *scm_ptr = private_data;
	int temp;

	temp = omap4460plus_scm_read_temp(scm_ptr, 0);
	return temp;
}

#endif

int omap4460plus_temp_sensor_init(struct scm *scm_ptr)
{
	struct omap_temp_sensor_registers *reg_ptr;
	struct omap4460plus_temp_sensor_data *ts_ptr;
	struct scm_regval *regval_ptr;
	int clk_rate, ret, i;

	scm_ptr->registers = kzalloc(sizeof(reg_ptr) *
				     scm_ptr->cnt, GFP_KERNEL);
	if (!scm_ptr->registers) {
		pr_err("Unable to allocate mem for scm registers\n");
		return -ENOMEM;
	}

	scm_ptr->ts_data = kzalloc(sizeof(ts_ptr) * scm_ptr->cnt, GFP_KERNEL);
	if (!scm_ptr->ts_data) {
		pr_err("Unable to allocate memory for ts data\n");
		ret = -ENOMEM;
		goto fail_alloc;
	}

	scm_ptr->regval = kzalloc(sizeof(regval_ptr) * scm_ptr->cnt,
						GFP_KERNEL);
	if (!scm_ptr->regval) {
		pr_err("Unable to allocate memory for regval\n");
		ret = -ENOMEM;
		goto fail_alloc;
	}

#ifdef CONFIG_THERMAL_FRAMEWORK
	scm_ptr->therm_fw = kzalloc(sizeof(struct thermal_dev *)
					* scm_ptr->cnt, GFP_KERNEL);
	if (!scm_ptr->therm_fw) {
		pr_err("Unable to allocate memory for ts data\n");
		return -ENOMEM;
	}
#elif defined(CONFIG_CPU_THERMAL)
	scm_ptr->cpu_therm = kzalloc(sizeof(struct thermal_sensor_conf)
		 * scm_ptr->cnt, GFP_KERNEL);
#endif

	for (i = 0; i < scm_ptr->cnt; i++) {
#ifdef CONFIG_THERMAL_FRAMEWORK
		scm_ptr->therm_fw[i] =
				kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
#endif
		scm_ptr->regval[i] =
			kzalloc(sizeof(struct scm_regval), GFP_KERNEL);
	}

	if (scm_ptr->rev == 1) {
		scm_ptr->registers[0] = &omap4460_mpu_temp_sensor_registers;
		scm_ptr->ts_data[0] = &omap4460_mpu_temp_sensor_data;
		scm_ptr->conv_table = omap4460_adc_to_temp;
		/*
		 * check if the efuse has a non-zero value if not
		 * it is an untrimmed sample and the temperatures
		 * may not be accurate
		 */
		if (!omap4plus_scm_readl(scm_ptr,
					scm_ptr->registers[0]->bgap_efuse))
			pr_info("Non-trimmed BGAP, Temp not accurate\n");
#ifdef CONFIG_THERMAL_FRAMEWORK
		if (scm_ptr->therm_fw) {
			scm_ptr->therm_fw[0]->name = "omap_ondie_sensor";
			scm_ptr->therm_fw[0]->domain_name = "cpu";
			scm_ptr->therm_fw[0]->dev = scm_ptr->dev;
			scm_ptr->therm_fw[0]->dev_ops = &omap_sensor_ops;
			scm_ptr->therm_fw[0]->sen_id = 0;
			scm_ptr->therm_fw[0]->slope = 376;
			scm_ptr->therm_fw[0]->constant_offset =  -16000;
			thermal_sensor_dev_register(scm_ptr->therm_fw[0]);
		} else {
			pr_err("%s:Cannot alloc memory for thermal fw\n",
			       __func__);
			ret = -ENOMEM;
		}
#elif defined(CONFIG_CPU_THERMAL)
		strncpy(scm_ptr->cpu_therm[0].name, "omap_ondie_sensor", SENSOR_NAME_LEN);
		scm_ptr->cpu_therm[0].private_data = scm_ptr;
		scm_ptr->cpu_therm[0].read_temperature = omap_read_temp;
		scm_ptr->cpu_therm[0].sensor_data = &omap4_1200mhz_bandgap_data;
		if (!omap4_register_thermal(scm_ptr->cpu_therm)) {
			/* kzfree() */
			scm_ptr->cpu_therm = NULL;
		}
#endif
	} else if (scm_ptr->rev == 2) {
		scm_ptr->registers[0] = &omap5430_mpu_temp_sensor_registers;
		scm_ptr->ts_data[0] = &omap5430_mpu_temp_sensor_data;
		scm_ptr->registers[1] = &omap5430_gpu_temp_sensor_registers;
		scm_ptr->ts_data[1] = &omap5430_gpu_temp_sensor_data;
		scm_ptr->registers[2] = &omap5430_core_temp_sensor_registers;
		scm_ptr->ts_data[2] = &omap5430_core_temp_sensor_data;
		scm_ptr->conv_table = omap5430_adc_to_temp;
#ifdef CONFIG_THERMAL_FRAMEWORK
		if (scm_ptr->therm_fw) {
			scm_ptr->therm_fw[0]->name = "omap_ondie_mpu_sensor";
			scm_ptr->therm_fw[0]->domain_name = "cpu";
			scm_ptr->therm_fw[0]->dev = scm_ptr->dev;
			scm_ptr->therm_fw[0]->dev_ops = &omap_sensor_ops;
			scm_ptr->therm_fw[0]->sen_id = 0;
			scm_ptr->therm_fw[0]->slope = 196;
			scm_ptr->therm_fw[0]->constant_offset =  -6822;
			thermal_sensor_dev_register(scm_ptr->therm_fw[0]);
			scm_ptr->therm_fw[1]->name = "omap_ondie_gpu_sensor";
			scm_ptr->therm_fw[1]->domain_name = "gpu";
			scm_ptr->therm_fw[1]->dev = scm_ptr->dev;
			scm_ptr->therm_fw[1]->dev_ops = &omap_sensor_ops;
			scm_ptr->therm_fw[1]->sen_id = 1;
			scm_ptr->therm_fw[1]->slope = 0;
			scm_ptr->therm_fw[1]->constant_offset =  7000;
			thermal_sensor_dev_register(scm_ptr->therm_fw[1]);
			scm_ptr->therm_fw[2]->name = "omap_ondie_core_sensor";
			scm_ptr->therm_fw[2]->domain_name = "core";
			scm_ptr->therm_fw[2]->dev = scm_ptr->dev;
			scm_ptr->therm_fw[2]->dev_ops = &omap_sensor_ops;
			scm_ptr->therm_fw[2]->sen_id = 2;
			scm_ptr->therm_fw[2]->slope = 0;
			scm_ptr->therm_fw[2]->constant_offset = 0;
			thermal_sensor_dev_register(scm_ptr->therm_fw[2]);
			/*TO DO:  Add for GPU and core */
		} else {
			pr_err("%s:Cannot alloc memory for thermal fw\n",
			       __func__);
			ret = -ENOMEM;
		}
#elif defined(CONFIG_CPU_THERMAL)
		pr_err("CPU_THERMAL not supported on OMAP5 yet");
		return -ENODEV;
#endif
	}

	if (scm_ptr->rev == 1) {

		clk_rate = clk_round_rate(scm_ptr->div_clk,
					  scm_ptr->ts_data[0]->max_freq);
		if (clk_rate < scm_ptr->ts_data[0]->min_freq ||
		    clk_rate == 0xffffffff) {
			ret = -ENODEV;
			goto clk_err;
		}

		ret = clk_set_rate(scm_ptr->div_clk, clk_rate);
		if (ret) {
			pr_err("Cannot set clock rate\n");
			goto clk_err;
		}

		scm_ptr->clk_rate = clk_rate;
	} else if (scm_ptr->rev == 2) {
			/* add clock rate code for omap5430 */
#ifdef CONFIG_MACH_OMAP_5430ZEBU
		scm_ptr->clk_rate = 1200;
#else

		clk_rate = clk_round_rate(scm_ptr->fclock,
				scm_ptr->ts_data[0]->max_freq);

		if (clk_rate < scm_ptr->ts_data[0]->min_freq ||
			clk_rate == 0xffffffff) {
			ret = -ENODEV;
			goto clk_err;
		}

		ret = clk_set_rate(scm_ptr->fclock, clk_rate);
		if (ret) {
			pr_err("Cannot set clock rate\n");
			goto clk_err;
		}
		scm_ptr->clk_rate = clk_rate;
#endif
	}

	clk_enable(scm_ptr->fclock);
	/* 1 clk cycle */
	for (i = 0; i < scm_ptr->cnt; i++)
		configure_temp_sensor_counter(scm_ptr, i, 1);
	for (i = 0; i < scm_ptr->cnt; i++) {
		temp_sensor_init_talert_thresholds(scm_ptr, i,
				scm_ptr->ts_data[i]->t_hot,
					scm_ptr->ts_data[i]->t_cold);
		temp_sensor_configure_tshut_hot(scm_ptr, i,
						scm_ptr->ts_data[i]->tshut_hot);
		temp_sensor_configure_tshut_cold(scm_ptr, i,
						 scm_ptr->ts_data[i]->
						 tshut_cold);
	}

	enable_continuous_mode(scm_ptr);

	/* Set .250 seconds time as default counter */
	for (i = 0; i < scm_ptr->cnt; i++) {
		configure_temp_sensor_counter(scm_ptr, i,
						scm_ptr->clk_rate / 4);
	}

	return 0;
clk_err:
	kfree(scm_ptr->ts_data);
fail_alloc:
	kfree(scm_ptr->registers);
	return ret;
}

void omap4460plus_temp_sensor_deinit(struct scm *scm_ptr)
{
	kfree(scm_ptr->ts_data);
	kfree(scm_ptr->registers);
}
