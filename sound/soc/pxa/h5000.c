/*
 *
 * Copyright (c) 2007 Milan Plzik <mmp@handhelds.org>
 *
 * based on spitz.c,
 * Authors: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *          Richard Purdie <richard@openedhand.com>
 *
 * This code is released under GPL (GNU Public License) with
 * absolutely no warranty. Please see http://www.gnu.org/ for a
 * complete discussion of the GPL.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <asm-arm/arch/gpio.h>
#include <asm/arch-pxa/h5400-asic.h>
#include <asm/arch-pxa/h5400-gpio.h>
#include <asm/hardware/samcop_base.h>

#include "pxa2xx-i2s.h"
#include "pxa2xx-pcm.h"
#include "../codecs/ak4535.h"


#define H5000_OFF	0
#define H5000_HP	1
#define H5000_MIC	2

#define H5000_SPK_OFF	0
#define H5000_SPK_ON	1

static int h5000_spk_func = 0;
static int h5000_jack_func = 0;


static void h5000_ext_control(struct snd_soc_codec *codec)
{
	switch (h5000_spk_func) {
	case H5000_SPK_OFF:
		snd_soc_dapm_set_endpoint(codec, "Ext Spk", 0);
		break;
	case H5000_SPK_ON:
		snd_soc_dapm_set_endpoint(codec, "Ext Spk", 1);
		break;
	default:
		printk (KERN_ERR "%s: invalid value %d for h5000_spk_func\n", 
			__FUNCTION__, h5000_spk_func);
		break;
	};

	switch (h5000_jack_func) {
	case H5000_OFF:
		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
		snd_soc_dapm_set_endpoint(codec, "Internal Mic", 1);
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
		break;
	case H5000_HP:
		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 1);
		snd_soc_dapm_set_endpoint(codec, "Internal Mic", 1);
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 0);
		break;
	case H5000_MIC:
		snd_soc_dapm_set_endpoint(codec, "Headphone Jack", 0);
		snd_soc_dapm_set_endpoint(codec, "Internal Mic", 0);
		snd_soc_dapm_set_endpoint(codec, "Mic Jack", 1);
		break;
	default:
		printk(KERN_ERR "%s: invalid value %d for h5000_jack_func\n", 
			__FUNCTION__, h5000_jack_func);
		break;
	};

	snd_soc_dapm_sync_endpoints(codec);
};

static int h5000_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	h5000_ext_control(codec);
	return 0;
};

static void h5000_shutdown(struct snd_pcm_substream *substream)
{
};

static int h5000_hw_params(struct snd_pcm_substream *substream, 
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
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, 0, clk,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, PXA2XX_I2S_SYSCLK, 0,
		SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	return 0;
};

static struct snd_soc_ops h5000_ops = {
	.startup = h5000_startup,
	.shutdown = h5000_shutdown,
	.hw_params = h5000_hw_params,
};

static int h5000_get_jack(struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol) 
{
	ucontrol->value.integer.value [0] = h5000_jack_func;
	return 0;
};

static int h5000_set_jack(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	
	if (h5000_jack_func == ucontrol->value.integer.value [0])
		return 0;
	
	h5000_jack_func = ucontrol->value.integer.value [0];
	h5000_ext_control(codec);
	return 1;
};

static int h5000_get_spk(struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol) 
{	
	ucontrol->value.integer.value [0] = h5000_spk_func;
	return 0;
};

static int h5000_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip (kcontrol);
	
	if (h5000_spk_func == ucontrol->value.integer.value [0])
		return 0;
	
	h5000_spk_func = ucontrol->value.integer.value [0];
	h5000_ext_control(codec);
	return 1;
};

static int h5000_audio_power(struct snd_soc_dapm_widget *widget, int event) 
{
	// mp - why do we need the ref count, dapm core should ref count all widget use.
	static int power_use_count = 0;
	
	if (SND_SOC_DAPM_EVENT_ON (event))
		power_use_count ++;
	else
		power_use_count --;
	
	samcop_set_gpio_b (&h5400_samcop.dev, SAMCOP_GPIO_GPB_AUDIO_POWER_ON, 
		(power_use_count > 0) ? SAMCOP_GPIO_GPB_AUDIO_POWER_ON : 0 );

	return 0;
};

static const struct snd_soc_dapm_widget ak4535_dapm_widgets[] = {
	SND_SOC_DAPM_SPK ("Ext Spk", h5000_audio_power),
	SND_SOC_DAPM_HP ("Headphone Jack", h5000_audio_power),
	SND_SOC_DAPM_MIC ("Internal Mic", h5000_audio_power),
};

/* I'm really not sure about this, please fix if neccessary */
static const char *audio_map [][3] = {
	/* Speaker is connected to speaker +- pins */
	{ "Ext Spk", NULL, "SPP" },
	{ "Ext Spk", NULL, "SPN" },

	/* Mono input to MOUT2 according to reference */
	{ "MIN", NULL, "MOUT2" },

	/* Headphone output is connected to left and right output */
	{ "Headphone Jack", NULL, "HPL" },
	{ "Headphone Jack", NULL, "HPR" },

	/* MICOUT is connected to AIN */
	{ "AIN", NULL, "MICOUT"},

	/* Microphones */
	{ "MICIN", NULL, "Internal Mic" },
	{ "MICEXT", NULL, "Mic Jack" },

	{ NULL, NULL, NULL },
};

static const char *jack_function [] = { "Off", "Headphone", "Mic", };
static const char *spk_function [] = { "Off", "On", };

static const struct soc_enum h5000_enum [] = {
	SOC_ENUM_SINGLE_EXT (2, jack_function),
	SOC_ENUM_SINGLE_EXT (2, spk_function),
};

static const struct snd_kcontrol_new ak4535_h5000_controls[] = {
	SOC_ENUM_EXT ("Jack Function", h5000_enum[0], h5000_get_jack, h5000_set_jack),
	SOC_ENUM_EXT ("Speaker Function", h5000_enum[1], h5000_get_spk, h5000_set_spk),

};

static int h5000_ak4535_init (struct snd_soc_codec *codec)
{
	int i, err;
	
	/* NC codec pins */
	snd_soc_dapm_set_endpoint(codec, "MOUT1", 0);
	snd_soc_dapm_set_endpoint(codec, "LOUT", 0);
	snd_soc_dapm_set_endpoint(codec, "ROUT", 0);
	
	// mp - not sure I understand here, is the codec driver wrong ?
	snd_soc_dapm_set_endpoint(codec, "MOUT2", 0);	/* FIXME: These pins are marked as INPUTS */
	snd_soc_dapm_set_endpoint(codec, "MIN", 0);	/* FIXME: and OUTPUTS in ak4535.c . We need to do this in order */
	snd_soc_dapm_set_endpoint(codec, "AIN", 0);	/* FIXME: to get DAPM working properly, because the pins are connected */
	snd_soc_dapm_set_endpoint(codec, "MICOUT", 0);	/* FIXME: OUTPUT -> INPUT. */

	/* Add h5000 specific controls */
	for (i = 0; i < ARRAY_SIZE (ak4535_h5000_controls); i++) {
		err = snd_ctl_add (codec->card,
			snd_soc_cnew(&ak4535_h5000_controls[i], codec, NULL));
		if (err < 0)
			return err;
	};
		
	/* Add h5000 specific widgets */
	for (i = 0; i < ARRAY_SIZE (ak4535_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &ak4535_dapm_widgets [i]);
	};

	/* Set up h5000 specific audio path audio_map */
	for (i = 0; audio_map [i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, audio_map [i][0], 
			audio_map [i][1], audio_map [i][2]);
	};

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
};


static struct snd_soc_dai_link h5000_dai = {
	.name = "ak4535",
	.stream_name = "AK4535",
	.cpu_dai = &pxa_i2s_dai,
	.codec_dai = &ak4535_dai,
	.init = h5000_ak4535_init,
	.ops = &h5000_ops,
};

static struct snd_soc_machine snd_soc_machine_h5000 = {
	.name = "h5000",
	.dai_link = &h5000_dai,
	.num_links = 1,
};

static struct ak4535_setup_data h5000_codec_setup = {
	.i2c_address = 0x10,
};

static struct snd_soc_device h5000_snd_devdata = {
	.machine = &snd_soc_machine_h5000,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_ak4535,
	.codec_data = &h5000_codec_setup,
};

static struct platform_device *h5000_snd_device;

static int __init h5000_init(void)
{
	int ret;

	if (!machine_is_h5400 ())
		return -ENODEV;

	request_module("i2c-pxa");

	h5000_snd_device = platform_device_alloc("soc-audio", -1);
	if (!h5000_snd_device)
		return -ENOMEM;
	
	/* enable audio codec */
	samcop_set_gpio_b(&h5400_samcop.dev, 
		SAMCOP_GPIO_GPB_CODEC_POWER_ON, SAMCOP_GPIO_GPB_CODEC_POWER_ON);

	platform_set_drvdata(h5000_snd_device, &h5000_snd_devdata);

	h5000_snd_devdata.dev = &h5000_snd_device->dev;
	ret = platform_device_add(h5000_snd_device);
	if (ret)
		platform_device_put(h5000_snd_device);

	return ret;
	
};

static void __exit h5000_exit(void)
{
	platform_device_unregister(h5000_snd_device);
	
	samcop_set_gpio_b(&h5400_samcop.dev, 
		SAMCOP_GPIO_GPB_CODEC_POWER_ON | SAMCOP_GPIO_GPB_AUDIO_POWER_ON, 0);
};

module_init (h5000_init);
module_exit (h5000_exit);

MODULE_AUTHOR ("Milan Plzik");
MODULE_DESCRIPTION ("ALSA SoC iPAQ h5000");
MODULE_LICENSE ("GPL");
