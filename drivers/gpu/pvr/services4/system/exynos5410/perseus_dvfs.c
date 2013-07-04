/*
 * perseus_dvfs.c - Exynos 5410 SGX544MP3 DVFS driver
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: June 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sysfs_helpers.h>
#include <plat/cpu.h>
#include <mach/asv-exynos.h>

#include "services_headers.h"
#include "sysinfo.h"
#include "sec_dvfs.h"
#include "sec_control_pwr_clk.h"
#include "sec_clock.h"

#define MAX_DVFS_LEVEL			10
#define BASE_START_LEVEL		0
#define BASE_UP_STEP_LEVEL		1
#define BASE_DOWN_STEP_LEVEL		1
#define BASE_WAKE_UP_LEVEL		3
#define DOWN_REQUIREMENT_THRESHOLD	3
#define GPU_DVFS_MAX_LEVEL		8
#define G3D_MAX_VOLT			1150000

#define setmask(a, b) (((1 < a) < 24)|b)
#define getclockmask(a) ((a | 0xFF000000) > 24)
#define getlevelmask(a) (a | 0xFFFFFF)

struct gpu_dvfs_data {
	int level;
	int clock;
	int voltage;
	int clock_source;
	int min_threadhold;
	int max_threadhold;
	int quick_down_threadhold;
	int quick_up_threadhold;
	int stay_total_count;
	int mask;
	int etc;
};

/* start define DVFS info */
static struct gpu_dvfs_data default_dvfs_data[] = {
/* level, clock, voltage, src clk, min, max, qmin, qmax, stay, mask, etc */
	{ 0,    700, 1250000,     700, 210, 256,   200, 256, 1, 0, 0 },
	{ 1,    640, 1225000,     640, 200, 240,   190, 250, 1, 0, 0 },
	{ 2,    600, 1200000,     600, 190, 240,   180, 250, 1, 0, 0 },
	{ 3,    532, 1150000,     532, 180, 240,   170, 250, 1, 0, 0 },
	{ 4,    480, 1100000,     480, 170, 200,   160, 250, 2, 0, 0 },
	{ 5,    350,  925000,     350, 160, 190,   150, 250, 3, 0, 0 },
	{ 6,    266,  900000,     266, 150, 200,   140, 250, 3, 0, 0 },
	{ 7,    177,  900000,     177,   0, 200,     0, 220, 3, 0, 0 },
};

/* end define DVFS info */
struct gpu_dvfs_data gdata[MAX_DVFS_LEVEL];

int sgx_dvfs_level = -1;
/* this value is dvfs mode- 0: auto, others: custom lock */
int sgx_dvfs_custom_clock;
int sgx_dvfs_min_lock;
int sgx_dvfs_max_lock;
int sgx_dvfs_down_requirement;

int custom_min_lock_level;
int custom_max_lock_level;

char sgx_dvfs_table_string[256]={0};
char* sgx_dvfs_table;

/* end sys parameters */

int sec_gpu_dvfs_level_from_clk_get(int clock)
{
	int i;

	for (i = 0; i < GPU_DVFS_MAX_LEVEL; i++) {
		/* This is necessary because the intent
		 * is the difference of kernel clock value
		 * and sgx clock table value to calibrate it */
		if ((gdata[i].clock / 10) == (clock / 10))
			return i;
	}

	return -1;
}

static int sec_gpu_lock_control_proc(int bmax, long value, size_t count)
{
	int lock_level = sec_gpu_dvfs_level_from_clk_get(value);
	int retval = -EINVAL;

	sgx_dvfs_level = sec_gpu_dvfs_level_from_clk_get(gpu_clock_get());
	if (lock_level < 0) { /* unlock something */
		if (bmax)
			sgx_dvfs_max_lock = custom_max_lock_level = 0;
		else
			sgx_dvfs_min_lock = custom_min_lock_level = 0;

		if (sgx_dvfs_min_lock && (sgx_dvfs_level > custom_min_lock_level)) /* min lock only - likely */
			sec_gpu_vol_clk_change(gdata[custom_min_lock_level].clock, gdata[custom_min_lock_level].voltage);
		else if (sgx_dvfs_max_lock && (sgx_dvfs_level < custom_max_lock_level)) /* max lock only - unlikely */
			sec_gpu_vol_clk_change(gdata[custom_max_lock_level].clock, gdata[custom_max_lock_level].voltage);

		if (value == 0)
			retval = count;
	} else{ /* lock something */
		if (bmax) {
			sgx_dvfs_max_lock = value;
			custom_max_lock_level = lock_level;
		} else {
			sgx_dvfs_min_lock = value;
			custom_min_lock_level = lock_level;
		}

        if ((sgx_dvfs_max_lock) && (sgx_dvfs_min_lock) && (sgx_dvfs_max_lock < sgx_dvfs_min_lock)){ /* abnormal status */
			if (sgx_dvfs_max_lock) /* max lock */
				sec_gpu_vol_clk_change(gdata[custom_max_lock_level].clock, gdata[custom_max_lock_level].voltage);
		} else { /* normal status */
			if ((bmax) && sgx_dvfs_max_lock && (sgx_dvfs_level < custom_max_lock_level)) /* max lock */
				sec_gpu_vol_clk_change(gdata[custom_max_lock_level].clock, gdata[custom_max_lock_level].voltage);
			if ((!bmax) && sgx_dvfs_min_lock && (sgx_dvfs_level > custom_min_lock_level)) /* min lock */
				sec_gpu_vol_clk_change(gdata[custom_min_lock_level].clock, gdata[custom_min_lock_level].voltage);
		}
		retval = count;
	}
	return retval;
}

static ssize_t get_dvfs_table(struct device *d, struct device_attribute *a, char *buf)
{
	return sprintf(buf, "%s\n", sgx_dvfs_table);
}
static DEVICE_ATTR(sgx_dvfs_table, S_IRUGO | S_IRGRP | S_IROTH, get_dvfs_table, 0);

static ssize_t get_min_clock(struct device *d, struct device_attribute *a, char *buf)
{
	PVR_LOG(("get_min_clock: %d MHz", sgx_dvfs_min_lock));
	return sprintf(buf, "%d\n", sgx_dvfs_min_lock);
}

static ssize_t set_min_clock(struct device *d, struct device_attribute *a, const char *buf, size_t count)
{
	long value;
	if (kstrtol(buf, 10, &value) == -EINVAL)
		return -EINVAL;
	return sec_gpu_lock_control_proc(0, value, count);
}
static DEVICE_ATTR(sgx_dvfs_min_lock, S_IRUGO | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, get_min_clock, set_min_clock);

static ssize_t get_max_clock(struct device *d, struct device_attribute *a, char *buf)
{
	PVR_LOG(("get_max_clock: %d MHz", sgx_dvfs_max_lock));
	return sprintf(buf, "%d\n", sgx_dvfs_max_lock);
}

static ssize_t set_max_clock(struct device *d, struct device_attribute *a, const char *buf, size_t count)
{
	long value;
	if (kstrtol(buf, 10, &value) == -EINVAL)
		return -EINVAL;
	return sec_gpu_lock_control_proc(1, value, count);
}
static DEVICE_ATTR(sgx_dvfs_max_lock, S_IRUGO | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, get_max_clock, set_max_clock);

static ssize_t volt_table_show(struct device *d, struct device_attribute *a, char *buf)
{
	int i, len = 0;

	for (i = 0; i < GPU_DVFS_MAX_LEVEL; i++) {
		len += sprintf(buf + len, "%d %d\n", gdata[i].clock, gdata[i].voltage);
	}

	return len;
}

static int sanitize_voltage(int voltage)
{
	int rest = voltage % 6250;

	if (rest)
		voltage += 6250 - rest;

	if (voltage > G3D_MAX_VOLT)
		voltage = G3D_MAX_VOLT;

	if (voltage < 600000)
		voltage = 600000;

	return voltage;
}

static ssize_t volt_table_store(struct device *d, struct device_attribute *a, const char *buf, size_t count)
{
	int i, t;
	int u[GPU_DVFS_MAX_LEVEL];

	if ((t = read_into((int*)&u, GPU_DVFS_MAX_LEVEL, buf, count)) < 0)
		return -EINVAL;

	if ((t == 2) && (GPU_DVFS_MAX_LEVEL != 2)) {
		if ((i = sec_gpu_dvfs_level_from_clk_get(u[0])) < 0)
			return -EINVAL;
		
		gdata[i].voltage = sanitize_voltage(u[1]);
	} else if (t == GPU_DVFS_MAX_LEVEL) {
		for(i = 0; i < GPU_DVFS_MAX_LEVEL; i++)
			gdata[i].voltage = sanitize_voltage(u[i]);
	} else
		return -EINVAL;

	return count;	
}
static DEVICE_ATTR(sgx_dvfs_volt_table, S_IRUGO | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, volt_table_show, volt_table_store);

void sec_gpu_dvfs_init(void)
{
	struct platform_device *pdev;
	int i = 0;
	ssize_t total = 0, offset = 0;

	memset(gdata, 0x00, sizeof(struct gpu_dvfs_data)*MAX_DVFS_LEVEL);

	for (i = 0; i < GPU_DVFS_MAX_LEVEL; i++) {
		gdata[i].level = default_dvfs_data[i].level;
		gdata[i].clock = default_dvfs_data[i].clock;
		gdata[i].voltage = get_match_volt(ID_G3D, default_dvfs_data[i].clock * 1000);
		gdata[i].clock_source = default_dvfs_data[i].clock_source;
		gdata[i].min_threadhold = default_dvfs_data[i].min_threadhold;
		gdata[i].max_threadhold = default_dvfs_data[i].max_threadhold;
		gdata[i].quick_down_threadhold = default_dvfs_data[i].quick_down_threadhold;
		gdata[i].quick_up_threadhold = default_dvfs_data[i].quick_up_threadhold;
		gdata[i].stay_total_count = default_dvfs_data[i].stay_total_count;
		gdata[i].mask  = setmask(default_dvfs_data[i].level, default_dvfs_data[i].clock);
		PVR_LOG(("G3D DVFS Info: Level:%d, Clock:%d MHz, Voltage:%d uV", gdata[i].level, gdata[i].clock, gdata[i].voltage));
	}

	/* default dvfs level depend on default clock setting */
	sgx_dvfs_level = sec_gpu_dvfs_level_from_clk_get(gpu_clock_get());
	sgx_dvfs_down_requirement = DOWN_REQUIREMENT_THRESHOLD;

	pdev = gpsPVRLDMDev;

	/* Required name attribute */
	if (device_create_file(&pdev->dev, &dev_attr_sgx_dvfs_min_lock) < 0)
		PVR_LOG(("device_create_file: dev_attr_sgx_dvfs_min_lock fail"));
	if (device_create_file(&pdev->dev, &dev_attr_sgx_dvfs_max_lock) < 0)
		PVR_LOG(("device_create_file: dev_attr_sgx_dvfs_max_lock fail"));
	if (device_create_file(&pdev->dev, &dev_attr_sgx_dvfs_volt_table) < 0)
		PVR_LOG(("device_create_file: dev_attr_sgx_dvfs_volt_table fail"));

	 /* Generate DVFS table list*/
	for( i = 0; i < GPU_DVFS_MAX_LEVEL ; i++) {
	    offset = sprintf(sgx_dvfs_table_string+total, "%d\n", gdata[i].clock);
	    total += offset;
	}

	sgx_dvfs_table = sgx_dvfs_table_string;
	if (device_create_file(&pdev->dev, &dev_attr_sgx_dvfs_table) < 0)
		PVR_LOG(("device_create_file: dev_attr_sgx_dvfs_table fail"));
}

void sec_gpu_dvfs_down_requirement_reset()
{
	int level;

	level = sec_gpu_dvfs_level_from_clk_get(gpu_clock_get());
	sgx_dvfs_down_requirement = gdata[level].stay_total_count;
}

extern unsigned int *g_debug_CCB_Info_RO;
extern unsigned int *g_debug_CCB_Info_WO;
extern int g_debug_CCB_Info_WCNT;
static int g_debug_CCB_Info_Flag = 0;
static int g_debug_CCB_count = 1;

int sec_clock_change_up(int level, int step)
{
	level -= step;

	if (level < 0)
		level = 0;

	if (sgx_dvfs_max_lock) {
		if (level < custom_max_lock_level)
			level = custom_max_lock_level;
	}

	sgx_dvfs_down_requirement = gdata[level].stay_total_count;
	sec_gpu_vol_clk_change(gdata[level].clock, gdata[level].voltage);
	if ((g_debug_CCB_Info_Flag % g_debug_CCB_count) == 0)
		PVR_LOG(("SGX CCB RO : %d, WO : %d, Total : %d", *g_debug_CCB_Info_RO, *g_debug_CCB_Info_WO, g_debug_CCB_Info_WCNT));

	g_debug_CCB_Info_WCNT = 0;
	g_debug_CCB_Info_Flag ++;

	return level;
}

int sec_clock_change_down(int level, int step)
{
	sgx_dvfs_down_requirement--;
	if (sgx_dvfs_down_requirement > 0 )
		return level;

	level += step;

	if (level > GPU_DVFS_MAX_LEVEL - 1)
		level = GPU_DVFS_MAX_LEVEL - 1;

	if (sgx_dvfs_min_lock) {
		if (level > custom_min_lock_level)
			level = custom_min_lock_level;
	}

	sgx_dvfs_down_requirement = gdata[level].stay_total_count;
	sec_gpu_vol_clk_change(gdata[level].clock, gdata[level].voltage);

	if ((g_debug_CCB_Info_Flag % g_debug_CCB_count) == 0)
		PVR_LOG(("SGX CCB RO : %d, WO : %d, Total : %d", *g_debug_CCB_Info_RO, *g_debug_CCB_Info_WO, g_debug_CCB_Info_WCNT));

	g_debug_CCB_Info_WCNT = 0;
	g_debug_CCB_Info_Flag ++;

	return level;
}

unsigned int g_g3dfreq;
void sec_gpu_dvfs_handler(int utilization_value)
{
	/*utilization_value is zero mean is gpu going to idle*/
	if (utilization_value == 0)
		return;

	sgx_dvfs_level = sec_gpu_dvfs_level_from_clk_get(gpu_clock_get());
	/* this check for current clock must be find in dvfs table */
	if (sgx_dvfs_level < 0) {
		PVR_LOG(("WARN: current clock: %d MHz not found in DVFS table. so set to max clock", gpu_clock_get()));
		sec_gpu_vol_clk_change(gdata[BASE_START_LEVEL].clock, gdata[BASE_START_LEVEL].voltage);
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "INFO: AUTO DVFS [%d MHz] <%d, %d>, utilization [%d]",
			gpu_clock_get(),
			gdata[sgx_dvfs_level].min_threadhold,
			gdata[sgx_dvfs_level].max_threadhold, utilization_value));

	/* check current level's threadhold value */
	if (gdata[sgx_dvfs_level].min_threadhold > utilization_value) {
		/* need to down current clock */
		sgx_dvfs_level = sec_clock_change_down(sgx_dvfs_level, BASE_DOWN_STEP_LEVEL);

	} else if (gdata[sgx_dvfs_level].max_threadhold < utilization_value) {
		/* need to up current clock */
		sgx_dvfs_level = sec_clock_change_up(sgx_dvfs_level, BASE_UP_STEP_LEVEL);
	} else 
		sgx_dvfs_down_requirement = gdata[sgx_dvfs_level].stay_total_count;

	g_g3dfreq = gdata[sgx_dvfs_level].clock;
}
