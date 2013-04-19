/*
 *  adonisduos_ymu831.c
 *
 *  Copyright (c) 2012 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or  modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#if defined(CONFIG_SND_DUOS_MODEM_SWITCH)
#include <linux/gpio.h>
#endif
#include <linux/switch.h>

#include <mach/regs-clock.h>
#include <mach/exynos5-audio.h>

#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "../codecs/ymu831/ymu831.h"
#include "../codecs/ymu831/ymu831_priv.h"

#include "jack_ymu831.c"

#if defined(CONFIG_SND_DUOS_MODEM_SWITCH)
static int modem_mode;
const char *modem_mode_text[] = {
	"CP1", "CP2"
};
#endif

struct ymu831_machine_priv {
	struct snd_soc_codec *codec;
};

#if defined(CONFIG_SND_DUOS_MODEM_SWITCH)
static const struct soc_enum modem_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(modem_mode_text), modem_mode_text),
};

static int get_modem_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = modem_mode;
	return 0;
}

static int set_modem_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	modem_mode = ucontrol->value.integer.value[0];

	if (modem_mode) {
		gpio_set_value(GPIO_PCM_SEL, 1);
	} else {
		gpio_set_value(GPIO_PCM_SEL, 0);
		/*msleep(50);*/
	}
	dev_info(codec->dev, "set modem select : %s\n",
		modem_mode_text[modem_mode]);
	return 0;
}
#endif

static int adonisduos_hifi_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* Set the codec DAI configuration */
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

	ret = snd_soc_dai_set_clkdiv(codec_dai, MC_ASOC_BCLK_MULT,
				MC_ASOC_LRCK_X32);

	if (ret < 0)
		return ret;

	return 0;
}

#if defined(CONFIG_SND_DUOS_MODEM_SWITCH)
static const struct snd_kcontrol_new adonis_controls[] = {
	SOC_ENUM_EXT("ModemSwitch Mode", modem_mode_enum[0],
		get_modem_mode, set_modem_mode),
};
#endif

static int adonisduos_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct ymu831_machine_priv *ymu831;

	ymu831 = snd_soc_card_get_drvdata(card);
	ymu831->codec = codec;

#if defined(CONFIG_SND_DUOS_MODEM_SWITCH)
	snd_soc_add_codec_controls(codec, adonis_controls,
					ARRAY_SIZE(adonis_controls));
#endif

	codec_dai->driver->playback.channels_max = 6;
	cpu_dai->driver->playback.channels_max = 6;

	create_jack_devices(codec);

	return 0;
}

/*
 * adonisduos YMU831 DAI operations.
 */
static struct snd_soc_ops adonisduos_hifi_ops = {
	.hw_params = adonisduos_hifi_hw_params,
};

static struct snd_soc_dai_link adonisduos_dai[] = {
	{ /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "ymu831-da0",
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-idma",
#else
		.platform_name = "samsung-audio",
#endif
		.codec_name = "spi2.0",
		.ops = &adonisduos_hifi_ops,
	},
	{ /* Primary DAI i/f */
		.name = "YMU831 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "ymu831-da0",
		.platform_name = "samsung-audio",
		.codec_name = "spi2.0",
		.ops = &adonisduos_hifi_ops,
	},
};

static struct snd_soc_card ymu831_snd_card = {
	.name = "adonisduos-YMU831",
	.dai_link = adonisduos_dai,
	.num_links = ARRAY_SIZE(adonisduos_dai),

	.late_probe = adonisduos_late_probe,
};

static int __devinit snd_adonisduos_probe(struct platform_device *pdev)
{
	int ret;
	struct ymu831_machine_priv *ymu831;

	ymu831 = kzalloc(sizeof(struct ymu831_machine_priv), GFP_KERNEL);
	if (!ymu831) {
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	exynos5_audio_set_mclk(1, 0);

	snd_soc_card_set_drvdata(&ymu831_snd_card, ymu831);

	ymu831_snd_card.dev = &pdev->dev;
	ret = snd_soc_register_card(&ymu831_snd_card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		kfree(ymu831);
	}

	return ret;
}

static int __devexit snd_adonisduos_remove(struct platform_device *pdev)
{
	struct ymu831_machine_priv *ymu831;

	ymu831 = snd_soc_card_get_drvdata(&ymu831_snd_card);

	snd_soc_unregister_card(&ymu831_snd_card);
	kfree(ymu831);

	return 0;
}

static struct platform_driver snd_adonisduos_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ymu831-card",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_adonisduos_probe,
	.remove = __devexit_p(snd_adonisduos_remove),
};

module_platform_driver(snd_adonisduos_driver);

MODULE_AUTHOR("JS. Park <aitdark.park@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC adonisduos YMU831");
MODULE_LICENSE("GPL");
