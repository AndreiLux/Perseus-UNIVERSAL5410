/*
 * EXM32 Development Motherboard I2S, for the Crystal CS42518 codec
 * and SH7760 based CPU Modules.
 *
 * Copyright (c) 2007 MSC Vertriebsges.m.b.H.
 * Written by Manuel Lauss <mlau@msc-ge.com>
 * http://www.exm32.com
 *
 * Licensed under the GPLv2.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asm/exm7760/exm7760.h>
#include <asm/io.h>

#include <linux/exm32.h>
#include "../codecs/cs4251x.h"

/* #define MACH_DEBUG */

#ifdef MACH_DEBUG
#define MSG(x...)	printk(KERN_INFO "exmmb-i2s: " x)
#else
#define MSG(x...)	do {} while (0)
#endif

/* SH7760 pinmux config register */
#define IPSEL 0xFE400034

extern struct snd_soc_cpu_dai sh4_ssi_dai[2];
extern struct snd_soc_platform sh7760_soc_platform;

static int
exm7760_i2s_machine_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;

	int ret;
	MSG("machine_hw_params() enter\n");

	/* set codec and i2s interfaces to slave mode */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_LEFT_J |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		MSG("codec dai setfmt() failed!\n");
		goto out;
	}

	ret = codec_dai->dai_ops.set_sysclk(codec_dai, 0, 24576000, 0);
	if (ret < 0) {
		MSG("codec does not want sysclk\n");
		goto out;
	}

	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_LEFT_J |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		MSG("cpu-i2s dai setfmt() failed!\n");
		goto out;
	}

	ret = 0;
out:
	MSG("machine_hw_params() leave\n");
	return ret;
}

static struct snd_soc_ops exm7760_i2s_ops = {
	.hw_params = exm7760_i2s_machine_hw_params,
};

static int exm7760_i2s_machine_init(struct snd_soc_codec *codec)
{
	unsigned short ipsel;

	ipsel = ctrl_inw(IPSEL);
	ipsel &= ~(3<<11);	/* switch to SSI on channels 0,1 */
	ctrl_outw(ipsel, IPSEL);

	/* EXM32 Motherboard uses the codec in MASTER mode (DSP possible)
	 *  with ADC (recording) data output at the SAI_SDOUT/SAI_SPCLK pins,
	 *  the chip clocked at 24.576 MHz from an external clocksource.
	 * RXP5/6 determine DAC input source:
	 *	RXP5 ----\
	 *	RXP6 ---\|
	 *		||
	 *		00 I2S data from CPU
	 *		01 DSP
	 *		10 I2S audio from GSM phone on navi module
	 *		11 SPDIF input from motherboard/IEEE1394 controller, ...
	 */
	cs4251x_write(codec, CS4251X_CLKCTL,
		      CS4251X_CLKSRC_AUTO_PLL_OMCK |
		      CS4251X_CLKSRC_OMCK_245760MHZ);
	cs4251x_write(codec, CS4251X_FUNCMODE,
		      CS4251X_ADCDAI_SAISDOUT_SAISP_CLK);
	cs4251x_write(codec, CS4251X_MUTEC, 0x1f);
	cs4251x_write(codec, CS4251X_RCVMODECTL, 0x80);
	cs4251x_write(codec, CS4251X_RCVMODECTL2, 0x02);

	/* set DAC input to I2S from CPU */
	cs4251x_gpio_mode(codec, CS4251X_RXP_5,
			  CS4251X_GPIO_MODE_GPOLOW, 0, 0);
	cs4251x_gpio_mode(codec, CS4251X_RXP_6,
			  CS4251X_GPIO_MODE_GPOLOW, 0, 0);

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

/* FIXME: units are simplex, and both are wired up (SSI0 = playback,
 *	  SSI1 = capture
 */
static struct snd_soc_dai_link exm7760_i2s_dai = {
	.name = "CS4251X",
	.stream_name = "CS4251X",
	.cpu_dai = &sh4_ssi_dai[0],
	.codec_dai = &cs4251x_dai,
	.init = exm7760_i2s_machine_init,
	.ops = &exm7760_i2s_ops,
};

static struct snd_soc_machine snd_soc_machine_template = {
	.name = "EXM32 Motherboard I2S",
	.dai_link = &exm7760_i2s_dai,
	.num_links = 1,
};

/* CS42518 setup data. */
static struct cs4251x_setup_data exm7760_i2s_codec_setup = {
	.i2c_address = 0x9e >> 1,
	.irq = EXMMB_CODEC_IRQ,
};

static struct snd_soc_device exm7760_i2s_snd_devdata = {
	.machine = &snd_soc_machine_template,
	.platform = &sh7760_soc_platform,
	.codec_dev = &soc_codec_dev_cs4251x,
	.codec_data = &exm7760_i2s_codec_setup,
};

static struct platform_device *exm7760_i2s_snd_device;

static int exm7760_i2s_init(void)
{
	int ret;
	unsigned short ipsel;

	printk(KERN_INFO "EXM32 Motherboard I2S Audio support\n");

	/* switch PFC to I2S on both audio paths */
	ipsel = ctrl_inw(IPSEL);
	ipsel &= ~(3<<10);
	ctrl_outw(ipsel, IPSEL);

	ret = -ENOMEM;
	exm7760_i2s_snd_device = platform_device_alloc("soc-audio", -1);
	if (!exm7760_i2s_snd_device)
		goto out;

	platform_set_drvdata(exm7760_i2s_snd_device,
			     &exm7760_i2s_snd_devdata);
	exm7760_i2s_snd_devdata.dev = &exm7760_i2s_snd_device->dev;
	ret = platform_device_add(exm7760_i2s_snd_device);

	if (ret)
		platform_device_put(exm7760_i2s_snd_device);

out:
	MSG("module_init() exit (ret %d)\n", ret);
	return ret;
}

static void exm7760_i2s_exit(void)
{
	MSG("exm7760_i2s_exit enter\n");
	platform_device_del(exm7760_i2s_snd_device);
	MSG("exm7760_i2s_exit leave\n");
}

module_init(exm7760_i2s_init);
module_exit(exm7760_i2s_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MSC EXM32 Motherboard I2S sound support");
MODULE_AUTHOR("Manuel Lauss <mlau@msc-ge.com>");
