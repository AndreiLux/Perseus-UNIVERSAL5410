/*
 * OMAP5 CPU idle Routines
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clockchips.h>
#include <linux/spinlock.h>

#include <asm/proc-fns.h>

#include <mach/omap4-common.h>

#include "pm.h"
#include "prm.h"

#ifdef CONFIG_CPU_IDLE

/* Machine specific information to be recorded in the C-state driver_data */
struct omap5_idle_statedata {
	u32 cpu_state;
	u32 mpu_logic_state;
	u32 mpu_state;
	u8 mpu_state_vote;
	u8 valid;
};

static struct cpuidle_params cpuidle_params_table[] = {
	/* C1 - CPU0 ON + CPU1 ON + MPU ON */
	{.exit_latency = 2 + 2 , .target_residency = 5, .valid = 1},
	/* C2 - CPU0 INA + CPU1 INA + MPU INA */
	{.exit_latency = 10 + 10 , .target_residency = 10, .valid = 1},
	/* C3- CPU0 CSWR + CPU1 CSWR + MPU CSWR */
	{.exit_latency = 328 + 440 , .target_residency = 960, .valid = 1},
};

#define OMAP5_NUM_STATES ARRAY_SIZE(cpuidle_params_table)

struct omap5_idle_statedata omap5_idle_data[OMAP5_NUM_STATES];
static struct powerdomain *mpu_pd, *cpu_pd[NR_CPUS];
static DEFINE_RAW_SPINLOCK(mpuss_idle_lock);

/**
 * omap5_enter_idle - Programs OMAP5 to enter the specified state
 * @dev: cpuidle device
 * @state: The target state to be programmed
 *
 * Called from the CPUidle framework to program the device to the
 * specified low power state selected by the governor.
 * Returns the amount of time spent in the low power state.
 */
static int omap5_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_state *state)
{
	struct omap5_idle_statedata *cx = cpuidle_get_statedata(state);
	struct timespec ts_preidle, ts_postidle, ts_idle;
	int cpu_id = smp_processor_id();

	/* Used to keep track of the total time in idle */
	getnstimeofday(&ts_preidle);

	local_irq_disable();
	local_fiq_disable();

	if (state > &dev->states[0])
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);

	/*
	 * Call idle CPU PM enter notifier chain so that
	 * VFP and per CPU interrupt context is saved.
	 */
	if (cx->cpu_state == PWRDM_POWER_OFF)
		cpu_pm_enter();

	raw_spin_lock(&mpuss_idle_lock);
	cx->mpu_state_vote++;
	raw_spin_unlock(&mpuss_idle_lock);

	if (cx->mpu_state_vote == num_online_cpus()) {
		pwrdm_set_logic_retst(mpu_pd, cx->mpu_logic_state);
		omap_set_pwrdm_state(mpu_pd, cx->mpu_state);
	}

	/*
	 * Call idle CPU cluster PM enter notifier chain
	 * to save GIC and wakeupgen context.
	 */
	if ((cx->mpu_state == PWRDM_POWER_RET) &&
		(cx->mpu_logic_state == PWRDM_POWER_OFF))
			cpu_cluster_pm_enter();

	omap_enter_lowpower(dev->cpu, cx->cpu_state);

	/*
	 * Call idle CPU PM exit notifier chain to restore
	 * VFP and per CPU IRQ context. Only CPU0 state is
	 * considered since CPU1 is managed by CPU hotplug.
	 */
	if (pwrdm_read_prev_pwrst(cpu_pd[dev->cpu]) == PWRDM_POWER_OFF)
		cpu_pm_exit();

	raw_spin_lock(&mpuss_idle_lock);
	cx->mpu_state_vote--;
	raw_spin_unlock(&mpuss_idle_lock);

	/*
	 * Call idle CPU cluster PM exit notifier chain
	 * to restore GIC and wakeupgen context.
	 */
	if (omap_mpuss_read_prev_context_state())
		cpu_cluster_pm_exit();

	if (state > &dev->states[0])
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);

	getnstimeofday(&ts_postidle);
	ts_idle = timespec_sub(ts_postidle, ts_preidle);

	local_irq_enable();
	local_fiq_enable();

	return ts_idle.tv_nsec / NSEC_PER_USEC + ts_idle.tv_sec * USEC_PER_SEC;
}

DEFINE_PER_CPU(struct cpuidle_device, omap5_idle_dev);

struct cpuidle_driver omap5_idle_driver = {
	.name =		"omap5_idle",
	.owner =	THIS_MODULE,
};

static inline struct omap5_idle_statedata *_fill_cstate(
					struct cpuidle_device *dev,
					int idx, const char *descr)
{
	struct omap5_idle_statedata *cx;
	struct cpuidle_state *state;

	if (!cpuidle_params_table[idx].valid && idx)
		return NULL;

	cx = &omap5_idle_data[idx];
	state =  &dev->states[idx];
	state->exit_latency	= cpuidle_params_table[idx].exit_latency;
	state->target_residency	= cpuidle_params_table[idx].target_residency;
	state->flags		= CPUIDLE_FLAG_TIME_VALID;
	state->enter		= omap5_enter_idle;
	cx->valid		= cpuidle_params_table[idx].valid;
	sprintf(state->name, "C%d", idx + 1);
	strncpy(state->desc, descr, CPUIDLE_DESC_LEN);
	cpuidle_set_statedata(state, cx);

	return cx;
}

/**
 * omap5_idle_init - Init routine for OMAP5 idle
 *
 * Registers the OMAP5 specific cpuidle driver to the cpuidle
 * framework with the valid set of states.
 */
int __init omap5_idle_init(void)
{
	struct omap5_idle_statedata *cx;
	struct cpuidle_device *dev;
	unsigned int cpu_id = 0;

	mpu_pd = pwrdm_lookup("mpu_pwrdm");
	cpu_pd[0] = pwrdm_lookup("cpu0_pwrdm");
	cpu_pd[1] = pwrdm_lookup("cpu1_pwrdm");
	if ((!mpu_pd) || (!cpu_pd[0]) || (!cpu_pd[1]))
		return -ENODEV;

	cpuidle_register_driver(&omap5_idle_driver);

	for_each_cpu(cpu_id, cpu_online_mask) {
		dev = &per_cpu(omap5_idle_dev, cpu_id);
		dev->cpu = cpu_id;

		/* C1 - CPU0 ON + CPU1 ON + MPU ON */
		cx = _fill_cstate(dev, 0, "MPUSS ON");
		dev->safe_state = &dev->states[0];
		cx->valid = 1;	/* C1 is always valid */
		cx->cpu_state = PWRDM_POWER_ON;
		cx->mpu_state = PWRDM_POWER_ON;
		cx->mpu_state_vote = 0;
		cx->mpu_logic_state = PWRDM_POWER_RET;
		dev->state_count++;

		/* C2 - CPU0 INA + CPU1 INA + MPU INA */
		cx = _fill_cstate(dev, 1, "MPUSS INACTIVE");
		if (cx != NULL) {
			cx->cpu_state = PWRDM_POWER_INACTIVE;
			cx->mpu_state = PWRDM_POWER_INACTIVE;
			cx->mpu_state_vote = 0;
			cx->mpu_logic_state = PWRDM_POWER_RET;
			dev->state_count++;
		}

		/* C3 - CPU0 CSWR + CPU1 CSWR + MPU CSWR */
		cx = _fill_cstate(dev, 2, "MPUSS CSWR");
		if (cx != NULL) {
			cx->cpu_state = PWRDM_POWER_RET;
			cx->mpu_state = PWRDM_POWER_RET;
			cx->mpu_state_vote = 0;
			cx->mpu_logic_state = PWRDM_POWER_RET;
			dev->state_count++;
		}

		pr_debug("Register %d C-states on CPU%d\n",
						dev->state_count, cpu_id);
		if (cpuidle_register_device(dev)) {
			pr_err("%s: CPUidle registration failed\n", __func__);
			return -EIO;
		}
	}

	return 0;
}
#else
int __init omap5_idle_init(void)
{
	return 0;
}
#endif /* CONFIG_CPU_IDLE */
