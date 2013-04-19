/* linux/arch/arm/mach-exynos/include/mach/pmu.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_PMU_H
#define __ASM_ARCH_PMU_H __FILE__

#define PMU_TABLE_END	NULL
#define CLUSTER_NUM	2

enum sys_powerdown {
	SYS_AFTR,
	SYS_LPA,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};

enum c2c_pwr_mode {
	MIN_LATENCY,
	SHORT_LATENCY,
	MAX_LATENCY,
};

enum running_cpu {
	KFC,
	ARM,
};

extern unsigned long l2x0_regs_phys;
struct exynos_pmu_conf {
	void __iomem *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

extern void exynos_sys_powerdown_conf(enum sys_powerdown mode);
extern void exynos_xxti_sys_powerdown(bool enable);
extern void s3c_cpu_resume(void);
extern void exynos_reset_assert_ctrl(bool on);
extern void exynos_set_core_flag(void);
extern void exynos_l2_common_pwr_ctrl(void);
extern void exynos_enable_idle_clock_down(unsigned int cluster);
extern void exynos_disable_idle_clock_down(unsigned int cluster);
extern void exynos_lpi_mask_ctrl(bool on);
extern void exynos_set_dummy_state(bool on);

#endif /* __ASM_ARCH_PMU_H */
