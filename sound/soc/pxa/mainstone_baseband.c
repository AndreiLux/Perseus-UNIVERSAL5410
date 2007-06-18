/*
 * mainstone_baseband.c
 * Mainstone Example Baseband modem  --  ALSA Soc Audio Layer
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    15th Apr 2006   Initial version.
 *
 * This is example code to demonstrate connecting a baseband modem to the PCM
 * DAI on the WM9713 codec on the Intel Mainstone platform. It is by no means
 * complete as it requires code to control the modem.
 *
 * The architecture consists of the WM9713 AC97 DAI connected to the PXA27x
 * AC97 controller and the WM9713 PCM DAI connected to the basebands DAI. The
 * baseband is controlled via a serial port. Audio is routed between the PXA27x
 * and the baseband via internal WM9713 analog paths.
 *
 * This driver is not the baseband modem driver. This driver only calls
 * functions from the Baseband driver to set up it's PCM DAI.
 *
 * It's intended to use this driver as follows:-
 *
 *  1. open() WM9713 PCM audio device.
 *  2. open() serial device (for AT commands).
 *  3. configure PCM audio device (rate etc) - sets up WM9713 PCM DAI,
 *      this will also set up the baseband PCM DAI (via calling baseband driver).
 *  4. send any further AT commands to set up baseband.
 *  5. configure codec audio mixer paths.
 *  6. open(), configure and read/write AC97 audio device - to Tx/Rx voice
 *
 * The PCM audio device is opened but IO is never performed on it as the IO is
 * directly between the codec and the baseband (and not the CPU).
 *
 * TODO:
 *  o Implement callbacks
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/audio.h>
#include <asm/arch/ssp.h>

#include "../codecs/wm9713.h"
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"
#include "pxa2xx-ssp.h"

static struct snd_soc_machine mainstone;

/* Do specific baseband PCM voice startup here */
static int baseband_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

/* Do specific baseband PCM voice shutdown here */
static void baseband_shutdown (struct snd_pcm_substream *substream)
{
}

/* Do specific baseband modem PCM voice hw params init here */
static int baseband_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	return 0;
}

/* Do specific baseband modem PCM voice hw params free here */
static int baseband_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

/*
 * Baseband Processor DAI
 */
static struct snd_soc_cpu_dai baseband_dai =
{	.name = "Baseband",
	.id = 0,
	.type = SND_SOC_DAI_PCM,
	.playback = {
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = {
		.startup = baseband_startup,
		.shutdown = baseband_shutdown,
		.hw_params = baseband_hw_params,
		.hw_free = baseband_hw_free,
		},
};

/* PM */
static int mainstone_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mainstone_resume(struct platform_device *pdev)
{
	return 0;
}

static int mainstone_probe(struct platform_device *pdev)
{
	return 0;
}

static int mainstone_remove(struct platform_device *pdev)
{
	return 0;
}

static int mainstone_wm9713_init(struct snd_soc_codec *codec)
{
	return 0;
}

/* the physical audio connections between the WM9713, Baseband and pxa2xx */
static struct snd_soc_dai_link mainstone_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI],
	.init = mainstone_wm9713_init,
},
{
	.name = "AC97 Aux",
	.stream_name = "AC97 Aux",
	.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_AUX],
},
{
	.name = "Baseband",
	.stream_name = "Voice",
	.cpu_dai = &baseband_dai,
	.codec_dai = &wm9713_dai[WM9713_DAI_PCM_VOICE],
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
	.codec_dev = &soc_codec_dev_wm9713,
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
MODULE_DESCRIPTION("Mainstone Example Baseband PCM Interface");
MODULE_LICENSE("GPL");
