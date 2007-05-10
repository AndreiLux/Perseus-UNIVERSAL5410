/*
 * au1x-i2s -- I2S sound support for Au1200/Au1550 PSC I2S controller
 *
 * Copyright (c) 2007 MSC Vertriebsges.m.b.H., <mlau@msc-ge.com>
 *
 * licensed under the GPLv2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/mach-au1x00/au1000.h>

#include "psc.h"

/* #define SSI_DEBUG */

#ifdef SSI_DEBUG
#define MSG(x...)	printk(KERN_INFO "au1x-i2s: " x)
#else
#define MSG(x...)	do {} while (0)
#endif

#define OUT32(val, reg)	writel(val, (void *)(u32)(reg))

/* supported I2S DAI hardware formats */
#define AU1XPSC_I2S_DAIFMT \
	(SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_LEFT_J |	\
	 SND_SOC_DAIFMT_NB_NF)

/* supported I2S direction */
#define AU1XPSC_I2S_DIR \
	(SND_SOC_DAIDIR_PLAYBACK | SND_SOC_DAIDIR_CAPTURE)

#define AU1XPSC_I2S_RATES \
	SNDRV_PCM_RATE_8000_192000

#define AU1XPSC_I2S_FMTS \
	(SNDRV_PCM_FMTBIT_S16_LE/* | SNDRV_PCM_FMTBIT_S24_LE*/)


static unsigned long au1xpsc_base[2] = {
	PSC0_BASE,
	PSC1_BASE,
};

static int au1xpsc_set_fmt(struct snd_soc_cpu_dai *cpu_dai, unsigned int fmt)
{
	unsigned long pscbase = au1xpsc_base[cpu_dai->id];
	int ret;
	unsigned long ct;

	MSG("set_fmt() enter\n");
	ret = -EINVAL;
	ct = au_readl(pscbase + PSC_I2SCFG);
	ct &= ~(1 << 26);
	au_writel(ct, pscbase + PSC_I2SCFG);
	au_sync();

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
	case SND_SOC_DAIFMT_I2S:
		ct |= (1<<9);			/* enable I2S mode */
		break;
	case SND_SOC_DAIFMT_MSB:
		ct &= ~((1<<9)|(1<<10));	/* justified xfer, MSb */
		break;
	case SND_SOC_DAIFMT_LSB:
		ct &= ~(1<<9);			/* justified xfer */
		ct |= (1<<10);			/* LSb */
		break;
	default:
		MSG("invalid FMT DAIFMT 0x%04x\n", fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		goto out;
	}

	ct &= ~((1 << 12) | (1 << 15));	/* NF-NB */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK)
	{
	case SND_SOC_DAIFMT_NB_NF:
		MSG("NB-NF\n");
		break;
	case SND_SOC_DAIFMT_NB_IF:
		MSG("NB-IF\n");
		ct |= (1 << 15);	/* IF */
		break;
	case SND_SOC_DAIFMT_IB_NF:
		MSG("IB-NF\n");
		ct |= (1 << 12);	/* IB */
		break;
	case SND_SOC_DAIFMT_IB_IF:
		MSG("IB-IF\n");
		ct |= (1 << 12) | (1 << 15);	/* IB-IF */
		break;
	default:
		MSG("invalid INV DAIFMT 0x%04x\n", fmt & SND_SOC_DAIFMT_INV_MASK);
		goto out;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK)
	{
	case SND_SOC_DAIFMT_CBM_CFM:		/* CODEC master */
		ct |= (1<<0);			/* I2S slave mode */
		break;
	case SND_SOC_DAIFMT_CBS_CFS:		/* CODEC slave */
		ct &= ~(1<<0);		/* Master mode */
		break;
	default:
		MSG("invalid MASTER DAIFMT 0x%04x\n", fmt & SND_SOC_DAIFMT_MASTER_MASK);
		goto out;
	}

	au_writel(ct, pscbase + PSC_I2SCFG);
	au_sync();

	ret = 0;
out:
	MSG("set_fmt() leave %d\n", ret);
	return ret;
}

/*
 * Called by ALSA when the hardware params are set by application. This
 * function can also be called multiple times and can allocate buffers
 * (using snd_pcm_lib_* ). It's non-atomic.
 */
static int au1xpsc_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned long pscbase = au1xpsc_base[rtd->dai->cpu_dai->id];
	unsigned long r;

	r = au_readl(pscbase + PSC_I2SCFG);
	r &= ~(1<<26);
	au_writel(r, pscbase + PSC_I2SCFG);	/* disable ctrl */

	/* set sample bitdepth */
	r &= ~(0x1f << 4);
	r |= (((params->msbits - 1) & 0x1f) << 4);

	/* set FIFO params: max rx/tx fifo threshold */
	r |= (3<<30) | (3<<28);

	au_writel(r | (1<<26), pscbase + PSC_I2SCFG);	/* enable */


	/* mask all IRQs, we don't want em anyway */
	au_writel(0xffffffff, pscbase + PSC_I2SMASK);
	au_sync();

	MSG("bits %d rate: %d/%d\n", params->msbits, params->rate_num,
		params->rate_den);

	return 0;
}

/*
 * Starts (Triggers) audio playback or capture.
 * Usually only needed for DMA
 */
static int au1xpsc_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned long pscbase = au1xpsc_base[rtd->dai->cpu_dai->id];
	int ret, bit;

	bit = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 4;
	ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		au_writel(1 << bit, pscbase + PSC_I2SPCR);
		au_sync();
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		au_writel(1 << (bit + 1), pscbase + PSC_I2SPCR);
		au_sync();
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_PM
/* suspend I2S controller */
static int au1xpsc_i2s_suspend(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{
#if 0
	unsigned long ioaddr = pdev->resource[0].start;

	/* switch the PSC into suspend mode */
	au_writel(2, ioaddr + PSC_CTL);
	au_sync();
#endif
	return 0;
}

/* resume I2S controller */
static int au1xpsc_i2s_resume(struct platform_device *pdev,
	struct snd_soc_cpu_dai *dai)
{
#if 0
	unsigned long ioaddr = pdev->resource[0].start;

	/* switch PSC on */
	au_writel(3, ioaddr + PSC_CTL);
	au_sync();
	while (0 == (au_readl(ioaddr + PSC_I2SSTAT) & 1))
		au_sync();
#endif
	return 0;
}

#else
#define au1xpsc_i2s_suspend	NULL
#define au1xpsc_i2s_resume	NULL
#endif

static int au1xpsc_i2s_probe(struct platform_device *pdev)
{
	/* FIXME: this whole thing should probably go to the board-stuff?
	 * Since I cannot predict here which clocksource the PSC should get
	 * Only the platform knows for sure
	 */
	unsigned long ioaddr;
	struct resource *r;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -ENODEV;

	/* get base address of PSC to use as I2S controller */
	ioaddr = r->start;

	/* reset PSC, switch to I2S, PSC on, wait for PSC ready */
	au_writel(0, ioaddr + PSC_CTL);
	au_sync();
	au_writel(3 | (1<<5), ioaddr + PSC_SEL);
	au_writel(0, ioaddr + PSC_I2SCFG);
	au_sync();
	au_writel(3, ioaddr + PSC_CTL);
	au_sync();

	/* controller might not become ready if it is clocked by the codec;
	 * codec is initialized later on and parameters are set even later
	 */

	printk(KERN_INFO "Au1x PSC%d switched to I2S mode\n",
		ioaddr == PSC0_BASE ? 0 : 1);

	return 0;
}

static void au1xpsc_i2s_remove(struct platform_device *pdev)
{
	unsigned long ioaddr = pdev->resource[0].start;

	/* disable PSC completely */
	au_writel(0, ioaddr + PSC_I2SCFG);
	au_writel(0, ioaddr + PSC_CTL);
	au_writel(0, ioaddr + PSC_SEL);
	au_sync();
}
#if 0
/* configure the I2S controllers MCLK or SYSCLK */
static unsigned int au1xpsc_i2s_config_sysclk(struct snd_soc_cpu_dai *iface,
	struct snd_soc_clock_info *info, unsigned int clk)
{
	MSG("cfg_sysclk() enter\n");
	
	MSG("cfg_sysclk() leave\n");
	return 0;

}
#endif
struct snd_soc_cpu_dai au1xpsc_i2s_dai[] = {
	{
	.name = "au1xpsc-i2s-0",
	.id = 0,
	.type = SND_SOC_DAI_I2S,
	.probe = au1xpsc_i2s_probe,
	.remove	= au1xpsc_i2s_remove,
	.suspend = au1xpsc_i2s_suspend,
	.resume = au1xpsc_i2s_resume,
//	.config_sysclk = au1xpsc_i2s_config_sysclk,
	.playback = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.capture = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.ops = {
		.trigger = au1xpsc_i2s_trigger,
		.hw_params = au1xpsc_i2s_hw_params,
	},
	.dai_ops = {
		.set_fmt	= au1xpsc_set_fmt,
	},
/*
	.caps = {
		.num_modes = ARRAY_SIZE(au1xpsc_i2s_modes),
		.mode = au1xpsc_i2s_modes,},
*/
	},
	{
	.name = "au1xpsc-i2s-1",
	.id = 1,
	.type = SND_SOC_DAI_I2S,
	.probe = au1xpsc_i2s_probe,
	.remove	= au1xpsc_i2s_remove,
	.suspend = au1xpsc_i2s_suspend,
	.resume = au1xpsc_i2s_resume,
/*	.config_sysclk = au1xpsc_i2s_config_sysclk, */
	.playback = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.capture = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.ops = {
		.trigger = au1xpsc_i2s_trigger,
		.hw_params = au1xpsc_i2s_hw_params,
	},
	.dai_ops = {
		.set_fmt	= au1xpsc_set_fmt,
	},
/*
	.caps = {
		.num_modes = ARRAY_SIZE(au1xpsc_i2s_modes),
		.mode = au1xpsc_i2s_modes,},
*/
	},
#ifdef CONFIG_SOC_AU1550
	{
	.name = "au1xpsc-i2s-2",
	.id = 2,
	.type = SND_SOC_DAI_I2S,
	.probe = au1xpsc_i2s_probe,
	.remove	= au1xpsc_i2s_remove,
	.suspend = au1xpsc_i2s_suspend,
	.resume = au1xpsc_i2s_resume,
//	.config_sysclk = au1xpsc_i2s_config_sysclk,
	.playback = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.capture = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.ops = {
		.trigger = au1xpsc_i2s_trigger,
		.hw_params = au1xpsc_i2s_hw_params,
	},
	.dai_ops = {
		.set_fmt	= au1xpsc_set_fmt,
	},
/*
	.caps = {
		.num_modes = ARRAY_SIZE(au1xpsc_i2s_modes),
		.mode = au1xpsc_i2s_modes,},
*/
	},
	{
	.name = "au1xpsc-i2s-3",
	.id = 3,
	.type = SND_SOC_DAI_I2S,
	.probe = au1xpsc_i2s_probe,
	.remove	= au1xpsc_i2s_remove,
	.suspend = au1xpsc_i2s_suspend,
	.resume = au1xpsc_i2s_resume,
/*	.config_sysclk = au1xpsc_i2s_config_sysclk, */
	.playback = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.capture = {
		.rates = AU1XPSC_I2S_RATES,
		.formats = AU1XPSC_I2S_FMTS,
		.channels_min = 2,
		.channels_max = 8,},
	.ops = {
		.trigger = au1xpsc_i2s_trigger,
		.hw_params = au1xpsc_i2s_hw_params,
	},
	.dai_ops = {
		.set_fmt	= au1xpsc_set_fmt,
	},
/*
	.caps = {
		.num_modes = ARRAY_SIZE(au1xpsc_i2s_modes),
		.mode = au1xpsc_i2s_modes,},
*/
	},
#endif
};
EXPORT_SYMBOL(au1xpsc_i2s_dai);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Au1200/Au1550 PSC AC97 audio driver");
MODULE_AUTHOR("Manuel Lauss <mano@roarinelk.homelinux.net>");
