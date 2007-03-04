/*
 * linux/sound/arm/mxc-pcm.c -- ALSA SoC interface for the Freescale i.MX CPU's
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * Based on pxa2xx-pcm.c by	Nicolas Pitre, (C) 2004 MontaVista Software, Inc.
 * and on mxc-alsa-mc13783 (C) 2006 Freescale.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <asm/dma.h>
#include <asm/hardware.h>

#include "imx-pcm.h"

/* debug */
#define IMX_DEBUG 0
#if IMX_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

static const struct snd_pcm_hardware mxc_pcm_hardware = {
	.info			= (SNDRV_PCM_INFO_INTERLEAVED |
				   SNDRV_PCM_INFO_BLOCK_TRANSFER |
				   SNDRV_PCM_INFO_MMAP |
				   SNDRV_PCM_INFO_MMAP_VALID |
				   SNDRV_PCM_INFO_PAUSE |
				   SNDRV_PCM_INFO_RESUME),
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE,
	.buffer_bytes_max	= 32 * 1024,
	.period_bytes_min	= 64,
	.period_bytes_max	= 8 * 1024,
	.periods_min		= 2,
	.periods_max		= 255,
	.fifo_size		= 0,
};

struct mxc_runtime_data {
	int dma_ch;
	struct mxc_pcm_dma_param *dma_params;
};

/*!
  * This function stops the current dma transfert for playback
  * and clears the dma pointers.
  *
  * @param	substream	pointer to the structure of the current stream.
  *
  */
static void audio_stop_dma(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int dma_size = frames_to_bytes(runtime, runtime->period_size);
	unsigned int offset  dma_size * s->periods;
	unsigned long flags;

	spin_lock_irqsave(&prtd->dma_lock, flags);

	dbg("MXC : audio_stop_dma active = 0\n");
	prtd->active = 0;
	prtd->period = 0;
	prtd->periods = 0;

	/* this stops the dma channel and clears the buffer ptrs */
	mxc_dma_stop(prtd->dma_wchannel);
	if(substream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_TO_DEVICE);
	else
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_FROM_DEVICE);

	spin_unlock_irqrestore(&prtd->dma_lock, flags);
}

/*!
  * This function is called whenever a new audio block needs to be
  * transferred to mc13783. The function receives the address and the size
  * of the new block and start a new DMA transfer.
  *
  * @param	substream	pointer to the structure of the current stream.
  *
  */
static int dma_new_period(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime =  substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int dma_size;
	unsigned int offset;
	int ret=0;
	dma_request_t sdma_request;

	if (prtd->active){
		memset(&sdma_request, 0, sizeof(dma_request_t));
		dma_size = frames_to_bytes(runtime, runtime->period_size);
	    dbg("s->period (%x) runtime->periods (%d)\n",
			s->period,runtime->periods);
		dbg("runtime->period_size (%d) dma_size (%d)\n",
			(unsigned int)runtime->period_size,
			runtime->dma_bytes);

       	offset = dma_size * prtd->period;
		snd_assert(dma_size <= DMA_BUF_SIZE, );
		if(substream == SNDRV_PCM_STREAM_PLAYBACK)
			sdma_request.sourceAddr = (char*)(dma_map_single(NULL,
				runtime->dma_area + offset, dma_size, DMA_TO_DEVICE));
		else
			sdma_request.destAddr = (char*)(dma_map_single(NULL,
				runtime->dma_area + offset, dma_size, DMA_FROM_DEVICE));
		sdma_request.count = dma_size;

		dbg("MXC: Start DMA offset (%d) size (%d)\n", offset,
						 runtime->dma_bytes);

       	mxc_dma_set_config(prtd->dma_wchannel, &sdma_request, 0);
		if((ret = mxc_dma_start(prtd->dma_wchannel)) < 0) {
			dbg("audio_process_dma: cannot queue DMA buffer\
							(%i)\n", ret);
			return err;
		}
		prtd->tx_spin = 1; /* FGA little trick to retrieve DMA pos */
		prtd->period++;
		prtd->period %= runtime->periods;
    }
	return ret;
}


/*!
  * This is a callback which will be called
  * when a TX transfer finishes. The call occurs
  * in interrupt context.
  *
  * @param	dat	pointer to the structure of the current stream.
  *
  */
static void audio_dma_irq(void *data)
{
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	struct mxc_runtime_data *prtd;
	unsigned int dma_size;
	unsigned int previous_period;
	unsigned int offset;

	substream = data;
	runtime = substream->runtime;
	prtd = runtime->private_data;
	previous_period  = prtd->periods;
	dma_size = frames_to_bytes(runtime, runtime->period_size);
	offset = dma_size * previous_period;

	prtd->tx_spin = 0;
	prtd->periods++;
	prtd->periods %= runtime->periods;

	/*
	  * Give back to the CPU the access to the non cached memory
	  */
	if(substream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_TO_DEVICE);
	else
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_FROM_DEVICE);
	/*
	  * If we are getting a callback for an active stream then we inform
	  * the PCM middle layer we've finished a period
	  */
 	if (prtd->active)
		snd_pcm_period_elapsed(substream);

	/*
	  * Trig next DMA transfer
	  */
	dma_new_period(substream);
}

/*!
  * This function configures the hardware to allow audio
  * playback operations. It is called by ALSA framework.
  *
  * @param	substream	pointer to the structure of the current stream.
  *
  * @return              0 on success, -1 otherwise.
  */
static int
snd_mxc_prepare(struct snd_pcm_substream *substream)
{
	struct mxc_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0;
	prtd->period = 0;
	prtd->periods = 0;

	dma_channel_params params;
	int channel = 0; // passed in ?

	if ((ret  = mxc_request_dma(&channel, "ALSA TX SDMA") < 0)){
		dbg("error requesting a write dma channel\n");
		return ret;
	}

	/* configure DMA params */
	memset(&params, 0, sizeof(dma_channel_params));
	params.bd_number = 1;
	params.arg = s;
	params.callback = callback;
	params.transfer_type = emi_2_per;
	params.watermark_level = SDMA_TXFIFO_WATERMARK;
	params.word_size = TRANSFER_16BIT;
	//dbg(KERN_ERR "activating connection SSI1 - SDMA\n");
	params.per_address = SSI1_BASE_ADDR;
	params.event_id = DMA_REQ_SSI1_TX1;
	params.peripheral_type = SSI;

	/* set up chn with params */
	mxc_dma_setup_channel(channel, &params);
	s->dma_wchannel = channel;

	return ret;
}

static int mxc_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	if((ret=snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params))) < 0)
		return ret;
	runtime->dma_addr = virt_to_phys(runtime->dma_area);

	return ret;
}

static int mxc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int mxc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct mxc_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->tx_spin = 0;
		/* requested stream startup */
		prtd->active = 1;
        ret = dma_new_period(substream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* requested stream shutdown */
		ret = audio_stop_dma(substream);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		prtd->active = 0;
		prtd->periods = 0;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		prtd->active = 1;
		prtd->tx_spin = 0;
		ret = dma_new_period(substream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		prtd->active = 0;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		prtd->active = 1;
		if (prtd->old_offset) {
			prtd->tx_spin = 0;
            ret = dma_new_period(substream);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t mxc_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int offset = 0;

	/* tx_spin value is used here to check if a transfert is active */
	if (prtd->tx_spin){
		offset = (runtime->period_size * (prtd->periods)) +
						(runtime->period_size >> 1);
		if (offset >= runtime->buffer_size)
			offset = runtime->period_size >> 1;
	} else {
		offset = (runtime->period_size * (s->periods));
		if (offset >= runtime->buffer_size)
			offset = 0;
	}

	return offset;
}


static int mxc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd;
	int ret;

	snd_soc_set_runtime_hwparams(substream, &mxc_pcm_hardware);

	if ((err = snd_pcm_hw_constraint_integer(runtime,
					SNDRV_PCM_HW_PARAM_PERIODS)) < 0)
		return err;
	if ((err = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &hw_playback_rates)) < 0)
		return err;
	msleep(10); // liam - why

	/* setup DMA controller for playback */
	if((err = configure_write_channel(&mxc_mc13783->s[SNDRV_PCM_STREAM_PLAYBACK],
					audio_dma_irq)) < 0 )
		return err;

	if((prtd = kzalloc(sizeof(struct mxc_runtime_data), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	runtime->private_data = prtd;
	return 0;

 err1:
	kfree(prtd);
 out:
	return ret;
}

static int mxc_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;

//	mxc_mc13783_t *chip;
	audio_stream_t *s;
	device_data_t* device;
	int ssi;

	//chip = snd_pcm_substream_chip(substream);
	s = &chip->s[substream->pstr->stream];
	device = &s->stream_device;
	ssi = device->ssi;

	//disable_stereodac();

	ssi_transmit_enable(ssi, false);
	ssi_interrupt_disable(ssi, ssi_tx_dma_interrupt_enable);
	ssi_tx_fifo_enable(ssi, ssi_fifo_0, false);
	ssi_enable(ssi, false);

	chip->s[substream->pstr->stream].stream = NULL;

	return 0;
}

static int
mxc_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

struct snd_pcm_ops mxc_pcm_ops = {
	.open		= mxc_pcm_open,
	.close		= mxc_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= mxc_pcm_hw_params,
	.hw_free	= mxc_pcm_hw_free,
	.prepare	= mxc_pcm_prepare,
	.trigger	= mxc_pcm_trigger,
	.pointer	= mxc_pcm_pointer,
	.mmap		= mxc_pcm_mmap,
};

static u64 mxc_pcm_dmamask = 0xffffffff;

int mxc_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &mxc_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (dai->playback.channels_min) {
		ret = mxc_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->capture.channels_min) {
		ret = mxc_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
 out:
	return ret;
}

struct snd_soc_platform mxc_soc_platform = {
	.name		= "mxc-audio",
	.pcm_ops 	= &mxc_pcm_ops,
	.pcm_new	= mxc_pcm_new,
	.pcm_free	= mxc_pcm_free_dma_buffers,
};

EXPORT_SYMBOL_GPL(mxc_soc_platform);

MODULE_AUTHOR("Liam Girdwood");
MODULE_DESCRIPTION("Freescale i.MX PCM DMA module");
MODULE_LICENSE("GPL");
