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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <asm/arch/dma.h>
#include <asm/arch/spba.h>
#include <asm/arch/clock.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>

#include "imx-ssi.h"
#include "imx31-pcm.h"

static struct mxc_pcm_dma_params imx_ssi1_pcm_stereo_out = {
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

static struct mxc_pcm_dma_params imx_ssi1_pcm_stereo_in = {
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

static struct mxc_pcm_dma_params imx_ssi2_pcm_stereo_out = {
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

static struct mxc_pcm_dma_params imx_ssi2_pcm_stereo_in = {
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

/*
 * SSI system clock configuration.
 */
static int imx_ssi_set_dai_sysclk(struct snd_soc_cpu_dai *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	u32 scr;

	if (cpu_dai->id == IMX_DAI_SSI1)
		scr = __raw_readw(SSI1_SCR);
	else
		scr = __raw_readw(SSI2_SCR);

	switch (clk_id) {
	case IMX_SSP_SYS_CLK:
		if (dir == SND_SOC_CLOCK_OUT)
			scr |= SSI_SCR_SYS_CLK_EN;
		else
			scr &= ~SSI_SCR_SYS_CLK_EN;
		break;
	default:
		return -EINVAL;
	}

	if (cpu_dai->id == IMX_DAI_SSI1)
		__raw_writew(scr, SSI1_SCR);
	else
		__raw_writew(scr, SSI2_SCR);

	return 0;
}

/*
 * SSI Clock dividers
 */
static int imx_ssi_set_dai_clkdiv(struct snd_soc_cpu_dai *cpu_dai,
	int div_id, int div)
{
	u32 stccr;

	if (cpu_dai->id == IMX_DAI_SSI1)
		stccr = __raw_readw(SSI1_STCCR);
	else
		stccr = __raw_readw(SSI2_STCCR);

	switch (div_id) {
	case IMX_SSI_DIV_2:
		stccr &= ~SSI_STCCR_DIV2;
		stccr |= div;
		break;
	case IMX_SSI_DIV_PSR:
		stccr &= ~SSI_STCCR_PSR;
		stccr |= div;
		break;
	case IMX_SSI_DIV_PM:
		stccr &= ~0xff;
		stccr |= SSI_STCCR_PM(div);
		break;
	default:
		return -EINVAL;
	}

	if (cpu_dai->id == IMX_DAI_SSI1)
		__raw_writew(stccr, SSI1_STCCR);
	else
		__raw_writew(stccr, SSI2_STCCR);

	return 0;
}

/*
 * SSI Network Mode or TDM slots configuration.
 */
static int imx_ssi_set_dai_tdm_slot(struct snd_soc_cpu_dai *cpu_dai,
	unsigned int mask, int slots)
{
	u32 stmsk, srmsk, scr, stccr;

	if (cpu_dai->id == IMX_DAI_SSI1) {
		stmsk = __raw_readw(SSI1_STMSK);
		srmsk = __raw_readw(SSI1_SRMSK);
		scr = __raw_readw(SSI1_SCR);
		stccr = __raw_readw(SSI1_STCCR);
	} else {
		stmsk = __raw_readw(SSI2_STMSK);
		srmsk = __raw_readw(SSI2_SRMSK);
		scr = __raw_readw(SSI2_SCR);
		stccr = __raw_readw(SSI2_STCCR);
	}

	stmsk = srmsk = mask;
	scr |= SSI_SCR_NET;
	stccr &= ~0x1f00;
	stccr |= SSI_STCCR_DC(slots);

	if (cpu_dai->id == IMX_DAI_SSI1) {
		__raw_writew(stmsk, SSI1_STMSK);
		__raw_writew(srmsk, SSI1_SRMSK);
		__raw_writew(scr, SSI1_SCR);
		__raw_writew(stccr, SSI1_STCCR);
	} else {
		__raw_writew(stmsk, SSI2_STMSK);
		__raw_writew(srmsk, SSI2_SRMSK);
		__raw_writew(scr, SSI2_SCR);
		__raw_writew(stccr, SSI2_STCCR);
	}

	return 0;
}

/*
 * SSI DAI format configuration.
 */
static int imx_ssi_set_dai_fmt(struct snd_soc_cpu_dai *cpu_dai,
		unsigned int fmt)
{
	u32 stcr = 0, srcr = 0;

	/* DAI mode */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		stcr |= SSI_STCR_TSCKP | SSI_STCR_TFSI |
			SSI_STCR_TEFS | SSI_STCR_TXBIT0;
		srcr |= SSI_SRCR_RSCKP | SSI_SRCR_RFSI |
			SSI_SRCR_REFS | SSI_SRCR_RXBIT0;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		stcr |= SSI_STCR_TSCKP | SSI_STCR_TFSI | SSI_STCR_TXBIT0;
		srcr |= SSI_SRCR_RSCKP | SSI_SRCR_RFSI | SSI_SRCR_RXBIT0;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		stcr |= SSI_STCR_TEFS; // data 1 bit after sync
		srcr |= SSI_SRCR_REFS; // data 1 bit after sync
	case SND_SOC_DAIFMT_DSP_A:
		stcr |= SSI_STCR_TFSL; // frame is 1 bclk long
		srcr |= SSI_SRCR_RFSL; // frame is 1 bclk long

		/* DAI clock inversion */
		switch(fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_IB_IF:
			stcr |= SSI_STCR_TFSI | SSI_STCR_TSCKP;
			srcr |= SSI_SRCR_RFSI | SSI_SRCR_RSCKP;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			stcr |= SSI_STCR_TSCKP;
			srcr |= SSI_SRCR_RSCKP;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			stcr |= SSI_STCR_TFSI;
			srcr |= SSI_SRCR_RFSI;
			break;
		}
		break;
	}

	/* DAI clock master masks */
	switch(fmt & SND_SOC_DAIFMT_CLOCK_MASK){
	case SND_SOC_DAIFMT_CBM_CFM:
		stcr |= SSI_STCR_TFDIR | SSI_STCR_TXDIR;
		srcr |= SSI_SRCR_RFDIR | SSI_SRCR_RXDIR;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		stcr |= SSI_STCR_TFDIR;
		srcr |= SSI_SRCR_RFDIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		stcr |= SSI_STCR_TXDIR;
		srcr |= SSI_SRCR_RXDIR;
		break;
	}

	/* async */
	//if (rtd->cpu_dai->flags & SND_SOC_DAI_ASYNC)
	//	SSI1_SCR |= SSI_SCR_SYN;

	if (cpu_dai->id == IMX_DAI_SSI1) {
		__raw_writew(stcr, SSI1_STCR);
		__raw_writew(0, SSI1_STCCR);
		__raw_writew(srcr, SSI1_SRCR);
		__raw_writew(0, SSI1_SRCCR);
	} else {
		__raw_writew(stcr, SSI2_STCR);
		__raw_writew(0, SSI2_STCCR);
		__raw_writew(srcr, SSI2_SRCR);
		__raw_writew(0, SSI2_SRCCR);
	}

	return 0;
}

static int imx_ssi_set_dai_tristate(struct snd_soc_cpu_dai *cpu_dai,
	int tristate)
{
	// via GPIO ??
	return 0;
}

static int imx_ssi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;

	if (cpu_dai->id == IMX_DAI_SSI1)
		mxc_clks_enable(SSI1_BAUD);
	else
		mxc_clks_enable(SSI2_BAUD);
	return 0;
}

static int imx_ssi_hw_tx_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	u32 stccr, stcr;

	if (cpu_dai->id == IMX_DAI_SSI1) {
		stccr = __raw_readw(SSI1_STCCR) & 0x600ff;
		stcr = __raw_readw(SSI1_STCR);
	} else {
		stccr = __raw_readw(SSI2_STCCR) & 0x600ff;
		stcr = __raw_readw(SSI2_STCR);
	}

	/* DAI data (word) size */
	switch(params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		stccr |= SSI_STCCR_WL(16);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		stccr |= SSI_STCCR_WL(20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		stccr |= SSI_STCCR_WL(24);
		break;
	}

	/* TDM - todo, only fifo 0 atm */
	stcr |= SSI_STCR_TFEN0;
	stccr |= SSI_STCCR_DC(params_channels(params));

	if (cpu_dai->id == IMX_DAI_SSI1) {
		__raw_writew(stcr, SSI1_STCR);
		__raw_writew(stccr, SSI1_STCCR);
	} else {
		__raw_writew(stcr, SSI2_STCR);
		__raw_writew(stccr, SSI2_STCCR);
	}

	return 0;
}

static int imx_ssi_hw_rx_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	u32 srccr, srcr;

	if (cpu_dai->id == IMX_DAI_SSI1) {
		srccr = __raw_readw(SSI1_SRCCR) & 0x600ff;
		srcr = __raw_readw(SSI1_SRCR);
	} else {
		srccr = __raw_readw(SSI2_SRCCR) & 0x600ff;
		srcr = __raw_readw(SSI2_SRCR);
	}

	/* DAI data (word) size */
	switch(params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		srccr |= SSI_SRCCR_WL(16);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		srccr |= SSI_SRCCR_WL(20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		srccr |= SSI_SRCCR_WL(24);
		break;
	}

	/* TDM - todo, only fifo 0 atm */
	srcr |= SSI_SRCR_RFEN0;
	srccr |= SSI_SRCCR_DC(params_channels(params));

	if (cpu_dai->id == IMX_DAI_SSI1) {
		__raw_writew(srcr, SSI1_SRCR);
		__raw_writew(srccr, SSI1_SRCCR);
	} else {
		__raw_writew(srcr, SSI2_SRCR);
		__raw_writew(srccr, SSI2_SRCCR);
	}
	return 0;
}

static int imx_ssi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;

	/* Tx/Rx config */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (cpu_dai->id == IMX_DAI_SSI1)
			cpu_dai->dma_data = &imx_ssi1_pcm_stereo_out;
		else
			cpu_dai->dma_data = &imx_ssi2_pcm_stereo_out;
		return imx_ssi_hw_tx_params(substream, params);
	} else {
		if (cpu_dai->id == IMX_DAI_SSI1)
			cpu_dai->dma_data = &imx_ssi1_pcm_stereo_in;
		else
			cpu_dai->dma_data = &imx_ssi2_pcm_stereo_in;
		return imx_ssi_hw_rx_params(substream, params);
	}
}

static int imx_ssi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	u32 scr, sier;

	if (cpu_dai->id == IMX_DAI_SSI1) {
		scr = __raw_readw(SSI1_SCR) & 0x600ff;
		sier = __raw_readw(SSI1_SIER);
	} else {
		scr = __raw_readw(SSI2_SCR) & 0x600ff;
		sier = __raw_readw(SSI2_SIER);
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			scr |= SSI_SCR_TE;
			sier |= SSI_SIER_TDMAE;
		} else {
			scr |= SSI_SCR_RE;
			sier |= SSI_SIER_RDMAE;
		}
		scr |= SSI_SCR_SSIEN;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			scr |= SSI_SCR_TE;
		else
			scr |= SSI_SCR_RE;
		scr |= SSI_SCR_SSIEN;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			sier |= SSI_SIER_TDMAE;
		else
			sier |= SSI_SIER_RDMAE;
	break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		scr &= ~SSI_SCR_SSIEN;
	case SNDRV_PCM_TRIGGER_STOP:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			scr &= ~SSI_SCR_TE;
		else
			scr &= ~SSI_SCR_RE;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			sier &= ~SSI_SIER_TDMAE;
		else
			sier &= ~SSI_SIER_TDMAE;
	break;
	default:
		return -EINVAL;
	}

	if (cpu_dai->id == IMX_DAI_SSI1) {
		__raw_writew(scr, SSI1_SCR);
		__raw_writew(sier, SSI1_SIER);
	} else {
		__raw_writew(scr, SSI2_SCR);
		__raw_writew(sier, SSI2_SIER);
	}

	return 0;
}

static void imx_ssi_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;

	/* shutdown SSI */
	if (!cpu_dai->active) {
		if(cpu_dai->id == IMX_DAI_SSI1) {
			__raw_writew(__raw_readw(SSI1_SCR) & ~SSI_SCR_SSIEN, SSI1_SCR);
			mxc_clks_disable(SSI1_BAUD);
		} else {
			__raw_writew(__raw_readw(SSI2_SCR) & ~SSI_SCR_SSIEN, SSI2_SCR);
			mxc_clks_disable(SSI2_BAUD);
		}
	}
}

#ifdef CONFIG_PM
static int imx_ssi_suspend(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{
	if(!dai->active)
		return 0;

	// do we need to disable any clocks

	return 0;
}

static int imx_ssi_resume(struct platform_device *pdev,
	struct snd_soc_cpu_dai *dai)
{
	if(!dai->active)
		return 0;

	// do we need to enable any clocks
	return 0;
}

#else
#define imx_ssi_suspend	NULL
#define imx_ssi_resume	NULL
#endif

#define IMX_SSI_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_96000)

#define IMX_SSI_BITS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

struct snd_soc_cpu_dai imx_ssi_pcm_dai[] = {
{
	.name = "imx-i2s-1",
	.id = IMX_DAI_SSI1,
	.type = SND_SOC_DAI_I2S,
	.suspend = imx_ssi_suspend,
	.resume = imx_ssi_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.formats = IMX_SSI_BITS,
		.rates = IMX_SSI_RATES,},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.formats = IMX_SSI_BITS,
		.rates = IMX_SSI_RATES,},
	.ops = {
		.startup = imx_ssi_startup,
		.shutdown = imx_ssi_shutdown,
		.trigger = imx_ssi_trigger,
		.hw_params = imx_ssi_hw_params,},
	.dai_ops = {
		.set_sysclk = imx_ssi_set_dai_sysclk,
		.set_clkdiv = imx_ssi_set_dai_clkdiv,
		.set_fmt = imx_ssi_set_dai_fmt,
		.set_tdm_slot = imx_ssi_set_dai_tdm_slot,
		.set_tristate = imx_ssi_set_dai_tristate,
	},
},
{
	.name = "imx-i2s-2",
	.id = IMX_DAI_SSI2,
	.type = SND_SOC_DAI_I2S,
	.suspend = imx_ssi_suspend,
	.resume = imx_ssi_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.formats = IMX_SSI_BITS,
		.rates = IMX_SSI_RATES,},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.formats = IMX_SSI_BITS,
		.rates = IMX_SSI_RATES,},
	.ops = {
		.startup = imx_ssi_startup,
		.shutdown = imx_ssi_shutdown,
		.trigger = imx_ssi_trigger,
		.hw_params = imx_ssi_hw_params,},
	.dai_ops = {
		.set_sysclk = imx_ssi_set_dai_sysclk,
		.set_clkdiv = imx_ssi_set_dai_clkdiv,
		.set_fmt = imx_ssi_set_dai_fmt,
		.set_tdm_slot = imx_ssi_set_dai_tdm_slot,
		.set_tristate = imx_ssi_set_dai_tristate,
	},
},};
EXPORT_SYMBOL_GPL(imx_ssi_pcm_dai);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("i.MX ASoC I2S driver");
MODULE_LICENSE("GPL");
