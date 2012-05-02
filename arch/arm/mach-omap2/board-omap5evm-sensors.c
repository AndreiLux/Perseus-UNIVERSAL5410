/*
 * arch/arm/mach-omap2/board-omap5evm-sensors.c
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Leed Aguilar <leed.aguilar@ti.com>
 *
 * Derived from: arch/arm/mach-omap2/board-omap5evm.c
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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/i2c/tsl2771.h>
#include <linux/input/mpu6050.h>

#include <plat/i2c.h>

#include "board-omap5evm.h"

#define OMAP5_TSL2771_INT_GPIO		149
#define OMAP5_MPU6050_INT_GPIO		150

/*
 * MPU6050 motion processing unit data structure
 * Acceleremoter and Gyro sensor attributes
 */
static struct mpu6050_platform_data mpu6050_platform_data = {
	.aux_i2c_supply = 0,
	.sample_rate_div = 0,
	.config = 0,
	.fifo_mode = 0,
	.mpu6050_accel = {
		.x_axis = 2,
		.y_axis = 2,
		.z_axis = 2,
		.fsr = MPU6050_RANGE_2G,
		.hpf = 4,               /* HPF ON and cut off 0.63HZ */
		.ctrl_mode = 2,         /* ZERO MOTION DETECTION */
		.mode_thr_val = 0,      /* Threshold val */
		.mode_thr_dur = 0,      /* Threshold duration */
		.irqflags = IRQF_TRIGGER_HIGH,
	},
	.mpu6050_gyro = {
		.x_axis = 2,
		.y_axis = 2,
		.z_axis = 2,
		.fsr = 0,
		.config = 0,
	},
};

/*
 * TSL2771 optical sensor data structure
 * Ambient light and proximity sensor attributes
 */
struct tsl2771_platform_data tsl2771_data = {
	.irq_flags      = (IRQF_TRIGGER_LOW | IRQF_ONESHOT),
	.flags          = (TSL2771_USE_ALS | TSL2771_USE_PROX),
	.def_enable                     = 0x0,
	.als_adc_time                   = 0xdb,
	.prox_adc_time                  = 0xff,
	.wait_time                      = 0x00,
	.als_low_thresh_low_byte        = 0x0,
	.als_low_thresh_high_byte       = 0x0,
	.als_high_thresh_low_byte       = 0x0,
	.als_high_thresh_high_byte      = 0x0,
	.prox_low_thresh_low_byte       = 0x0,
	.prox_low_thresh_high_byte      = 0x0,
	.prox_high_thresh_low_byte      = 0x0,
	.prox_high_thresh_high_byte     = 0x0,
	.interrupt_persistence          = 0xf6,
	.config                         = 0x00,
	.prox_pulse_count               = 0x03,
	.gain_control                   = 0xE0,
	.glass_attn                     = 0x01,
	.device_factor                  = 0x34,
};

/*
 * I2C2 sensor board info for pressure sensor (bmp085),
 * optical sensor (tsl2771), motion processing sensor (mpu6050)
 */
static struct i2c_board_info __initdata omap5evm_sensor_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp085", 0x77),
	},
	{
		I2C_BOARD_INFO("tsl2771", 0x39),
		.platform_data = &tsl2771_data,
		.irq = OMAP5_TSL2771_INT_GPIO,
	},
	{
		I2C_BOARD_INFO("mpu6050", 0x68),
		.platform_data = &mpu6050_platform_data,
		.irq = OMAP5_MPU6050_INT_GPIO,
	},
};

/*
 * I2C4 sensor board info for temperature sensor (tmp102)
 */
static struct i2c_board_info __initdata omap5evm_sensor_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("tmp102", 0x48),
	},
};

/*
 * Initialize and register all the available sensors
 */
int __init omap5evm_sensor_init(void)
{
	i2c_register_board_info(2, omap5evm_sensor_i2c2_boardinfo,
		ARRAY_SIZE(omap5evm_sensor_i2c2_boardinfo));

	i2c_register_board_info(4, omap5evm_sensor_i2c4_boardinfo,
		ARRAY_SIZE(omap5evm_sensor_i2c4_boardinfo));

	return 0;
}
