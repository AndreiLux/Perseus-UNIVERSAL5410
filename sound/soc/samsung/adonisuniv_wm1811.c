/*
 *  adonisuniv_wm1811.c
 *
 *  Copyright (c) 2011 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/gpio.h>
#include <mach/exynos5-audio.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>


#include "i2s.h"
#include "i2s-regs.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8994.h"


#define ADONISUNIV_DEFAULT_MCLK1	24000000
#define ADONISUNIV_DEFAULT_MCLK2	32768
#define ADONISUNIV_DEFAULT_SYNC_CLK	11289600

#define GPIO_MIC_BIAS_EN EXYNOS5410_GPJ1(2)


struct wm1811_machine_priv {
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct delayed_work mic_work;
	struct wake_lock jackdet_wake_lock;
};

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
static int adonisuniv_wm1811_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct wm1811_machine_priv *wm1811 = snd_soc_card_get_drvdata(codec->card);
	unsigned int pll_out;
	int bfs, rfs, ret;

	bfs = (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) ? 48 : 32;

	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		rfs = 512;
	else
		rfs = 256;

	pll_out = params_rate(params) * rfs;

	if (clk_get_rate(wm1811->pll) != (pll_out * 4))
		clk_set_rate(wm1811->pll, pll_out * 4);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
			pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1,
			WM8994_FLL_SRC_BCLK,
			params_rate(params) * bfs,
			params_rate(params) * rfs);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to start FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
			0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
			0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
			rfs, SND_SOC_CLOCK_OUT);

	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}
#else
static int adonisuniv_wm1811_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	dev_info(codec_dai->dev, "%s ++\n", __func__);
	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1, ADONISUNIV_DEFAULT_MCLK1,
				  pll_out);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to start FLL1: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
				     0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	dev_info(codec_dai->dev, "%s --\n", __func__);

	return 0;
}
#endif

static void adonisuniv_gpio_init(void)
{
	int err;

	/* Main Microphone BIAS */
	err = gpio_request(GPIO_MIC_BIAS_EN, "GPJ1");
	if (err) {
		pr_err(KERN_ERR "MIC_BIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_MIC_BIAS_EN, 1);

	/*This is tempary code to enable for main mic.(force enable GPIO) */
	gpio_set_value(GPIO_MIC_BIAS_EN, 0);
	gpio_free(GPIO_MIC_BIAS_EN);
}

/*
 * AdnoisUniv WM1811 GPIO enable configure.
 */
static int adonisuniv_ext_mainmicbias(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,  int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {

	case SND_SOC_DAPM_PRE_PMU:
		gpio_set_value(GPIO_MIC_BIAS_EN,  1);
		break;

	case SND_SOC_DAPM_POST_PMD:
		gpio_set_value(GPIO_MIC_BIAS_EN,  0);
		break;
	}

	return 0;
}

/*
 * AdnoisUniv WM1811 DAI operations.
 */
static struct snd_soc_ops adonisuniv_wm1811_aif1_ops = {
	.hw_params = adonisuniv_wm1811_aif1_hw_params,
};

static const struct snd_kcontrol_new adonisuniv_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("FM In"),
	SOC_DAPM_PIN_SWITCH("LINE"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget adonisuniv_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINE", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", adonisuniv_ext_mainmicbias),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_LINE("FM In", NULL),
};

const struct snd_soc_dapm_route adonisuniv_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "RCV", NULL, "HPOUT2N" },
	{ "RCV", NULL, "HPOUT2P" },

	{ "LINE", NULL, "LINEOUT2N" },
	{ "LINE", NULL, "LINEOUT2P" },

	{ "IN2LP:VXRN", NULL, "Main Mic" },
	{ "IN2LN", NULL, "Main Mic" },

	{ "IN1LP", NULL, "Headset Mic" },
	{ "IN1LN", NULL, "Headset Mic" },

	{ "IN1RP", NULL, "Sub Mic" },
	{ "IN1RN", NULL, "Sub Mic" },

	{ "IN1RP", NULL, "MICBIAS1" },
	{ "MICBIAS1", NULL, "Sub Mic" },

	{ "IN1LP", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },

};

static int adonisuniv_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	int ret;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK1,
				     ADONISUNIV_DEFAULT_MCLK1, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to boot clocking\n");

#if 0
	/* Force AIF1CLK on as it will be master for jack detection */
	ret = snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
	if (ret < 0)
		dev_err(codec->dev, "Failed to enable AIF1CLK: %d\n", ret);
#endif

	snd_soc_dapm_ignore_suspend(&codec->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "FM In");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "LINE");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "HDMI");

	wm1811->codec = codec;

	return 0;
}

static struct snd_soc_dai_link adonisuniv_dai[] = {
	{ /* Primary DAI i/f */
		.name = "WM8994 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.ops = &adonisuniv_wm1811_aif1_ops,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "wm8994-aif1",
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-idma",
#else
		.platform_name = "samsung-audio",
#endif
		.codec_name = "wm8994-codec",
		.ops = &adonisuniv_wm1811_aif1_ops,
	},
};


static struct snd_soc_card adonisuniv = {
	.name = "AdonisUniv Sound Card",
	.owner = THIS_MODULE,
	.dai_link = adonisuniv_dai,
	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(adonisuniv_dai). */
	.num_links = ARRAY_SIZE(adonisuniv_dai),

	.controls = adonisuniv_controls,
	.num_controls = ARRAY_SIZE(adonisuniv_controls),
	.dapm_widgets = adonisuniv_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(adonisuniv_dapm_widgets),
	.dapm_routes = adonisuniv_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(adonisuniv_dapm_routes),

	.late_probe = adonisuniv_late_probe,
};

static int __devinit snd_adonisuniv_probe(struct platform_device *pdev)
{
	int ret;
	struct wm1811_machine_priv *wm1811;

	wm1811 = kzalloc(sizeof *wm1811, GFP_KERNEL);
	if (!wm1811) {
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	exynos5_audio_set_mclk(1, 0);

	snd_soc_card_set_drvdata(&adonisuniv, wm1811);

	adonisuniv.dev = &pdev->dev;
	ret = snd_soc_register_card(&adonisuniv);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		kfree(wm1811);
	}

	adonisuniv_gpio_init();

	return ret;
}

static int __devexit snd_adonisuniv_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = &adonisuniv;
	struct wm1811_machine_priv *wm1811 = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(&adonisuniv);
	kfree(wm1811);

	exynos5_audio_set_mclk(0, 0);

	return 0;
}

static struct platform_driver snd_adonisuniv_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wm1811-card",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_adonisuniv_probe,
	.remove = __devexit_p(snd_adonisuniv_remove),
};

module_platform_driver(snd_adonisuniv_driver);

MODULE_AUTHOR("JS. Park <aitdark.park@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC AdonisUniv WM1811");
MODULE_LICENSE("GPL");
