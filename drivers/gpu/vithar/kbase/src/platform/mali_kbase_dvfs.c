/* drivers/gpu/vithar/kbase/src/platform/mali_kbase_dvfs.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_dvfs.c
 * DVFS
 */

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_uku.h>
#include <kbase/src/common/mali_kbase_mem.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/linux/mali_kbase_mem_linux.h>
#include <uk/mali_ukk.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <mach/map.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <mach/regs-clock.h>
#include <asm/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#include <kbase/src/platform/mali_kbase_dvfs.h>

#ifdef CONFIG_REGULATOR
static struct regulator *g3d_regulator=NULL;
#ifdef CONFIG_VITHAR_HWVER_R0P0
static int mali_gpu_vol = 1250000; /* 1.25V @ 533 MHz */
#else
static int mali_gpu_vol = 1050000; /* 1.05V @ 266 MHz */
#endif
#endif


#ifdef CONFIG_VITHAR_DVFS
typedef struct _mali_dvfs_info{
	unsigned int voltage;
	unsigned int clock;
	int min_threshold;
	int	max_threshold;
}mali_dvfs_info;

typedef struct _mali_dvfs_status_type{
	kbase_device *kbdev;
	int step;
	int utilisation;
	int keepcnt;
	uint noutilcnt;
}mali_dvfs_status;

static struct workqueue_struct *mali_dvfs_wq = 0;
int mali_dvfs_control=0;
osk_spinlock mali_dvfs_spinlock;


/*dvfs status*/
static mali_dvfs_status mali_dvfs_status_current;
static const mali_dvfs_info mali_dvfs_infotbl[MALI_DVFS_STEP]=
{
#if (MALI_DVFS_STEP == 6)
	{937500/*750000*/, 100, 0, 40},
	{937500/*81250*/, 160, 30, 60},
	{937500, 266, 50, 70},
	{1100000, 400, 60, 80},
	{1150000, 450, 70, 90},
	{1250000, 533, 80, 100}
#elif (MALI_DVFS_STEP == 2)
	{937500, 266, 0, 55},
	{1250000, 533, 45, 100}
#else
#error no table
#endif
};

static void kbase_platform_dvfs_set_vol(int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return;


	switch(vol)
	{
	case 1250000:
	case 1150000:
	case 1100000:
	case 1000000:
	case 937500:
	case 812500:
	case 750000:
		kbase_platform_set_voltage(NULL, vol);
		break;
	default:
		return;
	}

	_vol = vol;

#if MALI_DVFS_DEBUG
	printk("dvfs_set_vol %dmV\n", vol);
#endif
	return;
}

static void mali_dvfs_event_proc(struct work_struct *w)
{
	mali_dvfs_status dvfs_status;

	osk_spinlock_lock(&mali_dvfs_spinlock);
	dvfs_status = mali_dvfs_status_current;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

#if MALI_DVFS_START_MAX_STEP
	/*If no input is for longtime, first step will be max step. */
	if (dvfs_status.utilisation > 10 && dvfs_status.noutilcnt > 20) {
		dvfs_status.step=MALI_DVFS_STEP-2;
		dvfs_status.utilisation = 100;
	}
#endif

	if (dvfs_status.utilisation > mali_dvfs_infotbl[dvfs_status.step].max_threshold)
	{
		OSK_ASSERT(dvfs_status.step < MALI_DVFS_STEP);
		dvfs_status.step++;
		dvfs_status.keepcnt=0;
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[dvfs_status.step].voltage);
		kbase_platform_dvfs_set_clock(mali_dvfs_status_current.kbdev, mali_dvfs_infotbl[dvfs_status.step].clock);
	}else if ((dvfs_status.step>0) &&
			(dvfs_status.utilisation < mali_dvfs_infotbl[dvfs_status.step].min_threshold)) {
		dvfs_status.keepcnt++;
		if (dvfs_status.keepcnt > MALI_DVFS_KEEP_STAY_CNT)
		{
			OSK_ASSERT(dvfs_status.step > 0);
			dvfs_status.step--;
			dvfs_status.keepcnt=0;
			kbase_platform_dvfs_set_clock(mali_dvfs_status_current.kbdev, mali_dvfs_infotbl[dvfs_status.step].clock);
			kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[dvfs_status.step].voltage);
		}
	}else{
		dvfs_status.keepcnt=0;
	}

#if MALI_DVFS_START_MAX_STEP
	if (dvfs_status.utilisation == 0) {
		dvfs_status.noutilcnt++;
	} else {
		dvfs_status.noutilcnt=0;
	}
#endif

#if MALI_DVFS_DEBUG
	printk("[mali_dvfs] utilisation: %d  step: %d[%d,%d] cnt: %d\n",
			dvfs_status.utilisation, dvfs_status.step,
			mali_dvfs_infotbl[dvfs_status.step].min_threshold,
			mali_dvfs_infotbl[dvfs_status.step].max_threshold, dvfs_status.keepcnt);
#endif

	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current=dvfs_status;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

}

static DECLARE_WORK(mali_dvfs_work, mali_dvfs_event_proc);

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.utilisation = utilisation;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	/*add error handle here*/
	return MALI_TRUE;
}

int kbase_platform_dvfs_get_control_status(void)
{
	return mali_dvfs_control;
}

int kbase_platform_dvfs_init(struct device *dev)
{
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	/*default status
	add here with the right function to get initilization value.
	*/
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

	osk_spinlock_init(&mali_dvfs_spinlock,OSK_LOCK_ORDER_PM_METRICS);

	/*add a error handling here*/
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.kbdev = kbdev;
	mali_dvfs_status_current.utilisation = 100;
	mali_dvfs_status_current.step = MALI_DVFS_STEP-1;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

	return MALI_TRUE;
}

void kbase_platform_dvfs_term(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}
#endif


int kbase_platform_regulator_init(struct device *dev)
{

#ifdef CONFIG_REGULATOR
    g3d_regulator = regulator_get(NULL, "vdd_g3d");
    if(IS_ERR(g3d_regulator))
    {
        printk("[kbase_platform_regulator_init] failed to get vithar regulator\n");
	return -1;
    }

    if(regulator_enable(g3d_regulator) != 0)
    {
        printk("[kbase_platform_regulator_init] failed to enable vithar regulator\n");
	return -1;
    }

    if(regulator_set_voltage(g3d_regulator, mali_gpu_vol, mali_gpu_vol) != 0)
    {
        printk("[kbase_platform_regulator_init] failed to set vithar operating voltage [%d]\n", mali_gpu_vol);
	return -1;
    }
#endif

    return 0;
}

int kbase_platform_regulator_disable(struct device *dev)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_regulator_disable] g3d_regulator is not initialized\n");
	return -1;
    }

    if(regulator_disable(g3d_regulator) != 0)
    {
        printk("[kbase_platform_regulator_disable] failed to disable g3d regulator\n");
	return -1;
    }
#endif
    return 0;
}

int kbase_platform_regulator_enable(struct device *dev)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_regulator_enable] g3d_regulator is not initialized\n");
	return -1;
    }

    if(regulator_enable(g3d_regulator) != 0)
    {
        printk("[kbase_platform_regulator_enable] failed to enable g3d regulator\n");
	return -1;
    }
#endif
    return 0;
}

int kbase_platform_get_default_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
	*vol = mali_gpu_vol;
#else
	*vol = 0;
#endif
	return 0;
}

int kbase_platform_get_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_get_voltage] g3d_regulator is not initialized\n");
	return -1;
    }

    *vol = regulator_get_voltage(g3d_regulator);
#else
    *vol = 0;
#endif
    return 0;
}

int kbase_platform_set_voltage(struct device *dev, int vol)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_set_voltage] g3d_regulator is not initialized\n");
	return -1;
    }

    if(regulator_set_voltage(g3d_regulator, vol, vol) != 0)
    {
        printk("[kbase_platform_set_voltage] failed to set voltage\n");
	return -1;
    }
#endif
    return 0;
}

void kbase_platform_dvfs_set_clock(kbase_device *kbdev, int freq)
{
	static struct clk * mout_gpll = NULL;
	static struct clk * fin_gpll = NULL;
	static struct clk * fout_gpll = NULL;
	static int _freq = -1;
	static unsigned long gpll_rate_prev = 0;
	unsigned long gpll_rate = 0, aclk_400_rate = 0;
	unsigned long tmp = 0;

	if (!kbdev)
		panic("oops");

	if (mout_gpll==NULL) {
		mout_gpll = clk_get(kbdev->osdev.dev, "mout_gpll");
		fin_gpll = clk_get(kbdev->osdev.dev, "ext_xtal");
		fout_gpll = clk_get(kbdev->osdev.dev, "fout_gpll");
		if(IS_ERR(mout_gpll) || IS_ERR(fin_gpll) || IS_ERR(fout_gpll))
			panic("clk_get ERROR");
	}

	if(kbdev->sclk_g3d == 0)
		return;

	if (freq == _freq)
		return;

	switch(freq)
	{
	case 533:
		gpll_rate = 533000000;
		aclk_400_rate = 533000000;
		break;
	case 450:
		gpll_rate = 450000000;
		aclk_400_rate = 450000000;
		break;
	case 400:
		gpll_rate = 800000000;
		aclk_400_rate = 400000000;
		break;
	case 266:
		gpll_rate = 800000000;
		aclk_400_rate = 267000000;
		break;
	case 160:
		gpll_rate = 800000000;
		aclk_400_rate = 160000000;
		break;
	case 100:
		gpll_rate = 800000000;
		aclk_400_rate = 100000000;
		break;
	default:
		return;
	}

	/* if changed the GPLL rate, set rate for GPLL and wait for lock time */
	if( gpll_rate != gpll_rate_prev) {
		/*for stable clock input.*/
		clk_set_rate(kbdev->sclk_g3d, 100000000);
		clk_set_parent(mout_gpll, fin_gpll);

		/*change gpll*/
		clk_set_rate( fout_gpll, gpll_rate );

		/*restore parent*/
		clk_set_parent(mout_gpll, fout_gpll);
		gpll_rate_prev = gpll_rate;
	}

	_freq = freq;
	clk_set_rate(kbdev->sclk_g3d, aclk_400_rate);

	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(EXYNOS5_CLKDIV_STAT_TOP0);
	} while (tmp & 0x1000000);

#if MALI_DVFS_DEBUG
	printk("aclk400 %u[%d]\n", (unsigned int)clk_get_rate(kbdev->sclk_g3d),mali_dvfs_status_current.utilisation);
	printk("dvfs_set_clock GPLL : %d, ACLK_400 : %dMhz\n", freq);
#endif
	return;
}

