/*
 * imx31-pcm.c -- ALSA SoC interface for the Freescale i.MX CPU's
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
 * TODO:
 *  Refactor for new SDMA API when Freescale have it ready 
 *  (I am assuming the SDMA iapi needs rework for mainline).
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

#include "imx31-pcm.h"

/* debug */
#define IMX_PCM_DEBUG 0
#if IMX_PCM_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

/*
 * Coherent DMA memory is used by default, although Freescale have used 
 * bounce buffers in all their drivers for i.MX31 to date. If you have any 
 * issues, please select bounce buffers. 
 */
#define IMX31_DMA_BOUNCE 0

static const struct snd_pcm_hardware imx31_pcm_hardware = {
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
	struct imx31_pcm_dma_param *dma_params;
	spinlock_t dma_lock;
	int active, period, periods;
	int dma_wchannel;
	int dma_active;
	int old_offset;
	int dma_alloc;
};

static void audio_stop_dma(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned long flags;
#if IMX31_DMA_BOUNCE
	unsigned int dma_size;
	unsigned int offset;
	
	dma_size = frames_to_bytes(runtime, runtime->period_size);
	offset = dma_size * prtd->periods;
#endif

	/* stops the dma channel and clears the buffer ptrs */
	spin_lock_irqsave(&prtd->dma_lock, flags);
	prtd->active = 0;
	prtd->period = 0;
	prtd->periods = 0;
	mxc_dma_stop(prtd->dma_wchannel);

#if IMX31_DMA_BOUNCE
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
					DMA_TO_DEVICE);
	else
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
					DMA_FROM_DEVICE);
#endif	
	spin_unlock_irqrestore(&prtd->dma_lock, flags);
}

static int dma_new_period(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime =  substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int dma_size = frames_to_bytes(runtime, runtime->period_size);
	unsigned int offset = dma_size * prtd->period;
	int ret = 0;
	dma_request_t sdma_request;
	
	if (!prtd->active) 
		return 0;
		
	memset(&sdma_request, 0, sizeof(dma_request_t));
	
	dbg("period pos  ALSA %x DMA %x\n",runtime->periods, prtd->period);
	dbg("period size ALSA %x DMA %x Offset %x dmasize %x\n",
		(unsigned int) runtime->period_size, runtime->dma_bytes, 
			offset, dma_size);
	dbg("DMA addr %x\n", runtime->dma_addr + offset);
	
#if IMX31_DMA_BOUNCE
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		sdma_request.sourceAddr = (char*)(dma_map_single(NULL,
			runtime->dma_area + offset, dma_size, DMA_TO_DEVICE));
	else
		sdma_request.destAddr = (char*)(dma_map_single(NULL,
			runtime->dma_area + offset, dma_size, DMA_FROM_DEVICE));	
#else
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		sdma_request.sourceAddr = (char*)(runtime->dma_addr + offset);
	else
		sdma_request.destAddr = (char*)(runtime->dma_addr + offset);
#endif
	sdma_request.count = dma_size;

	ret = mxc_dma_set_config(prtd->dma_wchannel, &sdma_request, 0);
	if (ret < 0) {
		printk(KERN_ERR "imx31-pcm: cannot configure audio DMA channel\n");
		goto out;
	}
	
	ret = mxc_dma_start(prtd->dma_wchannel);
	if (ret < 0) {
		printk(KERN_ERR "imx31-pcm: cannot queue audio DMA buffer\n");
		goto out;
	}
	prtd->dma_active = 1;
	prtd->period++;
	prtd->period %= runtime->periods;
out:
	return ret;
}

static void audio_dma_irq(void *data)
{
	struct snd_pcm_substream *substream = 
		(struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
#if IMX31_DMA_BOUNCE
	unsigned int dma_size = frames_to_bytes(runtime, runtime->period_size);
	unsigned int offset = dma_size * prtd->periods;
#endif

	prtd->dma_active = 0;
	prtd->periods++;
	prtd->periods %= runtime->periods;

	dbg("irq per %d offset %x\n", prtd->periods, offset);
#if IMX31_DMA_BOUNCE
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_TO_DEVICE);
	else
		dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_FROM_DEVICE);
						
#endif
	
 	if (prtd->active)
		snd_pcm_period_elapsed(substream);
	dma_new_period(substream);
}

static int imx31_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime =  substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;

	prtd->period = 0;
	prtd->periods = 0;
	return 0;
}

static int imx31_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_pcm_dma_params *dma = rtd->dai->cpu_dai->dma_data;
	int ret = 0, channel = 0;
	
	/* only allocate the DMA chn once */
	if (!prtd->dma_alloc) {
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			ret  = mxc_request_dma(&channel, "ALSA TX SDMA");
			if (ret < 0) {
				printk(KERN_ERR "imx31-pcm: error requesting a write dma channel\n");
				return ret;
			}
	
		} else {
			ret = mxc_request_dma(&channel, "ALSA RX SDMA");
			if (ret < 0) {
				printk(KERN_ERR "imx31-pcm: error requesting a read dma channel\n");
				return ret;
			}
		}
		prtd->dma_wchannel = channel;
		prtd->dma_alloc = 1;
		
		/* set up chn with params */
		dma->params.callback = audio_dma_irq;
		dma->params.arg = substream;
	}
	
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dma->params.word_size = TRANSFER_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		dma->params.word_size = TRANSFER_24BIT;
		break;
	}
	
	ret = mxc_dma_setup_channel(channel, &dma->params);
	if (ret < 0) {
		printk(KERN_ERR "imx31-pcm: failed to setup audio DMA chn %d\n", channel);
		mxc_free_dma(channel);
		return ret;
	}
	
#if IMX31_DMA_BOUNCE
	ret = snd_pcm_lib_malloc_pages(substream, 
		params_buffer_bytes(params));
	if (ret < 0) {
		printk(KERN_ERR "imx31-pcm: failed to malloc pcm pages\n");
		if (channel)
			mxc_free_dma(channel);
		return ret;
	}
	runtime->dma_addr = virt_to_phys(runtime->dma_area);
#else
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
#endif
	return ret;
}

static int imx31_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	
	if (prtd->dma_wchannel) {
		mxc_free_dma(prtd->dma_wchannel);
		prtd->dma_wchannel = 0;
		prtd->dma_alloc = 0;
	}
#if IMX31_DMA_BOUNCE
	snd_pcm_lib_free_pages(substream);
#endif
	return 0;
}

static int imx31_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct mxc_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->dma_active = 0;
		/* requested stream startup */
		prtd->active = 1;
		ret = dma_new_period(substream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* requested stream shutdown */
		audio_stop_dma(substream);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		prtd->active = 0;
		prtd->periods = 0;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		prtd->active = 1;
		prtd->dma_active = 0;
		ret = dma_new_period(substream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		prtd->active = 0;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		prtd->active = 1;
		if (prtd->old_offset) {
			prtd->dma_active = 0;
			ret = dma_new_period(substream);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t imx31_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int offset = 0;

	/* is a transfer active ? */
	if (prtd->dma_active){
		offset = (runtime->period_size * (prtd->periods)) +
						(runtime->period_size >> 1);
		if (offset >= runtime->buffer_size)
			offset = runtime->period_size >> 1;
	} else {
		offset = (runtime->period_size * (prtd->periods));
		if (offset >= runtime->buffer_size)
			offset = 0;
	}
	dbg("pointer offset %x\n", offset);
	
	return offset;
}


static int imx31_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd;
	int ret;

	snd_soc_set_runtime_hwparams(substream, &imx31_pcm_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime, 
		SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	prtd = kzalloc(sizeof(struct mxc_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	runtime->private_data = prtd;
	return 0;
}

static int imx31_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	
	kfree(prtd);
	return 0;
}

static int
imx31_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

struct snd_pcm_ops imx31_pcm_ops = {
	.open		= imx31_pcm_open,
	.close		= imx31_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= imx31_pcm_hw_params,
	.hw_free	= imx31_pcm_hw_free,
	.prepare	= imx31_pcm_prepare,
	.trigger	= imx31_pcm_trigger,
	.pointer	= imx31_pcm_pointer,
	.mmap		= imx31_pcm_mmap,
};

static int imx31_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = imx31_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
		
	buf->bytes = size;
	return 0;
}

static void imx31_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static u64 imx31_pcm_dmamask = 0xffffffff;

int imx31_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &imx31_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;
#if IMX31_DMA_BOUNCE
	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, 
				SNDRV_DMA_TYPE_CONTINUOUS,
				snd_dma_continuous_data(GFP_KERNEL),
				imx31_pcm_hardware.buffer_bytes_max * 2, 
				imx31_pcm_hardware.buffer_bytes_max * 2);
	if (ret < 0) {
		printk(KERN_ERR "imx31-pcm: failed to preallocate pages\n");
		goto out;
	}	
#else
	if (dai->playback.channels_min) {
		ret = imx31_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->capture.channels_min) {
		ret = imx31_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
#endif
 out:
	return ret;
}

struct snd_soc_platform imx31_soc_platform = {
	.name		= "imx31-audio",
	.pcm_ops 	= &imx31_pcm_ops,
	.pcm_new	= imx31_pcm_new,
#if IMX31_DMA_BOUNCE
	.pcm_free	= NULL,
#else
	.pcm_free	= imx31_pcm_free_dma_buffers,
#endif
};
EXPORT_SYMBOL_GPL(imx31_soc_platform);

MODULE_AUTHOR("Liam Girdwood");
MODULE_DESCRIPTION("Freescale i.MX31 PCM DMA module");
MODULE_LICENSE("GPL");
