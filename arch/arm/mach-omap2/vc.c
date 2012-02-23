/*
 * OMAP Voltage Controller (VC) interface
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/string.h>

#include <asm/div64.h>

#include <plat/cpu.h>
#include <plat/prcm.h>

#include "voltage.h"
#include "vc.h"
#include "prm-regbits-34xx.h"
#include "prm-regbits-44xx.h"
#include "prm44xx.h"
#include "scrm44xx.h"
#include "pm.h"

/**
 * struct omap_vc_channel_cfg - describe the cfg_channel bitfield
 * @sa: bit for slave address
 * @rav: bit for voltage configuration register
 * @rac: bit for command configuration register
 * @racen: enable bit for RAC
 * @cmd: bit for command value set selection
 *
 * Channel configuration bits, common for OMAP3+
 * OMAP3 register: PRM_VC_CH_CONF
 * OMAP4 register: PRM_VC_CFG_CHANNEL
 * OMAP5 register: PRM_VC_SMPS_<voltdm>_CONFIG
 */
struct omap_vc_channel_cfg {
	u8 sa;
	u8 rav;
	u8 rac;
	u8 racen;
	u8 cmd;
};

static struct omap_vc_channel_cfg vc_default_channel_cfg = {
	.sa    = BIT(0),
	.rav   = BIT(1),
	.rac   = BIT(2),
	.racen = BIT(3),
	.cmd   = BIT(4),
};

/*
 * On OMAP3+, all VC channels have the above default bitfield
 * configuration, except the OMAP4 MPU channel.  This appears
 * to be a freak accident as every other VC channel has the
 * default configuration, thus creating a mutant channel config.
 */
static struct omap_vc_channel_cfg vc_mutant_channel_cfg = {
	.sa    = BIT(0),
	.rav   = BIT(2),
	.rac   = BIT(3),
	.racen = BIT(4),
	.cmd   = BIT(1),
};

static struct omap_vc_channel_cfg *vc_cfg_bits;
#define CFG_CHANNEL_MASK 0x1f

/**
 * omap_vc_config_channel - configure VC channel to PMIC mappings
 * @voltdm: pointer to voltagdomain defining the desired VC channel
 *
 * Configures the VC channel to PMIC mappings for the following
 * PMIC settings
 * - i2c slave address (SA)
 * - voltage configuration address (RAV)
 * - command configuration address (RAC) and enable bit (RACEN)
 * - command values for ON, ONLP, RET and OFF (CMD)
 *
 * This function currently only allows flexible configuration of the
 * non-default channel.  Starting with OMAP4, there are more than 2
 * channels, with one defined as the default (on OMAP4, it's MPU.)
 * Only the non-default channel can be configured.
 */
static int omap_vc_config_channel(struct voltagedomain *voltdm)
{
	struct omap_vc_channel *vc = voltdm->vc;

	/*
	 * For default channel, the only configurable bit is RACEN.
	 * All others must stay at zero (see function comment above.)
	 */
	if (vc->flags & OMAP_VC_CHANNEL_DEFAULT)
		vc->cfg_channel &= vc_cfg_bits->racen;

	voltdm->rmw(CFG_CHANNEL_MASK << vc->cfg_channel_sa_shift,
		    vc->cfg_channel << vc->cfg_channel_sa_shift,
		    vc->cfg_channel_reg);

	return 0;
}

/* Voltage scale and accessory APIs */
int omap_vc_pre_scale(struct voltagedomain *voltdm,
			struct omap_volt_data *target_volt,
			u8 *target_vsel, u8 *current_vsel)
{
	struct omap_vc_channel *vc = voltdm->vc;
	u32 vc_cmdval;

	if (IS_ERR_OR_NULL(voltdm) || IS_ERR_OR_NULL(target_volt) ||
			!target_vsel || !current_vsel) {
		pr_err("%s: invalid parms!\n", __func__);
		return -EINVAL;
	}

	/* Check if sufficient pmic info is available for this vdd */
	if (!voltdm->pmic) {
		pr_err("%s: Insufficient pmic info to scale the vdd_%s\n",
			__func__, voltdm->name);
		return -EINVAL;
	}

	if (!voltdm->pmic->uv_to_vsel) {
		pr_err("%s: PMIC function to convert voltage in uV to"
			"vsel not registered. Hence unable to scale voltage"
			"for vdd_%s\n", __func__, voltdm->name);
		return -ENODATA;
	}

	if (!voltdm->read || !voltdm->write) {
		pr_err("%s: No read/write API for accessing vdd_%s regs\n",
			__func__, voltdm->name);
		return -EINVAL;
	}

	*target_vsel = voltdm->pmic->uv_to_vsel(omap_get_operation_voltage(target_volt));
	*current_vsel = voltdm->pmic->uv_to_vsel(omap_get_operation_voltage(voltdm->nominal_volt));

	/* Setting the ON voltage to the new target voltage */
	vc_cmdval = voltdm->read(vc->cmdval_reg);
	vc_cmdval &= ~vc->common->cmd_on_mask;
	vc_cmdval |= (*target_vsel << vc->common->cmd_on_shift);
	voltdm->write(vc_cmdval, vc->cmdval_reg);

	voltdm->vc_param->on = target_volt->volt_nominal;

	omap_vp_update_errorgain(voltdm, target_volt);

	return 0;
}

void omap_vc_post_scale(struct voltagedomain *voltdm,
			struct omap_volt_data *target_volt,
			u8 target_vsel, u8 current_vsel)
{
	u32 smps_steps = 0, smps_delay = 0;

	smps_steps = abs(target_vsel - current_vsel);
	/* SMPS slew rate / step size. 2us added as buffer. */
	smps_delay = ((smps_steps * voltdm->pmic->step_size) /
			voltdm->pmic->slew_rate) + 2;
	udelay(smps_delay);
}

static int omap_vc_bypass_send_value(struct voltagedomain *voltdm,
		struct omap_vc_channel *vc, u8 sa, u8 reg, u32 data)
{
	u32 loop_cnt = 0, retries_cnt = 0;
	u32 vc_valid, vc_bypass_val_reg, vc_bypass_value;

	if (IS_ERR_OR_NULL(vc->common)) {
		pr_err("%s voldm=%s bad value for vc->common\n",
				__func__, voltdm->name);
		return -EINVAL;
	}

	vc_valid = vc->common->valid;
	vc_bypass_val_reg = vc->common->bypass_val_reg;
	vc_bypass_value = (data << vc->common->data_shift) |
		(reg << vc->common->regaddr_shift) |
		(sa << vc->common->slaveaddr_shift);

	voltdm->write(vc_bypass_value, vc_bypass_val_reg);
	voltdm->write(vc_bypass_value | vc_valid, vc_bypass_val_reg);

	vc_bypass_value = voltdm->read(vc_bypass_val_reg);

	/*
	 * Loop till the bypass command is acknowledged from the SMPS.
	 * NOTE: This is legacy code. The loop count and retry count needs
	 * to be revisited.
	 */
	while (!(vc_bypass_value & vc_valid)) {
		loop_cnt++;

		if (retries_cnt > 10) {
			pr_warning("%s: Retry count exceeded\n", __func__);
			return -ETIMEDOUT;
		}

		if (loop_cnt > 50) {
			retries_cnt++;
			loop_cnt = 0;
			udelay(10);
		}
		vc_bypass_value = voltdm->read(vc_bypass_val_reg);
	}

	return 0;

}

/* vc_bypass_scale - VC bypass method of voltage scaling */
int omap_vc_bypass_scale(struct voltagedomain *voltdm,
				struct omap_volt_data *target_volt)
{
	struct omap_vc_channel *vc = voltdm->vc;
	u32 loop_cnt = 0, retries_cnt = 0;
	u32 vc_valid, vc_bypass_val_reg, vc_bypass_value;
	u8 target_vsel, current_vsel;
	int ret;

	ret = omap_vc_pre_scale(voltdm, target_volt, &target_vsel, &current_vsel);
	if (ret)
		return ret;

	vc_valid = vc->common->valid;
	vc_bypass_val_reg = vc->common->bypass_val_reg;
	vc_bypass_value = (target_vsel << vc->common->data_shift) |
		(vc->volt_reg_addr << vc->common->regaddr_shift) |
		(vc->i2c_slave_addr << vc->common->slaveaddr_shift);

	voltdm->write(vc_bypass_value, vc_bypass_val_reg);
	voltdm->write(vc_bypass_value | vc_valid, vc_bypass_val_reg);

	vc_bypass_value = voltdm->read(vc_bypass_val_reg);
	/*
	 * Loop till the bypass command is acknowledged from the SMPS.
	 * NOTE: This is legacy code. The loop count and retry count needs
	 * to be revisited.
	 */
	while (!(vc_bypass_value & vc_valid)) {
		loop_cnt++;

		if (retries_cnt > 10) {
			pr_warning("%s: Retry count exceeded\n", __func__);
			return -ETIMEDOUT;
		}

		if (loop_cnt > 50) {
			retries_cnt++;
			loop_cnt = 0;
			udelay(10);
		}
		vc_bypass_value = voltdm->read(vc_bypass_val_reg);
	}

	return 0;

}

/* vc_bypass_scale_voltage - VC bypass method of voltage scaling */
int omap_vc_bypass_scale_voltage(struct voltagedomain *voltdm,
				struct omap_volt_data *target_volt)
{
	struct omap_vc_channel *vc;
	u8 target_vsel, current_vsel;
	int ret;

	if (IS_ERR_OR_NULL(voltdm)) {
		pr_err("%s bad voldm\n", __func__);
		return -EINVAL;
	}

	vc = voltdm->vc;
	if (IS_ERR_OR_NULL(vc)) {
		pr_err("%s voldm=%s bad vc\n", __func__, voltdm->name);
		return -EINVAL;
	}

	ret = omap_vc_pre_scale(voltdm, target_volt, &target_vsel, &current_vsel);
	if (ret)
		return ret;

	ret = omap_vc_bypass_send_value(voltdm, vc, vc->i2c_slave_addr,
					vc->volt_reg_addr, target_vsel);
	if (ret)
		return ret;

	omap_vc_post_scale(voltdm, target_volt, target_vsel, current_vsel);
	return 0;
}

/**
 * omap_vc_bypass_send_i2c_msg() - Function to control PMIC registers over SRI2C
 * @voltdm:	voltage domain
 * @slave_addr:	slave address of the device.
 * @reg_addr:	register address to access
 * @data:	what do we want to write there
 *
 * Many simpler PMICs with a single I2C interface still have configuration
 * registers that may need population. Typical being slew rate configurations
 * thermal shutdown configuration etc. When these PMICs are hooked on I2C_SR,
 * this function allows these configuration registers to be accessed.
 *
 * WARNING: Though this could be used for voltage register configurations over
 * I2C_SR, DONOT use it for that purpose, all the Voltage controller's internal
 * information is bypassed using this function and must be used judiciously.
 */
int omap_vc_bypass_send_i2c_msg(struct voltagedomain *voltdm, u8 slave_addr,
				u8 reg_addr, u8 data)
{
	struct omap_vc_channel *vc;

	if (IS_ERR_OR_NULL(voltdm)) {
		pr_err("%s bad voldm\n", __func__);
		return -EINVAL;
	}

	vc = voltdm->vc;
	if (IS_ERR_OR_NULL(vc)) {
		pr_err("%s voldm=%s bad vc\n", __func__, voltdm->name);
		return -EINVAL;
	}

	return omap_vc_bypass_send_value(voltdm, vc, slave_addr,
					reg_addr, data);
}

static u32 omap_usec_to_32k(u32 usec)
{
	/* DIV_ROUND_UP expanded to 64bit to avoid overflow */
	u64 val = 32768ULL * (u64)usec + 1000000ULL - 1;
	do_div(val, 1000000ULL);
	return val;
}

static void omap3_set_clksetup(u32 usec, struct voltagedomain *voltdm)
{
	voltdm->write(omap_usec_to_32k(usec), OMAP3_PRM_CLKSETUP_OFFSET);
}

static void omap3_set_i2c_timings(struct voltagedomain *voltdm, int off_mode)
{
	unsigned long voltsetup1;
	u32 tgt_volt;

	if (off_mode)
		tgt_volt = voltdm->vc_param->off;
	else
		tgt_volt = voltdm->vc_param->ret;

	voltsetup1 = (voltdm->vc_param->on - tgt_volt) /
			voltdm->pmic->slew_rate;

	voltsetup1 = voltsetup1 * voltdm->sys_clk.rate / 8 / 1000000 + 1;

	voltdm->rmw(voltdm->vfsm->voltsetup_mask,
		voltsetup1 << __ffs(voltdm->vfsm->voltsetup_mask),
		voltdm->vfsm->voltsetup_reg);
}

static void omap3_set_off_timings(struct voltagedomain *voltdm)
{
	unsigned long clksetup;
	unsigned long voltsetup2;
	unsigned long voltsetup2_old;
	u32 val;

	val = voltdm->read(OMAP3_PRM_VOLTCTRL_OFFSET);
	if (!(val & OMAP3430_SEL_OFF_MASK)) {
		omap3_set_i2c_timings(voltdm, 1);
		return;
	}

	clksetup = voltdm->read(OMAP3_PRM_CLKSETUP_OFFSET);

	/* voltsetup 2 in us */
	voltsetup2 = voltdm->vc_param->on / voltdm->pmic->slew_rate;

	/* convert to 32k clk cycles */
	voltsetup2 = DIV_ROUND_UP(voltsetup2 * 32768, 1000000);

	voltsetup2_old = voltdm->read(OMAP3_PRM_VOLTSETUP2_OFFSET);

	if (voltsetup2 > voltsetup2_old) {
		voltdm->write(voltsetup2, OMAP3_PRM_VOLTSETUP2_OFFSET);
		voltdm->write(clksetup - voltsetup2,
			OMAP3_PRM_VOLTOFFSET_OFFSET);
	} else
		voltdm->write(clksetup - voltsetup2_old,
			OMAP3_PRM_VOLTOFFSET_OFFSET);

	/*
	 * omap is not controlling voltage scaling during off-mode,
	 * thus set voltsetup1 to 0
	 */
	voltdm->rmw(voltdm->vfsm->voltsetup_mask, 0,
		voltdm->vfsm->voltsetup_reg);
}

static void omap3_set_core_ret_timings(struct voltagedomain *voltdm)
{
	omap3_set_clksetup(1, voltdm);

	/*
	 * Reset voltsetup 2 and voltoffset when entering retention
	 * as they are not used in this mode
	 */
	voltdm->write(0, OMAP3_PRM_VOLTSETUP2_OFFSET);
	voltdm->write(0, OMAP3_PRM_VOLTOFFSET_OFFSET);
	omap3_set_i2c_timings(voltdm, 0);
}

static void omap3_set_core_off_timings(struct voltagedomain *voltdm)
{
	u32 tstart, tshut;
	omap_pm_get_oscillator(&tstart, &tshut);
	omap3_set_clksetup(tstart, voltdm);
	omap3_set_off_timings(voltdm);
}

static void omap3_vc_channel_sleep(struct voltagedomain *voltdm)
{
	/* Set off timings if entering off */
	if (voltdm->target_state == PWRDM_POWER_OFF)
		omap3_set_off_timings(voltdm);
	else
		omap3_set_i2c_timings(voltdm, 0);
}

static void omap3_vc_channel_wakeup(struct voltagedomain *voltdm)
{
}

static void omap3_vc_core_sleep(struct voltagedomain *voltdm)
{
	u8 mode;

	switch (voltdm->target_state) {
	case PWRDM_POWER_OFF:
		mode = OMAP3430_AUTO_OFF_MASK;
		break;
	case PWRDM_POWER_RET:
		mode = OMAP3430_AUTO_RET_MASK;
		break;
	default:
		mode = OMAP3430_AUTO_SLEEP_MASK;
		break;
	}

	if (mode & OMAP3430_AUTO_OFF_MASK)
		omap3_set_core_off_timings(voltdm);
	else
		omap3_set_core_ret_timings(voltdm);

	voltdm->rmw(OMAP3430_AUTO_OFF_MASK | OMAP3430_AUTO_RET_MASK |
		    OMAP3430_AUTO_SLEEP_MASK, mode,
		    OMAP3_PRM_VOLTCTRL_OFFSET);
}

static void omap3_vc_core_wakeup(struct voltagedomain *voltdm)
{
	voltdm->rmw(OMAP3430_AUTO_OFF_MASK | OMAP3430_AUTO_RET_MASK |
		    OMAP3430_AUTO_SLEEP_MASK, 0, OMAP3_PRM_VOLTCTRL_OFFSET);
}

static void __init omap3_vc_init_channel(struct voltagedomain *voltdm)
{
	if (!strcmp(voltdm->name, "core")) {
		voltdm->sleep = omap3_vc_core_sleep;
		voltdm->wakeup = omap3_vc_core_wakeup;
		omap3_set_core_ret_timings(voltdm);
	} else {
		voltdm->sleep = omap3_vc_channel_sleep;
		voltdm->wakeup = omap3_vc_channel_wakeup;
		omap3_set_i2c_timings(voltdm, 0);
	}
}

static u32 omap4_calc_volt_ramp(struct voltagedomain *voltdm, u32 voltage_diff,
		u32 clk_rate)
{
	u32 prescaler;
	u32 cycles;
	u32 time;

	time = voltage_diff / voltdm->pmic->slew_rate;

	cycles = clk_rate / 1000 * time / 1000;

	cycles /= 64;
	prescaler = 0;

	/* shift to next prescaler until no overflow */

	/* scale for div 256 = 64 * 4 */
	if (cycles > 63) {
		cycles /= 4;
		prescaler++;
	}

	/* scale for div 512 = 256 * 2 */
	if (cycles > 63) {
		cycles /= 2;
		prescaler++;
	}

	/* scale for div 2048 = 512 * 4 */
	if (cycles > 63) {
		cycles /= 4;
		prescaler++;
	}

	/* check for overflow => invalid ramp time */
	if (cycles > 63) {
		pr_warning("%s: invalid setuptime for vdd_%s\n", __func__,
			voltdm->name);
		return 0;
	}

	cycles++;

	return (prescaler << OMAP4430_RAMP_UP_PRESCAL_SHIFT) |
		(cycles << OMAP4430_RAMP_UP_COUNT_SHIFT);
}

static u32 omap4_usec_to_val_scrm(u32 usec, int shift, u32 mask)
{
	u32 val;

	val = omap_usec_to_32k(usec) << shift;

	/* Check for overflow, if yes, force to max value */
	if (val > mask)
		val = mask;

	return val;
}

static void omap4_set_timings(struct voltagedomain *voltdm, bool off_mode)
{
	u32 val;
	u32 ramp;
	u32 tstart, tshut;
	int offset;

	if (off_mode) {
		ramp = omap4_calc_volt_ramp(voltdm,
			voltdm->vc_param->on - voltdm->vc_param->off,
			voltdm->sys_clk.rate);
		offset = voltdm->vfsm->voltsetup_off_reg;
	} else {
		ramp = omap4_calc_volt_ramp(voltdm,
			voltdm->vc_param->on - voltdm->vc_param->ret,
			voltdm->sys_clk.rate);
		offset = voltdm->vfsm->voltsetup_reg;
	}

	if (!ramp)
		return;

	val = ramp << OMAP4430_RAMP_DOWN_COUNT_SHIFT;

	val |= ramp << OMAP4430_RAMP_UP_COUNT_SHIFT;

	voltdm->write(val, offset);

	omap_pm_get_oscillator(&tstart, &tshut);

	val = omap4_usec_to_val_scrm(tstart, OMAP4_SETUPTIME_SHIFT,
		OMAP4_SETUPTIME_MASK);
	val |= omap4_usec_to_val_scrm(tshut, OMAP4_DOWNTIME_SHIFT,
		OMAP4_DOWNTIME_MASK);

	__raw_writel(val, OMAP4_SCRM_CLKSETUPTIME);

	tstart = voltdm->pmic->startup_time;
	tshut = voltdm->pmic->shutdown_time;

	if (tstart && tshut) {
		val = omap4_usec_to_val_scrm(tstart, OMAP4_WAKEUPTIME_SHIFT,
			OMAP4_WAKEUPTIME_MASK);
		val |= omap4_usec_to_val_scrm(tshut, OMAP4_SLEEPTIME_SHIFT,
			OMAP4_SLEEPTIME_MASK);
		__raw_writel(val, OMAP4_SCRM_PMICSETUPTIME);
	}
}

/* OMAP4 specific voltage init functions */
static void __init omap4_vc_init_channel(struct voltagedomain *voltdm)
{
	struct omap_voltdm_pmic *pmic = voltdm->pmic;
	u32 vc_val = 0;

	omap4_set_timings(voltdm, false);

	if (pmic->i2c_high_speed) {
		vc_val |= pmic->i2c_hscll_low << OMAP4430_HSCLL_SHIFT;
		vc_val |= pmic->i2c_hscll_high << OMAP4430_HSCLH_SHIFT;
	}

	vc_val |= pmic->i2c_scll_low << OMAP4430_SCLL_SHIFT;
	vc_val |= pmic->i2c_scll_high << OMAP4430_SCLH_SHIFT;

	if (vc_val)
		voltdm->write(vc_val, OMAP4_PRM_VC_CFG_I2C_CLK_OFFSET);
}

/**
 * omap_vc_i2c_init - initialize I2C interface to PMIC
 * @voltdm: voltage domain containing VC data
 *
 * Use PMIC supplied settings for I2C high-speed mode and
 * master code (if set) and program the VC I2C configuration
 * register.
 *
 * The VC I2C configuration is common to all VC channels,
 * so this function only configures I2C for the first VC
 * channel registers.  All other VC channels will use the
 * same configuration.
 */
static void __init omap_vc_i2c_init(struct voltagedomain *voltdm)
{
	struct omap_vc_channel *vc = voltdm->vc;
	static bool initialized;
	static bool i2c_high_speed;
	u8 mcode;

	if (initialized) {
		if (voltdm->pmic->i2c_high_speed != i2c_high_speed)
			pr_warn("%s: I2C config for vdd_%s does not match other channels (%u).",
				__func__, voltdm->name, i2c_high_speed);
		return;
	}

	i2c_high_speed = voltdm->pmic->i2c_high_speed;
	if (i2c_high_speed)
		voltdm->rmw(vc->common->i2c_cfg_hsen_mask,
			    vc->common->i2c_cfg_hsen_mask,
			    vc->common->i2c_cfg_reg);

	mcode = voltdm->pmic->i2c_mcode;
	if (mcode)
		voltdm->rmw(vc->common->i2c_mcode_mask,
			    mcode << __ffs(vc->common->i2c_mcode_mask),
			    vc->common->i2c_cfg_reg);

	initialized = true;
}

void __init omap_vc_init_channel(struct voltagedomain *voltdm)
{
	struct omap_vc_channel *vc = voltdm->vc;
	u8 on_vsel, onlp_vsel, ret_vsel, off_vsel;
	u32 val;

	if (!voltdm->pmic || !voltdm->pmic->uv_to_vsel) {
		pr_err("%s: No PMIC info for vdd_%s\n", __func__, voltdm->name);
		return;
	}

	if (!voltdm->read || !voltdm->write) {
		pr_err("%s: No read/write API for accessing vdd_%s regs\n",
			__func__, voltdm->name);
		return;
	}

	vc->cfg_channel = 0;
	if (vc->flags & OMAP_VC_CHANNEL_CFG_MUTANT)
		vc_cfg_bits = &vc_mutant_channel_cfg;
	else
		vc_cfg_bits = &vc_default_channel_cfg;

	/* get PMIC/board specific settings */
	vc->i2c_slave_addr = voltdm->pmic->i2c_slave_addr;
	vc->volt_reg_addr = voltdm->pmic->volt_reg_addr;
	vc->cmd_reg_addr = voltdm->pmic->cmd_reg_addr;

	/* Configure the i2c slave address for this VC */
	voltdm->rmw(vc->smps_sa_mask,
		    vc->i2c_slave_addr << __ffs(vc->smps_sa_mask),
		    vc->smps_sa_reg);
	vc->cfg_channel |= vc_cfg_bits->sa;

	/*
	 * Configure the PMIC register addresses.
	 */
	voltdm->rmw(vc->smps_volra_mask,
		    vc->volt_reg_addr << __ffs(vc->smps_volra_mask),
		    vc->smps_volra_reg);
	vc->cfg_channel |= vc_cfg_bits->rav;

	if (vc->cmd_reg_addr) {
		voltdm->rmw(vc->smps_cmdra_mask,
			    vc->cmd_reg_addr << __ffs(vc->smps_cmdra_mask),
			    vc->smps_cmdra_reg);
		vc->cfg_channel |= vc_cfg_bits->rac;
	}

	if (vc->cmd_reg_addr == vc->volt_reg_addr)
		vc->cfg_channel |= vc_cfg_bits->racen;

	/* Set up the on, inactive, retention and off voltage */
	on_vsel = voltdm->pmic->uv_to_vsel(voltdm->vc_param->on);
	onlp_vsel = voltdm->pmic->uv_to_vsel(voltdm->vc_param->onlp);
	ret_vsel = voltdm->pmic->uv_to_vsel(voltdm->vc_param->ret);
	off_vsel = voltdm->pmic->uv_to_vsel(voltdm->vc_param->off);
	val = ((on_vsel << vc->common->cmd_on_shift) |
	       (onlp_vsel << vc->common->cmd_onlp_shift) |
	       (ret_vsel << vc->common->cmd_ret_shift) |
	       (off_vsel << vc->common->cmd_off_shift));
	voltdm->write(val, vc->cmdval_reg);
	vc->cfg_channel |= vc_cfg_bits->cmd;

	/* Channel configuration */
	omap_vc_config_channel(voltdm);

	omap_vc_i2c_init(voltdm);

	if (cpu_is_omap34xx())
		omap3_vc_init_channel(voltdm);
	else if (cpu_is_omap44xx() || cpu_is_omap54xx())
		omap4_vc_init_channel(voltdm);
}

