/*
 * EXM32 Motherboard AC97 sound support; for Au1200 based systems.
 *
 * Copyright (c) 2007 MSC Vertriebsges.m.b.H, Manuel Lauss <mlau@msc-ge.com>
 * http://www.exm32.com
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

#include "../codecs/ac97.h"
#include "psc.h"

/* #define MACH_DEBUG */

#ifdef MACH_DEBUG
#define MSG(x...)	printk(KERN_INFO "exmmb-ac97: " x)
#else
#define MSG(x...)	do {} while (0)
#endif

/*
 * Alsa operations
 * Only implement the required operations for your platform.
 * These operations are specific to the machine only.
 */

/*
 * Initialise the machine audio subsystem.
 */
static int exm1200_mobo_ac97_machine_init(struct snd_soc_codec *codec)
{
	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

/* template digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link exm1200_mobo_ac97_dai = {
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &au1xpsc_ac97_dai[1],	/* we use PSC1 for I2S */
	.codec_dai = &ac97_dai,
	.init = exm1200_mobo_ac97_machine_init,
	.ops = NULL,
};

/* template audio machine driver */
static struct snd_soc_machine snd_soc_machine_exm97 = {
	.name = "EXM32 Motherboard AC97",
	.dai_link = &exm1200_mobo_ac97_dai,
	.num_links = 1,
};

/* template audio subsystem */
static struct snd_soc_device exm1200_mobo_ac97_snd_devdata = {
	.machine = &snd_soc_machine_exm97,
	.platform = &au1xpsc_soc_platform,
	.codec_dev = &soc_codec_dev_ac97,
};

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


static struct platform_device *exm1200_mobo_ac97_snd_device;

static int psc_to_ac97(void)
{
	unsigned long io;
	unsigned int i;

	/* modify sys_pinfunc for AC97 on PSC1 */
	io = au_readl(0xb190002c);
	io |= (1<<14);
	io &= ~((3<<21)|(1<<20));
	au_writel(io, 0xb190002c);
	au_sync();

	/* AC97 mode */
	io = 0xb1b00000;
	(void)au_readl(io + PSC_CTL);
	au_writel(0, io + PSC_CTL);
	au_writel(0, io + PSC_SEL);
	au_sync();
	au_writel(4 | (1<<5), io + PSC_SEL);

	/* cold reset */
	au_writel(2, io + PSC_AC97RST);
	au_sync();
	msleep(1200);
	au_writel(0, io + PSC_AC97RST);
	au_sync();

	/* enable PSC */
	au_writel(3, io + PSC_CTL);
	au_sync();

	i = 1000;
	while (((au_readl(io + PSC_AC97STAT) & 1) == 0) && (--i))
		au_sync();

	if (i == 0)
		printk(KERN_INFO "PSC down!\n");

	/* en ac97 core */
	au_writel(0x04000000, io + PSC_AC97CFG);
	au_sync();

	i = 1000;
	while (((au_readl(io + PSC_AC97STAT) & 2) == 0) && (--i))
		au_sync();

	if (i == 0)
		printk(KERN_INFO "PSC-AC97 not ready\n");
	return (i == 0);
}

static int exm1200_mobo_ac97_init(void)
{
	int ret = -ENOMEM;
	unsigned short brdctl;

	MSG("module_init() enter\n");

	/* switch FPGA to AC97 mode */
	brdctl = au_readw(EXM1200_FPGA_BRDCTRL);
	brdctl &= ~(1 << 13);
	au_writew(brdctl, EXM1200_FPGA_BRDCTRL);
	au_sync();

	if (psc_to_ac97()) {
		ret = -ENODEV;
		goto out;
	}

	exm1200_mobo_ac97_snd_device = platform_device_alloc("soc-audio", -1);
	if (!exm1200_mobo_ac97_snd_device)
		goto out;

	exm1200_mobo_ac97_snd_device->resource = au1x_psc_res;
	exm1200_mobo_ac97_snd_device->num_resources = ARRAY_SIZE(au1x_psc_res);
	exm1200_mobo_ac97_snd_device->id = 1;

	platform_set_drvdata(exm1200_mobo_ac97_snd_device, &exm1200_mobo_ac97_snd_devdata);
	exm1200_mobo_ac97_snd_devdata.dev = &exm1200_mobo_ac97_snd_device->dev;
	ret = platform_device_add(exm1200_mobo_ac97_snd_device);

	if (ret)
		platform_device_put(exm1200_mobo_ac97_snd_device);

out:
	MSG("module_init() exit (ret %d)\n", ret);
	return ret;
}

static void exm1200_mobo_ac97_exit(void)
{
	platform_device_del(exm1200_mobo_ac97_snd_device);
}

module_init(exm1200_mobo_ac97_init);
module_exit(exm1200_mobo_ac97_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MSC EXM32 Development Motherboard AC97 Audio support");
MODULE_AUTHOR("Manuel Lauss <mlau@msc-ge.com>");
