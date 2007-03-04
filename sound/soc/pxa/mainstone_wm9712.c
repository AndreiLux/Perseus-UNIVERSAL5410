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
 *  Revision history
 *    29th Jan 2006   Initial version.
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

#include "../codecs/wm9712.h"
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

/* mainstone machine dapm widgets */
static const struct snd_soc_dapm_widget mainstone_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic (Internal)", NULL),
};

/* example machine interconnections */
static const char* intercon[][3] = {

	/* mic is connected to mic1 - with bias */
	{"MIC1", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic (Internal)"},

	{NULL, NULL, NULL},
};

/*
 * This is an example machine initialisation for a wm8753 connected to a
 * Mainstone II. It is missing logic to detect hp/mic insertions and logic
 * to re-route the audio in such an event.
 */
static int mainstone_wm9712_init(struct snd_soc_codec *codec)
{
	int i;

	/* set up mainstone codec pins */
	snd_soc_dapm_set_endpoint(codec, "RXP", 0);
	snd_soc_dapm_set_endpoint(codec, "RXN", 0);
	//snd_soc_dapm_set_endpoint(codec, "MIC2", 0);

	/* Add mainstone specific widgets */
	for(i = 0; i < ARRAY_SIZE(mainstone_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &mainstone_dapm_widgets[i]);
	}

	/* set up mainstone specific audio path interconnects */
	for(i = 0; intercon[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, intercon[i][0], intercon[i][1], intercon[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

static struct snd_soc_dai_link mainstone_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
	.codec_dai = &wm9712_dai[WM9712_DAI_AC97_HIFI],
	.init = mainstone_wm9712_init,
},
{
	.name = "AC97 Aux",
	.stream_name = "AC97 Aux",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
	.codec_dai = &wm9712_dai[WM9712_DAI_AC97_AUX],
},
};

static struct snd_soc_machine mainstone = {
	.name = "Mainstone",
	.probe = mainstone_probe,
	.remove = mainstone_remove,
	.suspend_pre = mainstone_suspend,
	.resume_post = mainstone_resume,
	.dai_link = mainstone_dai,
	.num_links = ARRAY_SIZE(mainstone_dai),
};

static struct snd_soc_device mainstone_snd_ac97_devdata = {
	.machine = &mainstone,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm9712,
};

static struct platform_device *mainstone_snd_ac97_device;

static int __init mainstone_init(void)
{
	int ret;

	mainstone_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!mainstone_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(mainstone_snd_ac97_device, &mainstone_snd_ac97_devdata);
	mainstone_snd_ac97_devdata.dev = &mainstone_snd_ac97_device->dev;

	if((ret = platform_device_add(mainstone_snd_ac97_device)) != 0)
		platform_device_put(mainstone_snd_ac97_device);

	return ret;
}

static void __exit mainstone_exit(void)
{
	platform_device_unregister(mainstone_snd_ac97_device);
}

module_init(mainstone_init);
module_exit(mainstone_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC WM9712 Mainstone");
MODULE_LICENSE("GPL");
