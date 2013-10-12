/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>

#include <plat/devs.h>

#include <mach/map.h>
#include <mach/bts.h>
#include <mach/regs-bts.h>
#include <mach/regs-pmu.h>
#include <mach/regs-mem.h>

enum bts_index {
	BTS_IDX_FIMD1M0 = 0,
	BTS_IDX_FIMD1M1,
	BTS_IDX_TVM0,
	BTS_IDX_TVM1,
	BTS_IDX_FIMC_LITE0,
	BTS_IDX_FIMC_LITE1,
	BTS_IDX_3AA,
	BTS_IDX_ROTATOR,
	BTS_IDX_SSS,
	BTS_IDX_SSSSLIM,
	BTS_IDX_G2D,
	BTS_IDX_EAGLE,
	BTS_IDX_KFC,
	BTS_IDX_MFC0,
	BTS_IDX_MFC1,
	BTS_IDX_G3D0,
	BTS_IDX_G3D1,
	BTS_IDX_MDMA0,
	BTS_IDX_MDMA1,
	BTS_IDX_JPEG0,
	BTS_IDX_JPEG2,
	BTS_IDX_USBDRD300,
	BTS_IDX_USBDRD301,
	BTS_IDX_UFS,
	BTS_IDX_MMC0,
	BTS_IDX_MMC1,
	BTS_IDX_MMC2,
	BTS_IDX_MSCL0,
	BTS_IDX_MSCL1,
	BTS_IDX_MSCL2,
	BTS_IDX_GSCL0,
	BTS_IDX_GSCL1,
};

enum bts_id {
	BTS_FIMD1M0 = (1 << BTS_IDX_FIMD1M0),
	BTS_FIMD1M1 = (1 << BTS_IDX_FIMD1M1),
	BTS_TVM0 = (1 << BTS_IDX_TVM0),
	BTS_TVM1 = (1 << BTS_IDX_TVM1),
	BTS_FIMC_LITE0 = (1 << BTS_IDX_FIMC_LITE0),
	BTS_FIMC_LITE1 = (1 << BTS_IDX_FIMC_LITE1),
	BTS_3AA= (1 << BTS_IDX_3AA),
	BTS_ROTATOR = (1 << BTS_IDX_ROTATOR),
	BTS_SSS = (1 << BTS_IDX_SSS),
	BTS_SSSSLIM = (1 << BTS_IDX_SSSSLIM),
	BTS_G2D = (1 << BTS_IDX_G2D),
	BTS_EAGLE = (1 << BTS_IDX_EAGLE),
	BTS_KFC = (1 << BTS_IDX_KFC),
	BTS_MFC0 = (1 << BTS_IDX_MFC0),
	BTS_MFC1 = (1 << BTS_IDX_MFC1),
	BTS_G3D0 = (1 << BTS_IDX_G3D0),
	BTS_G3D1 = (1 << BTS_IDX_G3D1),
	BTS_MDMA0 = (1 << BTS_IDX_MDMA0),
	BTS_MDMA1 = (1 << BTS_IDX_MDMA1),
	BTS_JPEG0 = (1 << BTS_IDX_JPEG0),
	BTS_JPEG2 = (1 << BTS_IDX_JPEG2),
	BTS_USBDRD300 = (1 << BTS_IDX_USBDRD300),
	BTS_USBDRD301 = (1 << BTS_IDX_USBDRD301),
	BTS_UFS = (1 << BTS_IDX_UFS),
	BTS_MMC0 = (1 << BTS_IDX_MMC0),
	BTS_MMC1 = (1 << BTS_IDX_MMC1),
	BTS_MMC2 = (1 << BTS_IDX_MMC2),
	BTS_MSCL0 = (1 << BTS_IDX_MSCL0),
	BTS_MSCL1 = (1 << BTS_IDX_MSCL1),
	BTS_MSCL2 = (1 << BTS_IDX_MSCL2),
	BTS_GSCL0 = (1 << BTS_IDX_GSCL0),
	BTS_GSCL1 = (1 << BTS_IDX_GSCL1),
};

enum bts_clock_index {
	BTS_CLOCK_G3D = 0,
	BTS_CLOCK_MMC,
	BTS_CLOCK_USB,
	BTS_CLOCK_DIS1,
	BTS_CLOCK_MAX,
};

struct bts_table {
	struct bts_set_table *table_list;
	unsigned int table_num;
};

struct bts_info {
	enum bts_id id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table;
	const char *devname;
	const char *pd_name;
	const char *clk_name;
	struct clk *clk;
	bool on;
	struct list_head list;
	struct list_head scen_list;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

struct bts_ip_clk {
	const char *clkname;
	const char *devname;
	struct clk *clk;
};

struct bts_scen_status {
	bool fimc_on;
	bool g3d_on;
	bool g3d_mode;
	unsigned int g3d_freq;
};

struct bts_scen_status pr_state = {
	.fimc_on = false,
	.g3d_on = false,
	.g3d_mode = false,
	.g3d_freq = 177,
};

#define update_fimc_on(a) (pr_state.fimc_on = a)
#define update_g3d_on(a) (pr_state.g3d_on = a)
#define update_g3d_freq(a) (pr_state.g3d_freq = a)
#define update_g3d_mode(a) (pr_state.g3d_mode = a)

#define G3D_177 (177)

#define BTS_TABLE(num)	\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},		\
	{READ_TOKEN_MAX_VALUE, 0xffdf},			\
	{READ_BW_UPPER_BOUNDARY, 0x18},			\
	{READ_BW_LOWER_BOUNDARY, 0x1},			\
	{READ_INITIAL_TOKEN_VALUE, 0x8},		\
	{WRITE_CHANNEL_PRIORITY, num},		\
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},		\
	{WRITE_BW_UPPER_BOUNDARY, 0x18},		\
	{WRITE_BW_LOWER_BOUNDARY, 0x1},			\
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},		\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}

static struct bts_set_table fbm_l_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0x4444},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_DEMOTION_WINDOW, 0x7fff},
	{WRITE_DEMOTION_TOKEN, 0x1},
	{WRITE_DEFAULT_WINDOW, 0x7fff},
	{WRITE_DEFAULT_TOKEN, 0x1},
	{WRITE_PROMOTION_WINDOW, 0x7fff},
	{WRITE_PROMOTION_TOKEN, 0x1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

static struct bts_set_table g3d_fbm_l_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0x4444},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_DEMOTION_WINDOW, 0x7fff},
	{WRITE_DEMOTION_TOKEN, 0x1},
	{WRITE_DEFAULT_WINDOW, 0x7fff},
	{WRITE_DEFAULT_TOKEN, 0x1},
	{WRITE_PROMOTION_WINDOW, 0x7fff},
	{WRITE_PROMOTION_TOKEN, 0x1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{WRITE_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

static struct bts_set_table g3d_read_ch_static_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x77},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, 0x8},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x2},
	{READ_QOS_CONTROL, 0x3},
};

static struct bts_set_table g3d_read_ch_fbm_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x1f},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, 0x1f},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
};

BTS_TABLE(0x8888);
BTS_TABLE(0xcccc);

static struct bts_ip_clk exynos5_bts_clk[] = {
	[BTS_CLOCK_G3D] = {
		.clkname = "clk_ahb2apb_g3dp",
	},
	[BTS_CLOCK_MMC] = {
		.clkname = "clk_ahb2apb_fsys1p",
	},
	[BTS_CLOCK_USB] = {
		.clkname = "clk_ahb2apb_fsyssp",
	},
	[BTS_CLOCK_DIS1] = {
		.clkname = "axi_disp1",
	},
};

#define BTS_CPU (BTS_KFC | BTS_EAGLE)
#define BTS_FIMD (BTS_FIMD1M0 | BTS_FIMD1M1)
#define BTS_TV (BTS_TVM0 | BTS_TVM1)
#define BTS_DIS1 (BTS_TVM0 | BTS_TVM1 | BTS_FIMD1M0 | BTS_FIMD1M1)
#define BTS_FIMC (BTS_FIMC_LITE0 | BTS_FIMC_LITE1 | BTS_3AA)
#define BTS_MDMA (BTS_MDMA0 | BTS_MDMA1)
#define BTS_MFC (BTS_MFC0 | BTS_MFC1)
#define BTS_G3D (BTS_G3D0 | BTS_G3D1)
#define BTS_JPEG (BTS_JPEG0 | BTS_JPEG2)
#define BTS_USB (BTS_USBDRD300 | BTS_USBDRD301)
#define BTS_MMC (BTS_MMC0 | BTS_MMC1 | BTS_MMC2)
#define BTS_MSCL (BTS_MSCL0 | BTS_MSCL1 | BTS_MSCL2)
#define BTS_GSCL (BTS_GSCL0 | BTS_GSCL1)

#ifdef BTS_DBGGEN
#define BTS_DBG(x...) pr_err(x)
#else
#define BTS_DBG(x...) do {} while (0)
#endif

#ifdef BTS_DBGGEN1
#define BTS_DBG1(x...) pr_err(x)
#else
#define BTS_DBG1(x...) do {} while (0)
#endif

static struct bts_info exynos5_bts[] = {
	[BTS_IDX_FIMD1M0] = {
		.id = BTS_FIMD1M0,
		.name = "fimd1m0",
		.pa_base = EXYNOS5_PA_BTS_DISP10,
		.pd_name = "pd-fimd1",
		.devname = "exynos5-fb.1",
		.clk_name = "lcd",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_FIMD1M1] = {
		.id = BTS_FIMD1M1,
		.name = "fimd1m1",
		.pa_base = EXYNOS5_PA_BTS_DISP11,
		.pd_name = "pd-fimd1",
		.devname = "exynos5-fb.1",
		.clk_name = "lcd",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_TVM0] = {
		.id = BTS_TVM0,
		.name = "tvm0",
		.pa_base = EXYNOS5_PA_BTS_MIXER0,
		.pd_name = "pd-mixer",
		.devname = "s5p-mixer",
		.clk_name = "mixer",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_TVM1] = {
		.id = BTS_TVM1,
		.name = "tvm1",
		.pa_base = EXYNOS5_PA_BTS_MIXER1,
		.pd_name = "pd-mixer",
		.devname = "s5p-mixer",
		.clk_name = "mixer",
		.table.table_list = axiqos_0x8888_table,
		.table.table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = true,
	},
	[BTS_IDX_FIMC_LITE0] = {
		.id = BTS_FIMC_LITE0,
		.name = "fimc_lite0",
		.pa_base = EXYNOS5_PA_BTS_FIMCLITE0,
		.pd_name = "pd-fimclite",
		.clk_name = "gscl_flite0",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = true,
	},
	[BTS_IDX_FIMC_LITE1] = {
		.id = BTS_FIMC_LITE1,
		.name = "fimc_lite1",
		.pa_base = EXYNOS5_PA_BTS_FIMCLITE1,
		.pd_name = "pd-fimclite",
		.clk_name = "gscl_flite1",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = true,
	},
	[BTS_IDX_3AA] = {
		.id = BTS_3AA,
		.name = "3aa",
		.pa_base = EXYNOS5_PA_BTS_3AA,
		.pd_name = "pd-fimclite",
		.clk_name = "gscl_3aa",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = true,
	},
	[BTS_IDX_ROTATOR] = {
		.id = BTS_ROTATOR,
		.name = "rotator",
		.pa_base = EXYNOS5_PA_BTS_ROTATOR,
		.pd_name = "DEFAULT",
		.clk_name = "rotator",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_SSS] = {
		.id = BTS_SSS,
		.name = "sss",
		.pa_base = EXYNOS5_PA_BTS_SSS,
		.pd_name = "pd-g2d",
		.clk_name = "secss",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_SSSSLIM] = {
		.id = BTS_SSSSLIM,
		.name = "sssslim",
		.pa_base = EXYNOS5_PA_BTS_SSSSLIM,
		.pd_name = "pd-g2d",
		.clk_name = "slimsss",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_G2D] = {
		.id = BTS_G2D,
		.name = "g2d",
		.pa_base = EXYNOS5_PA_BTS_G2D,
		.pd_name = "pd-g2d",
		.devname = "s5p-fimg2d",
		.clk_name = "fimg2d",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_EAGLE] = {
		.id = BTS_EAGLE,
		.name = "eagle",
		.pa_base = EXYNOS5_PA_BTS_EAGLE,
		.pd_name = "pd-eagle",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_KFC] = {
		.id = BTS_KFC,
		.name = "kfc",
		.pa_base = EXYNOS5_PA_BTS_KFC,
		.pd_name = "pd-kfc",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MFC0] = {
		.id = BTS_MFC0,
		.name = "mfc0",
		.pa_base = EXYNOS5_PA_BTS_MFC0,
		.pd_name = "pd-mfc",
		.devname = "s3c-mfc",
		.clk_name = "mfc",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MFC1] = {
		.id = BTS_MFC1,
		.name = "mfc1",
		.pa_base = EXYNOS5_PA_BTS_MFC1,
		.pd_name = "pd-mfc",
		.devname = "s3c-mfc",
		.clk_name = "mfc",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_G3D0] = {
		.id = BTS_G3D0,
		.name = "g3d0",
		.pa_base = EXYNOS5_PA_BTS_G3D0,
		.pd_name = "pd-g3d",
		.devname = "mali.0",
		.clk_name = "g3d",
		.table.table_list = g3d_fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(g3d_fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_G3D1] = {
		.id = BTS_G3D1,
		.name = "g3d1",
		.pa_base = EXYNOS5_PA_BTS_G3D1,
		.pd_name = "pd-g3d",
		.devname = "mali.0",
		.clk_name = "g3d",
		.table.table_list = g3d_fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(g3d_fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MDMA0] = {
		.id = BTS_MDMA0,
		.name = "mdma0",
		.pa_base = EXYNOS5_PA_BTS_MDMA,
		.pd_name = "pd-g2d",
		.devname = "dma-pl330.2",
		.clk_name = "dma",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MDMA1] = {
		.id = BTS_MDMA1,
		.name = "mdma1",
		.pa_base = EXYNOS5_PA_BTS_MDMA1,
		.pd_name = "DEFAULT",
		.clk_name = "mdma1",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_JPEG0] = {
		.id = BTS_JPEG0,
		.name = "jpeg0",
		.pa_base = EXYNOS5_PA_BTS_JPEG,
		.pd_name = "DEFAULT",
		.clk_name = "jpeg-hx",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_JPEG2] = {
		.id = BTS_JPEG2,
		.name = "jpeg2",
		.pa_base = EXYNOS5_PA_BTS_JPEG2,
		.pd_name = "DEFAULT",
		.clk_name = "jpeg2-hx",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_USBDRD300] = {
		.id = BTS_USBDRD300,
		.name = "usbdrd300",
		.pa_base = EXYNOS5_PA_BTS_USBDRD300,
		.pd_name = "DEFAULT",
		.devname = "exynos-dwc3.0",
		.clk_name = "usbdrd30",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_USBDRD301] = {
		.id = BTS_USBDRD301,
		.name = "usbdrd301",
		.pa_base = EXYNOS5_PA_BTS_USBDRD301,
		.pd_name = "DEFAULT",
		.devname = "exynos-dwc3.1",
		.clk_name = "usbdrd30",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_UFS] = {
		.id = BTS_UFS,
		.name = "ufs",
		.pa_base = EXYNOS5_PA_BTS_UFS,
		.pd_name = "DEFAULT",
		.clk_name = "ufs",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MMC0] = {
		.id = BTS_MMC0,
		.name = "mmc0",
		.pa_base = EXYNOS5_PA_BTS_MMC0,
		.pd_name = "DEFAULT",
		.devname = "dw_mmc.0",
		.clk_name = "dwmci",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MMC1] = {
		.id = BTS_MMC1,
		.name = "mmc1",
		.pa_base = EXYNOS5_PA_BTS_MMC1,
		.pd_name = "DEFAULT",
		.devname = "dw_mmc.1",
		.clk_name = "dwmci",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MMC2] = {
		.id = BTS_MMC2,
		.name = "mmc2",
		.pa_base = EXYNOS5_PA_BTS_MMC2,
		.pd_name = "DEFAULT",
		.devname = "dw_mmc.2",
		.clk_name = "dwmci",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MSCL0] = {
		.id = BTS_MSCL0,
		.name = "mscl0",
		.pa_base = EXYNOS5_PA_BTS_MSCL0,
		.pd_name = "pd-mscl",
		.devname = "exynos5-scaler.0",
		.clk_name = "mscl",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MSCL1] = {
		.id = BTS_MSCL1,
		.name = "mscl1",
		.pa_base = EXYNOS5_PA_BTS_MSCL1,
		.devname = "exynos5-scaler.1",
		.pd_name = "pd-mscl",
		.clk_name = "mscl",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_MSCL2] = {
		.id = BTS_MSCL2,
		.name = "mscl2",
		.pa_base = EXYNOS5_PA_BTS_MSCL2,
		.devname = "exynos5-scaler.2",
		.pd_name = "pd-mscl",
		.clk_name = "mscl",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_GSCL0] = {
		.id = BTS_GSCL0,
		.name = "gscl0",
		.pa_base = EXYNOS5_PA_BTS_GSCL0,
		.pd_name = "pd-gscaler0",
		.devname = "exynos-gsc.0",
		.clk_name = "gscl",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
	[BTS_IDX_GSCL1] = {
		.id = BTS_GSCL1,
		.name = "gscl1",
		.pa_base = EXYNOS5_PA_BTS_GSCL1,
		.pd_name = "pd-gscaler1",
		.devname = "exynos-gsc.1",
		.clk_name = "gscl",
		.table.table_list = fbm_l_r_high_table,
		.table.table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = true,
	},
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static unsigned int read_clusterid(void)
{
	unsigned int mpidr;
	asm ("mrc\tp15, 0, %0, c0, c0, 5\n":"=r"(mpidr));
	return (mpidr >> 8) & 0xff;
}

void bts_drex_initialize(void)
{
#if defined(CONFIG_S5P_DP)
	__raw_writel(0x0, EXYNOS5_DREXI_0_QOSCONTROL15);
	__raw_writel(0x80, EXYNOS5_DREXI_0_QOSCONTROL12);
	__raw_writel(0x80, EXYNOS5_DREXI_0_QOSCONTROL8);
	__raw_writel(0x33, EXYNOS5_DREXI_0_BRBRSVCONTROL);
	__raw_writel(0x88588858, EXYNOS5_DREXI_0_BRBRSVCONFIG);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG0_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG1_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG2_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG3_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG0_W);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG1_W);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG2_W);
	__raw_writel(0x01000604, EXYNOS5_DREXI_0_BP_CONFIG3_W);
	__raw_writel(0x1, EXYNOS5_DREXI_0_BP_CONTROL0);
	__raw_writel(0x1, EXYNOS5_DREXI_0_BP_CONTROL1);
	__raw_writel(0x1, EXYNOS5_DREXI_0_BP_CONTROL2);
	__raw_writel(0x1, EXYNOS5_DREXI_0_BP_CONTROL3);

	__raw_writel(0x0, EXYNOS5_DREXI_1_QOSCONTROL15);
	__raw_writel(0x80, EXYNOS5_DREXI_1_QOSCONTROL12);
	__raw_writel(0x80, EXYNOS5_DREXI_1_QOSCONTROL8);
	__raw_writel(0x33, EXYNOS5_DREXI_1_BRBRSVCONTROL);
	__raw_writel(0x88588858, EXYNOS5_DREXI_1_BRBRSVCONFIG);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG0_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG1_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG2_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG3_R);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG0_W);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG1_W);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG2_W);
	__raw_writel(0x01000604, EXYNOS5_DREXI_1_BP_CONFIG3_W);
	__raw_writel(0x1, EXYNOS5_DREXI_1_BP_CONTROL0);
	__raw_writel(0x1, EXYNOS5_DREXI_1_BP_CONTROL1);
	__raw_writel(0x1, EXYNOS5_DREXI_1_BP_CONTROL2);
	__raw_writel(0x1, EXYNOS5_DREXI_1_BP_CONTROL3);
#endif
}

static int exynos_bts_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	unsigned int reg;
	switch (event) {
	case PM_POST_SUSPEND:
		bts_drex_initialize();
		if (!read_clusterid()) {
			bts_initialize("pd-eagle", true);

			reg = __raw_readl(S5P_VA_PMU);
			__raw_writel(reg | EXYNOS5420_ACE_KFC, EXYNOS5420_SFR_AXI_CGDIS1_REG);

			bts_initialize("pd-kfc", true);

			__raw_writel(reg, EXYNOS5420_SFR_AXI_CGDIS1_REG);

		} else {
			bts_initialize("pd-kfc", true);

			reg = __raw_readl(EXYNOS5420_SFR_AXI_CGDIS1_REG);
			__raw_writel(reg | EXYNOS5420_ACE_EAGLE, EXYNOS5420_SFR_AXI_CGDIS1_REG);

			bts_initialize("pd-eagle", true);

			__raw_writel(reg, EXYNOS5420_SFR_AXI_CGDIS1_REG);
		}
			bts_initialize("DEFAULT", true);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

static void set_bts_ip_table(struct bts_info *bts)
{
	int i;
	struct bts_set_table *table = bts->table.table_list;

	BTS_DBG("[BTS] bts set: %s\n", bts->name);

	if (bts->id & BTS_G3D)
		clk_enable(exynos5_bts_clk[BTS_CLOCK_G3D].clk);
	else if (bts->id & BTS_MMC)
		clk_enable(exynos5_bts_clk[BTS_CLOCK_MMC].clk);
	else if (bts->id & BTS_USB)
		clk_enable(exynos5_bts_clk[BTS_CLOCK_USB].clk);
	else if (bts->id & BTS_DIS1)
		clk_enable(exynos5_bts_clk[BTS_CLOCK_DIS1].clk);

	if (bts->clk)
		clk_enable(bts->clk);

	for (i = 0; i < bts->table.table_num; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	if (bts->id & BTS_G3D)
		clk_disable(exynos5_bts_clk[BTS_CLOCK_G3D].clk);
	else if (bts->id & BTS_MMC)
		clk_disable(exynos5_bts_clk[BTS_CLOCK_MMC].clk);
	else if (bts->id & BTS_USB)
		clk_disable(exynos5_bts_clk[BTS_CLOCK_USB].clk);
	else if (bts->id & BTS_DIS1)
		clk_disable(exynos5_bts_clk[BTS_CLOCK_DIS1].clk);

	if (bts->clk)
		clk_disable(bts->clk);
}

static void set_bts_g3d_table(bool mode)
{
	int i;
	struct bts_set_table *table;
	unsigned int table_num;

	BTS_DBG("[BTS] set bts g3d MO %d\n", mode);

	if (mode) {
		table = g3d_read_ch_static_table;
		table_num = ARRAY_SIZE(g3d_read_ch_static_table);
	} else {
		table = g3d_read_ch_fbm_table;
		table_num = ARRAY_SIZE(g3d_read_ch_fbm_table);
	}

	clk_enable(exynos5_bts_clk[BTS_CLOCK_G3D].clk);
	clk_enable(exynos5_bts[BTS_IDX_G3D0].clk);

	for (i = 0; i < table_num; i++) {
		__raw_writel(table->val, exynos5_bts[BTS_IDX_G3D0].va_base + table->reg);
		__raw_writel(table->val, exynos5_bts[BTS_IDX_G3D1].va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	clk_disable(exynos5_bts_clk[BTS_CLOCK_G3D].clk);
	clk_disable(exynos5_bts[BTS_IDX_G3D0].clk);

	update_g3d_mode(mode);
}

void bts_change_g3d_state(unsigned int freq)
{
	bool g3d_on = exynos5_bts[BTS_IDX_G3D0].on;

	spin_lock(&bts_lock);

	BTS_DBG("[BTS] g3d freq changed %d\n", freq);

	if (!g3d_on) {
		spin_unlock(&bts_lock);
		return;
	}

	if (pr_state.fimc_on) {
		if (freq <= G3D_177) {
			if (!pr_state.g3d_mode)
				set_bts_g3d_table(true);
		} else {
			if (pr_state.g3d_mode)
				set_bts_g3d_table(false);
		}
	} else {
		if (pr_state.g3d_mode)
			set_bts_g3d_table(false);
	}

	update_g3d_freq(freq);

	spin_unlock(&bts_lock);
}

void bts_initialize(char *pd_name, bool on)
{
	struct bts_info *bts;
	bool fimc_state = false;
	bool g3d_state = false;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			bts->on = on;
			BTS_DBG("[BTS] %s on/off:%d\n", bts->name, bts->on);

			if (bts->id & BTS_FIMC) {
				update_fimc_on(on);
				fimc_state = on;
			}

			if (bts->id & BTS_G3D) {
				update_g3d_on(on);
				g3d_state = on;
			}

			if (on)
				set_bts_ip_table(bts);
		}
	if (g3d_state && pr_state.fimc_on) {
		update_g3d_freq(177);
		if (pr_state.g3d_freq <= G3D_177)
			set_bts_g3d_table(true);
	}

	if (fimc_state && pr_state.g3d_on) {
		if (pr_state.g3d_freq <= G3D_177)
			set_bts_g3d_table(true);
	}

	spin_unlock(&bts_lock);
}

static int __init exynos5_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		exynos5_bts[i].va_base
			= ioremap(exynos5_bts[i].pa_base, SZ_4K);

		if (exynos5_bts[i].clk_name) {
			clk = clk_get_sys(exynos5_bts[i].devname,
				exynos5_bts[i].clk_name);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
					exynos5_bts[i].clk_name);
			else
				exynos5_bts[i].clk = clk;
		}

		list_add(&exynos5_bts[i].list, &bts_list);
	}

	for (i = 0; i < ARRAY_SIZE(exynos5_bts_clk); i++) {
		clk = clk_get_sys(exynos5_bts_clk[i].devname, exynos5_bts_clk[i].clkname);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
					exynos5_bts_clk[i].clkname);
			else
				exynos5_bts_clk[i].clk = clk;
	}

	bts_drex_initialize();

	bts_initialize("pd-eagle", true);
	bts_initialize("pd-kfc", true);

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos5_bts_init);
