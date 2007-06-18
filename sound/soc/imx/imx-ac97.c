/*
 * imx-ssi.c  --  SSI driver for Freescale IMX
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  Based on mxc-alsa-mc13783 (C) 2006 Freescale.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    29th Aug 2006   Initial version.
 *
 */


static imx_pcm_dma_params_t imx_ssi1_pcm_stereo_out = {
	.name			= "SSI1 PCM Stereo out",
	.params = {
		.bd_number = 1,
		.transfer_type = emi_2_per,
		.watermark_level = SDMA_TXFIFO_WATERMARK,
		.word_size = TRANSFER_16BIT, // maybe add this in setup func
		.per_address = SSI1_STX0,
		.event_id = DMA_REQ_SSI1_TX1,
		.peripheral_type = SSI,
	},
};

static imx_pcm_dma_params_t imx_ssi1_pcm_stereo_in = {
	.name			= "SSI1 PCM Stereo in",
	.params = {
		.bd_number = 1,
		.transfer_type = per_2_emi,
		.watermark_level = SDMA_RXFIFO_WATERMARK,
		.word_size = TRANSFER_16BIT, // maybe add this in setup func
		.per_address = SSI1_SRX0,
		.event_id = DMA_REQ_SSI1_RX1,
		.peripheral_type = SSI,
	},
};

static imx_pcm_dma_params_t imx_ssi2_pcm_stereo_out = {
	.name			= "SSI2 PCM Stereo out",
	.params = {
		.bd_number = 1,
		.transfer_type = per_2_emi,
		.watermark_level = SDMA_TXFIFO_WATERMARK,
		.word_size = TRANSFER_16BIT, // maybe add this in setup func
		.per_address = SSI2_STX0,
		.event_id = DMA_REQ_SSI2_TX1,
		.peripheral_type = SSI,
	},
};

static imx_pcm_dma_params_t imx_ssi2_pcm_stereo_in = {
	.name			= "SSI2 PCM Stereo in",
	.params = {
		.bd_number = 1,
		.transfer_type = per_2_emi,
		.watermark_level = SDMA_RXFIFO_WATERMARK,
		.word_size = TRANSFER_16BIT, // maybe add this in setup func
		.per_address = SSI2_SRX0,
		.event_id = DMA_REQ_SSI2_RX1,
		.peripheral_type = SSI,
	},
};

static unsigned short imx_ssi_ac97_read(struct snd_ac97 *ac97, unsigned short reg)
{
}

static void imx_ssi_ac97_write(struct snd_ac97 *ac97, unsigned short reg, unsigned short val)
{
}

static void imx_ssi_ac97_warm_reset(struct snd_ac97 *ac97)
{
}

static void imx_ssi_ac97_cold_reset(struct snd_ac97 *ac97)
{
}

struct snd_ac97_bus_ops soc_ac97_ops = {
	.read	= imx_ssi_ac97_read,
	.write	= imx_ssi_ac97_write,
	.warm_reset	= imx_ssi_ac97_warm_reset,
	.reset	= imx_ssi_ac97_cold_reset,
};


static intimx_ssi1_ac97_probe(struct platform_device *pdev)
{
	int ret;


	return ret;
}

static void imx_ssi1_ac97_remove(struct platform_device *pdev)
{
	/* shutdown SSI */
		if(rtd->cpu_dai->id == 0)
			SSI1_SCR &= ~SSI_SCR_SSIEN;
		else
			SSI2_SCR &= ~SSI_SCR_SSIEN;
	}

}

static int imx_ssi1_ac97_prepare(struct snd_pcm_substream *substream)
{
	// set vra
}

static int imx_ssi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	if (!rtd->cpu_dai->active) {

	}

	return 0;
}

static int imx_ssi1_trigger(struct snd_pcm_substream *substream, int cmd)
{

	return ret;
}

static void imx_ssi_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;


}

#ifdef CONFIG_PM
static int imx_ssi_suspend(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{
	if(!dai->active)
		return 0;


	return 0;
}

static int imx_ssi_resume(struct platform_device *pdev,
	struct snd_soc_cpu_dai *dai)
{
	if(!dai->active)
		return 0;

	return 0;
}

#else
#define imx_ssi_suspend	NULL
#define imx_ssi_resume	NULL
#endif

#define IMX_AC97_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000)

struct snd_soc_cpu_dai imx_ssi_ac97_dai = {
	.name = "imx-ac97-1",
	.id = 0,
	.type = SND_SOC_DAI_AC97,
	.suspend = imx_ssi_suspend,
	.resume = imx_ssi_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = IMX_AC97_RATES,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = IMX_AC97_RATES,},
	.ops = {
		.probe = imx_ac97_probe,
		.remove = imx_ac97_shutdown,
		.trigger = imx_ssi_trigger,
		.prepare = imx_ssi_ac97_prepare,},
},
{
	.name = "imx-ac97-2",
	.id = 1,
	.type = SND_SOC_DAI_AC97,
	.suspend = imx_ssi_suspend,
	.resume = imx_ssi_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = IMX_AC97_RATES,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = IMX_AC97_RATES,},
	.ops = {
		.probe = imx_ac97_probe,
		.remove = imx_ac97_shutdown,
		.trigger = imx_ssi_trigger,
		.prepare = imx_ssi_ac97_prepare,},
};

EXPORT_SYMBOL_GPL(imx_ssi_ac97_dai);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("i.MX ASoC AC97 driver");
MODULE_LICENSE("GPL");
