/*
 * eti_b1_bluecore  --  SoC audio for AT91RM9200-based Endrelia ETI_B1 board
 *
 * ASoC Codec and Machine driver for CSR BlueCore PCM interface.
 *
 * Author:	Frank Mandarino <fmandarino@endrelia.com>
 *		Endrelia Technologies Inc.
 * Created:	Mar 16, 2007
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/arch/hardware.h>
#include <asm/arch/at91_pio.h>
#include <asm/arch/gpio.h>

#include "at91-pcm.h"
#include "at91-ssc.h"

#if 0
#define	DBG(x...)	printk(KERN_INFO "eti_b1_bluecore: " x)
#else
#define	DBG(x...)
#endif


/*
 * CSR BlueCore PCM interface codec driver.
 */
#define BLUECORE_VERSION "0.1"

#define BLUECORE_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE)

static struct snd_soc_codec_dai bluecore_dai = {
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

static struct snd_soc_codec_device soc_codec_dev_bluecore = {
	.probe = 	bluecore_soc_probe,
	.remove = 	bluecore_soc_remove,
};


/*
 * Endrelia ETI_B1 BlueCore Machine driver.
 */
#define AT91_PIO_TF2	(1 << (AT91_PIN_PB12 - PIN_BASE) % 32)
#define AT91_PIO_TK2	(1 << (AT91_PIN_PB13 - PIN_BASE) % 32)
#define AT91_PIO_TD2	(1 << (AT91_PIN_PB14 - PIN_BASE) % 32)
#define AT91_PIO_RD2	(1 << (AT91_PIN_PB15 - PIN_BASE) % 32)
#define AT91_PIO_RK2	(1 << (AT91_PIN_PB16 - PIN_BASE) % 32)
#define AT91_PIO_RF2	(1 << (AT91_PIN_PB17 - PIN_BASE) % 32)


/*
 * CSR BlueCore PCM interface machine driver operations.
 *
 * Only hw_params() is required, and only the CPU_DAI needs to be
 * initiailzed, as there is no real CODEC DAI.  The CODEC DAI
 * is controlled by BlueZ BCCMD and SCO operations.
 */
static int eti_b1_bcore_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret;
	int cmr_div, period;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/*
	 * The BlueCore PCM interface only operates at 8kHz, and
	 * the PCM_CLK signal must be less than 2048kHz.
	 */
	cmr_div = 75;	/* PCM_CLK = 60MHz/(2*75) = 400kHz */
	period = 24;	/* PCM_SYNC = PCM_CLK/(2*(24+1)) = 8000Hz */

	/* set the MCK divider for PCM_CLK */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai, AT91SSC_CMR_DIV, cmr_div);
	if (ret < 0)
		return ret;

	/* set the BCLK divider for DACLRC */
	ret = cpu_dai->dai_ops.set_clkdiv(cpu_dai, AT91SSC_TCMR_PERIOD, period);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops eti_b1_bcore_ops = {
	.hw_params = eti_b1_bcore_hw_params,
};

static struct snd_soc_dai_link eti_b1_bcore_dai = {
	.name = "BlueCore PCM",
	.stream_name = "BlueCore",
	.cpu_dai = &at91_ssc_dai[2],
	.codec_dai = &bluecore_dai,
	.ops = &eti_b1_bcore_ops,
};

static struct snd_soc_machine snd_soc_machine_eti_b1_bcore = {
	.name = "ETI_B1_BLUECORE",
	.dai_link = &eti_b1_bcore_dai,
	.num_links = 1,
};

static struct snd_soc_device eti_b1_bcore_snd_devdata = {
	.machine = &snd_soc_machine_eti_b1_bcore,
	.platform = &at91_soc_platform,
	.codec_dev = &soc_codec_dev_bluecore,
};

static struct platform_device *eti_b1_bcore_snd_device;

static int __init eti_b1_bcore_init(void)
{
	int ret;
	u32 ssc_pio_lines;
	struct at91_ssc_periph *ssc = eti_b1_bcore_dai.cpu_dai->private_data;

	if (!request_mem_region(AT91RM9200_BASE_SSC2, SZ_16K, "soc-audio-2")) {
		DBG("SSC2 memory region is busy\n");
		return -EBUSY;
	}

	ssc->base = ioremap(AT91RM9200_BASE_SSC2, SZ_16K);
	if (!ssc->base) {
		DBG("SSC2 memory ioremap failed\n");
		ret = -ENOMEM;
		goto fail_release_mem;
	}

	ssc->pid = AT91RM9200_ID_SSC2;

	eti_b1_bcore_snd_device = platform_device_alloc("soc-audio", 2);
	if (!eti_b1_bcore_snd_device) {
		DBG("platform device allocation failed\n");
		ret = -ENOMEM;
		goto fail_io_unmap;
	}

	platform_set_drvdata(eti_b1_bcore_snd_device, &eti_b1_bcore_snd_devdata);
	eti_b1_bcore_snd_devdata.dev = &eti_b1_bcore_snd_device->dev;

	ret = platform_device_add(eti_b1_bcore_snd_device);
	if (ret) {
		DBG("platform device add failed\n");
		platform_device_put(eti_b1_bcore_snd_device);
		goto fail_io_unmap;
	}

 	ssc_pio_lines = AT91_PIO_TF2 | AT91_PIO_TK2 | AT91_PIO_TD2
			| AT91_PIO_RD2 /* | AT91_PIO_RK1 | AT91_PIO_RF1 */;

	/* Reset all PIO registers and assign lines to peripheral A */
 	at91_sys_write(AT91_PIOB + PIO_PDR,  ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_ODR,  ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_IFDR, ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_CODR, ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_IDR,  ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_MDDR, ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_PUDR, ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_ASR,  ssc_pio_lines);
 	at91_sys_write(AT91_PIOB + PIO_OWDR, ssc_pio_lines);

	printk(KERN_INFO "eti_b1_bcore: BlueCore PCM in Slave Mode\n");
	return ret;

fail_io_unmap:
	iounmap(ssc->base);
fail_release_mem:
	release_mem_region(AT91RM9200_BASE_SSC2, SZ_16K);
	return ret;
}

static void __exit eti_b1_bcore_exit(void)
{
	struct at91_ssc_periph *ssc = eti_b1_bcore_dai.cpu_dai->private_data;

	platform_device_unregister(eti_b1_bcore_snd_device);

	iounmap(ssc->base);
	release_mem_region(AT91RM9200_BASE_SSC2, SZ_16K);
}

module_init(eti_b1_bcore_init);
module_exit(eti_b1_bcore_exit);

/* Module information */
MODULE_AUTHOR("Frank Mandarino <fmandarino@endrelia.com>");
MODULE_DESCRIPTION("ALSA SoC ETI-B1-BlueCore");
MODULE_LICENSE("GPL");
