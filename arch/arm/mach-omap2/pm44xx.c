/*
 * OMAP4 Power Management Routines
 *
 * Copyright (C) 2010-2011 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <asm/system_misc.h>
#include <linux/io.h>

#include <mach/ctrl_module_wkup_44xx.h>
#include <linux/interrupt.h>

#include <asm/system_misc.h> 

#include "common.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "prm44xx.h"
#include "prcm44xx.h"
#include "prm-regbits-44xx.h"
#include "prminst44xx.h"
#include "pm.h"

struct power_state {
	struct powerdomain *pwrdm;
	u32 next_state;
	u32 next_logic_state;
#ifdef CONFIG_SUSPEND
	u32 saved_state;
	u32 saved_logic_state;
#endif
	struct list_head node;
};

static LIST_HEAD(pwrst_list);
u16 pm44xx_errata;

#ifdef CONFIG_SUSPEND
static int omap4_pm_suspend(void)
{
	struct power_state *pwrst;
	int state, ret = 0, logic_state;
	u32 cpu_id = smp_processor_id();

	/* Save current powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->saved_state = pwrdm_read_next_pwrst(pwrst->pwrdm);
		pwrst->saved_logic_state = pwrdm_read_logic_retst(pwrst->pwrdm);
	}

	/* Set targeted power domain states by suspend */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		logic_state = PWRDM_POWER_RET;

#ifdef CONFIG_OMAP_ALLOW_OSWR
	/*OSWR is supported on silicon > ES2.0 */
		if ((pwrst->pwrdm->pwrsts_logic_ret == PWRSTS_OFF_RET)
					&& (omap_rev() >= OMAP4430_REV_ES2_1))
			logic_state = PWRDM_POWER_OFF;
#endif
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);

		// !!!

//		pwrdm_set_logic_retst(pwrst->pwrdm, pwrst->next_logic_state);
		pwrdm_set_logic_retst(pwrst->pwrdm, logic_state);
	}

	/*
	 * For MPUSS to hit power domain retention(CSWR or OSWR),
	 * CPU0 and CPU1 power domains need to be in OFF or DORMANT state,
	 * since CPU power domain CSWR is not supported by hardware
	 * Only master CPU follows suspend path. All other CPUs follow
	 * CPU hotplug path in system wide suspend. On OMAP4, CPU power
	 * domain CSWR is not supported by hardware.
	 * More details can be found in OMAP4430 TRM section 4.3.4.2.
	 */
	omap_enter_lowpower(cpu_id, PWRDM_POWER_OFF);

	/* Restore next powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		state = pwrdm_read_prev_pwrst(pwrst->pwrdm);
		if (state > pwrst->next_state) {
			pr_info("Powerdomain (%s) didn't enter "
			       "target state %d\n",
			       pwrst->pwrdm->name, pwrst->next_state);
			ret = -1;
		}
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->saved_state);
		pwrdm_set_logic_retst(pwrst->pwrdm, pwrst->saved_logic_state);
	}
	if (ret)
		pr_crit("Could not enter target state in pm_suspend\n");
	else
		pr_info("Successfully put all powerdomains to target state\n");

	return 0;
}
#endif /* CONFIG_SUSPEND */

/**
 * get_achievable_state() - Provide achievable state
 * @available_states:	what states are available
 * @req_min_state:	what state is the minimum we'd like to hit
 *
 * Power domains have varied capabilities. When attempting a low power
 * state such as OFF/RET, a specific min requested state may not be
 * supported on the power domain, in which case, the next higher power
 * state which is supported is returned. This is because a combination
 * of system power states where the parent PD's state is not in line
 * with expectation can result in system instabilities.
 */
static inline u8 get_achievable_state(u8 available_states, u8 req_min_state)
{
	u16 mask = 0xFF << req_min_state;

	if (available_states & mask)
		return __ffs(available_states & mask);
	return PWRDM_POWER_ON;
}

static int __init pwrdms_setup(struct powerdomain *pwrdm, void *unused)
{
	struct power_state *pwrst;

	if (!pwrdm->pwrsts)
		return 0;

	/*
	 * Skip CPU0 and CPU1 power domains. CPU1 is programmed
	 * through hotplug path and CPU0 explicitly programmed
	 * further down in the code path
	 */
	if (!strncmp(pwrdm->name, "cpu", 3))
		return 0;

	pwrst = kmalloc(sizeof(struct power_state), GFP_ATOMIC);
	if (!pwrst)
		return -ENOMEM;

	pwrst->pwrdm = pwrdm;
	pwrst->next_state = PWRDM_POWER_RET;
	pwrst->next_logic_state = PWRDM_POWER_RET;
	list_add(&pwrst->node, &pwrst_list);

	return omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
}

void omap4_pm_off_mode_enable(int enable)
{
	u32 next_state = PWRDM_POWER_RET;
	u32 next_logic_state = PWRDM_POWER_ON;
	struct power_state *pwrst;

	if (enable) {
		next_state = PWRDM_POWER_OFF;
		next_logic_state = PWRDM_POWER_OFF;
	}

	omap4_device_set_state_off(enable);

	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->next_state =
			get_achievable_state(pwrst->pwrdm->pwrsts, next_state);
		pwrst->next_logic_state =
			get_achievable_state(pwrst->pwrdm->pwrsts_logic_ret,
				next_logic_state);
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
		pwrdm_set_logic_retst(pwrst->pwrdm, pwrst->next_logic_state);
	}
}

void omap4_pm_oswr_mode_enable(int enable)
{
	u32 next_logic_state;
	struct power_state *pwrst;

	if (enable)
		next_logic_state = PWRDM_POWER_OFF;
	else
		next_logic_state = PWRDM_POWER_RET;

	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->next_logic_state = next_logic_state;
		pwrdm_set_logic_retst(pwrst->pwrdm, pwrst->next_logic_state);
	}
}

/**
 * omap_default_idle - OMAP4 default ilde routine.'
 *
 * Implements OMAP4 memory, IO ordering requirements which can't be addressed
 * with default cpu_do_idle() hook. Used by all CPUs with !CONFIG_CPUIDLE and
 * by secondary CPU with CONFIG_CPUIDLE.
 */
static void omap_default_idle(void)
{
	local_fiq_disable();

	omap_do_wfi();

	local_fiq_enable();
}

/**
 * omap4_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static inline int omap4_init_static_deps(void)
{
	int ret;
	struct clockdomain *emif_clkdm, *mpuss_clkdm, *l3_1_clkdm, *l4wkup;
	struct clockdomain *ducati_clkdm, *l3_2_clkdm, *l4_per_clkdm;

	if (!cpu_is_omap44xx())
		return -ENODEV;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	pr_err("Power Management for TI OMAP4.\n");

	/*
	 * Work around for OMAP443x Errata i632: "LPDDR2 Corruption After OFF
	 * Mode Transition When CS1 Is Used On EMIF":
	 * Overwrite EMIF1/EMIF2
	 * SECURE_EMIF1_SDRAM_CONFIG2_REG
	 * SECURE_EMIF2_SDRAM_CONFIG2_REG
	 */
	if (cpu_is_omap443x()) {
		void __iomem *secure_ctrl_mod;

		secure_ctrl_mod = ioremap(OMAP4_CTRL_MODULE_WKUP, SZ_4K);
		BUG_ON(!secure_ctrl_mod);

		__raw_writel(0x10, secure_ctrl_mod +
			     OMAP4_CTRL_SECURE_EMIF1_SDRAM_CONFIG2_REG);
		__raw_writel(0x10, secure_ctrl_mod +
			     OMAP4_CTRL_SECURE_EMIF2_SDRAM_CONFIG2_REG);
		wmb();
		iounmap(secure_ctrl_mod);
	}rch/arm/mach-omap2/pm44xx.c


	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains\n");
		goto err2;
	}

	/*
	 * The dynamic dependency between MPUSS -> MEMIF and
	 * MPUSS -> L4_PER/L3_* and DUCATI -> L3_* doesn't work as
	 * expected. The hardware recommendation is to enable static
	 * dependencies for these to avoid system lock ups or random crashes.
	 * The L4 wakeup depedency is added to workaround the OCP sync hardware
	 * BUG with 32K synctimer which lead to incorrect timer value read
	 * from the 32K counter. The BUG applies for GPTIMER1 and WDT2 which
	 * are part of L4 wakeup clockdomain.
	 */
	mpuss_clkdm = clkdm_lookup("mpuss_clkdm");
	emif_clkdm = clkdm_lookup("l3_emif_clkdm");
	l3_1_clkdm = clkdm_lookup("l3_1_clkdm");
	l3_2_clkdm = clkdm_lookup("l3_2_clkdm");
	l4_per_clkdm = clkdm_lookup("l4_per_clkdm");
	l4wkup = clkdm_lookup("l4_wkup_clkdm");
	ducati_clkdm = clkdm_lookup("ducati_clkdm");
	if ((!mpuss_clkdm) || (!emif_clkdm) || (!l3_1_clkdm) || (!l4wkup) ||
		(!l3_2_clkdm) || (!ducati_clkdm) || (!l4_per_clkdm))
		return -EINVAL;

	ret = clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l3_1_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l3_2_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l4_per_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l4wkup);
	ret |= clkdm_add_wkdep(ducati_clkdm, l3_1_clkdm);
	ret |= clkdm_add_wkdep(ducati_clkdm, l3_2_clkdm);
	if (ret) {
		pr_err("Failed to add MPUSS -> L3/EMIF/L4PER, DUCATI -> L3 "
				"wakeup dependency\n");
	}

	return ret;
}

static inline int omap5_init_static_deps(void)
{
	struct clockdomain *mpuss_clkdm, *emif_clkdm, *l4per_clkdm;
	int ret;

	/*
	 * The dynamic dependency between MPUSS -> EMIF/L4PER
	 * doesn't work as expected. The hardware recommendation is to
	 * enable static dependencies for these to avoid system
	 * lock ups or random crashes.
	 */
	mpuss_clkdm = clkdm_lookup("mpu_clkdm");
	emif_clkdm = clkdm_lookup("emif_clkdm");
	l4per_clkdm = clkdm_lookup("l4per_clkdm");
	if (!mpuss_clkdm || !emif_clkdm || ! l4per_clkdm)
		return -EINVAL;

	ret = clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l4per_clkdm);
	if (ret)
		pr_err("Failed to add MPUSS -> L4PER/EMIF wakeup dependency\n");

	return ret;
}

static irqreturn_t prcm_interrupt_handler (int irq, void *dev_id)
{
u32 irqenable_mpu, irqstatus_mpu;

	irqenable_mpu = omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
							OMAP4_PRM_IRQENABLE_MPU_OFFSET);
	irqstatus_mpu = omap4_prm_read_inst_reg(OMAP4430_PRM_OCP_SOCKET_INST,
							OMAP4_PRM_IRQSTATUS_MPU_OFFSET);

	/* Check if a IO_ST interrupt */
	if (irqstatus_mpu & OMAP4430_IO_ST_MASK) {
		/* Re-enable UART3 */
		omap_trigger_wuclk_ctrl();
	}

	return IRQ_HANDLED;
}

/**
 * omap_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static int __init omap_pm_init(void)
{
	int ret;

	if (!(cpu_is_omap44xx() || cpu_is_omap54xx()))
		return -ENODEV;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	pr_info("Power Management for TI OMAP4XX/OMAP5XXX devices.\n");

	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains\n");
		goto err2;
	}

#ifdef CONFIG_PM
		/* Enable GLOBAL_WUEN */
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_GLOBAL_WUEN_MASK, OMAP4430_GLOBAL_WUEN_MASK,
			OMAP4430_PRM_PARTITION, OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_IO_PMCTRL_OFFSET);

		ret = request_irq(omap_prcm_event_to_irq("io"),
						(irq_handler_t)prcm_interrupt_handler,
						IRQF_SHARED | IRQF_NO_SUSPEND, "pm_io",
						omap_pm_init);
		if (ret) {
				printk(KERN_ERR "request_irq failed to register for pm_io\n");
				goto err2;
		}

#endif

	if (cpu_is_omap44xx())
		ret = omap4_init_static_deps();
	else
		ret = omap5_init_static_deps();

	if (ret) {
		pr_err("Failed to initialise static depedencies\n");
		goto err2;
	}

	ret = omap_mpuss_init();
	if (ret) {
		pr_err("Failed to initialise OMAP4 MPUSS\n");
		goto err2;
	}

	(void) clkdm_for_each(omap_pm_clkdms_setup, NULL);

	/*
	 * ROM code initializes IVAHD and TESLA clock registers during
	 * secure RAM restore phase on omap4430 EMU/HS devices, thus
	 * IVAHD / TESLA clock registers must be saved / restored
	 * during MPU OSWR / device off.
	 */
	if (cpu_is_omap443x() && omap_type() != OMAP2_DEVICE_TYPE_GP)
		pm44xx_errata |= PM_OMAP4_ROM_IVAHD_TESLA_ERRATUM;

	/*
	 * Similar to above errata, ROM code modifies L3INSTR clock
	 * registers also and these must be saved / restored during
	 * MPU OSWR / device off.
	 */
	if (omap_type() != OMAP2_DEVICE_TYPE_GP)
		pm44xx_errata |= PM_OMAP4_ROM_L3INSTR_ERRATUM;

#ifdef CONFIG_SUSPEND
	omap_pm_suspend = omap4_pm_suspend;
#endif

	/* Overwrite the default cpu_do_idle() */
	arm_pm_idle = omap_default_idle;

	if (cpu_is_omap44xx()) {
		/* Overwrite the default arch_idle() */
		pm_idle = omap_default_idle;
		omap4_idle_init();
	} else if (cpu_is_omap54xx()) {
		omap5_idle_init();
	}

err2:
	return ret;
}
late_initcall(omap4_pm_init);
