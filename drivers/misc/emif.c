/*
 * EMIF platform driver
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Aneesh V <aneesh@ti.com>
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 * Vibhore Vardhan <vvardhan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/emif.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include "emif_regs.h"
#include <plat/common.h>

/** struct emif_data - Per device static data for driver's use
 * @thermal_handling_pending:	Whether thermal handling is pending or not
 * @thermal_notify_pending:	Whether thermal notification is pending or not
 * @duplicate:			Whether the DDR devices attached to this EMIF
 *				instance are exactly same as that on EMIF1. In
 *				this case we can save some memory and processing
 * @temperature_level:		Maximum temperature of LPDDR2 devices attached
 *				to this EMIF - read from MR4 register. If there
 *				are two devices attached to this EMIF, this
 *				value is the maximum of the two temperature
 *				levels.
 * @irq:			IRQ number
 * @base:			base address of memory-mapped IO registers.
 * @dev:			device pointer.
 * @addressing			table with addressing information from the spec
 * @regs_cache:			An array of 'struct emif_regs' that stores
 *				calculated register values for different
 *				frequencies, to avoid re-calculating them on
 *				each DVFS transition.
 * @curr_regs:			The set of register values used in the last
 *				frequency change (i.e. corresponding to the
 *				frequency in effect at the moment)
 * @plat_data:			Pointer to saved platform data.
 * @debugfs_root:		dentry to the root folder for EMIF in debugfs
 * @np_ddr:			Pointer to ddr device tree node
 */
struct emif_data {
	u8				thermal_handling_pending;
	u8				thermal_notify_pending;
	u8				duplicate;
	u8				temperature_level;
	u32				irq;
	struct list_head		siblings;
	void __iomem			*base;
	struct device			*dev;
	const struct lpddr2_addressing	*addressing;
	struct emif_regs		*regs_cache[EMIF_MAX_NUM_FREQUENCIES];
	struct emif_regs		*curr_regs;
	struct emif_platform_data	*plat_data;
	struct dentry			*debugfs_root;
#if defined(CONFIG_OF)
	struct device_node		*np_ddr;
#endif
};

static struct emif_data *emif1;
static LIST_HEAD(device_list);
static u32 t_ck; /* DDR clock period in ps */

static void do_emif_regdump_show(struct seq_file *s, struct emif_data *emif,
	struct emif_regs *regs)
{
	u32 type = emif->plat_data->device_info->type;
	u32 ip_rev = emif->plat_data->ip_rev;

	seq_printf(s, "EMIF register cache dump for %dMHz\n",
		regs->freq/100000);

	seq_printf(s, "ref_ctrl_shdw\t: 0x%08x\n", regs->ref_ctrl_shdw);
	seq_printf(s, "sdram_tim1_shdw\t: 0x%08x\n", regs->sdram_tim1_shdw);
	seq_printf(s, "sdram_tim2_shdw\t: 0x%08x\n", regs->sdram_tim2_shdw);
	seq_printf(s, "sdram_tim3_shdw\t: 0x%08x\n", regs->sdram_tim3_shdw);

	if (ip_rev == EMIF_4D) {
		seq_printf(s, "read_idle_ctrl_shdw_normal\t: 0x%08x\n",
			regs->read_idle_ctrl_shdw_normal);
		seq_printf(s, "read_idle_ctrl_shdw_volt_ramp\t: 0x%08x\n",
			regs->read_idle_ctrl_shdw_volt_ramp);
	} else if (ip_rev == EMIF_4D5) {
		seq_printf(s, "dll_calib_ctrl_shdw_normal\t: 0x%08x\n",
			regs->dll_calib_ctrl_shdw_normal);
		seq_printf(s, "dll_calib_ctrl_shdw_volt_ramp\t: 0x%08x\n",
			regs->dll_calib_ctrl_shdw_volt_ramp);
	}

	if (type == DDR_TYPE_LPDDR2_S2 || type == DDR_TYPE_LPDDR2_S4) {
		seq_printf(s, "ref_ctrl_shdw_derated\t: 0x%08x\n",
			regs->ref_ctrl_shdw_derated);
		seq_printf(s, "sdram_tim1_shdw_derated\t: 0x%08x\n",
			regs->sdram_tim1_shdw_derated);
		seq_printf(s, "sdram_tim3_shdw_derated\t: 0x%08x\n",
			regs->sdram_tim3_shdw_derated);
	}
}

static int emif_regdump_show(struct seq_file *s, void *unused)
{
	struct emif_data	*emif	= s->private;
	struct emif_regs	**regs_cache;
	int			i;

	if (emif->duplicate)
		regs_cache = emif1->regs_cache;
	else
		regs_cache = emif->regs_cache;

	for (i = 0; i < EMIF_MAX_NUM_FREQUENCIES && regs_cache[i]; i++) {
		do_emif_regdump_show(s, emif, regs_cache[i]);
		seq_printf(s, "\n");
	}

	return 0;
}

static int emif_regdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, emif_regdump_show, inode->i_private);
}

static const struct file_operations emif_regdump_fops = {
	.open			= emif_regdump_open,
	.read			= seq_read,
	.release		= single_release,
};

static int emif_mr4_show(struct seq_file *s, void *unused)
{
	struct emif_data *emif = s->private;

	seq_printf(s, "MR4=%d\n", emif->temperature_level);
	return 0;
}

static int emif_mr4_open(struct inode *inode, struct file *file)
{
	return single_open(file, emif_mr4_show, inode->i_private);
}

static const struct file_operations emif_mr4_fops = {
	.open			= emif_mr4_open,
	.read			= seq_read,
	.release		= single_release,
};

static int __init emif_debugfs_init(struct emif_data *emif)
{
	struct dentry	*dentry;
	int		ret;

	dentry = debugfs_create_dir(dev_name(emif->dev), NULL);
	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);
		goto err0;
	}
	emif->debugfs_root = dentry;

	dentry = debugfs_create_file("regcache_dump", S_IRUGO,
			emif->debugfs_root, emif, &emif_regdump_fops);
	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);
		goto err1;
	}

	dentry = debugfs_create_file("mr4", S_IRUGO,
			emif->debugfs_root, emif, &emif_mr4_fops);
	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);
		goto err1;
	}

	return 0;
err1:
	debugfs_remove_recursive(emif->debugfs_root);
err0:
	return ret;
}

static void __devexit emif_debugfs_exit(struct emif_data *emif)
{
	debugfs_remove_recursive(emif->debugfs_root);
	emif->debugfs_root = NULL;
}

/*
 * Calculate the period of DDR clock from frequency value
 */
static void set_ddr_clk_period(u32 freq)
{
	/* Divide 10^12 by frequency to get period in ps */
	t_ck = (u32)DIV_ROUND_UP_ULL(1000000000000ull, freq);
}

/*
 * Get bus width used by EMIF. Note that this may be different from the
 * bus width of the DDR devices used. For instance two 16-bit DDR devices
 * may be connected to a given CS of EMIF. In this case bus width as far
 * as EMIF is concerned is 32, where as the DDR bus width is 16 bits.
 */
static u32 get_emif_bus_width(struct emif_data *emif)
{
	u32		width;
	void __iomem	*base = emif->base;

	width = (readl(base + EMIF_SDRAM_CONFIG) & NARROW_MODE_MASK)
			>> NARROW_MODE_SHIFT;
	width = width == 0 ? 32 : 16;

	return width;
}

/*
 * Get the CL from SDRAM_CONFIG register
 */
static u32 get_cl(struct emif_data *emif)
{
	u32		cl;
	void __iomem	*base = emif->base;

	cl = (readl(base + EMIF_SDRAM_CONFIG) & CL_MASK) >> CL_SHIFT;

	return cl;
}

static void do_freq_update(void)
{
	omap4_prcm_freq_update();
}

/* Find addressing table entry based on the device's type and density */
static const struct lpddr2_addressing *get_addressing_table(
	const struct ddr_device_info *device_info)
{
	u32		index, type, density;

	type = device_info->type;
	density = device_info->density;

	switch (type) {
	case DDR_TYPE_LPDDR2_S4:
		index = density - 1;
		break;
	case DDR_TYPE_LPDDR2_S2:
		switch (density) {
		case DDR_DENSITY_1Gb:
		case DDR_DENSITY_2Gb:
			index = density + 3;
			break;
		default:
			index = density - 1;
		}
		break;
	default:
		return NULL;
	}

	return &lpddr2_jedec_addressing_table[index];
}

/*
 * Find the the right timing table from the array of timing
 * tables of the device using DDR clock frequency
 */
static const struct lpddr2_timings *get_timings_table(struct emif_data *emif,
		u32 freq)
{
	u32				i, min, max, freq_nearest;
	const struct lpddr2_timings	*timings = NULL;
	const struct lpddr2_timings	*timings_arr = emif->plat_data->timings;
	struct				device *dev = emif->dev;

	/* Start with a very high frequency - 1GHz */
	freq_nearest = 1000000000;

	/*
	 * Find the timings table such that:
	 *  1. the frequency range covers the required frequency(safe) AND
	 *  2. the max_freq is closest to the required frequency(optimal)
	 */
	for (i = 0; i < emif->plat_data->timings_arr_size; i++) {
		max = timings_arr[i].max_freq;
		min = timings_arr[i].min_freq;
		if ((freq >= min) && (freq <= max) && (max < freq_nearest)) {
			freq_nearest = max;
			timings = &timings_arr[i];
		}
	}

	if (!timings)
		dev_err(dev, "Couldn't find timings for - %dHz\n", freq);

	dev_dbg(dev, "timings table: freq %d, speed bin freq %d\n",
		freq, freq_nearest);

	return timings;
}

static u32 get_sdram_ref_ctrl_shdw(u32 freq,
		const struct lpddr2_addressing *addressing)
{
	u32 ref_ctrl_shdw = 0, val = 0, freq_khz, t_refi;

	/* Scale down frequency and t_refi to avoid overflow */
	freq_khz = freq / 1000;
	t_refi = addressing->tREFI_ns / 100;

	/*
	 * refresh rate to be set is 'tREFI(in us) * freq in MHz
	 * division by 10000 to account for change in units
	 */
	val = t_refi * freq_khz / 10000;
	ref_ctrl_shdw |= val << REFRESH_RATE_SHIFT;

	return ref_ctrl_shdw;
}

static u32 get_sdram_tim_1_shdw(const struct lpddr2_timings *timings,
		const struct ddr_min_tck *min_tck,
		const struct lpddr2_addressing *addressing,
		u32 ip_rev)
{
	u32 tim1 = 0, val = 0;

	val = max(min_tck->tWTR, DIV_ROUND_UP(timings->tWTR, t_ck)) - 1;
	tim1 |= val << T_WTR_SHIFT;

	if (addressing->num_banks == B8)
		val = DIV_ROUND_UP(timings->tFAW, t_ck*4);
	else
		val = max(min_tck->tRRD, DIV_ROUND_UP(timings->tRRD, t_ck));
	tim1 |= (val - 1) << T_RRD_SHIFT;

	val = DIV_ROUND_UP(timings->tRAS_min + timings->tRPab, t_ck) - 1;
	tim1 |= val << T_RC_SHIFT;

	val = max(min_tck->tRASmin, DIV_ROUND_UP(timings->tRAS_min, t_ck));
	tim1 |= (val - 1) << T_RAS_SHIFT;

	val = max(min_tck->tWR, DIV_ROUND_UP(timings->tWR, t_ck)) - 1;
	tim1 |= val << T_WR_SHIFT;

	val = max(min_tck->tRCD, DIV_ROUND_UP(timings->tRCD, t_ck)) - 1;
	tim1 |= val << T_RCD_SHIFT;

	val = max(min_tck->tRPab, DIV_ROUND_UP(timings->tRPab, t_ck)) - 1;
	tim1 |= val << T_RP_SHIFT;

	if (ip_rev == EMIF_4D5) {
		val = DIV_ROUND_UP(timings->tRTW, t_ck) - 1;
		tim1 |= val << T_RTW_SHIFT;
	}

	return tim1;
}

static u32 get_sdram_tim_1_shdw_derated(const struct lpddr2_timings *timings,
		const struct ddr_min_tck *min_tck,
		const struct lpddr2_addressing *addressing, u32 ip_rev)
{
	u32 tim1 = 0, val = 0;

	val = max(min_tck->tWTR, DIV_ROUND_UP(timings->tWTR, t_ck)) - 1;
	tim1 = val << T_WTR_SHIFT;

	/*
	 * tFAW is approximately 4 times tRRD. So add 1875*4 = 7500ps
	 * to tFAW for de-rating
	 */
	if (addressing->num_banks == B8) {
		val = DIV_ROUND_UP(timings->tFAW + 7500, 4 * t_ck) - 1;
	} else {
		val = DIV_ROUND_UP(timings->tRRD + 1875, t_ck);
		val = max(min_tck->tRRD, val) - 1;
	}
	tim1 |= val << T_RRD_SHIFT;

	val = DIV_ROUND_UP(timings->tRAS_min + timings->tRPab + 1875, t_ck);
	tim1 |= (val - 1) << T_RC_SHIFT;

	val = DIV_ROUND_UP(timings->tRAS_min + 1875, t_ck);
	val = max(min_tck->tRASmin, val) - 1;
	tim1 |= val << T_RAS_SHIFT;

	val = max(min_tck->tWR, DIV_ROUND_UP(timings->tWR, t_ck)) - 1;
	tim1 |= val << T_WR_SHIFT;

	val = max(min_tck->tRCD, DIV_ROUND_UP(timings->tRCD + 1875, t_ck));
	tim1 |= (val - 1) << T_RCD_SHIFT;

	val = max(min_tck->tRPab, DIV_ROUND_UP(timings->tRPab + 1875, t_ck));
	tim1 |= (val - 1) << T_RP_SHIFT;

	if (ip_rev == EMIF_4D5) {
		val = DIV_ROUND_UP(timings->tRTW, t_ck) - 1;
		tim1 |= val << T_RTW_SHIFT;
	}

	return tim1;
}

static u32 get_sdram_tim_2_shdw(const struct lpddr2_timings *timings,
		const struct ddr_min_tck *min_tck,
		const struct lpddr2_addressing *addressing,
		u32 type, u32 ip_rev)
{
	u32 tim2 = 0, val = 0;

	val = min_tck->tCKE - 1;
	tim2 |= val << T_CKE_SHIFT;

	val = max(min_tck->tRTP, DIV_ROUND_UP(timings->tRTP, t_ck)) - 1;
	tim2 |= val << T_RTP_SHIFT;

	/* tXSNR = tRFCab_ps + 10 ns(tRFCab_ps for LPDDR2). */
	val = DIV_ROUND_UP(addressing->tRFCab_ps + 10000, t_ck) - 1;
	tim2 |= val << T_XSNR_SHIFT;

	/* XSRD same as XSNR for LPDDR2 */
	tim2 |= val << T_XSRD_SHIFT;

	if (ip_rev == EMIF_4D5) {
		val = DIV_ROUND_UP(timings->tAONPD, t_ck) - 1;
		tim2 |= val << T_ODT_SHIFT;
	}

	val = max(min_tck->tXP, DIV_ROUND_UP(timings->tXP, t_ck)) - 1;
	tim2 |= val << T_XP_SHIFT;

	return tim2;
}

static u32 get_sdram_tim_3_shdw(const struct lpddr2_timings *timings,
		const struct ddr_min_tck *min_tck,
		const struct lpddr2_addressing *addressing,
		u32 type, u32 ip_rev, u32 derated)
{
	u32 tim3 = 0, val = 0;

	val = timings->tRAS_max_ns / addressing->tREFI_ns - 1;
	val = val > 0xF ? 0xF : val;
	tim3 |= val << T_RAS_MAX_SHIFT;

	val = DIV_ROUND_UP(addressing->tRFCab_ps, t_ck) - 1;
	tim3 |= val << T_RFC_SHIFT;

	if (ip_rev == EMIF_4D5)
		val = DIV_ROUND_UP(timings->tDQSCK_max + 1000, t_ck) - 1;
	else
		val = DIV_ROUND_UP(timings->tDQSCK_max, t_ck) - 1;

	tim3 |= val << T_TDQSCKMAX_SHIFT;

	val = DIV_ROUND_UP(timings->tZQCS, t_ck) - 1;
	tim3 |= val << ZQ_ZQCS_SHIFT;

	val = DIV_ROUND_UP(timings->tCKESR, t_ck);
	val = max(min_tck->tCKESR, val) - 1;
	tim3 |= val << T_CKESR_SHIFT;

	if (ip_rev == EMIF_4D5) {
		tim3 |= (EMIF_T_CSTA - 1) << T_CSTA_SHIFT;

		val = DIV_ROUND_UP(EMIF_T_PDLL_UL, 128) - 1;
		tim3 |= val << T_PDLL_UL_SHIFT;
	}

	return tim3;
}

static u32 get_zq_config_reg(const struct lpddr2_addressing *addressing,
		bool cs1_used, bool cal_resistors_per_cs)
{
	u32 zq = 0, val = 0;

	val = EMIF_ZQCS_INTERVAL_US * 1000 / addressing->tREFI_ns;
	zq |= val << ZQ_REFINTERVAL_SHIFT;

	val = DIV_ROUND_UP(T_ZQCL_DEFAULT_NS, T_ZQCS_DEFAULT_NS) - 1;
	zq |= val << ZQ_ZQCL_MULT_SHIFT;

	val = DIV_ROUND_UP(T_ZQINIT_DEFAULT_NS, T_ZQCL_DEFAULT_NS) - 1;
	zq |= val << ZQ_ZQINIT_MULT_SHIFT;

	zq |= ZQ_SFEXITEN_ENABLE << ZQ_SFEXITEN_SHIFT;

	if (cal_resistors_per_cs)
		zq |= ZQ_DUALCALEN_ENABLE << ZQ_DUALCALEN_SHIFT;
	else
		zq |= ZQ_DUALCALEN_DISABLE << ZQ_DUALCALEN_SHIFT;

	zq |= ZQ_CS0EN_MASK; /* CS0 is used for sure */

	val = cs1_used ? 1 : 0;
	zq |= val << ZQ_CS1EN_SHIFT;

	return zq;
}

static u32 get_temp_alert_config(const struct lpddr2_addressing *addressing,
		const struct emif_custom_configs *custom_configs, bool cs1_used,
		u32 sdram_io_width, u32 emif_bus_width)
{
	u32 alert = 0, interval, devcnt;

	if (custom_configs && (custom_configs->mask &
				EMIF_CUSTOM_CONFIG_TEMP_ALERT_POLL_INTERVAL))
		interval = custom_configs->temp_alert_poll_interval_ms;
	else
		interval = TEMP_ALERT_POLL_INTERVAL_DEFAULT_MS;

	interval *= 1000000;			/* Convert to ns */
	interval /= addressing->tREFI_ns;	/* Convert to refresh cycles */
	alert |= (interval << TA_REFINTERVAL_SHIFT);

	/*
	 * sdram_io_width is in 'log2(x) - 1' form. Convert emif_bus_width
	 * also to this form and subtract to get TA_DEVCNT, which is
	 * in log2(x) form.
	 */
	emif_bus_width = __fls(emif_bus_width) - 1;
	devcnt = emif_bus_width - sdram_io_width;
	alert |= devcnt << TA_DEVCNT_SHIFT;

	/* DEVWDT is in 'log2(x) - 3' form */
	alert |= (sdram_io_width - 2) << TA_DEVWDT_SHIFT;

	alert |= 1 << TA_SFEXITEN_SHIFT;
	alert |= 1 << TA_CS0EN_SHIFT;
	alert |= (cs1_used ? 1 : 0) << TA_CS1EN_SHIFT;

	return alert;
}

static u32 get_read_idle_ctrl_shdw(u8 volt_ramp)
{
	u32 idle = 0, val = 0;

	/*
	 * Maximum value in normal conditions and increased frequency
	 * when voltage is ramping
	 */
	if (volt_ramp)
		val = READ_IDLE_INTERVAL_DVFS / t_ck / 64 - 1;
	else
		val = 0x1FF;

	/*
	 * READ_IDLE_CTRL register in EMIF4D has same offset and fields
	 * as DLL_CALIB_CTRL in EMIF4D5, so use the same shifts
	 */
	idle |= val << DLL_CALIB_INTERVAL_SHIFT;
	idle |= EMIF_READ_IDLE_LEN_VAL << ACK_WAIT_SHIFT;

	return idle;
}

static u32 get_dll_calib_ctrl_shdw(u8 volt_ramp)
{
	u32 calib = 0, val = 0;

	if (volt_ramp == DDR_VOLTAGE_RAMPING)
		val = DLL_CALIB_INTERVAL_DVFS / t_ck / 16 - 1;
	else
		val = 0; /* Disabled when voltage is stable */

	calib |= val << DLL_CALIB_INTERVAL_SHIFT;
	calib |= DLL_CALIB_ACK_WAIT_VAL << ACK_WAIT_SHIFT;

	return calib;
}

static u32 get_ddr_phy_ctrl_1_attilaphy_4d(const struct lpddr2_timings *timings,
	u32 freq, u8 RL)
{
	u32 phy = EMIF_DDR_PHY_CTRL_1_BASE_VAL_ATTILAPHY, val = 0;

	val = RL + DIV_ROUND_UP(timings->tDQSCK_max, t_ck) - 1;
	phy |= val << READ_LATENCY_SHIFT_4D;

	if (freq <= 100000000)
		val = EMIF_DLL_SLAVE_DLY_CTRL_100_MHZ_AND_LESS_ATTILAPHY;
	else if (freq <= 200000000)
		val = EMIF_DLL_SLAVE_DLY_CTRL_200_MHZ_ATTILAPHY;
	else
		val = EMIF_DLL_SLAVE_DLY_CTRL_400_MHZ_ATTILAPHY;

	phy |= val << DLL_SLAVE_DLY_CTRL_SHIFT_4D;

	return phy;
}

static u32 get_phy_ctrl_1_intelliphy_4d5(u32 freq, u8 cl)
{
	u32 phy = EMIF_DDR_PHY_CTRL_1_BASE_VAL_INTELLIPHY, half_delay;

	/*
	 * DLL operates at 266 MHz. If DDR frequency is near 266 MHz,
	 * half-delay is not needed else set half-delay
	 */
	if (freq >= 265000000 && freq < 267000000)
		half_delay = 0;
	else
		half_delay = 1;

	phy |= half_delay << DLL_HALF_DELAY_SHIFT_4D5;
	phy |= ((cl + DIV_ROUND_UP(EMIF_PHY_TOTAL_READ_LATENCY_INTELLIPHY_PS,
			t_ck) - 1) << READ_LATENCY_SHIFT_4D5);

	return phy;
}

static u32 get_ext_phy_ctrl_2_intelliphy_4d5(void)
{
	u32 fifo_we_slave_ratio;

	fifo_we_slave_ratio =  DIV_ROUND_CLOSEST(
		EMIF_INTELLI_PHY_DQS_GATE_OPENING_DELAY_PS * 256 , t_ck);

	return fifo_we_slave_ratio | fifo_we_slave_ratio << 11 |
		fifo_we_slave_ratio << 22;
}

static u32 get_ext_phy_ctrl_3_intelliphy_4d5(void)
{
	u32 fifo_we_slave_ratio;

	fifo_we_slave_ratio =  DIV_ROUND_CLOSEST(
		EMIF_INTELLI_PHY_DQS_GATE_OPENING_DELAY_PS * 256 , t_ck);

	return fifo_we_slave_ratio >> 10 | fifo_we_slave_ratio << 1 |
		fifo_we_slave_ratio << 12 | fifo_we_slave_ratio << 23;
}

static u32 get_ext_phy_ctrl_4_intelliphy_4d5(void)
{
	u32 fifo_we_slave_ratio;

	fifo_we_slave_ratio =  DIV_ROUND_CLOSEST(
		EMIF_INTELLI_PHY_DQS_GATE_OPENING_DELAY_PS * 256 , t_ck);

	return fifo_we_slave_ratio >> 9 | fifo_we_slave_ratio << 2 |
		fifo_we_slave_ratio << 13;
}

static u32 get_pwr_mgmt_ctrl(u32 freq, struct emif_data *emif, u32 ip_rev)
{
	u32 pwr_mgmt_ctrl	= 0, timeout;
	u32 lpmode		= EMIF_LP_MODE_SELF_REFRESH;
	u32 timeout_perf	= EMIF_LP_MODE_TIMEOUT_PERFORMANCE;
	u32 timeout_pwr		= EMIF_LP_MODE_TIMEOUT_POWER;
	u32 freq_threshold	= EMIF_LP_MODE_FREQ_THRESHOLD;

	struct emif_custom_configs *cust_cfgs = emif->plat_data->custom_configs;

	if (cust_cfgs && (cust_cfgs->mask & EMIF_CUSTOM_CONFIG_LPMODE)) {
		lpmode		= cust_cfgs->lpmode;
		timeout_perf	= cust_cfgs->lpmode_timeout_performance;
		timeout_pwr	= cust_cfgs->lpmode_timeout_power;
		freq_threshold  = cust_cfgs->lpmode_freq_threshold;
	}

	/* Timeout based on DDR frequency */
	timeout = freq >= freq_threshold ? timeout_perf : timeout_pwr;

	/* The value to be set in register is "log2(timeout) - 3" */
	if (timeout < 16) {
		timeout = 0;
	} else {
		timeout = __fls(timeout) - 3;
		if (timeout & (timeout - 1))
			timeout++;
	}

	switch (lpmode) {
	case EMIF_LP_MODE_CLOCK_STOP:
		pwr_mgmt_ctrl = (timeout << CS_TIM_SHIFT) |
					SR_TIM_MASK | PD_TIM_MASK;
		break;
	case EMIF_LP_MODE_SELF_REFRESH:
		pwr_mgmt_ctrl = (timeout << SR_TIM_SHIFT) |
					CS_TIM_MASK | PD_TIM_MASK;
		break;
	case EMIF_LP_MODE_PWR_DN:
		pwr_mgmt_ctrl = (timeout << PD_TIM_SHIFT) |
					CS_TIM_MASK | SR_TIM_MASK;
		break;
	case EMIF_LP_MODE_DISABLE:
	default:
		pwr_mgmt_ctrl = CS_TIM_MASK |
					PD_TIM_MASK | SR_TIM_MASK;
	}

	/* No CS_TIM in EMIF_4D5 */
	if (ip_rev == EMIF_4D5)
		pwr_mgmt_ctrl &= ~CS_TIM_MASK;

	pwr_mgmt_ctrl |= lpmode << LP_MODE_SHIFT;

	return pwr_mgmt_ctrl;
}

/*
 * Get the temperature level of the EMIF instance:
 * Reads the MR4 register of attached SDRAM parts to find out the temperature
 * level. If there are two parts attached(one on each CS), then the temperature
 * level for the EMIF instance is the higher of the two temperatures.
 */
static void get_temperature_level(struct emif_data *emif)
{
	u32		temp, temperature_level;
	void __iomem	*base;

	base = emif->base;

	/* Read mode register 4 */
	writel(DDR_MR4, base + EMIF_LPDDR2_MODE_REG_CONFIG);
	temperature_level = readl(base + EMIF_LPDDR2_MODE_REG_DATA);
	temperature_level = (temperature_level & MR4_SDRAM_REF_RATE_MASK) >>
				MR4_SDRAM_REF_RATE_SHIFT;

	if (emif->plat_data->device_info->cs1_used) {
		writel(DDR_MR4 | CS_MASK, base + EMIF_LPDDR2_MODE_REG_CONFIG);
		temp = readl(base + EMIF_LPDDR2_MODE_REG_DATA);
		temp = (temp & MR4_SDRAM_REF_RATE_MASK)
				>> MR4_SDRAM_REF_RATE_SHIFT;
		temperature_level = max(temp, temperature_level);
	}

	/* treat everything less than nominal(3) in MR4 as nominal */
	if (unlikely(temperature_level < SDRAM_TEMP_NOMINAL))
		temperature_level = SDRAM_TEMP_NOMINAL;

	/* if we get reserved value in MR4 persist with the existing value */
	if (likely(temperature_level != SDRAM_TEMP_RESERVED_4))
		emif->temperature_level = temperature_level;
}

/*
 * Program EMIF shadow registers that are not dependent on temperature
 * or voltage
 */
static void setup_registers(struct emif_data *emif, struct emif_regs *regs)
{
	void __iomem	*base = emif->base;

	writel(regs->sdram_tim2_shdw, base + EMIF_SDRAM_TIMING_2_SHDW);
	writel(regs->phy_ctrl_1_shdw, base + EMIF_DDR_PHY_CTRL_1_SHDW);

	/* Settings specific for EMIF4D5 */
	if (emif->plat_data->ip_rev != EMIF_4D5)
		return;
	writel(regs->ext_phy_ctrl_2_shdw, base + EMIF_EXT_PHY_CTRL_2_SHDW);
	writel(regs->ext_phy_ctrl_3_shdw, base + EMIF_EXT_PHY_CTRL_3_SHDW);
	writel(regs->ext_phy_ctrl_4_shdw, base + EMIF_EXT_PHY_CTRL_4_SHDW);
}

/* When voltage ramps forced read idle should happen more often */
static void setup_volt_sensitive_regs(struct emif_data *emif,
		struct emif_regs *regs, u32 volt_state)
{
	u32		calib_ctrl;
	void __iomem	*base = emif->base;

	/*
	 * EMIF_READ_IDLE_CTRL in EMIF4D refers to the same register as
	 * EMIF_DLL_CALIB_CTRL in EMIF4D5 and dll_calib_ctrl_shadow_*
	 * is an alias of the respective read_idle_ctrl_shdw_* (members of
	 * a union). So, the below code takes care of both cases
	 */
	if (volt_state == DDR_VOLTAGE_RAMPING)
		calib_ctrl = regs->dll_calib_ctrl_shdw_volt_ramp;
	else
		calib_ctrl = regs->dll_calib_ctrl_shdw_normal;

	writel(calib_ctrl, base + EMIF_DLL_CALIB_CTRL_SHDW);
}

/*
 * setup_temperature_sensitive_regs() - set the timings for temperature
 * sensitive registers. This happens once at initialisation time based
 * on the temperature at boot time and subsequently based on the temperature
 * alert interrupt. Temperature alert can happen when the temperature
 * increases or drops. So this function can have the effect of either
 * derating the timings or going back to nominal values.
 */
static void setup_temperature_sensitive_regs(struct emif_data *emif,
		struct emif_regs *regs)
{
	u32		tim1, tim3, ref_ctrl, type;
	void __iomem	*base = emif->base;
	u32		temperature = emif->temperature_level;

	type = emif->plat_data->device_info->type;

	tim1 = regs->sdram_tim1_shdw;
	tim3 = regs->sdram_tim3_shdw;
	ref_ctrl = regs->ref_ctrl_shdw;

	/* No de-rating for non-lpddr2 devices */
	if (type != DDR_TYPE_LPDDR2_S2 && type != DDR_TYPE_LPDDR2_S4)
		goto out;

	if (temperature == SDRAM_TEMP_HIGH_DERATE_REFRESH) {
		ref_ctrl = regs->ref_ctrl_shdw_derated;
	} else if (temperature == SDRAM_TEMP_HIGH_DERATE_REFRESH_AND_TIMINGS) {
		tim1 = regs->sdram_tim1_shdw_derated;
		tim3 = regs->sdram_tim3_shdw_derated;
		ref_ctrl = regs->ref_ctrl_shdw_derated;
	}

out:
	writel(tim1, base + EMIF_SDRAM_TIMING_1_SHDW);
	writel(tim3, base + EMIF_SDRAM_TIMING_3_SHDW);
	writel(ref_ctrl, base + EMIF_SDRAM_REFRESH_CTRL_SHDW);
}

static irqreturn_t handle_temp_alert(void __iomem *base, struct emif_data *emif)
{
	u32		old_temp_level;

	old_temp_level = emif->temperature_level;
	get_temperature_level(emif);

	if (unlikely(emif->temperature_level == old_temp_level))
		return IRQ_HANDLED;

	emif->thermal_notify_pending = 1;
	if (likely(emif->temperature_level < old_temp_level)) {
		/* Temperature coming down - defer handling to thread */
		emif->thermal_handling_pending = 1;
	} else if (likely(emif->temperature_level !=
			  SDRAM_TEMP_VERY_HIGH_SHUTDOWN)) {
		/* Temperature is going up - handle immediately */
		if (!emif->curr_regs) {
			dev_err(emif->dev, "temperature alert before registers are calculated, not de-rating timings\n");
			goto out;
		}
		setup_temperature_sensitive_regs(emif, emif->curr_regs);
		do_freq_update();
	}

out:
	return IRQ_WAKE_THREAD;
}

static irqreturn_t emif_interrupt_handler(int irq, void *dev_id)
{
	u32			interrupts;
	struct emif_data	*emif = dev_id;
	void __iomem		*base = emif->base;
	struct device		*dev = emif->dev;
	irqreturn_t		ret = IRQ_HANDLED;

	/* Save the status and clear it */
	interrupts = readl(base + EMIF_SYSTEM_OCP_INTERRUPT_STATUS);
	writel(interrupts, base + EMIF_SYSTEM_OCP_INTERRUPT_STATUS);

	/*
	 * Handle temperature alert
	 * Temperature alert should be same for all ports
	 * So, it's enough to process it only for one of the ports
	 */
	if (interrupts & TA_SYS_MASK)
		ret = handle_temp_alert(base, emif);

	if (interrupts & ERR_SYS_MASK)
		dev_err(dev, "Access error from SYS port - %x\n", interrupts);

	if (emif->plat_data->hw_caps & EMIF_HW_CAPS_LL_INTERFACE) {
		/* Save the status and clear it */
		interrupts = readl(base + EMIF_LL_OCP_INTERRUPT_STATUS);
		writel(interrupts, base + EMIF_LL_OCP_INTERRUPT_STATUS);

		if (interrupts & ERR_LL_MASK)
			dev_err(dev, "Access error from LL port - %x\n",
				interrupts);
	}

	return ret;
}

static irqreturn_t emif_threaded_isr(int irq, void *dev_id)
{
	struct device		*dev;
	struct emif_data	*emif;

	emif = dev_id;
	dev = emif->dev;

	if (emif->temperature_level == SDRAM_TEMP_VERY_HIGH_SHUTDOWN) {
		dev_emerg(emif->dev, "SDRAM temperature exceeds operating limit.. Needs shut down!!!\n");
		kernel_power_off();
	}

	if (emif->thermal_handling_pending) {
		if (emif->curr_regs) {
			setup_temperature_sensitive_regs(emif, emif->curr_regs);
			do_freq_update();
		} else {
			dev_err(emif->dev, "temperature alert before registers are calculated, not de-rating timings\n");
		}

		emif->thermal_handling_pending = 0;
	}

	if (emif->thermal_notify_pending) {
		sysfs_notify(&(emif->dev->kobj), NULL, "temperature");
		kobject_uevent(&(emif->dev->kobj), KOBJ_CHANGE);
		emif->thermal_notify_pending = 0;
		dev_warn(dev, "SDRAM temperature change, new level MR4=%d\n",
				emif->temperature_level);
	}

	return IRQ_HANDLED;
}

static void clear_all_interrupts(struct emif_data *emif)
{
	void __iomem	*base = emif->base;

	writel(readl(base + EMIF_SYSTEM_OCP_INTERRUPT_STATUS),
		base + EMIF_SYSTEM_OCP_INTERRUPT_STATUS);
	if (emif->plat_data->hw_caps & EMIF_HW_CAPS_LL_INTERFACE)
		writel(readl(base + EMIF_LL_OCP_INTERRUPT_STATUS),
			base + EMIF_LL_OCP_INTERRUPT_STATUS);
}

static int __init setup_interrupts(struct emif_data *emif)
{
	u32		interrupts, type;
	void __iomem	*base = emif->base;

	type = emif->plat_data->device_info->type;

	clear_all_interrupts(emif);

	/* Enable interrupts for SYS interface */
	interrupts = EN_ERR_SYS_MASK;
	if (type == DDR_TYPE_LPDDR2_S2 || type == DDR_TYPE_LPDDR2_S4)
		interrupts |= EN_TA_SYS_MASK;
	writel(interrupts, base + EMIF_SYSTEM_OCP_INTERRUPT_ENABLE_SET);

	/* Enable interrupts for LL interface */
	if (emif->plat_data->hw_caps & EMIF_HW_CAPS_LL_INTERFACE) {
		/* TA need not be enabled for LL */
		interrupts = EN_ERR_LL_MASK;
		writel(interrupts, base + EMIF_LL_OCP_INTERRUPT_ENABLE_SET);
	}

	/* setup IRQ handlers */
	return request_threaded_irq(emif->irq,
				    emif_interrupt_handler,
				    emif_threaded_isr,
				    0, dev_name(emif->dev),
				    emif);

}

static void __init emif_onetime_settings(struct emif_data *emif)
{
	u32				pwr_mgmt_ctrl, zq, temp_alert_cfg;
	void __iomem			*base = emif->base;
	const struct lpddr2_addressing	*addressing;
	const struct ddr_device_info	*device_info;

	device_info = emif->plat_data->device_info;
	addressing = get_addressing_table(device_info);

	/*
	 * Init power management settings
	 * We don't know the frequency yet. Use a high frequency
	 * value for a conservative timeout setting
	 */
	pwr_mgmt_ctrl = get_pwr_mgmt_ctrl(1000000000, emif,
			emif->plat_data->ip_rev);
	writel(pwr_mgmt_ctrl, base + EMIF_POWER_MANAGEMENT_CONTROL);

	/* Init ZQ calibration settings */
	zq = get_zq_config_reg(addressing, device_info->cs1_used,
		device_info->cal_resistors_per_cs);
	writel(zq, base + EMIF_SDRAM_OUTPUT_IMPEDANCE_CALIBRATION_CONFIG);

	/* Check temperature level temperature level*/
	get_temperature_level(emif);
	if (emif->temperature_level == SDRAM_TEMP_VERY_HIGH_SHUTDOWN)
		dev_emerg(emif->dev, "SDRAM temperature exceeds operating limit.. Needs shut down!!!\n");

	/* Init temperature polling */
	temp_alert_cfg = get_temp_alert_config(addressing,
		emif->plat_data->custom_configs, device_info->cs1_used,
		device_info->io_width, get_emif_bus_width(emif));
	writel(temp_alert_cfg, base + EMIF_TEMPERATURE_ALERT_CONFIG);

	/*
	 * Program external PHY control registers that are not frequency
	 * dependent
	 */
	if (emif->plat_data->phy_type != EMIF_PHY_TYPE_INTELLIPHY)
		return;
	writel(EMIF_EXT_PHY_CTRL_1_VAL, base + EMIF_EXT_PHY_CTRL_1_SHDW);
	writel(EMIF_EXT_PHY_CTRL_5_VAL, base + EMIF_EXT_PHY_CTRL_5_SHDW);
	writel(EMIF_EXT_PHY_CTRL_6_VAL, base + EMIF_EXT_PHY_CTRL_6_SHDW);
	writel(EMIF_EXT_PHY_CTRL_7_VAL, base + EMIF_EXT_PHY_CTRL_7_SHDW);
	writel(EMIF_EXT_PHY_CTRL_8_VAL, base + EMIF_EXT_PHY_CTRL_8_SHDW);
	writel(EMIF_EXT_PHY_CTRL_9_VAL, base + EMIF_EXT_PHY_CTRL_9_SHDW);
	writel(EMIF_EXT_PHY_CTRL_10_VAL, base + EMIF_EXT_PHY_CTRL_10_SHDW);
	writel(EMIF_EXT_PHY_CTRL_11_VAL, base + EMIF_EXT_PHY_CTRL_11_SHDW);
	writel(EMIF_EXT_PHY_CTRL_12_VAL, base + EMIF_EXT_PHY_CTRL_12_SHDW);
	writel(EMIF_EXT_PHY_CTRL_13_VAL, base + EMIF_EXT_PHY_CTRL_13_SHDW);
	writel(EMIF_EXT_PHY_CTRL_14_VAL, base + EMIF_EXT_PHY_CTRL_14_SHDW);
	writel(EMIF_EXT_PHY_CTRL_15_VAL, base + EMIF_EXT_PHY_CTRL_15_SHDW);
	writel(EMIF_EXT_PHY_CTRL_16_VAL, base + EMIF_EXT_PHY_CTRL_16_SHDW);
	writel(EMIF_EXT_PHY_CTRL_17_VAL, base + EMIF_EXT_PHY_CTRL_17_SHDW);
	writel(EMIF_EXT_PHY_CTRL_18_VAL, base + EMIF_EXT_PHY_CTRL_18_SHDW);
	writel(EMIF_EXT_PHY_CTRL_19_VAL, base + EMIF_EXT_PHY_CTRL_19_SHDW);
	writel(EMIF_EXT_PHY_CTRL_20_VAL, base + EMIF_EXT_PHY_CTRL_20_SHDW);
	writel(EMIF_EXT_PHY_CTRL_21_VAL, base + EMIF_EXT_PHY_CTRL_21_SHDW);
	writel(EMIF_EXT_PHY_CTRL_22_VAL, base + EMIF_EXT_PHY_CTRL_22_SHDW);
	writel(EMIF_EXT_PHY_CTRL_23_VAL, base + EMIF_EXT_PHY_CTRL_23_SHDW);
	writel(EMIF_EXT_PHY_CTRL_24_VAL, base + EMIF_EXT_PHY_CTRL_24_SHDW);
}

static void __devexit cleanup_emif(struct emif_data *emif)
{
	if (emif) {
		if (emif->plat_data) {
			kfree(emif->plat_data->custom_configs);
			kfree(emif->plat_data->timings);
			kfree(emif->plat_data->min_tck);
			kfree(emif->plat_data->device_info);
			kfree(emif->plat_data);
		}
		kfree(emif);
	}
}

static void get_default_timings(struct emif_data *emif)
{
	struct emif_platform_data *pd = emif->plat_data;

	pd->timings = lpddr2_jedec_timings;
	pd->timings_arr_size = ARRAY_SIZE(lpddr2_jedec_timings);

	dev_warn(emif->dev, "Using default timings\n");
}

static int is_dev_data_valid(u32 type, u32 density, u32 io_width, u32 phy_type,
		u32 ip_rev, struct device *dev)
{
	int valid;

	valid = (type == DDR_TYPE_LPDDR2_S4 ||
			type == DDR_TYPE_LPDDR2_S2)
		&& (density >= DDR_DENSITY_64Mb
			&& density <= DDR_DENSITY_8Gb)
		&& (io_width >= DDR_IO_WIDTH_8
			&& io_width <= DDR_IO_WIDTH_32);

	/* Combinations of EMIF and PHY revisions that we support today */
	switch (ip_rev) {
	case EMIF_4D:
		valid = valid && (phy_type == EMIF_PHY_TYPE_ATTILAPHY);
		break;
	case EMIF_4D5:
		valid = valid && (phy_type == EMIF_PHY_TYPE_INTELLIPHY);
		break;
	default:
		valid = 0;
	}

	if (!valid)
		dev_err(dev, "Invalid DDR details\n");
	return valid;
}

static int is_custom_config_valid(struct emif_custom_configs *cust_cfgs,
		struct device *dev)
{
	int valid = 1;

	if ((cust_cfgs->mask & EMIF_CUSTOM_CONFIG_LPMODE) &&
		(cust_cfgs->lpmode != EMIF_LP_MODE_DISABLE))
		valid = cust_cfgs->lpmode_freq_threshold &&
			cust_cfgs->lpmode_timeout_performance &&
			cust_cfgs->lpmode_timeout_power;

	if (cust_cfgs->mask & EMIF_CUSTOM_CONFIG_TEMP_ALERT_POLL_INTERVAL)
		valid = valid && cust_cfgs->temp_alert_poll_interval_ms;

	if (!valid)
		dev_warn(dev, "Invalid custom configs\n");

	return valid;
}

#if defined(CONFIG_OF)
static void __devinit of_get_custom_configs(struct device_node *np_emif,
		struct emif_data *emif)
{
	struct emif_custom_configs	*cust_cfgs = NULL;
	int				len;
	const int			*lpmode, *poll_intvl;

	lpmode = of_get_property(np_emif, "low-power-mode", &len);
	poll_intvl = of_get_property(np_emif, "temp-alert-poll-interval", &len);

	if (lpmode || poll_intvl)
		cust_cfgs = kzalloc(sizeof(*cust_cfgs), GFP_KERNEL);

	if (!cust_cfgs)
		return;

	if (lpmode) {
		cust_cfgs->mask |= EMIF_CUSTOM_CONFIG_LPMODE;
		cust_cfgs->lpmode = *lpmode;
		of_property_read_u32(np_emif,
				"low-power-mode-timeout-performance",
				&cust_cfgs->lpmode_timeout_performance);
		of_property_read_u32(np_emif,
				"low-power-mode-timeout-power",
				&cust_cfgs->lpmode_timeout_power);
		of_property_read_u32(np_emif,
				"low-power-mode-freq-threshold",
				&cust_cfgs->lpmode_freq_threshold);
	}

	if (poll_intvl) {
		cust_cfgs->mask |=
				EMIF_CUSTOM_CONFIG_TEMP_ALERT_POLL_INTERVAL;
		cust_cfgs->temp_alert_poll_interval_ms = *poll_intvl;
	}

	if (!is_custom_config_valid(cust_cfgs, emif->dev)) {
		kfree(cust_cfgs);
		return;
	}

	emif->plat_data->custom_configs = cust_cfgs;
}

static void __devinit of_get_min_tck(struct device_node *np,
		struct emif_data *emif)
{
	int			ret = 0;
	struct ddr_min_tck	*min;

	min = kzalloc(sizeof(*min), GFP_KERNEL);
	if (!min)
		goto default_min_tck;

	ret |= of_property_read_u32(np, "tRPab-min-tck", &min->tRPab);
	ret |= of_property_read_u32(np, "tRCD-min-tck", &min->tRCD);
	ret |= of_property_read_u32(np, "tWR-min-tck", &min->tWR);
	ret |= of_property_read_u32(np, "tRASmin-min-tck", &min->tRASmin);
	ret |= of_property_read_u32(np, "tRRD-min-tck", &min->tRRD);
	ret |= of_property_read_u32(np, "tWTR-min-tck", &min->tWTR);
	ret |= of_property_read_u32(np, "tXP-min-tck", &min->tXP);
	ret |= of_property_read_u32(np, "tRTP-min-tck", &min->tRTP);
	ret |= of_property_read_u32(np, "tCKE-min-tck", &min->tCKE);
	ret |= of_property_read_u32(np, "tCKESR-min-tck", &min->tCKESR);
	ret |= of_property_read_u32(np, "tFAW-min-tck", &min->tFAW);

	if (ret) {
		kfree(min);
		goto default_min_tck;
	}

	emif->plat_data->min_tck = min;
	return;

default_min_tck:
	dev_warn(emif->dev, "Using default min-tck values\n");
	emif->plat_data->min_tck = &lpddr2_min_tck;
}

static int __devinit of_do_get_timings(struct device_node *np,
		struct lpddr2_timings *tim)
{
	int ret;

	ret = of_property_read_u32(np, "max-freq", &tim->max_freq);
	ret |= of_property_read_u32(np, "min-freq", &tim->min_freq);
	ret |= of_property_read_u32(np, "tRPab", &tim->tRPab);
	ret |= of_property_read_u32(np, "tRCD", &tim->tRCD);
	ret |= of_property_read_u32(np, "tWR", &tim->tWR);
	ret |= of_property_read_u32(np, "tRAS-min", &tim->tRAS_min);
	ret |= of_property_read_u32(np, "tRRD", &tim->tRRD);
	ret |= of_property_read_u32(np, "tWTR", &tim->tWTR);
	ret |= of_property_read_u32(np, "tXP", &tim->tXP);
	ret |= of_property_read_u32(np, "tRTP", &tim->tRTP);
	ret |= of_property_read_u32(np, "tCKESR", &tim->tCKESR);
	ret |= of_property_read_u32(np, "tDQSCK-max", &tim->tDQSCK_max);
	ret |= of_property_read_u32(np, "tFAW", &tim->tFAW);
	ret |= of_property_read_u32(np, "tZQCS", &tim->tZQCS);
	ret |= of_property_read_u32(np, "tZQCL", &tim->tZQCL);
	ret |= of_property_read_u32(np, "tZQinit", &tim->tZQinit);
	ret |= of_property_read_u32(np, "tRAS-max-ns", &tim->tRAS_max_ns);
	ret |= of_property_read_u32(np, "tRTW", &tim->tRTW);
	ret |= of_property_read_u32(np, "tAONPD", &tim->tAONPD);
	ret |= of_property_read_u32(np, "tDQSCK-max-derated",
		&tim->tDQSCK_max_derated);

	return ret;
}

static void __devinit of_get_ddr_timings(struct device_node *np_ddr,
		struct emif_data *emif)
{
	struct lpddr2_timings	*timings = NULL;
	u32			arr_sz = 0, i = 0;
	struct device_node	*np_tim;
	char			*tim_compat;

	switch (emif->plat_data->device_info->type) {
	case DDR_TYPE_LPDDR2_S2:
	case DDR_TYPE_LPDDR2_S4:
		tim_compat = "jedec,lpddr2-timings";
		break;
	default:
		dev_warn(emif->dev, "memory type not supported in DT\n");
	}

	for_each_child_of_node(np_ddr, np_tim)
		if (of_device_is_compatible(np_tim, tim_compat))
			arr_sz++;

	if (arr_sz)
		timings = kzalloc(sizeof(*timings) * arr_sz, GFP_KERNEL);

	if (!timings)
		goto default_timings;

	for_each_child_of_node(np_ddr, np_tim) {
		if (of_device_is_compatible(np_tim, tim_compat)) {
			if (of_do_get_timings(np_tim, &timings[i])) {
				kfree(timings);
				goto default_timings;
			}
			i++;
		}
	}

	emif->plat_data->timings = timings;
	emif->plat_data->timings_arr_size = arr_sz;

	return;

default_timings:
	get_default_timings(emif);

	return;
}

static void __devinit of_get_ddr_info(struct device_node *np_emif,
		struct device_node *np_ddr,
		struct ddr_device_info *dev_info)
{
	u32 density = 0, io_width = 0;
	int len;

	if (of_find_property(np_emif, "cs1-used", &len))
		dev_info->cs1_used = true;

	if (of_find_property(np_emif, "cal-resistor-per-cs", &len))
		dev_info->cal_resistors_per_cs = true;

	if (of_device_is_compatible(np_ddr , "jedec,lpddr2-s4"))
		dev_info->type = DDR_TYPE_LPDDR2_S4;
	else if (of_device_is_compatible(np_ddr , "jedec,lpddr2-s2"))
		dev_info->type = DDR_TYPE_LPDDR2_S2;

	of_property_read_u32(np_ddr, "density", &density);
	of_property_read_u32(np_ddr, "io-width", &io_width);

	/* Convert from density in Mb to the density encoding in jedc_ddr.h */
	if (density & (density - 1))
		dev_info->density = 0;
	else
		dev_info->density = __fls(density) - 5;

	/* Convert from io_width in bits to io_width encoding in jedc_ddr.h */
	if (io_width & (io_width - 1))
		dev_info->io_width = 0;
	else
		dev_info->io_width = __fls(io_width) - 1;
}

static struct emif_data * __devinit of_get_device_details(
		struct device_node *np_emif, struct device *dev)
{
	struct emif_data		*emif = NULL;
	struct ddr_device_info		*dev_info = NULL;
	struct emif_platform_data	*pd = NULL;
	struct device_node		*np_ddr;
	int				len;

	np_ddr = of_parse_phandle(np_emif, "device-handle", 0);
	if (!np_ddr)
		goto error;
	emif	= kzalloc(sizeof(struct emif_data), GFP_KERNEL);
	pd	= kzalloc(sizeof(*pd), GFP_KERNEL);
	dev_info = kzalloc(sizeof(*dev_info), GFP_KERNEL);

	if (!emif || !pd || !dev_info)
		goto error;

	emif->plat_data		= pd;
	pd->device_info		= dev_info;
	emif->dev		= dev;
	emif->np_ddr		= np_ddr;
	emif->temperature_level	= SDRAM_TEMP_NOMINAL;

	if (of_device_is_compatible(np_emif, "ti,emif-4d"))
		emif->plat_data->ip_rev = EMIF_4D;
	else if (of_device_is_compatible(np_emif, "ti,emif-4d5"))
		emif->plat_data->ip_rev = EMIF_4D5;

	of_property_read_u32(np_emif, "phy-type", &pd->phy_type);

	if (of_find_property(np_emif, "hw-caps-ll-interface", &len))
		pd->hw_caps |= EMIF_HW_CAPS_LL_INTERFACE;

	of_get_ddr_info(np_emif, np_ddr, dev_info);
	if (!is_dev_data_valid(pd->device_info->type, pd->device_info->density,
			pd->device_info->io_width, pd->phy_type, pd->ip_rev,
			emif->dev))
		goto error;
	/*
	 * For EMIF instances other than EMIF1 see if the devices connected
	 * are exactly same as on EMIF1(which is typically the case). If so,
	 * mark it as a duplicate of EMIF1. This will save some memory and
	 * computation.
	 */
	if (emif1 && emif1->np_ddr == np_ddr) {
		emif->duplicate = true;
		goto out;
	}

	of_get_custom_configs(np_emif, emif);
	of_get_ddr_timings(np_ddr, emif);
	of_get_min_tck(np_ddr, emif);
	goto out;

error:
	dev_err(dev, "of_get_device_details() failure!!\n");
	cleanup_emif(emif);
	return NULL;
out:
	return emif;
}
#endif

static struct emif_data * __devinit get_device_details(
		struct platform_device *pdev)
{
	u32			size;
	struct emif_data	*emif = NULL;
	struct ddr_device_info	*dev_info;
	struct emif_platform_data *pd;

	pd = pdev->dev.platform_data;

	if (!(pd && pd->device_info && is_dev_data_valid(pd->device_info->type,
			pd->device_info->density, pd->device_info->io_width,
			pd->phy_type, pd->ip_rev, &pdev->dev)))
		goto error;

	emif	= kzalloc(sizeof(struct emif_data), GFP_KERNEL);
	pd	= kmemdup(pd, sizeof(*pd), GFP_KERNEL);
	dev_info = kmemdup(pd->device_info,
			sizeof(*pd->device_info), GFP_KERNEL);

	if (!emif || !pd || !dev_info)
		goto error;

	emif->plat_data		= pd;
	emif->dev		= &pdev->dev;
	emif->temperature_level	= SDRAM_TEMP_NOMINAL;

	pd->device_info	= dev_info;

	/*
	 * For EMIF instances other than EMIF1 see if the devices connected
	 * are exactly same as on EMIF1(which is typically the case). If so,
	 * mark it as a duplicate of EMIF1 and skip copying timings data.
	 * This will save some memory and some computation later.
	 */
	emif->duplicate = emif1 && (memcmp(dev_info,
		emif1->plat_data->device_info,
		sizeof(struct ddr_device_info)) == 0);

	if (emif->duplicate) {
		pd->timings = NULL;
		pd->min_tck = NULL;
		goto out;
	}

	/*
	 * Copy custom configs - ignore allocation error, if any, as
	 * custom_configs is not very critical
	 */
	if (pd->custom_configs)
		pd->custom_configs = kmemdup(pd->custom_configs,
			sizeof(*pd->custom_configs), GFP_KERNEL);

	if (pd->custom_configs &&
	    !is_custom_config_valid(pd->custom_configs, emif->dev)) {
		kfree(pd->custom_configs);
		pd->custom_configs = NULL;
	}

	/*
	 * Copy timings and min-tck values from platform data. If it is not
	 * available or if memory allocation fails, use JEDEC defaults
	 */
	size = sizeof(struct lpddr2_timings) * pd->timings_arr_size;
	if (pd->timings)
		pd->timings = kmemdup(pd->timings, size, GFP_KERNEL);

	if (!pd->timings)
		get_default_timings(emif);

	if (pd->min_tck)
		pd->min_tck = kmemdup(pd->min_tck, sizeof(*pd->min_tck),
				GFP_KERNEL);

	if (!pd->min_tck) {
		pd->min_tck = &lpddr2_min_tck;
		dev_warn(emif->dev, "Using default min-tck values\n");
	}

	goto out;

error:
	dev_err(&pdev->dev, "get_device_details() failure!!\n");
	cleanup_emif(emif);
	return NULL;

out:
	return emif;
}

static int __devinit emif_probe(struct platform_device *pdev)
{
	struct emif_data	*emif;
	struct resource		*res;

#if defined(CONFIG_OF)
	if (pdev->dev.of_node)
		emif = of_get_device_details(pdev->dev.of_node, &pdev->dev);
	else
#endif
		emif = get_device_details(pdev);

	if (!emif)
		goto error;

	if (!emif1)
		emif1 = emif;

	emif->addressing = get_addressing_table(emif->plat_data->device_info);
	list_add(&emif->siblings, &device_list);

	/* Save pointers to each other in emif and device structures */
	emif->dev = &pdev->dev;
	platform_set_drvdata(pdev, emif);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto error;

	emif->base = ioremap(res->start, SZ_1M);
	if (!emif->base)
		goto error;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		goto error;
	emif->irq = res->start;

	emif_onetime_settings(emif);
	setup_interrupts(emif);
	emif_debugfs_init(emif);

	dev_info(&pdev->dev, "Device configured with addr = %p and IRQ%d\n",
			emif->base, emif->irq);
	return 0;

error:
	dev_err(&pdev->dev, "probe failure!!\n");
	cleanup_emif(emif);
	return -ENODEV;
}

static int __devexit emif_remove(struct platform_device *pdev)
{
	struct emif_data *emif = platform_get_drvdata(pdev);

	emif_debugfs_exit(emif);
	cleanup_emif(emif);

	return 0;
}

static void emif_shutdown(struct platform_device *pdev)
{
	struct emif_data	*emif = platform_get_drvdata(pdev);
	void __iomem		*base = emif->base;

	/* Disable all interrupts */
	writel(readl(base + EMIF_SYSTEM_OCP_INTERRUPT_ENABLE_SET),
		base + EMIF_SYSTEM_OCP_INTERRUPT_ENABLE_CLEAR);
	if (emif->plat_data->hw_caps & EMIF_HW_CAPS_LL_INTERFACE)
		writel(readl(base + EMIF_LL_OCP_INTERRUPT_ENABLE_SET),
			base + EMIF_LL_OCP_INTERRUPT_ENABLE_CLEAR);

	/* Clear all interrupts */
	clear_all_interrupts(emif);
}
static int get_emif_reg_values(struct emif_data *emif, u32 freq,
		struct emif_regs *regs)
{
	u32				cs1_used, ip_rev, phy_type;
	u32				cl, type;
	const struct lpddr2_timings	*timings;
	const struct ddr_min_tck	*min_tck;
	const struct ddr_device_info	*device_info;
	const struct lpddr2_addressing	*addressing;
	struct emif_data		*emif_for_calc;
	struct device			*dev;
	const struct emif_custom_configs *custom_configs;

	dev = emif->dev;
	/*
	 * If the devices on this EMIF instance is duplicate of EMIF1,
	 * use EMIF1 details for the calculation
	 */
	emif_for_calc	= emif->duplicate ? emif1 : emif;
	timings		= get_timings_table(emif_for_calc, freq);
	addressing	= emif_for_calc->addressing;
	if (!timings || !addressing) {
		dev_err(dev, "not enough data available for %dHz", freq);
		return -1;
	}

	device_info	= emif_for_calc->plat_data->device_info;
	type		= device_info->type;
	cs1_used	= device_info->cs1_used;
	ip_rev		= emif_for_calc->plat_data->ip_rev;
	phy_type	= emif_for_calc->plat_data->phy_type;

	min_tck		= emif_for_calc->plat_data->min_tck;
	custom_configs	= emif_for_calc->plat_data->custom_configs;

	set_ddr_clk_period(freq);

	regs->ref_ctrl_shdw = get_sdram_ref_ctrl_shdw(freq, addressing);
	regs->sdram_tim1_shdw = get_sdram_tim_1_shdw(timings, min_tck,
			addressing, ip_rev);
	regs->sdram_tim2_shdw = get_sdram_tim_2_shdw(timings, min_tck,
			addressing, type, ip_rev);
	regs->sdram_tim3_shdw = get_sdram_tim_3_shdw(timings, min_tck,
		addressing, type, ip_rev, EMIF_NORMAL_TIMINGS);

	cl = get_cl(emif);

	if (phy_type == EMIF_PHY_TYPE_ATTILAPHY && ip_rev == EMIF_4D) {
		regs->phy_ctrl_1_shdw = get_ddr_phy_ctrl_1_attilaphy_4d(
			timings, freq, cl);
	} else if (phy_type == EMIF_PHY_TYPE_INTELLIPHY && ip_rev == EMIF_4D5) {
		regs->phy_ctrl_1_shdw = get_phy_ctrl_1_intelliphy_4d5(freq, cl);
		regs->ext_phy_ctrl_2_shdw = get_ext_phy_ctrl_2_intelliphy_4d5();
		regs->ext_phy_ctrl_3_shdw = get_ext_phy_ctrl_3_intelliphy_4d5();
		regs->ext_phy_ctrl_4_shdw = get_ext_phy_ctrl_4_intelliphy_4d5();
	} else {
		return -1;
	}

	/* Only timeout values in pwr_mgmt_ctrl_shdw register */
	regs->pwr_mgmt_ctrl_shdw =
		get_pwr_mgmt_ctrl(freq, emif_for_calc, ip_rev) &
		(CS_TIM_MASK | SR_TIM_MASK | PD_TIM_MASK);

	if (ip_rev & EMIF_4D) {
		regs->read_idle_ctrl_shdw_normal =
			get_read_idle_ctrl_shdw(DDR_VOLTAGE_STABLE);

		regs->read_idle_ctrl_shdw_volt_ramp =
			get_read_idle_ctrl_shdw(DDR_VOLTAGE_RAMPING);
	} else if (ip_rev & EMIF_4D5) {
		regs->dll_calib_ctrl_shdw_normal =
			get_dll_calib_ctrl_shdw(DDR_VOLTAGE_STABLE);

		regs->dll_calib_ctrl_shdw_volt_ramp =
			get_dll_calib_ctrl_shdw(DDR_VOLTAGE_RAMPING);
	}

	if (type == DDR_TYPE_LPDDR2_S2 || type == DDR_TYPE_LPDDR2_S4) {
		regs->ref_ctrl_shdw_derated = get_sdram_ref_ctrl_shdw(freq / 4,
			addressing);

		regs->sdram_tim3_shdw_derated = get_sdram_tim_3_shdw(timings,
			min_tck, addressing, type, ip_rev,
			EMIF_DERATED_TIMINGS);

		regs->sdram_tim1_shdw_derated =
			get_sdram_tim_1_shdw_derated(timings, min_tck,
				addressing, ip_rev);
	}

	regs->freq = freq;

	return 0;
}

/*
 * get_regs() - gets the cached emif_regs structure for a given EMIF instance
 * given frequency(freq):
 *
 * As an optimisation, every EMIF instance other than EMIF1 shares the
 * register cache with EMIF1 if the devices connected on this instance
 * are same as that on EMIF1(indicated by the duplicate flag)
 *
 * If we do not have an entry corresponding to the frequency given, we
 * allocate a new entry and calculate the values
 *
 * Upon finding the right reg dump, save it in curr_regs. It can be
 * directly used for thermal de-rating and voltage ramping changes.
 */
static struct emif_regs *get_regs(struct emif_data *emif, u32 freq)
{
	int			i;
	struct emif_regs	**regs_cache;
	struct emif_regs	*regs = NULL;
	struct device		*dev;

	dev = emif->dev;
	if (emif->curr_regs && emif->curr_regs->freq == freq) {
		dev_dbg(dev, "Using curr_regs - %u Hz", freq);
		return emif->curr_regs;
	}

	if (emif->duplicate)
		regs_cache = emif1->regs_cache;
	else
		regs_cache = emif->regs_cache;

	for (i = 0; i < EMIF_MAX_NUM_FREQUENCIES && regs_cache[i]; i++) {
		if (regs_cache[i]->freq == freq) {
			regs = regs_cache[i];
			dev_dbg(dev, "Reg dump found in reg cache for %u Hz\n",
				freq);
			break;
		}
	}

	/*
	 * If we don't have an entry for this frequency in the cache create one
	 * and calculate the values
	 */
	if (!regs) {
		regs = kmalloc(sizeof(struct emif_regs), GFP_ATOMIC);
		if (!regs)
			return NULL;

		if (get_emif_reg_values(emif, freq, regs)) {
			kfree(regs);
			return NULL;
		}

		/*
		 * Now look for an un-used entry in the cache and save the
		 * newly created struct. If there are no free entries
		 * over-write the last entry
		 */
		for (i = 0; i < EMIF_MAX_NUM_FREQUENCIES && regs_cache[i]; i++)
			;

		if (i >= EMIF_MAX_NUM_FREQUENCIES) {
			dev_warn(dev, "regs_cache full - reusing a slot!!\n");
			i = EMIF_MAX_NUM_FREQUENCIES - 1;
			kfree(regs_cache[i]);
		}
		regs_cache[i] = regs;
	}

	return regs;
}

/*
 * Function un-used right now. Will be used later when DVFS framework
 * is available
 */
static void __attribute__((unused)) do_volt_notify_handling(
		struct emif_data *emif, u32 volt_state)
{
	dev_dbg(emif->dev, "voltage notification : %d", volt_state);

	if (!emif->curr_regs) {
		dev_err(emif->dev,
			"Volt-notify before registers are ready: %d\n",
			volt_state);
		return;
	}

	setup_volt_sensitive_regs(emif, emif->curr_regs, volt_state);
	do_freq_update();
}

/*
 * Function un-used right now. Will be used later when DVFS framework
 * is available
 */
static void __attribute__((unused)) do_freq_pre_notify_handling(void *emif_data,
		u32 new_freq)
{
	struct emif_regs *regs;
	struct emif_data *emif = emif_data;

	regs = get_regs(emif, new_freq);
	if (!regs)
		return;

	emif->curr_regs = regs;

	/*
	 * Update the shadow registers:
	 * Temperature and voltage-ramp sensitive settings are also configured
	 * in terms of DDR cycles. So, we need to update them too when there
	 * is a freq change
	 */
	dev_dbg(emif->dev, "setting up shadow registers for %uHz", new_freq);
	setup_registers(emif, regs);
	setup_temperature_sensitive_regs(emif, regs);
	setup_volt_sensitive_regs(emif, regs, DDR_VOLTAGE_STABLE);
}

#if defined(CONFIG_OF)
static const struct of_device_id omap_emif_of_match[] = {
		{ .compatible = "ti,emif-4d" },
		{ .compatible = "ti,emif-4d5" },
		{},
};
MODULE_DEVICE_TABLE(of, omap_emif_of_match);
#endif

void emif_freq_pre_notify_handler(u32 new_freq)
{
	struct emif_data *emif;
	list_for_each_entry(emif, &device_list, siblings) {
		do_freq_pre_notify_handling(emif, new_freq);
	}
}
EXPORT_SYMBOL(emif_freq_pre_notify_handler);

static struct platform_driver omap_emif_driver = {
	.probe		= emif_probe,
	.remove		= __devexit_p(emif_remove),
	.shutdown	= emif_shutdown,
	.driver = {
		.name = "emif",
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(omap_emif_of_match),
#endif
	},
};

static int __init omap_emif_register(void)
{
	return platform_driver_register(&omap_emif_driver);
}
