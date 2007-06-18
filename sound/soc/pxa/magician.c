/*
 * SoC audio for HTC Magician
 *
 * Copyright (c) 2006 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * based on spitz.c,
 * Authors: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *          Richard Purdie <richard@openedhand.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/hardware/scoop.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/magician.h>
#include <asm/arch/magician_cpld.h>
#include <asm/mach-types.h>
#include "../codecs/uda1380.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-i2s.h"
#include "pxa2xx-ssp.h"

#define MAGICIAN_HP_ON     0
#define MAGICIAN_HP_OFF    1

#define MAGICIAN_SPK_ON    0
#define MAGICIAN_SPK_OFF   1

#define MAGICIAN_MIC       0
#define MAGICIAN_MIC_EXT   1

/*
 * SSP GPIO's
 */
#define GPIO23_SSPSCLK_MD	(23 | GPIO_ALT_FN_2_OUT)
#define GPIO24_SSPSFRM_MD	(24 | GPIO_ALT_FN_2_OUT)
#define GPIO25_SSPTXD_MD	(25 | GPIO_ALT_FN_2_OUT)

static int magician_hp_func = MAGICIAN_HP_OFF;
static int magician_spk_func = MAGICIAN_SPK_ON;
static int magician_in_sel = MAGICIAN_MIC;

extern struct platform_device magician_cpld;

static void magician_ext_control(struct snd_soc_codec *codec)
{
	snd_soc_dapm_set_endpoint(codec, "Speaker",
			(magician_spk_func == MAGICIAN_SPK_ON));

	snd_soc_dapm_set_endpoint(codec, "Headphone Jack",
			(magician_hp_func == MAGICIAN_HP_ON));

	switch (magician_in_sel) {
	case MAGICIAN_MIC:
		snd_soc_dapm_set_endpoint(codec, "Headset Mic", 0);
		snd_soc_dapm_set_endpoint(codec, "Call Mic", 1);
		break;
	case MAGICIAN_MIC_EXT:
		snd_soc_dapm_set_endpoint(codec, "Call Mic", 0);
		snd_soc_dapm_set_endpoint(codec, "Headset Mic", 1);
		break;
	}
	snd_soc_dapm_sync_endpoints(codec);
}

static int magician_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	/* check the jack status at stream startup */
	magician_ext_control(codec);

	return 0;
}

/*
 * Magician uses SSP port for playback.
 */
static int magician_playback_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int acps, acds, div4;
	int ret = 0;

	/*
	 * Rate = SSPSCLK / (word size(16))
	 * SSPSCLK = (ACPS / ACDS) / SSPSCLKDIV(div4 or div1)
	 */
	switch (params_rate(params)) {
	case 8000:
		acps = 32842000;
		acds = PXA2XX_SSP_CLK_AUDIO_DIV_32;	/* wrong - 32 bits/sample */
		div4 = PXA2XX_SSP_CLK_SCDB_4;
		break;
	case 11025:
		acps = 5622000;
		acds = PXA2XX_SSP_CLK_AUDIO_DIV_8;	/* 16 bits/sample, 1 slot */
		div4 = PXA2XX_SSP_CLK_SCDB_4;
		break;
	case 22050:
		acps = 5622000;
		acds = PXA2XX_SSP_CLK_AUDIO_DIV_4;
		div4 = PXA2XX_SSP_CLK_SCDB_4;
		break;
	case 44100:
		acps = 11345000;
		acds = PXA2XX_SSP_CLK_AUDIO_DIV_4;
		div4 = PXA2XX_SSP_CLK_SCDB_4;
		break;
	case 48000:
		acps = 12235000;
		acds = PXA2XX_SSP_CLK_AUDIO_DIV_4;
		div4 = PXA2XX_SSP_CLK_SCDB_4;
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_MSB |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_MSB |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set audio clock as clock source */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_SSP_CLK_AUDIO, 0,
			SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	/* set the SSP audio system clock ACDS divider */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai,
			PXA2XX_SSP_AUDIO_DIV_ACDS, acds);
	if (ret < 0)
		return ret;

	/* set the SSP audio system clock SCDB divider4 */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai,
			PXA2XX_SSP_AUDIO_DIV_SCDB, div4);
	if (ret < 0)
		return ret;

	/* set SSP audio pll clock */
	ret = cpu_dai->dai_ops.set_pll(cpu_dai, 0, 0, acps);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * We have to enable the SSP port early so the UDA1380 can flush
 * it's register cache. The UDA1380 can only write it's interpolator and
 * decimator registers when the link is running.
 */
static int magician_playback_prepare(struct snd_pcm_substream *substream)
{
	/* enable SSP clock - is this needed ? */
	SSCR0_P(1) |= SSCR0_SSE;

	/* FIXME: ENABLE I2S */
	SACR0 |= SACR0_BCKD;
	SACR0 |= SACR0_ENB;
	pxa_set_cken(CKEN8_I2S, 1);

	return 0;
}

static int magician_playback_hw_free(struct snd_pcm_substream *substream)
{
	/* FIXME: DISABLE I2S */
	SACR0 &= ~SACR0_ENB;
	SACR0 &= ~SACR0_BCKD;
	pxa_set_cken(CKEN8_I2S, 0);
	return 0;
}

/*
 * Magician uses I2S for capture.
 */
static int magician_capture_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai,
			SND_SOC_DAIFMT_MSB | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai,
			SND_SOC_DAIFMT_MSB | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as output */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_I2S_SYSCLK, 0,
			SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * We have to enable the I2S port early so the UDA1380 can flush
 * it's register cache. The UDA1380 can only write it's interpolator and
 * decimator registers when the link is running.
 */
static int magician_capture_prepare(struct snd_pcm_substream *substream)
{
	SACR0 |= SACR0_ENB;
	return 0;
}

static struct snd_soc_ops magician_capture_ops = {
	.startup = magician_startup,
	.hw_params = magician_capture_hw_params,
	.prepare = magician_capture_prepare,
};

static struct snd_soc_ops magician_playback_ops = {
	.startup = magician_startup,
	.hw_params = magician_playback_hw_params,
	.prepare = magician_playback_prepare,
	.hw_free = magician_playback_hw_free,
};

static int magician_get_jack(struct snd_kcontrol * kcontrol,
			     struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = magician_hp_func;
	return 0;
}

static int magician_set_hp(struct snd_kcontrol * kcontrol,
			     struct snd_ctl_elem_value * ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (magician_hp_func == ucontrol->value.integer.value[0])
		return 0;

	magician_hp_func = ucontrol->value.integer.value[0];
	magician_ext_control(codec);
	return 1;
}

static int magician_get_spk(struct snd_kcontrol * kcontrol,
			    struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = magician_spk_func;
	return 0;
}

static int magician_set_spk(struct snd_kcontrol * kcontrol,
			    struct snd_ctl_elem_value * ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (magician_spk_func == ucontrol->value.integer.value[0])
		return 0;

	magician_spk_func = ucontrol->value.integer.value[0];
	magician_ext_control(codec);
	return 1;
}

static int magician_get_input(struct snd_kcontrol * kcontrol,
			      struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = magician_in_sel;
	return 0;
}

static int magician_set_input(struct snd_kcontrol * kcontrol,
			      struct snd_ctl_elem_value * ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (magician_in_sel == ucontrol->value.integer.value[0])
		return 0;

	magician_in_sel = ucontrol->value.integer.value[0];

	switch (magician_in_sel) {
	case MAGICIAN_MIC:
		magician_egpio_disable(&magician_cpld,
				       EGPIO_NR_MAGICIAN_IN_SEL0);
		magician_egpio_enable(&magician_cpld,
				      EGPIO_NR_MAGICIAN_IN_SEL1);
		break;
	case MAGICIAN_MIC_EXT:
		magician_egpio_disable(&magician_cpld,
				       EGPIO_NR_MAGICIAN_IN_SEL0);
		magician_egpio_disable(&magician_cpld,
				       EGPIO_NR_MAGICIAN_IN_SEL1);
	}

	return 1;
}

static int magician_spk_power(struct snd_soc_dapm_widget *w, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		magician_egpio_enable(&magician_cpld,
				      EGPIO_NR_MAGICIAN_SPK_POWER);
	else
		magician_egpio_disable(&magician_cpld,
				       EGPIO_NR_MAGICIAN_SPK_POWER);
	return 0;
}

static int magician_hp_power(struct snd_soc_dapm_widget *w, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		magician_egpio_enable(&magician_cpld,
				      EGPIO_NR_MAGICIAN_EP_POWER);
	else
		magician_egpio_disable(&magician_cpld,
				       EGPIO_NR_MAGICIAN_EP_POWER);
	return 0;
}

static int magician_mic_bias(struct snd_soc_dapm_widget *w, int event)
{
//	if (SND_SOC_DAPM_EVENT_ON(event))
//		magician_egpio_enable(&magician_cpld,
//			EGPIO_NR_MAGICIAN_MIC_POWER);
//	else
//		magician_egpio_disable(&magician_cpld,
//			EGPIO_NR_MAGICIAN_MIC_POWER);
	return 0;
}

/* magician machine dapm widgets */
static const struct snd_soc_dapm_widget uda1380_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", magician_hp_power),
	SND_SOC_DAPM_SPK("Speaker", magician_spk_power),
	SND_SOC_DAPM_MIC("Call Mic", magician_mic_bias),
	SND_SOC_DAPM_MIC("Headset Mic", magician_mic_bias),
};

/* magician machine audio_map */
static const char *audio_map[][3] = {

	/* Headphone connected to VOUTL, VOUTR */
	{"Headphone Jack", NULL, "VOUTL"},
	{"Headphone Jack", NULL, "VOUTR"},

	/* Speaker connected to VOUTL, VOUTR */
	{"Speaker", NULL, "VOUTL"},
	{"Speaker", NULL, "VOUTR"},

	/* Mics are connected to VINM */
	{"VINM", NULL, "Headset Mic"},
	{"VINM", NULL, "Call Mic"},

	{NULL, NULL, NULL},
};

static const char *hp_function[] = { "On", "Off" };
static const char *spk_function[] = { "On", "Off" };
static const char *input_select[] = { "Call Mic", "Headset Mic" };
static const struct soc_enum magician_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, hp_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, input_select),
};

static const struct snd_kcontrol_new uda1380_magician_controls[] = {
	SOC_ENUM_EXT("Headphone Switch", magician_enum[0], magician_get_jack,
			magician_set_hp),
	SOC_ENUM_EXT("Speaker Switch", magician_enum[1], magician_get_spk,
			magician_set_spk),
	SOC_ENUM_EXT("Input Select", magician_enum[2], magician_get_input,
			magician_set_input),
};

/*
 * Logic for a uda1380 as connected on a HTC Magician
 */
static int magician_uda1380_init(struct snd_soc_codec *codec)
{
	int i, err;

	/* NC codec pins */
	snd_soc_dapm_set_endpoint(codec, "VOUTLHP", 0);
	snd_soc_dapm_set_endpoint(codec, "VOUTRHP", 0);

	/* FIXME: is anything connected here? */
	snd_soc_dapm_set_endpoint(codec, "VINL", 0);
	snd_soc_dapm_set_endpoint(codec, "VINR", 0);

	/* Add magician specific controls */
	for (i = 0; i < ARRAY_SIZE(uda1380_magician_controls); i++) {
		if ((err = snd_ctl_add(codec->card,
				snd_soc_cnew(&uda1380_magician_controls[i],
				codec, NULL))) < 0)
			return err;
	}

	/* Add magician specific widgets */
	for (i = 0; i < ARRAY_SIZE(uda1380_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &uda1380_dapm_widgets[i]);
	}

	/* Set up magician specific audio path interconnects */
	for (i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map[i][0],
				audio_map[i][1], audio_map[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

/* magician digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link magician_dai[] = {
{
	.name = "uda1380",
	.stream_name = "UDA1380 Playback",
	.cpu_dai = &pxa_ssp_dai[0],
	.codec_dai = &uda1380_dai[UDA1380_DAI_PLAYBACK],
	.init = magician_uda1380_init,
	.ops = &magician_playback_ops,
},
{
	.name = "uda1380",
	.stream_name = "UDA1380 Capture",
	.cpu_dai = &pxa_i2s_dai,
	.codec_dai = &uda1380_dai[UDA1380_DAI_CAPTURE],
	.ops = &magician_capture_ops,
}
};

/* magician audio machine driver */
static struct snd_soc_machine snd_soc_machine_magician = {
	.name = "Magician",
	.dai_link = magician_dai,
	.num_links = ARRAY_SIZE(magician_dai),
};

/* magician audio private data */
static struct uda1380_setup_data magician_uda1380_setup = {
	.i2c_address = 0x18,
	.dac_clk = UDA1380_DAC_CLK_WSPLL,
};

/* magician audio subsystem */
static struct snd_soc_device magician_snd_devdata = {
	.machine = &snd_soc_machine_magician,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_uda1380,
	.codec_data = &magician_uda1380_setup,
};

static struct platform_device *magician_snd_device;

static int __init magician_init(void)
{
	int ret;

	if (!machine_is_magician())
		return -ENODEV;

	magician_egpio_enable(&magician_cpld, EGPIO_NR_MAGICIAN_CODEC_POWER);

	/* we may need to have the clock running here - pH5 */
	magician_egpio_enable(&magician_cpld, EGPIO_NR_MAGICIAN_CODEC_RESET);
	udelay(5);
	magician_egpio_disable(&magician_cpld, EGPIO_NR_MAGICIAN_CODEC_RESET);

	/* correct place? we'll need it to talk to the uda1380 */
	request_module("i2c-pxa");

	magician_snd_device = platform_device_alloc("soc-audio", -1);
	if (!magician_snd_device)
		return -ENOMEM;

	platform_set_drvdata(magician_snd_device, &magician_snd_devdata);
	magician_snd_devdata.dev = &magician_snd_device->dev;
	ret = platform_device_add(magician_snd_device);

	if (ret)
		platform_device_put(magician_snd_device);

	pxa_gpio_mode(GPIO23_SSPSCLK_MD);
	pxa_gpio_mode(GPIO24_SSPSFRM_MD);
	pxa_gpio_mode(GPIO25_SSPTXD_MD);

	return ret;
}

static void __exit magician_exit(void)
{
	platform_device_unregister(magician_snd_device);

	magician_egpio_disable(&magician_cpld, EGPIO_NR_MAGICIAN_SPK_POWER);
	magician_egpio_disable(&magician_cpld, EGPIO_NR_MAGICIAN_EP_POWER);
	magician_egpio_disable(&magician_cpld, EGPIO_NR_MAGICIAN_MIC_POWER);
	magician_egpio_disable(&magician_cpld, EGPIO_NR_MAGICIAN_CODEC_POWER);
}

module_init(magician_init);
module_exit(magician_exit);

MODULE_AUTHOR("Philipp Zabel");
MODULE_DESCRIPTION("ALSA SoC Magician");
MODULE_LICENSE("GPL");
