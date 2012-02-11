/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/cma.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/regs-serial.h>

#include <mach/map.h>
#include <mach/sysmmu.h>
#include <mach/exynos-ion.h>

#include "common.h"

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK5250_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK5250_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK5250_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg smdk5250_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK5250_UCON_DEFAULT,
		.ulcon		= SMDK5250_ULCON_DEFAULT,
		.ufcon		= SMDK5250_UFCON_DEFAULT,
	},
};

#ifdef CONFIG_CMA
/* defined in arch/arm/mach-exynos/reserve-mem.c */
extern void exynos_cma_region_reserve(struct cma_region *,
				struct cma_region *, size_t, const char *);

static void __init exynos_reserve_mem(void)
{
	static struct cma_region regions[] = {
		{
			.name = "ion",
			.size = CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
			{
				.alignment = SZ_1M
			}
		}, {
			.size = 0 /* END OF REGION DEFINITIONS */
		}
	};

	static const char map[] __initconst = "ion-exynos=ion;";

	exynos_cma_region_reserve(regions, NULL, 0, map);
}
#else /* CONFIG_CMA */
static inline void exynos_reserve_mem(void)
{
}
#endif

static struct platform_device *smdk5250_devices[] __initdata = {
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
	&SYSMMU_PLATDEV(2d),
	&SYSMMU_PLATDEV(jpeg),
	&SYSMMU_PLATDEV(gsc0),
	&SYSMMU_PLATDEV(gsc1),
	&SYSMMU_PLATDEV(gsc2),
	&SYSMMU_PLATDEV(gsc3),
	&SYSMMU_PLATDEV(flite0),
	&SYSMMU_PLATDEV(flite1),
	&SYSMMU_PLATDEV(tv),
	&SYSMMU_PLATDEV(rot),
	&SYSMMU_PLATDEV(is_isp),
	&SYSMMU_PLATDEV(is_drc),
	&SYSMMU_PLATDEV(is_fd),
	&SYSMMU_PLATDEV(is_cpu),
	&SYSMMU_PLATDEV(is_odc),
	&SYSMMU_PLATDEV(is_sclrc),
	&SYSMMU_PLATDEV(is_sclrp),
	&SYSMMU_PLATDEV(is_dis0),
	&SYSMMU_PLATDEV(is_dis1),
	&SYSMMU_PLATDEV(is_3dnr),
#ifdef CONFIG_ION_EXYNOS
	&exynos_device_ion,
#endif
};

static void __init smdk5250_map_io(void)
{
	clk_xusbxti.rate = 24000000;

	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(clk_xusbxti.rate);
	s3c24xx_init_uarts(smdk5250_uartcfgs, ARRAY_SIZE(smdk5250_uartcfgs));
}

static void __init exynos_sysmmu_init(void)
{
#ifdef CONFIG_VIDEO_JPEG_V2X
	sysmmu_set_owner(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_l).dev, &s5p_device_mfc.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_r).dev, &s5p_device_mfc.dev);
#endif
#if defined(CONFIG_VIDEO_EXYNOS_TV)
	ASSIGN_SYSMMU_POWERDOMAIN(tv, &exynos5_device_pd[PD_DISP1].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_mixer.dev);

#endif
#ifdef CONFIG_VIDEO_EXYNOS_GSCALER
	ASSIGN_SYSMMU_POWERDOMAIN(gsc0, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(gsc1, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(gsc2, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(gsc3, &exynos5_device_pd[PD_GSCL].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc0).dev, &exynos5_device_gsc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc1).dev, &exynos5_device_gsc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc2).dev, &exynos5_device_gsc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(gsc3).dev, &exynos5_device_gsc3.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	ASSIGN_SYSMMU_POWERDOMAIN(flite0, &exynos5_device_pd[PD_GSCL].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(flite1, &exynos5_device_pd[PD_GSCL].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(flite0).dev,
						&exynos_device_flite0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(flite1).dev,
						&exynos_device_flite1.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_ROTATOR
	sysmmu_set_owner(&SYSMMU_PLATDEV(rot).dev, &exynos_device_rotator.dev);
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(2d).dev, &s5p_device_fimg2d.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS5_FIMC_IS
	ASSIGN_SYSMMU_POWERDOMAIN(is_isp, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_drc, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_fd, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_cpu, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_odc, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_sclrc, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_sclrp, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_dis0, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_dis1, &exynos5_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_3dnr, &exynos5_device_pd[PD_ISP].dev);

	sysmmu_set_owner(&SYSMMU_PLATDEV(is_isp).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_drc).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_fd).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_cpu).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_odc).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_sclrc).dev
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_sclrp).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_dis0).dev
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_dis1).dev,
						&exynos5_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_3dnr).dev,
						&exynos5_device_fimc_is.dev);
#endif
}

static void __init smdk5250_machine_init(void)
{
	exynos_sysmmu_init();
	exynos_reserve_mem();
	exynos_ion_set_platdata();

	platform_add_devices(smdk5250_devices, ARRAY_SIZE(smdk5250_devices));
}

MACHINE_START(SMDK5250, "SMDK5250")
	.atag_offset	= 0x100,
	.init_irq	= exynos5_init_irq,
	.map_io		= smdk5250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= smdk5250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos5_restart,
MACHINE_END
