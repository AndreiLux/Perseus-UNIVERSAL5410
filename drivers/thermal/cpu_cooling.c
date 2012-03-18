/*
 *  linux/drivers/thermal/cpu_cooling.c
 *
 *  Copyright (C) 2011	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2011  Amit Daniel <amit.kachhap@linaro.org>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>

struct cpufreq_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	struct freq_clip_table *tab_ptr;
	unsigned int tab_size;
	unsigned int cpufreq_state;
	const struct cpumask *allowed_cpus;
	struct list_head node;
};

static LIST_HEAD(cooling_cpufreq_list);
static DEFINE_MUTEX(cooling_cpufreq_lock);
static DEFINE_IDR(cpufreq_idr);
static DEFINE_PER_CPU(unsigned int, max_policy_freq);
static struct freq_clip_table *notify_table;
static int notify_state;
static BLOCKING_NOTIFIER_HEAD(cputherm_state_notifier_list);

static int get_idr(struct idr *idr, struct mutex *lock, int *id)
{
	int err;
again:
	if (unlikely(idr_pre_get(idr, GFP_KERNEL) == 0))
		return -ENOMEM;

	if (lock)
		mutex_lock(lock);
	err = idr_get_new(idr, NULL, id);
	if (lock)
		mutex_unlock(lock);
	if (unlikely(err == -EAGAIN))
		goto again;
	else if (unlikely(err))
		return err;

	*id = *id & MAX_ID_MASK;
	return 0;
}

static void release_idr(struct idr *idr, struct mutex *lock, int id)
{
	if (lock)
		mutex_lock(lock);
	idr_remove(idr, id);
	if (lock)
		mutex_unlock(lock);
}

int cputherm_register_notifier(struct notifier_block *nb, unsigned int list)
{
	int ret = 0;

	switch (list) {
	case CPUFREQ_COOLING_TYPE:
	case CPUHOTPLUG_COOLING_TYPE:
		ret = blocking_notifier_chain_register(
				&cputherm_state_notifier_list, nb);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(cputherm_register_notifier);

int cputherm_unregister_notifier(struct notifier_block *nb, unsigned int list)
{
	int ret = 0;

	switch (list) {
	case CPUFREQ_COOLING_TYPE:
	case CPUHOTPLUG_COOLING_TYPE:
		ret = blocking_notifier_chain_unregister(
				&cputherm_state_notifier_list, nb);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(cputherm_unregister_notifier);

/*Below codes defines functions to be used for cpufreq as cooling device*/
static bool is_cpufreq_valid(int cpu)
{
	struct cpufreq_policy policy;
	return !cpufreq_get_policy(&policy, cpu) ? true : false;
}

static int cpufreq_apply_cooling(struct cpufreq_cooling_device *cpufreq_device,
				unsigned long cooling_state)
{
	unsigned int event, cpuid;
	struct freq_clip_table *th_table;

	if (cooling_state > cpufreq_device->tab_size)
		return -EINVAL;

	cpufreq_device->cpufreq_state = cooling_state;

	/*cpufreq thermal notifier uses this cpufreq device pointer*/
	notify_state = cooling_state;

	if (notify_state > 0) {
		th_table = &(cpufreq_device->tab_ptr[cooling_state - 1]);
		memcpy(notify_table, th_table, sizeof(struct freq_clip_table));
		event = CPUFREQ_COOLING_TYPE;
		blocking_notifier_call_chain(&cputherm_state_notifier_list,
						event, notify_table);
	}

	for_each_cpu(cpuid, cpufreq_device->allowed_cpus) {
		if (is_cpufreq_valid(cpuid))
			cpufreq_update_policy(cpuid);
	}

	notify_state = -1;

	return 0;
}

static int cpufreq_thermal_notifier(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned long max_freq = 0;

	if ((event != CPUFREQ_ADJUST) || (notify_state == -1))
		return 0;

	if (notify_state > 0) {
		max_freq = notify_table->freq_clip_max;

		if (per_cpu(max_policy_freq, policy->cpu) == 0)
			per_cpu(max_policy_freq, policy->cpu) = policy->max;
	} else {
		if (per_cpu(max_policy_freq, policy->cpu) != 0) {
			max_freq = per_cpu(max_policy_freq, policy->cpu);
			per_cpu(max_policy_freq, policy->cpu) = 0;
		} else {
			max_freq = policy->max;
		}
	}

	/* Never exceed user_policy.max*/
	if (max_freq > policy->user_policy.max)
		max_freq = policy->user_policy.max;

	if (policy->max != max_freq)
		cpufreq_verify_within_limits(policy, 0, max_freq);

	return 0;
}

/*
 * cpufreq cooling device callback functions
 */
static int cpufreq_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	int ret = -EINVAL;
	struct cpufreq_cooling_device *cpufreq_device;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_device, &cooling_cpufreq_list, node) {
		if (cpufreq_device && cpufreq_device->cool_dev == cdev) {
			*state = cpufreq_device->tab_size;
			ret = 0;
			break;
		}
	}
	mutex_unlock(&cooling_cpufreq_lock);
	return ret;
}

static int cpufreq_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	int ret = -EINVAL;
	struct cpufreq_cooling_device *cpufreq_device;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_device, &cooling_cpufreq_list, node) {
		if (cpufreq_device && cpufreq_device->cool_dev == cdev) {
			*state = cpufreq_device->cpufreq_state;
			ret = 0;
			break;
		}
	}
	mutex_unlock(&cooling_cpufreq_lock);
	return ret;
}

/*This cooling may be as PASSIVE/ACTIVE type*/
static int cpufreq_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	int ret = -EINVAL;
	struct cpufreq_cooling_device *cpufreq_device;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_device, &cooling_cpufreq_list, node) {
		if (cpufreq_device && cpufreq_device->cool_dev == cdev) {
			ret = 0;
			break;
		}
	}
	mutex_unlock(&cooling_cpufreq_lock);

	if (!ret)
		ret = cpufreq_apply_cooling(cpufreq_device, state);

	return ret;
}

/* bind cpufreq callbacks to cpufreq cooling device */
static struct thermal_cooling_device_ops cpufreq_cooling_ops = {
	.get_max_state = cpufreq_get_max_state,
	.get_cur_state = cpufreq_get_cur_state,
	.set_cur_state = cpufreq_set_cur_state,
};

static struct notifier_block thermal_cpufreq_notifier_block = {
	.notifier_call = cpufreq_thermal_notifier,
};

struct thermal_cooling_device *cpufreq_cooling_register(
	struct freq_clip_table *tab_ptr, unsigned int tab_size,
	const struct cpumask *mask_val)
{
	struct thermal_cooling_device *cool_dev;
	struct cpufreq_cooling_device *cpufreq_dev = NULL;
	unsigned int cpufreq_dev_count = 0;
	char dev_name[THERMAL_NAME_LENGTH];
	int ret = 0, id = 0, i;

	if (tab_ptr == NULL || tab_size == 0)
		return ERR_PTR(-EINVAL);

	list_for_each_entry(cpufreq_dev, &cooling_cpufreq_list, node)
		cpufreq_dev_count++;

	cpufreq_dev =
		kzalloc(sizeof(struct cpufreq_cooling_device), GFP_KERNEL);

	if (!cpufreq_dev)
		return ERR_PTR(-ENOMEM);

	if (cpufreq_dev_count == 0) {
		notify_table = kzalloc(sizeof(struct freq_clip_table),
					GFP_KERNEL);
		if (!notify_table) {
			kfree(cpufreq_dev);
			return ERR_PTR(-ENOMEM);
		}
	}

	cpufreq_dev->tab_ptr = tab_ptr;
	cpufreq_dev->tab_size = tab_size;
	cpufreq_dev->allowed_cpus = mask_val;

	/* Initialize all the tab_ptr->mask_val to the passed mask_val */
	for (i = 0; i < tab_size; i++)
		((struct freq_clip_table *)&tab_ptr[i])->mask_val = mask_val;

	ret = get_idr(&cpufreq_idr, &cooling_cpufreq_lock, &cpufreq_dev->id);
	if (ret) {
		kfree(cpufreq_dev);
		return ERR_PTR(-EINVAL);
	}

	sprintf(dev_name, "thermal-cpufreq-%d", cpufreq_dev->id);

	cool_dev = thermal_cooling_device_register(dev_name, cpufreq_dev,
						&cpufreq_cooling_ops);
	if (!cool_dev) {
		release_idr(&cpufreq_idr, &cooling_cpufreq_lock,
						cpufreq_dev->id);
		kfree(cpufreq_dev);
		return ERR_PTR(-EINVAL);
	}
	cpufreq_dev->id = id;
	cpufreq_dev->cool_dev = cool_dev;
	mutex_lock(&cooling_cpufreq_lock);
	list_add_tail(&cpufreq_dev->node, &cooling_cpufreq_list);
	mutex_unlock(&cooling_cpufreq_lock);

	/*Register the notifier for first cpufreq cooling device*/
	if (cpufreq_dev_count == 0)
		cpufreq_register_notifier(&thermal_cpufreq_notifier_block,
						CPUFREQ_POLICY_NOTIFIER);
	return cool_dev;
}
EXPORT_SYMBOL(cpufreq_cooling_register);

void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct cpufreq_cooling_device *cpufreq_dev = NULL;
	unsigned int cpufreq_dev_count = 0;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_dev, &cooling_cpufreq_list, node) {
		if (cpufreq_dev && cpufreq_dev->cool_dev == cdev)
			break;
		cpufreq_dev_count++;
	}

	if (!cpufreq_dev || cpufreq_dev->cool_dev != cdev) {
		mutex_unlock(&cooling_cpufreq_lock);
		return;
	}

	list_del(&cpufreq_dev->node);
	mutex_unlock(&cooling_cpufreq_lock);

	/*Unregister the notifier for the last cpufreq cooling device*/
	if (cpufreq_dev_count == 1) {
		cpufreq_unregister_notifier(&thermal_cpufreq_notifier_block,
					CPUFREQ_POLICY_NOTIFIER);
		kfree(notify_table);
	}

	thermal_cooling_device_unregister(cpufreq_dev->cool_dev);
	release_idr(&cpufreq_idr, &cooling_cpufreq_lock, cpufreq_dev->id);
	kfree(cpufreq_dev);
}
EXPORT_SYMBOL(cpufreq_cooling_unregister);
