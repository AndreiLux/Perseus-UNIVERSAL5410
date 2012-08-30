/* exynos_drm_fimd.c
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include "drmP.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include <drm/exynos_drm.h>
#include <plat/regs-fb-v4.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_fbdev.h"

/*
 * FIMD is stand for Fully Interactive Mobile Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

/* position control register for hardware window 0, 2 ~ 4.*/
#define VIDOSD_A(win)		(VIDOSD_BASE + 0x00 + (win) * 16)
#define VIDOSD_B(win)		(VIDOSD_BASE + 0x04 + (win) * 16)
/* size control register for hardware window 0. */
#define VIDOSD_C_SIZE_W0	(VIDOSD_BASE + 0x08)
/* alpha control register for hardware window 1 ~ 4. */
#define VIDOSD_C(win)		(VIDOSD_BASE + 0x18 + (win) * 16)
/* size control register for hardware window 1 ~ 4. */
#define VIDOSD_D(win)		(VIDOSD_BASE + 0x0C + (win) * 16)

#define VIDWx_BUF_START(win, buf)	(VIDW_BUF_START(buf) + (win) * 8)
#define VIDWx_BUF_END(win, buf)		(VIDW_BUF_END(buf) + (win) * 8)
#define VIDWx_BUF_SIZE(win, buf)	(VIDW_BUF_SIZE(buf) + (win) * 4)

/* color key control register for hardware window 1 ~ 4. */
#define WKEYCON0_BASE(x)		((WKEYCON0 + 0x140) + (x * 8))
/* color key value register for hardware window 1 ~ 4. */
#define WKEYCON1_BASE(x)		((WKEYCON1 + 0x140) + (x * 8))

/* FIMD has totally five hardware windows. */
#define WINDOWS_NR	5

#define get_fimd_context(dev)	platform_get_drvdata(to_platform_device(dev))

struct fimd_win_data {
	unsigned int		offset_x;
	unsigned int		offset_y;
	unsigned int		ovl_width;
	unsigned int		ovl_height;
	unsigned int		fb_width;
	unsigned int		fb_height;
	unsigned int		fb_pitch;
	unsigned int		bpp;
	dma_addr_t		dma_addr;
	void __iomem		*vaddr;
	unsigned int		buf_offsize;
	unsigned int		line_size;	/* bytes */
	bool			enabled;
};

struct fimd_context {
	struct exynos_drm_subdrv	subdrv;
	int				irq;
	struct drm_crtc			*crtc;
	struct clk			*bus_clk;
	struct clk			*lcd_clk;
	struct resource			*regs_res;
	void __iomem			*regs;
	void __iomem			*regs_mie;
	struct fimd_win_data		win_data[WINDOWS_NR];
	unsigned int			clkdiv[MAX_NR_PANELS];
	unsigned int			default_win;
	unsigned long			irq_flags;
	u32				vidcon0;
	u32				vidcon1;
	int				idx;
	bool				suspended;
	struct mutex			lock;

	struct exynos_drm_panel_info *panel;
};

static bool fimd_display_is_connected(struct device *dev)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* TODO. */

	return true;
}

static void *fimd_get_panel(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	return ctx->panel;
}

static int fimd_check_timing(struct device *dev, void *timing)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fb_videomode *check_timing = timing;
	int i;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	for (i = 0;i< MAX_NR_PANELS;i++) {
		if (ctx->panel[i].timing.xres == -1 &&
			 ctx->panel[i].timing.yres == -1)
			 break;

		if (ctx->panel[i].timing.xres == check_timing->xres &&
			 ctx->panel[i].timing.yres == check_timing->yres &&
			ctx->panel[i].timing.refresh == check_timing->refresh
			)
			return 0;
	}

	return -EINVAL;
}

static int fimd_power_on(struct fimd_context *ctx, bool enable);

static int fimd_display_power_on(struct device *dev, int mode)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	bool enable;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
		enable = true;
		break;
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		enable = false;
		break;
	default:
		DRM_DEBUG_KMS("unspecified mode %d\n", mode);
		break;
	}

	fimd_power_on(ctx, enable);

	return 0;
}

static struct exynos_drm_display_ops fimd_display_ops = {
	.type = EXYNOS_DISPLAY_TYPE_LCD,
	.is_connected = fimd_display_is_connected,
	.get_panel = fimd_get_panel,
	.check_timing = fimd_check_timing,
	.power_on = fimd_display_power_on,
};

static void fimd_apply(struct device *subdrv_dev)
{
	struct fimd_context *ctx = get_fimd_context(subdrv_dev);
	struct exynos_drm_manager *mgr = ctx->subdrv.manager;
	struct exynos_drm_manager_ops *mgr_ops = mgr->ops;
	struct exynos_drm_overlay_ops *ovl_ops = mgr->overlay_ops;
	struct fimd_win_data *win_data;
	int i;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	for (i = 0; i < WINDOWS_NR; i++) {
		win_data = &ctx->win_data[i];
		if (win_data->enabled && (ovl_ops && ovl_ops->commit))
			ovl_ops->commit(subdrv_dev, i);
	}

	if (mgr_ops && mgr_ops->commit)
		mgr_ops->commit(subdrv_dev);
}

static void fimd_commit(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct exynos_drm_panel_info *panel = &ctx->panel[ctx->idx];
	struct fb_videomode *timing = &panel->timing;
	u32 val;

	if (ctx->suspended)
		return;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* setup polarity values from machine code. */
	writel(ctx->vidcon1, ctx->regs + VIDCON1);

	/* setup vertical timing values. */
	val = VIDTCON0_VBPD(timing->upper_margin - 1) |
	       VIDTCON0_VFPD(timing->lower_margin - 1) |
	       VIDTCON0_VSPW(timing->vsync_len - 1);
	writel(val, ctx->regs + VIDTCON0);

	/* setup horizontal timing values.  */
	val = VIDTCON1_HBPD(timing->left_margin - 1) |
	       VIDTCON1_HFPD(timing->right_margin - 1) |
	       VIDTCON1_HSPW(timing->hsync_len - 1);
	writel(val, ctx->regs + VIDTCON1);

	/* setup horizontal and vertical display size. */
	val = VIDTCON2_LINEVAL(timing->yres - 1) |
	       VIDTCON2_HOZVAL(timing->xres - 1);
	writel(val, ctx->regs + VIDTCON2);

	/* setup clock source, clock divider, enable dma. */
	val = ctx->vidcon0;
	val &= ~(VIDCON0_CLKVAL_F_MASK | VIDCON0_CLKDIR);

	if (ctx->clkdiv[ctx->idx] > 1)
		val |= VIDCON0_CLKVAL_F(ctx->clkdiv[ctx->idx] - 1) | VIDCON0_CLKDIR;
	else
		val &= ~VIDCON0_CLKDIR;	/* 1:1 clock */

	/*
	 * fields of register with prefix '_F' would be updated
	 * at vsync(same as dma start)
	 */
	val |= VIDCON0_ENVID | VIDCON0_ENVID_F;
	writel(val, ctx->regs + VIDCON0);
}

static int fimd_enable_vblank(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	u32 val;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->suspended)
		return -EPERM;

	if (!test_and_set_bit(0, &ctx->irq_flags)) {
		val = readl(ctx->regs + VIDINTCON0);

		val |= VIDINTCON0_INT_ENABLE;
		val |= VIDINTCON0_INT_FRAME;

		val &= ~VIDINTCON0_FRAMESEL0_MASK;
		val |= VIDINTCON0_FRAMESEL0_VSYNC;
		val &= ~VIDINTCON0_FRAMESEL1_MASK;
		val |= VIDINTCON0_FRAMESEL1_NONE;

		writel(val, ctx->regs + VIDINTCON0);
	}

	return 0;
}

static void fimd_disable_vblank(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	u32 val;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->suspended)
		return;

	if (test_and_clear_bit(0, &ctx->irq_flags)) {
		val = readl(ctx->regs + VIDINTCON0);

		val &= ~VIDINTCON0_INT_FRAME;
		val &= ~VIDINTCON0_INT_ENABLE;

		writel(val, ctx->regs + VIDINTCON0);
	}
}

static struct exynos_drm_manager_ops fimd_manager_ops = {
	.apply = fimd_apply,
	.commit = fimd_commit,
	.enable_vblank = fimd_enable_vblank,
	.disable_vblank = fimd_disable_vblank,
};

static void fimd_win_mode_set(struct device *dev,
			      struct exynos_drm_overlay *overlay)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win;
	unsigned long offset;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (!overlay) {
		dev_err(dev, "overlay is NULL\n");
		return;
	}

	win = overlay->zpos;
	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win > WINDOWS_NR)
		return;

	if(win == ctx->default_win) {
		for(ctx->idx = 0;ctx->idx < MAX_NR_PANELS;ctx->idx++) {
			if (ctx->panel[ctx->idx].timing.xres == -1 &&
				ctx->panel[ctx->idx].timing.yres == -1) {
					DRM_ERROR("Invalid panel parameters");
					ctx->idx = 0; /* Reset to first panel index*/
					break;
				}
			if (ctx->panel[ctx->idx].timing.xres == overlay->fb_width &&
				ctx->panel[ctx->idx].timing.yres == overlay->fb_height)
					break;
		}
	}

	offset = overlay->fb_x * (overlay->bpp >> 3);
	offset += overlay->fb_y * overlay->fb_pitch;

	DRM_DEBUG_KMS("offset = 0x%lx, pitch = %x\n",
		offset, overlay->fb_pitch);

	win_data = &ctx->win_data[win];

	win_data->offset_x = overlay->crtc_x;
	win_data->offset_y = overlay->crtc_y;
	win_data->ovl_width = overlay->crtc_width;
	win_data->ovl_height = overlay->crtc_height;
	win_data->fb_width = overlay->fb_width;
	win_data->fb_height = overlay->fb_height;
	win_data->fb_pitch = overlay->fb_pitch;
	win_data->dma_addr = overlay->dma_addr[0] + offset;
	win_data->vaddr = overlay->vaddr[0] + offset;
	win_data->bpp = overlay->bpp;
	win_data->buf_offsize = overlay->fb_pitch -
		(overlay->fb_width * (overlay->bpp >> 3));
	win_data->line_size = overlay->fb_width * (overlay->bpp >> 3);

	DRM_DEBUG_KMS("offset_x = %d, offset_y = %d\n",
			win_data->offset_x, win_data->offset_y);
	DRM_DEBUG_KMS("ovl_width = %d, ovl_height = %d\n",
			win_data->ovl_width, win_data->ovl_height);
	DRM_DEBUG_KMS("paddr = 0x%lx, vaddr = 0x%lx\n",
			(unsigned long)win_data->dma_addr,
			(unsigned long)win_data->vaddr);
	DRM_DEBUG_KMS("fb_width = %d, crtc_width = %d\n",
			overlay->fb_width, overlay->crtc_width);
}

static void fimd_win_set_pixfmt(struct device *dev, unsigned int win)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data = &ctx->win_data[win];
	unsigned long val;
	unsigned long bytes;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	val = WINCONx_ENWIN;

	switch (win_data->bpp) {
	case 1:
		val |= WINCON0_BPPMODE_1BPP;
		val |= WINCONx_BITSWP;
		bytes = win_data->fb_width >> 3;
		break;
	case 2:
		val |= WINCON0_BPPMODE_2BPP;
		val |= WINCONx_BITSWP;
		bytes = win_data->fb_width >> 2;
		break;
	case 4:
		val |= WINCON0_BPPMODE_4BPP;
		val |= WINCONx_BITSWP;
		bytes = win_data->fb_width >> 1;
		break;
	case 8:
		val |= WINCON0_BPPMODE_8BPP_PALETTE;
		val |= WINCONx_BYTSWP;
		bytes = win_data->fb_width;
		break;
	case 16:
		val |= WINCON0_BPPMODE_16BPP_565;
		val |= WINCONx_HAWSWP;
		bytes = win_data->fb_width << 1;
		break;
	case 24:
		val |= WINCON0_BPPMODE_24BPP_888;
		val |= WINCONx_WSWP;
		bytes = win_data->fb_width * 3;
		break;
	case 32:
		val |= WINCON1_BPPMODE_28BPP_A4888
			| WINCON1_BLD_PIX | WINCON1_ALPHA_SEL;
		val |= WINCONx_WSWP;
		bytes = win_data->fb_width << 2;
		break;
	default:
		DRM_DEBUG_KMS("invalid pixel size so using unpacked 24bpp.\n");
		bytes = win_data->fb_width * 3;
		val |= WINCON0_BPPMODE_24BPP_888;
		val |= WINCONx_WSWP;
		break;
	}

	/*
	 * Adjust the burst size based on the number of bytes to be read.
	 * Each WORD of the BURST is 8 bytes long. There are 3 BURST sizes
	 * supported by fimd.
	 * WINCONx_BURSTLEN_4WORD = 32 bytes
	 * WINCONx_BURSTLEN_8WORD = 64 bytes
	 * WINCONx_BURSTLEN_16WORD = 128 bytes
	 */
	if (win_data->fb_width <= 64)
		val |= WINCONx_BURSTLEN_4WORD;
	else
		val |= WINCONx_BURSTLEN_16WORD;

	DRM_DEBUG_KMS("bpp = %d\n", win_data->bpp);

	writel(val, ctx->regs + WINCON(win));
}

static void fimd_win_set_colkey(struct device *dev, unsigned int win)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	unsigned int keycon0 = 0, keycon1 = 0;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	keycon0 = ~(WxKEYCON0_KEYBL_EN | WxKEYCON0_KEYEN_F |
			WxKEYCON0_DIRCON) | WxKEYCON0_COMPKEY(0);

	keycon1 = WxKEYCON1_COLVAL(0xffffffff);

	writel(keycon0, ctx->regs + WKEYCON0_BASE(win));
	writel(keycon1, ctx->regs + WKEYCON1_BASE(win));
}

static void mie_set_6bit_dithering(struct fimd_context *ctx)
{
	struct fb_videomode *timing = &ctx->panel->timing;
	unsigned long val;
	int i;

	writel(MIE_HRESOL(timing->xres) | MIE_VRESOL(timing->yres) |
				MIE_MODE_UI, ctx->regs_mie + MIE_CTRL1);

	writel(MIE_WINHADDR0(0) | MIE_WINHADDR1(timing->xres),
						ctx->regs_mie + MIE_WINHADDR);
	writel(MIE_WINVADDR0(0) | MIE_WINVADDR1(timing->yres),
						ctx->regs_mie + MIE_WINVADDR);

	val = (timing->xres + timing->left_margin +
			timing->right_margin + timing->hsync_len) *
	      (timing->yres + timing->upper_margin +
			timing->lower_margin + timing->vsync_len) /
							(MIE_PWMCLKVAL + 1);
	writel(PWMCLKCNT(val), ctx->regs_mie + MIE_PWMCLKCNT);

	writel((MIE_VBPD(timing->upper_margin)) |
		MIE_VFPD(timing->lower_margin) |
		MIE_VSPW(timing->vsync_len), ctx->regs_mie + MIE_PWMVIDTCON1);

	writel(MIE_HBPD(timing->left_margin) |
		MIE_HFPD(timing->right_margin) |
		MIE_HSPW(timing->hsync_len), ctx->regs_mie + MIE_PWMVIDTCON2);

	writel(MIE_DITHCON_EN | MIE_RGB6MODE,
					ctx->regs_mie + MIE_AUXCON);

	/* Bypass MIE image brightness enhancement */
	for (i = 0; i <= 0x30; i += 4) {
		writel(0, ctx->regs_mie + 0x100 + i);
		writel(0, ctx->regs_mie + 0x200 + i);
	}
}

static void fimd_win_commit(struct device *dev, int zpos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win = zpos;
	unsigned long val, alpha, size;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->suspended)
		return;

	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win > WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	/*
	 * SHADOWCON register is used for enabling timing.
	 *
	 * for example, once only width value of a register is set,
	 * if the dma is started then fimd hardware could malfunction so
	 * with protect window setting, the register fields with prefix '_F'
	 * wouldn't be updated at vsync also but updated once unprotect window
	 * is set.
	 */

	/* protect windows */
	val = readl(ctx->regs + SHADOWCON);
	val |= SHADOWCON_WINx_PROTECT(win);
	writel(val, ctx->regs + SHADOWCON);

	/* buffer start address */
	val = (unsigned long)win_data->dma_addr;
	writel(val, ctx->regs + VIDWx_BUF_START(win, 0));

	/* buffer end address */
	size = win_data->fb_height * win_data->fb_pitch;
	val = (unsigned long)(win_data->dma_addr + size);
	writel(val, ctx->regs + VIDWx_BUF_END(win, 0));

	DRM_DEBUG_KMS("start addr = 0x%lx, end addr = 0x%lx, size = 0x%lx\n",
			(unsigned long)win_data->dma_addr, val, size);
	DRM_DEBUG_KMS("ovl_width = %d, ovl_height = %d\n",
			win_data->ovl_width, win_data->ovl_height);

	/* buffer size */
	val = VIDW_BUF_SIZE_OFFSET(win_data->buf_offsize) |
		VIDW_BUF_SIZE_PAGEWIDTH(win_data->line_size);
	writel(val, ctx->regs + VIDWx_BUF_SIZE(win, 0));

	/* OSD position */
	val = VIDOSDxA_TOPLEFT_X(win_data->offset_x) |
		VIDOSDxA_TOPLEFT_Y(win_data->offset_y);
	writel(val, ctx->regs + VIDOSD_A(win));

	val = VIDOSDxB_BOTRIGHT_X(win_data->offset_x +
					win_data->ovl_width - 1) |
		VIDOSDxB_BOTRIGHT_Y(win_data->offset_y +
					win_data->ovl_height - 1);
	writel(val, ctx->regs + VIDOSD_B(win));

	DRM_DEBUG_KMS("osd pos: tx = %d, ty = %d, bx = %d, by = %d\n",
			win_data->offset_x, win_data->offset_y,
			win_data->offset_x + win_data->ovl_width - 1,
			win_data->offset_y + win_data->ovl_height - 1);

	/* hardware window 0 doesn't support alpha channel. */
	if (win != 0) {
		/* OSD alpha */
		alpha = VIDISD14C_ALPHA1_R(0xf) |
			VIDISD14C_ALPHA1_G(0xf) |
			VIDISD14C_ALPHA1_B(0xf);

		writel(alpha, ctx->regs + VIDOSD_C(win));
	}

	/* OSD size */
	if (win != 3 && win != 4) {
		u32 offset = VIDOSD_D(win);
		if (win == 0)
			offset = VIDOSD_C_SIZE_W0;
		val = win_data->ovl_width * win_data->ovl_height;
		writel(val, ctx->regs + offset);

		DRM_DEBUG_KMS("osd size = 0x%x\n", (unsigned int)val);
	}

	fimd_win_set_pixfmt(dev, win);

	/* hardware window 0 doesn't support color key. */
	if (win != 0)
		fimd_win_set_colkey(dev, win);

	/* wincon */
	val = readl(ctx->regs + WINCON(win));
	val |= WINCONx_ENWIN;
	writel(val, ctx->regs + WINCON(win));

	mie_set_6bit_dithering(ctx);

	/* Enable DMA channel and unprotect windows */
	val = readl(ctx->regs + SHADOWCON);
	val |= SHADOWCON_CHx_ENABLE(win);
	val &= ~SHADOWCON_WINx_PROTECT(win);
	writel(val, ctx->regs + SHADOWCON);

	win_data->enabled = true;
}

static void fimd_win_disable(struct device *dev, int zpos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win = zpos;
	u32 val;

	DRM_DEBUG_KMS("%s\n", __FILE__);
	if (ctx->suspended)
		return;

	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win > WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	/* protect windows */
	val = readl(ctx->regs + SHADOWCON);
	val |= SHADOWCON_WINx_PROTECT(win);
	writel(val, ctx->regs + SHADOWCON);

	/* wincon */
	val = readl(ctx->regs + WINCON(win));
	val &= ~WINCONx_ENWIN;
	writel(val, ctx->regs + WINCON(win));

	/* unprotect windows */
	val = readl(ctx->regs + SHADOWCON);
	val &= ~SHADOWCON_CHx_ENABLE(win);
	val &= ~SHADOWCON_WINx_PROTECT(win);
	writel(val, ctx->regs + SHADOWCON);

	win_data->enabled = false;
}

static struct exynos_drm_overlay_ops fimd_overlay_ops = {
	.mode_set = fimd_win_mode_set,
	.commit = fimd_win_commit,
	.disable = fimd_win_disable,
};

static struct exynos_drm_manager fimd_manager = {
	.pipe		= -1,
	.ops		= &fimd_manager_ops,
	.overlay_ops	= &fimd_overlay_ops,
	.display_ops	= &fimd_display_ops,
};

static irqreturn_t fimd_irq_handler(int irq, void *dev_id)
{
	struct fimd_context *ctx = (struct fimd_context *)dev_id;
	struct exynos_drm_subdrv *subdrv = &ctx->subdrv;
	struct drm_device *drm_dev = subdrv->drm_dev;
	struct exynos_drm_manager *manager = subdrv->manager;
	u32 val;

	val = readl(ctx->regs + VIDINTCON1);

	if (val & VIDINTCON1_INT_FRAME)
		/* VSYNC interrupt */
		writel(VIDINTCON1_INT_FRAME, ctx->regs + VIDINTCON1);

	/* check the crtc is detached already from encoder */
	if (manager->pipe < 0)
		goto out;

	drm_handle_vblank(drm_dev, manager->pipe);
	exynos_drm_crtc_finish_pageflip(drm_dev, manager->pipe);

out:
	return IRQ_HANDLED;
}

static int fimd_subdrv_probe(struct drm_device *drm_dev, struct device *dev)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	/*
	 * enable drm irq mode.
	 * - with irq_enabled = 1, we can use the vblank feature.
	 *
	 * P.S. note that we wouldn't use drm irq handler but
	 *	just specific driver own one instead because
	 *	drm framework supports only one irq handler.
	 */
	drm_dev->irq_enabled = 1;

	/*
	 * with vblank_disable_allowed = 1, vblank interrupt will be disabled
	 * by drm timer once a current process gives up ownership of
	 * vblank event.(after drm_vblank_put function is called)
	 */
	drm_dev->vblank_disable_allowed = 1;

	return 0;
}

static void fimd_subdrv_remove(struct drm_device *drm_dev)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* TODO. */
}

static int fimd_calc_clkdiv(struct fimd_context *ctx,
			    struct fb_videomode *timing)
{
	unsigned long clk = clk_get_rate(ctx->lcd_clk);
	u32 retrace;
	u32 clkdiv;
	u32 best_framerate = 0;
	u32 framerate;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	retrace = timing->left_margin + timing->hsync_len +
				timing->right_margin + timing->xres;
	retrace *= timing->upper_margin + timing->vsync_len +
				timing->lower_margin + timing->yres;

	/* default framerate is 60Hz */
	if (!timing->refresh)
		timing->refresh = 60;

	clk /= retrace;

	for (clkdiv = 1; clkdiv < 0x100; clkdiv++) {
		int tmp;

		/* get best framerate */
		framerate = clk / clkdiv;
		tmp = timing->refresh - framerate;
		if (tmp < 0) {
			best_framerate = framerate;
			continue;
		} else {
			if (!best_framerate)
				best_framerate = framerate;
			else if (tmp < (best_framerate - framerate))
				best_framerate = framerate;
			break;
		}
	}

	return clkdiv;
}

static void fimd_clear_win(struct fimd_context *ctx, int win)
{
	u32 val;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	writel(0, ctx->regs + WINCON(win));
	writel(0, ctx->regs + VIDOSD_A(win));
	writel(0, ctx->regs + VIDOSD_B(win));
	writel(0, ctx->regs + VIDOSD_C(win));

	if (win == 1 || win == 2)
		writel(0, ctx->regs + VIDOSD_D(win));

	val = readl(ctx->regs + SHADOWCON);
	val &= ~SHADOWCON_WINx_PROTECT(win);
	writel(val, ctx->regs + SHADOWCON);
}

static int fimd_power_on(struct fimd_context *ctx, bool enable)
{
	struct exynos_drm_subdrv *subdrv = &ctx->subdrv;
	struct device *dev = subdrv->dev;
	struct exynos_drm_fimd_pdata *pdata = dev->platform_data;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (enable != false && enable != true)
		return -EINVAL;

	if (enable) {
		int ret;

		ret = clk_enable(ctx->bus_clk);
		if (ret < 0)
			return ret;

		ret = clk_enable(ctx->lcd_clk);
		if  (ret < 0) {
			clk_disable(ctx->bus_clk);
			return ret;
		}

		ctx->suspended = false;

		/* if vblank was enabled status, enable it again. */
		if (test_and_clear_bit(0, &ctx->irq_flags))
			fimd_enable_vblank(dev);

		fimd_apply(dev);

		if (pdata->panel_type == DP_LCD)
			writel(MIE_CLK_ENABLE, ctx->regs + DPCLKCON);
	} else {
		clk_disable(ctx->lcd_clk);
		clk_disable(ctx->bus_clk);

		ctx->suspended = true;

		if (pdata->panel_type == DP_LCD)
			writel(0, ctx->regs + DPCLKCON);
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_IOMMU
static int iommu_init(struct platform_device *pdev)
{
	struct platform_device *pds;

	pds = find_sysmmu_dt(pdev, "sysmmu");
	if (pds==NULL) {
		printk(KERN_ERR "No sysmmu found\n");
		return -1;
	}

	platform_set_sysmmu(&pds->dev, &pdev->dev);
	exynos_drm_common_mapping = s5p_create_iommu_mapping(&pdev->dev,
					0x20000000, SZ_256M, 4,
					exynos_drm_common_mapping);

	if (!exynos_drm_common_mapping) {
		printk(KERN_ERR "IOMMU mapping not created\n");
		return -1;
	}

	return 0;
}

static void iommu_deinit(struct platform_device *pdev)
{
	s5p_destroy_iommu_mapping(&pdev->dev);
	DRM_DEBUG("released the IOMMU mapping\n");

	return;
}
#endif

static int __devinit fimd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fimd_context *ctx;
	struct exynos_drm_subdrv *subdrv;
	struct exynos_drm_fimd_pdata *pdata;
	struct exynos_drm_panel_info *panel;
	struct resource *res;
	struct clk *clk_parent;
	int win,i;
	int ret = -EINVAL;

#ifdef CONFIG_EXYNOS_IOMMU
	ret = iommu_init(pdev);
	if (ret < 0) {
		dev_err(dev, "failed to initialize IOMMU\n");
		return -ENODEV;
	}
#endif
	DRM_DEBUG_KMS("%s\n", __FILE__);

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(dev, "no platform data specified\n");
		return -EINVAL;
	}

	panel = pdata->panel;
	if (!panel) {
		dev_err(dev, "panel is null.\n");
		return -EINVAL;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->bus_clk = clk_get(dev, "fimd");
	if (IS_ERR(ctx->bus_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		ret = PTR_ERR(ctx->bus_clk);
		goto err_clk_get;
	}

	ctx->lcd_clk = clk_get(dev, "sclk_fimd");
	if (IS_ERR(ctx->lcd_clk)) {
		dev_err(dev, "failed to get lcd clock\n");
		ret = PTR_ERR(ctx->lcd_clk);
		goto err_bus_clk;
	}

	clk_parent = clk_get(NULL, "sclk_vpll");
	if (IS_ERR(clk_parent)) {
		ret = PTR_ERR(clk_parent);
		goto err_clk;
	}

	if (clk_set_parent(ctx->lcd_clk, clk_parent)) {
		ret = PTR_ERR(ctx->lcd_clk);
		goto err_clk;
	}

	if (clk_set_rate(ctx->lcd_clk, pdata->clock_rate)) {
		ret = PTR_ERR(ctx->lcd_clk);
		goto err_clk;
	}

	clk_put(clk_parent);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to find registers\n");
		ret = -ENOENT;
		goto err_clk;
	}

	ctx->regs_res = request_mem_region(res->start, resource_size(res),
					   dev_name(dev));
	if (!ctx->regs_res) {
		dev_err(dev, "failed to claim register region\n");
		ret = -ENOENT;
		goto err_clk;
	}

	ctx->regs = ioremap(res->start, resource_size(res));
	if (!ctx->regs) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto err_req_region_io;
	}

	ctx->regs_mie = ioremap(MIE_BASE_ADDRESS, 0x400);
	if (!ctx->regs_mie) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto err_req_region_io_mie;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (!res) {
		dev_err(dev, "irq request failed.\n");
		goto err_req_region_irq;
	}

	ctx->irq = res->start;

	ret = request_irq(ctx->irq, fimd_irq_handler, 0, "drm_fimd", ctx);
	if (ret < 0) {
		dev_err(dev, "irq request failed.\n");
		goto err_req_irq;
	}

	ctx->vidcon0 = pdata->vidcon0;
	ctx->vidcon1 = pdata->vidcon1;
	ctx->default_win = pdata->default_win;
	ctx->panel = panel;

	subdrv = &ctx->subdrv;

	subdrv->dev = dev;
	subdrv->manager = &fimd_manager;
	subdrv->probe = fimd_subdrv_probe;
	subdrv->remove = fimd_subdrv_remove;

	mutex_init(&ctx->lock);

	platform_set_drvdata(pdev, ctx);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	for (i = 0;i < MAX_NR_PANELS;i++) {
		if(panel[i].timing.xres == -1 && panel[i].timing.yres == -1)
			break;

		ctx->clkdiv[i] = fimd_calc_clkdiv(ctx, &panel[i].timing);
		panel[i].timing.pixclock = clk_get_rate(ctx->lcd_clk) / ctx->clkdiv[i];
		DRM_DEBUG_KMS("pixel clock = %d, clkdiv = %d\n for panel[%d]",
				panel[i].timing.pixclock, ctx->clkdiv[i],i);
	}

	for (win = 0; win < WINDOWS_NR; win++)
		fimd_clear_win(ctx, win);

	if (pdata->panel_type == DP_LCD)
		writel(MIE_CLK_ENABLE, ctx->regs + DPCLKCON);

	exynos_drm_subdrv_register(subdrv);

	return 0;

err_req_irq:
err_req_region_irq:
	iounmap(ctx->regs_mie);

err_req_region_io_mie:
	iounmap(ctx->regs);

err_req_region_io:
	release_resource(ctx->regs_res);
	kfree(ctx->regs_res);

err_clk:
	clk_disable(ctx->lcd_clk);
	clk_put(ctx->lcd_clk);

err_bus_clk:
	clk_disable(ctx->bus_clk);
	clk_put(ctx->bus_clk);

err_clk_get:
#ifdef CONFIG_EXYNOS_IOMMU
	iommu_deinit(pdev);
#endif
	kfree(ctx);
	return ret;
}

static int __devexit fimd_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fimd_context *ctx = platform_get_drvdata(pdev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	exynos_drm_subdrv_unregister(&ctx->subdrv);

	if (ctx->suspended)
		goto out;

	clk_disable(ctx->lcd_clk);
	clk_disable(ctx->bus_clk);

	pm_runtime_set_suspended(dev);
	pm_runtime_put_sync(dev);

out:
	pm_runtime_disable(dev);

	clk_put(ctx->lcd_clk);
	clk_put(ctx->bus_clk);

	iounmap(ctx->regs_mie);
	iounmap(ctx->regs);
	release_resource(ctx->regs_res);
	kfree(ctx->regs_res);
	free_irq(ctx->irq, ctx);
#ifdef CONFIG_EXYNOS_IOMMU
	iommu_deinit(pdev);
#endif
	kfree(ctx);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int fimd_suspend(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	/*
	 * do not use pm_runtime_suspend(). if pm_runtime_suspend() is
	 * called here, an error would be returned by that interface
	 * because the usage_count of pm runtime is more than 1.
	 */
	return fimd_power_on(ctx, false);
}

static int fimd_resume(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	/*
	 * if entered to sleep when lcd panel was on, the usage_count
	 * of pm runtime would still be 1 so in this case, fimd driver
	 * should be on directly not drawing on pm runtime interface.
	 */
	if (!pm_runtime_suspended(dev))
		return fimd_power_on(ctx, true);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int fimd_runtime_suspend(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	return fimd_power_on(ctx, false);
}

static int fimd_runtime_resume(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	return fimd_power_on(ctx, true);
}
#endif

static struct platform_device_id exynos_drm_driver_ids[] = {
	{
		.name		= "exynos4-fb",
	}, {
		.name		= "exynos5-fb",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_drm_driver_ids);

static const struct dev_pm_ops fimd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(fimd_suspend, fimd_resume)
	SET_RUNTIME_PM_OPS(fimd_runtime_suspend, fimd_runtime_resume, NULL)
};

struct platform_driver fimd_driver = {
	.probe		= fimd_probe,
	.remove		= __devexit_p(fimd_remove),
	.id_table       = exynos_drm_driver_ids,
	.driver		= {
		.name	= "exynos-drm-fimd",
		.owner	= THIS_MODULE,
		.pm	= &fimd_pm_ops,
	},
};
