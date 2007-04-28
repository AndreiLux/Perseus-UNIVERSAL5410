/*
 * au1x-ac97.c -- AC97 support for the Au1200/Au1550 PSC AC97 controller
 *
 * (c) 2007 MSC Vertriebsges.m.bH, Manuel Lauss <mlau@msc-ge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/mach-au1x00/au1000.h>

#include "psc.h"

/* #define AC97_DEBUG */

#ifdef AC97_DEBUG
#define MSG(x...)	printk(KERN_INFO "au1x-ac97: " x)
#else
#define MSG(x...)	do {} while (0)
#endif


#define AC97_RD		(1<<25)

#define AC97_DIR \
	(SND_SOC_DAIDIR_PLAYBACK | SND_SOC_DAIDIR_CAPTURE)

#define AC97_RATES \
	SNDRV_PCM_RATE_8000_48000

#define AC97_FMTS	\
	SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3BE


static unsigned long au1xpsc_base[2] = {
	PSC0_BASE,
	PSC1_BASE,
};

/* AC97 controlller reads codec register */
static unsigned short au1xpsc_ac97_read(struct snd_ac97 *ac97,
	unsigned short reg)
{
	unsigned long pscbase = au1xpsc_base[1];	/* HACK */
	unsigned short data, tmo;

	au_writel(AC97_RD | ((reg & 127) << 16), pscbase + PSC_AC97CDC);
	au_sync();

	tmo = 1000;
	while ((!(au_readl(pscbase + PSC_AC97EVNT) & (1<<24))) && --tmo)
		udelay(2);

	if (!tmo) {
		data = 0xffff;
	} else
		data = au_readl(pscbase + PSC_AC97CDC) & 0xffff;

	au_writel(1<<24, pscbase + PSC_AC97EVNT);
	au_sync();

	return data;
}

/* AC97 controller writes to codec register */
static void au1xpsc_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
	unsigned short val)
{
	unsigned long pscbase = au1xpsc_base[1];	/* HACK */
	unsigned int tmo;

	au_writel(((reg&127)<<16)|(val&0xffff), pscbase + PSC_AC97CDC);
	au_sync();
	tmo = 1000;
	while ((!(au_readl(pscbase + PSC_AC97EVNT) & (1<<24))) && --tmo)
		au_sync();

	au_writel(1<<24, pscbase + PSC_AC97EVNT);
	au_sync();
}

/* AC97 controller asserts a warm reset */
static void au1xpsc_ac97_warm_reset(struct snd_ac97 *ac97)
{
	unsigned long pscbase = au1xpsc_base[1];	/* HACK */

	au_writel(1, pscbase + PSC_AC97RST);
	au_sync();
	msleep(10);
	au_writel(0, pscbase + PSC_AC97RST);
	au_sync();
	MSG("warmreset() leave\n");
}

/* AC97 controller asserts a cold reset */
static void au1xpsc_ac97_cold_reset(struct snd_ac97 *ac97)
{
	/* cold reset can only be done if the PSC is already configured
	 * for AC97 mode; unfortunately ASoC calls cold reset BEFORE any
	 * of the probe() functions.   So for now the cold_reset() func-
	 * tionality is in the board init code,  where the PSC is also
	 * switched to AC97 mode.
	 */
}

/* AC97 controller operations */
struct snd_ac97_bus_ops soc_ac97_ops = {
	.read	= au1xpsc_ac97_read,
	.write	= au1xpsc_ac97_write,
	.warm_reset	= au1xpsc_ac97_warm_reset,
	.reset	= au1xpsc_ac97_cold_reset,
};
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static int au1xpsc_ac97_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned long pscbase = au1xpsc_base[rtd->dai->cpu_dai->id];
	unsigned long r;
	int chans, recv;

	chans = params_channels(params);
	recv = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 1;

	/* need to disable the controller before changing any other
	 *  AC97CFG reg contents
	 */
	r = au_readl(pscbase + PSC_AC97CFG);
	au_writel(r & ~(1<<26), pscbase + PSC_AC97CFG);
	au_sync();

	/* set sample bitdepth: REG[24:21]=(BITS-2)/2 */
	r &= ~(0xf << 21);
	r |= (((params->msbits-2)>>1) & 0xf) << 21;

	/* channels */
	r |= (3 << (1 + (recv ? 0 : 10)));	/* stereo pair */

	/* set FIFO params: max fifo threshold, 2 slots TX/RX  */
	r |= (3<<30) | (3<<28);

	/* finally enable the AC97 controller again */
	au_writel(r | (1<<26), pscbase + PSC_AC97CFG);
	au_sync();

	MSG("bits %d rate: %d/%d channels %d\n",  params->msbits,
		params->rate_num, params->rate_den, chans);

	return 0;
}

static int au1xpsc_ac97_trigger(struct snd_pcm_substream *substream,
				int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned long pscbase = au1xpsc_base[rtd->dai->cpu_dai->id];
	int ret, rcv;

	rcv = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 1;
	ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		au_writel(1 << (rcv ? 4 : 0), pscbase + PSC_AC97PCR);
		au_sync();
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		au_writel(1 << (rcv ? 5 : 1), pscbase + PSC_AC97PCR);
		au_sync();
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

struct snd_soc_cpu_dai au1xpsc_ac97_dai[] = {
{
	.name = "au1xpsc-ac97-0",
	.id = 0,
	.type = SND_SOC_DAI_AC97,	/* PSC0 */
	.playback = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.capture = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.ops = {
		.trigger = au1xpsc_ac97_trigger,
		.hw_params = au1xpsc_ac97_hw_params,
	},
},
{
	.name = "au1xpsc-ac97-1",	/* PSC1 */
	.id = 1,
	.type = SND_SOC_DAI_AC97,
	.playback = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.capture = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.ops = {
		.trigger = au1xpsc_ac97_trigger,
		.hw_params = au1xpsc_ac97_hw_params,
	},
},
#ifdef CONFIG_SOC_AU1550
{
	.name = "au1xpsc-ac97-2",
	.id = 2,
	.type = SND_SOC_DAI_AC97,	/* PSC2 */
	.playback = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.capture = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.ops = {
		.trigger = au1xpsc_ac97_trigger,
		.hw_params = au1xpsc_ac97_hw_params,
	},
},
{
	.name = "au1xpsc-ac97-3",	/* PSC3 */
	.id = 3,
	.type = SND_SOC_DAI_AC97,
	.playback = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.capture = {
		.rates	= AC97_RATES,
		.formats= AC97_FMTS,
		.channels_min = 2,
		.channels_max = 2,
	},
	.ops = {
		.trigger = au1xpsc_ac97_trigger,
		.hw_params = au1xpsc_ac97_hw_params,
	},
},
#endif
};
EXPORT_SYMBOL_GPL(au1xpsc_ac97_dai);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Au1200/Au1550 PSC AC97 audio driver");
MODULE_AUTHOR("Manuel Lauss <mano@roarinelk.homelinux.net>");
