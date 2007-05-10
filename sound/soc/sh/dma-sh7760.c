/*
 * SH7760 ("camelot") DMABRG audio DMA unit support
 *
 * Copyright (C) 2007 Manuel Lauss <mano@roarinelk.homelinux.net>
 *  licensed under the terms outlined in the file COPYING at the root
 *  of the linux kernel sources.
 *
 * The SH7760 DMABRG provides 4 dma channels (2x rec, 2x play), which
 * trigger an interrupt when one half of the programmed transfer size
 * has been xmitted.
 *
 * TODO: * make Big-Endian safe: select correct ACR_{RAM|TAM}_* bits,
 *	 * make 20Bit safe (16 only for now)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <asm/dmabrg.h>
#include <asm/io.h>

/* #define PCM_DEBUG */

#define MSG(x...)	printk(KERN_INFO "sh7760-pcm: " x)

#ifdef PCM_DEBUG
#define DBG(x...)	MSG(x)
#else
#define DBG(x...)	do {} while (0)
#endif

/* registers and bits */
#define BRGATXSAR	0x00
#define BRGARXDAR	0x04
#define BRGATXTCR	0x08
#define BRGARXTCR	0x0C
#define BRGACR		0x10
#define BRGATXTCNT	0x14
#define BRGARXTCNT	0x18

#define ACR_RAR		(1 << 18)
#define ACR_RDS		(1 << 17)
#define ACR_RDE		(1 << 16)
#define ACR_TAR		(1 << 2)
#define ACR_TDS		(1 << 1)
#define ACR_TDE		(1 << 0)

/* receiver/transmitter data alignment */
#define ACR_RAM_NONE	(0 << 24)
#define ACR_RAM_4BYTE	(1 << 24)
#define ACR_RAM_2WORD	(2 << 24)
#define ACR_TAM_NONE	(0 << 8)
#define ACR_TAM_4BYTE	(1 << 8)
#define ACR_TAM_2WORD	(2 << 9)


struct camelot_pcm {
	unsigned long mmio;  /* DMABRG audio channel control reg MMIO */
	int txid;	     /* ID of first DMABRG IRQ for this unit */
} cam_pcm_data[2] = {
	{
		.mmio	=	0xFE3C0040,
		.txid	=	DMABRGIRQ_A0TXF,
	},
	{
		.mmio	=	0xFE3C0060,
		.txid	=	DMABRGIRQ_A1TXF,
	},
};

#define BRGREG(x)	(*(volatile unsigned long *)(cam->mmio + (x)))

/*
 * set a minimum of 16kb per period, to avoid interrupt-"storm" and
 * resulting skipping. In general, the bigger the minimum size, the
 * better for overall system performance. (The SH7760 is a puny CPU
 * with a slow SDRAM interface and poor internal bus bandwidth,
 * *especially* when the LCDC is active.). The minimum for the DMAC
 * is 4 bytes. 16kbytes is enough to get skip-free playback of
 * 44kHz/16bit/stereo MP3 on a somewhat loaded system, and maintain
 * reasonable responsiveness in e.g. MPlayer.
 */
#define DMABRG_PERIOD_MIN	16 * 1024
#define DMABRG_PERIOD_MAX	0x03fffffc
#define DMABRG_PREALLOC_BUFFER	64 * 1024
#define DMABRG_PREALLOC_BUFFER_MAX	(32 * 1024 * 1024)

/* support everything the SSI supports */
#define DMABRG_RATES	\
	SNDRV_PCM_RATE_8000_192000

#define DMABRG_FMTS	\
	(SNDRV_PCM_FMTBIT_S8      | SNDRV_PCM_FMTBIT_U8      |	\
	 SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_U16_LE  |	\
	 SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE |	\
	 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_U24_3LE |	\
	 SNDRV_PCM_FMTBIT_S32_LE  | SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_pcm_hardware camelot_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP_VALID),
	.formats =	DMABRG_FMTS,
	.rates =	DMABRG_RATES,
	.rate_min =		8000,
	.rate_max =		192000,
	.channels_min =		2,
	.channels_max =		8,		/* max of the SSI */
	.buffer_bytes_max =	DMABRG_PERIOD_MAX,
	.period_bytes_min =	DMABRG_PERIOD_MIN,
	.period_bytes_max =	DMABRG_PERIOD_MAX / 2,
	.periods_min =		2,
	.periods_max =		2,
};

static int camelot_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct camelot_pcm *cam = &cam_pcm_data[rtd->dai->cpu_dai->id];
	int recv = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0:1;
	int ret, dmairq;

	DBG("pcm_open() enter\n");

	snd_soc_set_runtime_hwparams(substream, &camelot_pcm_hardware);

	/* DMABRG buffer half/full events */
	dmairq = (recv) ? cam->txid + 2 : cam->txid;
	ret = dmabrg_request_irq(dmairq,
				 (DMABRGHANDLER)snd_pcm_period_elapsed,
				 substream);
	if (unlikely(ret)) {
		MSG("audio unit %d irqs already taken!\n",
			rtd->dai->cpu_dai->id);
		return -EBUSY;
	}
	ret = dmabrg_request_irq(dmairq + 1,
				 (DMABRGHANDLER)snd_pcm_period_elapsed,
				 substream);

	DBG("pcm_open() leave\n");

	return 0;
}

static int camelot_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct camelot_pcm *cam = &cam_pcm_data[rtd->dai->cpu_dai->id];
	int recv = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0:1;
	int dmairq;

	DBG("pcm_close() enter\n");

	dmairq = (recv) ? cam->txid + 2 : cam->txid;

	dmabrg_free_irq(dmairq);
	dmabrg_free_irq(dmairq + 1);

	DBG("pcm_close() leave\n");

	return 0;
}

static int camelot_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *hw_params)
{
	DBG("hw_params()\n");
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int camelot_hw_free(struct snd_pcm_substream *substream)
{
	DBG("hw_free()\n");
	return snd_pcm_lib_free_pages(substream);
}

static int camelot_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct camelot_pcm *cam = &cam_pcm_data[rtd->dai->cpu_dai->id];

	DBG("prepare() enter\n");
	DBG("PCM data: addr 0x%08lx len %d\n", (u32)runtime->dma_addr,
					       runtime->dma_bytes);
 
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		BRGREG(BRGATXSAR) = runtime->dma_addr;
		BRGREG(BRGATXTCR) = runtime->dma_bytes;
	} else {
		BRGREG(BRGARXDAR) = runtime->dma_addr;
		BRGREG(BRGARXTCR) = runtime->dma_bytes;
	}

	DBG("prepare() leave\n");
	return 0;
}

static inline void dmabrg_play_dma_start(struct camelot_pcm *cam)
{
	unsigned long acr = BRGREG(BRGACR) & ~(ACR_TDS | ACR_RDS);
	/* start DMABRG engine: XFER start, auto-addr-reload */
	BRGREG(BRGACR) = acr | ACR_TDE | ACR_TAR | ACR_TAM_2WORD;
}

static inline void dmabrg_play_dma_stop(struct camelot_pcm *cam)
{
	unsigned long acr = BRGREG(BRGACR) & ~(ACR_TDS | ACR_RDS);
	/* forcibly terminate data transmission */
	BRGREG(BRGACR) = acr | ACR_TDS;
}

static inline void dmabrg_rec_dma_start(struct camelot_pcm *cam)
{
	unsigned long acr = BRGREG(BRGACR) & ~(ACR_TDS | ACR_RDS);
	/* start DMABRG engine: recv start, auto-reload */
	BRGREG(BRGACR) = acr | ACR_RDE | ACR_RAR | ACR_RAM_2WORD;
}

static inline void dmabrg_rec_dma_stop(struct camelot_pcm *cam)
{
	unsigned long acr = BRGREG(BRGACR) & ~(ACR_TDS | ACR_RDS);
	/* forcibly terminate data receiver */
	BRGREG(BRGACR) = acr | ACR_RDS;
}

static int camelot_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct camelot_pcm *cam = &cam_pcm_data[rtd->dai->cpu_dai->id];
	int recv = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0:1;

	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
		if (recv)
			dmabrg_rec_dma_start(cam);
		else
			dmabrg_play_dma_start(cam);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (recv)
			dmabrg_rec_dma_stop(cam);
		else
			dmabrg_play_dma_stop(cam);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_uframes_t camelot_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct camelot_pcm *cam = &cam_pcm_data[rtd->dai->cpu_dai->id];
	int recv = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0:1;
	snd_pcm_uframes_t ret;

	if (recv)
		ret = bytes_to_frames(runtime, BRGREG(BRGARXTCNT));
	else
		ret = bytes_to_frames(runtime, BRGREG(BRGATXTCNT));

	return ret;
}

static struct snd_pcm_ops camelot_pcm_ops = {
	.open		= camelot_pcm_open,
	.close		= camelot_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= camelot_hw_params,
	.hw_free	= camelot_hw_free,
	.prepare	= camelot_prepare,
	.trigger	= camelot_trigger,
	.pointer	= camelot_pointer,
};

static void camelot_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int camelot_pcm_new(struct snd_card *card,
			   struct snd_soc_codec_dai *dai,
			   struct snd_pcm *pcm)
{
	DBG("pcm_new() enter\n");

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
	      NULL, DMABRG_PREALLOC_BUFFER, DMABRG_PREALLOC_BUFFER_MAX);

	DBG("pcm_new() leave\n");
	return 0;
}

/* au1xpsc audio platform */
struct snd_soc_platform sh7760_soc_platform = {
	.name		= "sh7760-dmabrg",
	.pcm_ops 	= &camelot_pcm_ops,
	.pcm_new	= camelot_pcm_new,
	.pcm_free	= camelot_pcm_free,
};
EXPORT_SYMBOL_GPL(sh7760_soc_platform);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SH7760 Audio DMA (DMABRG) driver");
MODULE_AUTHOR("Manuel Lauss <mano@roarinelk.homelinux.net>");
