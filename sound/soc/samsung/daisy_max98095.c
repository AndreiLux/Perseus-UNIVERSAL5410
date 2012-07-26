/*
 * Exynos machine ASoC driver for boards using MAX98095 codec.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/max98095.h>

#include <mach/regs-clock.h>
#include <mach/gpio.h>

#include "i2s.h"
#include "s3c-i2s-v2.h"
#include "../codecs/max98095.h"

/* Audio clock settings are belonged to board specific part. Every
 * board can set audio source clock setting which is matched with H/W
 * like this function-'set_audio_clock_heirachy'.
 */
static int set_audio_clock_heirachy(struct platform_device *pdev)
{
	struct clk *fout_epll, *mout_epll, *sclk_audbus, *audss, *i2sclk;
	int ret = 0;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_WARNING "%s: Cannot find fout_epll.\n",
				__func__);
		return -EINVAL;
	}

	mout_epll = clk_get(NULL, "mout_epll");
	if (IS_ERR(mout_epll)) {
		printk(KERN_WARNING "%s: Cannot find mout_epll.\n",
				__func__);
		ret = -EINVAL;
		goto out1;
	}

	sclk_audbus = clk_get(&pdev->dev, "audio-bus");
	if (IS_ERR(sclk_audbus)) {
		printk(KERN_WARNING "%s: Cannot find audio-bus.\n",
				__func__);
		ret = -EINVAL;
		goto out2;
	}

	audss = clk_get(&pdev->dev, "mout_audss");
	if (IS_ERR(audss)) {
		printk(KERN_WARNING "%s: Cannot find audss.\n",
				__func__);
		ret = -EINVAL;
		goto out3;
	}

	i2sclk = clk_get(NULL, "i2sclk");
	if (IS_ERR(i2sclk)) {
		printk(KERN_WARNING "%s: Cannot find i2sclk.\n",
				__func__);
		ret = -EINVAL;
		goto out4;
	}

	/* Set audio clock hierarchy for S/PDIF */
	if (clk_set_parent(mout_epll, fout_epll))
		printk(KERN_WARNING "Failed to set parent of epll.\n");
	if (clk_set_parent(sclk_audbus, mout_epll))
		printk(KERN_WARNING "Failed to set parent of audbus.\n");
	if (clk_set_parent(audss, fout_epll))
		printk(KERN_WARNING "Failed to set parent of audss.\n");
	if (clk_set_parent(i2sclk, sclk_audbus))
		printk(KERN_WARNING "Failed to set parent of i2sclk.\n");

	clk_put(i2sclk);
out4:
	clk_put(audss);
out3:
	clk_put(sclk_audbus);
out2:
	clk_put(mout_epll);
out1:
	clk_put(fout_epll);

	return ret;
}

static int set_epll_rate(unsigned long rate)
{
	int ret;
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");

	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	ret = clk_set_rate(fout_epll, rate);
	if (ret < 0) {
		printk(KERN_ERR "failed to clk_set_rate of fout_epll for audio\n");
		goto out;
	}
out:
	clk_put(fout_epll);

	return 0;
}

static int daisy_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
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
		printk(KERN_ERR "rclk = %lu is not yet supported!\n", rclk);
		return -EINVAL;
	}

	ret = set_epll_rate(rclk * psr);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
					     SND_SOC_DAIFMT_NB_NF |
					     SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
					   SND_SOC_DAIFMT_NB_NF |
					   SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * MAX98095 DAI operations.
 */
static struct snd_soc_ops daisy_ops = {
	.hw_params = daisy_hw_params,
};

static struct snd_soc_jack daisy_hp_jack;
static struct snd_soc_jack_pin daisy_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

static struct snd_soc_jack_gpio daisy_hp_jack_gpio = {
	.name = "headphone detect",
	.report = SND_JACK_HEADPHONE,
};

static struct snd_soc_jack daisy_mic_jack;
static struct snd_soc_jack_pin daisy_mic_jack_pins[] = {
	{
		.pin = "Mic Jack",
		.mask = SND_JACK_MICROPHONE,
	},
};

static const struct snd_soc_dapm_route daisy_audio_map[] = {
	{"Mic Jack", "NULL", "MICBIAS2"},
	{"MIC2", "NULL", "Mic Jack"},
};

static const struct snd_soc_dapm_widget daisy_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

static int daisy_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* TODO(thutt): This must be hooked up to the headphone-switch
	 * & microphone-detect GPIOs on the Exynos and plumbed through
	 * to to /dev/input.  These two GPIOs are presently done as a
	 * rework to existing boards.
	 */
	snd_soc_jack_new(codec, "Mic Jack", SND_JACK_MICROPHONE,
			 &daisy_mic_jack);
	snd_soc_jack_add_pins(&daisy_mic_jack,
			      ARRAY_SIZE(daisy_mic_jack_pins),
			      daisy_mic_jack_pins);

	/* TODO: Move this to device tree */
	if (of_machine_is_compatible("google,snow")) {
		daisy_hp_jack_gpio.gpio = EXYNOS5_GPX2(2);
		snd_soc_jack_new(codec, "Headphone Jack",
				 SND_JACK_HEADPHONE, &daisy_hp_jack);
		snd_soc_jack_add_pins(&daisy_hp_jack,
				      ARRAY_SIZE(daisy_hp_jack_pins),
				      daisy_hp_jack_pins);
		snd_soc_jack_add_gpios(&daisy_hp_jack, 1, &daisy_hp_jack_gpio);
	}

	/* Microphone BIAS is needed to power the analog mic.
	 * MICBIAS2 is connected to analog mic (MIC3, which is in turn
	 * connected to MIC2 via 'External MIC') on Daisy.
	 *
	 * Ultimately, the following should hold:
	 *
	 *   Microphone in jack	    => MICBIAS2 enabled &&
	 *			       'External Mic' = MIC2
	 *   Microphone not in jack => MICBIAS2 disabled &&
	 *			       'External Mic' = MIC1
	*/
	snd_soc_dapm_force_enable_pin(dapm, "MICBIAS2");

	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link daisy_dai[] = {
	{ /* Primary DAI i/f */
		.name = "MAX98095 RX",
		.stream_name = "Playback",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "HiFi",
		.platform_name = "samsung-audio",
		.init = daisy_init,
		.ops = &daisy_ops,
	}, { /* Capture i/f */
		.name = "MAX98095 TX",
		.stream_name = "Capture",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "HiFi",
		.platform_name = "samsung-audio",
		.ops = &daisy_ops,
	},
};

static struct snd_soc_card daisy_snd = {
	.name = "DAISY-I2S",
	.dai_link = daisy_dai,
	.num_links = ARRAY_SIZE(daisy_dai),
	.dapm_widgets = daisy_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(daisy_dapm_widgets),
	.dapm_routes = daisy_audio_map,
	.num_dapm_routes = ARRAY_SIZE(daisy_audio_map),
};

static struct platform_device *daisy_snd_device;

static int __init daisy_audio_init(void)
{
	struct device_node *dn;
	int i, ret;

	/* The below needs to be replaced with proper full device-tree probing
	 * of the ASoC device, but the core plumbing hasn't been completed yet
	 * so we're doing this only half-way now.
	 */

	if (!of_machine_is_compatible("google,snow") &&
	    !of_machine_is_compatible("google,daisy"))
		return -ENODEV;

	dn = of_find_compatible_node(NULL, NULL, "maxim,max98095");
	if (!dn)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(daisy_dai); i++)
		daisy_dai[i].codec_of_node = of_node_get(dn);

	of_node_put(dn);

	daisy_snd_device = platform_device_alloc("soc-audio", -1);
	if (!daisy_snd_device)
		return -ENOMEM;

	platform_set_drvdata(daisy_snd_device, &daisy_snd);
	ret = platform_device_add(daisy_snd_device);
	if (ret) {
		platform_device_put(daisy_snd_device);
		return ret;
	}

	return set_audio_clock_heirachy(daisy_snd_device);
}
module_init(daisy_audio_init);

static void __exit daisy_audio_exit(void)
{
	platform_device_unregister(daisy_snd_device);
}
module_exit(daisy_audio_exit);

MODULE_DESCRIPTION("ALSA SoC DAISY MAX98095");
MODULE_LICENSE("GPL");
