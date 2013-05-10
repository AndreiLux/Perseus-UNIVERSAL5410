/* sound/soc/samsung/i2s.c
 *
 * ALSA SoC Audio Layer - Samsung I2S Controller driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassisinghbrar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <plat/audio.h>
#include <plat/clock.h>
#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/regs-audss.h>

#include "dma.h"
#include "idma.h"
#include "i2s.h"
#include "i2s-regs.h"
#include "srp-type.h"
#include "srp_alp/srp_alp.h"

/* Initialize divider value to avoid over-clock */
#define EXYNOS_AUDSS_DIV_INIT_VAL	(0xF84)

/* Target SRP/BUS clock rate */
#define TARGET_SRPCLK_RATE	(100000000)
#define TARGET_BUSCLK_RATE	(TARGET_SRPCLK_RATE >> 1)

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

#define SLEEP	(0)
#define RUNTIME	(1)

struct i2s_dai {
	/* Platform device for this DAI */
	struct platform_device *pdev;
	/* IOREMAP'd SFRs */
	void __iomem	*addr;
	/* AUDSS SFRs */
	void __iomem	*audss_base;
	/* Physical base address of SFRs */
	u32	base;
	/* Rate of RCLK source clock */
	unsigned long rclk_srcrate;
	/* Frame Clock */
	unsigned frmclk;
	/*
	 * Specifically requested RCLK,BCLK by MACHINE Driver.
	 * 0 indicates CPU driver is free to choose any value.
	 */
	unsigned rfs, bfs;
	/* I2S Controller's core clock */
	struct clk *clk;
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	/* SRP clock */
	struct clk *srpclk;
#endif
	/* Audss's source clock */
	struct clk *fout_epll;
	struct clk *mout_audss;
	/* Audss's i2s source clock */
	struct clk *mout_i2s;
	/* SRP clock's divider */
	struct clk *dout_srp;
	/* Bus clock's divider */
	struct clk *dout_bus;
	/* i2s clock's divider */
	struct clk *dout_i2s;
	/* Clock for generating I2S signals */
	struct clk *op_clk;
	/* Array of clock names for op_clk */
	const char **src_clk;
	/* Pointer to the Primary_Fifo if this is Sec_Fifo, NULL otherwise */
	struct i2s_dai *pri_dai;
	/* Pointer to the Secondary_Fifo if it has one, NULL otherwise */
	struct i2s_dai *sec_dai;
#define DAI_OPENED	(1 << 0) /* Dai is opened */
#define DAI_MANAGER	(1 << 1) /* Dai is the manager */
	unsigned mode;
	/* Driver for this DAI */
	struct snd_soc_dai_driver i2s_dai_drv;
	/* DMA parameters */
	struct s3c_dma_params dma_playback;
	struct s3c_dma_params dma_capture;
	struct s3c_dma_params idma_playback;
	u32	quirks;
	u32	opencnt;
	u32	clk_init;

	u32	suspend_i2smod;
	u32	suspend_i2scon;
	u32	suspend_i2spsr;
	u32	suspend_i2sahb[((I2SSTR1 - I2SAHB) >> 2) + 1];

	u32	suspend_audss_clksrc;
	u32	suspend_audss_clkdiv;
	u32	suspend_audss_clkgate;
};

/* Lock for cross i/f checks */
static DEFINE_SPINLOCK(lock);

/* Lock for audss */
static DEFINE_SPINLOCK(audss_lock);
int audss_enable_cnt;

static void __iomem *i2s0_addr;

/* If this is the 'overlay' stereo DAI */
static inline bool is_secondary(struct i2s_dai *i2s)
{
	return i2s->pri_dai ? true : false;
}

/* If operating in SoC-Slave mode */
static inline bool is_slave(struct i2s_dai *i2s)
{
	return (readl(i2s->addr + I2SMOD) & MOD_SLAVE) ? true : false;
}

/* If this interface of the controller is transmitting data */
static inline bool tx_active(struct i2s_dai *i2s)
{
	u32 active;

	if (!i2s)
		return false;

	active = readl(i2s->addr + I2SCON);

	if (is_secondary(i2s))
		active &= CON_TXSDMA_ACTIVE;
	else
		active &= CON_TXDMA_ACTIVE;

	return active ? true : false;
}

/* If the other interface of the controller is transmitting data */
static inline bool other_tx_active(struct i2s_dai *i2s)
{
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;

	return tx_active(other);
}

/* If any interface of the controller is transmitting data */
static inline bool any_tx_active(struct i2s_dai *i2s)
{
	return tx_active(i2s) || other_tx_active(i2s);
}

/* If this interface of the controller is receiving data */
static inline bool rx_active(struct i2s_dai *i2s)
{
	u32 active;

	if (!i2s)
		return false;

	active = readl(i2s->addr + I2SCON) & CON_RXDMA_ACTIVE;

	return active ? true : false;
}

/* If the other interface of the controller is receiving data */
static inline bool other_rx_active(struct i2s_dai *i2s)
{
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;

	return rx_active(other);
}

/* If any interface of the controller is receiving data */
static inline bool any_rx_active(struct i2s_dai *i2s)
{
	return rx_active(i2s) || other_rx_active(i2s);
}

/* If the other DAI is transmitting or receiving data */
static inline bool other_active(struct i2s_dai *i2s)
{
	return other_rx_active(i2s) || other_tx_active(i2s);
}

/* If this DAI is transmitting or receiving data */
static inline bool this_active(struct i2s_dai *i2s)
{
	return tx_active(i2s) || rx_active(i2s);
}

/* If the controller is active anyway */
static inline bool any_active(struct i2s_dai *i2s)
{
	return this_active(i2s) || other_active(i2s);
}

static inline struct i2s_dai *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

static inline bool is_opened(struct i2s_dai *i2s)
{
	if (i2s && (i2s->mode & DAI_OPENED))
		return true;
	else
		return false;
}

static inline bool is_manager(struct i2s_dai *i2s)
{
	if (is_opened(i2s) && (i2s->mode & DAI_MANAGER))
		return true;
	else
		return false;
}

/* Read RCLK of I2S (in multiples of LRCLK) */
static inline unsigned get_rfs(struct i2s_dai *i2s)
{
	u32 rfs = (readl(i2s->addr + I2SMOD) >> 3) & 0x3;

	switch (rfs) {
	case 3:	return 768;
	case 2: return 384;
	case 1:	return 512;
	default: return 256;
	}
}

/* Write RCLK of I2S (in multiples of LRCLK) */
static inline void set_rfs(struct i2s_dai *i2s, unsigned rfs)
{
	u32 mod = readl(i2s->addr + I2SMOD);

	mod &= ~MOD_RCLK_MASK;

	switch (rfs) {
	case 768:
		mod |= MOD_RCLK_768FS;
		break;
	case 512:
		mod |= MOD_RCLK_512FS;
		break;
	case 384:
		mod |= MOD_RCLK_384FS;
		break;
	default:
		mod |= MOD_RCLK_256FS;
		break;
	}

	writel(mod, i2s->addr + I2SMOD);
}

/* Read Bit-Clock of I2S (in multiples of LRCLK) */
static inline unsigned get_bfs(struct i2s_dai *i2s)
{
	u32 bfs = (readl(i2s->addr + I2SMOD) >> 1) & 0x3;

	switch (bfs) {
	case 3: return 24;
	case 2: return 16;
	case 1:	return 48;
	default: return 32;
	}
}

/* Write Bit-Clock of I2S (in multiples of LRCLK) */
static inline void set_bfs(struct i2s_dai *i2s, unsigned bfs)
{
	u32 mod = readl(i2s->addr + I2SMOD);

	mod &= ~MOD_BCLK_MASK;

	switch (bfs) {
	case 48:
		mod |= MOD_BCLK_48FS;
		break;
	case 32:
		mod |= MOD_BCLK_32FS;
		break;
	case 24:
		mod |= MOD_BCLK_24FS;
		break;
	case 16:
		mod |= MOD_BCLK_16FS;
		break;
	default:
		dev_err(&i2s->pdev->dev, "Wrong BCLK Divider!\n");
		return;
	}

	writel(mod, i2s->addr + I2SMOD);
}

/* Sample-Size */
static inline int get_blc(struct i2s_dai *i2s)
{
	int blc = readl(i2s->addr + I2SMOD);

	blc = (blc >> 13) & 0x3;

	switch (blc) {
	case 2: return 24;
	case 1:	return 8;
	default: return 16;
	}
}

/* TX Channel Control */
static void i2s_txctrl(struct i2s_dai *i2s, int on)
{
	void __iomem *addr = i2s->addr;
	u32 con = readl(addr + I2SCON);
	u32 mod = readl(addr + I2SMOD) & ~MOD_MASK;

	if (on) {
		con |= CON_ACTIVE;
		con &= ~CON_TXCH_PAUSE;

		if (is_secondary(i2s)) {
			con |= CON_TXSDMA_ACTIVE;
			con &= ~CON_TXSDMA_PAUSE;
		} else {
			con |= CON_TXDMA_ACTIVE;
			con &= ~CON_TXDMA_PAUSE;
		}

		if (any_rx_active(i2s))
			mod |= MOD_TXRX;
		else
			mod |= MOD_TXONLY;
	} else {
		if (is_secondary(i2s)) {
			con |=  CON_TXSDMA_PAUSE;
			con &= ~CON_TXSDMA_ACTIVE;
		} else {
			con |=  CON_TXDMA_PAUSE;
			con &= ~CON_TXDMA_ACTIVE;
		}

		if (other_tx_active(i2s)) {
			writel(con, addr + I2SCON);
			return;
		}

		con |=  CON_TXCH_PAUSE;

		if (any_rx_active(i2s))
			mod |= MOD_RXONLY;
		else
			con &= ~CON_ACTIVE;
	}

	writel(mod, addr + I2SMOD);
	writel(con, addr + I2SCON);
}

/* RX Channel Control */
static void i2s_rxctrl(struct i2s_dai *i2s, int on)
{
	void __iomem *addr = i2s->addr;
	u32 con = readl(addr + I2SCON);
	u32 mod = readl(addr + I2SMOD) & ~MOD_MASK;

	if (on) {
		con |= CON_RXDMA_ACTIVE | CON_ACTIVE;
		con &= ~(CON_RXDMA_PAUSE | CON_RXCH_PAUSE);

		if (any_tx_active(i2s))
			mod |= MOD_TXRX;
		else
			mod |= MOD_RXONLY;
	} else {
		con |=  CON_RXDMA_PAUSE | CON_RXCH_PAUSE;
		con &= ~CON_RXDMA_ACTIVE;

		if (any_tx_active(i2s))
			mod |= MOD_TXONLY;
		else
			con &= ~CON_ACTIVE;
	}

	writel(mod, addr + I2SMOD);
	writel(con, addr + I2SCON);
}

/* Flush FIFO of an interface */
static inline void i2s_fifo(struct i2s_dai *i2s, u32 flush)
{
	void __iomem *fic;
	u32 val;

	if (!i2s)
		return;

	if (is_secondary(i2s))
		fic = i2s->addr + I2SFICS;
	else
		fic = i2s->addr + I2SFIC;

	/* Flush the FIFO */
	writel(readl(fic) | flush, fic);

	/* Be patient */
	val = msecs_to_loops(1) / 1000; /* 1 usec */
	while (--val)
		cpu_relax();

	writel(readl(fic) & ~flush, fic);
}

static int i2s_set_sysclk(struct snd_soc_dai *dai,
	  int clk_id, unsigned int rfs, int dir)
{
	struct i2s_dai *i2s = to_info(dai);
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;
	u32 mod = readl(i2s->addr + I2SMOD);

	switch (clk_id) {
	case SAMSUNG_I2S_OPCLK:
		mod &= ~MOD_OPCLK_MASK;
		mod |= dir;
		break;
	case SAMSUNG_I2S_CDCLK:
		/* Shouldn't matter in GATING(CLOCK_IN) mode */
		if (dir == SND_SOC_CLOCK_IN)
			rfs = 0;

		if ((rfs && other->rfs && (other->rfs != rfs)) ||
				(any_active(i2s) &&
				(((dir == SND_SOC_CLOCK_IN)
					&& !(mod & MOD_CDCLKCON)) ||
				((dir == SND_SOC_CLOCK_OUT)
					&& (mod & MOD_CDCLKCON))))) {
			dev_err(&i2s->pdev->dev,
				"%s:%d Other DAI busy\n", __func__, __LINE__);
			return -EAGAIN;
		}

		if (dir == SND_SOC_CLOCK_IN)
			mod |= MOD_CDCLKCON;
		else
			mod &= ~MOD_CDCLKCON;

		i2s->rfs = rfs;
		break;

	case SAMSUNG_I2S_RCLKSRC_0: /* clock corrsponding to IISMOD[10] := 0 */
	case SAMSUNG_I2S_RCLKSRC_1: /* clock corrsponding to IISMOD[10] := 1 */
		if ((i2s->quirks & QUIRK_NO_MUXPSR)
				|| (clk_id == SAMSUNG_I2S_RCLKSRC_0)) {
			clk_id = 0;
			mod &= ~MOD_IMS_SYSMUX;
		} else {
			clk_id = 1;
			mod |= MOD_IMS_SYSMUX;
		}

		if (!any_active(i2s)) {
			if (i2s->op_clk) {
				i2s->rclk_srcrate = clk_get_rate(i2s->op_clk);
				break;
			}

			i2s->op_clk = clk_id ? i2s->mout_i2s : i2s->dout_bus;
			i2s->rclk_srcrate = clk_get_rate(i2s->op_clk);

			/* Over-ride the other's */
			if (other) {
				other->op_clk = i2s->op_clk;
				other->rclk_srcrate = i2s->rclk_srcrate;
			}
		} else if ((!clk_id && (mod & MOD_IMS_SYSMUX))
				|| (clk_id && !(mod & MOD_IMS_SYSMUX))) {
			dev_err(&i2s->pdev->dev,
				"%s:%d Other DAI busy\n", __func__, __LINE__);
			return -EAGAIN;
		} else {
			/* Call can't be on the active DAI */
			i2s->op_clk = other->op_clk;
			i2s->rclk_srcrate = other->rclk_srcrate;
			return 0;
		}
		break;

	default:
		dev_err(&i2s->pdev->dev, "We don't serve that!\n");
		return -EINVAL;
	}

	writel(mod, i2s->addr + I2SMOD);

	return 0;
}

static int i2s_set_fmt(struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct i2s_dai *i2s = to_info(dai);
	u32 mod = 0;
	u32 tmp = 0;
	unsigned long flags;

	if (i2s == NULL) {
		pr_err("######### %s: soc_dai_drvdata is NULL\n", __func__);
	} else if (i2s->addr != i2s0_addr) {
		pr_err("######### %s: i2s->addr = %p, i2s0_addr = %p\n",
					__func__, i2s->addr, i2s0_addr);
	}

	spin_lock_irqsave(&lock, flags);

	mod = readl(i2s->addr + I2SMOD);

	/* Format is priority */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		tmp |= MOD_LR_RLOW;
		tmp |= MOD_SDF_MSB;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		tmp |= MOD_LR_RLOW;
		tmp |= MOD_SDF_LSB;
		break;
	case SND_SOC_DAIFMT_I2S:
		tmp |= MOD_SDF_IIS;
		break;
	default:
		dev_err(&i2s->pdev->dev, "Format not supported\n");
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	/*
	 * INV flag is relative to the FORMAT flag - if set it simply
	 * flips the polarity specified by the Standard
	 */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		if (tmp & MOD_LR_RLOW)
			tmp &= ~MOD_LR_RLOW;
		else
			tmp |= MOD_LR_RLOW;
		break;
	default:
		dev_err(&i2s->pdev->dev, "Polarity not supported\n");
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		tmp |= MOD_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		tmp &= ~MOD_SLAVE;
		break;
	default:
		dev_err(&i2s->pdev->dev, "master/slave format not supported\n");
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	if (any_active(i2s) &&
			((mod & (MOD_SDF_MASK | MOD_LR_RLOW
				| MOD_SLAVE)) != tmp)) {
		dev_err(&i2s->pdev->dev,
				"%s:%d Other DAI busy\n", __func__, __LINE__);
		spin_unlock_irqrestore(&lock, flags);
		return -EAGAIN;
	}

	mod &= ~(MOD_SDF_MASK | MOD_LR_RLOW | MOD_SLAVE);
	mod |= tmp;
	writel(mod, i2s->addr + I2SMOD);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

static int i2s_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct i2s_dai *i2s = to_info(dai);
	u32 mod = readl(i2s->addr + I2SMOD);

	if (!is_secondary(i2s))
		mod &= ~(MOD_DC2_EN | MOD_DC1_EN);

	switch (params_channels(params)) {
	case 6:
		mod |= MOD_DC1_EN | MOD_DC2_EN;
		break;
	case 4:
		mod |= MOD_DC1_EN;
		break;
	case 2:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			i2s->dma_playback.dma_size = 4;
		else
			i2s->dma_capture.dma_size = 4;
		break;
	case 1:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			i2s->dma_playback.dma_size = 2;
		else
			i2s->dma_capture.dma_size = 2;

		break;
	default:
		dev_err(&i2s->pdev->dev, "%d channels not supported\n",
				params_channels(params));
		return -EINVAL;
	}

	if (is_secondary(i2s))
		mod &= ~MOD_BLCS_MASK;
	else
		mod &= ~MOD_BLCP_MASK;

	if (is_manager(i2s))
		mod &= ~MOD_BLC_MASK;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		if (is_secondary(i2s))
			mod |= MOD_BLCS_8BIT;
		else
			mod |= MOD_BLCP_8BIT;
		if (is_manager(i2s))
			mod |= MOD_BLC_8BIT;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		if (is_secondary(i2s))
			mod |= MOD_BLCS_16BIT;
		else
			mod |= MOD_BLCP_16BIT;
		if (is_manager(i2s))
			mod |= MOD_BLC_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		if (is_secondary(i2s))
			mod |= MOD_BLCS_24BIT;
		else
			mod |= MOD_BLCP_24BIT;
		if (is_manager(i2s))
			mod |= MOD_BLC_24BIT;
		break;
	default:
		dev_err(&i2s->pdev->dev, "Format(%d) not supported\n",
				params_format(params));
		return -EINVAL;
	}
	writel(mod, i2s->addr + I2SMOD);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_dai_set_dma_data(dai, substream,
			(void *)&i2s->dma_playback);
	else
		snd_soc_dai_set_dma_data(dai, substream,
			(void *)&i2s->dma_capture);

	i2s->frmclk = params_rate(params);

	return 0;
}

static void audss_reg_save(struct i2s_dai *i2s)
{
	if ((i2s->pdev->id != 0) && (i2s->pdev->id != SAMSUNG_I2S_SECOFF))
		return;

	i2s->suspend_audss_clksrc = readl(i2s->audss_base +
					  EXYNOS_CLKSRC_AUDSS_OFFSET);
	i2s->suspend_audss_clkdiv = readl(i2s->audss_base +
					  EXYNOS_CLKDIV_AUDSS_OFFSET);
	i2s->suspend_audss_clkgate = readl(i2s->audss_base +
					   EXYNOS_CLKGATE_AUDSS_OFFSET);

	pr_info("Registers of Audio Subsystem are saved\n");

	return;
}

static void audss_reg_restore(struct i2s_dai *i2s)
{
	if ((i2s->pdev->id != 0) && (i2s->pdev->id != SAMSUNG_I2S_SECOFF))
		return;

	writel(i2s->suspend_audss_clkgate,
	       i2s->audss_base + EXYNOS_CLKGATE_AUDSS_OFFSET);
	writel(i2s->suspend_audss_clkdiv,
	       i2s->audss_base + EXYNOS_CLKDIV_AUDSS_OFFSET);
	writel(i2s->suspend_audss_clksrc,
	       i2s->audss_base + EXYNOS_CLKSRC_AUDSS_OFFSET);

	return;
}

static void i2s_reg_save(struct i2s_dai *i2s)
{
	u32 n, offset;

	i2s->suspend_i2smod = readl(i2s->addr + I2SMOD);
	i2s->suspend_i2scon = readl(i2s->addr + I2SCON);
	i2s->suspend_i2spsr = readl(i2s->addr + I2SPSR);
	if ((i2s->pdev->id == 0) || (i2s->pdev->id == SAMSUNG_I2S_SECOFF)) {
		for (n = 0, offset = I2SAHB; offset <= I2SSTR1; offset += 4)
			i2s->suspend_i2sahb[n++] = readl(i2s->addr + offset);
	}

	dev_dbg(&i2s->pdev->dev, "Registers of I2S are saved\n");

	return;
}

static void i2s_reg_restore(struct i2s_dai *i2s)
{
	u32 n, offset;

	writel(i2s->suspend_i2smod, i2s->addr + I2SMOD);
	writel(i2s->suspend_i2scon, i2s->addr + I2SCON);
	writel(i2s->suspend_i2spsr, i2s->addr + I2SPSR);
	if ((i2s->pdev->id == 0) || (i2s->pdev->id == SAMSUNG_I2S_SECOFF)) {
		for (n = 0, offset = I2SAHB; offset <= I2SSTR1; offset += 4)
			writel(i2s->suspend_i2sahb[n++], i2s->addr + offset);
	}

	dev_dbg(&i2s->pdev->dev, "Registers of I2S are restored\n");

	return;
}

static void pm_runtime_ctl(struct i2s_dai *i2s, bool enabled)
{
	struct platform_device *pdev = NULL;

	pdev = is_secondary(i2s) ? i2s->pri_dai->pdev : i2s->pdev;
	enabled ? pm_runtime_get_sync(&pdev->dev)
		: pm_runtime_put_sync(&pdev->dev);
}

void audss_enable(void *i2s_info)
{
	struct i2s_dai *i2s = (struct i2s_dai *) i2s_info;
	int ret;

	if (!i2s->clk_init) {
		/* To avoid over-clock */
		writel(EXYNOS_AUDSS_DIV_INIT_VAL,
				i2s->audss_base + EXYNOS_CLKDIV_AUDSS_OFFSET);

		ret = clk_set_parent(i2s->mout_audss, i2s->fout_epll);
		if (ret)
			dev_err(&i2s->pdev->dev, "failed to set parent %s of clock %s.\n",
					i2s->fout_epll->name, i2s->mout_audss->name);

		ret = clk_set_parent(i2s->mout_i2s, i2s->mout_audss);
		if (ret)
			dev_err(&i2s->pdev->dev, "failed to set parent %s of clock %s.\n",
					i2s->mout_audss->name, i2s->mout_i2s->name);


		clk_set_rate(i2s->dout_srp, TARGET_SRPCLK_RATE);
		clk_set_rate(i2s->dout_bus, TARGET_BUSCLK_RATE);

		dev_info(&i2s->pdev->dev, "EPLL rate = %ld\n", clk_get_rate(i2s->fout_epll));
		dev_info(&i2s->pdev->dev, "SRP rate = %ld\n", clk_get_rate(i2s->dout_srp));
		dev_info(&i2s->pdev->dev, "BUS rate = %ld\n", clk_get_rate(i2s->dout_bus));

		i2s->clk_init = 1;
		audss_reg_save(i2s);
	}

	spin_lock(&audss_lock);
	audss_enable_cnt++;

	pr_info("%s: cnt %d\n", __func__, audss_enable_cnt);

	if (audss_enable_cnt == 1) {
		audss_reg_restore(i2s);
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		clk_enable(i2s->srpclk);
#endif
		srp_core_reset();
	}

	spin_unlock(&audss_lock);
}

void audss_disable(void *i2s_info)
{
	struct i2s_dai *i2s = i2s_info;

	spin_lock(&audss_lock);
	audss_enable_cnt--;

	pr_info("%s: cnt %d\n", __func__, audss_enable_cnt);

	if (audss_enable_cnt == 0) {
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		clk_disable(i2s->srpclk);
#endif
		audss_reg_save(i2s);
	}

	spin_unlock(&audss_lock);
}

void i2s_enable(struct snd_soc_dai *dai, int pm_mode)
{
	struct i2s_dai *i2s = to_info(dai);
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;
	unsigned long flags;

	struct platform_device *pdev = i2s->pri_dai ?
				       i2s->pri_dai->pdev : i2s->pdev;
	struct s3c_audio_pdata *i2s_pdata = pdev->dev.platform_data;

	spin_lock_irqsave(&lock, flags);

	if (is_secondary(i2s) && is_opened(i2s)) {
		goto exit_func;
	} else if (!is_secondary(i2s) && i2s->opencnt++) {
		goto exit_func;
	}

	i2s->mode |= DAI_OPENED;

	if (is_manager(other))
		i2s->mode &= ~DAI_MANAGER;
	else
		i2s->mode |= DAI_MANAGER;

	/* Enforce set_sysclk in Master mode */
	i2s->rclk_srcrate = 0;

	if (!is_opened(other)) {
		spin_unlock_irqrestore(&lock, flags);

		pm_runtime_ctl(i2s, true);

		if (pm_mode == SLEEP) {
			audss_reg_restore(i2s);
			i2s_reg_restore(i2s);
		}

		return;
	}

exit_func:
	spin_unlock_irqrestore(&lock, flags);
	return;
}

void i2s_disable(struct snd_soc_dai *dai, int pm_mode)
{
	struct i2s_dai *i2s = to_info(dai);
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	if (is_secondary(i2s) && !is_opened(i2s)) {
		spin_unlock_irqrestore(&lock, flags);
		return;
	} else if (!is_secondary(i2s) && --i2s->opencnt) {
		spin_unlock_irqrestore(&lock, flags);
		return;
	}

	i2s->mode &= ~DAI_OPENED;
	i2s->mode &= ~DAI_MANAGER;

	if (is_opened(other))
		other->mode |= DAI_MANAGER;

	/* Reset any constraint on RFS and BFS */
	i2s->rfs = 0;
	i2s->bfs = 0;

	spin_unlock_irqrestore(&lock, flags);

	/* Gate CDCLK by default */
	if (!is_opened(other)) {
		i2s_set_sysclk(dai, SAMSUNG_I2S_CDCLK,
				0, SND_SOC_CLOCK_IN);

		if (pm_mode == SLEEP) {
			i2s_reg_save(i2s);
			audss_reg_save(i2s);
		}

		pm_runtime_ctl(i2s, false);
	}
}

/* We set constraints on the substream acc to the version of I2S */
static int i2s_startup(struct snd_pcm_substream *substream,
	  struct snd_soc_dai *dai)
{
	i2s_enable(dai, RUNTIME);

	return 0;
}

static void i2s_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	i2s_disable(dai, RUNTIME);
}

static int config_setup(struct i2s_dai *i2s)
{
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;
	unsigned rfs, bfs, blc;
	u32 psr;

	blc = get_blc(i2s);

	bfs = i2s->bfs;

	if (!bfs && other)
		bfs = other->bfs;

	/* Select least possible multiple(2) if no constraint set */
	if (!bfs)
		bfs = blc * 2;

	rfs = i2s->rfs;

	if (!rfs && other)
		rfs = other->rfs;

	if ((rfs == 256 || rfs == 512) && (blc == 24)) {
		dev_err(&i2s->pdev->dev,
			"%d-RFS not supported for 24-blc\n", rfs);
		return -EINVAL;
	}

	if (!rfs) {
		if (bfs == 16 || bfs == 32)
			rfs = 256;
		else
			rfs = 384;
	}

	/* If already setup and running */
	if (any_active(i2s) && (get_rfs(i2s) != rfs || get_bfs(i2s) != bfs)) {
		dev_err(&i2s->pdev->dev,
				"%s:%d Other DAI busy\n", __func__, __LINE__);
		return -EAGAIN;
	}

	/* Don't bother RFS, BFS & PSR in Slave mode */
	if (is_slave(i2s))
		return 0;

	set_bfs(i2s, bfs);
	set_rfs(i2s, rfs);

	if (!(i2s->quirks & QUIRK_NO_MUXPSR)) {
		psr = i2s->rclk_srcrate / i2s->frmclk / rfs;
		writel(((psr - 1) << 8) | PSR_PSREN, i2s->addr + I2SPSR);
		dev_dbg(&i2s->pdev->dev,
			"RCLK_SRC=%luHz PSR=%u, RCLK=%dfs, BCLK=%dfs\n",
				i2s->rclk_srcrate, psr, rfs, bfs);
	}

	return 0;
}

#if 0 // unused
static int i2s_lrsync(struct i2s_dai *i2s)
{
	u32 con;
	unsigned long timeout;

	pr_debug("Entered %s\n", __func__);

	/* Wait max 5ms */
	timeout = jiffies + (HZ / 200);

	while (!time_after(jiffies, timeout)) {
		con = readl(i2s->addr + I2SCON);
		if (con & CON_LRINDEX)
			break;

		cpu_relax();
	}

	if (!(con & CON_LRINDEX)) {
		printk(KERN_ERR "%s: timeout\n", __func__);
		printk(" I2SCON: %x, I2SMOD: %x, I2SPSR: %x\n",
			readl(i2s->addr + I2SCON), readl(i2s->addr + I2SMOD),
			readl(i2s->addr + I2SPSR));
		printk("AUDSS_CLKSRC=%x, AUDSS_CLKDIV=%x, AUDSS_CLKGATE=%x\n",
			 readl(EXYNOS_CLKSRC_AUDSS), readl(EXYNOS_CLKDIV_AUDSS),
			 readl(EXYNOS_CLKGATE_AUDSS));
		return -ETIMEDOUT;
	}

	return 0;
}
#endif

static int i2s_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct i2s_dai *i2s = to_info(rtd->cpu_dai);
	unsigned long flags;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#if 0
		if (is_slave(i2s)) {
			ret = i2s_lrsync(i2s);
			if (ret)
				goto exit_err;
		}
#endif

		spin_lock_irqsave(&lock, flags);

		if (config_setup(i2s)) {
			spin_unlock_irqrestore(&lock, flags);
			return -EINVAL;
		}

		if (capture)
			i2s_rxctrl(i2s, 1);
		else
			i2s_txctrl(i2s, 1);

		spin_unlock_irqrestore(&lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:

		spin_lock_irqsave(&lock, flags);

		if (capture) {
			i2s_rxctrl(i2s, 0);
			i2s_fifo(i2s, FIC_RXFLUSH);
		} else {
			i2s_txctrl(i2s, 0);
		}

		spin_unlock_irqrestore(&lock, flags);
		break;
	}

/* exit_err: // unused label due to the above hack (#if 0 ... #endif) */
	return ret;
}

static int i2s_set_clkdiv(struct snd_soc_dai *dai,
	int div_id, int div)
{
	struct i2s_dai *i2s = to_info(dai);
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;

	switch (div_id) {
	case SAMSUNG_I2S_DIV_BCLK:
		if ((any_active(i2s) && div && (get_bfs(i2s) != div))
			|| (other && other->bfs && (other->bfs != div))) {
			dev_err(&i2s->pdev->dev,
				"%s:%d Other DAI busy\n", __func__, __LINE__);
			return -EAGAIN;
		}
		i2s->bfs = div;
		break;
	default:
		dev_err(&i2s->pdev->dev,
			"Invalid clock divider(%d)\n", div_id);
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_sframes_t
i2s_delay(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct i2s_dai *i2s = to_info(dai);
	u32 reg = readl(i2s->addr + I2SFIC);
	snd_pcm_sframes_t delay;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		delay = FIC_RXCOUNT(reg);
	else if (is_secondary(i2s))
		delay = FICS_TXCOUNT(readl(i2s->addr + I2SFICS));
	else
		delay = FIC_TXCOUNT(reg);

	return delay;
}

static int clk_set_heirachy(struct i2s_dai *i2s)
{
	unsigned int ret = 0;

	if ((i2s->pdev->id != 0) && (i2s->pdev->id != SAMSUNG_I2S_SECOFF)) {
		dev_err(&i2s->pdev->dev, "Not support i2s channel %d\n",
							i2s->pdev->id);
		return ret;
	}

	i2s->fout_epll = clk_get(&i2s->pdev->dev, "fout_epll");
	if (IS_ERR(i2s->fout_epll)) {
		dev_err(&i2s->pdev->dev, "failed to get fout_epll clock\n");
		ret = PTR_ERR(i2s->fout_epll);
		goto err0;
	}

	i2s->mout_audss = clk_get(&i2s->pdev->dev, "mout_audss");
	if (IS_ERR(i2s->mout_audss)) {
		dev_err(&i2s->pdev->dev, "failed to get mout_audss clock\n");
		ret = PTR_ERR(i2s->mout_audss);
		goto err1;
	}

	i2s->mout_i2s = clk_get(&i2s->pdev->dev, "mout_i2s");
	if (IS_ERR(i2s->mout_i2s)) {
		dev_err(&i2s->pdev->dev, "failed to get mout_i2s clock\n");
		ret = PTR_ERR(i2s->mout_i2s);
		goto err2;
	}

	i2s->dout_srp = clk_get(&i2s->pdev->dev, "dout_srp");
	if (IS_ERR(i2s->dout_srp)) {
		dev_err(&i2s->pdev->dev, "failed to get dout_srp div\n");
		ret = PTR_ERR(i2s->dout_srp);
		goto err3;
	}

	i2s->dout_bus = clk_get(&i2s->pdev->dev, "dout_bus");
	if (IS_ERR(i2s->dout_bus)) {
		dev_err(&i2s->pdev->dev, "failed to get dout_bus div\n");
		ret = PTR_ERR(i2s->dout_bus);
		goto err4;
	}

	i2s->dout_i2s = clk_get(&i2s->pdev->dev, "dout_i2s");
	if (IS_ERR(i2s->dout_i2s)) {
		dev_err(&i2s->pdev->dev, "failed to get dout_i2s div\n");
		ret = PTR_ERR(i2s->dout_i2s);
		goto err5;
	}

	return ret;

err5:
	clk_put(i2s->dout_bus);
err4:
	clk_put(i2s->dout_srp);
err3:
	clk_put(i2s->mout_i2s);
err2:
	clk_put(i2s->mout_audss);
err1:
	clk_put(i2s->fout_epll);
err0:
	return ret;
}

#ifdef CONFIG_PM
static int i2s_suspend(struct snd_soc_dai *dai)
{
	if (dai->active)
		i2s_disable(dai, SLEEP);

	return 0;
}

static int i2s_resume(struct snd_soc_dai *dai)
{
	if (dai->active)
		i2s_enable(dai, SLEEP);

	return 0;
}
#else
#define i2s_suspend NULL
#define i2s_resume  NULL
#endif

static int samsung_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct i2s_dai *i2s = to_info(dai);
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;
	struct platform_device *pdev = i2s->pri_dai ?
				       i2s->pri_dai->pdev : i2s->pdev;
	struct s3c_audio_pdata *i2s_pdata = pdev->dev.platform_data;
	int ret;

	if (other && other->clk) /* If this is second dai probe */
		goto probe_exit;

	i2s->addr = ioremap(i2s->base, SZ_256);
	if (i2s->addr == NULL) {
		dev_err(&i2s->pdev->dev, "cannot ioremap registers\n");
		ret = -ENOMEM;
		goto err1;
	}

	i2s0_addr = i2s->addr;

	i2s->audss_base = ioremap(EXYNOS_PA_AUDSS, SZ_4K);
	if (i2s->audss_base == NULL) {
		dev_err(&i2s->pdev->dev, "cannot ioremap registers\n");
		ret = -ENOMEM;
		goto err2;
	}

	i2s->clk = clk_get(&i2s->pdev->dev, "iis");
	if (IS_ERR(i2s->clk)) {
		dev_err(&i2s->pdev->dev, "failed to get i2s clock\n");
		ret = PTR_ERR(i2s->clk);
		goto err3;
	}

#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	i2s->srpclk = clk_get(&i2s->pdev->dev, "srpclk");
	if (IS_ERR(i2s->srpclk)) {
		dev_err(&i2s->pdev->dev, "failed to get srp clock\n");
		ret = PTR_ERR(i2s->srpclk);
		goto err4;
	}
#endif

probe_exit:
	/* Set clock hierarchy for audio subsystem */
	ret = clk_set_heirachy(i2s);
	if (ret) {
		dev_err(&i2s->pdev->dev, "failed to set clock hierachy.\n");
		if (is_secondary(i2s))
			goto err5;
		else
			goto err1;
	}

	if (other) {
		other->addr = i2s->addr;
		other->audss_base = i2s->audss_base;
		other->clk = i2s->clk;
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		other->srpclk = i2s->srpclk;
#endif
		other->fout_epll = i2s->fout_epll;
		other->mout_audss = i2s->mout_audss;
		other->mout_i2s = i2s->mout_i2s;
		other->dout_srp = i2s->dout_srp;
		other->dout_bus = i2s->dout_bus;

		if (is_secondary(i2s)) {
			if (srp_enabled_status()) {
				i2s->idma_playback.dma_addr = srp_get_idma_addr();
				srp_prepare_pm((void *) i2s);
			}

			idma_reg_addr_init(i2s->addr,
					   i2s->idma_playback.dma_addr);
		}
	}

	pm_runtime_ctl(i2s, true);

	if (i2s->quirks & QUIRK_NEED_RSTCLR)
		writel(CON_RSTCLR, i2s->addr + I2SCON);

	/* Reset any constraint on RFS and BFS */
	i2s->rfs = 0;
	i2s->bfs = 0;
	i2s_txctrl(i2s, 0);
	i2s_rxctrl(i2s, 0);
	i2s_fifo(i2s, FIC_TXFLUSH);
	i2s_fifo(other, FIC_TXFLUSH);
	i2s_fifo(i2s, FIC_RXFLUSH);

	/* Gate CDCLK by default */
	if (!is_opened(other))
		i2s_set_sysclk(dai, SAMSUNG_I2S_CDCLK,
				0, SND_SOC_CLOCK_IN);

	i2s->opencnt = 0;
	pm_runtime_ctl(i2s, false);

	return 0;

err5:
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	clk_put(i2s->srpclk);
err4:
#endif
	clk_put(i2s->clk);
err3:
	iounmap(i2s->audss_base);
err2:
	iounmap(i2s->addr);
err1:
	return ret;
}

static int samsung_i2s_dai_remove(struct snd_soc_dai *dai)
{
	struct i2s_dai *i2s = snd_soc_dai_get_drvdata(dai);
	struct i2s_dai *other = i2s->pri_dai ? : i2s->sec_dai;

	if (!other || !other->clk) {

		if (i2s->quirks & QUIRK_NEED_RSTCLR)
			writel(0, i2s->addr + I2SCON);

#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		clk_disable(i2s->srpclk);
		clk_put(i2s->srpclk);
#endif
		clk_disable(i2s->clk);
		clk_put(i2s->clk);

		iounmap(i2s->addr);
	}

	i2s->clk = NULL;

	return 0;
}

static const struct snd_soc_dai_ops samsung_i2s_dai_ops = {
	.trigger = i2s_trigger,
	.hw_params = i2s_hw_params,
	.set_fmt = i2s_set_fmt,
	.set_clkdiv = i2s_set_clkdiv,
	.set_sysclk = i2s_set_sysclk,
	.startup = i2s_startup,
	.shutdown = i2s_shutdown,
	.delay = i2s_delay,
};

#define SAMSUNG_I2S_RATES	SNDRV_PCM_RATE_8000_96000

#define SAMSUNG_I2S_FMTS	(SNDRV_PCM_FMTBIT_S8 | \
					SNDRV_PCM_FMTBIT_S16_LE | \
					SNDRV_PCM_FMTBIT_S24_LE)

static __devinit
struct i2s_dai *i2s_alloc_dai(struct platform_device *pdev, bool sec)
{
	struct i2s_dai *i2s;

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct i2s_dai), GFP_KERNEL);
	if (i2s == NULL)
		return NULL;

	i2s->pdev = pdev;
	i2s->pri_dai = NULL;
	i2s->sec_dai = NULL;
	i2s->i2s_dai_drv.symmetric_rates = 1;
	i2s->i2s_dai_drv.probe = samsung_i2s_dai_probe;
	i2s->i2s_dai_drv.remove = samsung_i2s_dai_remove;
	i2s->i2s_dai_drv.ops = &samsung_i2s_dai_ops;
	i2s->i2s_dai_drv.suspend = i2s_suspend;
	i2s->i2s_dai_drv.resume = i2s_resume;
	i2s->i2s_dai_drv.playback.channels_min = 2;
	i2s->i2s_dai_drv.playback.channels_max = 6;
	i2s->i2s_dai_drv.playback.rates = SAMSUNG_I2S_RATES;
	i2s->i2s_dai_drv.playback.formats = SAMSUNG_I2S_FMTS;

	if (!sec) {
		i2s->i2s_dai_drv.capture.channels_min = 1;
		i2s->i2s_dai_drv.capture.channels_max = 2;
		i2s->i2s_dai_drv.capture.rates = SAMSUNG_I2S_RATES;
		i2s->i2s_dai_drv.capture.formats = SAMSUNG_I2S_FMTS;
	} else {	/* Create a new platform_device for Secondary */
		i2s->pdev = platform_device_register_resndata(NULL,
				pdev->name, pdev->id + SAMSUNG_I2S_SECOFF,
				NULL, 0, NULL, 0);
		if (IS_ERR(i2s->pdev))
			return NULL;
	}

	/* Pre-assign snd_soc_dai_set_drvdata */
	dev_set_drvdata(&i2s->pdev->dev, i2s);

	return i2s;
}

static __devinit int samsung_i2s_probe(struct platform_device *pdev)
{
	u32 dma_pl_chan, dma_cp_chan, dma_pl_sec_chan;
	struct i2s_dai *pri_dai, *sec_dai = NULL;
	struct s3c_audio_pdata *i2s_pdata;
	struct samsung_i2s *i2s_cfg;
	struct resource *res;
	u32 regs_base, quirks;
	int ret = 0;

	/* Call during Seconday interface registration */
	if (pdev->id >= SAMSUNG_I2S_SECOFF) {
		sec_dai = dev_get_drvdata(&pdev->dev);
		snd_soc_register_dai(&sec_dai->pdev->dev,
			&sec_dai->i2s_dai_drv);
		return 0;
	}

	i2s_pdata = pdev->dev.platform_data;
	if (i2s_pdata == NULL) {
		dev_err(&pdev->dev, "Can't work without s3c_audio_pdata\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get I2S-TX dma resource\n");
		return -ENXIO;
	}
	dma_pl_chan = res->start;

	res = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get I2S-RX dma resource\n");
		return -ENXIO;
	}
	dma_cp_chan = res->start;

	res = platform_get_resource(pdev, IORESOURCE_DMA, 2);
	if (res)
		dma_pl_sec_chan = res->start;
	else
		dma_pl_sec_chan = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get I2S SFR address\n");
		return -ENXIO;
	}

	if (!request_mem_region(res->start, resource_size(res),
							"samsung-i2s")) {
		dev_err(&pdev->dev, "Unable to request SFR region\n");
		return -EBUSY;
	}
	regs_base = res->start;

	i2s_cfg = &i2s_pdata->type.i2s;
	quirks = i2s_cfg->quirks;

	pri_dai = i2s_alloc_dai(pdev, false);
	if (!pri_dai) {
		dev_err(&pdev->dev, "Unable to alloc I2S_pri\n");
		ret = -ENOMEM;
		goto err;
	}

	pri_dai->dma_playback.dma_addr = regs_base + I2STXD;
	pri_dai->dma_capture.dma_addr = regs_base + I2SRXD;
	pri_dai->dma_playback.client =
		(struct s3c2410_dma_client *)&pri_dai->dma_playback;
	pri_dai->dma_capture.client =
		(struct s3c2410_dma_client *)&pri_dai->dma_capture;
	pri_dai->dma_playback.channel = dma_pl_chan;
	pri_dai->dma_capture.channel = dma_cp_chan;
	pri_dai->src_clk = i2s_cfg->src_clk;
	pri_dai->dma_playback.dma_size = 4;
	pri_dai->dma_capture.dma_size = 4;
	pri_dai->base = regs_base;
	pri_dai->quirks = quirks;

	if (quirks & QUIRK_PRI_6CHAN)
		pri_dai->i2s_dai_drv.playback.channels_max = 6;

	if (quirks & QUIRK_SEC_DAI) {
		sec_dai = i2s_alloc_dai(pdev, true);
		if (!sec_dai) {
			dev_err(&pdev->dev, "Unable to alloc I2S_sec\n");
			ret = -ENOMEM;
			goto err;
		}
		sec_dai->dma_playback.dma_addr = regs_base + I2STXDS;
		sec_dai->dma_playback.client =
			(struct s3c2410_dma_client *)&sec_dai->dma_playback;
		/* Use iDMA always if SysDMA not provided */
		sec_dai->dma_playback.channel = dma_pl_sec_chan ? : -1;
		sec_dai->src_clk = i2s_cfg->src_clk;
		sec_dai->dma_playback.dma_size = 4;
		sec_dai->base = regs_base;
		sec_dai->quirks = quirks;
		sec_dai->idma_playback.dma_addr = i2s_cfg->idma_addr;

		sec_dai->pri_dai = pri_dai;
		pri_dai->sec_dai = sec_dai;
	}

	snd_soc_register_dai(&pri_dai->pdev->dev, &pri_dai->i2s_dai_drv);

	pm_runtime_enable(&pdev->dev);

	return 0;
err:
	release_mem_region(regs_base, resource_size(res));

	return ret;
}

static __devexit int samsung_i2s_remove(struct platform_device *pdev)
{
	struct i2s_dai *i2s, *other;
	struct resource *res;

	i2s = dev_get_drvdata(&pdev->dev);
	other = i2s->pri_dai ? : i2s->sec_dai;

	if (other) {
		other->pri_dai = NULL;
		other->sec_dai = NULL;
	} else {
		pm_runtime_disable(&pdev->dev);
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (res)
			release_mem_region(res->start, resource_size(res));
	}

	i2s->pri_dai = NULL;
	i2s->sec_dai = NULL;

	snd_soc_unregister_dai(&pdev->dev);

	return 0;
}

static int i2s_runtime_suspend(struct device *dev)
{
	struct i2s_dai *i2s = dev_get_drvdata(dev);

	pr_info("Entered %s function\n", __func__);

	i2s_reg_save(i2s);
	clk_disable(i2s->clk);
	audss_disable(i2s);
	pr_debug("MAU OFF\n");

	return 0;
}

static int i2s_runtime_resume(struct device *dev)
{
	struct i2s_dai *i2s = dev_get_drvdata(dev);
	struct s3c_audio_pdata *i2s_pdata = dev->platform_data;
	struct platform_device *pdev = container_of(dev, \
				struct platform_device, dev);

	pr_info("Entered %s function\n", __func__);

	audss_enable(i2s);
	clk_enable(i2s->clk);
	i2s_reg_restore(i2s);

	if (i2s_pdata->cfg_gpio && i2s_pdata->cfg_gpio(pdev))
		dev_err(&pdev->dev, "Unable to configure gpio\n");

	if (i2s->quirks & QUIRK_NEED_RSTCLR)
		writel(CON_RSTCLR, i2s->addr + I2SCON);

	pr_debug("MAU ON\n");

	return 0;
}

static const struct dev_pm_ops i2s_pmops = {
	SET_RUNTIME_PM_OPS(
		i2s_runtime_suspend,
		i2s_runtime_resume,
		NULL
	)
};

static struct platform_driver samsung_i2s_driver = {
	.probe  = samsung_i2s_probe,
	.remove = __devexit_p(samsung_i2s_remove),
	.driver = {
		.name = "samsung-i2s",
		.owner = THIS_MODULE,
		.pm	= &i2s_pmops,
	},
};

module_platform_driver(samsung_i2s_driver);

/* Module information */
MODULE_AUTHOR("Jaswinder Singh, <jassisinghbrar@gmail.com>");
MODULE_DESCRIPTION("Samsung I2S Interface");
MODULE_ALIAS("platform:samsung-i2s");
MODULE_LICENSE("GPL");
