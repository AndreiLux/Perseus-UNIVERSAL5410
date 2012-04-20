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

/**
 * omap_mcbsp_dev_attr - OMAP McBSP device attributes for omap_hwmod
 * @sidetone: name of the sidetone device
 */
struct omap_mcbsp_dev_attr {
	const char *sidetone;
};

extern int omap_mcbsp_count;

int omap_mcbsp_init(void);

#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4) || \
	defined(CONFIG_ARCH_OMAP5)
u16 omap_mcbsp_get_max_tx_threshold(unsigned int id);
u16 omap_mcbsp_get_max_rx_threshold(unsigned int id);
u16 omap_mcbsp_get_fifo_size(unsigned int id);
#endif
/* McBSP functional clock source changing function */

/* McBSP signal muxing API */
void omap2_mcbsp1_mux_clkr_src(u8 mux);
void omap2_mcbsp1_mux_fsr_src(u8 mux);

int omap_mcbsp_dma_ch_params(unsigned int id, unsigned int stream);

/* Sidetone specific API */

#endif
