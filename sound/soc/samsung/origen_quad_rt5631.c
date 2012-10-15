/*
 *  Copyright (c) 2012, Insignal Co., Ltd.
 *
 *  Author: Claude <claude@insginal.co.kr>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "i2s.h"

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}

static int origen_quad_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		printk("Unsupported PCM Format - %d\n", params_format(params));
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		printk("Unsupported Rate - %d\n", params_rate(params));
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_0,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops origen_quad_ops = {
	.hw_params = origen_quad_hw_params,
};

static int origen_quad_ak4678_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link origen_quad_dai[] = {
	{
		.name = "RT5631 HiFi",
		.stream_name = "Primary",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "rt5631-hifi",
		.platform_name = "samsung-i2s.0",
		.codec_name = "rt5631.1-001a",
		.init = origen_quad_ak4678_init_paiftx,
		.ops = &origen_quad_ops,
	},
};

static struct snd_soc_card origen_quad = {
	.name = "OrigenQuad",
	.dai_link = origen_quad_dai,
	.num_links = ARRAY_SIZE(origen_quad_dai),
};

static struct platform_device *origen_quad_snd_device;

#include <linux/io.h>
#include <mach/regs-clock.h>

#define CLKOUT		S5P_PMUREG(0x0A00)
#define ETC6PUD		(S5P_VA_GPIO2 + 0x228)

static int __init origen_quad_audio_init(void)
{
	int ret;

	unsigned int reg;
	void __iomem *val;

	reg = __raw_readl(CLKOUT);
	reg = (8 << 8);
	__raw_writel(reg, CLKOUT);

	val = ioremap(0x11000228, 4);

	reg = __raw_readl(val);
	reg &= ~(3 << 2);
	reg |= (3 << 2);
	__raw_writel(reg, val);

	origen_quad_snd_device = platform_device_alloc("soc-audio", -1);
	if (!origen_quad_snd_device)
		return -ENOMEM;

	platform_set_drvdata(origen_quad_snd_device, &origen_quad);

	ret = platform_device_add(origen_quad_snd_device);
	if (ret)
		platform_device_put(origen_quad_snd_device);

	return ret;
}
module_init(origen_quad_audio_init);

static void __exit origen_quad_audio_exit(void)
{
	platform_device_unregister(origen_quad_snd_device);
}
module_exit(origen_quad_audio_exit);

MODULE_AUTHOR("Claude <claude@insignal.co.kr>");
MODULE_DESCRIPTION("ALSA SoC Driver for Origen Quad Board");
MODULE_LICENSE("GPL");
