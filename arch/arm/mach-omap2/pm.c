/*
 * pm.c - Common OMAP2+ power management-related code
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Copyright (C) 2010 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/opp.h>
#include <linux/export.h>
#include <linux/suspend.h>

#include <asm/system_misc.h>

#include "common.h"
#include <linux/pm_qos.h>

#include <plat/omap-pm.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/common.h>

#include "prcm-common.h"
#include "voltage.h"
#include "powerdomain.h"
#include "clockdomain.h"
#include "pm.h"
#include "twl-common.h"
#include "prm2xxx_3xxx.h"
#include "prm44xx.h"
#include "dvfs.h"

static struct omap_device_pm_latency *pm_lats;
static void (*io_chain_trigger_func) (void);
static DEFINE_SPINLOCK(io_chain_lock);

/*
 * omap_pm_suspend: points to a function that does the SoC-specific
 * suspend work
 */
int (*omap_pm_suspend)(void);

static int __init _init_omap_device(char *name)
static struct device *mpu_dev;
static struct device *iva_dev;
static struct device *l3_dev;
static struct device *dsp_dev;

bool omap_pm_is_ready_status;

struct device *omap2_get_mpuss_device(void)
{
	WARN_ON_ONCE(!mpu_dev);
	return mpu_dev;
}

struct device *omap2_get_iva_device(void)
{
	WARN_ON_ONCE(!iva_dev);
	return iva_dev;
}

struct device *omap2_get_l3_device(void)
{
	WARN_ON_ONCE(!l3_dev);
	return l3_dev;
}

struct device *omap4_get_dsp_device(void)
{
	WARN_ON_ONCE(!dsp_dev);
	return dsp_dev;
}
EXPORT_SYMBOL(omap4_get_dsp_device);

static int _init_omap_device(char *name)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup(name);
	if (WARN(!oh, "%s: could not find omap_hwmod for %s\n",
		 __func__, name))
		return -ENODEV;

	pdev = omap_device_build(oh->name, 0, oh, NULL, 0, pm_lats, 0, false);
	if (WARN(IS_ERR(pdev), "%s: could not build omap_device for %s\n",
		 __func__, name))
		return -ENODEV;

	return 0;
}

/*
 * Build omap_devices for processors and bus.
 */
static void __init omap2_init_processor_devices(void)
{
	_init_omap_device("mpu");
	if (omap3_has_iva())
		_init_omap_device("iva");

	if (cpu_is_omap44xx() || cpu_is_omap54xx()) {
		_init_omap_device("l3_main_1");
		_init_omap_device("dsp");
		_init_omap_device("iva");
	} else {
		_init_omap_device("l3_main");
	}
}

void omap_trigger_io_chain(void)
{
	unsigned long flags;

	if (io_chain_trigger_func) {
		spin_lock_irqsave(&io_chain_lock, flags);
		io_chain_trigger_func();
		spin_unlock_irqrestore(&io_chain_lock, flags);
	}
}

/* Types of sleep_switch used in omap_set_pwrdm_state */
#define FORCEWAKEUP_SWITCH	0
#define LOWPOWERSTATE_SWITCH	1

int __init omap_pm_clkdms_setup(struct clockdomain *clkdm, void *unused)
{
	if (clkdm->flags & CLKDM_CAN_ENABLE_AUTO)
		clkdm_allow_idle(clkdm);
	else if (clkdm->flags & CLKDM_CAN_FORCE_SLEEP &&
		 atomic_read(&clkdm->usecount) == 0)
		clkdm_sleep(clkdm);
	return 0;
}

/*
 * This sets pwrdm state (other than mpu & core. Currently only ON &
 * RET are supported.
 */
int omap_set_pwrdm_state(struct powerdomain *pwrdm, u32 pwrst)
{
	u8 curr_pwrst, next_pwrst;
	int sleep_switch = -1, ret = 0, hwsup = 0;

	if (!pwrdm || IS_ERR(pwrdm))
		return -EINVAL;

	while (!(pwrdm->pwrsts & (1 << pwrst))) {
		if (pwrst == PWRDM_POWER_OFF)
			return ret;
		pwrst--;
	}

	next_pwrst = pwrdm_read_next_pwrst(pwrdm);
	if (next_pwrst == pwrst)
		return ret;

	curr_pwrst = pwrdm_read_pwrst(pwrdm);
	if (curr_pwrst < PWRDM_POWER_ON) {
		if ((curr_pwrst > pwrst) &&
			(pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE)) {
			sleep_switch = LOWPOWERSTATE_SWITCH;
		} else {
			hwsup = clkdm_in_hwsup(pwrdm->pwrdm_clkdms[0]);
			clkdm_wakeup(pwrdm->pwrdm_clkdms[0]);
			sleep_switch = FORCEWAKEUP_SWITCH;
		}
	}

	ret = pwrdm_set_next_pwrst(pwrdm, pwrst);
	if (ret)
		pr_err("%s: unable to set power state of powerdomain: %s\n",
		       __func__, pwrdm->name);

	switch (sleep_switch) {
	case FORCEWAKEUP_SWITCH:
		if (hwsup)
			clkdm_allow_idle(pwrdm->pwrdm_clkdms[0]);
		else
			clkdm_sleep(pwrdm->pwrdm_clkdms[0]);
		break;
	case LOWPOWERSTATE_SWITCH:
		pwrdm_set_lowpwrstchange(pwrdm);
		pwrdm_wait_transition(pwrdm);
		pwrdm_state_switch(pwrdm);
		break;
	}

	return ret;
}



/*
 * This API is to be called during init to set the various voltage
 * domains to the voltage as per the opp table. Typically we boot up
 * at the nominal voltage. So this function finds out the rate of
 * the clock associated with the voltage domain, finds out the correct
 * opp entry and sets the voltage domain to the voltage specified
 * in the opp entry
 */
static int __init omap2_set_init_voltage(char *vdd_name, char *clk_name,
					 const char *oh_name)
{
	struct voltagedomain *voltdm;
	struct clk *clk;
	struct opp *opp;
	struct device *dev;
	unsigned long freq_cur, freq_valid, bootup_volt;
	int ret = -EINVAL;

	if (!vdd_name || !clk_name || !oh_name) {
		pr_err("%s: invalid parameters\n", __func__);
		goto exit;
	}

	dev = omap_device_get_by_hwmod_name(oh_name);
	if (IS_ERR(dev)) {
		pr_err("%s: Unable to get dev pointer for hwmod %s\n",
			__func__, oh_name);
		goto exit;
	}

	voltdm = voltdm_lookup(vdd_name);
	if (IS_ERR(voltdm)) {
		pr_err("%s: unable to get vdd pointer for vdd_%s\n",
			__func__, vdd_name);
		goto exit;
	}

	clk =  clk_get(NULL, clk_name);
	if (IS_ERR(clk)) {
		pr_err("%s: unable to get clk %s\n", __func__, clk_name);
		goto exit;
	}

	freq_cur = clk->rate;
	freq_valid = freq_cur;

	rcu_read_lock();
	opp = opp_find_freq_ceil(dev, &freq_valid);
	if (IS_ERR(opp)) {
		opp = opp_find_freq_floor(dev, &freq_valid);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			pr_err("%s: no boot OPP match for %ld on vdd_%s\n",
				__func__, freq_cur, vdd_name);
			ret = -ENOENT;
			goto exit_ck;
		}
	}

	bootup_volt = opp_get_voltage(opp);
	rcu_read_unlock();
	if (!bootup_volt) {
		pr_err("%s: unable to find voltage corresponding "
			"to the bootup OPP for vdd_%s\n", __func__, vdd_name);
		ret = -ENOENT;
		goto exit_ck;
	}

	/*
	 * Frequency and Voltage have to be sequenced: if we move from
	 * a lower frequency to higher frequency, raise voltage, followed by
	 * frequency, and vice versa. we assume that the voltage at boot
	 * is the required voltage for the frequency it was set for.
	 * NOTE:
	 * we can check the frequency, but there is numerous ways to set
	 * voltage. We play the safe path and just set the voltage.
	 */

	if (freq_cur < freq_valid) {
		ret = voltdm_scale(voltdm,
			omap_voltage_get_voltdata(voltdm, bootup_volt));
		if (ret) {
			pr_err("%s: Fail set voltage-%s(f=%ld v=%ld)on vdd%s\n",
				__func__, vdd_name, freq_valid,
				bootup_volt, vdd_name);
			goto exit_ck;
		}
	}

	/* Set freq only if there is a difference in freq */
	if (freq_valid != freq_cur) {
		ret = clk_set_rate(clk, freq_valid);
		if (ret) {
			pr_err("%s: Fail set clk-%s(f=%ld v=%ld)on vdd%s\n",
				__func__, clk_name, freq_valid,
				bootup_volt, vdd_name);
			goto exit_ck;
		}
	}

	if (freq_cur >= freq_valid) {
		ret = voltdm_scale(voltdm,
			omap_voltage_get_voltdata(voltdm, bootup_volt));
		if (ret) {
			pr_err("%s: Fail set voltage-%s(f=%ld v=%ld)on vdd%s\n",
				__func__, clk_name, freq_valid,
				bootup_volt, vdd_name);
			goto exit_ck;
		}
	}

	ret = 0;
exit_ck:
	clk_put(clk);

	if (!ret)
		return 0;

exit:
	pr_err("%s: unable to set vdd_%s\n", __func__, vdd_name);
	return -EINVAL;
}

#ifdef CONFIG_SUSPEND
static int omap_pm_enter(suspend_state_t suspend_state)
{
	int ret = 0;

	if (!omap_pm_suspend)
		return -ENOENT; /* XXX doublecheck */

	switch (suspend_state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = omap_pm_suspend();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int omap_pm_begin(suspend_state_t state)
{
	disable_hlt();
	if (cpu_is_omap34xx())
		omap_prcm_irq_prepare();
	return 0;
}

static void omap_pm_end(void)
{
	enable_hlt();
	return;
}

static void omap_pm_finish(void)
{
	if (cpu_is_omap34xx())
		omap_prcm_irq_complete();
}

static const struct platform_suspend_ops omap_pm_ops = {
	.begin		= omap_pm_begin,
	.end		= omap_pm_end,
	.enter		= omap_pm_enter,
	.finish		= omap_pm_finish,
	.valid		= suspend_valid_only_mem,
};

#endif /* CONFIG_SUSPEND */

static void __init omap3_init_voltages(void)
{
	if (!cpu_is_omap34xx())
		return;

	omap2_set_init_voltage("mpu_iva", "dpll1_ck", "mpu");
	omap2_set_init_voltage("core", "l3_ick", "l3_main");
}

static void __init omap4_init_voltages(void)
{
	if (!cpu_is_omap44xx())
		return;

	omap2_set_init_voltage("mpu", "dpll_mpu_ck", "mpu");
	omap2_set_init_voltage("core", "l3_div_ck", "l3_main_1");
	omap2_set_init_voltage("iva", "dpll_iva_m5x2_ck", "iva");
#if 0
	omap2_set_init_voltage("mpu", "dpll_mpu_ck", mpu_dev);
	omap2_set_init_voltage("core", "virt_l3_ck", l3_dev);
	omap2_set_init_voltage("iva", "dpll_iva_m5x2_ck", iva_dev);
#endif
}

/* Interface to the per-device PM QoS framework */
static int omap2_dev_pm_qos_handler(struct notifier_block *nb,
				    unsigned long new_value,
				    void *req)
{
	struct omap_device *od;
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	struct dev_pm_qos_request *dev_pm_qos_req = req;

	pr_debug("OMAP PM CONSTRAINTS: req@0x%p, new_value=%lu\n",
		 req, new_value);

	/* Look for the platform device for the constraint target device */
	pdev = to_platform_device(dev_pm_qos_req->dev);

	/* Try to catch non platform devices */
	if (pdev->name == NULL) {
		pr_err("%s: Error: platform device for device %s not valid\n",
		       __func__, dev_name(dev_pm_qos_req->dev));
		return -EINVAL;
	}

	/* Find the associated omap_device for dev */
	od = container_of(pdev, struct omap_device, pdev);

	/* Find the primary omap_hwmod for dev */
	oh = od->hwmods[0];

	if (od->hwmods_cnt != 1)
		pr_warn_once("%s: No unique hwmod for device %s, using the "
			     "hwmod[0] entry (%s)\n", __func__,
			     dev_name(dev_pm_qos_req->dev), oh->name);

	pr_debug("OMAP PM CONSTRAINTS: req@0x%p, dev=0x%p, new_value=%lu\n",
		 req, dev_pm_qos_req->dev, new_value);

	/* Apply the constraint */
	return omap_hwmod_set_wkup_lat_constraint(oh, dev_pm_qos_req,
						  new_value);
}

static struct notifier_block omap2_dev_pm_qos_notifier = {
	.notifier_call	= omap2_dev_pm_qos_handler,
};

static int __init omap2_dev_pm_qos_init(void)
{
	int ret;

	ret = dev_pm_qos_add_global_notifier(&omap2_dev_pm_qos_notifier);
	if (ret)
		WARN(1, KERN_ERR "Cannot add global notifier for dev PM QoS\n");

	return ret;
}

/* Interface to the memory throughput class of the PM QoS framework */
static int omap2_pm_qos_tput_handler(struct notifier_block *nb,
				     unsigned long new_value,
				     void *unused)
{
	int ret = 0;
	struct device *l3_dev;
	static struct device dummy_l3_dev = {
		.init_name = "omap2_pm_qos_tput_handler",
	};

	pr_debug("OMAP PM MEM TPUT: new_value=%lu\n", new_value);

	/* Apply the constraint */

	l3_dev = omap2_get_l3_device();
	if (!l3_dev) {
		pr_err("Unable to get l3 device pointer");
		ret = -EINVAL;
		goto err;
	}

	/*
	 * Add a call to the DVFS layer, using the new_value (in kB/s)
	 * as the minimum memory throughput to calculate the L3
	 * minimum allowed frequency
	 */
	/* Convert the throughput(in KiB/s) into Hz. */
	new_value = (new_value * 1000) / 4;

	ret = omap_device_scale(&dummy_l3_dev, l3_dev, new_value);

err:
	return ret;
}

static struct notifier_block omap2_pm_qos_tput_notifier = {
	.notifier_call	= omap2_pm_qos_tput_handler,
};

static int __init omap2_pm_qos_tput_init(void)
{
	int ret;

	ret = pm_qos_add_notifier(PM_QOS_MEMORY_THROUGHPUT,
				  &omap2_pm_qos_tput_notifier);
	if (ret)
		WARN(1, KERN_ERR "Cannot add global notifier for dev PM QoS\n");

	return ret;
}

static void __init omap5_init_voltages(void)
{
	if (!cpu_is_omap54xx())
		return;

	omap2_set_init_voltage("mpu", "virt_dpll_mpu_ck", mpu_dev);
	omap2_set_init_voltage("core", "virt_l3_ck", l3_dev);
	omap2_set_init_voltage("mm", "dpll_iva_h12x2_ck", iva_dev);
}

static int __init omap2_common_pm_init(void)
{
#ifdef CONFIG_OMAP5_VIRTIO
	if (cpu_is_omap54xx()) {
		pr_err("FIXME: omap2_common_pm_init\n");
		return 0;
	}

        if (!of_have_populated_dt())                                            
                omap2_init_processor_devices(); 

#endif
	omap_pm_if_init();

	if (cpu_is_omap34xx() && omap3_has_io_chain_ctrl())
		io_chain_trigger_func = omap3_trigger_io_chain;

	if (cpu_is_omap44xx())
		io_chain_trigger_func = omap4_trigger_io_chain;

	/* Register to the per-device PM QoS framework */
	omap2_dev_pm_qos_init();

	/* Register to the throughput class of PM QoS */
	omap2_pm_qos_tput_init();

	return 0;
}

postcore_initcall(omap2_common_pm_init);

static int __init omap2_common_pm_late_init(void)
{
	/*
	 * In the case of DT, the PMIC and SR initialization will be done using
	 * a completely different mechanism.
	 * Disable this part if a DT blob is available.
	 */
	if (of_have_populated_dt())
		return 0;

#ifdef CONFIG_OMAP5_VIRTIO
	if (cpu_is_omap54xx()) {
		pr_err("FIXME: omap2_common_pm_late_init\n");
		return 0;
	}
#endif

	/* Init the OMAP TWL parameters */
	omap_init_all_pmic();

	/* Init the voltage layer */
	omap_pmic_late_init();
	omap_voltage_late_init();

	/* Initialize the voltages */
	omap3_init_voltages();
	omap4_init_voltages();
	omap5_init_voltages();

	/* Smartreflex device init */
	omap_devinit_smartreflex();

#ifdef CONFIG_SUSPEND
	suspend_set_ops(&omap_pm_ops);
#endif
	omap_pm_is_ready_status = true;
	/* let the other CPU know as well */
	smp_wmb();

	return 0;
}
late_initcall(omap2_common_pm_late_init);
