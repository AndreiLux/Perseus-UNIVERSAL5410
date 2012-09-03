/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - INT clock frequency scaling support in DEVFREQ framework
 *	This version supports EXYNOS5250 only. This changes bus frequencies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/opp.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/pm_qos.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/asv-exynos.h>

#include "exynos_ppmu.h"
#include "exynos5_ppmu.h"
#include "governor.h"

#define MAX_SAFEVOLT	1050000 /* 1.05V */

/* Assume that the bus for mif is saturated if the utilization is 23% */
#define INT_BUS_SATURATION_RATIO	23
#define EXYNOS5_BUS_INT_POLL_TIME	msecs_to_jiffies(100)

enum int_level_idx {
	LV_0,
	LV_1,
	LV_2,
	_LV_END
};

struct busfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;
	bool disabled;
	struct regulator *vdd_int;
	struct opp *curr_opp;

	struct notifier_block pm_notifier;
	struct mutex lock;
	struct pm_qos_request mif_req;

	struct clk *int_clk;

	struct exynos5_ppmu_handle *ppmu;
	struct delayed_work work;
	int busy;
};

struct int_bus_opp_table {
	unsigned int idx;
	unsigned long clk;
	unsigned long volt;
};

static struct int_bus_opp_table exynos5_int_opp_table[] = {
	{LV_0, 266000, 1050000},
	{LV_1, 200000, 1050000},
	{LV_2, 160000, 1050000},
	{0, 0, 0},
};

static int exynos5_int_setvolt(struct busfreq_data_int *data, struct opp *opp)
{
	unsigned long volt;

	rcu_read_lock();
	volt = opp_get_voltage(opp);
	rcu_read_unlock();

	return regulator_set_voltage(data->vdd_int, volt, MAX_SAFEVOLT);
}

static int exynos5_busfreq_int_target(struct device *dev, unsigned long *_freq,
			      u32 flags)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);
	struct opp *opp;
	unsigned long old_freq, freq;
	unsigned long volt;

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		return PTR_ERR(opp);
	}

	freq = opp_get_freq(opp);
	volt = opp_get_voltage(opp);
	rcu_read_unlock();

	rcu_read_lock();
	old_freq = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	if (old_freq == freq)
		return 0;

	dev_dbg(dev, "targetting %lukHz %luuV\n", freq, volt);

	mutex_lock(&data->lock);

	if (data->disabled)
		goto out;

	if (freq > exynos5_int_opp_table[_LV_END - 1].clk)
		pm_qos_update_request(&data->mif_req,
				data->busy * old_freq * 16 / 100000);
	else
		pm_qos_update_request(&data->mif_req, -1);

	if (old_freq < freq)
		err = exynos5_int_setvolt(data, opp);
	if (err)
		goto out;

	err = clk_set_rate(data->int_clk, freq * 1000);

	if (err)
		goto out;

	if (old_freq > freq)
		err = exynos5_int_setvolt(data, opp);
	if (err)
		goto out;

	data->curr_opp = opp;
out:
	mutex_unlock(&data->lock);
	return err;
}

static int exynos5_int_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	rcu_read_lock();
	stat->current_frequency = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	stat->busy_time = data->busy;
	stat->total_time = 100;

	return 0;
}

static void exynos5_int_poll_start(struct busfreq_data_int *data)
{
	schedule_delayed_work(&data->work, EXYNOS5_BUS_INT_POLL_TIME);
}

static void exynos5_int_poll_stop(struct busfreq_data_int *data)
{
	cancel_delayed_work_sync(&data->work);
}

static void exynos5_int_poll(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct busfreq_data_int *data;
	int ret;

	dwork = to_delayed_work(work);
	data = container_of(dwork, struct busfreq_data_int, work);

	ret = exynos5_ppmu_get_busy(data->ppmu, PPMU_SET_RIGHT);

	if (ret >= 0) {
		data->busy = ret;
		mutex_lock(&data->devfreq->lock);
		update_devfreq(data->devfreq);
		mutex_unlock(&data->devfreq->lock);
	}

	schedule_delayed_work(&data->work, EXYNOS5_BUS_INT_POLL_TIME);
}

static void exynos5_int_exit(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_unregister_opp_notifier(dev, data->devfreq);
}

static struct devfreq_dev_profile exynos5_devfreq_int_profile = {
	.initial_freq		= 160000,
	.polling_ms		= 0,
	.target			= exynos5_busfreq_int_target,
	.get_dev_status		= exynos5_int_get_dev_status,
	.exit			= exynos5_int_exit,
};

static int exynos5250_init_int_tables(struct busfreq_data_int *data)
{
	int i, err = 0;

	for (i = LV_0; i < _LV_END; i++) {
		exynos5_int_opp_table[i].volt = asv_get_volt(ID_INT, exynos5_int_opp_table[i].clk);
		if (exynos5_int_opp_table[i].volt == 0) {
			dev_err(data->dev, "Invalid value\n");
			return -EINVAL;
		}
	}

	for (i = LV_0; i < _LV_END; i++) {
		err = opp_add(data->dev, exynos5_int_opp_table[i].clk,
				exynos5_int_opp_table[i].volt);
		if (err) {
			dev_err(data->dev, "Cannot add opp entries.\n");
			return err;
		}
	}

	return 0;
}
static struct devfreq_simple_ondemand_data exynos5_int_ondemand_data = {
	.downdifferential = 2,
	.upthreshold = INT_BUS_SATURATION_RATIO,
};

static int exynos5_busfreq_int_pm_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct busfreq_data_int *data = container_of(this, struct busfreq_data_int,
						 pm_notifier);
	struct opp *opp;
	unsigned long maxfreq = ULONG_MAX;
	unsigned long freq;
	int err = 0;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		/* Set Fastest and Deactivate DVFS */
		mutex_lock(&data->lock);

		data->disabled = true;

		rcu_read_lock();
		opp = opp_find_freq_floor(data->dev, &maxfreq);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			err = PTR_ERR(opp);
			goto unlock;
		}
		freq = opp_get_freq(opp);
		rcu_read_unlock();

		err = exynos5_int_setvolt(data, opp);
		if (err)
			goto unlock;

		err = clk_set_rate(data->int_clk, freq * 1000);

		if (err)
			goto unlock;

		data->curr_opp = opp;
unlock:
		mutex_unlock(&data->lock);
		if (err)
			return NOTIFY_BAD;
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		/* Reactivate */
		mutex_lock(&data->lock);
		data->disabled = false;
		mutex_unlock(&data->lock);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static __devinit int exynos5_busfreq_int_probe(struct platform_device *pdev)
{
	struct busfreq_data_int *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	unsigned long initial_freq;
	int err = 0;

	data = devm_kzalloc(&pdev->dev, sizeof(struct busfreq_data_int), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	data->pm_notifier.notifier_call = exynos5_busfreq_int_pm_notifier_event;
	data->dev = dev;
	mutex_init(&data->lock);

	err = exynos5250_init_int_tables(data);
	if (err)
		goto err_regulator;

	data->vdd_int = regulator_get(dev, "vdd_int");
	if (IS_ERR(data->vdd_int)) {
		dev_err(dev, "Cannot get the regulator \"vdd_int\"\n");
		err = PTR_ERR(data->vdd_int);
		goto err_regulator;
	}

	data->int_clk = clk_get(dev, "int_clk");
	if (IS_ERR(data->int_clk)) {
		dev_err(dev, "Cannot get clock \"int_clk\"\n");
		err = PTR_ERR(data->int_clk);
		goto err_clock;
	}

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &exynos5_devfreq_int_profile.initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
		       exynos5_devfreq_int_profile.initial_freq);
		err = PTR_ERR(opp);
		goto err_opp_add;
	}
	initial_freq = opp_get_freq(opp);
	rcu_read_unlock();
	data->curr_opp = opp;

	err = clk_set_rate(data->int_clk, initial_freq * 1000);
	if (err) {
		dev_err(dev, "Failed to set initial frequency\n");
		goto err_opp_add;
	}

	err = exynos5_int_setvolt(data, opp);
	if (err)
		goto err_opp_add;

	platform_set_drvdata(pdev, data);

	data->ppmu = exynos5_ppmu_get();
	if (!data->ppmu)
		goto err_ppmu_get;

	INIT_DELAYED_WORK_DEFERRABLE(&data->work, exynos5_int_poll);
	exynos5_int_poll_start(data);

	data->devfreq = devfreq_add_device(dev, &exynos5_devfreq_int_profile,
					   &devfreq_simple_ondemand,
					   &exynos5_int_ondemand_data);

	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

	devfreq_register_opp_notifier(dev, data->devfreq);

	err = register_pm_notifier(&data->pm_notifier);
	if (err) {
		dev_err(dev, "Failed to setup pm notifier\n");
		goto err_devfreq_add;
	}

	pm_qos_add_request(&data->mif_req, PM_QOS_MEMORY_THROUGHPUT, -1);

	return 0;

err_devfreq_add:
	devfreq_remove_device(data->devfreq);
	exynos5_int_poll_stop(data);
err_ppmu_get:
	platform_set_drvdata(pdev, NULL);
err_opp_add:
	clk_put(data->int_clk);
err_clock:
	regulator_put(data->vdd_int);
err_regulator:
	return err;
}

static __devexit int exynos5_busfreq_int_remove(struct platform_device *pdev)
{
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	pm_qos_remove_request(&data->mif_req);
	unregister_pm_notifier(&data->pm_notifier);
	devfreq_remove_device(data->devfreq);
	regulator_put(data->vdd_int);
	clk_put(data->int_clk);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos5_busfreq_int_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	exynos5_int_poll_stop(data);
	return 0;
}

static int exynos5_busfreq_int_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	exynos5_int_poll_start(data);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(exynos5_busfreq_int_pm, exynos5_busfreq_int_suspend,
		exynos5_busfreq_int_resume);

static struct platform_driver exynos5_busfreq_int_driver = {
	.probe		= exynos5_busfreq_int_probe,
	.remove		= __devexit_p(exynos5_busfreq_int_remove),
	.driver		= {
		.name		= "exynos5-bus-int",
		.owner		= THIS_MODULE,
		.pm		= &exynos5_busfreq_int_pm,
	},
};

static int __init exynos5_busfreq_int_init(void)
{
	return platform_driver_register(&exynos5_busfreq_int_driver);
}
late_initcall(exynos5_busfreq_int_init);

static void __exit exynos5_busfreq_int_exit(void)
{
	platform_driver_unregister(&exynos5_busfreq_int_driver);
}
module_exit(exynos5_busfreq_int_exit);
