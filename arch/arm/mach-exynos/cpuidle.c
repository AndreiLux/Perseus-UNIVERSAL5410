/* linux/arch/arm/mach-exynos/cpuidle.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#ifdef CONFIG_SEC_PM
#include <linux/moduleparam.h>
#endif

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/unified.h>
#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/tlbflush.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/asv.h>
#include <mach/devfreq.h>

#include <plat/pm.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-core.h>
#include <plat/usb-phy.h>
#include <plat/audio.h>
#include <plat/clock.h>
#include <mach/sec_debug.h>

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)
#else
#define REG_DIRECTGO_ADDR	EXYNOS_INFORM0
#define REG_DIRECTGO_FLAG	EXYNOS_INFORM1
#endif

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10
#define EXYNOS_CHECK_LPA	0xABAD0000

#ifdef CONFIG_SEC_PM
#if defined(CONFIG_V1A) || defined(CONFIG_N1A)
#define CPUIDLE_ENABLE_MASK (ENABLE_C2 | ENABLE_C3_AFTR)
#else
#define CPUIDLE_ENABLE_MASK (ENABLE_C2 | ENABLE_C3_AFTR | ENABLE_C3_LPA)
#endif

#ifdef CONFIG_SND_SAMSUNG_USE_IDMA_DRAM
extern int check_dram_status(void);
#endif

static enum {
	ENABLE_C2	= BIT(0),
	ENABLE_C3_AFTR	= BIT(1),
	ENABLE_C3_LPA	= BIT(2),
} enable_mask = CPUIDLE_ENABLE_MASK;

DEFINE_SPINLOCK(enable_mask_lock);

static int set_enable_mask(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_uint(val, kp);
	unsigned long flags;

	if (rv)
		return rv;

	spin_lock_irqsave(&enable_mask_lock, flags);

	if (!(enable_mask & ENABLE_C2)) {
		unsigned int cpuid = smp_processor_id();
		int i;
		for_each_online_cpu(i) {
			if (i == cpuid)
				continue;
			arm_send_ping_ipi(i);
		}
	}

	spin_unlock_irqrestore(&enable_mask_lock, flags);

	return 0;
}

static struct kernel_param_ops enable_mask_param_ops = {
	.set = set_enable_mask,
	.get = param_get_uint,
};

module_param_cb(enable_mask, &enable_mask_param_ops, &enable_mask, 0644);
MODULE_PARM_DESC(enabled_mask, "bitmask for C state - C2, AFTR, LPA");
#endif

#ifdef CONFIG_SEC_PM_DEBUG
unsigned int log_en;
module_param_named(log_en, log_en, uint, 0644);
#endif

#if defined(CONFIG_BT_BCM4339)
extern int bt_is_running;
extern void bt_uart_rts_ctrl(int flag);
#endif
static int exynos_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			      int index);
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos_enter_c2(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv,
				 int index);
#endif
static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);

struct check_reg_lpa {
	void __iomem	*check_reg;
	unsigned int	check_bit;
};

/*
 * List of check power domain list for LPA mode
 * These register are have to power off to enter LPA mode
 */
static struct check_reg_lpa exynos5_power_domain[] = {
	{.check_reg = EXYNOS5_GSCL_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5_ISP_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5410_MFC_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5410_G3D_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5410_DISP1_STATUS,	.check_bit = 0x7},
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */
static struct check_reg_lpa exynos5_clock_gating[] = {
	{.check_reg = EXYNOS5_CLKGATE_IP_DISP1,		.check_bit = 0x00000008},
	{.check_reg = EXYNOS5_CLKGATE_IP_MFC,		.check_bit = 0x00000001},
	{.check_reg = EXYNOS5_CLKGATE_IP_GEN,		.check_bit = 0x0000001E},
	{.check_reg = EXYNOS5_CLKGATE_BUS_FSYS0,	.check_bit = 0x00000006},
#if defined (CONFIG_SOC_EXYNOS5410)
	{.check_reg = EXYNOS5_CLKGATE_IP_PERIC,         .check_bit = 0x003F7FC0},
#else
	{.check_reg = EXYNOS5_CLKGATE_IP_PERIC,		.check_bit = 0x00077FC0},
#endif
};

static struct clk *clkm_phy;
static bool mif_max = false;

#if defined(CONFIG_EXYNOS_DEV_DWMCI)
enum hc_type {
	HC_SDHC,
	HC_MSHC,
};

struct check_device_op {
	void __iomem		*base;
	struct platform_device	*pdev;
	enum hc_type		type;
};

static struct check_device_op chk_sdhc_op[] = {
	{.base = 0, .pdev = &exynos5_device_dwmci0, .type = HC_MSHC},
	{.base = 0, .pdev = &exynos5_device_dwmci1, .type = HC_MSHC},
	{.base = 0, .pdev = &exynos5_device_dwmci2, .type = HC_MSHC},
};

static int sdmmc_dev_num;
/* If SD/MMC interface is working: return = 1 or not 0 */
static int check_sdmmc_op(unsigned int ch)
{
	if (unlikely(ch >= sdmmc_dev_num)) {
		pr_err("Invalid ch[%d] for SD/MMC\n", ch);
		return 0;
	}
	if (soc_is_exynos5410())
		return (__raw_readl(EXYNOS5_CLKSRC_MASK_FSYS) & (1 << (ch * 4))) ? 1 : 0;
	else
		return (__raw_readl(EXYNOS5_CLKSRC_MASK_FSYS) & (1 << ((ch * 4) + 8))) ? 1 : 0;
}

/* Check all sdmmc controller */
static int loop_sdmmc_check(void)
{
	unsigned int iter;

	for (iter = 0; iter < sdmmc_dev_num; iter++) {
		if (check_sdmmc_op(iter)) {
			pr_debug("SDMMC [%d] working\n", iter);
			return 1;
		}
	}
	return 0;
}
#endif

static int exynos_check_reg_status(struct check_reg_lpa *reg_list,
				    unsigned int list_cnt)
{
	unsigned int i;
	unsigned int tmp;

	for (i = 0; i < list_cnt; i++) {
		tmp = __raw_readl(reg_list[i].check_reg);
		if (tmp & reg_list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

static int exynos_uart_fifo_check(void)
{
	unsigned int ret;
	unsigned int check_val;

	ret = 0;

	/* Check UART for console is empty */
	check_val = __raw_readl(S5P_VA_UART(CONFIG_S3C_LOWLEVEL_UART_PORT) +
				0x18);

	ret = ((check_val >> 16) & 0xff);

	return ret;
}

#if	defined(CONFIG_GPS_BCMxxxxx)
static int check_gps_op(void)
{
	/* This	pin	is high	when gps is working */
	int	gps_is_running = gpio_get_value(GPIO_GPS_PWR_EN);
	return gps_is_running;
}
#endif

static int check_bt_op(void)
{
#if defined(CONFIG_BT_BCM4339)
	return bt_is_running;
#else
	return 0;
#endif
}
static int __maybe_unused exynos_check_enter_mode(void)
{
#ifdef CONFIG_SEC_PM
	if (!(enable_mask & ENABLE_C3_LPA))
		return EXYNOS_CHECK_DIDLE;
#endif
	/* Check power domain */
	if (exynos_check_reg_status(exynos5_power_domain,
			    ARRAY_SIZE(exynos5_power_domain)))
		return EXYNOS_CHECK_DIDLE;

	/* Check clock gating */
	if (exynos_check_reg_status(exynos5_clock_gating,
			    ARRAY_SIZE(exynos5_clock_gating)))
		return EXYNOS_CHECK_DIDLE;

#ifdef CONFIG_SND_SAMSUNG_USE_IDMA_DRAM
	if (check_dram_status())
		return EXYNOS_CHECK_DIDLE;
#endif

#if defined(CONFIG_SOC_EXYNOS5420)
	if (check_adma_status())
		return EXYNOS_CHECK_DIDLE;
#endif
#if defined(CONFIG_EXYNOS_DEV_DWMCI)
	if (loop_sdmmc_check())
		return EXYNOS_CHECK_DIDLE;
#endif
#if	defined(CONFIG_GPS_BCMxxxxx)
	if (check_gps_op())
		return EXYNOS_CHECK_DIDLE;
#endif
	if (exynos_check_usb_op())
		return EXYNOS_CHECK_DIDLE;

	return EXYNOS_CHECK_LPA;
}

static struct cpuidle_state exynos5_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= exynos_enter_idle,
		.exit_latency		= 1,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM clock gating(WFI)",
	},
	[1] = {
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		.enter			= exynos_enter_c2,
		.exit_latency		= 30,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C2",
		.desc			= "ARM power down",
	},
	[2] = {
#endif
		.enter                  = exynos_enter_lowpower,
		.exit_latency           = 300,
		.target_residency       = 5000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C3",
		.desc                   = "ARM power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);

static struct cpuidle_driver exynos_idle_driver = {
	.name		= "exynos_idle",
	.owner		= THIS_MODULE,
};

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(void)
{
	__raw_writel(0x40003ffe, EXYNOS_WAKEUP_MASK);
}

#if !defined(CONFIG_ARM_TRUSTZONE)
static unsigned int g_pwr_ctrl, g_diag_reg;

static void save_cpu_arch_register(void)
{
	/*read power control register*/
	asm("mrc p15, 0, %0, c15, c0, 0" : "=r"(g_pwr_ctrl) : : "cc");
	/*read diagnostic register*/
	asm("mrc p15, 0, %0, c15, c0, 1" : "=r"(g_diag_reg) : : "cc");
	return;
}

static void restore_cpu_arch_register(void)
{
	/*write power control register*/
	asm("mcr p15, 0, %0, c15, c0, 0" : : "r"(g_pwr_ctrl) : "cc");
	/*write diagnostic register*/
	asm("mcr p15, 0, %0, c15, c0, 1" : : "r"(g_diag_reg) : "cc");
	return;
}
#else
static void save_cpu_arch_register(void)
{
}

static void restore_cpu_arch_register(void)
{
}
#endif

static int idle_finisher(unsigned long flags)
{
#if defined(CONFIG_ARM_TRUSTZONE)
	if (soc_is_exynos5420())
		flush_cache_all();

	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);
#else
	cpu_do_idle();
#endif
	return 1;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int c2_finisher(unsigned long flags)
{
#if defined(CONFIG_ARM_TRUSTZONE)
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	/*
	 * Secure monitor disables the SMP bit and takes the CPU out of the
	 * coherency domain.
	 */
	local_flush_tlb_all();
#else
	cpu_do_idle();
#endif
	return 1;
}
#endif

static int exynos_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	unsigned long tmp;
	unsigned int ret = 0;
	unsigned int cpuid = smp_processor_id();

	local_irq_disable();
	sec_debug_task_log_msg(cpuid, "aftr+");
#ifdef CONFIG_SEC_PM_DEBUG
	if (log_en & ENABLE_C3_AFTR)
		pr_info("+++aftr\n");
#endif
	do_gettimeofday(&before);

	exynos_set_wakeupmask();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	if (soc_is_exynos5410() || soc_is_exynos5420())
		exynos_disable_idle_clock_down(KFC);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

	if (soc_is_exynos5410() || soc_is_exynos5420())
		exynos_enable_idle_clock_down(KFC);

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);

	do_gettimeofday(&after);
#ifdef CONFIG_SEC_PM_DEBUG
	if (log_en & ENABLE_C3_AFTR)
		pr_info("---aftr\n");
#endif
	sec_debug_task_log_msg(cpuid, "aftr-");

	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static struct sleep_save exynos5_lpa_save[] = {
	/* CMU side */
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_TOP),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_GSCL),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_DISP1_0),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_MAUDIO),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_PERIC0),
	SAVE_ITEM(EXYNOS5_CLKSRC_MASK_PERIC1),
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP3),
#if defined(CONFIG_SOC_EXYNOS5420)
	SAVE_ITEM(EXYNOS5_CLKSRC_TOP5),
#endif
};

static struct sleep_save exynos5_set_clksrc[] = {
	{ .reg = EXYNOS5_CLKSRC_MASK_TOP		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_GSCL		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_DISP1_0		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_MAUDIO		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_FSYS		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_PERIC0		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_PERIC1		, .val = 0xffffffff, },
};

static int exynos_enter_core0_lpa(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned long tmp;
	unsigned int cpuid = smp_processor_id();

	/*
	 * Before enter central sequence mode, clock src register have to set
	 */
	s3c_pm_do_save(exynos5_lpa_save, ARRAY_SIZE(exynos5_lpa_save));
	s3c_pm_do_restore_core(exynos5_set_clksrc,
			       ARRAY_SIZE(exynos5_set_clksrc));

	local_irq_disable();
	sec_debug_task_log_msg(cpuid, "lpa+");
#ifdef CONFIG_SEC_PM_DEBUG
	if (log_en & ENABLE_C3_LPA)
		pr_info("+++lpa\n");
#endif
	do_gettimeofday(&before);

	/*
	 * Unmasking all wakeup source.
	 */
	__raw_writel(0x7FFFE000, EXYNOS_WAKEUP_MASK);

#if defined(CONFIG_BT_BCM4339)
	bt_uart_rts_ctrl(1);
#endif
	/* Configure GPIO Power down control register */
	exynos_set_lpa_pdn(S3C_GPIO_END);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_LPA);

	if (soc_is_exynos5410() || soc_is_exynos5420())
		exynos_disable_idle_clock_down(KFC);

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	do {
		/* Waiting for flushing UART fifo */
	} while (exynos_uart_fifo_check());

	set_boot_flag(cpuid, C2_STATE);

	if (soc_is_exynos5420()) {
		__raw_writel(EXYNOS_CHECK_LPA, EXYNOS_PMU_SPARE1);

		tmp = __raw_readl(EXYNOS5420_SFR_AXI_CGDIS1_REG);
		tmp |= (EXYNOS5420_UFS | EXYNOS5420_ACE_KFC | EXYNOS5420_ACE_EAGLE);
		__raw_writel(tmp, EXYNOS5420_SFR_AXI_CGDIS1_REG);

		exynos5_mif_transition_disable(true);

		if (clkm_phy->usage) {
			mif_max = true;
			clk_disable(clkm_phy);
		}
	}
	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

		goto early_wakeup;
	}
#ifdef CONFIG_SMP
#if !defined(CONFIG_ARM_TRUSTZONE)
	scu_enable(S5P_VA_SCU);
#endif
#endif
	/* For release retention */
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_GPIO_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_MMCA_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_MMCB_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_MMCC_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIB_OPTION);
	if (soc_is_exynos5420())
		__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_HSI_OPTION);

early_wakeup:
	if (soc_is_exynos5420()) {
		if (mif_max) {
			clk_enable(clkm_phy);
			mif_max = false;
		}

		exynos5_mif_transition_disable(false);

		__raw_writel(0, EXYNOS_PMU_SPARE1);

		tmp = __raw_readl(EXYNOS5420_SFR_AXI_CGDIS1_REG);
		tmp &= ~(EXYNOS5420_UFS | EXYNOS5420_ACE_KFC | EXYNOS5420_ACE_EAGLE);
		__raw_writel(tmp, EXYNOS5420_SFR_AXI_CGDIS1_REG);

		exynos5_mif_nocp_resume();
	}

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

	if (soc_is_exynos5410() || soc_is_exynos5420())
		exynos_enable_idle_clock_down(KFC);

	s3c_pm_do_restore_core(exynos5_lpa_save,
			       ARRAY_SIZE(exynos5_lpa_save));

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS_WAKEUP_STAT);

#if defined(CONFIG_BT_BCM4339)
	bt_uart_rts_ctrl(0);
#endif
	do_gettimeofday(&after);
#ifdef CONFIG_SEC_PM_DEBUG
	if (log_en & ENABLE_C3_LPA)
		pr_info("---lpa\n");
#endif
	sec_debug_task_log_msg(cpuid, "lpa-");

	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;

	local_irq_disable();
	do_gettimeofday(&before);

	cpu_do_idle();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static unsigned int exynos_get_core_num(void)
{
	unsigned int cluster_id = read_cpuid_mpidr() & 0x100;
	unsigned int pwr_offset = 0;
	unsigned int cpu;
	unsigned int tmp;

	struct cpumask cpu_power_on_mask;

	cpumask_clear(&cpu_power_on_mask);

	if (soc_is_exynos5410()) {
		if (samsung_rev() < EXYNOS5410_REV_1_0) {
			if (cluster_id == 0)
				pwr_offset = 4;
		} else {
			if (cluster_id != 0)
				pwr_offset = 4;
		}
	} else {

		if (cluster_id != 0)
			pwr_offset = 4;
	}

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		tmp = __raw_readl(EXYNOS_ARM_CORE_STATUS(cpu + pwr_offset));

		if (tmp & EXYNOS_CORE_LOCAL_PWR_EN)
			cpumask_set_cpu(cpu, &cpu_power_on_mask);
	}

	return cpumask_weight(&cpu_power_on_mask);
}

static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int new_index = index;
	unsigned int cluster_id = read_cpuid_mpidr() & 0x100;

	if (!cluster_id)
		return exynos_enter_c2(dev, drv, (new_index - 1));

	/* This mode only can be entered when other core's are offline */
	if (num_online_cpus() > 1)
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		return exynos_enter_c2(dev, drv, (new_index - 1));
#else
		return exynos_enter_idle(dev, drv, (new_index - 2));
#endif
	if (exynos_get_core_num() > 1)
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		return exynos_enter_c2(dev, drv, (new_index - 1));
#else
		return exynos_enter_idle(dev, drv, (new_index - 2));
#endif

#ifdef CONFIG_SEC_PM
	if (exynos_check_enter_mode() == EXYNOS_CHECK_DIDLE) {
		if (enable_mask & ENABLE_C3_AFTR)
			return exynos_enter_core0_aftr(dev, drv, new_index);
		else
			return exynos_enter_idle(dev, drv, (new_index - 2));
	} else {
		return exynos_enter_core0_lpa(dev, drv, new_index);
	}
#else
	if (exynos_check_enter_mode() == EXYNOS_CHECK_DIDLE)
		return exynos_enter_core0_aftr(dev, drv, new_index);
	else
		return exynos_enter_core0_lpa(dev, drv, new_index);
#endif
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned int cpuid = smp_processor_id(), cpu_offset = 0;
	unsigned int cluster_id = read_cpuid(CPUID_MPIDR) >> 8 & 0xf;
	unsigned int value;

#ifdef CONFIG_SEC_PM
	if (!(enable_mask & ENABLE_C2))
		return exynos_enter_idle(dev, drv, (index - 1));
#endif
	/* HACK : Disabling C2 on KFC for EXYNOS5420 */
	if (soc_is_exynos5420())
	        return exynos_enter_idle(dev, drv, (index - 1));

	local_irq_disable();
	do_gettimeofday(&before);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	set_boot_flag(cpuid, C2_STATE);
	sec_debug_task_log_msg(cpuid, "c2+");
	cpu_pm_enter();

	if (soc_is_exynos5410()) {
		if (samsung_rev() < EXYNOS5410_REV_1_0) {
			if (cluster_id == 0)
				cpu_offset = cpuid + 4;
			else
				cpu_offset = cpuid;
		} else {
			if (cluster_id == 0)
				cpu_offset = cpuid;
			else
				cpu_offset = cpuid + 4;
		}
	} else {

		if (cluster_id == 0)
			cpu_offset = cpuid;
		else
			cpu_offset = cpuid + 4;
	}

	__raw_writel(0x0, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));

	value = __raw_readl(EXYNOS5410_ARM_INTR_SPREAD_ENABLE);
	value &= ~(0x1 << cpu_offset);
	__raw_writel(value, EXYNOS5410_ARM_INTR_SPREAD_ENABLE);

	ret = cpu_suspend(0, c2_finisher);
	if (ret)
		__raw_writel(0x3, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));

	value = __raw_readl(EXYNOS5410_ARM_INTR_SPREAD_ENABLE);
	value |= (0x1 << cpu_offset);
	__raw_writel(value, EXYNOS5410_ARM_INTR_SPREAD_ENABLE);

	clear_boot_flag(cpuid, C2_STATE);
	if (ret)
		sec_debug_task_log_msg(cpuid, "c2_");    /* early wakeup */
	else
		sec_debug_task_log_msg(cpuid, "c2-");    /* normal wakeup */
	cpu_pm_exit();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}
#endif

void exynos_enable_idle_clock_down(unsigned int cluster)
{
	unsigned int tmp;

	if (cluster) {
		/* For A15 core */
		tmp = __raw_readl(EXYNOS5_PWR_CTRL1);
		tmp &= ~((0x7 << 28) | (0x7 << 16) | (1 << 9) | (1 << 8));
		tmp |= (0x7 << 28) | (0x7 << 16) | 0x3ff;
		__raw_writel(tmp, EXYNOS5_PWR_CTRL1);

		tmp = __raw_readl(EXYNOS5_PWR_CTRL2);
		tmp &= ~((0x3 << 24) | (0xffff << 8) | (0x77));
		tmp |= (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0);
		tmp |= (1 << 25) | (1 << 24);
		__raw_writel(tmp, EXYNOS5_PWR_CTRL2);
	} else {
		/* For A7 core */
		tmp = __raw_readl(EXYNOS5_PWR_CTRL_KFC);
		tmp &= ~((0x3F << 16) | (1 << 8));
		tmp |= (0x3F << 16) | 0x1ff;
		__raw_writel(tmp, EXYNOS5_PWR_CTRL_KFC);

		tmp = __raw_readl(EXYNOS5_PWR_CTRL2_KFC);
		tmp &= ~((0x1 << 24) | (0xffff << 8) | (0x7));
		tmp |= (1 << 16) | (1 << 8) | (1 << 0);
		tmp |= 1 << 24;
		__raw_writel(tmp, EXYNOS5_PWR_CTRL2_KFC);
	}

	pr_debug("%s idle clock down is enabled\n", cluster ? "ARM" : "KFC");
}

void exynos_disable_idle_clock_down(unsigned int cluster)
{
	unsigned int tmp;

	if (cluster) {
		/* For A15 core */
		tmp = __raw_readl(EXYNOS5_PWR_CTRL1);
		tmp &= ~((0x7 << 28) | (0x7 << 16) | (1 << 9) | (1 << 8));
		__raw_writel(tmp, EXYNOS5_PWR_CTRL1);

		tmp = __raw_readl(EXYNOS5_PWR_CTRL2);
		tmp &= ~((0x3 << 24) | (0xffff << 8) | (0x77));
		__raw_writel(tmp, EXYNOS5_PWR_CTRL2);
	} else {
		/* For A7 core */
		tmp = __raw_readl(EXYNOS5_PWR_CTRL_KFC);
		tmp &= ~((0x7 << 16) | (1 << 8));
		__raw_writel(tmp, EXYNOS5_PWR_CTRL_KFC);

		tmp = __raw_readl(EXYNOS5_PWR_CTRL2_KFC);
		tmp &= ~((0x1 << 24) | (0xffff << 8) | (0x7));
		__raw_writel(tmp, EXYNOS5_PWR_CTRL2_KFC);
	}

	pr_debug("%s idle clock down is disabled\n", cluster ? "ARM" : "KFC");
}

static int exynos_cpuidle_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		disable_hlt();
		pr_debug("PM_SUSPEND_PREPARE for CPUIDLE\n");
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		enable_hlt();
		pr_debug("PM_POST_SUSPEND for CPUIDLE\n");
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block exynos_cpuidle_notifier = {
	.notifier_call = exynos_cpuidle_notifier_event,
};

static int __init exynos_init_cpuidle(void)
{
	int i, max_cpuidle_state, cpu_id;
	struct cpuidle_device *device;
	struct cpuidle_driver *drv = &exynos_idle_driver;
	struct cpuidle_state *idle_set;
	struct platform_device *pdev;
	struct resource *res;

	if (soc_is_exynos5410() || soc_is_exynos5420()) {
		exynos_enable_idle_clock_down(ARM);
		exynos_enable_idle_clock_down(KFC);
	}

	/* Setup cpuidle driver */
	idle_set = exynos5_cpuidle_set;
	drv->state_count = ARRAY_SIZE(exynos5_cpuidle_set);

	max_cpuidle_state = drv->state_count;
	for (i = 0; i < max_cpuidle_state; i++) {
		memcpy(&drv->states[i], &idle_set[i],
				sizeof(struct cpuidle_state));
	}
	drv->safe_state_index = 0;
	cpuidle_register_driver(&exynos_idle_driver);

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

	device->state_count = max_cpuidle_state;

	if (cpuidle_register_device(device)) {
		printk(KERN_ERR "CPUidle register device failed\n,");
		return -EIO;
		}
	}

#if defined(CONFIG_EXYNOS_DEV_DWMCI)
	sdmmc_dev_num = ARRAY_SIZE(chk_sdhc_op);

	for (i = 0; i < sdmmc_dev_num; i++) {

		pdev = chk_sdhc_op[i].pdev;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			pr_err("failed to get iomem region\n");
			return -EINVAL;
		}

		chk_sdhc_op[i].base = ioremap(res->start, resource_size(res));

		if (!chk_sdhc_op[i].base) {
			pr_err("failed to map io region\n");
			return -EINVAL;
		}
	}
#endif

	if (soc_is_exynos5420()) {
		clkm_phy = clk_get(NULL, "clkm_phy");
		if (IS_ERR(clkm_phy)) {
			pr_err("Cannot get clock \"clkm_phy\"\n");
			return PTR_ERR(clkm_phy);
		}
	}

	register_pm_notifier(&exynos_cpuidle_notifier);

	return 0;
}
device_initcall(exynos_init_cpuidle);
