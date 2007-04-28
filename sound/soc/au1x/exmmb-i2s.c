/*
 * EXM32 mobo I2S, for the Crystal CS42518 codec and Au1200 based CPU Modules.
 *
 * Copyright (c) 2007 MSC Vertriebsges.m.b.H, Manuel Lauss <mlau@msc-ge.com>
 * see http://www.exm32.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
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
#include <asm/mach-exm1200/exm1200.h>

#include <linux/exm32.h>

#include "../codecs/cs4251x.h"
#include "psc.h"

/* #define MACH_DEBUG */

#ifdef MACH_DEBUG
#define MSG(x...)	printk(KERN_INFO "exmmb-i2s: " x)
#else
#define MSG(x...)	do {} while (0)
#endif

static int exm1200_mobo_i2s_machine_hw_params(struct snd_pcm_substream *substream,
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

/* machine Alsa PCM operations */
static struct snd_soc_ops exm1200_mobo_i2s_ops = {
	.hw_params = exm1200_mobo_i2s_machine_hw_params,
};

/*
 * Initialise the machine audio subsystem.
 */
static int exm1200_mobo_i2s_machine_init(struct snd_soc_codec *codec)
{
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
	cs4251x_write(codec, CS4251X_CLKCTL, CS4251X_CLKSRC_AUTO_PLL_OMCK | CS4251X_CLKSRC_OMCK_245760MHZ);
	cs4251x_write(codec, CS4251X_FUNCMODE, CS4251X_ADCDAI_SAISDOUT_SAISP_CLK);
	cs4251x_write(codec, CS4251X_MUTEC, 0x1f);
	cs4251x_write(codec, CS4251X_RCVMODECTL, 0x80);
	cs4251x_write(codec, CS4251X_RCVMODECTL2, 0x02);
	/* set DAC input to I2S from CPU */
	cs4251x_gpio_mode(codec, CS4251X_RXP_5, CS4251X_GPIO_MODE_GPOLOW, 0, 0);
	cs4251x_gpio_mode(codec, CS4251X_RXP_6, CS4251X_GPIO_MODE_GPOLOW, 0, 0);

	/* mark unused codec pins as NC */

	/* Add template specific controls */

	/* Add template specific widgets */

	/* Set up template specific audio path audio_map */

	/* synchronise subsystem */
	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

static struct snd_soc_dai_link exm1200_mobo_i2s_dai = {
	.name = "CS4251X",
	.stream_name = "CS4251X",
	.cpu_dai = &au1xpsc_i2s_dai[1],	/* we use PSC1 for I2S */
	.codec_dai = &cs4251x_dai,
	.init = exm1200_mobo_i2s_machine_init,
	.ops = &exm1200_mobo_i2s_ops,
};

static struct snd_soc_machine snd_soc_machine_template = {
	.name = "EXM32 Motherboard I2S",
	.dai_link = &exm1200_mobo_i2s_dai,
	.num_links = 1,
};

/* CS42518 setup data. */
static struct cs4251x_setup_data exm1200_mobo_i2s_codec_setup = {
	.i2c_address = 0x9e >> 1,
	.irq = EXMMB_CODEC_IRQ,
};

static struct snd_soc_device exm1200_mobo_i2s_snd_devdata = {
	.machine = &snd_soc_machine_template,
	.platform = &au1xpsc_soc_platform,
	.codec_dev = &soc_codec_dev_cs4251x,
	.codec_data = &exm1200_mobo_i2s_codec_setup,
};

static struct platform_device *exm1200_mobo_i2s_snd_device;

static struct resource au1x_psc_res[] = {
	[0] = {
		.start	= PSC1_BASE,
		.end	= PSC1_BASE + 0xffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 11,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 16,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= 17,
		.flags	= IORESOURCE_DMA,
	},
};

static int exm1200_mobo_i2s_init(void)
{
	int ret = -ENOMEM;
	unsigned short brdctl;

	MSG("module_init() enter\n");

	/* switch FPGA to I2S mode */
	brdctl = au_readw(EXM1200_FPGA_BRDCTRL);
	brdctl |= (1 << 13);
	au_writew(brdctl, EXM1200_FPGA_BRDCTRL);
	au_sync();

	exm1200_mobo_i2s_snd_device = platform_device_alloc("soc-audio", -1);
	if (!exm1200_mobo_i2s_snd_device)
		goto out;

	exm1200_mobo_i2s_snd_device->resource = au1x_psc_res;
	exm1200_mobo_i2s_snd_device->num_resources = ARRAY_SIZE(au1x_psc_res);
	exm1200_mobo_i2s_snd_device->id = 1;

	platform_set_drvdata(exm1200_mobo_i2s_snd_device, &exm1200_mobo_i2s_snd_devdata);
	exm1200_mobo_i2s_snd_devdata.dev = &exm1200_mobo_i2s_snd_device->dev;
	ret = platform_device_add(exm1200_mobo_i2s_snd_device);

	if (ret)
		platform_device_put(exm1200_mobo_i2s_snd_device);

out:
	MSG("module_init() exit (ret %d)\n", ret);
	return ret;
}

static void exm1200_mobo_i2s_exit(void)
{
	platform_device_del(exm1200_mobo_i2s_snd_device);
}

module_init(exm1200_mobo_i2s_init);
module_exit(exm1200_mobo_i2s_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MSC EXM32 Development Motherboard I2S Audio support");
MODULE_AUTHOR("Manuel Lauss <mlau@msc-ge.com>");
