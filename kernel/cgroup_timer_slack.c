/*
 * cgroup_timer_slack.c - control group timer slack subsystem
 *
 * Copyright Nokia Corparation, 2011
 * Author: Kirill A. Shutemov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/cgroup.h>
#include <linux/slab.h>
#include <linux/err.h>
#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
#include <linux/earlysuspend.h>
#endif

struct cgroup_subsys timer_slack_subsys;
struct tslack_cgroup {
	struct cgroup_subsys_state css;
	unsigned long min_slack_ns;
#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
	unsigned long min_slack_suspend_ns;
#endif
};

#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
bool is_early_suspend_registered = false;
bool is_system_active = true;
#endif

static struct tslack_cgroup *cgroup_to_tslack(struct cgroup *cgroup)
{
	struct cgroup_subsys_state *css;

	css = cgroup_subsys_state(cgroup, timer_slack_subsys.subsys_id);
	return container_of(css, struct tslack_cgroup, css);
}

#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
/*
 * Sets the status for suspended system
 */
static void tslack_early_suspend(struct early_suspend *handler)
{
	is_system_active = false;
}

/*
 * Sets the status for active system
 */
static void tslack_late_resume(struct early_suspend *handler)
{
	is_system_active = true;
}

/*
 * Struct for the timer slack management during suspend/resume
 */
static struct early_suspend tslack_suspend = {
	.suspend = tslack_early_suspend,
	.resume = tslack_late_resume,
};
#endif

static struct cgroup_subsys_state *tslack_create(struct cgroup *cgroup)
{
	struct tslack_cgroup *tslack_cgroup;

#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
	/* Register the timer slack management during suspend/resume */
	if (!is_early_suspend_registered) {
		is_early_suspend_registered = true;
		register_early_suspend(&tslack_suspend);
	}
#endif

	tslack_cgroup = kmalloc(sizeof(*tslack_cgroup), GFP_KERNEL);
	if (!tslack_cgroup)
		return ERR_PTR(-ENOMEM);

	if (cgroup->parent) {
		struct tslack_cgroup *parent;

		parent = cgroup_to_tslack(cgroup->parent);
		tslack_cgroup->min_slack_ns = parent->min_slack_ns;
#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
		tslack_cgroup->min_slack_suspend_ns = parent->min_slack_suspend_ns;
#endif
	} else {
		tslack_cgroup->min_slack_ns = 0UL;
#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
		tslack_cgroup->min_slack_suspend_ns = 0UL;
#endif
	}

	return &tslack_cgroup->css;
}

static void tslack_destroy(struct cgroup *cgroup)
{
	kfree(cgroup_to_tslack(cgroup));
}

static u64 tslack_read_min(struct cgroup *cgroup, struct cftype *cft)
{
	return cgroup_to_tslack(cgroup)->min_slack_ns;
}

static int tslack_write_min(struct cgroup *cgroup, struct cftype *cft, u64 val)
{
	if (val > ULONG_MAX)
		return -EINVAL;

	cgroup_to_tslack(cgroup)->min_slack_ns = val;

	return 0;
}

#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
/*
 * Gets the minimal timer slack value for suspended system
 */
static u64 tslack_read_min_suspend(struct cgroup *cgroup, struct cftype *cft)
{
	return cgroup_to_tslack(cgroup)->min_slack_suspend_ns;
}

/*
 * Sets the minimal timer slack value for suspended system
 */
static int tslack_write_min_suspend(struct cgroup *cgroup, struct cftype *cft, u64 val)
{
	if (val > ULONG_MAX)
		return -EINVAL;

	cgroup_to_tslack(cgroup)->min_slack_suspend_ns = val;

	return 0;
}

/*
 * Gets the minimal timer slack value according to the system status (active/suspended)
 */
static unsigned long tslack_get_dynamic_min(struct cgroup *cgroup)
{
	return (is_system_active) ?
				cgroup_to_tslack(cgroup)->min_slack_ns :
				cgroup_to_tslack(cgroup)->min_slack_suspend_ns;
}
#endif

static u64 tslack_read_effective(struct cgroup *cgroup, struct cftype *cft)
{
	unsigned long min;

#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
	min = tslack_get_dynamic_min(cgroup);
#else
	min = cgroup_to_tslack(cgroup)->min_slack_ns;
#endif

	while (cgroup->parent) {
		cgroup = cgroup->parent;
#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
		min = max(tslack_get_dynamic_min(cgroup), min);
#else
		min = max(cgroup_to_tslack(cgroup)->min_slack_ns, min);
#endif
	}

	return min;
}

static struct cftype files[] = {
	{
		.name = "min_slack_ns",
		.read_u64 = tslack_read_min,
		.write_u64 = tslack_write_min,
	},
#ifdef CONFIG_CGROUP_DYNAMIC_TIMER_SLACK
	{
		.name = "min_slack_suspend_ns",
		.read_u64 = tslack_read_min_suspend,
		.write_u64 = tslack_write_min_suspend,
	},
#endif
	{
		.name = "effective_slack_ns",
		.read_u64 = tslack_read_effective,
	},
};

static int tslack_populate(struct cgroup_subsys *subsys, struct cgroup *cgroup)
{
	return cgroup_add_files(cgroup, subsys, files, ARRAY_SIZE(files));
}

static int tslack_allow_attach(struct cgroup *cgrp, struct cgroup_taskset *tset)
{
	const struct cred *cred = current_cred(), *tcred;
	struct task_struct *task;

	cgroup_taskset_for_each(task, cgrp, tset) {
		tcred = __task_cred(task);

		if ((current != task) && !capable(CAP_SYS_NICE) &&
		    cred->euid != tcred->uid && cred->euid != tcred->suid)
			return -EACCES;
	}

	return 0;
}

struct cgroup_subsys timer_slack_subsys = {
	.name		= "timer_slack",
	.subsys_id	= timer_slack_subsys_id,
	.create		= tslack_create,
	.destroy	= tslack_destroy,
	.populate	= tslack_populate,
	.allow_attach	= tslack_allow_attach,
};

unsigned long task_get_effective_timer_slack(struct task_struct *tsk)
{
	struct cgroup *cgroup;
	unsigned long slack;

	rcu_read_lock();
	cgroup = task_cgroup(tsk, timer_slack_subsys.subsys_id);
	slack = tslack_read_effective(cgroup, NULL);
	rcu_read_unlock();

	return max(tsk->timer_slack_ns, slack);
}
