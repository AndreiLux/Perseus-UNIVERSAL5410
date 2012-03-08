/*
 * linux/arch/arm/mach-exynos/dev-dwmci.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Platform device for Synopsys DesignWare Mobile Storage IP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/mmc/host.h>

#include <plat/devs.h>
#include <plat/cpu.h>

#include <mach/map.h>

#define DWMCI_CLKSEL	0x09c

static int exynos_dwmci_get_bus_wd(u32 slot_id)
{
	return 4;
}

static int exynos_dwmci_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;

	/* Set Phase Shift Register */
	host->pdata->ddr_timing = 0x00010000;
	host->pdata->sdr_timing = 0x00010000;

	return 0;
}

static void exynos_dwmci_set_io_timing(void *data, unsigned char timing)
{
	struct dw_mci *host = (struct dw_mci *)data;

	if (timing == MMC_TIMING_UHS_DDR50)
		__raw_writel(host->pdata->ddr_timing,
			host->regs + DWMCI_CLKSEL);
	else
		__raw_writel(host->pdata->sdr_timing,
			host->regs + DWMCI_CLKSEL);
}

static struct dw_mci_board exynos_dwmci_pdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION,
	.bus_hz			= 80 * 1000 * 1000,
	.detect_delay_ms	= 200,
	.init			= exynos_dwmci_init,
	.get_bus_wd		= exynos_dwmci_get_bus_wd,
	.set_io_timing		= exynos_dwmci_set_io_timing,
};

static u64 exynos_dwmci_dmamask = DMA_BIT_MASK(32);

#define EXYNOS_DWMCI_RESOURCE(_series) \
static struct resource exynos##_series##_dwmci_resource[] = { \
	[0] = DEFINE_RES_MEM(EXYNOS##_series##_PA_DWMCI, SZ_4K), \
	[1] = DEFINE_RES_IRQ(EXYNOS##_series##_IRQ_DWMCI), \
};

EXYNOS_DWMCI_RESOURCE(4)
EXYNOS_DWMCI_RESOURCE(5)

#define EXYNOS_DWMCI_PLATFORM_DEVICE(_series) \
struct platform_device exynos##_series##_device_dwmci = \
{\
	.name		= "dw_mmc", \
	.id		= -1, \
	.num_resources	= ARRAY_SIZE(exynos##_series##_dwmci_resource), \
	.resource	= exynos##_series##_dwmci_resource, \
	.dev		= { \
		.dma_mask		= &exynos_dwmci_dmamask, \
		.coherent_dma_mask	= DMA_BIT_MASK(32), \
		.platform_data		= &exynos_dwmci_pdata, \
	}, \
};

EXYNOS_DWMCI_PLATFORM_DEVICE(4)
EXYNOS_DWMCI_PLATFORM_DEVICE(5)

void __init exynos_dwmci_set_platdata(struct dw_mci_board *pd)
{
	struct dw_mci_board *npd;

	if ((soc_is_exynos4210()) || soc_is_exynos4212() ||
		soc_is_exynos4412())
		npd = s3c_set_platdata(pd, sizeof(struct dw_mci_board),
			&exynos4_device_dwmci);
	else if (soc_is_exynos5250())
		npd = s3c_set_platdata(pd, sizeof(struct dw_mci_board),
			&exynos5_device_dwmci);
	else
		npd = NULL;

	if (!npd->init)
		npd->init = exynos_dwmci_init;
	if (!npd->get_bus_wd)
		npd->get_bus_wd = exynos_dwmci_get_bus_wd;
	if (!npd->set_io_timing)
		npd->set_io_timing = exynos_dwmci_set_io_timing;
}
