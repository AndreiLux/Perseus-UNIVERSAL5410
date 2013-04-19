/*
 *  adonisuniv_ymu831.c
 *
 *  Copyright (c) 2009 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or  modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <mach/regs-clock.h>
#include "../codecs/ymu831/ymu831.h"
#include "../codecs/ymu831/ymu831_priv.h"

struct ymu831_machine_priv {

	struct clk *epll;
	struct clk *clkout;
	struct snd_soc_codec *codec;
};

static int adonisuniv_hifi_hw_params(struct snd_pcm_substream *substream,
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

/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;

static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct mc_asoc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct mc_asoc_jack *jack = &priv->data.jack;
	int report = 0;
	int status = jack->hs_jack->status;


	if ((status & SND_JACK_HEADPHONE) ||
		(status & SND_JACK_HEADSET)) {
		report = 1;
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct mc_asoc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct mc_asoc_jack *jack = &priv->data.jack;
	int report = 0;
	int status = jack->hs_jack->status;


	if (status & SND_JACK_BTN_0)
		report = 1;

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct mc_asoc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct mc_asoc_jack *jack = &priv->data.jack;


	if ((!size) || (buf[0] != '1')) {
		snd_soc_jack_report(jack->hs_jack,
				    0, SND_JACK_HEADSET);
#ifdef CONFIG_SWITCH
		switch_set_state(jack->h2w_sdev, 0);
#endif
		pr_info("Forced remove microphone\n");
	} else {

		snd_soc_jack_report(jack->hs_jack,
				    SND_JACK_HEADSET, SND_JACK_HEADSET);

#ifdef CONFIG_SWITCH
		switch_set_state(jack->h2w_sdev, 1);
#endif
		pr_info("Forced detect microphone\n");
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);


static int adonisuniv_hifiaudio_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;

	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(jack_class, NULL, 0, codec, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);

	return 0;
}

static int adonisuniv_suspend_post(struct snd_soc_card *card)
{
	struct ymu831_machine_priv *ymu831;

	ymu831 = snd_soc_card_get_drvdata(card);

	clk_disable(ymu831->clkout);

	return 0;
}

static int adonisuniv_resume_pre(struct snd_soc_card *card)
{
	struct ymu831_machine_priv *ymu831;
	int ret;

	ymu831 = snd_soc_card_get_drvdata(card);

	ret = clk_enable(ymu831->clkout);

	return ret;
}

/*
 * Adonisuniv YMU831 DAI operations.
 */
static struct snd_soc_ops adonisuniv_hifi_ops = {
	.hw_params = adonisuniv_hifi_hw_params,
};

static struct snd_soc_dai_link adonisuniv_dai[] = {
	{ /* Primary DAI i/f */
		.name = "YMU831 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "ymu831-da0",
		.platform_name = "samsung-audio",

		.codec_name = "spi2.0", /* SPI bus:2, chipselect:0 */
		.init = adonisuniv_hifiaudio_init,
		.ops = &adonisuniv_hifi_ops,
	},
        { /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "ymu831-da0",
#ifndef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-audio",
#else
		.platform_name = "samsung-idma",
#endif
		.codec_name = "spi2.0",
		.ops = &adonisuniv_hifi_ops,
	},
};

static struct snd_soc_card ymu831_snd_card = {
	.name = "AdonisUniv-YMU831",
	.dai_link = adonisuniv_dai,
	.num_links = ARRAY_SIZE(adonisuniv_dai),
        .suspend_post = adonisuniv_suspend_post,
        .resume_pre = adonisuniv_resume_pre,
};

static struct platform_device *adonisuniv_snd_device;

static int __init adonisuniv_audio_init(void)
{
	int ret;
	struct ymu831_machine_priv *ymu831;

	ymu831 = kzalloc(sizeof(struct ymu831_machine_priv), GFP_KERNEL);
	if (!ymu831) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	ymu831->epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(ymu831->epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		ret = PTR_ERR(ymu831->epll);
		goto err_clk_epll_get;
	}

	ymu831->clkout = clk_get(NULL, "clkout");
	if (IS_ERR(ymu831->clkout)) {
		pr_err("failed to get clkout\n");
		ret = PTR_ERR(ymu831->clkout);
		goto err_clk_get;
	}

	clk_enable(ymu831->clkout);

	snd_soc_card_set_drvdata(&ymu831_snd_card, ymu831);

	adonisuniv_snd_device = platform_device_alloc("soc-audio", -1);
	if (!adonisuniv_snd_device)
		goto err_device_alloc;

	platform_set_drvdata(adonisuniv_snd_device, &ymu831_snd_card);

	ret = platform_device_add(adonisuniv_snd_device);
	if (ret)
		goto err_device_add;

	return ret;

err_device_add:
	platform_device_put(adonisuniv_snd_device);
err_device_alloc:
	clk_disable(ymu831->clkout);
	clk_put(ymu831->clkout);
err_clk_get:
	clk_put(ymu831->epll);
err_clk_epll_get:
	kfree(ymu831);
err_kzalloc:
	return ret;
}

module_init(adonisuniv_audio_init);

static void __exit adonisuniv_audio_exit(void)
{
	struct ymu831_machine_priv *ymu831;

	ymu831 = snd_soc_card_get_drvdata(&ymu831_snd_card);

	platform_device_unregister(adonisuniv_snd_device);

	clk_put(ymu831->clkout);
	clk_put(ymu831->epll);

	kfree(ymu831);
}

module_exit(adonisuniv_audio_exit);

MODULE_AUTHOR("JS. Park <aitdark.park@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC ADONISUNIV YMU831");
MODULE_LICENSE("GPL");
