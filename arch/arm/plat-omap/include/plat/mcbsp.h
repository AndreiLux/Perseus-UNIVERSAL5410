/*
 * arch/arm/plat-omap/include/mach/mcbsp.h
 *
 * Defines for Multi-Channel Buffered Serial Port
 *
 * Copyright (C) 2002 RidgeRun, Inc.
 * Author: Steve Johnson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef __ASM_ARCH_OMAP_MCBSP_H
#define __ASM_ARCH_OMAP_MCBSP_H

#include <linux/spinlock.h>
#include <linux/clk.h>

#define MCBSP_CONFIG_TYPE2	0x2
#define MCBSP_CONFIG_TYPE3	0x3
#define MCBSP_CONFIG_TYPE4	0x4

/* Platform specific configuration */
struct omap_mcbsp_ops {
	void (*request)(unsigned int);
	void (*free)(unsigned int);
};

struct omap_mcbsp_platform_data {
	struct omap_mcbsp_ops *ops;
	u16 buffer_size;
	u8 reg_size;
	u8 reg_step;

	/* McBSP platform and instance specific features */
	bool has_wakeup; /* Wakeup capability */
	bool has_ccr; /* Transceiver has configuration control registers */
	int (*enable_st_clock)(unsigned int, bool);
	int (*set_clk_src)(struct device *dev, struct clk *clk, const char *src);
	int (*mux_signal)(struct device *dev, const char *signal, const char *src);
	char clks_pad_src[30];
	char clks_prcm_src[30];
};

struct omap_mcbsp_st_data {
	void __iomem *io_base_st;
	bool running;
	bool enabled;
	s16 taps[128];	/* Sidetone filter coefficients */
	int nr_taps;	/* Number of filter coefficients in use */
	s16 ch0gain;
	s16 ch1gain;
};

struct omap_mcbsp {
	struct device *dev;
	unsigned long phys_base;
	unsigned long phys_dma_base;
	void __iomem *io_base;
	u8 id;
	u8 free;

	int rx_irq;
	int tx_irq;

	/* DMA stuff */
	u8 dma_rx_sync;
	u8 dma_tx_sync;

	/* Protect the field .free, while checking if the mcbsp is in use */
	spinlock_t lock;
	struct omap_mcbsp_platform_data *pdata;
	struct clk *fclk;
	struct omap_mcbsp_st_data *st_data;
#endif
#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4) || \
	defined(CONFIG_ARCH_OMAP5)
	int dma_op_mode;
	u16 max_tx_thres;
	u16 max_rx_thres;
	void *reg_cache;
	int reg_cache_size;
};

/**
 * omap_mcbsp_dev_attr - OMAP McBSP device attributes for omap_hwmod
 * @sidetone: name of the sidetone device
 */
struct omap_mcbsp_dev_attr {
	const char *sidetone;
};

extern struct omap_mcbsp **mcbsp_ptr;
extern int omap_mcbsp_count;

int omap_mcbsp_init(void);
void omap_mcbsp_config(unsigned int id, const struct omap_mcbsp_reg_cfg * config);
#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4) || \
	defined(CONFIG_ARCH_OMAP5)
void omap_mcbsp_set_tx_threshold(unsigned int id, u16 threshold);
void omap_mcbsp_set_rx_threshold(unsigned int id, u16 threshold);
u16 omap_mcbsp_get_max_tx_threshold(unsigned int id);
u16 omap_mcbsp_get_max_rx_threshold(unsigned int id);
u16 omap_mcbsp_get_fifo_size(unsigned int id);
u16 omap_mcbsp_get_tx_delay(unsigned int id);
u16 omap_mcbsp_get_rx_delay(unsigned int id);
int omap_mcbsp_get_dma_op_mode(unsigned int id);
int omap_mcbsp_request(unsigned int id);
void omap_mcbsp_free(unsigned int id);
void omap_mcbsp_start(unsigned int id, int tx, int rx);
void omap_mcbsp_stop(unsigned int id, int tx, int rx);
#endif
/* McBSP functional clock source changing function */
extern int omap2_mcbsp_set_clks_src(u8 id, u8 fck_src_id);

/* McBSP signal muxing API */
void omap2_mcbsp1_mux_clkr_src(u8 mux);
void omap2_mcbsp1_mux_fsr_src(u8 mux);

int omap_mcbsp_dma_ch_params(unsigned int id, unsigned int stream);
int omap_mcbsp_dma_reg_params(unsigned int id, unsigned int stream);

/* Sidetone specific API */
int omap_st_set_chgain(unsigned int id, int channel, s16 chgain);
int omap_st_get_chgain(unsigned int id, int channel, s16 *chgain);
int omap_st_enable(unsigned int id);
int omap_st_disable(unsigned int id);
int omap_st_is_enabled(unsigned int id);

#endif
