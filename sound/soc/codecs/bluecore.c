/*
 * bluecore.c  --  ALSA Soc CSR BlueCore PCM interface.
 *
 * Author:	Frank Mandarino <fmandarino@endrelia.com>
 *		Endrelia Technologies Inc.
 * Created:	Mar 16, 2007
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>

#define BLUECORE_VERSION "0.1"

#define BLUECORE_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE)

struct snd_soc_codec_dai bluecore_dai = {
	.name = "BlueCore",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000,
		.formats = BLUECORE_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000,
		.formats = BLUECORE_FORMATS,},
};

EXPORT_SYMBOL_GPL(bluecore_dai);

static int bluecore_soc_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	printk(KERN_INFO "bluecore: BlueCore PCM SoC Audio %s\n", BLUECORE_VERSION);

	socdev->codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (socdev->codec == NULL)
		return -ENOMEM;
	codec = socdev->codec;
	mutex_init(&codec->mutex);

	codec->name = "BlueCore";
	codec->owner = THIS_MODULE;
	codec->dai = &bluecore_dai;
	codec->num_dai = 1;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if(ret < 0)
		goto err;

	ret = snd_soc_register_card(socdev);
	if (ret < 0)
		goto bus_err;
	return 0;

bus_err:
	snd_soc_free_pcms(socdev);

err:
	kfree(socdev->codec);
	socdev->codec = NULL;
	return ret;
}

static int bluecore_soc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if(codec == NULL)
		return 0;

	snd_soc_free_pcms(socdev);
	kfree(socdev->codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_bluecore = {
	.probe = 	bluecore_soc_probe,
	.remove = 	bluecore_soc_remove,
};

EXPORT_SYMBOL_GPL(soc_codec_dev_bluecore);

MODULE_DESCRIPTION("Soc BlueCore PCM driver");
MODULE_AUTHOR("Frank Mandarino");
MODULE_LICENSE("GPL");

