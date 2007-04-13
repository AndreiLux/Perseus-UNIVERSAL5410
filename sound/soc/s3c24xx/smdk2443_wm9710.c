/*
 * smdk2443_wm9710.c  --  SoC audio for smdk2443
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    8th Mar 2007   Initial version.
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

#include <asm/mach-types.h>
#include <asm/hardware/scoop.h>
#include <asm/arch/regs-iis.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <asm/arch/audio.h>
#include <asm/io.h>
#include <asm/arch/spi-gpio.h>
#include "../codecs/ac97.h"
#include "s3c24xx-pcm.h"
#include "s3c24xx-ac97.h"

#define SMDK2443_DEBUG 1
#if SMDK2443_DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

static struct snd_soc_machine smdk2443;

static int smdk2443_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int smdk2443_resume(struct platform_device *pdev)
{
	return 0;
}

static int smdk2443_probe(struct platform_device *pdev)
{
	return 0;
}

static int smdk2443_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct snd_soc_dapm_widget smdk2443_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic (Internal)", NULL),
};

/* example machine interconnections */
static const char* intercon[][3] = {

	/* mic is connected to mic1 - with bias */
	{"MIC1", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic (Internal)"},

	{NULL, NULL, NULL},
};

static int smdk2443_wm9710_init(struct snd_soc_codec *codec)
{
	int i;

	/* set up smdk2443 NC codec pins */
	snd_soc_dapm_set_endpoint(codec, "RXP", 0);
	snd_soc_dapm_set_endpoint(codec, "RXN", 0);

	/* Add smdk2443 specific widgets */
	for(i = 0; i < ARRAY_SIZE(smdk2443_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &smdk2443_dapm_widgets[i]);
	}

	/* set up smdk2443 specific audio path interconnects */
	for(i = 0; intercon[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(codec, intercon[i][0], intercon[i][1], intercon[i][2]);
	}

	snd_soc_dapm_sync_endpoints(codec);
	return 0;
}

static struct snd_soc_dai_link smdk2443_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &s3c2443_ac97_dai[0],
	.codec_dai = &ac97_dai,
	.init = smdk2443_wm9710_init,
},
};

static struct snd_soc_machine smdk2443 = {
	.name = "SMDK2443",
	.probe = smdk2443_probe,
	.remove = smdk2443_remove,
	.suspend_pre = smdk2443_suspend,
	.resume_post = smdk2443_resume,
	.dai_link = smdk2443_dai,
	.num_links = ARRAY_SIZE(smdk2443_dai),
};

static struct snd_soc_device smdk2443_snd_ac97_devdata = {
	.machine = &smdk2443,
	.platform = &s3c24xx_soc_platform,
	.codec_dev = &soc_codec_dev_ac97,
};

static struct platform_device *smdk2443_snd_ac97_device;

static int __init smdk2443_init(void)
{
	int ret;

	smdk2443_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!smdk2443_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(smdk2443_snd_ac97_device, &smdk2443_snd_ac97_devdata);
	smdk2443_snd_ac97_devdata.dev = &smdk2443_snd_ac97_device->dev;

	if((ret = platform_device_add(smdk2443_snd_ac97_device)) != 0)
		platform_device_put(smdk2443_snd_ac97_device);

	return ret;
}

static void __exit smdk2443_exit(void)
{
	platform_device_unregister(smdk2443_snd_ac97_device);
}

module_init(smdk2443_init);
module_exit(smdk2443_exit);

/* Module information */
MODULE_AUTHOR("Graeme Gregory, graeme.gregory@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC WM9710 SMDK2443");
MODULE_LICENSE("GPL");
