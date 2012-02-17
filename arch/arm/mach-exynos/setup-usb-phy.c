/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/regs-pmu.h>
#include <mach/regs-usb-phy.h>
#include <mach/regs-usb3-exynos-drd-phy.h>
#include <plat/cpu.h>
#include <plat/usb-phy.h>

#define PHY_ENABLE	(1 << 0)
#define PHY_DISABLE	(0 << 0)

enum usb_phy_type {
	USB_PHY		= (0x1 << 0),
	USB_PHY0	= (0x1 << 0),
	USB_PHY1	= (0x1 << 1),
	USB_PHY_HSIC0	= (0x1 << 1),
	USB_PHY_HSIC1	= (0x1 << 2),
};

struct exynos_usb_phy {
	u8 phy0_usage;
	u8 phy1_usage;
	u8 phy2_usage;
	unsigned long flags;
};

static struct exynos_usb_phy usb_phy_control;
static atomic_t host_usage;
static DEFINE_SPINLOCK(phy_lock);

static int exynos4_usb_host_phy_is_on(void)
{
	return (readl(EXYNOS4_PHYPWR) & PHY1_STD_ANALOG_POWERDOWN) ? 0 : 1;
}

static void exynos_usb_phy_control(enum usb_phy_type phy_type , int on)
{
	spin_lock(&phy_lock);

	if (phy_type & USB_PHY0) {
		if (on == PHY_ENABLE) {
			usb_phy_control.phy0_usage++;
			writel(PHY_ENABLE, EXYNOS5_USBDEV_PHY_CONTROL);
		} else if (on == PHY_DISABLE && (--usb_phy_control.phy0_usage) == 0)
			writel(PHY_DISABLE, EXYNOS5_USBDEV_PHY_CONTROL);
	}

	spin_unlock(&phy_lock);
}

static int exynos4_usb_phy1_init(struct platform_device *pdev)
{
	struct clk *otg_clk;
	struct clk *xusbxti_clk;
	u32 phyclk;
	u32 rstcon;
	int err;

	atomic_inc(&host_usage);

	otg_clk = clk_get(&pdev->dev, "otg");
	if (IS_ERR(otg_clk)) {
		dev_err(&pdev->dev, "Failed to get otg clock\n");
		return PTR_ERR(otg_clk);
	}

	err = clk_enable(otg_clk);
	if (err) {
		clk_put(otg_clk);
		return err;
	}

	if (exynos4_usb_host_phy_is_on())
		return 0;

	writel(readl(S5P_USBHOST_PHY_CONTROL) | S5P_USBHOST_PHY_ENABLE,
			S5P_USBHOST_PHY_CONTROL);

	/* set clock frequency for PLL */
	phyclk = readl(EXYNOS4_PHYCLK) & ~CLKSEL_MASK;

	xusbxti_clk = clk_get(&pdev->dev, "xusbxti");
	if (xusbxti_clk && !IS_ERR(xusbxti_clk)) {
		switch (clk_get_rate(xusbxti_clk)) {
		case 12 * MHZ:
			phyclk |= CLKSEL_12M;
			break;
		case 24 * MHZ:
			phyclk |= CLKSEL_24M;
			break;
		default:
		case 48 * MHZ:
			/* default reference clock */
			break;
		}
		clk_put(xusbxti_clk);
	}

	writel(phyclk, EXYNOS4_PHYCLK);

	/* floating prevention logic: disable */
	writel((readl(EXYNOS4_PHY1CON) | FPENABLEN), EXYNOS4_PHY1CON);

	/* set to normal HSIC 0 and 1 of PHY1 */
	writel((readl(EXYNOS4_PHYPWR) & ~PHY1_HSIC_NORMAL_MASK),
			EXYNOS4_PHYPWR);

	/* set to normal standard USB of PHY1 */
	writel((readl(EXYNOS4_PHYPWR) & ~PHY1_STD_NORMAL_MASK), EXYNOS4_PHYPWR);

	/* reset all ports of both PHY and Link */
	rstcon = readl(EXYNOS4_RSTCON) | HOST_LINK_PORT_SWRST_MASK |
		PHY1_SWRST_MASK;
	writel(rstcon, EXYNOS4_RSTCON);
	udelay(10);

	rstcon &= ~(HOST_LINK_PORT_SWRST_MASK | PHY1_SWRST_MASK);
	writel(rstcon, EXYNOS4_RSTCON);
	udelay(80);

	clk_disable(otg_clk);
	clk_put(otg_clk);

	return 0;
}

static int exynos4_usb_phy1_exit(struct platform_device *pdev)
{
	struct clk *otg_clk;
	int err;

	if (atomic_dec_return(&host_usage) > 0)
		return 0;

	otg_clk = clk_get(&pdev->dev, "otg");
	if (IS_ERR(otg_clk)) {
		dev_err(&pdev->dev, "Failed to get otg clock\n");
		return PTR_ERR(otg_clk);
	}

	err = clk_enable(otg_clk);
	if (err) {
		clk_put(otg_clk);
		return err;
	}

	writel((readl(EXYNOS4_PHYPWR) | PHY1_STD_ANALOG_POWERDOWN),
			EXYNOS4_PHYPWR);

	writel(readl(S5P_USBHOST_PHY_CONTROL) & ~S5P_USBHOST_PHY_ENABLE,
			S5P_USBHOST_PHY_CONTROL);

	clk_disable(otg_clk);
	clk_put(otg_clk);

	return 0;
}

static int exynos5_usb_phy30_init(struct platform_device *pdev)
{
	u32 reg;

	exynos_usb_phy_control(USB_PHY0, PHY_ENABLE);

	/* Reset USB 3.0 PHY */
	writel(0x087fffc0, EXYNOS_USB3_LINKSYSTEM);
	writel(0x00000000, EXYNOS_USB3_PHYREG0);
	writel(0x24d4e6e4, EXYNOS_USB3_PHYPARAM0);
	writel(0x03fff820, EXYNOS_USB3_PHYPARAM1);
	writel(0x00000000, EXYNOS_USB3_PHYBATCHG);
	writel(0x00000000, EXYNOS_USB3_PHYRESUME);
	/* REVISIT : Over-current pin is inactive on SMDK5250 */
	if (soc_is_exynos5250())
		writel((readl(EXYNOS_USB3_LINKPORT) & ~(0x3<<4)) |
			(0x3<<2), EXYNOS_USB3_LINKPORT);

	/* UTMI Power Control */
	writel(EXYNOS_USB3_PHYUTMI_OTGDISABLE, EXYNOS_USB3_PHYUTMI);

	/* Set 100MHz external clock */
	reg = EXYNOS_USB3_PHYCLKRST_PORTRESET |
		/* HS PLL uses ref_pad_clk{p,m} or ref_alt_clk_{p,m}
		* as reference */
		EXYNOS_USB3_PHYCLKRST_REFCLKSEL(2) |
		/* Digital power supply in normal operating mode */
		EXYNOS_USB3_PHYCLKRST_RETENABLEN |
		/* 0x27-100MHz, 0x2a-24MHz, 0x31-20MHz, 0x38-19.2MHz */
		EXYNOS_USB3_PHYCLKRST_FSEL(0x27) |
		/* 0x19-100MHz, 0x68-24MHz, 0x7d-20Mhz */
		EXYNOS_USB3_PHYCLKRST_MPLL_MULTIPLIER(0x19) |
		/* Enable ref clock for SS function */
		EXYNOS_USB3_PHYCLKRST_REF_SSP_EN |
		/* Enable spread spectrum */
		EXYNOS_USB3_PHYCLKRST_SSC_EN;

	writel(reg, EXYNOS_USB3_PHYCLKRST);

	udelay(10);

	reg &= ~(EXYNOS_USB3_PHYCLKRST_PORTRESET);
	writel(reg, EXYNOS_USB3_PHYCLKRST);

	return 0;
}

static int exynos5_usb_phy30_exit(struct platform_device *pdev)
{
	u32 reg;

	reg = EXYNOS_USB3_PHYUTMI_OTGDISABLE |
		EXYNOS_USB3_PHYUTMI_FORCESUSPEND |
		EXYNOS_USB3_PHYUTMI_FORCESLEEP;
	writel(reg, EXYNOS_USB3_PHYUTMI);

	exynos_usb_phy_control(USB_PHY0, PHY_DISABLE);

	return 0;
}

int s5p_usb_phy_init(struct platform_device *pdev, int type)
{
	if (type == S5P_USB_PHY_HOST)
		return exynos4_usb_phy1_init(pdev);
	else if (type == S5P_USB_PHY_DRD)
		return exynos5_usb_phy30_init(pdev);

	return -EINVAL;
}

int s5p_usb_phy_exit(struct platform_device *pdev, int type)
{
	if (type == S5P_USB_PHY_HOST)
		return exynos4_usb_phy1_exit(pdev);
	else if (type == S5P_USB_PHY_DRD)
		return exynos5_usb_phy30_exit(pdev);

	return -EINVAL;
}
