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
#include <asm/arch/pxa-regs.h>
#include <asm/arch/mainstone.h>
#include <asm/arch/audio.h>

#include "../codecs/wm8753.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-i2s.h"
#include "pxa2xx-ssp.h"

/*
 * SSP GPIO's
 */
#define GPIO26_SSP1RX_MD	(26 | GPIO_ALT_FN_1_IN)
#define GPIO25_SSP1TX_MD	(25 | GPIO_ALT_FN_2_OUT)
#define GPIO23_SSP1CLKS_MD	(23 | GPIO_ALT_FN_2_IN)
#define GPIO24_SSP1FRMS_MD	(24 | GPIO_ALT_FN_2_IN)
#define GPIO23_SSP1CLKM_MD	(23 | GPIO_ALT_FN_2_OUT)
#define GPIO24_SSP1FRMM_MD	(24 | GPIO_ALT_FN_2_OUT)
#define GPIO53_SSP1SYSCLK_MD	(53 | GPIO_ALT_FN_2_OUT)

#define GPIO11_SSP2RX_MD	(11 | GPIO_ALT_FN_2_IN)
#define GPIO13_SSP2TX_MD	(13 | GPIO_ALT_FN_1_OUT)
#define GPIO22_SSP2CLKS_MD	(22 | GPIO_ALT_FN_3_IN)
#define GPIO88_SSP2FRMS_MD	(88 | GPIO_ALT_FN_3_IN)
#define GPIO22_SSP2CLKM_MD	(22 | GPIO_ALT_FN_3_OUT)
#define GPIO88_SSP2FRMM_MD	(88 | GPIO_ALT_FN_3_OUT)
#define GPIO22_SSP2SYSCLK_MD	(22 | GPIO_ALT_FN_2_OUT)

#define GPIO82_SSP3RX_MD	(82 | GPIO_ALT_FN_1_IN)
#define GPIO81_SSP3TX_MD	(81 | GPIO_ALT_FN_1_OUT)
#define GPIO84_SSP3CLKS_MD	(84 | GPIO_ALT_FN_1_IN)
#define GPIO83_SSP3FRMS_MD	(83 | GPIO_ALT_FN_1_IN)
#define GPIO84_SSP3CLKM_MD	(84 | GPIO_ALT_FN_1_OUT)
#define GPIO83_SSP3FRMM_MD	(83 | GPIO_ALT_FN_1_OUT)
#define GPIO45_SSP3SYSCLK_MD	(45 | GPIO_ALT_FN_3_OUT)

#if 0
static struct pxa2xx_gpio ssp_gpios[3][4] = {
	{{ /* SSP1 SND_SOC_DAIFMT_CBM_CFM */
		.rx = GPIO26_SSP1RX_MD,
		.tx = GPIO25_SSP1TX_MD,
		.clk = (23 | GPIO_ALT_FN_2_IN),
		.frm = (24 | GPIO_ALT_FN_2_IN),
		.sys = GPIO53_SSP1SYSCLK_MD,
	},
	{ /* SSP1 SND_SOC_DAIFMT_CBS_CFS */
		.rx = GPIO26_SSP1RX_MD,
		.tx = GPIO25_SSP1TX_MD,
		.clk = (23 | GPIO_ALT_FN_2_OUT),
		.frm = (24 | GPIO_ALT_FN_2_OUT),
		.sys = GPIO53_SSP1SYSCLK_MD,
	},
	{ /* SSP1 SND_SOC_DAIFMT_CBS_CFM */
		.rx = GPIO26_SSP1RX_MD,
		.tx = GPIO25_SSP1TX_MD,
		.clk = (23 | GPIO_ALT_FN_2_OUT),
		.frm = (24 | GPIO_ALT_FN_2_IN),
		.sys = GPIO53_SSP1SYSCLK_MD,
	},
	{ /* SSP1 SND_SOC_DAIFMT_CBM_CFS */
		.rx = GPIO26_SSP1RX_MD,
		.tx = GPIO25_SSP1TX_MD,
		.clk = (23 | GPIO_ALT_FN_2_IN),
		.frm = (24 | GPIO_ALT_FN_2_OUT),
		.sys = GPIO53_SSP1SYSCLK_MD,
	}},
	{{ /* SSP2 SND_SOC_DAIFMT_CBM_CFM */
		.rx = GPIO11_SSP2RX_MD,
		.tx = GPIO13_SSP2TX_MD,
		.clk = (22 | GPIO_ALT_FN_3_IN),
		.frm = (88 | GPIO_ALT_FN_3_IN),
		.sys = GPIO22_SSP2SYSCLK_MD,
	},
	{ /* SSP2 SND_SOC_DAIFMT_CBS_CFS */
		.rx = GPIO11_SSP2RX_MD,
		.tx = GPIO13_SSP2TX_MD,
		.clk = (22 | GPIO_ALT_FN_3_OUT),
		.frm = (88 | GPIO_ALT_FN_3_OUT),
		.sys = GPIO22_SSP2SYSCLK_MD,
	},
	{ /* SSP2 SND_SOC_DAIFMT_CBS_CFM */
		.rx = GPIO11_SSP2RX_MD,
		.tx = GPIO13_SSP2TX_MD,
		.clk = (22 | GPIO_ALT_FN_3_OUT),
		.frm = (88 | GPIO_ALT_FN_3_IN),
		.sys = GPIO22_SSP2SYSCLK_MD,
	},
	{ /* SSP2 SND_SOC_DAIFMT_CBM_CFS */
		.rx = GPIO11_SSP2RX_MD,
		.tx = GPIO13_SSP2TX_MD,
		.clk = (22 | GPIO_ALT_FN_3_IN),
		.frm = (88 | GPIO_ALT_FN_3_OUT),
		.sys = GPIO22_SSP2SYSCLK_MD,
	}},
	{{ /* SSP3 SND_SOC_DAIFMT_CBM_CFM */
		.rx = GPIO82_SSP3RX_MD,
		.tx = GPIO81_SSP3TX_MD,
		.clk = (84 | GPIO_ALT_FN_3_IN),
		.frm = (83 | GPIO_ALT_FN_3_IN),
		.sys = GPIO45_SSP3SYSCLK_MD,
	},
	{ /* SSP3 SND_SOC_DAIFMT_CBS_CFS */
		.rx = GPIO82_SSP3RX_MD,
		.tx = GPIO81_SSP3TX_MD,
		.clk = (84 | GPIO_ALT_FN_3_OUT),
		.frm = (83 | GPIO_ALT_FN_3_OUT),
		.sys = GPIO45_SSP3SYSCLK_MD,
	},
	{ /* SSP3 SND_SOC_DAIFMT_CBS_CFM */
		.rx = GPIO82_SSP3RX_MD,
		.tx = GPIO81_SSP3TX_MD,
		.clk = (84 | GPIO_ALT_FN_3_OUT),
		.frm = (83 | GPIO_ALT_FN_3_IN),
		.sys = GPIO45_SSP3SYSCLK_MD,
	},
	{ /* SSP3 SND_SOC_DAIFMT_CBM_CFS */
		.rx = GPIO82_SSP3RX_MD,
		.tx = GPIO81_SSP3TX_MD,
		.clk = (84 | GPIO_ALT_FN_3_IN),
		.frm = (83 | GPIO_ALT_FN_3_OUT),
		.sys = GPIO45_SSP3SYSCLK_MD,
	}},
};
#endif

static struct snd_soc_machine mainstone;

static int mainstone_hifi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int pll_out = 0, bclk = 0, fmt = 0;
	int ret = 0;

	/*
	 * The WM8753 is far better at generating accurate audio clocks than the
	 * pxa2xx I2S controller, so we will use it as master when we can.
	 * i.e all rates except 8k and 16k as BCLK must be 64 * rate when the
	 * pxa27x or pxa25x is slave. Note this restriction does not apply to SSP
	 * I2S emulation mode.
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

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_I2S_SYSCLK, 0,
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

static int mainstone_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;

	/* disable the PLL */
	return codec_dai->dai_ops.set_pll(codec_dai, WM8753_PLL1, 0, 0);
}

/*
 * Mainstone WM8753 HiFi DAI opserations.
 */
static struct snd_soc_ops mainstone_hifi_ops = {
	.hw_params = mainstone_hifi_hw_params,
	.hw_free = mainstone_hifi_hw_free,
};

static int mainstone_voice_startup(struct snd_pcm_substream *substream)
{
	/* enable USB on the go MUX so we can use SSPFRM2 */
	MST_MSCWR2 |= MST_MSCWR2_USB_OTG_SEL;
	MST_MSCWR2 &= ~MST_MSCWR2_USB_OTG_RST;

	return 0;
}

static void mainstone_voice_shutdown(struct snd_pcm_substream *substream)
{
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	/* disable USB on the go MUX so we can use ttyS0 */
	MST_MSCWR2 &= ~MST_MSCWR2_USB_OTG_SEL;
	MST_MSCWR2 |= MST_MSCWR2_USB_OTG_RST;

	/* liam may need to tristate DAI */
}

static int mainstone_voice_hw_params(struct snd_pcm_substream *substream,
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
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_SSP_CLK_PLL, 0,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

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

static int mainstone_voice_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;

	/* disable the PLL */
	return codec_dai->dai_ops.set_pll(codec_dai, WM8753_PLL2, 0, 0);
}

static struct snd_soc_ops mainstone_voice_ops = {
	.startup = mainstone_voice_startup,
	.shutdown = mainstone_voice_shutdown,
	.hw_params = mainstone_voice_hw_params,
	.hw_free = mainstone_voice_hw_free,
};

static long mst_audio_suspend_mask;

static int mainstone_suspend(struct platform_device *pdev, pm_message_t state)
{
	mst_audio_suspend_mask = MST_MSCWR2;
	MST_MSCWR2 |= MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mainstone_resume(struct platform_device *pdev)
{
	MST_MSCWR2 &= mst_audio_suspend_mask | ~MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mainstone_probe(struct platform_device *pdev)
{
	MST_MSCWR2 &= ~MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mainstone_remove(struct platform_device *pdev)
{
	MST_MSCWR2 |= MST_MSCWR2_AC97_SPKROFF;
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

static const struct snd_kcontrol_new wm8753_mainstone_controls[] = {
	SOC_SINGLE("Headphone Detect Switch", WM8753_OUTCTL, 6, 1, 0),
	SOC_ENUM("Headphone Detect Polarity", wm8753_enum),
};

/*
 * This is an example machine initialisation for a wm8753 connected to a
 * Mainstone II. It is missing logic to detect hp/mic insertions and logic
 * to re-route the audio in such an event.
 */
static int mainstone_wm8753_init(struct snd_soc_codec *codec)
{
	int i, err;

	/* set up mainstone codec pins */
	snd_soc_dapm_set_endpoint(codec, "RXP", 0);
	snd_soc_dapm_set_endpoint(codec, "RXN", 0);
	snd_soc_dapm_set_endpoint(codec, "MIC2", 0);

	/* add mainstone specific controls */
	for (i = 0; i < ARRAY_SIZE(wm8753_mainstone_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&wm8753_mainstone_controls[i],codec, NULL))) < 0)
			return err;
	}

	/* set up mainstone specific audio path audio_mapnects */
	for(i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0], audio_map[i][1], audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

static struct snd_soc_dai_link mainstone_dai[] = {
{ /* Hifi Playback - for similatious use with voice below */
	.name = "WM8753",
	.stream_name = "WM8753 HiFi",
	.cpu_dai = &pxa_i2s_dai,
	.codec_dai = &wm8753_dai[WM8753_DAI_HIFI],
	.init = mainstone_wm8753_init,
	.ops = &mainstone_hifi_ops,
},
{ /* Voice via BT */
	.name = "Bluetooth",
	.stream_name = "Voice",
	.cpu_dai = &pxa_ssp_dai[1],
	.codec_dai = &wm8753_dai[WM8753_DAI_VOICE],
	.ops = &mainstone_voice_ops,
},
};

static struct snd_soc_machine mainstone = {
	.name = "Mainstone",
	.probe = mainstone_probe,
	.remove = mainstone_remove,
	.suspend_pre = mainstone_suspend,
	.resume_post = mainstone_resume,
	.dai_link = mainstone_dai,
	.num_links = ARRAY_SIZE(mainstone_dai),
};

static struct wm8753_setup_data mainstone_wm8753_setup = {
	.i2c_address = 0x1a,
};

static struct snd_soc_device mainstone_snd_devdata = {
	.machine = &mainstone,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8753,
	.codec_data = &mainstone_wm8753_setup,
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

	/* SSP port 2 slave */
	pxa_gpio_mode(GPIO11_SSP2RX_MD);
	pxa_gpio_mode(GPIO13_SSP2TX_MD);
	pxa_gpio_mode(GPIO22_SSP2CLKS_MD);
	pxa_gpio_mode(GPIO88_SSP2FRMS_MD);

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
MODULE_DESCRIPTION("ALSA SoC WM8753 Mainstone");
MODULE_LICENSE("GPL");
