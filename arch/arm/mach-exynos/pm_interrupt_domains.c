/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/pm_interrupt_domains.h>

/* codes for interrupt power domain */
static int exynos_pm_interrupt_runtime_suspend(struct device *dev, int flags);
static int exynos_pm_interrupt_runtime_idle(struct device *dev, int flags);
static int exynos_pm_interrupt_runtime_resume(struct device *dev, int flags);
static void exynos_pm_interrupt_runtime_enable(struct device *dev);
static void exynos_pm_interrupt_runtime_disable(struct device *dev);
static void exynos_pm_interrupt_runtime_barrier(struct device *dev);
static void exynos_pm_interrupt_runtime_internal_barrier(struct device *dev);
static void exynos_pm_interrupt_runtime_set_status(struct device *dev, unsigned int status);

static LIST_HEAD(interrupt_domain_list);
static DEFINE_SPINLOCK(interrupt_domain_list_lock);

struct exynos_pm_domain *exynos_pm_interrupt_dev_to_pd(struct device *dev)
{
	if (IS_ERR_OR_NULL(dev->pm_domain))
		return ERR_PTR(-EINVAL);

	return (struct exynos_pm_domain *)
		container_of(dev->pm_domain, struct generic_pm_domain, domain);
}

static struct exynos_pm_interrupt_domain_data*
	exynos_pm_interrupt_to_domain_data(struct pm_domain_data *pdd)
{
	return container_of(pdd, struct exynos_pm_interrupt_domain_data, base);
}

static struct exynos_pm_interrupt_domain_data*
	exynos_pm_interrupt_dev_domain_data(struct device *dev)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);

	return exynos_pm_interrupt_to_domain_data(psd->domain_data);
}

#define exynos_pm_interrupt_int_callback(name, callback)			\
static int exynos_pm_interrupt_callback_##name					\
(struct exynos_pm_domain *domain, struct device *dev)				\
{										\
	int (*cb)(struct device *dev);						\
	int ret = (int)0;							\
										\
	cb = domain->pd.dev_ops.callback;					\
	if (cb) {								\
		ret = cb(dev);							\
	} else {								\
		cb = exynos_pm_interrupt_dev_domain_data(dev)->ops.callback;	\
		if (cb)								\
			ret = cb(dev);						\
	}									\
	return ret;								\
}

#define exynos_pm_interrupt_bool_callback(name, callback)			\
static bool exynos_pm_interrupt_callback_##name					\
(struct exynos_pm_domain *domain, struct device *dev)				\
{										\
	bool (*cb)(struct device *dev);						\
	bool ret = (bool)0;							\
										\
	cb = domain->pd.dev_ops.callback;					\
	if (cb) {								\
		ret = cb(dev);							\
	} else {								\
		cb = exynos_pm_interrupt_dev_domain_data(dev)->ops.callback;	\
		if (cb)								\
			ret = cb(dev);						\
	}									\
	return ret;								\
}

exynos_pm_interrupt_int_callback(stop, stop);
exynos_pm_interrupt_int_callback(start, start);
exynos_pm_interrupt_int_callback(save, save_state);
exynos_pm_interrupt_int_callback(restore, restore_state);
exynos_pm_interrupt_bool_callback(active_wakeup, active_wakeup);
exynos_pm_interrupt_int_callback(suspend, suspend);
exynos_pm_interrupt_int_callback(suspend_late, suspend_late);
exynos_pm_interrupt_int_callback(resume_early, resume_early);
exynos_pm_interrupt_int_callback(resume, resume);
exynos_pm_interrupt_int_callback(freeze, freeze);
exynos_pm_interrupt_int_callback(freeze_late, freeze_late);
exynos_pm_interrupt_int_callback(thaw_early, thaw_early);
exynos_pm_interrupt_int_callback(thaw, thaw);

static int exynos_pm_interrupt_rpm_idle(struct device *dev, int flags)
{
	int (*cb)(struct device *dev);
	int retval = 0;

	if (dev->power.runtime_status != RPM_ACTIVE)
		retval = -EAGAIN;
	else if (dev->power.idle_notification)
		retval = -EINPROGRESS;

	if (retval)
		goto out;

	dev->power.idle_notification = true;

	cb = NULL;
	if (dev->pm_domain)
		cb = dev->pm_domain->ops.runtime_idle;
	else if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_idle;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_idle;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_idle;
	else if (dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_idle;

	if (cb) {
		spin_unlock(&dev->power.lock);
		cb(dev);
		spin_lock(&dev->power.lock);
	}

	dev->power.idle_notification = false;

out:
	return retval;
}

static int exynos_pm_interrupt_rpm_suspend(struct device *dev, int flags)
{
	int (*cb)(struct device *dev);
	struct device *parent = NULL;
	int retval = 0;

repeat:
	if (dev->power.runtime_status == RPM_RESUMING)
		retval = -EAGAIN;

	if (retval)
		goto out;

	if (dev->power.runtime_status == RPM_SUSPENDING) {
		for (;;) {
			if (dev->power.runtime_status != RPM_SUSPENDING)
				break;
		}
		goto repeat;
	}

	cb = NULL;
	if (dev->pm_domain)
		cb = dev->pm_domain->ops.runtime_suspend;
	else if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_suspend;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_suspend;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_suspend;
	else if (dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_suspend;

	if (cb) {
		spin_unlock(&dev->power.lock);
		retval = cb(dev);
		spin_lock(&dev->power.lock);
	}

	if (retval)
		goto fail;

	update_pm_runtime_accounting(dev);
	dev->power.runtime_status = RPM_SUSPENDED;

	if (dev->parent) {
		parent = dev->parent;
		atomic_add_unless(&parent->power.child_count, -1, 0);
	}

	if (parent && !parent->power.ignore_children && !dev->power.irq_safe) {
		spin_unlock(&dev->power.lock);

		spin_lock(&parent->power.lock);
		exynos_pm_interrupt_rpm_idle(parent, 0);
		spin_unlock(&parent->power.lock);

		spin_lock(&dev->power.lock);
	}

out:
	return retval;

fail:
	update_pm_runtime_accounting(dev);
	dev->power.runtime_status = RPM_ACTIVE;

	return retval;
}

static int exynos_pm_interrupt_rpm_resume(struct device *dev, int flags)
{
	int (*cb)(struct device *dev);
	struct device *parent = NULL;
	int retval = 0;

repeat:
	if (dev->power.disable_depth > 0)
		retval = -EACCES;

	if (retval)
		goto out;

	if (dev->power.runtime_status == RPM_ACTIVE) {
		retval = 1;
		goto out;
	}

	if (dev->power.runtime_status == RPM_RESUMING ||
		dev->power.runtime_status == RPM_SUSPENDING) {
		for (;;) {
			if (dev->power.runtime_status != RPM_RESUMING &&
				dev->power.runtime_status != RPM_SUSPENDING)
				break;
		}

		goto repeat;
	}

	if (!parent && dev->parent) {
		parent = dev->parent;

		spin_unlock(&dev->power.lock);
		spin_lock(&parent->power.lock);

		if (!parent->power.disable_depth &&
			!parent->power.ignore_children) {
			exynos_pm_interrupt_rpm_resume(parent, 0);
			if (parent->power.runtime_status != RPM_ACTIVE)
				retval = -EBUSY;
		}
		spin_unlock(&parent->power.lock);
		spin_lock(&dev->power.lock);
		if (retval)
			goto out;
		goto repeat;
	}

	cb = NULL;
	if (dev->pm_domain)
		cb = dev->pm_domain->ops.runtime_resume;
	else if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_resume;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_resume;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_resume;
	else if (dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_resume;

	if (cb) {
		spin_unlock(&dev->power.lock);
		retval = cb(dev);
		spin_lock(&dev->power.lock);
	}

	if (retval) {
		update_pm_runtime_accounting(dev);
		dev->power.runtime_status = RPM_SUSPENDED;
	} else {
		update_pm_runtime_accounting(dev);
		dev->power.runtime_status = RPM_ACTIVE;
		if (parent)
			atomic_inc(&parent->power.child_count);
	}

out:
	return retval;
}

static int exynos_pm_interrupt_generic_callback_runtime_idle(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm && pm->runtime_idle) {
		int ret = pm->runtime_idle(dev);
		if (ret)
			return ret;
	}

	exynos_pm_interrupt_runtime_suspend(dev, 0);

	return 0;
}

static void exynos_pm_interrupt_generic_callback_complete(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm && pm->complete)
		pm->complete(dev);

	exynos_pm_interrupt_runtime_idle(dev, 0);
}

static void exynos_pm_interrupt_set_active(struct exynos_pm_domain *domain)
{
	if (domain->pd.resume_count == 0)
		domain->pd.status = GPD_STATE_ACTIVE;
}

static void exynos_pm_interrupt_acquire_lock(struct exynos_pm_domain *domain)
{
	spin_lock(&domain->interrupt_lock);
}

static void exynos_pm_interrupt_release_lock(struct exynos_pm_domain *domain)
{
	spin_unlock(&domain->interrupt_lock);
}

static int exynos_pm_interrupt_poweron(struct exynos_pm_domain *domain)
{
	int ret = 0;

	if (domain->pd.status == GPD_STATE_ACTIVE ||
		(domain->pd.prepared_count > 0 && domain->pd.suspend_power_off)) {
		return 0;
	}

	if (domain->pd.status != GPD_STATE_POWER_OFF) {
		exynos_pm_interrupt_set_active(domain);
		return 0;
	}

	if (domain->pd.power_on) {
		ret = domain->pd.power_on(&domain->pd);
		if (ret)
			return ret;
	}

	exynos_pm_interrupt_set_active(domain);

	return 0;
}

static int exynos_pm_interrupt_poweroff(struct exynos_pm_domain *domain)
{
	struct exynos_pm_interrupt_domain_data *domain_data;
	struct pm_domain_data *pdd;
	unsigned int not_suspended;
	int ret = 0;

start:

	if (domain->pd.status == GPD_STATE_POWER_OFF ||
		domain->pd.resume_count > 0 || domain->pd.prepared_count > 0)
		return 0;

	not_suspended = 0;
	list_for_each_entry(pdd, &domain->pd.dev_list, list_node) {
		if (pdd->dev->driver && (!pm_runtime_suspended(pdd->dev) ||
			pdd->dev->power.irq_safe))
			not_suspended++;
	}

	if (not_suspended > domain->pd.in_progress)
		return -EBUSY;

	if (domain->pd.poweroff_task) {
		domain->pd.status = GPD_STATE_REPEAT;
		return 0;
	}

	domain->pd.status = GPD_STATE_BUSY;
	domain->pd.poweroff_task = current;

	list_for_each_entry_reverse(pdd, &domain->pd.dev_list, list_node) {
		ret = 0;

		domain_data = exynos_pm_interrupt_to_domain_data(pdd);

		if (!domain_data->need_restore) {
			exynos_pm_interrupt_release_lock(domain);
			exynos_pm_interrupt_callback_start(domain, pdd->dev);
			ret = exynos_pm_interrupt_callback_save(domain, pdd->dev);
			exynos_pm_interrupt_callback_stop(domain, pdd->dev);
			exynos_pm_interrupt_acquire_lock(domain);
			if (!ret)
				domain_data->need_restore = true;
		}

		if (domain->pd.status == GPD_STATE_ACTIVE || domain->pd.resume_count > 0)
			goto out;

		if (ret) {
			exynos_pm_interrupt_set_active(domain);
			goto out;
		}

		if (domain->pd.status == GPD_STATE_REPEAT) {
			domain->pd.poweroff_task = NULL;
			goto start;
		}
	}

	if (domain->pd.power_off) {
		ret = domain->pd.power_off(&domain->pd);
		if (ret == -EBUSY) {
			exynos_pm_interrupt_set_active(domain);
			goto out;
		}
	}

	domain->pd.status = GPD_STATE_POWER_OFF;

out:
	domain->pd.poweroff_task = NULL;
	return ret;
}

static int exynos_pm_interrupt_pd_runtime_suspend(struct device *dev)
{
	struct exynos_pm_domain *domain;
	int ret;

	domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	ret = exynos_pm_interrupt_callback_stop(domain, dev);
	if (ret)
		return ret;

	exynos_pm_interrupt_acquire_lock(domain);
	domain->pd.in_progress++;
	exynos_pm_interrupt_poweroff(domain);
	domain->pd.in_progress--;
	exynos_pm_interrupt_release_lock(domain);

	return 0;
}

static int exynos_pm_interrupt_pd_runtime_resume(struct device *dev)
{
	struct exynos_pm_domain *domain;
	struct exynos_pm_interrupt_domain_data *domain_data;
	int ret;

	domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	exynos_pm_interrupt_acquire_lock(domain);
	ret = exynos_pm_interrupt_poweron(domain);
	if (ret) {
		exynos_pm_interrupt_release_lock(domain);
		return ret;
	}
	domain->pd.status = GPD_STATE_BUSY;
	domain->pd.resume_count++;

	for (;;) {
		if (!domain->pd.poweroff_task || domain->pd.poweroff_task == current)
			break;
	}

	domain_data = exynos_pm_interrupt_dev_domain_data(dev);
	if (domain_data->need_restore) {
		exynos_pm_interrupt_release_lock(domain);
		exynos_pm_interrupt_callback_start(domain, dev);
		exynos_pm_interrupt_callback_restore(domain, dev);
		exynos_pm_interrupt_callback_stop(domain, dev);
		exynos_pm_interrupt_acquire_lock(domain);

		domain_data->need_restore = false;
	}


	domain->pd.resume_count--;
	exynos_pm_interrupt_set_active(domain);
	exynos_pm_interrupt_release_lock(domain);

	exynos_pm_interrupt_callback_start(domain, dev);

	return 0;
}

static int exynos_pm_interrupt_pd_prepare(struct device *dev)
{
	struct exynos_pm_domain *domain;
	int ret;

	domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	pm_runtime_get_noresume(dev);

	exynos_pm_interrupt_acquire_lock(domain);

	if (domain->pd.prepared_count++ == 0) {
		domain->pd.suspended_count = 0;
		domain->pd.suspend_power_off = domain->pd.status == GPD_STATE_POWER_OFF;
	}

	exynos_pm_interrupt_release_lock(domain);

	if (domain->pd.suspend_power_off) {
		pm_runtime_put_noidle(dev);
		return 0;
	}

	exynos_pm_interrupt_runtime_resume(dev, 0);
	exynos_pm_interrupt_runtime_disable(dev);

	ret = pm_generic_prepare(dev);
	if (ret) {
		exynos_pm_interrupt_acquire_lock(domain);

		if (--domain->pd.prepared_count == 0)
			domain->pd.suspend_power_off = false;

		exynos_pm_interrupt_release_lock(domain);
		exynos_pm_interrupt_runtime_enable(dev);
	}

	exynos_pm_interrupt_runtime_put_sync(dev);
	return ret;
}

static int exynos_pm_interrupt_pd_suspend(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_suspend(domain, dev);
}

static int exynos_pm_interrupt_pd_suspend_late(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_suspend_late(domain, dev);
}

static int exynos_pm_interrupt_pd_suspend_noirq(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	if (domain->pd.suspend_power_off ||
		(dev->power.wakeup_path &&
		 exynos_pm_interrupt_callback_active_wakeup(domain, dev)))
		return 0;

	exynos_pm_interrupt_callback_stop(domain, dev);

	domain->pd.suspended_count++;
	exynos_pm_interrupt_poweroff(domain);

	return 0;
}

static int exynos_pm_interrupt_pd_resume_noirq(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	if (domain->pd.suspend_power_off ||
		(dev->power.wakeup_path &&
		exynos_pm_interrupt_callback_active_wakeup(domain, dev)))
		return 0;

	exynos_pm_interrupt_poweron(domain);
	domain->pd.suspended_count--;

	return exynos_pm_interrupt_callback_start(domain, dev);
}

static int exynos_pm_interrupt_pd_resume_early(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_resume_early(domain, dev);
}

static int exynos_pm_interrupt_pd_resume(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_resume(domain, dev);
}

static int exynos_pm_interrupt_pd_freeze(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_freeze(domain, dev);
}

static int exynos_pm_interrupt_pd_freeze_late(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_freeze_late(domain, dev);
}

static int exynos_pm_interrupt_pd_freeze_noirq(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_stop(domain, dev);
}

static int exynos_pm_interrupt_pd_thaw_noirq(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		 0 : exynos_pm_interrupt_callback_start(domain, dev);
}

static int exynos_pm_interrupt_pd_thaw_early(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_thaw_early(domain, dev);
}

static int exynos_pm_interrupt_pd_thaw(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	return domain->pd.suspend_power_off ?
		0 : exynos_pm_interrupt_callback_thaw(domain, dev);
}

static int exynos_pm_interrupt_pd_restore_noirq(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	if (IS_ERR(domain))
		return -EINVAL;

	if (domain->pd.suspended_count++ == 0) {
		domain->pd.status = GPD_STATE_POWER_OFF;
		if (domain->pd.suspend_power_off) {
			if (domain->pd.power_off)
				domain->pd.power_off(&domain->pd);
			return 0;
		}
	}

	if (domain->pd.suspend_power_off)
		return 0;

	exynos_pm_interrupt_poweron(domain);

	return exynos_pm_interrupt_callback_start(domain, dev);
}

static void exynos_pm_interrupt_pd_complete(struct device *dev)
{
	struct exynos_pm_domain *domain = exynos_pm_interrupt_dev_to_pd(dev);
	bool run_complete;

	if (IS_ERR(domain))
		return;

	exynos_pm_interrupt_acquire_lock(domain);
	run_complete = !domain->pd.suspend_power_off;
	if (--domain->pd.prepared_count == 0)
		domain->pd.suspend_power_off = false;
	exynos_pm_interrupt_release_lock(domain);

	if (run_complete) {
		exynos_pm_interrupt_generic_callback_complete(dev);
		exynos_pm_interrupt_runtime_set_status(dev, RPM_ACTIVE);
		exynos_pm_interrupt_runtime_enable(dev);
		exynos_pm_interrupt_runtime_idle(dev, 0);
	}
}

static int exynos_pm_interrupt_default_save_state(struct device *dev)
{
	int (*cb)(struct device *__dev);
	struct device_driver *drv = dev->driver;

	cb = exynos_pm_interrupt_dev_domain_data(dev)->ops.save_state;
	if (cb)
		return cb(dev);

	if (drv && drv->pm && drv->pm->runtime_suspend)
		return drv->pm->runtime_suspend(dev);

	return 0;
}

static int exynos_pm_interrupt_default_restore_state(struct device *dev)
{
	int (*cb)(struct device *__dev);
	struct device_driver *drv;

	if (!dev)
		return -EINVAL;

	cb = exynos_pm_interrupt_dev_domain_data(dev)->ops.restore_state;
	if (cb)
		return cb(dev);

	drv = dev->driver;

	if (drv->pm && drv->pm->runtime_resume)
		return drv->pm->runtime_resume(dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int exynos_pm_interrupt_default_suspend(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.suspend;

	return cb ? cb(dev) : pm_generic_suspend(dev);
}

static int exynos_pm_interrupt_default_suspend_late(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.suspend_late;

	return cb ? cb(dev) : pm_generic_suspend_late(dev);
}

static int exynos_pm_interrupt_default_resume_early(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.resume_early;

	return cb ? cb(dev) : pm_generic_resume_early(dev);
}

static int exynos_pm_interrupt_default_resume(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.resume;

	return cb ? cb(dev) : pm_generic_resume(dev);
}

static int exynos_pm_interrupt_default_freeze(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.freeze;

	return cb ? cb(dev) : pm_generic_freeze(dev);
}

static int exynos_pm_interrupt_default_freeze_late(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.freeze_late;

	return cb ? cb(dev) : pm_generic_freeze_late(dev);
}

static int exynos_pm_interrupt_default_thaw_early(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.thaw_early;

	return cb ? cb(dev) : pm_generic_thaw_early(dev);
}

static int exynos_pm_interrupt_default_thaw(struct device *dev)
{
	int (*cb)(struct device *__dev) = exynos_pm_interrupt_dev_domain_data(dev)->ops.thaw;

	return cb ? cb(dev) : pm_generic_thaw(dev);
}

#else /* !CONFIG_PM_SLEEP */

#define exynos_pm_interrupt_default_suspend		NULL
#define exynos_pm_interrupt_default_suspend_late	NULL
#define exynos_pm_interrupt_default_resume_early	NULL
#define exynos_pm_interrupt_default_resume		NULL
#define exynos_pm_interrupt_default_freeze		NULL
#define exynos_pm_interrupt_default_freeze_late		NULL
#define exynos_pm_interrupt_default_thaw_early		NULL
#define exynos_pm_interrupt_default_thaw		NULL

#endif /* !CONFIG_PM_SLEEP */

void exynos_pm_interrupt_powerdomain_init(struct exynos_pm_domain *domain,
							bool is_off)
{
	INIT_LIST_HEAD(&domain->pd.dev_list);
	spin_lock_init(&domain->interrupt_lock);
	domain->pd.status = is_off ? GPD_STATE_POWER_OFF : GPD_STATE_ACTIVE;
	domain->pd.in_progress = 0;
	domain->pd.resume_count = 0;
	domain->pd.device_count = 0;
	domain->pd.poweroff_task = NULL;

	domain->pd.domain.ops.runtime_suspend = exynos_pm_interrupt_pd_runtime_suspend;
	domain->pd.domain.ops.runtime_resume = exynos_pm_interrupt_pd_runtime_resume;
	domain->pd.domain.ops.runtime_idle = exynos_pm_interrupt_generic_callback_runtime_idle;
	domain->pd.domain.ops.prepare = exynos_pm_interrupt_pd_prepare;
	domain->pd.domain.ops.suspend = exynos_pm_interrupt_pd_suspend;
	domain->pd.domain.ops.suspend_late = exynos_pm_interrupt_pd_suspend_late;
	domain->pd.domain.ops.suspend_noirq = exynos_pm_interrupt_pd_suspend_noirq;;
	domain->pd.domain.ops.resume_noirq = exynos_pm_interrupt_pd_resume_noirq;
	domain->pd.domain.ops.resume_early = exynos_pm_interrupt_pd_resume_early;
	domain->pd.domain.ops.resume = exynos_pm_interrupt_pd_resume;
	domain->pd.domain.ops.freeze = exynos_pm_interrupt_pd_freeze;
	domain->pd.domain.ops.freeze_late = exynos_pm_interrupt_pd_freeze_late;
	domain->pd.domain.ops.freeze_noirq = exynos_pm_interrupt_pd_freeze_noirq;
	domain->pd.domain.ops.thaw_noirq = exynos_pm_interrupt_pd_thaw_noirq;
	domain->pd.domain.ops.thaw_early = exynos_pm_interrupt_pd_thaw_early;
	domain->pd.domain.ops.thaw = exynos_pm_interrupt_pd_thaw;
	domain->pd.domain.ops.poweroff = exynos_pm_interrupt_pd_suspend;
	domain->pd.domain.ops.poweroff_late = exynos_pm_interrupt_pd_suspend_late;
	domain->pd.domain.ops.poweroff_noirq = exynos_pm_interrupt_pd_suspend_noirq;
	domain->pd.domain.ops.restore_noirq = exynos_pm_interrupt_pd_restore_noirq;
	domain->pd.domain.ops.restore_early = exynos_pm_interrupt_pd_resume_early;
	domain->pd.domain.ops.restore = exynos_pm_interrupt_pd_resume;
	domain->pd.domain.ops.complete = exynos_pm_interrupt_pd_complete;
	domain->pd.dev_ops.save_state = exynos_pm_interrupt_default_save_state;
	domain->pd.dev_ops.restore_state = exynos_pm_interrupt_default_restore_state;
	domain->pd.dev_ops.suspend = exynos_pm_interrupt_default_suspend;
	domain->pd.dev_ops.suspend_late = exynos_pm_interrupt_default_suspend_late;
	domain->pd.dev_ops.resume_early = exynos_pm_interrupt_default_resume_early;
	domain->pd.dev_ops.resume = exynos_pm_interrupt_default_resume;
	domain->pd.dev_ops.freeze = exynos_pm_interrupt_default_freeze;
	domain->pd.dev_ops.freeze_late = exynos_pm_interrupt_default_freeze_late;
	domain->pd.dev_ops.thaw_early = exynos_pm_interrupt_default_thaw_early;
	domain->pd.dev_ops.thaw = exynos_pm_interrupt_default_thaw;

	spin_lock(&interrupt_domain_list_lock);
	list_add(&domain->pd.gpd_list_node, &interrupt_domain_list);
	spin_unlock(&interrupt_domain_list_lock);
}

int exynos_pm_interrupt_add_device(struct exynos_pm_domain *domain,
						struct device *dev)
{
	struct exynos_pm_interrupt_domain_data *domain_data;
	struct pm_domain_data *pdd;
	int ret = 0;

	if (IS_ERR_OR_NULL(domain) || IS_ERR_OR_NULL(dev))
		return -EINVAL;

	domain_data = kzalloc(sizeof(struct exynos_pm_interrupt_domain_data), GFP_KERNEL);
	if (!domain_data)
		return -ENOMEM;

	spin_lock_init(&domain_data->lock);

	dev_pm_get_subsys_data(dev);
	exynos_pm_interrupt_acquire_lock(domain);

	list_for_each_entry(pdd, &domain->pd.dev_list, list_node) {
		if (pdd->dev == dev) {
			ret = -EINVAL;
			goto err;
		}
	}
	domain->pd.device_count++;

	spin_lock(&domain_data->lock);
	spin_lock(&dev->power.lock);

	dev->pm_domain = &domain->pd.domain;
	dev->power.subsys_data->domain_data = &domain_data->base;
	domain_data->base.dev = dev;
	list_add_tail(&domain_data->base.list_node, &domain->pd.dev_list);
	domain_data->need_restore = domain->pd.status == GPD_STATE_POWER_OFF;

	spin_unlock(&dev->power.lock);
	spin_unlock(&domain_data->lock);

	exynos_pm_interrupt_release_lock(domain);

	return 0;

err:
	exynos_pm_interrupt_release_lock(domain);
	kfree(domain_data);

	return ret;
}

void exynos_pm_interrupt_need_restore(struct device *dev, bool val)
{
	struct pm_subsys_data *psd;

	spin_lock(&dev->power.lock);

	psd = dev_to_psd(dev);
	if (psd && psd->domain_data)
		exynos_pm_interrupt_to_domain_data(psd->domain_data)->need_restore = val;

	spin_unlock(&dev->power.lock);
}

static void exynos_pm_interrupt_runtime_enable(struct device *dev)
{
	spin_lock(&dev->power.lock);
	if (dev->power.disable_depth > 0)
		dev->power.disable_depth--;
	else
		dev_warn(dev, "Unbalanced %s!\n", __func__);
	spin_unlock(&dev->power.lock);
}

static void exynos_pm_interrupt_runtime_disable(struct device *dev)
{
	spin_lock(&dev->power.lock);
	if (dev->power.disable_depth > 0) {
		dev->power.disable_depth++;
		goto out;
	}

	if (!dev->power.disable_depth++)
		exynos_pm_interrupt_runtime_barrier(dev);
out:
	spin_unlock(&dev->power.lock);
}

static void exynos_pm_interrupt_runtime_barrier(struct device *dev)
{
	spin_lock(&dev->power.lock);

	pm_runtime_get_noresume(dev);
	exynos_pm_interrupt_runtime_internal_barrier(dev);
	pm_runtime_put_noidle(dev);

	spin_unlock(&dev->power.lock);
}

static void exynos_pm_interrupt_runtime_internal_barrier(struct device *dev)
{
	if (dev->power.runtime_status == RPM_SUSPENDING ||
		dev->power.runtime_status == RPM_RESUMING ||
		dev->power.idle_notification) {

		for (;;) {
			if (dev->power.runtime_status != RPM_SUSPENDING &&
				dev->power.runtime_status != RPM_RESUMING &&
				!dev->power.idle_notification)
				break;
		}
	}
}

static void exynos_pm_interrupt_runtime_set_status(struct device *dev, unsigned int status)
{
	struct device *parent = dev->parent;
	bool notify_parent = false;

	spin_lock(&dev->power.lock);

	if (status == RPM_SUSPENDED) {
		if (parent) {
			atomic_add_unless(&parent->power.child_count, -1, 0);
			notify_parent = !parent->power.ignore_children;
		}
		goto out_set;
	}

	if (parent) {
		spin_lock_nested(&parent->power.lock, SINGLE_DEPTH_NESTING);

		if (dev->power.runtime_status == RPM_SUSPENDED)
			atomic_inc(&parent->power.child_count);

		spin_unlock(&parent->power.lock);
	}

out_set:
	update_pm_runtime_accounting(dev);
	dev->power.runtime_status = status;

	spin_unlock(&dev->power.lock);

	if (notify_parent)
		pm_runtime_idle(parent);
}

static int exynos_pm_interrupt_runtime_suspend(struct device *dev, int flags)
{
	int retval;

	spin_lock(&dev->power.lock);
	if (flags & RPM_GET_PUT) {
		if (!atomic_dec_and_test(&dev->power.usage_count)) {
			spin_unlock(&dev->power.lock);
			return 0;
		}
	}

	retval = exynos_pm_interrupt_rpm_suspend(dev, flags);
	spin_unlock(&dev->power.lock);

	return retval;
}

static int exynos_pm_interrupt_runtime_idle(struct device *dev, int flags)
{
	int retval;

	spin_lock(&dev->power.lock);
	if (flags & RPM_GET_PUT) {
		if (!atomic_dec_and_test(&dev->power.usage_count)) {
			spin_unlock(&dev->power.lock);
			return 0;
		}
	}

	retval = exynos_pm_interrupt_rpm_idle(dev, flags);
	spin_unlock(&dev->power.lock);

	return retval;
}

static int exynos_pm_interrupt_runtime_resume(struct device *dev, int flags)
{
	int retval;

	spin_lock(&dev->power.lock);
	if (flags & RPM_GET_PUT)
		atomic_inc(&dev->power.usage_count);

	retval = exynos_pm_interrupt_rpm_resume(dev, flags);
	spin_unlock(&dev->power.lock);

	return retval;
}

static void exynos_pm_interrupt_unused_poweroff(void)
{
	struct generic_pm_domain *pd;

	spin_lock(&interrupt_domain_list_lock);

	list_for_each_entry(pd, &interrupt_domain_list, gpd_list_node)
		exynos_pm_interrupt_poweroff((struct exynos_pm_domain *)pd);

	spin_unlock(&interrupt_domain_list_lock);
}

int exynos_pm_interrupt_runtime_get_sync(struct device *dev)
{
	return exynos_pm_interrupt_runtime_resume(dev, RPM_GET_PUT);
}

int exynos_pm_interrupt_runtime_put_sync(struct device *dev)
{
	return exynos_pm_interrupt_runtime_idle(dev, RPM_GET_PUT);
}

void exynos_pm_interrupt_runtime_forbid(struct device *dev)
{
	spin_lock(&dev->power.lock);
	if (!dev->power.runtime_auto)
		goto out;

	dev->power.runtime_auto = false;
	atomic_inc(&dev->power.usage_count);
	exynos_pm_interrupt_rpm_resume(dev, 0);

out:
	spin_unlock(&dev->power.lock);
}

void exynos_pm_interrupt_runtime_allow(struct device *dev)
{
	spin_lock(&dev->power.lock);
	if (dev->power.runtime_auto)
		goto out;

	dev->power.runtime_auto = true;
	if (atomic_dec_and_test(&dev->power.usage_count))
		exynos_pm_interrupt_rpm_idle(dev, 0);
out:
	spin_unlock(&dev->power.lock);
}

static __init int exynos_pm_interrupt_domain_init(void)
{
	spin_lock_init(&interrupt_domain_list_lock);
	INIT_LIST_HEAD(&interrupt_domain_list);

	return 0;
}
core_initcall(exynos_pm_interrupt_domain_init);

static __init int exynos_pm_domain_idle(void)
{
	exynos_pm_interrupt_unused_poweroff();

	return 0;
}
late_initcall(exynos_pm_domain_idle);
