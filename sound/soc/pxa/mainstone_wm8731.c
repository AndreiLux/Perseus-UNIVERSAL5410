/*
 * mainstone.c  --  SoC audio for Mainstone
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  Mainstone audio amplifier code taken from arch/arm/mach-pxa/mainstone.c
 *  Copyright:	MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    5th June 2006   Initial version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/mainstone.h>
#include <asm/arch/audio.h>

#include "../codecs/wm8731.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-i2s.h"

static struct snd_soc_machine mainstone;

static int mainstone_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int clk = 0;
	int ret = 0;

	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 48000:
	case 96000:
		clk = 12288000;
		break;
	case 11025:
	case 22050:
	case 44100:
		clk = 11289600;
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8731_SYSCLK, clk,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_I2S_SYSCLK, 0,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops mainstone_ops = {
	.hw_params = mainstone_hw_params,
};

static const struct snd_soc_dapm_widget dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
};

static const char* intercon[][3] = {

	/* speaker connected to LHPOUT */
	{"Ext Spk", NULL, "LHPOUT"},

	/* mic is connected to Mic Jack, with WM8731 Mic Bias */
	{"MICIN", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Int Mic"},

	/* terminator */
	{NULL, NULL, NULL},
};

/*
 * Logic for a wm8731 as connected on a Endrelia ETI-B1 board.
 */
static int mainstone_wm8731_init(struct snd_soc_codec *codec)
{
	int i;


	/* Add specific widgets */
	for(i = 0; i < ARRAY_SIZE(dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &dapm_widgets[i]);
	}

	/* Set up specific audio path interconnects */
	for(i = 0; intercon[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, intercon[i][0], intercon[i][1], intercon[i][2]);
	}

	/* not connected */
	snd_soc_dapm_set_endpoint(codec, "RLINEIN", 0);
	snd_soc_dapm_set_endpoint(codec, "LLINEIN", 0);

	/* always connected */
	snd_soc_dapm_set_endpoint(codec, "Int Mic", 1);
	snd_soc_dapm_set_endpoint(codec, "Ext Spk", 1);

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

static struct snd_soc_dai_link mainstone_dai[] = {
{
	.name = "WM8731",
	.stream_name = "WM8731 HiFi",
	.cpu_dai = &pxa_i2s_dai,
	.codec_dai = &wm8731_dai,
	.init = mainstone_wm8731_init,
	.ops = &mainstone_ops,
	},
};

static struct snd_soc_machine mainstone = {
	.name = "Mainstone",
	.dai_link = mainstone_dai,
	.num_links = ARRAY_SIZE(mainstone_dai),
};

static struct wm8731_setup_data corgi_wm8731_setup = {
	.i2c_address = 0x1b,
};

static struct snd_soc_device mainstone_snd_devdata = {
	.machine = &mainstone,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8731,
	.codec_data = &corgi_wm8731_setup,
};

static struct platform_device *mainstone_snd_device;

static int __init mainstone_init(void)
{
	int ret;

	mainstone_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mainstone_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mainstone_snd_device, &mainstone_snd_devdata);
	mainstone_snd_devdata.dev = &mainstone_snd_device->dev;
	ret = platform_device_add(mainstone_snd_device);

	if (ret)
		platform_device_put(mainstone_snd_device);

	return ret;
}

static void __exit mainstone_exit(void)
{
	platform_device_unregister(mainstone_snd_device);
}

module_init(mainstone_init);
module_exit(mainstone_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC WM8731 Mainstone");
MODULE_LICENSE("GPL");
