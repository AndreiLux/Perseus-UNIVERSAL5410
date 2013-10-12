/*
 *  linux/drivers/devfreq/governor_simpleondemand.c
 *
 *  Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/pm_qos.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "governor.h"

struct devfreq_simple_ondemand_notifier_block {
	struct list_head node;
	struct notifier_block nb;
	struct devfreq *df;
};

static LIST_HEAD(devfreq_simple_ondemand_list);
static DEFINE_MUTEX(devfreq_simple_ondemand_mutex);

static int devfreq_simple_ondemand_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_simple_ondemand_notifier_block *pq_nb;
	struct devfreq_simple_ondemand_data *simple_ondemand_data;

	pq_nb = container_of(nb, struct devfreq_simple_ondemand_notifier_block, nb);

	simple_ondemand_data = pq_nb->df->data;

	mutex_lock(&pq_nb->df->lock);
	update_devfreq(pq_nb->df);
	mutex_unlock(&pq_nb->df->lock);

	return NOTIFY_OK;
}

/* Default constants for DevFreq-Simple-Ondemand (DFSO) */
#define DFSO_UPTHRESHOLD	(90)
#define DFSO_DOWNDIFFERENCTIAL	(5)
static int devfreq_simple_ondemand_func(struct devfreq *df,
					unsigned long *freq)
{
	struct devfreq_dev_status stat;
	int err = df->profile->get_dev_status(df->dev.parent, &stat);
	unsigned long long a, b;
	unsigned int dfso_upthreshold = DFSO_UPTHRESHOLD;
	unsigned int dfso_downdifferential = DFSO_DOWNDIFFERENCTIAL;
	struct devfreq_simple_ondemand_data *data = df->data;
	unsigned long max = (df->max_freq) ? df->max_freq : UINT_MAX;
	unsigned long pm_qos_min;

	if (!data)
		return -EINVAL;

	pm_qos_min = pm_qos_request(data->pm_qos_class);

	if (err)
		return err;

	if (data->upthreshold)
		dfso_upthreshold = data->upthreshold;
	if (data->downdifferential)
		dfso_downdifferential = data->downdifferential;

	if (dfso_upthreshold > 100 ||
	    dfso_upthreshold < dfso_downdifferential)
		return -EINVAL;

	if (data->cal_qos_max)
		max = (df->max_freq) ? df->max_freq : 0;

	/* Assume MAX if it is going to be divided by zero */
	if (stat.total_time == 0) {
		if (data->cal_qos_max)
			max = max3(max, data->cal_qos_max, pm_qos_min);
		*freq = max;
		return 0;
	}

	/* Set MAX if it's busy enough */
	if (stat.busy_time * 100 >
	    stat.total_time * dfso_upthreshold) {
		if (data->cal_qos_max)
			max = max3(max, data->cal_qos_max, pm_qos_min);
		*freq = max;
		return 0;
	}

	/* Set MAX if we do not know the initial frequency */
	if (stat.current_frequency == 0) {
		if (data->cal_qos_max)
			max = max3(max, data->cal_qos_max, pm_qos_min);
		*freq = max;
		return 0;
	}

	/* Keep the current frequency */
	if (stat.busy_time * 100 >
	    stat.total_time * (dfso_upthreshold - dfso_downdifferential)) {
		*freq = stat.current_frequency;
		return 0;
	}

	/* Set the desired frequency based on the load */
	a = stat.busy_time;
	a *= stat.current_frequency;
	b = div_u64(a, stat.total_time);
	b *= 100;
	b = div_u64(b, (dfso_upthreshold - dfso_downdifferential / 2));

	if (data->cal_qos_max) {
		if (b > data->cal_qos_max)
			b = data->cal_qos_max;
	}

	*freq = (unsigned long) b;

	/* compare calculated freq and pm_qos_min */
	if (pm_qos_min)
		*freq = max(pm_qos_min, *freq);

	if (df->min_freq && *freq < df->min_freq)
		*freq = df->min_freq;
	if (df->max_freq && *freq > df->max_freq)
		*freq = df->max_freq;

	return 0;
}

static int devfreq_simple_ondemand_init(struct devfreq *df)
{
	int ret;
	struct devfreq_simple_ondemand_notifier_block *pq_nb;
	struct devfreq_simple_ondemand_data *data = df->data;

	if (!data)
		return -EINVAL;

	pq_nb = kzalloc(sizeof(*pq_nb), GFP_KERNEL);
	if (!pq_nb)
		return -ENOMEM;

	pq_nb->df = df;
	pq_nb->nb.notifier_call = devfreq_simple_ondemand_notifier;
	INIT_LIST_HEAD(&pq_nb->node);

	ret = pm_qos_add_notifier(data->pm_qos_class, &pq_nb->nb);
	if (ret < 0)
		goto err;

	mutex_lock(&devfreq_simple_ondemand_mutex);
	list_add_tail(&pq_nb->node, &devfreq_simple_ondemand_list);
	mutex_unlock(&devfreq_simple_ondemand_mutex);

	return 0;
err:
	kfree(pq_nb);

	return ret;
}

const struct devfreq_governor devfreq_simple_ondemand = {
	.name = "simple_ondemand",
	.get_target_freq = devfreq_simple_ondemand_func,
	.init = devfreq_simple_ondemand_init,
};
