/*
 * mainstone.c  --  SoC audio for Mainstone
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
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
 *  Revision history
 *    30th Oct 2005   Initial version.
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

#include "../codecs/ac97.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"

static struct snd_soc_machine mainstone;
static long mst_audio_suspend_mask;

static int mainstone_suspend(struct platform_device *pdev, pm_message_t state)
{
	mst_audio_suspend_mask = MST_MSCWR2;
	MST_MSCWR2 |= MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mainstone_resume(struct platform_device *pdev)
{
	MST_MSCWR2 &= mst_audio_suspend_mask | ~MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mainstone_probe(struct platform_device *pdev)
{
	MST_MSCWR2 &= ~MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static int mainstone_remove(struct platform_device *pdev)
{
	MST_MSCWR2 |= MST_MSCWR2_AC97_SPKROFF;
	return 0;
}

static struct snd_soc_machine_config codecs[] = {
{
	.name = "AC97",
	.sname = "AC97 HiFi",
	.iface = &pxa_ac97_interface[0],
},
{
	.name = "AC97 Aux",
	.sname = "AC97 Aux",
	.iface = &pxa_ac97_interface[1],
},
};

static struct snd_soc_machine mainstone = {
	.name = "Mainstone",
	.probe = mainstone_probe,
	.remove = mainstone_remove,
	.suspend_pre = mainstone_suspend,
	.resume_post = mainstone_resume,
	.config = codecs,
	.nconfigs = ARRAY_SIZE(codecs),
};

static struct snd_soc_device mainstone_snd_devdata = {
	.machine = &mainstone,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_ac97,
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
MODULE_DESCRIPTION("ALSA SoC Mainstone");
MODULE_LICENSE("GPL");
