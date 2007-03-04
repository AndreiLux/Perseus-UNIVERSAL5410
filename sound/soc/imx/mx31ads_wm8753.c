/*
 * mx31ads_wm8753.c  --  SoC audio for mx31ads
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  mx31ads audio amplifier code taken from arch/arm/mach-pxa/mx31ads.c
 *  Copyright:	MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    30th Oct 2005   Initial version.
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

#include <asm/hardware.h>

#include "../codecs/wm8753.h"
#include "imx31-pcm.h"
#include "imx-ssi.h"

static struct snd_soc_machine mx31ads;

static int mx31ads_hifi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int pll_out = 0, bclk = 0, fmt = 0;
	int ret = 0;

	/*
	 * The WM8753 is better at generating accurate audio clocks than the
	 * MX31 SSI controller, so we will use it as master when we can.
	 */
	switch (params_rate(params)) {
	case 8000:
	case 16000:
		fmt = SND_SOC_DAIFMT_CBS_CFS;
		pll_out = 12288000;
		break;
	case 48000:
		fmt = SND_SOC_DAIFMT_CBM_CFS;
		bclk = WM8753_BCLK_DIV_4;
		pll_out = 12288000;
		break;
	case 96000:
		fmt = SND_SOC_DAIFMT_CBM_CFS;
		bclk = WM8753_BCLK_DIV_2;
		pll_out = 12288000;
		break;
	case 11025:
		fmt = SND_SOC_DAIFMT_CBM_CFS;
		bclk = WM8753_BCLK_DIV_16;
		pll_out = 11289600;
		break;
	case 22050:
		fmt = SND_SOC_DAIFMT_CBM_CFS;
		bclk = WM8753_BCLK_DIV_8;
		pll_out = 11289600;
		break;
	case 44100:
		fmt = SND_SOC_DAIFMT_CBM_CFS;
		bclk = WM8753_BCLK_DIV_4;
		pll_out = 11289600;
		break;
	case 88200:
		fmt = SND_SOC_DAIFMT_CBM_CFS;
		bclk = WM8753_BCLK_DIV_2;
		pll_out = 11289600;
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | fmt);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | fmt);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8753_MCLK, pll_out,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the SSI system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set codec BCLK division for sample rate */
	ret = codec_dai->dai_ops.set_clkdiv(codec_dai, WM8753_BCLKDIV, bclk);
	if (ret < 0)
		return ret;

	/* codec PLL input is 13 MHz */
	ret = codec_dai->dai_ops.set_pll(codec_dai, WM8753_PLL1, 13000000, pll_out);
	if (ret < 0)
		return ret;

	return 0;
}

static int mx31ads_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;

	/* disable the PLL */
	return codec_dai->dai_ops.set_pll(codec_dai, WM8753_PLL1, 0, 0);
}

/*
 * mx31ads WM8753 HiFi DAI opserations.
 */
static struct snd_soc_ops mx31ads_hifi_ops = {
	.hw_params = mx31ads_hifi_hw_params,
	.hw_free = mx31ads_hifi_hw_free,
};

static int mx31ads_voice_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void mx31ads_voice_shutdown(struct snd_pcm_substream *substream)
{
}

static int mx31ads_voice_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int pll_out = 0, bclk = 0, pcmdiv = 0;
	int ret = 0;

	/*
	 * The WM8753 is far better at generating accurate audio clocks than the
	 * pxa2xx SSP controller, so we will use it as master when we can.
	 */
	switch (params_rate(params)) {
	case 8000:
		pll_out = 12288000;
		pcmdiv = WM8753_PCM_DIV_6; /* 2.048 MHz */
		bclk = WM8753_VXCLK_DIV_8; /* 256kHz */
		break;
	case 16000:
		pll_out = 12288000;
		pcmdiv = WM8753_PCM_DIV_3; /* 4.096 MHz */
		bclk = WM8753_VXCLK_DIV_8; /* 512kHz */
		break;
	case 48000:
		pll_out = 12288000;
		pcmdiv = WM8753_PCM_DIV_1; /* 12.288 MHz */
		bclk = WM8753_VXCLK_DIV_8; /* 1.536 MHz */
		break;
	case 11025:
		pll_out = 11289600;
		pcmdiv = WM8753_PCM_DIV_4; /* 11.2896 MHz */
		bclk = WM8753_VXCLK_DIV_8; /* 352.8 kHz */
		break;
	case 22050:
		pll_out = 11289600;
		pcmdiv = WM8753_PCM_DIV_2; /* 11.2896 MHz */
		bclk = WM8753_VXCLK_DIV_8; /* 705.6 kHz */
		break;
	case 44100:
		pll_out = 11289600;
		pcmdiv = WM8753_PCM_DIV_1; /* 11.2896 MHz */
		bclk = WM8753_VXCLK_DIV_8; /* 1.4112 MHz */
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8753_PCMCLK, pll_out,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the SSP system clock as input (unused) */
//	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_SSP_CLK_PLL, 0,
//		SND_SOC_CLOCK_IN);
//	if (ret < 0)
//		return ret;

	/* set codec BCLK division for sample rate */
	ret = codec_dai->dai_ops.set_clkdiv(codec_dai, WM8753_VXCLKDIV, bclk);
	if (ret < 0)
		return ret;

	/* set codec PCM division for sample rate */
	ret = codec_dai->dai_ops.set_clkdiv(codec_dai, WM8753_PCMDIV, pcmdiv);
	if (ret < 0)
		return ret;

	/* codec PLL input is 13 MHz */
	ret = codec_dai->dai_ops.set_pll(codec_dai, WM8753_PLL2, 13000000, pll_out);
	if (ret < 0)
		return ret;

	return 0;
}

static int mx31ads_voice_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;

	/* disable the PLL */
	return codec_dai->dai_ops.set_pll(codec_dai, WM8753_PLL2, 0, 0);
}

static struct snd_soc_ops mx31ads_voice_ops = {
	.startup = mx31ads_voice_startup,
	.shutdown = mx31ads_voice_shutdown,
	.hw_params = mx31ads_voice_hw_params,
	.hw_free = mx31ads_voice_hw_free,
};

static int mx31ads_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mx31ads_resume(struct platform_device *pdev)
{
	return 0;
}

static int mx31ads_probe(struct platform_device *pdev)
{
	return 0;
}

static int mx31ads_remove(struct platform_device *pdev)
{
	return 0;
}

/* example machine audio_mapnections */
static const char* audio_map[][3] = {

	/* mic is connected to mic1 - with bias */
	{"MIC1", NULL, "Mic Bias"},
	{"MIC1N", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic1 Jack"},
	{"Mic Bias", NULL, "Mic1 Jack"},

	{"ACIN", NULL, "ACOP"},
	{NULL, NULL, NULL},
};

/* headphone detect support on my board */
static const char * hp_pol[] = {"Headphone", "Speaker"};
static const struct soc_enum wm8753_enum =
	SOC_ENUM_SINGLE(WM8753_OUTCTL, 1, 2, hp_pol);

static const struct snd_kcontrol_new wm8753_mx31ads_controls[] = {
	SOC_SINGLE("Headphone Detect Switch", WM8753_OUTCTL, 6, 1, 0),
	SOC_ENUM("Headphone Detect Polarity", wm8753_enum),
};

/*
 * This is an example machine initialisation for a wm8753 connected to a
 * mx31ads II. It is missing logic to detect hp/mic insertions and logic
 * to re-route the audio in such an event.
 */
static int mx31ads_wm8753_init(struct snd_soc_codec *codec)
{
	int i, err;

	/* set up mx31ads codec pins */
	snd_soc_dapm_set_endpoint(codec, "RXP", 0);
	snd_soc_dapm_set_endpoint(codec, "RXN", 0);
	snd_soc_dapm_set_endpoint(codec, "MIC2", 0);

	/* add mx31ads specific controls */
	for (i = 0; i < ARRAY_SIZE(wm8753_mx31ads_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&wm8753_mx31ads_controls[i],codec, NULL))) < 0)
			return err;
	}

	/* set up mx31ads specific audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0], audio_map[i][1], audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

static struct snd_soc_dai_link mx31ads_dai[] = {
{ /* Hifi Playback - for similatious use with voice below */
	.name = "WM8753",
	.stream_name = "WM8753 HiFi",
	.cpu_dai = &imx_ssi_pcm_dai[0],
	.codec_dai = &wm8753_dai[WM8753_DAI_HIFI],
	.init = mx31ads_wm8753_init,
	.ops = &mx31ads_hifi_ops,
},
//{ /* Voice via BT */
//	.name = "Bluetooth",
//	.stream_name = "Voice",
//	.cpu_dai = &pxa_ssp_dai[1],
//	.codec_dai = &wm8753_dai[WM8753_DAI_VOICE],
//	.ops = &mx31ads_voice_ops,
//},
};

static struct snd_soc_machine mx31ads = {
	.name = "mx31ads",
	.probe = mx31ads_probe,
	.remove = mx31ads_remove,
	.suspend_pre = mx31ads_suspend,
	.resume_post = mx31ads_resume,
	.dai_link = mx31ads_dai,
	.num_links = ARRAY_SIZE(mx31ads_dai),
};

static struct wm8753_setup_data mx31ads_wm8753_setup = {
	.i2c_address = 0x1a,
};

static struct snd_soc_device mx31ads_snd_devdata = {
	.machine = &mx31ads,
	.platform = &mxc_soc_platform,
	.codec_dev = &soc_codec_dev_wm8753,
	.codec_data = &mx31ads_wm8753_setup,
};

static struct platform_device *mx31ads_snd_device;

static int __init mx31ads_init(void)
{
	int ret;

	mx31ads_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mx31ads_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mx31ads_snd_device, &mx31ads_snd_devdata);
	mx31ads_snd_devdata.dev = &mx31ads_snd_device->dev;
	ret = platform_device_add(mx31ads_snd_device);

	if (ret)
		platform_device_put(mx31ads_snd_device);

	return ret;
}

static void __exit mx31ads_exit(void)
{
	platform_device_unregister(mx31ads_snd_device);
}

module_init(mx31ads_init);
module_exit(mx31ads_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC WM8753 mx31ads");
MODULE_LICENSE("GPL");
