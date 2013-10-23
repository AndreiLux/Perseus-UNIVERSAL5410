/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>

#include <mach/map.h>
#include <mach/exynos5-audio.h>
#include <mach/regs-pmu.h>


static struct clk *mclk_clkout;
static unsigned int mclk_usecount;

struct platform_device *exynos5_audio_devices[] __initdata = {
#ifdef CONFIG_SND_SAMSUNG_AC97
	&exynos_device_ac97,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos5_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos5_device_pcm0,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos5_device_spdif,
#endif
#if defined(CONFIG_SND_SAMSUNG_RP) || defined(CONFIG_SND_SAMSUNG_ALP)
	&exynos5_device_srp,
#endif
	&samsung_asoc_dma,
	&samsung_asoc_idma,
};

static void exynos5_audio_setup_clocks(void)
{
	struct clk *xxti;
	struct clk *clkout;
	struct clk *epll;


	xxti = clk_get(NULL, "xxti");
	if (!xxti) {
		pr_err("%s: cannot get xxti clock\n", __func__);
		return;
	}

	clkout = clk_get(NULL, "clkout");
	if (!clkout) {
		pr_err("%s: cannot get clkout\n", __func__);
		clk_put(xxti);
		return;
	}

	clk_set_parent(clkout, xxti);
	clk_put(clkout);
	clk_put(xxti);


	epll = clk_get(NULL, "fout_epll");
	if (!epll) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		return;
	}

	mclk_clkout = clk_get(NULL, "clkout");
	if (!mclk_clkout) {
		pr_err("%s: cannot get get clkout (clkout)\n", __func__);
		clk_put(epll);
		return;
	}
}

void exynos5_audio_set_mclk(bool enable, bool forced)
{
	void __iomem *exynos_etc6;

	/* forced disable */
	if (forced) {
		mclk_usecount = 0;
		clk_disable(mclk_clkout);

		pr_info("%s: mclk forced disable\n", __func__);
		pr_info("%s: %x\n", __func__, readl(EXYNOS_PMU_DEBUG));
		return;
	}

	if (enable) {
		/* Change MCLK drive strength */
		exynos_etc6 = ioremap(EXYNOS5410_PA_GPIO_LB, SZ_4K);

		if (exynos_etc6 == NULL)
			pr_err("unable to ioremap for EINT base address\n");
		else
			__raw_writel(0x000004, exynos_etc6 + 0x16C);

		if (mclk_usecount == 0) {
			clk_enable(mclk_clkout);
			pr_info("%s: mclk enable\n", __func__);
		}

		mclk_usecount++;

		iounmap(exynos_etc6);
	} else {
		if (--mclk_usecount > 0)
			return;

		clk_disable(mclk_clkout);
		pr_info("%s: mclk disable\n", __func__);
	}

	pr_info("%s: %x\n", __func__, readl(EXYNOS_PMU_DEBUG));
}
EXPORT_SYMBOL(exynos5_audio_set_mclk);

void __init exynos5_audio_init(void)
{
	exynos5_audio_setup_clocks();

	platform_add_devices(exynos5_audio_devices,
			ARRAY_SIZE(exynos5_audio_devices));
}
