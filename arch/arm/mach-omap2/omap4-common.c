/*
 * OMAP4 specific common source file.
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Author:
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>

#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>
#include <asm/memblock.h>
#include <asm/smp_twd.h>

#include <plat/irqs.h>
#include <plat/sram.h>
#include <plat/omap-secure.h>

#include <mach/hardware.h>
#include <mach/omap-wakeupgen.h>

#include "common.h"
#include "omap4-sar-layout.h"
#include <linux/export.h>

#ifdef CONFIG_CACHE_L2X0
#define L2X0_POR_OFFSET_VALUE	0x5
#define L2X0_POR_OFFSET_MASK	0x1f
static void __iomem *l2cache_base;
#endif

static void __iomem *gic_dist_base_addr;

#ifdef CONFIG_OMAP4_ERRATA_I688
/* Used to implement memory barrier on DRAM path */
#define OMAP4_DRAM_BARRIER_VA			0xfe600000

void __iomem *dram_sync, *sram_sync;

static phys_addr_t paddr;
static u32 size;

void omap_bus_sync(void)
{
	if (dram_sync && sram_sync) {
		writel_relaxed(readl_relaxed(dram_sync), dram_sync);
		writel_relaxed(readl_relaxed(sram_sync), sram_sync);
		isb();
	}
}
EXPORT_SYMBOL(omap_bus_sync);

/* Steal one page physical memory for barrier implementation */
int __init omap_barrier_reserve_memblock(void)
{

	size = ALIGN(PAGE_SIZE, SZ_1M);
	paddr = arm_memblock_steal(size, SZ_1M);

	return 0;
}

void __init omap_barriers_init(void)
{
	struct map_desc dram_io_desc[1];

	dram_io_desc[0].virtual = OMAP4_DRAM_BARRIER_VA;
	dram_io_desc[0].pfn = __phys_to_pfn(paddr);
	dram_io_desc[0].length = size;
	dram_io_desc[0].type = MT_MEMORY_SO;
	iotable_init(dram_io_desc, ARRAY_SIZE(dram_io_desc));
	dram_sync = (void __iomem *) dram_io_desc[0].virtual;
	sram_sync = (void __iomem *) OMAP4_SRAM_VA;

	pr_info("OMAP4: Map 0x%08llx to 0x%08lx for dram barrier\n",
		(long long) paddr, dram_io_desc[0].virtual);

}
#else
void __init omap_barriers_init(void)
{}
#endif

void __init gic_init_irq(void)
{
	void __iomem *omap_irq_base;

	/* Static mapping, never released */
	if (cpu_is_omap44xx())
		gic_dist_base_addr = ioremap(OMAP44XX_GIC_DIST_BASE, SZ_4K);
	else
		gic_dist_base_addr = ioremap(OMAP54XX_GIC_DIST_BASE, SZ_4K);

	BUG_ON(!gic_dist_base_addr);

	/* Static mapping, never released */
	if (cpu_is_omap44xx())
		omap_irq_base = ioremap(OMAP44XX_GIC_CPU_BASE, SZ_512);
	else
		omap_irq_base = ioremap(OMAP54XX_GIC_CPU_BASE, SZ_512);

	BUG_ON(!omap_irq_base);

	omap_wakeupgen_init();

	gic_init(0, 29, gic_dist_base_addr, omap_irq_base);
}

void gic_dist_disable(void)
{
	if (gic_dist_base_addr)
		__raw_writel(0x0, gic_dist_base_addr + GIC_DIST_CTRL);
}

u32 gic_readl(u32 offset, u8 idx)
{
	return __raw_readl(gic_dist_base_addr + offset + 4 * idx);
}


bool gic_dist_disabled(void)
{
	return !(__raw_readl(gic_dist_base_addr + GIC_DIST_CTRL) & 0x1);
}

void gic_dist_enable(void)
{
	__raw_writel(0x1, gic_dist_base_addr + GIC_DIST_CTRL);
}

void gic_timer_retrigger(void)
{
	u32 twd_int = __raw_readl(twd_base + TWD_TIMER_INTSTAT);
	u32 gic_int = __raw_readl(gic_dist_base_addr + GIC_DIST_PENDING_SET);
	u32 twd_ctrl = __raw_readl(twd_base + TWD_TIMER_CONTROL);

	if (twd_int && !(gic_int & BIT(OMAP44XX_IRQ_LOCALTIMER))) {
		/*
		 * The local timer interrupt got lost while the distributor was
		 * disabled.  Ack the pending interrupt, and retrigger it.
		 */
		pr_warn("%s: lost localtimer interrupt\n", __func__);
		__raw_writel(1, twd_base + TWD_TIMER_INTSTAT);
		if (!(twd_ctrl & TWD_TIMER_CONTROL_PERIODIC)) {
			__raw_writel(1, twd_base + TWD_TIMER_COUNTER);
			twd_ctrl |= TWD_TIMER_CONTROL_ENABLE;
			__raw_writel(twd_ctrl, twd_base + TWD_TIMER_CONTROL);
		}
	}
}

#ifdef CONFIG_CACHE_L2X0

void __iomem *omap4_get_l2cache_base(void)
{
	return l2cache_base;
}

static void omap4_l2x0_disable(void)
{
	/* Disable PL310 L2 Cache controller */
	omap_smc1(0x102, 0x0);
}

static void omap4_l2x0_set_debug(unsigned long val)
{
	/* Program PL310 L2 Cache controller debug register */
	omap_smc1(0x100, val);
}

static int __init omap_l2_cache_init(void)
{
	u32 aux_ctrl = 0;
	u32 por_ctrl = 0;
	u32 lockdown = 0;

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx())
		return -ENODEV;

	/* Static mapping, never released */
	l2cache_base = ioremap(OMAP44XX_L2CACHE_BASE, SZ_4K);
	if (WARN_ON(!l2cache_base))
		return -ENOMEM;

	/*
	 * 16-way associativity, parity disabled
	 * Way size - 32KB (es1.0)
	 * Way size - 64KB (es2.0 +)
	 */
	aux_ctrl = readl_relaxed(l2cache_base + L2X0_AUX_CTRL);
	aux_ctrl = ((1 << L2X0_AUX_CTRL_ASSOCIATIVITY_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_REPLACE_POLICY_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_NS_LOCKDOWN_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_NS_INT_CTRL_SHIFT));

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		aux_ctrl |= 0x2 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT;
	} else {
		/*
		 * Drop instruction prefetch as it degrades performances
		 */
		aux_ctrl |= ((0x3 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) |
			(1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) |
			(1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) |
			(1 << L2X0_AUX_CTRL_EARLY_BRESP_SHIFT));
	}
	if (omap_rev() != OMAP4430_REV_ES1_0)
		omap_smc1(0x109, aux_ctrl);

	/* Setup POR Control register */
	por_ctrl = readl_relaxed(l2cache_base + L2X0_PREFETCH_CTRL);

	/*
	 * Double linefill is available only on OMAP4460 L2X0.
	 * It may cause single cache line memory corruption, leave it disabled
	 * on all devices
	 */
	por_ctrl &= ~(1 << L2X0_PREFETCH_DOUBLE_LINEFILL_SHIFT);
	por_ctrl &= ~L2X0_POR_OFFSET_MASK;
	por_ctrl |= L2X0_POR_OFFSET_VALUE;

	/* Set POR through Monitor API only in GP devices */
	if (omap_type() == OMAP2_DEVICE_TYPE_GP &&
		omap_rev() >= OMAP4430_REV_ES2_2)
		omap_smc1(0x113, por_ctrl);

	/*
	 * WA for OMAP4460 stability issue on ES1.0
	 * Lock-down specific L2 cache ways which  makes effective
	 * L2 size as 512 KB instead of 1 MB
	 */
	if (omap_rev() == OMAP4460_REV_ES1_0) {
		pr_info("4460 ES1.0 Cache Issue workaround enabled\n");
		lockdown = 0xa5a5;
		writel_relaxed(lockdown, l2cache_base + L2X0_LOCKDOWN_WAY_D0);
		writel_relaxed(lockdown, l2cache_base + L2X0_LOCKDOWN_WAY_D1);
		writel_relaxed(lockdown, l2cache_base + L2X0_LOCKDOWN_WAY_I0);
		writel_relaxed(lockdown, l2cache_base + L2X0_LOCKDOWN_WAY_I1);
	}

	/* Enable PL310 L2 Cache controller */
	omap_smc1(0x102, 0x1);

	l2x0_init(l2cache_base, aux_ctrl, L2X0_AUX_CTRL_MASK);

	/*
	 * Override default outer_cache.disable with a OMAP4
	 * specific one
	*/
	outer_cache.disable = omap4_l2x0_disable;
	outer_cache.set_debug = omap4_l2x0_set_debug;

	return 0;
}
early_initcall(omap_l2_cache_init);
#endif

