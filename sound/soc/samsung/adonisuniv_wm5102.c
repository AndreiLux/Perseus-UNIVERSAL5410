/*
 *  adonisuniv_wm5102.c
 *
 *  Copyright (c) 2012 Samsung Electronics Co. Ltd
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
#include <linux/regmap.h>

#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/gpio.h>
#include <mach/gpio-exynos.h>
#include <mach/exynos5-audio.h>
#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/core.h>
#include <sound/tlv.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/wm5102.h"

#define USE_BIAS_LEVEL_POST

#define ADONISUNIV_DEFAULT_MCLK1	24000000
#define ADONISUNIV_DEFAULT_MCLK2	32768

#define ADONISUNIV_TELCLK_RATE		(48000 * 512)

#define CLK_MODE_MEDIA 0
#define CLK_MODE_TELEPHONY 1

static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);

struct wm5102_machine_priv {
	int clock_mode;
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct snd_soc_dai *aif[3];
	struct delayed_work mic_work;
	struct wake_lock jackdet_wake_lock;
	int aif2mode;

	int aif1rate;
	unsigned int hp_impedance_step;

};
static int lhpf1_coeff;
static int lhpf2_coeff;

static unsigned int lhpf_filter_values[] = {
	0xF03A, 0xF04B, 0xF099, 0x0000, 0x0000
};

const char *lhpf_filter_text[] = {
	"64Hz", "130Hz", "267Hz", "user defined1", "user defined2"
};

const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static const struct soc_enum lhpf_filter_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lhpf_filter_text), lhpf_filter_text),
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static struct {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
} hp_gain_table[] = {
	{    0,      42, 0 },
	{   43,     100, 2 },
	{  101,     200, 4 },
	{  201,     450, 6 },
	{  451,    1000, 8 },
	{ 1001, INT_MAX, 0 },
};

static struct snd_soc_codec *the_codec;

void adonisuniv_wm5102_hpdet_cb(unsigned int meas)
{
	int i;
	struct wm5102_machine_priv *priv;

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = snd_soc_card_get_drvdata(the_codec->card);

	for (i = 0; i < ARRAY_SIZE(hp_gain_table); i++) {
		if (meas < hp_gain_table[i].min || meas > hp_gain_table[i].max)
			continue;

		dev_crit(the_codec->dev, "SET GAIN %d step for %d ohms\n",
			 hp_gain_table[i].gain, meas);
		priv->hp_impedance_step = hp_gain_table[i].gain;
	}
}

static int wm5102_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	val += priv->hp_impedance_step;
	dev_crit(codec->dev, "SET GAIN %d according to impedance, moved %d step\n",
			 val, priv->hp_impedance_step);

	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;

	err = snd_soc_update_bits_locked(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	return err;
}

static int adonisuniv_start_sysclk(struct snd_soc_card *card)
{
	struct wm5102_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	ret = snd_soc_codec_set_pll(priv->codec, WM5102_FLL1,
				     ARIZONA_CLK_SRC_MCLK1,
				    ADONISUNIV_DEFAULT_MCLK1,
				    priv->aif1rate * 512);
	if (ret != 0) {
		dev_err(priv->codec->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}

	return ret;
}

static int adonisuniv_stop_sysclk(struct snd_soc_card *card)
{
	struct wm5102_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	ret = snd_soc_codec_set_pll(priv->codec, WM5102_FLL1, 0, 0, 0);
	if (ret != 0) {
		dev_err(priv->codec->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(priv->codec, ARIZONA_CLK_SYSCLK, 0,
				0, 0);
	if (ret != 0) {
		dev_err(priv->codec->dev, "Failed to stop SYSCLK: %d\n", ret);
		return ret;
	}

	return ret;
}

#ifdef USE_BIAS_LEVEL_POST
static int adonisuniv_set_bias_level(struct snd_soc_card *card,
				  struct snd_soc_dapm_context *dapm,
				  enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;

	if (dapm->dev != codec_dai->dev)
		return 0;

	dev_info(card->dev, "%s: %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level != SND_SOC_BIAS_STANDBY)
			break;

		adonisuniv_start_sysclk(card);
		break;
	default:
		break;
	}

	return 0;
}

static int adonisuniv_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;

	if (dapm->dev != codec_dai->dev)
		return 0;

	dev_info(card->dev, "%s: %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		adonisuniv_stop_sysclk(card);
		break;
	default:
		break;
	}

	dapm->bias_level = level;

	return 0;
}
#endif

int adonisuniv_set_media_clocking(struct wm5102_machine_priv *priv)
{
	struct snd_soc_codec *codec = priv->codec;
	int ret;
	int fs;

	if (priv->aif1rate >= 192000)
		fs = 256;
	else
		fs = 512;

	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
				    ADONISUNIV_DEFAULT_MCLK1,
				    priv->aif1rate * fs);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       priv->aif1rate * fs,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       ADONISUNIV_TELCLK_RATE,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev,
				 "Unable to set ASYNCCLK to FLL2: %d\n", ret);

	/* AIF1 from SYSCLK, AIF2 and 3 from ASYNCCLK */
	ret = snd_soc_dai_set_sysclk(priv->aif[0], ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0)
		dev_err(codec->dev, "Can't set AIF1 to SYSCLK: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(priv->aif[1], ARIZONA_CLK_ASYNCCLK, 0, 0);
	if (ret < 0)
		dev_err(codec->dev, "Can't set AIF2 to ASYNCCLK: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(priv->aif[2], ARIZONA_CLK_ASYNCCLK, 0, 0);
	if (ret < 0)
		dev_err(codec->dev, "Can't set AIF3 to ASYNCCLK: %d\n", ret);

	return 0;
}

static void adonisuniv_gpio_init(void)
{
#ifdef GPIO_MICBIAS_EN
	int err;

	/* Main Microphone BIAS */
	err = gpio_request(GPIO_MICBIAS_EN, "MAINMIC_BIAS");
	if (err) {
		pr_err(KERN_ERR "MIC_BIAS_EN GPIO set error!\n");
		return;
	}
	gpio_direction_output(GPIO_MICBIAS_EN, 1);

	/*This is tempary code to enable for main mic.(force enable GPIO) */
	gpio_set_value(GPIO_MICBIAS_EN, 0);
#endif
}

/*
 * AdnoisUniv wm5102 GPIO enable configure.
 */
static int adonisuniv_ext_mainmicbias(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,  int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct snd_soc_codec *codec = card->rtd[0].codec;

#ifdef GPIO_MICBIAS_EN
	switch (event) {

	case SND_SOC_DAPM_PRE_PMU:
		gpio_set_value(GPIO_MICBIAS_EN,  1);
		break;

	case SND_SOC_DAPM_POST_PMD:
		gpio_set_value(GPIO_MICBIAS_EN,  0);
		break;
	}

	dev_dbg(codec->dev, "Main Mic BIAS: %d\n", event);
#else
	dev_err(codec->dev, "Noting to do for Main Mic BIAS\n");
#endif

	return 0;
}

static int get_lhpf1_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = lhpf1_coeff;
	return 0;
}

static int set_lhpf1_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct regmap *regmap = codec->control_data;

	lhpf1_coeff = ucontrol->value.integer.value[0];

	dev_info(codec->dev, "%s: lhpf1 mode=%d, val=0x%x\n", __func__,
				lhpf1_coeff, lhpf_filter_values[lhpf1_coeff]);

	regmap_update_bits(regmap, ARIZONA_HPLPF1_2, ARIZONA_LHPF1_COEFF_MASK,
			lhpf_filter_values[lhpf1_coeff]);
	return 0;
}

static int get_lhpf2_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = lhpf2_coeff;
	return 0;
}

static int set_lhpf2_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct regmap *regmap = codec->control_data;

	lhpf2_coeff = ucontrol->value.integer.value[0];

	dev_info(codec->dev, "%s: lhpf2 mode=%d, val=0x%x (%p)\n", __func__,
			lhpf2_coeff, lhpf_filter_values[lhpf2_coeff], codec);

	regmap_update_bits(regmap, ARIZONA_HPLPF2_2, ARIZONA_LHPF2_COEFF_MASK,
			lhpf_filter_values[lhpf2_coeff]);
	return 0;
}

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);

	ucontrol->value.integer.value[0] = priv->aif2mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);

	priv->aif2mode = ucontrol->value.integer.value[0];

	dev_info(codec->dev, "set aif2 mode: %s\n",
					 aif2_mode_text[priv->aif2mode]);
	return  0;
}

static const struct snd_kcontrol_new adonisuniv_codec_controls[] = {
	SOC_ENUM_EXT("LHPF1 COEFF FILTER", lhpf_filter_mode_enum[0],
		get_lhpf1_coeff, set_lhpf1_coeff),

	SOC_ENUM_EXT("LHPF2 COEFF FILTER", lhpf_filter_mode_enum[0],
		get_lhpf2_coeff, set_lhpf2_coeff),

	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),

	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, wm5102_put_impedance_volsw,
		digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1R,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, wm5102_put_impedance_volsw,
		digital_tlv),
};

static const struct snd_kcontrol_new adonisuniv_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("VPS"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("3rd Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget adonisuniv_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HDMIL"),
	SND_SOC_DAPM_OUTPUT("HDMIR"),
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("VPS", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", adonisuniv_ext_mainmicbias),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("3rd Mic", NULL),
};

const struct snd_soc_dapm_route adonisuniv_dapm_routes[] = {
	{ "HDMIL", NULL, "AIF1RX1" },
	{ "HDMIR", NULL, "AIF1RX2" },
	{ "HDMI", NULL, "HDMIL" },
	{ "HDMI", NULL, "HDMIR" },

	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },

	{ "VPS", NULL, "HPOUT2L" },
	{ "VPS", NULL, "HPOUT2R" },

	{ "RCV", NULL, "EPOUTN" },
	{ "RCV", NULL, "EPOUTP" },

	{ "IN1L", NULL, "Main Mic" },
	{ "Main Mic", NULL, "MICVDD" },

	{ "Headset Mic", NULL, "MICBIAS1" },
	{ "IN1R", NULL, "Headset Mic" },

	{ "Sub Mic", NULL, "MICBIAS2" },
	{ "IN2L", NULL, "Sub Mic" },

	{ "3rd Mic", NULL, "MICBIAS3" },
	{ "IN2R", NULL, "3rd Mic" },
};

static int adonisuniv_wm5102_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct wm5102_machine_priv *priv =
					snd_soc_card_get_drvdata(codec->card);
	int ret;

	dev_info(codec_dai->dev, "aif1: %dch, %dHz, %dbytes\n",
						params_channels(params),
						params_rate(params),
						params_buffer_bytes(params));

	priv->aif1rate = params_rate(params);

	adonisuniv_set_media_clocking(priv);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Failed to set audio format in codec: %d\n", ret);
		return ret;
	}

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Failed to set audio format in cpu: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
				     0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Failed to set system clock in cpu: %d\n", ret);
		return ret;
	}

	return 0;
}

static int adonisuniv_wm5102_aif1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_info(card->dev, "%s\n", __func__);

	return 0;
}

/*
 * AdnoisUniv wm5102 DAI operations.
 */
static struct snd_soc_ops adonisuniv_wm5102_aif1_ops = {
	.hw_params = adonisuniv_wm5102_aif1_hw_params,
	.hw_free = adonisuniv_wm5102_aif1_hw_free,
};

static int adonisuniv_wm5102_aif2_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct wm5102_machine_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int ret;
	int prate, bclk;

	dev_info(codec_dai->dev, "aif2: %dch, %dHz, %dbytes\n",
						params_channels(params),
						params_rate(params),
						params_buffer_bytes(params));

	prate = params_rate(params);
	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	default:
		dev_warn(codec_dai->dev,
				"Unsupported LRCLK %d, falling back to 8000Hz\n",
				(int)params_rate(params));
		prate = 8000;
		bclk = 256000;
	}

	/* Set the codec DAI configuration, aif2_mode:0 is slave */

	if (priv->aif2mode == 0)
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	else
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"Failed to set audio format in codec: %d\n", ret);
		return ret;
	}

	if (priv->aif2mode  == 0) {
		ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2_REFCLK,
					  ARIZONA_FLL_SRC_MCLK1,
					  ADONISUNIV_DEFAULT_MCLK1,
					  ADONISUNIV_TELCLK_RATE);
		if (ret != 0) {
			dev_err(codec_dai->dev,
					"Failed to start FLL2 REF: %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2,
					  ARIZONA_FLL_SRC_AIF2BCLK,
					  bclk,
					  ADONISUNIV_TELCLK_RATE);
		if (ret != 0) {
			dev_err(codec_dai->dev,
					 "Failed to start FLL2%d\n", ret);
			return ret;
		}
	} else {
		ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2, 0, 0, 0);
		if (ret != 0)
			dev_err(codec_dai->dev,
					"Failed to stop FLL2: %d\n", ret);

		ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2_REFCLK,
					  ARIZONA_FLL_SRC_NONE, 0, 0);
		if (ret != 0) {
			dev_err(codec_dai->dev,
				 "Failed to start FLL2 REF: %d\n", ret);
			return ret;
		}
		ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2,
					  ARIZONA_CLK_SRC_MCLK1,
					  ADONISUNIV_DEFAULT_MCLK1,
					  ADONISUNIV_TELCLK_RATE);
		if (ret != 0) {
			dev_err(codec_dai->dev,
					"Failed to start FLL2: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static struct snd_soc_ops adonisuniv_wm5102_aif2_ops = {
	.hw_params = adonisuniv_wm5102_aif2_hw_params,
};

static int adonisuniv_wm5102_aif3_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	dev_info(codec_dai->dev, "aif3: %dch, %dHz, %dbytes\n",
						params_channels(params),
						params_rate(params),
						params_buffer_bytes(params));

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set BT mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct snd_soc_ops adonisuniv_wm5102_aif3_ops = {
	.hw_params = adonisuniv_wm5102_aif3_hw_params,
};

static int adonisuniv_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct wm5102_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);
	int i, ret;

	priv->codec = codec;
	the_codec = codec;

	for (i = 0; i < 3; i++)
		priv->aif[i] = card->rtd[i].codec_dai;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	snd_soc_dapm_ignore_suspend(&codec->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "VPS");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");

	adonisuniv_set_media_clocking(priv);

	ret = snd_soc_add_codec_controls(codec, adonisuniv_codec_controls,
					ARRAY_SIZE(adonisuniv_codec_controls));
	if (ret < 0) {
		dev_err(codec->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	dev_info(codec->dev, "%s: Successfully created\n", __func__);

	return snd_soc_dapm_sync(&codec->dapm);
}

static int adonisuniv_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	int ret;

	if (codec->active) {
		dev_info(codec->dev, "sound card is still active state");
		return 0;
	}

	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1,
				    ARIZONA_CLK_SRC_MCLK1,
				    ADONISUNIV_DEFAULT_MCLK1, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, WM5102_FLL2, 0, 0, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to stop FLL2: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK, 0, 0, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to stop SYSCLK: %d\n", ret);
		return ret;
	}

	exynos5_audio_set_mclk(0, 1);

	return 0;
}

static int adonisuniv_resume_pre(struct snd_soc_card *card)
{
	struct wm5102_machine_priv *wm5102_priv
					 = snd_soc_card_get_drvdata(card);

	exynos5_audio_set_mclk(1, 0);

	adonisuniv_set_media_clocking(wm5102_priv);

	return 0;
}

static struct snd_soc_dai_driver adonisuniv_ext_dai[] = {
	{
		.name = "adonisuniv.cp",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "adonisuniv.bt",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link adonisuniv_dai[] = {
	{
		.name = "AdonisUniv_WM5102 Playback",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "wm5102-aif1",
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-idma",
#else
		.platform_name = "samsung-audio",
#endif
		.codec_name = "wm5102-codec",
		.ops = &adonisuniv_wm5102_aif1_ops,
	},
	{
		.name = "AdonisUniv_WM5102 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "adonisuniv.cp",
		.codec_dai_name = "wm5102-aif2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm5102-codec",
		.ops = &adonisuniv_wm5102_aif2_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "AdonisUniv_WM5102 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "adonisuniv.bt",
		.codec_dai_name = "wm5102-aif3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm5102-codec",
		.ops = &adonisuniv_wm5102_aif3_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "AdonisUniv_WM5102 Multi Ch",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm5102-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm5102-codec",
		.ops = &adonisuniv_wm5102_aif1_ops,
	},
};

static struct snd_soc_card adonisuniv = {
	.name = "AdonisUniv Sound Card",
	.owner = THIS_MODULE,

	.dai_link = adonisuniv_dai,
	.num_links = ARRAY_SIZE(adonisuniv_dai),

	.controls = adonisuniv_controls,
	.num_controls = ARRAY_SIZE(adonisuniv_controls),
	.dapm_widgets = adonisuniv_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(adonisuniv_dapm_widgets),
	.dapm_routes = adonisuniv_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(adonisuniv_dapm_routes),

	.late_probe = adonisuniv_late_probe,

	.suspend_post = adonisuniv_suspend_post,
	.resume_pre = adonisuniv_resume_pre,

#ifdef USE_BIAS_LEVEL_POST
	.set_bias_level = adonisuniv_set_bias_level,
	.set_bias_level_post = adonisuniv_set_bias_level_post,
#endif
};

static int __devinit snd_adonisuniv_probe(struct platform_device *pdev)
{
	int ret;
	struct wm5102_machine_priv *wm5102;

	wm5102 = kzalloc(sizeof *wm5102, GFP_KERNEL);
	if (!wm5102) {
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	exynos5_audio_set_mclk(1, 0);

	ret = snd_soc_register_dais(&pdev->dev, adonisuniv_ext_dai,
					ARRAY_SIZE(adonisuniv_ext_dai));
	if (ret != 0)
		pr_err("Failed to register external DAIs: %d\n", ret);

	snd_soc_card_set_drvdata(&adonisuniv, wm5102);

	adonisuniv.dev = &pdev->dev;
	ret = snd_soc_register_card(&adonisuniv);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		kfree(wm5102);
	}

	adonisuniv_gpio_init();

	return ret;
}

static int __devexit snd_adonisuniv_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = &adonisuniv;
	struct wm5102_machine_priv *wm5102 = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(&adonisuniv);
	kfree(wm5102);

	exynos5_audio_set_mclk(0, 0);

#ifdef GPIO_MICBIAS_EN
	gpio_free(GPIO_MICBIAS_EN);
#endif

	return 0;
}

static struct platform_driver snd_adonisuniv_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wm5102-card",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_adonisuniv_probe,
	.remove = __devexit_p(snd_adonisuniv_remove),
};

module_platform_driver(snd_adonisuniv_driver);

MODULE_AUTHOR("JS. Park <aitdark.park@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC AdonisUniv wm5102");
MODULE_LICENSE("GPL");
