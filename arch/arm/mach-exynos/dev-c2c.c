/* linux/arch/arm/mach-exynos/dev-c2c.c
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Base EXYNOS C2C resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <mach/c2c.h>
#include <plat/irqs.h>
#include <plat/cpu.h>

static struct resource exynos_c2c_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS_PA_C2C, SZ_64K),
	[1] = DEFINE_RES_MEM(EXYNOS_PA_C2C_CP, SZ_64K),
	[2] = DEFINE_RES_IRQ(IRQ_C2C_SSCM0),
	[3] = DEFINE_RES_IRQ(IRQ_C2C_SSCM1),
};

static u64 exynos_c2c_dma_mask = DMA_BIT_MASK(32);

struct platform_device exynos_device_c2c = {
	.name		= "samsung-c2c",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos_c2c_resource),
	.resource	= exynos_c2c_resource,
	.dev		= {
		.dma_mask		= &exynos_c2c_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

void __init exynos_c2c_set_platdata(struct exynos_c2c_platdata *pd)
{
	struct exynos_c2c_platdata *npd = pd;
	pr_err("[C2C] %s: +++\n", __func__);

	if (!npd->setup_gpio)
		npd->setup_gpio = exynos_c2c_cfg_gpio;

	if (soc_is_exynos5250()) {
		/* Set C2C_CTRL Register */
		writel(0x1, EXYNOS_C2C_CTRL);
	} else if (soc_is_exynos5410()) {
		static void __iomem *c2c_ext_addr;

		pr_err("[C2C] %s: SOC == Exynos5410\n", __func__);

		/* Set C2C_CTRL Register */
		c2c_ext_addr = ioremap(0x100431E8, SZ_4);
		if (!c2c_ext_addr) {
			pr_err("Memory Allocation Err for PMU retention(C2C)\n");
			return ;
		}
		npd->pmu_retention = c2c_ext_addr;

		c2c_ext_addr = ioremap(0x100429C0, SZ_4);
		if (!c2c_ext_addr) {
			pr_err("Memory Allocation Err for PMU retention(C2C)\n");
			return ;
		}
		npd->c2c_ddrphy_dlllock = c2c_ext_addr;

		c2c_ext_addr = ioremap(0x10030910, SZ_4);
		if (!c2c_ext_addr) {
			pr_err("Memory Allocation Err for C2C_STATE\n");
			return ;
		}
		npd->c2c_state_reg = c2c_ext_addr;


		/* ioremap for extention C2C registers */
		c2c_ext_addr = ioremap(EXYNOS_PA_C2C_EXT, SZ_64);
		if (!c2c_ext_addr) {
			pr_err("[C2C] %s: ioremap fail\n", __func__);
			return;
		}

		/* Set C2C configure register address */
		npd->c2c_sysreg = c2c_ext_addr + 0x20;

		/* Set C2C_MREQ_INFO register to gain highest QoS */
		writel(0xf, c2c_ext_addr + 0x30);
	}

	if (readl(npd->c2c_sysreg) == 0x0)
		writel(C2C_SYSREG_DEFAULT, npd->c2c_sysreg);

	exynos_device_c2c.dev.platform_data = npd;
}

