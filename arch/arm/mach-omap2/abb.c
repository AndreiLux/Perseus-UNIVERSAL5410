/*
 * OMAP Adaptive Body-Bias core
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Mike Turquette <mturquette@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/delay.h>

#include "abb.h"
#include "voltage.h"

/*
 * omap_abb_set_opp - program ABB ldo based on new voltage
 *
 * @voltdm - pointer to voltage domain that just finished scaling voltage
 *
 * Look up the ABB ldo state for the new voltage that voltdm just finished
 * transitioning to and compare it to current ldo state.  If a change is needed
 * then clear appropriate PRM_IRQSTATUS bit, transition ldo and then clear
 * PRM_IRQSTATUS bit again.  Returns 0 on success, -EERROR otherwise.
 */
int omap_abb_set_opp(struct voltagedomain *voltdm, u8 opp_sel)
{
	struct omap_abb_instance *abb = voltdm->abb;
	int ret, timeout;

	/* bail early if no transition is necessary */
	if (opp_sel == abb->_opp_sel)
		return 0;

	/* clear interrupt status */
	timeout = 0;
	while (timeout++ < ABB_TRANXDONE_TIMEOUT) {
		abb->common->ops->clear_tranxdone(abb->prm_irq_id);

		ret = abb->common->ops->check_tranxdone(abb->prm_irq_id);
		if (!ret)
			break;

		udelay(1);
	}

	if (timeout >= ABB_TRANXDONE_TIMEOUT) {
		pr_warning("%s: vdd_%s ABB TRANXDONE timeout\n",
				__func__, voltdm->name);
		return -ETIMEDOUT;
	}

	/* program next state of ABB ldo */
	voltdm->rmw(abb->common->opp_sel_mask,
			opp_sel << __ffs(abb->common->opp_sel_mask),
			abb->ctrl_offs);

	/* initiate ABB ldo change */
	voltdm->rmw(abb->common->opp_change_mask,
			abb->common->opp_change_mask,
			abb->ctrl_offs);

	/* clear interrupt status */
	timeout = 0;
	while (timeout++ < ABB_TRANXDONE_TIMEOUT) {
		abb->common->ops->clear_tranxdone(abb->prm_irq_id);

		ret = abb->common->ops->check_tranxdone(abb->prm_irq_id);
		if (!ret)
			break;

		udelay(1);
	}

	if (timeout >= ABB_TRANXDONE_TIMEOUT) {
		pr_warning("%s: vdd_%s ABB TRANXDONE timeout\n",
				__func__, voltdm->name);
		return -ETIMEDOUT;
	}

	/* track internal state */
	abb->_opp_sel = opp_sel;

	return 0;
}

/*
 * omap_abb_pre_scale - possibly execute ABB sequence prior to voltage scale
 * @voltdm - voltage domain that is about to scale
 * @target_volt - voltage that voltdm is scaling towards
 */
int omap_abb_pre_scale(struct voltagedomain *voltdm,
		struct omap_volt_data *target_volt)
{
	struct omap_abb_instance *abb = voltdm->abb;
	struct omap_volt_data *nominal_volt;
	u8 opp_sel;

	if (!voltdm || !target_volt || IS_ERR(target_volt))
		return -EINVAL;

	nominal_volt = omap_voltage_get_curr_vdata(voltdm);

	if (IS_ERR(nominal_volt))
		return 0;

	/* FIXME handle boot-time corner case */
	if (nominal_volt == 0)
		return 0;

	/* bail if the sequence is wrong */
	if (target_volt->volt_nominal > nominal_volt->volt_nominal)
		return 0;

	opp_sel = target_volt->opp_sel;

	/* bail early if no transition is necessary */
	if (opp_sel == abb->_opp_sel)
		return 0;

	return omap_abb_set_opp(voltdm, opp_sel);
}

/*
 * omap_abb_pre_scale - possibly execute ABB sequence prior to voltage scale
 * @voltdm - voltage domain that is about to scale
 * @target_volt - voltage that voltdm is scaling towards
 */
int omap_abb_post_scale(struct voltagedomain *voltdm,
		struct omap_volt_data *target_volt)
{
	struct omap_abb_instance *abb = voltdm->abb;
	struct omap_volt_data *nominal_volt;
	u8 opp_sel;

	if (!voltdm || !target_volt)
		return -EINVAL;

	nominal_volt = omap_voltage_get_curr_vdata(voltdm);
	if (!nominal_volt)
		return -EINVAL;

	/* bail if the sequence is wrong */
	if (target_volt->volt_nominal < nominal_volt->volt_nominal)
		return 0;

	opp_sel = target_volt->opp_sel;

	/* bail early if no transition is necessary */
	if (opp_sel == abb->_opp_sel)
		return 0;

	return omap_abb_set_opp(voltdm, opp_sel);
}

/*
 * omap_abb_enable - enable ABB ldo on a particular voltage domain
 *
 * @voltdm - pointer to particular voltage domain
 */
void omap_abb_enable(struct voltagedomain *voltdm)
{
	struct omap_abb_instance *abb = voltdm->abb;

	if (abb->enabled)
		return;

	abb->enabled = true;

	voltdm->rmw(abb->common->sr2en_mask, abb->common->sr2en_mask,
			abb->setup_offs);
}

/*
 * omap_abb_disable - disable ABB ldo on a particular voltage domain
 *
 * @voltdm - pointer to particular voltage domain
 *
 * Included for completeness.  Not currently used but will be needed in the
 * future if ABB is converted to a loadable module.
 */
void omap_abb_disable(struct voltagedomain *voltdm)
{
	struct omap_abb_instance *abb = voltdm->abb;

	if (!abb->enabled)
		return;

	abb->enabled = false;

	voltdm->rmw(abb->common->sr2en_mask,
			(0 << __ffs(abb->common->sr2en_mask)),
			abb->setup_offs);
}

/*
 * omap_abb_init - Initialize an ABB ldo instance
 *
 * @voltdm: voltage domain upon which ABB ldo resides
 *
 * Initializes an individual ABB ldo for Forward Body-Bias.  FBB is used to
 * insure stability at higher voltages.  Note that some older OMAP chips have a
 * Reverse Body-Bias mode meant to save power at low voltage, but that mode is
 * unsupported and phased out on newer chips.
 */
void __init omap_abb_init(struct voltagedomain *voltdm)
{
	struct omap_abb_instance *abb = voltdm->abb;
	u32 sys_clk_rate;
	u32 sr2_wt_cnt_val;
	u32 clock_cycles;
	u32 settling_time;
	u32 val;

	if (IS_ERR_OR_NULL(abb))
		return;

	/*
	 * SR2_WTCNT_VALUE is the settling time for the ABB ldo after a
	 * transition and must be programmed with the correct time at boot.
	 * The value programmed into the register is the number of SYS_CLK
	 * clock cycles that match a given wall time profiled for the ldo.
	 * This value depends on:
	 * settling time of ldo in micro-seconds (varies per OMAP family)
	 * # of clock cycles per SYS_CLK period (varies per OMAP family)
	 * the SYS_CLK frequency in MHz (varies per board)
	 * The formula is:
	 *
	 *                      ldo settling time (in micro-seconds)
	 * SR2_WTCNT_VALUE = ------------------------------------------
	 *                   (# system clock cycles) * (sys_clk period)
	 *
	 * Put another way:
	 *
	 * SR2_WTCNT_VALUE = settling time / (# SYS_CLK cycles / SYS_CLK rate))
	 *
	 * To avoid dividing by zero multiply both "# clock cycles" and
	 * "settling time" by 10 such that the final result is the one we want.
	 */

	/* convert SYS_CLK rate to MHz & prevent divide by zero */
	sys_clk_rate = DIV_ROUND_CLOSEST(voltdm->sys_clk.rate, 1000000);
	clock_cycles = abb->common->clock_cycles * 10;
	settling_time = abb->common->settling_time * 10;

	/* calculate cycle rate */
	clock_cycles = DIV_ROUND_CLOSEST(clock_cycles, sys_clk_rate);

	/* calulate SR2_WTCNT_VALUE */
	sr2_wt_cnt_val = DIV_ROUND_CLOSEST(settling_time, clock_cycles);

	voltdm->rmw(abb->common->sr2_wtcnt_value_mask,
			(sr2_wt_cnt_val << __ffs(abb->common->sr2_wtcnt_value_mask)),
			abb->setup_offs);

	/* allow Forward Body-Bias */
	voltdm->rmw(abb->common->active_fbb_sel_mask,
			abb->common->active_fbb_sel_mask,
			abb->setup_offs);

	/* did bootloader set OPP_SEL? */
	val = voltdm->read(abb->ctrl_offs);
	val &= abb->common->opp_sel_mask;
	abb->_opp_sel = val >> __ffs(abb->common->opp_sel_mask);

	/* enable the ldo if not done by bootloader */
	val = voltdm->read(abb->setup_offs);
	val &= abb->common->sr2en_mask;
	if (val)
		abb->enabled = true;
	else
		omap_abb_enable(voltdm);

	return;
}
