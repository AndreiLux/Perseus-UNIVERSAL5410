/* linux/arch/arm/mach-exynos/dev-sysmmu.c
 *
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - System MMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/iovmm.h>

#include <mach/map.h>
#include <mach/irqs.h>
#include <mach/sysmmu.h>

static u64 exynos_sysmmu_dma_mask = DMA_BIT_MASK(32);

#define SYSMMU_PLATFORM_DEVICE(ipname, devid)				\
static struct sysmmu_platform_data platdata_##ipname = {		\
	.dbgname = #ipname,						\
	.qos = -1,							\
	.prop = SYSMMU_PROP_READWRITE,					\
	.tlbinv_entry = false,						\
};									\
struct platform_device SYSMMU_PLATDEV(ipname) =				\
{									\
	.name		= SYSMMU_DEVNAME_BASE,				\
	.id		= devid,					\
	.dev		= {						\
		.dma_mask		= &exynos_sysmmu_dma_mask,	\
		.coherent_dma_mask	= DMA_BIT_MASK(32),		\
		.platform_data		= &platdata_##ipname,		\
	},								\
}

SYSMMU_PLATFORM_DEVICE(mfc_lr,	0);
SYSMMU_PLATFORM_DEVICE(tv,	2);
SYSMMU_PLATFORM_DEVICE(jpeg,	3);
SYSMMU_PLATFORM_DEVICE(rot,	4);
SYSMMU_PLATFORM_DEVICE(fimc0,	5); /* fimc* and gsc* exist exclusively */
SYSMMU_PLATFORM_DEVICE(fimc1,	6);
SYSMMU_PLATFORM_DEVICE(fimc2,	7);
SYSMMU_PLATFORM_DEVICE(fimc3,	8);
SYSMMU_PLATFORM_DEVICE(gsc0,	5);
SYSMMU_PLATFORM_DEVICE(gsc1,	6);
SYSMMU_PLATFORM_DEVICE(gsc2,	7);
SYSMMU_PLATFORM_DEVICE(gsc3,	8);
SYSMMU_PLATFORM_DEVICE(isp0,	9); /* 1st group of SysMMU 1.x in ISP */
SYSMMU_PLATFORM_DEVICE(fimd0,	10);
SYSMMU_PLATFORM_DEVICE(fimd1,	11);
SYSMMU_PLATFORM_DEVICE(camif0,	12);
SYSMMU_PLATFORM_DEVICE(camif1,	13);
SYSMMU_PLATFORM_DEVICE(camif2,	14);
SYSMMU_PLATFORM_DEVICE(2d,	15);
SYSMMU_PLATFORM_DEVICE(isp1,	16); /* 2nd group of SysMMU 1.x in ISP */
SYSMMU_PLATFORM_DEVICE(isp2,	17); /* Group of SysMMU 3.x in ISP */
SYSMMU_PLATFORM_DEVICE(scaler,	18);
SYSMMU_PLATFORM_DEVICE(s3d,	19);
SYSMMU_PLATFORM_DEVICE(mjpeg,	20);
SYSMMU_PLATFORM_DEVICE(isp3,	21); /* Gr. of SysMMU for ISP in other block */
SYSMMU_PLATFORM_DEVICE(2d_wr,	22);
SYSMMU_PLATFORM_DEVICE(scaler0r, 23);
SYSMMU_PLATFORM_DEVICE(scaler0w, 24);
SYSMMU_PLATFORM_DEVICE(scaler1r, 25);
SYSMMU_PLATFORM_DEVICE(scaler1w, 26);
SYSMMU_PLATFORM_DEVICE(scaler2r, 27);
SYSMMU_PLATFORM_DEVICE(scaler2w, 28);
SYSMMU_PLATFORM_DEVICE(mjpeg2,	29);
SYSMMU_PLATFORM_DEVICE(fimd1a,	30);

#define SYSMMU_RESOURCE_NAME(core, ipname) sysmmures_##core##_##ipname

#define SYSMMU_RESOURCE(core, ipname)					\
	static struct resource SYSMMU_RESOURCE_NAME(core, ipname)[] __initdata =

#define DEFINE_SYSMMU_RESOURCE(core, mem, irq)				\
	DEFINE_RES_MEM_NAMED(core##_PA_SYSMMU_##mem, SZ_4K, #mem),	\
	DEFINE_RES_IRQ_NAMED(core##_IRQ_SYSMMU_##irq##_0, #mem)

#define SYSMMU_RESOURCE_DEFINE(core, ipname, mem, irq)			\
	SYSMMU_RESOURCE(core, ipname) {					\
		DEFINE_SYSMMU_RESOURCE(core, mem, irq)			\
	}

struct sysmmu_qos_map {
	struct platform_device *pdev;
	short qos;
};
#define SYSMMU_QOS_MAPPING(ipname, val) {		\
	.pdev = &SYSMMU_PLATDEV(ipname),		\
	.qos = val,					\
}

struct sysmmu_version_map {
	struct platform_device *pdev;
	struct sysmmu_version ver;
};
#define SYSMMU_VERSION_MAPPING(ipname, maj, min) {	\
	.pdev = &SYSMMU_PLATDEV(ipname),		\
	.ver = {					\
		.major = maj,				\
		.minor = min,				\
	},						\
}

struct sysmmu_prop_map {
	struct platform_device *pdev;
	enum sysmmu_property prop;
};
#define SYSMMU_PROPERTY_MAPPING(ipname, property) {	\
	.pdev = &SYSMMU_PLATDEV(ipname),		\
	.prop = (enum sysmmu_property)(property),	\
}

struct sysmmu_tlbinv_map {
	struct platform_device *pdev;
	bool tlbinv_entry;
};
#define SYSMMU_TLBINVENTRY_MAPPING(ipname, val) {	\
	.pdev = &SYSMMU_PLATDEV(ipname),		\
	.tlbinv_entry = val,				\
}

struct sysmmu_child_norpm_map {
	struct platform_device *pdev;
	bool no_child_rpm;
};

#define SYSMMU_CHILD_NORPM(ipname) {	\
	.pdev = &SYSMMU_PLATDEV(ipname),	\
	.no_child_rpm = true,			\
}

struct sysmmu_resource_map {
	struct platform_device *pdev;
	struct resource *res;
	u32 rnum;
	char *clockname;
};

#define SYSMMU_RESOURCE_MAPPING(core, ipname, resname) {		\
	.pdev = &SYSMMU_PLATDEV(ipname),				\
	.res = SYSMMU_RESOURCE_NAME(EXYNOS##core, resname),		\
	.rnum = ARRAY_SIZE(SYSMMU_RESOURCE_NAME(EXYNOS##core, resname)),\
}

#define SYSMMU_RESOURCE_MAPPING_CLKDEP(core, ipname, resname, clkname) {\
	.pdev = &SYSMMU_PLATDEV(ipname),				\
	.res = SYSMMU_RESOURCE_NAME(EXYNOS##core, resname),		\
	.rnum = ARRAY_SIZE(SYSMMU_RESOURCE_NAME(EXYNOS##core, resname)),\
	.clockname = clkname,						\
}

#ifdef CONFIG_ARCH_EXYNOS4
SYSMMU_RESOURCE_DEFINE(EXYNOS4, fimc0,	FIMC0,	FIMC0);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, fimc1,	FIMC1,	FIMC1);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, fimc2,	FIMC2,	FIMC2);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, fimc3,	FIMC3,	FIMC3);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, jpeg,	JPEG,	JPEG);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, 2d,	G2D,	2D);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, tv,	TV,	TV_M0);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, 2d_acp,	G2D_ACP, 2D);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, rot,	ROTATOR, ROTATOR);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, fimd0,	FIMD0,	LCD0_M0);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, fimd1,	FIMD1,	LCD1_M1);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, flite0,	FIMC_LITE0, FIMC_LITE0);
SYSMMU_RESOURCE_DEFINE(EXYNOS4, flite1,	FIMC_LITE1, FIMC_LITE1);
SYSMMU_RESOURCE(EXYNOS4, mfc_lr) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS4, MFC_L, MFC_M0),
	DEFINE_SYSMMU_RESOURCE(EXYNOS4, MFC_R, MFC_M1),
};
SYSMMU_RESOURCE(EXYNOS4, isp0) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS4, ISP, FIMC_ISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS4, DRC, FIMC_DRC),
	DEFINE_SYSMMU_RESOURCE(EXYNOS4, FD, FIMC_FD),
};
SYSMMU_RESOURCE_DEFINE(EXYNOS4, isp1, ISPCPU, FIMC_CX);

static struct sysmmu_resource_map sysmmu_resmap4[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(4, fimc0,	fimc0),
	SYSMMU_RESOURCE_MAPPING(4, fimc1,	fimc1),
	SYSMMU_RESOURCE_MAPPING(4, fimc2,	fimc2),
	SYSMMU_RESOURCE_MAPPING(4, fimc3,	fimc3),
	SYSMMU_RESOURCE_MAPPING(4, tv,	tv),
	SYSMMU_RESOURCE_MAPPING(4, mfc_lr,	mfc_lr),
	SYSMMU_RESOURCE_MAPPING(4, rot,	rot),
	SYSMMU_RESOURCE_MAPPING(4, jpeg,	jpeg),
	SYSMMU_RESOURCE_MAPPING(4, fimd0,	fimd0),
};

static struct sysmmu_resource_map sysmmu_resmap4210[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(4, 2d,	2d),
	SYSMMU_RESOURCE_MAPPING(4, fimd1,	fimd1),
};

static struct sysmmu_resource_map sysmmu_resmap4212[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(4,	2d,	2d_acp),
	SYSMMU_RESOURCE_MAPPING(4,	camif0, flite0),
	SYSMMU_RESOURCE_MAPPING(4,	camif1, flite1),
	SYSMMU_RESOURCE_MAPPING(4,	isp0,	isp0),
	SYSMMU_RESOURCE_MAPPING(4,	isp1,	isp1),
};
#endif /* CONFIG_ARCH_EXYNOS4 */

#ifdef CONFIG_ARCH_EXYNOS5
SYSMMU_RESOURCE_DEFINE(EXYNOS5, 2d,	2D,	2D);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, rot,	ROTATOR, ROTATOR);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, tv,	TV,	TV);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc0,	GSC0,	GSC0);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, flite0,	LITE0,	LITE0);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, flite1,	LITE1,	LITE1);
SYSMMU_RESOURCE(EXYNOS5, mfc_lr) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, MFC_R, MFC_R),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, MFC_L, MFC_L),
};

SYSMMU_RESOURCE(EXYNOS5, isp1) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS5,	ODC, ODC),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, DIS0, DIS0),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, DIS1, DIS1),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, 3DNR, 3DNR),
};
SYSMMU_RESOURCE_DEFINE(EXYNOS5, isp2, SCALERP, SCALERPISP);

static struct sysmmu_resource_map sysmmu_resmap5[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(5,	2d,	2d),
	SYSMMU_RESOURCE_MAPPING(5,	tv,	tv),
	SYSMMU_RESOURCE_MAPPING(5,	isp1,	isp1),
	SYSMMU_RESOURCE_MAPPING(5,	isp2,	isp2),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	rot,	rot, "rotator"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc0,	gsc0, "gscl"),
};

#ifdef CONFIG_SOC_EXYNOS5250
SYSMMU_RESOURCE_DEFINE(EXYNOS5, fimd1,	FIMD1,	FIMD1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, flite2g, LITE2,	LITE2);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, jpeg,	JPEG,	JPEG);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc1g,	GSC1,	GSC1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc2,	GSC2,	GSC2);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc3,	GSC3,	GSC3);
SYSMMU_RESOURCE(EXYNOS5, isp0_g) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, ISP, ISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, DRC, DRC),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, FD, FD),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, ISPCPU, MCUISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, SCALERC, SCALERCISP),
};

static struct sysmmu_resource_map sysmmu_resmap5250[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(5,	isp0,	isp0_g),
	SYSMMU_RESOURCE_MAPPING(5,	fimd1,	fimd1),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	jpeg,	jpeg, "jpeg"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif2,	flite2g, NULL),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc1,	gsc1g, "gscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc2,	gsc2, "gscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc3,	gsc3, "gscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	mfc_lr,	mfc_lr, "mfc"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif0,	flite0, "fimc-lite.0"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif1,	flite1, "fimc-lite.1"),
};
#endif

#ifdef CONFIG_SOC_EXYNOS5410
SYSMMU_RESOURCE_DEFINE(EXYNOS5, fimd1,	FIMD1,	FIMD1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, flite2a, LITE2A, LITE2);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, s3d, S3D, S3D);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, jpega, JPEG, JPEGA);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, jpegm, MJPEG, JPEG);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler, SCALER, SCALER);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, fimd0, FIMD0, FIMD0);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc1a,	GSC1A,	GSC1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc2,	GSC2,	GSC2);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc3,	GSC3,	GSC3);
SYSMMU_RESOURCE(EXYNOS5, isp0_a) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, ISP, ISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, DRC, DRCA),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, FD, FD),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, ISPCPU, MCUISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, SCALERC, SCALERCISP),
};
SYSMMU_RESOURCE_DEFINE(EXYNOS5, isp3,	3AA,	3AA);

static struct sysmmu_resource_map sysmmu_resmap5410[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(5,	fimd0,	fimd0),
	SYSMMU_RESOURCE_MAPPING(5,	fimd1,	fimd1),
	SYSMMU_RESOURCE_MAPPING(5,	isp0,	isp0_a),
	SYSMMU_RESOURCE_MAPPING(5,	mfc_lr,	mfc_lr),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	jpeg,	jpega, "jpeg"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	mjpeg,	jpegm, "jpeg-hx"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler,	scaler, "sc-pclk"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	s3d,	s3d, "s3d"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	isp3,	isp3, "3aa"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif0,	flite0, "gscl_flite0"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif1,	flite1, "gscl_flite1"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif2,	flite2a, "gscl_flite2"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc1,	gsc1a, "gscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc2,	gsc2, "gscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc3,	gsc3, "gscl"),
};

static struct sysmmu_qos_map sysmmu_qos5410[] __initdata = {
	SYSMMU_QOS_MAPPING(fimd1,	15),
	SYSMMU_QOS_MAPPING(tv,		15),
	SYSMMU_QOS_MAPPING(2d,		8),
	SYSMMU_QOS_MAPPING(rot,		8),
	SYSMMU_QOS_MAPPING(gsc0,	8),
	SYSMMU_QOS_MAPPING(gsc1,	8),
	SYSMMU_QOS_MAPPING(gsc2,	8),
	SYSMMU_QOS_MAPPING(gsc3,	8),
	SYSMMU_QOS_MAPPING(mfc_lr,	8),
	SYSMMU_QOS_MAPPING(s3d,		8),
	SYSMMU_QOS_MAPPING(jpeg,	8),
	SYSMMU_QOS_MAPPING(mjpeg,	8),
	SYSMMU_QOS_MAPPING(scaler,	8),
	SYSMMU_QOS_MAPPING(fimd0,	15),
	SYSMMU_QOS_MAPPING(isp0,	8),
	SYSMMU_QOS_MAPPING(isp1,	8),
	SYSMMU_QOS_MAPPING(isp2,	8),
	SYSMMU_QOS_MAPPING(isp3,	8),
	SYSMMU_QOS_MAPPING(camif0,	15),
	SYSMMU_QOS_MAPPING(camif1,	15),
	SYSMMU_QOS_MAPPING(camif2,	15),
};

static struct sysmmu_version_map sysmmu_version5410[] __initdata = {
	SYSMMU_VERSION_MAPPING(fimd1,	1,	2),
	SYSMMU_VERSION_MAPPING(2d,	3,	1),
	SYSMMU_VERSION_MAPPING(rot,	3,	1),
	SYSMMU_VERSION_MAPPING(tv,	1,	2),
	SYSMMU_VERSION_MAPPING(gsc0,	3,	1),
	SYSMMU_VERSION_MAPPING(gsc1,	3,	2),
	SYSMMU_VERSION_MAPPING(gsc2,	3,	1),
	SYSMMU_VERSION_MAPPING(gsc3,	3,	1),
	SYSMMU_VERSION_MAPPING(mfc_lr,	2,	1),
	SYSMMU_VERSION_MAPPING(s3d,	1,	2),
	SYSMMU_VERSION_MAPPING(jpeg,	1,	2),
	SYSMMU_VERSION_MAPPING(mjpeg,	1,	2),
	SYSMMU_VERSION_MAPPING(scaler,	3,	1),
	SYSMMU_VERSION_MAPPING(fimd0,	1,	2),
	SYSMMU_VERSION_MAPPING(isp0,	1,	2),
	SYSMMU_VERSION_MAPPING(isp1,	1,	2),
	SYSMMU_VERSION_MAPPING(isp2,	3,	1),
	SYSMMU_VERSION_MAPPING(isp3,	1,	2),
	SYSMMU_VERSION_MAPPING(camif0,	1,	2),
	SYSMMU_VERSION_MAPPING(camif1,	1,	2),
	SYSMMU_VERSION_MAPPING(camif2,	1,	2),
};

static struct sysmmu_tlbinv_map sysmmu_tlbinv5410[] __initdata = {
	SYSMMU_TLBINVENTRY_MAPPING(fimd1, true),
	SYSMMU_TLBINVENTRY_MAPPING(tv, true),
	SYSMMU_TLBINVENTRY_MAPPING(camif0, true),
	SYSMMU_TLBINVENTRY_MAPPING(camif1, true),
	SYSMMU_TLBINVENTRY_MAPPING(camif2, true),
	SYSMMU_TLBINVENTRY_MAPPING(isp3, true),
};

#endif

#ifdef CONFIG_SOC_EXYNOS5420
SYSMMU_RESOURCE_DEFINE(EXYNOS5, s3d, S3D, S3D);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, jpegm, MJPEG, JPEG);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, jpegm2, MJPEG2, JPEGA);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, gsc1g,	GSC1,	GSC1);
SYSMMU_RESOURCE(EXYNOS5, isp0_a) {
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, ISP, ISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, DRC, DRCA),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, FD, FD),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, ISPCPU, MCUISP),
	DEFINE_SYSMMU_RESOURCE(EXYNOS5, SCALERC, SCALERCISP),
};
SYSMMU_RESOURCE_DEFINE(EXYNOS5, isp3,	3AA,	3AA);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, 2d_wr,	2D_WR,	2DV);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler0r, R_MSCL0, R_MSCL0);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler0w, W_MSCL0, W_MSCL0);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler1r, R_MSCL1, R_MSCL1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler1w, W_MSCL1, W_MSCL1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler2r, R_MSCL2, R_MSCL2);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, scaler2w, W_MSCL2, W_MSCL2);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, fimd1,	FIMD1,	FIMD1);
SYSMMU_RESOURCE_DEFINE(EXYNOS5, fimd1a,	FIMD1A,	FIMD1A);

static struct sysmmu_resource_map sysmmu_resmap5420[] __initdata = {
	SYSMMU_RESOURCE_MAPPING(5,	isp0,	isp0_a),
	SYSMMU_RESOURCE_MAPPING(5,	mfc_lr,	mfc_lr),
	SYSMMU_RESOURCE_MAPPING(5,	2d_wr,	2d_wr),
	SYSMMU_RESOURCE_MAPPING(5,	s3d,	s3d),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	fimd1,	fimd1, "lcd"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	fimd1a,	fimd1a, "lcd"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	mjpeg,	jpegm, "jpeg-hx"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	mjpeg2,	jpegm2, "jpeg2-hx"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	isp3,	isp3, "gscl_3aa"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif0,	flite0, "gscl_flite0"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	camif1,	flite1, "gscl_flite1"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler0r, scaler0r, "mscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler0w, scaler0w, "mscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler1r, scaler1r, "mscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler1w, scaler1w, "mscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler2r, scaler2r, "mscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	scaler2w, scaler2w, "mscl"),
	SYSMMU_RESOURCE_MAPPING_CLKDEP(5,	gsc1,	gsc1g, "gscl"),
};

static struct sysmmu_qos_map sysmmu_qos5420[] __initdata = {
	SYSMMU_QOS_MAPPING(2d,		8),
	SYSMMU_QOS_MAPPING(2d_wr,	8),
	SYSMMU_QOS_MAPPING(fimd1,	15),
	SYSMMU_QOS_MAPPING(fimd1a,	15),
	SYSMMU_QOS_MAPPING(tv,		15),
	SYSMMU_QOS_MAPPING(gsc0,	8),
	SYSMMU_QOS_MAPPING(gsc1,	8),
	SYSMMU_QOS_MAPPING(mfc_lr,	8),
	SYSMMU_QOS_MAPPING(s3d,		8),
	SYSMMU_QOS_MAPPING(jpeg,	8),
	SYSMMU_QOS_MAPPING(mjpeg2,	8),
	SYSMMU_QOS_MAPPING(scaler0r,	8),
	SYSMMU_QOS_MAPPING(scaler0w,	8),
	SYSMMU_QOS_MAPPING(scaler1r,	8),
	SYSMMU_QOS_MAPPING(scaler1w,	8),
	SYSMMU_QOS_MAPPING(scaler2r,	8),
	SYSMMU_QOS_MAPPING(scaler2w,	8),
	SYSMMU_QOS_MAPPING(isp0,	8),
	SYSMMU_QOS_MAPPING(isp1,	8),
	SYSMMU_QOS_MAPPING(isp2,	8),
	SYSMMU_QOS_MAPPING(isp3,	8),
	SYSMMU_QOS_MAPPING(camif0,	15),
	SYSMMU_QOS_MAPPING(camif1,	15),
};

static struct sysmmu_prop_map sysmmu_prop5420[] __initdata = {
	SYSMMU_PROPERTY_MAPPING(2d, SYSMMU_PROP_READ),
	SYSMMU_PROPERTY_MAPPING(2d_wr, SYSMMU_PROP_WRITE),
	SYSMMU_PROPERTY_MAPPING(scaler0r, SYSMMU_PROP_READ),
	SYSMMU_PROPERTY_MAPPING(scaler0w, SYSMMU_PROP_WRITE),
	SYSMMU_PROPERTY_MAPPING(scaler1r, SYSMMU_PROP_READ),
	SYSMMU_PROPERTY_MAPPING(scaler1w, SYSMMU_PROP_WRITE),
	SYSMMU_PROPERTY_MAPPING(scaler2r, SYSMMU_PROP_READ),
	SYSMMU_PROPERTY_MAPPING(scaler2w, SYSMMU_PROP_WRITE),
	SYSMMU_PROPERTY_MAPPING(fimd1,
		(0x11 << SYSMMU_PROP_WINDOW_SHIFT) | SYSMMU_PROP_PREFETCH),
	SYSMMU_PROPERTY_MAPPING(fimd1a,
		(0xe << SYSMMU_PROP_WINDOW_SHIFT) | SYSMMU_PROP_PREFETCH),
	SYSMMU_PROPERTY_MAPPING(camif0,
				SYSMMU_PROP_WRITE | SYSMMU_PROP_PREFETCH),
	SYSMMU_PROPERTY_MAPPING(camif1,
				SYSMMU_PROP_WRITE | SYSMMU_PROP_PREFETCH),
};

static struct sysmmu_tlbinv_map sysmmu_tlbinv5420[] __initdata = {
	SYSMMU_TLBINVENTRY_MAPPING(tv, true),
	SYSMMU_TLBINVENTRY_MAPPING(camif0, true),
	SYSMMU_TLBINVENTRY_MAPPING(camif1, true),
	SYSMMU_TLBINVENTRY_MAPPING(camif2, true),
	SYSMMU_TLBINVENTRY_MAPPING(isp3, true),
};

#endif /* CONFIG_SOC_EXYNOS5420 */
#endif /* CONFIG_ARCH_EXYNOS5 */

#ifdef CONFIG_IOMMU_API
/* platform_set_sysmmu - link a System MMU with its master device
 *
 * @sysmmu: pointer to device descriptor of System MMU
 * @dev: pointer to device descriptor of master device of System MMU
 *
 * This function links System MMU with its master device. Since a System MMU
 * is dedicated to its master device by H/W design, it is important to inform
 * their relationship to System MMU device driver. This function informs System
 * MMU driver what is the master device of the probing System MMU.
 * This information is used by the System MMU (exynos-iommu) to make their
 * relationship in the heirarch of kobjs of registered devices.
 * The link created here:
 * - Before call: NULL <- @sysmmu
 * - After call : @dev <- @sysmmu
 *
 * If a master is already assigned to @sysmmu and @sysmmu->archdata.iommu & 1
 * is 1, the link is created as follows:
 *  - Before call: existing_master <- @sysmmu <- existing_master
 *  - After call : existing_master <- @dev <- @sysmmu
 */
static void __init platform_set_sysmmu(
				struct device *sysmmu, struct device *dev)
{
	if ((unsigned long)sysmmu->archdata.iommu & 1)
		dev->archdata.iommu = sysmmu->archdata.iommu;

	sysmmu->archdata.iommu = (void *)((unsigned long)dev | 1);
}
#else
#define platform_set_sysmmu(sysmmu, dev) do { } while (0)
#endif /* CONFIG_IOMMU_API */

static void __init exynos4_sysmmu_init(void)
{
#ifdef CONFIG_S5P_DEV_FIMG2D
	platform_set_sysmmu(&SYSMMU_PLATDEV(2d).dev, &s5p_device_fimg2d.dev);
#endif
#ifdef CONFIG_S5P_DEV_MFC
	platform_set_sysmmu(&SYSMMU_PLATDEV(mfc_lr).dev, &s5p_device_mfc.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMC0
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimc0).dev, &s5p_device_fimc0.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMC1
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimc1).dev, &s5p_device_fimc1.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMC2
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimc2).dev, &s5p_device_fimc2.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMC3
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimc3).dev, &s5p_device_fimc3.dev);
#endif
#ifdef CONFIG_S5P_DEV_TV
	platform_set_sysmmu(&SYSMMU_PLATDEV(tv).dev, &s5p_device_mixer.dev);
#endif
#if defined(CONFIG_EXYNOS5_DEV_JPEG) || defined(CONFIG_S5P_DEV_JPEG)
	platform_set_sysmmu(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMD0
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimd0).dev, &s5p_device_fimd0.dev);
#endif
#ifdef CONFIG_EXYNOS4_DEV_FIMC_IS
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp0).dev,
						&exynos4_device_fimc_is.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp1).dev,
						&exynos4_device_fimc_is.dev);
#endif
}

static void __init exynos_sysmmu_clk_add_alias(const char *alias_devname,
					       const char *devname)
{
	struct clk *r = clk_get_sys(devname, SYSMMU_CLOCK_NAME);
	struct clk_lookup *l;

	if (IS_ERR(r)) {
		pr_err("%s: Failed to clk_get_sys(%s, %s) = %ld\n", __func__,
				devname, SYSMMU_CLOCK_NAME, PTR_ERR(r));
		return;
	}

	l = clkdev_alloc(r, SYSMMU_CLOCK_NAME, alias_devname);
	clk_put(r);
	if (!l) {
		pr_err("%s: Failed to clkdev_alloc(%s, %s)\n", __func__,
				SYSMMU_CLOCK_NAME, alias_devname);
		return;
	}
	clkdev_add(l);
}

static void __init exynos5_sysmmu_init(void)
{
#ifdef CONFIG_EXYNOS5_DEV_JPEG_HX
	platform_set_sysmmu(&SYSMMU_PLATDEV(mjpeg).dev,
						&exynos5_device_jpeg_hx.dev);
	if (soc_is_exynos5420()) {
		struct clk *pclk, *pclk_mmu;

		pclk_mmu = clk_get_sys(SYSMMU_CLOCK_DEVNAME(jpeg, 3),
					SYSMMU_CLOCK_NAME);
		if (IS_ERR(pclk_mmu)) {
			pr_err("%s: Failed to get clk of MMU clk of jpeg\n",
				__func__);
		} else {
			pclk = clk_get_sys(NULL, "jpeg-hx");
			if (IS_ERR(pclk)) {
				pr_err("%s: Failed to get clk of jpeg-hx\n",
					__func__);
			} else {
				clk_set_parent(pclk, pclk_mmu);
				clk_put(pclk);
			}

			pclk = clk_get_sys(NULL, "jpeg2-hx");
			if (IS_ERR(pclk)) {
				pr_err("%s: Failed to get clk of jpeg2-hx\n",
					__func__);
			} else {
				clk_set_parent(pclk, pclk_mmu);
				clk_put(pclk);
			}

			clk_put(pclk_mmu);
		}

		platform_set_sysmmu(&SYSMMU_PLATDEV(mjpeg2).dev,
						&exynos5_device_jpeg2_hx.dev);
	}
#endif
#if defined(CONFIG_EXYNOS5_DEV_JPEG) || defined(CONFIG_S5P_DEV_JPEG)
	platform_set_sysmmu(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#ifdef CONFIG_S5P_DEV_MFC
	platform_set_sysmmu(&SYSMMU_PLATDEV(mfc_lr).dev, &s5p_device_mfc.dev);
#endif
#ifdef CONFIG_S5P_DEV_TV
	platform_set_sysmmu(&SYSMMU_PLATDEV(tv).dev, &s5p_device_mixer.dev);
#endif
#ifdef CONFIG_EXYNOS5_DEV_GSC
	platform_set_sysmmu(&SYSMMU_PLATDEV(gsc0).dev,
						&exynos5_device_gsc0.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(gsc1).dev,
						&exynos5_device_gsc1.dev);

	if (soc_is_exynos5250() || soc_is_exynos5410()) {
		platform_set_sysmmu(&SYSMMU_PLATDEV(gsc2).dev,
				&exynos5_device_gsc2.dev);
		platform_set_sysmmu(&SYSMMU_PLATDEV(gsc3).dev,
				&exynos5_device_gsc3.dev);
	}
#endif
#ifdef CONFIG_EXYNOS5_DEV_SCALER
	if (soc_is_exynos5410()) {
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler).dev,
				&exynos5_device_scaler0.dev);
	} else if (soc_is_exynos5420()) {
		exynos_sysmmu_clk_add_alias(SYSMMU_CLOCK_DEVNAME(scaler0w, 24),
					    SYSMMU_CLOCK_DEVNAME(scaler0r, 23));
		exynos_sysmmu_clk_add_alias(SYSMMU_CLOCK_DEVNAME(scaler1w, 26),
					    SYSMMU_CLOCK_DEVNAME(scaler1r, 25));
		exynos_sysmmu_clk_add_alias(SYSMMU_CLOCK_DEVNAME(scaler2w, 28),
					    SYSMMU_CLOCK_DEVNAME(scaler2r, 27));
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler0r).dev,
				&exynos5_device_scaler0.dev);
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler0w).dev,
				&exynos5_device_scaler0.dev);
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler1r).dev,
				&exynos5_device_scaler1.dev);
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler1w).dev,
				&exynos5_device_scaler1.dev);
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler2r).dev,
				&exynos5_device_scaler2.dev);
		platform_set_sysmmu(&SYSMMU_PLATDEV(scaler2w).dev,
				&exynos5_device_scaler2.dev);
	}
#endif
#ifdef CONFIG_EXYNOS_DEV_ROTATOR
	platform_set_sysmmu(&SYSMMU_PLATDEV(rot).dev,
						&exynos5_device_rotator.dev);
#endif
#if defined(CONFIG_S5P_DEV_FIMG2D) && defined(CONFIG_VIDEO_EXYNOS_FIMG2D)
	platform_set_sysmmu(&SYSMMU_PLATDEV(2d).dev,
						&s5p_device_fimg2d.dev);
	exynos_sysmmu_clk_add_alias(SYSMMU_CLOCK_DEVNAME(2d_wr, 22),
				    SYSMMU_CLOCK_DEVNAME(2d, 15));
	platform_set_sysmmu(&SYSMMU_PLATDEV(2d_wr).dev,
						&s5p_device_fimg2d.dev);
#endif
#ifdef CONFIG_EXYNOS5_DEV_FIMC_IS
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp0).dev,
						&exynos5_device_fimc_is.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp1).dev,
						&exynos5_device_fimc_is.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp2).dev,
						&exynos5_device_fimc_is.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(isp3).dev,
						&exynos5_device_fimc_is.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS5_FIMC_IS
	platform_set_sysmmu(&SYSMMU_PLATDEV(camif0).dev,
						&exynos5_device_fimc_is.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(camif1).dev,
						&exynos5_device_fimc_is.dev);
	if (soc_is_exynos5410())
		platform_set_sysmmu(&SYSMMU_PLATDEV(camif2).dev,
						&exynos5_device_fimc_is.dev);
#endif
	/*
	 * If FIMC-LITEs are needed to be controlled by exynos5_device_fimc_is
	 * in FIMC-IS driver and by exynos_device_flite0/1/2 in FIMC-LITE
	 * driver, the following restrictions must be kept:
	 * - iommu_attach_device() call against exynos5_device_fimc_is,
	 *   exynos_device_flite0, exynos_device_flite1 and exynos_device_flite2
	 *   is mutually exclusive.
	 * - platform_set_sysmmu() call against exynos5_device_fimc_is must be
	 *   prior to the call against exynos_device_flite0/1/2.
	 */
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	platform_set_sysmmu(&SYSMMU_PLATDEV(camif0).dev,
						&exynos_device_flite0.dev);
	platform_set_sysmmu(&SYSMMU_PLATDEV(camif1).dev,
						&exynos_device_flite1.dev);
	if (soc_is_exynos5410())
		platform_set_sysmmu(&SYSMMU_PLATDEV(camif2).dev,
						&exynos_device_flite2.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMD0
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimd0).dev, &s5p_device_fimd0.dev);
#endif
#ifdef CONFIG_S5P_DEV_FIMD1
	platform_set_sysmmu(&SYSMMU_PLATDEV(fimd1).dev, &s5p_device_fimd1.dev);
	if (soc_is_exynos5420() && (samsung_rev() >= EXYNOS5420_REV_1_0)) {
		platform_set_sysmmu(&SYSMMU_PLATDEV(fimd1a).dev, &s5p_device_fimd1.dev);
		swap(SYSMMU_PLATDEV(fimd1).dev.platform_data,
				SYSMMU_PLATDEV(fimd1a).dev.platform_data);
		swap(SYSMMU_PLATDEV(fimd1).resource, SYSMMU_PLATDEV(fimd1a).resource);
	}
#endif
}

/**
 * init_symmu_platform_device - register System MMU devices
 *
 * This function registers System MMU devices. This function must be called
 * before the System MMU devices is added to a power domain since adding
 * to a power domain needs their added devices to be registered.
 * Please refer to arch/arm/mach-exynos/pm_domains.c.
 */
static int __init init_sysmmu_platform_device(void)
{
	int i, j;
	struct sysmmu_resource_map *resmap[2] = {NULL, NULL};
	int nmap[2] = {0, 0};
	struct sysmmu_version_map *versions = NULL;
	struct sysmmu_tlbinv_map *tlbinv = NULL;
	struct sysmmu_qos_map *qoss = NULL;
	int nvmap = 0;
	int nqmap = 0;
	struct sysmmu_prop_map *propmap = NULL;
	int npropmap = 0;
	int ntmap = 0;

#ifdef CONFIG_ARCH_EXYNOS5
#ifdef CONFIG_SOC_EXYNOS5250
	if (soc_is_exynos5250()) {
		resmap[0] = sysmmu_resmap5;
		nmap[0] = ARRAY_SIZE(sysmmu_resmap5);
		resmap[1] = sysmmu_resmap5250;
		nmap[1] = ARRAY_SIZE(sysmmu_resmap5250);
	}
#endif
#ifdef CONFIG_SOC_EXYNOS5410
	if (soc_is_exynos5410()) {
		resmap[0] = sysmmu_resmap5;
		nmap[0] = ARRAY_SIZE(sysmmu_resmap5);
		resmap[1] = sysmmu_resmap5410;
		nmap[1] = ARRAY_SIZE(sysmmu_resmap5410);
		versions = sysmmu_version5410;
		nvmap = ARRAY_SIZE(sysmmu_version5410);
		qoss = sysmmu_qos5410;
		nqmap = ARRAY_SIZE(sysmmu_qos5410);
		tlbinv = sysmmu_tlbinv5410;
		ntmap = ARRAY_SIZE(sysmmu_tlbinv5410);
	}
#endif
#ifdef CONFIG_SOC_EXYNOS5420
	if (soc_is_exynos5420()) {
		resmap[0] = sysmmu_resmap5;
		nmap[0] = ARRAY_SIZE(sysmmu_resmap5);
		resmap[1] = sysmmu_resmap5420;
		nmap[1] = ARRAY_SIZE(sysmmu_resmap5420);
		qoss = sysmmu_qos5420;
		nqmap = ARRAY_SIZE(sysmmu_qos5420);
		propmap = sysmmu_prop5420;
		npropmap = ARRAY_SIZE(sysmmu_prop5420);
		tlbinv = sysmmu_tlbinv5420;
		ntmap = ARRAY_SIZE(sysmmu_tlbinv5420);
	}
#endif
#endif /* CONFIG_ARCH_EXYNOS5 */

#ifdef CONFIG_ARCH_EXYNOS4
	if (resmap[0] == NULL) {
		resmap[0] = sysmmu_resmap4;
		nmap[0] = ARRAY_SIZE(sysmmu_resmap4);
	}

	if (soc_is_exynos4210()) {
		resmap[1] = sysmmu_resmap4210;
		nmap[1] = ARRAY_SIZE(sysmmu_resmap4210);
	}

	if (soc_is_exynos4412() || soc_is_exynos4212()) {
		resmap[1] = sysmmu_resmap4212;
		nmap[1] = ARRAY_SIZE(sysmmu_resmap4212);
	}
#endif

	if (versions) {
		for (i = 0; i < nvmap; i++) {
			struct sysmmu_platform_data *platdata;
			platdata = versions[i].pdev->dev.platform_data;
			platdata->ver = versions[i].ver;
		}
	}

	if (qoss) {
		for (i = 0; i < nqmap; i++) {
			struct sysmmu_platform_data *platdata;
			platdata = qoss[i].pdev->dev.platform_data;
			platdata->qos = qoss[i].qos;
		}
	}

	if (propmap) {
		for (i = 0; i < npropmap; i++) {
			struct sysmmu_platform_data *platdata;
			platdata = propmap[i].pdev->dev.platform_data;
			platdata->prop = propmap[i].prop;
		}
	}

	if (tlbinv) {
		for (i = 0; i < ntmap; i++) {
			struct sysmmu_platform_data *platdata;
			platdata = tlbinv[i].pdev->dev.platform_data;
			platdata->tlbinv_entry = tlbinv[i].tlbinv_entry;
		}
	}

	for (j = 0; j < 2; j++) {
		for (i = 0; i < nmap[j]; i++) {
			struct sysmmu_resource_map *map;
			struct sysmmu_platform_data *platdata;

			map = &resmap[j][i];

			platdata = map->pdev->dev.platform_data;
			platdata->clockname = map->clockname;

			if (platform_device_add_resources(map->pdev, map->res,
								map->rnum)) {
				pr_err("%s: Failed to add resource for %s.%d\n",
						__func__,
						map->pdev->name, map->pdev->id);
				continue;
			}

			if (platform_device_register(map->pdev)) {
				pr_err("%s: Failed to register %s.%d\n",
					__func__, map->pdev->name,
						map->pdev->id);
			}
		}
	}

	return 0;
}
postcore_initcall(init_sysmmu_platform_device);

/**
 * setup_sysmmu_owner
 *           - make a relationship between System MMU and its master device
 *
 * This function changes the device hierarchy of the both of System MMU and
 * its master device that is specified by platfor_set_sysmmu().
 * It must be ensured that this function is called after the both of System MMU
 * and its master device is registered.
 * It must be also ensured that this function is called before the both devices
 * are probe()ed since it is not correct to change the hierarchy of a probe()ed
 * device.
 */
static int __init setup_sysmmu_owner(void)
{
	if (soc_is_exynos5250() || soc_is_exynos5410() ||
	    soc_is_exynos5420())
		exynos5_sysmmu_init();
	else if (soc_is_exynos4412() || soc_is_exynos4212())
		exynos4_sysmmu_init();

	return 0;
}
arch_initcall_sync(setup_sysmmu_owner);
