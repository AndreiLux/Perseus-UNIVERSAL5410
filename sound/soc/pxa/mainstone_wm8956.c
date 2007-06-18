/*
 * mainstone.c  --  SoC audio for Mainstone
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  Mainstone audio amplifier code taken from arch/arm/mach-pxa/mainstone.c
 *  Copyright:	MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/mainstone.h>
#include <asm/arch/audio.h>

#include "../codecs/wm8956.h"
#include "pxa2xx-pcm.h"

static struct snd_soc_machine mainstone;

/*
 * Init - to complete.
 */
static int mainstone_wm8956_init(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_dai_link mainstone_dai[] = {
{
	.name = "WM8956",
	.stream_name = "WM8956 HiFi",
	.cpu_dai = &pxa_i2s_dai,
	.codec_dai = &wm8956_dai,
	.init = mainstone_wm8956_init,
	.config_sysclk = mainstone_config_sysclk,
},
};

static struct snd_soc_machine mainstone = {
	.name = "Mainstone",
	.dai_link = mainstone_dai,
	.num_links = ARRAY_SIZE(mainstone_dai),
};

static struct wm8956_setup_data wm8956_setup = {
	.i2c_address = 0x1a,
};

static struct snd_soc_device mainstone_snd_devdata = {
	.machine = &mainstone,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8956,
	.codec_data = &wm8956_setup,
};

static struct platform_device *mainstone_snd_device;

static int __init mainstone_init(void)
{
	int ret;

	mainstone_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mainstone_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mainstone_snd_device, &mainstone_snd_devdata);
	mainstone_snd_devdata.dev = &mainstone_snd_device->dev;
	ret = platform_device_add(mainstone_snd_device);

	if (ret)
		platform_device_put(mainstone_snd_device);

	return ret;
}

static void __exit mainstone_exit(void)
{
	platform_device_unregister(mainstone_snd_device);
}

module_init(mainstone_init);
module_exit(mainstone_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC WM8956 Mainstone");
MODULE_LICENSE("GPL");
