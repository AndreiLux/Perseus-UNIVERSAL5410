/* 
 * dbdma - DBDMA for Au1200/Au1550 PSC Audio interfaces (AC97/I2S)
 *
 * Copyright (c) 2007 MSC Vertriebsges.m.b.H, <mlau@msc-ge.com>
 *
 * licensed under the GPLv2.
 *
 */

/*
 * TODO: the au1200/au1550 DBDMA API needs an overhaul or least a few
 *	 extensions:
 *	  - change STS/DTS (access size) for a descriptor ring on the fly:
 *		au1xxx_dbdma_adjust_size(chanid, int sts, int dts);
 *	  - maybe even a way to add/remove descriptor from the ring, to support
 *	    varying number of periods.
 *	  - destroy/free a ring (certainly required for the above)
 *	  - probably more I haven't thought about yet
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

#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1xxx_dbdma.h>

#include "psc.h"

/*#define PCM_DEBUG*/

#ifdef PCM_DEBUG
#define MSG(x...)	printk(KERN_INFO "au1x-pcm: " x)
#else
#define MSG(x...)	do {} while (0)
#endif

struct au1xpsc_pcm_dmadata {
	struct snd_pcm_substream *substream;
	unsigned long pos;		/* current byte position being played */
	unsigned long periods;		/* number of SG segments in total */
	unsigned long curr_period;	/* current segment DDMA is working on */
	unsigned long period_bytes;	/* size in bytes of one SG segment */
};

static struct au1xpsc_pcm {
	unsigned long pscbase;
	unsigned int	dmatx;
	unsigned int	dmarx;

	u32		tx_chan;
	u32		rx_chan;
	struct au1xpsc_pcm_dmadata dma_play;
	struct au1xpsc_pcm_dmadata dma_rec;
} au1xpsc_pcm_data[PSC_COUNT] = {
	{
		.pscbase	= PSC0_BASE,
		.dmatx		= DSCR_CMD0_PSC0_TX,
		.dmarx		= DSCR_CMD0_PSC0_RX,
	},
	{
		.pscbase	= PSC1_BASE,
		.dmatx		= DSCR_CMD0_PSC1_TX,
		.dmarx		= DSCR_CMD0_PSC1_RX,
	},
#ifdef CONFIG_SOC_AU1550
	{
		.pscbase	= PSC2_BASE,
		.dmatx		= DSCR_CMD0_PSC2_TX,
		.dmarx		= DSCR_CMD0_PSC2_RX,
	},
	{
		.pscbase	= PSC3_BASE,
		.dmatx		= DSCR_CMD0_PSC3_TX,
		.dmarx		= DSCR_CMD0_PSC3_RX,
	},
#endif
};


/*
 * These settings are somewhat okay, at least on my machine audio plays almost
 * skip-free. Especially the 64kB buffer seems to help a LOT.
 */
#define AU1XPSC_PERIOD_MIN_BYTES	1024
#define AU1XPSC_BUFFER_MIN_BYTES	65536
#define AU1XPSC_PERIODS_MIN		4

/* PCM hardware DMA capabilities - platform specific */
static const struct snd_pcm_hardware au1xpsc_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
				/* later also S24 would be possible
				  SNDRV_PCM_FMTBIT_S24_LE, */
	.period_bytes_min	= AU1XPSC_PERIOD_MIN_BYTES,
	.period_bytes_max	= 4096 * 1024 - 1,
	.periods_min		= AU1XPSC_PERIODS_MIN,
	.periods_max		= AU1XPSC_PERIODS_MIN,
	.buffer_bytes_max	= 4096 * 1024 - 1,
	.fifo_size		= 16,	/* fifo entries of AC97/I2S PSC */
};

/* one descriptor completed, update dma data */
static inline void dma_update(struct au1xpsc_pcm_dmadata *dd)
{
	dd->curr_period++;
	if (dd->curr_period >= dd->periods) {
		/* if the NOCV flag is set, DBDMA should start with the
		  first descriptor again. Wrap around here too */
		dd->pos = 0;
		dd->curr_period = 0;
	} else
		dd->pos += dd->period_bytes;
}

static void au1x_dbdma_tx_callback(int irq, void *dev_id)
{
	struct au1xpsc_pcm *psc = dev_id;
	dma_update(&psc->dma_play);
	snd_pcm_period_elapsed(psc->dma_play.substream);
}

static void au1x_dbdma_rx_callback(int irq, void *dev_id)
{
	struct au1xpsc_pcm *psc = dev_id;
	dma_update(&psc->dma_rec);
	snd_pcm_period_elapsed(psc->dma_rec.substream);
}

/*
 * Called by ALSA when the hardware params are set by application. This
 * function can also be called multiple times and can allocate buffers
 * (using snd_pcm_lib_* ). It's non-atomic.
 */
static int au1xpsc_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct au1xpsc_pcm *psc = &au1xpsc_pcm_data[rtd->dai->cpu_dai->id];
	struct au1xpsc_pcm_dmadata *dd;
	int dir, len, segs, i, ret;

	MSG("pcm_hw_params() enter\n");

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0)
		goto out;

	dir  = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 1;
	segs = params_periods(params);
	len = params_period_bytes(params);

	dd = (dir) ? &psc->dma_rec : &psc->dma_play;

	/* invalidate whole DBDMA ring */
	au1xxx_dbdma_stop(dir ? psc->rx_chan : psc->tx_chan);
	au1xxx_dbdma_reset(dir ? psc->rx_chan : psc->tx_chan);

	MSG("runtime->dma_area = 0x%08lx dma_addr_t = 0x%08lx dma_size = %d\n",
		(unsigned long)runtime->dma_area,
		(unsigned long)runtime->dma_addr, runtime->dma_bytes);

	for (i = 0; i < segs; i++) {
		if (dir == 0) {
			ret = au1xxx_dbdma_put_source_flags(psc->tx_chan,
				(void *)(unsigned long)runtime->dma_addr + (i * len),
				len,
				DDMA_FLAGS_IE | DDMA_FLAGS_NOCV);
		} else {
			ret = au1xxx_dbdma_put_dest_flags(psc->rx_chan,
				(void *)(unsigned long)runtime->dma_addr + (i * len),
				len,
				DDMA_FLAGS_IE | DDMA_FLAGS_NOCV);
		}
		if (!ret) {
			printk(KERN_INFO "au1x-pcm: error adding DMA data\n");
			ret = -EINVAL;
			goto out;
		}
	}

#ifdef PCM_DEBUG
	au1xxx_dbdma_dump(dir ? psc->rx_chan : psc->tx_chan);
#endif
	dd->pos = 0;
	dd->substream = substream;
	dd->period_bytes = len;
	dd->periods = segs;
	ret = 0;
out:
	MSG("pcm_hw_params() leave %d\n", ret);
	return ret;
}

/*
 * Free's resources allocated by hw_params, can be called multiple times
 */
static int au1xpsc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct au1xpsc_pcm *psc = &au1xpsc_pcm_data[rtd->dai->cpu_dai->id];
	struct au1xpsc_pcm_dmadata *dd;

	MSG("pcm_hw_free() enter\n");

	dd = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? &psc->dma_play
							      : &psc->dma_rec;

	dd->substream = NULL;
	dd->pos = 0;
	dd->periods = 0;
	dd->period_bytes = 0;

	snd_pcm_lib_free_pages(substream);

	MSG("pcm_hw_free() leave\n");
	return 0;
}

/*
 * Called by ALSA when the PCM substream is prepared, can set format, sample
 * rate, etc.  This function is non atomic and can be called multiple times,
 * it can refer to the runtime info.
 */
static int au1xpsc_pcm_prepare(struct snd_pcm_substream *substream)
{
	MSG("prepare() enter\n");
	MSG("prepare() leave\n");
	return 0;
}

/*
 * Starts (Triggers) audio playback or capture.
 * Usually only needed for DMA
 */
static int au1xpsc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct au1xpsc_pcm *psc = &au1xpsc_pcm_data[rtd->dai->cpu_dai->id];
	u32 dmachan;

	dmachan = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			? psc->tx_chan : psc->rx_chan;
	
	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
		au1xxx_dbdma_start(dmachan);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		au1xxx_dbdma_stop(dmachan);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Returns the DMA audio frame position
 */
static snd_pcm_uframes_t
au1xpsc_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct au1xpsc_pcm *psc = &au1xpsc_pcm_data[rtd->dai->cpu_dai->id];
	int dir;
	snd_pcm_uframes_t ret;
	unsigned long off;

	dir  = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 1;
	if (dir != 0) {
		off = psc->dma_rec.pos;
	} else {
		off = psc->dma_play.pos;
	}
	ret = bytes_to_frames(runtime, off);

	return ret;
}

 /*
 * Called by ALSA when a PCM substream is opened, private data can be allocated.
 */
static int au1xpsc_pcm_open(struct snd_pcm_substream *substream)
{
	MSG("pcm_open() enter\n");
	
	snd_soc_set_runtime_hwparams(substream, &au1xpsc_pcm_hardware);

	MSG("pcm_open() leave\n");
	return 0;
}

/*
 * Called by ALSA when a PCM substream is closed. Private data can be
 * freed here.
 */
static int au1xpsc_pcm_close(struct snd_pcm_substream *substream)
{
	MSG("pcm_close() enter\n");
	
	MSG("pcm_close() leave\n");
	return 0;
}

/* ALSA PCM operations */
struct snd_pcm_ops au1xpsc_pcm_ops = {
	.open		= au1xpsc_pcm_open,
	.close		= au1xpsc_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= au1xpsc_pcm_hw_params,
	.hw_free	= au1xpsc_pcm_hw_free,
	.prepare	= au1xpsc_pcm_prepare,
	.trigger	= au1xpsc_pcm_trigger,
	.pointer	= au1xpsc_pcm_pointer,
};

/*
 * Called by ASoC core to free platform DMA.
 */
static void au1xpsc_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	MSG("pcm_free_dma() enter\n");	
	snd_pcm_lib_preallocate_free_for_all(pcm);
	MSG("pcm_free_dma() leave\n");
}

static dbdev_tab_t au1xpsc_mem_dbdev =
{
	DSCR_CMD0_ALWAYS, DEV_FLAGS_ANYUSE, 0, 16, 0x00000000, 0, 0
};

/*
 * Called by the ASoC core to create and initialise the platform DMA.
 */
static int au1xpsc_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int memid;
	struct au1xpsc_pcm *psc = &au1xpsc_pcm_data[1];	/* HACK!!!! */

	/*
	struct platform_device *pdev = to_platform_device(card->dev);
	struct au1xpsc_pcm *psc = platform_get_drvdata(pdev);
	*/
	MSG("pcm_new() enter\n");
	MSG("psc_base 0x%08lx\n", psc->pscbase);

/* FIXME: THIS STUFF SHOULD BE MOVED TO pcm_open(), and the whole DMA
	 descriptor ring torn down again in pcm_close(). This is necessary to
	 support different amounts of periods and/or sampledepths!!
*/

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV, NULL,
		AU1XPSC_BUFFER_MIN_BYTES, (4096 * 1024) - 1);

	/* allocate DBDMA channels */
	memid = au1xxx_ddma_add_device(&au1xpsc_mem_dbdev);

	psc->tx_chan = au1xxx_dbdma_chan_alloc(memid, psc->dmatx,
					 au1x_dbdma_tx_callback, (void *)psc);

	psc->rx_chan = au1xxx_dbdma_chan_alloc(psc->dmarx, memid,
					 au1x_dbdma_rx_callback, (void *)psc);

	/* this needs to be adjusted for the requested bitdepth! */
	au1xxx_dbdma_set_devwidth(psc->tx_chan, 16);
	au1xxx_dbdma_set_devwidth(psc->rx_chan, 16);

	au1xxx_dbdma_ring_alloc(psc->tx_chan, AU1XPSC_PERIODS_MIN);
	au1xxx_dbdma_ring_alloc(psc->rx_chan, AU1XPSC_PERIODS_MIN);

	MSG("pcm_new() leave\n");
	return 0;
}

static int au1xpsc_pcm_probe(struct platform_device *pdev)
{
	MSG("probe() enter\n");
	MSG("probe() leave\n");
	return 0;
}

static int au1xpsc_pcm_remove(struct platform_device *pdev)
{
	MSG("remove() enter\n");	
	MSG("remove() leave\n");
	return 0;
}

/* au1xpsc audio platform */
struct snd_soc_platform au1xpsc_soc_platform = {
	.name		= "au1xpsc-pcm-dbdma",
	.probe		= au1xpsc_pcm_probe,
	.remove		= au1xpsc_pcm_remove,
	.pcm_ops 	= &au1xpsc_pcm_ops,
	.pcm_new	= au1xpsc_pcm_new,
	.pcm_free	= au1xpsc_pcm_free_dma_buffers,
};
EXPORT_SYMBOL_GPL(au1xpsc_soc_platform);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Au1200/Au1550 Audio DMA driver");
MODULE_AUTHOR("Manuel Lauss <mano@roarinelk.homelinux.net>");
