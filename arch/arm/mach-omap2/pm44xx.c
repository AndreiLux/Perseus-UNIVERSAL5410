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
#include <linux/io.h>

#include <asm/system_misc.h> 

#include "common.h"
#include <plat/omap_hwmod.h>

#include "powerdomain.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "prm44xx.h"
#include "prcm44xx.h"
#include "prm-regbits-44xx.h"
#include "prminst44xx.h"
#include "pm.h"
#include "voltage.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"
#include "cm2_54xx.h"
#include "cm44xx.h"
#include "prm54xx.h"
#include "dvfs.h"

static const char * const autoidle_hwmods[] = {
	"gpio2",
	"gpio3",
	"gpio4",
	"gpio5",
	"gpio6",
	"kbd",
	"timer1",
	"emif1",
	"emif2",
	"gpmc",
};

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

static struct powerdomain *mpu_pwrdm, *core_pwrdm, *per_pwrdm;

#ifdef CONFIG_SUSPEND
/**
 * omap4_restore_pwdms_after_suspend() - Restore powerdomains after suspend
 *
 * Re-program all powerdomains to saved power domain states.
 *
 * returns 0 if all power domains hit targeted power state, -1 if any domain
 * failed to hit targeted power state (status related to the actual restore
 * is not returned).
 */
static int omap4_restore_pwdms_after_suspend(void)
{
	struct power_state *pwrst;
	int cstate, pstate, ret = 0;

	/* Restore next powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		cstate = pwrdm_read_pwrst(pwrst->pwrdm);
		pstate = pwrdm_read_prev_pwrst(pwrst->pwrdm);
		if (pstate > pwrst->next_state) {
			pr_info("Powerdomain (%s) didn't enter "
				"target state %d Vs achieved state %d.\n"
				"current state %d\n",
				pwrst->pwrdm->name, pwrst->next_state,
				pstate, cstate);
			ret = -1;
		}

		/* If state already ON due to h/w dep, don't do anything */
		if (cstate == PWRDM_POWER_ON)
			continue;

		/* If we have already achieved saved state, nothing to do */
		if (cstate == pwrst->saved_state)
			continue;

		/*
		 * Skip pd program if saved state higher than current state
		 * Since we would have already returned if the state
		 * was ON, if the current state is yet another low power
		 * state, the PRCM specification clearly states that
		 * transition from a lower LP state to a higher LP state
		 * is forbidden.
		 */
		if (pwrst->saved_state > cstate)
			continue;

		if (pwrst->pwrdm->pwrsts)
			omap_set_pwrdm_state(pwrst->pwrdm, pwrst->saved_state);

		if (pwrst->pwrdm->pwrsts_logic_ret)
			pwrdm_set_logic_retst(pwrst->pwrdm,
						pwrst->saved_logic_state);
	}

	return ret;
}

static int omap4_pm_suspend(void)
{
	struct power_state *pwrst;
	int ret = 0;
	u32 cpu_id = smp_processor_id();

	/*
	 * If any device was in the middle of a scale operation
	 * then abort, as we cannot predict which part of the scale
	 * operation we interrupted.
	 */
	if (omap_dvfs_is_any_dev_scaling()) {
		pr_err("%s: oops.. middle of scale op.. aborting suspend\n",
			__func__);
		return -EBUSY;
	}

	/* Save current powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->saved_state = pwrdm_read_next_pwrst(pwrst->pwrdm);
		pwrst->saved_logic_state = pwrdm_read_logic_retst(pwrst->pwrdm);
	}

	/* Set targeted power domain states by suspend */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
		pwrdm_set_logic_retst(pwrst->pwrdm, pwrst->next_logic_state);
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

	ret = omap4_restore_pwdms_after_suspend();

	if (ret)
		pr_crit("Could not enter target state in pm_suspend\n");
	else
		pr_info("Successfully put all powerdomains to target state\n");

	return 0;
}
#endif /* CONFIG_SUSPEND */

/**
 * omap4_pm_cold_reset() - Cold reset OMAP4
 * @reason:	why am I resetting.
 *
 * As per the TRM, it is recommended that we set all the power domains to
 * ON state before we trigger cold reset.
 */
int omap4_pm_cold_reset(char *reason)
{
	struct power_state *pwrst;

	/* Switch ON all pwrst registers */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		if (pwrst->pwrdm->pwrsts_logic_ret)
			pwrdm_set_logic_retst(pwrst->pwrdm, PWRDM_POWER_ON);
		if (pwrst->pwrdm->pwrsts)
			omap_set_pwrdm_state(pwrst->pwrdm, PWRDM_POWER_ON);
	}

	WARN(1, "Arch Cold reset has been triggered due to %s\n", reason);
	omap4_prminst_global_cold_sw_reset(); /* never returns */

	/* If we reached here - something bad went on.. */
	BUG();

	/* make the compiler happy */
	return -EINTR;
}

/**
 * get_achievable_state() - Provide achievable state
 * @available_states:	what states are available
 * @req_min_state:	what state is the minimum we'd like to hit
 * @is_parent_pd:	is this a parent power domain?
 *
 * Power domains have varied capabilities. When attempting a low power
 * state such as OFF/RET, a specific min requested state may not be
 * supported on the power domain, in which case:
 * a) if this power domain is a parent power domain, we do not intend
 * for it to go to a lower power state(because we are not targetting it),
 * select the next higher power state which is supported is returned.
 * b) However, for all children power domains, we first try to match
 * with a lower power domain state before attempting a higher state.
 * This is because a combination of system power states where the
 * parent PD's state is not in line with expectation can result in
 * system instabilities.
 */
static inline u8 get_achievable_state(u8 available_states, u8 req_min_state,
					bool is_parent_pd)
{
	u8 max_mask = 0xFF << req_min_state;
	u8 min_mask = ~max_mask;

	/* First see if we have an accurate match */
	if (available_states & BIT(req_min_state))
		return req_min_state;

	/* See if a lower power state is possible on this child domain */
	if (!is_parent_pd && available_states & min_mask)
		return __ffs(available_states & min_mask);

	if (available_states & max_mask)
		return __ffs(available_states & max_mask);

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

#if 0
/*
 * Enable hardware supervised mode for all clockdomains if it's
 * supported. Initiate sleep transition for other clockdomains, if
 * they are not used
 */
static int __init clkdms_setup(struct clockdomain *clkdm, void *unused)
{
	if (clkdm->flags & CLKDM_CAN_ENABLE_AUTO)
		clkdm_allow_idle(clkdm);
	else if (clkdm->flags & CLKDM_CAN_FORCE_SLEEP &&
			atomic_read(&clkdm->usecount) == 0)
		clkdm_sleep(clkdm);
	return 0;
}
#endif

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
		bool parent_power_domain = false;

		if (!strcmp(pwrst->pwrdm->name, "core_pwrdm") ||
		    !strcmp(pwrst->pwrdm->name, "mpu_pwrdm") ||
		    !strcmp(pwrst->pwrdm->name, "ivahd_pwrdm"))
			parent_power_domain = true;

		pwrst->next_state =
			get_achievable_state(pwrst->pwrdm->pwrsts, next_state,
				parent_power_domain);
		pwrst->next_logic_state =
			get_achievable_state(pwrst->pwrdm->pwrsts_logic_ret,
				next_logic_state, parent_power_domain);
		pr_debug("%s: %20s - next state / next logic state: %d (%d) / %d (%d)\n",
			__func__, pwrst->pwrdm->name, pwrst->next_state,
			pwrst->saved_state, pwrst->next_logic_state,
						pwrst->saved_logic_state);

		if (pwrst->pwrdm->pwrsts &&
		    pwrst->next_state < pwrst->saved_state)
			omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
		if (pwrst->pwrdm->pwrsts_logic_ret &&
		    pwrst->next_logic_state < pwrst->saved_logic_state)
			pwrdm_set_logic_retst(pwrst->pwrdm,
				pwrst->next_logic_state);
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

static void __init prcm_setup_regs(void)
{
#if defined(CONFIG_MACH_OMAP_5430ZEBU)
	struct omap_hwmod *oh;
#endif

#ifdef CONFIG_MACH_OMAP_5430ZEBU
	struct omap_hwmod *oh;  
	/* Idle gpmc */
	oh = omap_hwmod_lookup("gpmc");
	omap_hwmod_idle(oh);

	/* Idle OCP_WP_NOC */
	__raw_writel(0, OMAP54XX_CM_L3INSTR_OCP_WP_NOC_CLKCTRL);

	/* Idle hdq */
	__raw_writel(0, OMAP54XX_CM_L4PER_HDQ1W_CLKCTRL);
#endif

	/*
	 * Errata ID: i608 Impacted OMAP4430 ES 1.0,2.0,2.1,2.2
	 * On OMAP4, Retention-Till-Access Memory feature is not working
	 * reliably and hardware recommondation is keep it disabled by
	 * default
	 */
	if (cpu_is_omap443x()) {
	 omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_PARTITION, OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_SRAM_WKUP_SETUP_OFFSET);
	 omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_PARTITION, OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_LDO_SRAM_CORE_SETUP_OFFSET);
	 omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_PARTITION, OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_LDO_SRAM_MPU_SETUP_OFFSET);
	 omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
		0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
		OMAP4430_PRM_PARTITION, OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_LDO_SRAM_IVA_SETUP_OFFSET);
	}

	/* Toggle CLKREQ in RET and OFF states */
	omap4_prminst_write_inst_reg(0x2, OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_CLKREQCTRL_OFFSET);
	/*
	 * De-assert PWRREQ signal in Device OFF state
	 *      0x3: PWRREQ is de-asserted if all voltage domain are in
	 *      OFF state. Conversely, PWRREQ is asserted upon any
	 *      voltage domain entering or staying in ON or SLEEP or
	 *      RET state.
	 */
	omap4_prminst_write_inst_reg(0x3, OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_DEVICE_INST, OMAP4_PRM_PWRREQCTRL_OFFSET);

#ifdef CONFIG_OMAP_PM_STANDALONE
	/*
	 * XXX: Enable MM autoret on omap5 by default
	 * Should be removed once MM domain is used properly
	 */
	if (cpu_is_omap54xx()) {
		u32 val;

		val = omap4_prminst_read_inst_reg(OMAP54XX_PRM_PARTITION,
			OMAP54XX_PRM_DEVICE_INST, OMAP54XX_PRM_VOLTCTRL_OFFSET);
		val |= 0x2 << 4;
		omap4_prminst_write_inst_reg(val, OMAP54XX_PRM_PARTITION,
			OMAP54XX_PRM_DEVICE_INST, OMAP54XX_PRM_VOLTCTRL_OFFSET);
	}
#endif
}

/**
 * omap_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static int __init omap_pm_init(void)
{
	int ret, i;
#ifdef CONFIG_SUSPEND
	struct power_state *pwrst;
#endif

	if (!(cpu_is_omap44xx() || cpu_is_omap54xx()))
		return -ENODEV;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	pr_info("Power Management for TI OMAP4XX/OMAP5XXX devices.\n");

	prcm_setup_regs();

	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains\n");
		goto err2;
	}

	if (cpu_is_omap44xx())
		ret = omap4_init_static_deps();
	else
		ret = omap5_init_static_deps();

	if (ret) {
		pr_err("Failed to initialise static depedencies\n");
		goto err2;
	}

	for (i = 0; i < ARRAY_SIZE(autoidle_hwmods); i++) {
		struct omap_hwmod *oh;

		oh = omap_hwmod_lookup(autoidle_hwmods[i]);
		if (oh)
			omap_hwmod_disable_clkdm_usecounting(oh);
		else
			pr_warning("hwmod %s not found\n", autoidle_hwmods[i]);
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

	mpu_pwrdm = pwrdm_lookup("mpu_pwrdm");
	core_pwrdm = pwrdm_lookup("core_pwrdm");
	per_pwrdm = pwrdm_lookup("l4per_pwrdm");

	if (cpu_is_omap44xx()) {
		omap4_idle_init();
	} else if (cpu_is_omap54xx()) {
		omap5_idle_init();
	}

#ifdef CONFIG_SUSPEND
	/* Initialize current powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->saved_state = pwrdm_read_next_pwrst(pwrst->pwrdm);
		pwrst->saved_logic_state = pwrdm_read_logic_retst(pwrst->pwrdm);
	}

	/* Enable device OFF mode */
	if (cpu_is_omap44xx())
		omap4_pm_off_mode_enable(1);
#endif
err2:
	return ret;
}
late_initcall(omap_pm_init);
