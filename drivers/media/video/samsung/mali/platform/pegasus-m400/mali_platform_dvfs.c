/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform_dvfs.c
 * Platform specific Mali driver dvfs functions
 */

#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#include <linux/sysdev.h>
#include <linux/platform_device.h>

#include <linux/sysfs_helpers.h>

#include <asm/io.h>

#include "mali_device_pause_resume.h"
#include <linux/workqueue.h>

#ifdef CONFIG_CPU_FREQ
#include <mach/asv.h>
#define EXYNOS4_ASV_ENABLED
#endif

#define MALI_VOLTAGE_STEP	12500
#define MALI_MAX_VOLTAGE	1200000
#define MALI_MIN_VOLTAGE	600000

static int bMaliDvfsRun=0;

typedef struct mali_dvfs_tableTag{
	unsigned int clock;
	unsigned int freq;
	unsigned int vol;
}mali_dvfs_table;

mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS]={
	{800   ,1000000   , 1200000}, // L0
	{733   ,1000000   , 1175000}, // L1
	{640   ,1000000   , 1125000}, // L2
	{533   ,1000000   , 1075000}, // L3
	{440   ,1000000   , 1025000}, // L4
	{350   ,1000000   ,  950000}, // L5
	{266   ,1000000   ,  900000}, // L6
	{160   ,1000000   ,  875000}, // L7
	{108   ,1000000   ,  850000}, // L8
	{54    ,1000000   ,  825000}, // L9
};

int findStep(int clock) {
	int i;
	int ret = MALI_DVFS_STEPS;

	for(i = 0; i < MALI_DVFS_STEPS; i++){
		if(mali_dvfs[i].clock > clock) continue;
		ret = i;
		if(mali_dvfs[i].clock == clock) break;
	}

	return ret;
}

struct mali_policy_config{
	unsigned int lowStep;
	unsigned int highStep;
	unsigned int currentStep;
	unsigned int upThreshold;
	unsigned int downDifferential;
}mali_policy = {
	.upThreshold = 90,
	.downDifferential = 10,
};


#ifdef EXYNOS4_ASV_ENABLED
#define ASV_LEVEL_PRIME     13	/* ASV0, 1, 12 is reserved */
static unsigned int asv_3d_volt_9_table[MALI_DVFS_STEPS][ASV_LEVEL_PRIME] = {
	{ 1150000, 1137500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1075000, 1062500, 1037500, 1025000, 1012500},	/* L3(533Mhz) */
	{ 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000, 1012500, 1000000,  975000,  962500,  950000},	/* L4(440Mhz) */
	{ 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  950000,  937500,  912500,  900000,  887500},	/* L5(350Mhz) */
	{  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  900000,  887500,  875000,  875000,  875000},	/* L6(266Mhz) */
	{  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  875000,  862500,  850000,  850000,  850000}, /* L7(160Mhz) */
};
#endif /* ASV_LEVEL */

int mali_dvfs_control=0;

static u32 mali_dvfs_utilization = 255;

static void mali_dvfs_work_handler(struct work_struct *w);

static struct workqueue_struct *mali_dvfs_wq = 0;
extern mali_io_address clk_register_map;
extern _mali_osk_lock_t *mali_dvfs_lock;

static u64 time_in_state[MALI_DVFS_STEPS];
static struct timeval last_switch;
static int time_enable = 0;

static DECLARE_WORK(mali_dvfs_work, mali_dvfs_work_handler);

#if MALI_PMM_RUNTIME_JOB_CONTROL_ON
int get_mali_dvfs_control_status(void)
{
	return mali_dvfs_control;
}

static void do_time_slice(int level)
{
	u64 delta;
	struct timeval now;	
	
	do_gettimeofday(&now);
	
	delta = (now.tv_sec - last_switch.tv_sec) * USEC_PER_SEC +
		    (now.tv_usec - last_switch.tv_usec);

	time_in_state[level] += delta;

	last_switch = now;
}

mali_bool set_mali_dvfs_current_step(unsigned int step)
{
	_mali_osk_lock_wait(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);
	if(!!time_enable)
		do_time_slice(mali_policy.currentStep);
	mali_policy.currentStep = step;
	_mali_osk_lock_signal(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);
	return MALI_TRUE;
}
#endif
static mali_bool set_mali_dvfs_status(u32 step,mali_bool boostup)
{
	u32 validatedStep=step;

#ifdef CONFIG_REGULATOR
	if (mali_regulator_get_usecount() == 0) {
		MALI_DEBUG_PRINT(1, ("regulator use_count is 0 \n"));
		return MALI_FALSE;
	}
#endif

	if (boostup) {
#ifdef CONFIG_REGULATOR
		/*change the voltage*/
		mali_regulator_set_voltage(mali_dvfs[step].vol, mali_dvfs[step].vol);
#endif
		/*change the clock*/
		mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
	} else {
		/*change the clock*/
		mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
#ifdef CONFIG_REGULATOR
		/*change the voltage*/
		mali_regulator_set_voltage(mali_dvfs[step].vol, mali_dvfs[step].vol);
#endif
	}

#ifdef EXYNOS4_ASV_ENABLED
	if (samsung_rev() < EXYNOS4412_REV_2_0) {
		if (mali_dvfs[step].clock <= 160)
			exynos4x12_set_abb_member(ABB_G3D, ABB_MODE_100V);
		else
			exynos4x12_set_abb_member(ABB_G3D, ABB_MODE_130V);
	}
#endif

	set_mali_dvfs_current_step(validatedStep);
	/*for future use*/
	mali_policy.currentStep = validatedStep;

	return MALI_TRUE;
}

static mali_bool change_mali_dvfs_status(u32 step, mali_bool boostup )
{
	unsigned int read_val;

	MALI_DEBUG_PRINT(1, ("> change_mali_dvfs_status: %d, %d \n",step, boostup));

	if (!set_mali_dvfs_status(step, boostup)) {
		MALI_DEBUG_PRINT(1, ("error on set_mali_dvfs_status: %d, %d \n",step, boostup));
		return MALI_FALSE;
	}

	/*wait until clock and voltage is stablized*/
	while(1) {
		read_val = _mali_osk_mem_ioread32(clk_register_map, 0x00);
		if ((read_val & 0x8000)==0x0000) break;
			_mali_osk_time_ubusydelay(25);
	}

	return MALI_TRUE;
}

#ifdef EXYNOS4_ASV_ENABLED
extern unsigned int exynos_result_of_asv;

static mali_bool mali_dvfs_table_update(void)
{
	unsigned int i;

	for (i = 3; i < 8; i++) {
		MALI_PRINT((":::exynos_result_of_asv : %d\n", exynos_result_of_asv));

		mali_dvfs[i].vol = asv_3d_volt_9_table[i-3][exynos_result_of_asv];
/*
		if (samsung_rev() >= EXYNOS4412_REV_2_0 && ((is_special_flag() >> G3D_LOCK_FLAG) & 0x1)) {
			mali_dvfs[i].vol += 25000;
		}
*/
		MALI_PRINT(("mali_dvfs[%d].vol = %d\n", i, mali_dvfs[i].vol));
	}

	return MALI_TRUE;
}
#endif

static mali_bool mali_dvfs_status(u32 utilization)
{
	unsigned int nextStep;
	unsigned int level;
	unsigned int target_freq;
	int i;

	mali_bool boostup = MALI_FALSE;
	struct mali_policy_config mp;

	level = 0; // Step delta
	mp = mali_policy;

#ifdef EXYNOS4_ASV_ENABLED
	static mali_bool asv_applied = MALI_FALSE;

	if (asv_applied == MALI_FALSE) {
		mali_dvfs_table_update();
		change_mali_dvfs_status(mp.lowStep, 0);
		asv_applied = MALI_TRUE;

		return MALI_TRUE;
	}
#endif

	if (utilization > (int)(255 * mp.upThreshold / 100) &&
	   (mp.currentStep > mp.highStep)) {
		--level;
	}

	if (utilization < (int)(255 * (mp.upThreshold - mp.downDifferential) / 100) &&
	   (mp.currentStep < mp.lowStep)) {
		target_freq = (mali_dvfs[mp.currentStep].clock * utilization) / 255;
		for(i = mp.currentStep; i < mp.lowStep; i++) {
			if(mali_dvfs[i].clock < target_freq) break;
			++level;
		}
	}

	/*if we don't have a level change, don't do anything*/
	if (level != 0) {
		nextStep = mp.currentStep + level;

		/*change mali dvfs status*/
		if (!change_mali_dvfs_status(nextStep,level ? MALI_FALSE : MALI_TRUE)) {
			MALI_DEBUG_PRINT(1, ("error on change_mali_dvfs_status \n"));
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

int mali_dvfs_is_running(void)
{
	return bMaliDvfsRun;

}

void mali_dvfs_late_resume(void)
{
	// set the init clock as low when resume
	set_mali_dvfs_status(mali_policy.lowStep,0);
}

static void mali_dvfs_work_handler(struct work_struct *w)
{
	bMaliDvfsRun=1;

	if (!mali_dvfs_status(mali_dvfs_utilization))
		MALI_DEBUG_PRINT(1,( "error on mali dvfs status in mali_dvfs_work_handler"));

	bMaliDvfsRun=0;
}

void deinit_mali_dvfs_status(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}

mali_bool mali_dvfs_handler(u32 utilization)
{
	mali_dvfs_utilization = utilization;
	queue_work_on(0, mali_dvfs_wq,&mali_dvfs_work);

	/*add error handle here*/
	return MALI_TRUE;
}

/* #### Sysfs start #### */

static int sanitize_voltage(int voltage)
{
	voltage *= 1000;
	
	if(voltage % MALI_VOLTAGE_STEP)
		voltage += voltage % MALI_VOLTAGE_STEP;

	if(voltage > MALI_MAX_VOLTAGE)
		voltage = MALI_MAX_VOLTAGE;

	if(voltage < MALI_MIN_VOLTAGE)
		voltage = MALI_MIN_VOLTAGE;

	return voltage;
}

static ssize_t max_freq_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mali_dvfs[mali_policy.highStep].clock);
}

static ssize_t max_freq_store(struct sysdev_class * cls, struct sysdev_class_attribute *attr,
			      const char *buf, size_t count) 
{
	unsigned int ret = -EINVAL;
	int freq, i, s;

	ret = sscanf(buf, "%d", &freq);
	if (ret != 1) {
		return -EINVAL;
	} else {
		for(i = 0; i < MALI_DVFS_STEPS; i++){
			if(mali_dvfs[i].clock != freq) continue;

			s = findStep(freq);
			if(s > mali_policy.lowStep)
				s = mali_policy.lowStep;

			mali_policy.highStep = s;

			if(mali_policy.currentStep > mali_policy.highStep)
				change_mali_dvfs_status(mali_policy.highStep, MALI_FALSE);
			
			break;
		}
	}
	return count;	
}

static ssize_t min_freq_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mali_dvfs[mali_policy.lowStep].clock);
}

static ssize_t min_freq_store(struct sysdev_class * cls, struct sysdev_class_attribute *attr,
			      const char *buf, size_t count) 
{
	unsigned int ret = -EINVAL;
	int freq, i, s;

	ret = sscanf(buf, "%d", &freq);
	if (ret != 1) {
		return -EINVAL;
	} else {
		for(i = 0; i < MALI_DVFS_STEPS; i++){
			if(mali_dvfs[i].clock != freq) continue;

			s = findStep(freq);
			if(s < mali_policy.highStep)
				s = mali_policy.highStep;

			mali_policy.lowStep = s;

			if(mali_policy.currentStep < mali_policy.lowStep)
				change_mali_dvfs_status(mali_policy.lowStep, MALI_TRUE);
			break;
		}
	}
	return count;	
}

static ssize_t freq_table_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	int i, len = 0;

	for (i = MALI_DVFS_STEPS - 1; i >= 0; i--) {
		len += sprintf(buf + len, "%d\n", mali_dvfs[i].clock);
	}

	return len;
}

static ssize_t freq_table_store(struct sysdev_class * cls, struct sysdev_class_attribute *attr,
			      const char *buf, size_t count) 
{
	int i, t;
	int u[MALI_DVFS_STEPS];
	
	if((t = read_into((int*)&u, MALI_DVFS_STEPS, buf, count)) < 0)
		return -EINVAL;
	if(t == 2 && MALI_DVFS_STEPS != 2){
		mali_dvfs[findStep(u[0])].clock = u[1];
	} else if (t == MALI_DVFS_STEPS) {
		for(i = 0; i < MALI_DVFS_STEPS; i++) {
			mali_dvfs[MALI_DVFS_STEPS - i - 1].clock = u[i];
		}
	} else
		return -EINVAL;

	return count;	
}

static ssize_t volt_table_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	int i, len = 0;

	for (i = MALI_DVFS_STEPS - 1; i >= 0; i--) {
		len += sprintf(buf + len, "%dMHz %dmV\n", mali_dvfs[i].clock,
		      ((mali_dvfs[i].vol % 1000) + mali_dvfs[i].vol)/1000);
	}

	return len;
}

static ssize_t volt_table_store(struct sysdev_class * cls, struct sysdev_class_attribute *attr,
			      const char *buf, size_t count) 
{
	int i, t;
	int u[MALI_DVFS_STEPS];

	if((t = read_into((int*)&u, MALI_DVFS_STEPS, buf, count)) < 0)
		return -EINVAL;
	if(t == 2 && MALI_DVFS_STEPS != 2) {
		mali_dvfs[findStep(u[0])].vol = sanitize_voltage(u[1]);
	} else if (t == MALI_DVFS_STEPS) {
		for(i = 0; i < MALI_DVFS_STEPS; i++) {
			mali_dvfs[MALI_DVFS_STEPS - i - 1].vol = sanitize_voltage(u[i]);
		}
	} else
		return -EINVAL;

	return count;	
}

static ssize_t thresholds_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	return sprintf(buf, "Up: %d down-differential: %d\n",
		       mali_policy.upThreshold, mali_policy.downDifferential);
}

static ssize_t thresholds_store(struct sysdev_class * cls, struct sysdev_class_attribute *attr,
			      const char *buf, size_t count) 
{
	unsigned int ret = -EINVAL;
	int up, down;

	ret = sscanf(buf, "%d %d", &up, &down);
	if (ret != 2) {
		return -EINVAL;
	} else {
		if(up > 0 && up < 100 && down > 0 && down < (99 - up)) {
			mali_policy.upThreshold = up;
			mali_policy.downDifferential = down;
		}
	}
	return count;	
}

static ssize_t current_freq_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mali_dvfs[mali_policy.currentStep].clock);
}

static ssize_t time_in_state_show(struct sysdev_class * cls, 
			     struct sysdev_class_attribute *attr, char *buf)
{
	int i, len = 0;

	for (i = MALI_DVFS_STEPS - 1; i >= 0; i--) {
		len += sprintf(buf + len, "%llu\t%d\n", time_in_state[i], mali_dvfs[i].clock);
	}

	return len;
}

static ssize_t time_in_state_store(struct sysdev_class * cls, struct sysdev_class_attribute *attr,
			      const char *buf, size_t count) 
{
	unsigned int ret = -EINVAL;
	int i, in;

	ret = sscanf(buf, "%d", &in);
	if (ret != 1) {
		return -EINVAL;
	} else {
		switch(in) {
			case 1: time_enable = 1; break;
	  		case 0: time_enable = 0; break;
	  		default: for (i = 0; i < MALI_DVFS_STEPS; i++)
					time_in_state[i] = 0;
		}
	}
	return count;
}

struct sysdev_class gpu_m400_sysclass = {
	.name	= "gpu",
};

static SYSDEV_CLASS_ATTR(max_freq, S_IRUGO | S_IWUGO, max_freq_show, max_freq_store);
static SYSDEV_CLASS_ATTR(min_freq, S_IRUGO | S_IWUGO, min_freq_show, min_freq_store);
static SYSDEV_CLASS_ATTR(freq_table, S_IRUGO | S_IWUGO, freq_table_show, freq_table_store);
static SYSDEV_CLASS_ATTR(volt_table, S_IRUGO | S_IWUGO, volt_table_show, volt_table_store);
static SYSDEV_CLASS_ATTR(thresholds, S_IRUGO | S_IWUGO, thresholds_show, thresholds_store);
static SYSDEV_CLASS_ATTR(current_freq, S_IRUGO, current_freq_show, NULL);
static SYSDEV_CLASS_ATTR(time_in_state, S_IRUGO | S_IWUGO, time_in_state_show, time_in_state_store);

static struct sysdev_class_attribute *gpu_attributes[] = {
	&attr_max_freq,
	&attr_min_freq,
	&attr_freq_table,
	&attr_volt_table,
	&attr_thresholds,
	&attr_current_freq,
	&attr_time_in_state,
};

mali_bool init_mali_dvfs_status(int step)
{
	int i = 0;

	/*init sysinterfaces*/
	sysdev_class_register(&gpu_m400_sysclass);
	for (i = 0;  i < ARRAY_SIZE(gpu_attributes); i++) {
		sysdev_class_create_file(&gpu_m400_sysclass, gpu_attributes[i]);
	}

	for (i = 0; i < MALI_DVFS_STEPS; i++)
		time_in_state[i] = 0;

	do_gettimeofday(&last_switch);

	/*default status
	add here with the right function to get initilization value.
	*/
	mali_policy.lowStep = findStep(160);
	mali_policy.highStep = (samsung_rev() >= EXYNOS4412_REV_2_0) ? findStep(533) : findStep(440);
	
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

	/*add a error handling here*/
	set_mali_dvfs_current_step(step);

	return MALI_TRUE;
}
