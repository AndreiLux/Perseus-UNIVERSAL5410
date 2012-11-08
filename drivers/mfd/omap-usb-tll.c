/**
 * omap-usb-tll.c - The USB TLL driver for OMAP EHCI & OHCI
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Keshava Munegowda <keshava_mgowda@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/platform_data/usb-omap.h>

#define USBTLL_DRIVER_NAME	"usbhs_tll"

/* TLL Register Set */
#define	OMAP_USBTLL_REVISION				(0x00)
#define	OMAP_USBTLL_SYSCONFIG				(0x10)
#define	OMAP_USBTLL_SYSCONFIG_CACTIVITY			(1 << 8)
#define	OMAP_USBTLL_SYSCONFIG_SIDLEMODE			(1 << 3)
#define	OMAP_USBTLL_SYSCONFIG_ENAWAKEUP			(1 << 2)
#define	OMAP_USBTLL_SYSCONFIG_SOFTRESET			(1 << 1)
#define	OMAP_USBTLL_SYSCONFIG_AUTOIDLE			(1 << 0)

#define	OMAP_USBTLL_SYSSTATUS				(0x14)
#define	OMAP_USBTLL_SYSSTATUS_RESETDONE			(1 << 0)

#define	OMAP_USBTLL_IRQSTATUS				(0x18)
#define	OMAP_USBTLL_IRQENABLE				(0x1C)

#define	OMAP_TLL_SHARED_CONF				(0x30)
#define	OMAP_TLL_SHARED_CONF_USB_90D_DDR_EN		(1 << 6)
#define	OMAP_TLL_SHARED_CONF_USB_180D_SDR_EN		(1 << 5)
#define	OMAP_TLL_SHARED_CONF_USB_DIVRATION		(1 << 2)
#define	OMAP_TLL_SHARED_CONF_FCLK_REQ			(1 << 1)
#define	OMAP_TLL_SHARED_CONF_FCLK_IS_ON			(1 << 0)

#define	OMAP_TLL_CHANNEL_CONF(num)			(0x040 + 0x004 * num)
#define OMAP_TLL_CHANNEL_CONF_FSLSMODE_SHIFT		24
#define OMAP_TLL_CHANNEL_CONF_DRVVBUS			(1 << 16)
#define OMAP_TLL_CHANNEL_CONF_CHRGVBUS			(1 << 15)
#define	OMAP_TLL_CHANNEL_CONF_ULPINOBITSTUFF		(1 << 11)
#define	OMAP_TLL_CHANNEL_CONF_ULPI_ULPIAUTOIDLE		(1 << 10)
#define	OMAP_TLL_CHANNEL_CONF_UTMIAUTOIDLE		(1 << 9)
#define	OMAP_TLL_CHANNEL_CONF_ULPIDDRMODE		(1 << 8)
#define OMAP_TLL_CHANNEL_CONF_MODE_TRANSPARENT_UTMI	(2 << 1)
#define OMAP_TLL_CHANNEL_CONF_CHANMODE_FSLS		(1 << 1)
#define	OMAP_TLL_CHANNEL_CONF_CHANEN			(1 << 0)

#define OMAP_TLL_FSLSMODE_6PIN_PHY_DAT_SE0		0x0
#define OMAP_TLL_FSLSMODE_6PIN_PHY_DP_DM		0x1
#define OMAP_TLL_FSLSMODE_3PIN_PHY			0x2
#define OMAP_TLL_FSLSMODE_4PIN_PHY			0x3
#define OMAP_TLL_FSLSMODE_6PIN_TLL_DAT_SE0		0x4
#define OMAP_TLL_FSLSMODE_6PIN_TLL_DP_DM		0x5
#define OMAP_TLL_FSLSMODE_3PIN_TLL			0x6
#define OMAP_TLL_FSLSMODE_4PIN_TLL			0x7
#define OMAP_TLL_FSLSMODE_2PIN_TLL_DAT_SE0		0xA
#define OMAP_TLL_FSLSMODE_2PIN_DAT_DP_DM		0xB

#define	OMAP_TLL_ULPI_FUNCTION_CTRL(num)		(0x804 + 0x100 * num)
#define	OMAP_TLL_ULPI_INTERFACE_CTRL(num)		(0x807 + 0x100 * num)
#define	OMAP_TLL_ULPI_OTG_CTRL(num)			(0x80A + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_EN_RISE(num)			(0x80D + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_EN_FALL(num)			(0x810 + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_STATUS(num)			(0x813 + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_LATCH(num)			(0x814 + 0x100 * num)
#define	OMAP_TLL_ULPI_DEBUG(num)			(0x815 + 0x100 * num)
#define	OMAP_TLL_ULPI_SCRATCH_REGISTER(num)		(0x816 + 0x100 * num)

#define REV2_TLL_CHANNEL_COUNT				2
#define DEFAULT_TLL_CHANNEL_COUNT			3

/* Update if any chip has more */
#define MAX_TLL_CHANNEL_COUNT				3

#define OMAP_TLL_CHANNEL_1_EN_MASK			(1 << 0)
#define OMAP_TLL_CHANNEL_2_EN_MASK			(1 << 1)
#define OMAP_TLL_CHANNEL_3_EN_MASK			(1 << 2)

/* Values of USBTLL_REVISION - Note: these are not given in the TRM */
#define OMAP_USBTLL_REV1		0x00000015	/* OMAP3 */
#define OMAP_USBTLL_REV2		0x00000018	/* OMAP 3630 */
#define OMAP_USBTLL_REV3		0x00000004	/* OMAP4 */
#define OMAP_USBTLL_REV4		0x6		/* OMAP5 */

#define is_ehci_tll_mode(x)	(x == OMAP_EHCI_PORT_MODE_TLL)

/* only PHY and UNUSED modes don't need TLL */
#define mode_needs_tll(x)	((x != OMAP_USBHS_PORT_MODE_UNUSED) && \
				 (x != OMAP_EHCI_PORT_MODE_PHY))

struct usbtll_omap {
	void __iomem				*base;
	int					nch;	/* number of channels */
	struct clk				*ch_clk[MAX_TLL_CHANNEL_COUNT];
	struct usbtll_omap_platform_data	*pdata;
	/* secure the register updates */
	spinlock_t				lock;
};

/*-------------------------------------------------------------------------*/

static const char usbtll_driver_name[] = USBTLL_DRIVER_NAME;
static struct device *tll_dev;

/*-------------------------------------------------------------------------*/

static inline void usbtll_write(void __iomem *base, u32 reg, u32 val)
{
	__raw_writel(val, base + reg);
}

static inline u32 usbtll_read(void __iomem *base, u32 reg)
{
	return __raw_readl(base + reg);
}

static inline void usbtll_writeb(void __iomem *base, u8 reg, u8 val)
{
	__raw_writeb(val, base + reg);
}

static inline u8 usbtll_readb(void __iomem *base, u8 reg)
{
	return __raw_readb(base + reg);
}

/*-------------------------------------------------------------------------*/

static bool is_ohci_port(enum usbhs_omap_port_mode pmode)
{
	switch (pmode) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		return true;

	default:
		return false;
	}
}

/*
 * convert the port-mode enum to a value we can use in the FSLSMODE
 * field of USBTLL_CHANNEL_CONF
 */
static unsigned ohci_omap3_fslsmode(enum usbhs_omap_port_mode mode)
{
	switch (mode) {
	case OMAP_USBHS_PORT_MODE_UNUSED:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
		return OMAP_TLL_FSLSMODE_6PIN_PHY_DAT_SE0;

	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
		return OMAP_TLL_FSLSMODE_6PIN_PHY_DP_DM;

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
		return OMAP_TLL_FSLSMODE_3PIN_PHY;

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
		return OMAP_TLL_FSLSMODE_4PIN_PHY;

	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
		return OMAP_TLL_FSLSMODE_6PIN_TLL_DAT_SE0;

	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		return OMAP_TLL_FSLSMODE_6PIN_TLL_DP_DM;

	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		return OMAP_TLL_FSLSMODE_3PIN_TLL;

	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		return OMAP_TLL_FSLSMODE_4PIN_TLL;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
		return OMAP_TLL_FSLSMODE_2PIN_TLL_DAT_SE0;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		return OMAP_TLL_FSLSMODE_2PIN_DAT_DP_DM;
	default:
		pr_warn("Invalid port mode, using default\n");
		return OMAP_TLL_FSLSMODE_6PIN_PHY_DAT_SE0;
	}
}

/**
 * usbtll_omap_probe - initialize TI-based HCDs
 *
 * Allocates basic resources for this USB host controller.
 */
static int usbtll_omap_probe(struct platform_device *pdev)
{
	struct device				*dev =  &pdev->dev;
	struct usbtll_omap_platform_data	*pdata = dev->platform_data;
	void __iomem				*base;
	struct resource				*res;
	struct usbtll_omap			*tll;
	unsigned				reg;
	int					ret = 0;
	int					i, ver;
	bool needs_tll;

	dev_dbg(dev, "starting TI HSUSB TLL Controller\n");

	tll = kzalloc(sizeof(struct usbtll_omap), GFP_KERNEL);
	if (!tll) {
		dev_err(dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	if (!pdata) {
		dev_err(dev, "%s : Platform data mising\n", __func__);
		return -ENODEV;
	}

	spin_lock_init(&tll->lock);

	tll->pdata = pdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "usb tll get resource failed\n");
		ret = -ENODEV;
		goto err_mem;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(dev, "TLL ioremap failed\n");
		ret = -ENOMEM;
		goto err_mem;
	}

	tll->base = base;
	platform_set_drvdata(pdev, tll);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	ver =  usbtll_read(base, OMAP_USBTLL_REVISION);
	switch (ver) {
	case OMAP_USBTLL_REV1:
	case OMAP_USBTLL_REV4:
		tll->nch = DEFAULT_TLL_CHANNEL_COUNT;
		break;
	case OMAP_USBTLL_REV2:
	case OMAP_USBTLL_REV3:
		tll->nch = REV2_TLL_CHANNEL_COUNT;
		break;
	default:
		tll->nch = DEFAULT_TLL_CHANNEL_COUNT;
		dev_info(dev,
		 "USB TLL Rev : 0x%x not recognized, assuming %d channels\n",
			ver, tll->nch);
		break;
	}

	for (i = 0; i < tll->nch; i++) {
		char clk_name[] = "usb_tll_hs_usb_chx_clk";
		struct clk *fck;

		sprintf(clk_name, "usb_tll_hs_usb_ch%d_clk", i);
		fck = clk_get(dev, clk_name);

		if (IS_ERR(fck)) {
			dev_err(dev, "can't get clock : %s\n", clk_name);
			ret = PTR_ERR(fck);
			goto err_clk;
		}
		tll->ch_clk[i] = fck;
	}

	needs_tll = false;
	for (i = 0; i < tll->nch; i++)
		needs_tll |= mode_needs_tll(pdata->port_mode[i]);

	if (needs_tll) {

		/* Program Common TLL register */
		reg = usbtll_read(base, OMAP_TLL_SHARED_CONF);
		reg |= (OMAP_TLL_SHARED_CONF_FCLK_IS_ON
			| OMAP_TLL_SHARED_CONF_USB_DIVRATION);
		reg &= ~OMAP_TLL_SHARED_CONF_USB_90D_DDR_EN;
		reg &= ~OMAP_TLL_SHARED_CONF_USB_180D_SDR_EN;

		usbtll_write(base, OMAP_TLL_SHARED_CONF, reg);

		/* Enable channels now */
		for (i = 0; i < tll->nch; i++) {
			reg = usbtll_read(base,	OMAP_TLL_CHANNEL_CONF(i));

			if (is_ohci_port(pdata->port_mode[i])) {
				reg |= ohci_omap3_fslsmode(pdata->port_mode[i])
				<< OMAP_TLL_CHANNEL_CONF_FSLSMODE_SHIFT;
				reg |= OMAP_TLL_CHANNEL_CONF_CHANMODE_FSLS;
			} else if (pdata->port_mode[i] ==
					OMAP_EHCI_PORT_MODE_TLL) {
				/*
				 * Disable AutoIdle, BitStuffing
				 * and use SDR Mode
				 */
				reg &= ~(OMAP_TLL_CHANNEL_CONF_UTMIAUTOIDLE
					| OMAP_TLL_CHANNEL_CONF_ULPINOBITSTUFF
					| OMAP_TLL_CHANNEL_CONF_ULPIDDRMODE);
			} else if (pdata->port_mode[i] ==
					OMAP_EHCI_PORT_MODE_HSIC) {
				/*
				 * HSIC Mode requires UTMI port configurations
				 */
				reg |= OMAP_TLL_CHANNEL_CONF_DRVVBUS
				 | OMAP_TLL_CHANNEL_CONF_CHRGVBUS
				 | OMAP_TLL_CHANNEL_CONF_MODE_TRANSPARENT_UTMI
				 | OMAP_TLL_CHANNEL_CONF_ULPINOBITSTUFF;
			} else {
				continue;
			}
			reg |= OMAP_TLL_CHANNEL_CONF_CHANEN;
			usbtll_write(base, OMAP_TLL_CHANNEL_CONF(i), reg);

			usbtll_writeb(base,
				      OMAP_TLL_ULPI_SCRATCH_REGISTER(i),
				      0xbe);
		}
	}

	/* only after this can omap_tll_enable/disable work */
	tll_dev = dev;

err_clk:
	for (--i; i >= 0 ; i--)
		clk_put(tll->ch_clk[i]);

	pm_runtime_put_sync(dev);
	if (ret == 0) {
		pr_info("OMAP USB TLL : revision 0x%x, channels %d\n",
				ver, tll->nch);
		return 0;
	}

	iounmap(base);
	pm_runtime_disable(dev);

err_mem:
	kfree(tll);
	return ret;
}

/**
 * usbtll_omap_remove - shutdown processing for UHH & TLL HCDs
 * @pdev: USB Host Controller being removed
 *
 * Reverses the effect of usbtll_omap_probe().
 */
static int usbtll_omap_remove(struct platform_device *pdev)
{
	struct usbtll_omap *tll = platform_get_drvdata(pdev);
	int i;

	tll_dev = NULL;
	iounmap(tll->base);
	for (i = 0; i < tll->nch; i++)
		clk_put(tll->ch_clk[i]);

	pm_runtime_disable(&pdev->dev);
	kfree(tll);
	return 0;
}

static struct platform_driver usbtll_omap_driver = {
	.driver = {
		.name		= (char *)usbtll_driver_name,
		.owner		= THIS_MODULE,
	},
	.probe		= usbtll_omap_probe,
	.remove		= usbtll_omap_remove,
};

int omap_tll_enable(void)
{
	struct usbtll_omap			*tll;
	unsigned long				flags;
	int i;

	if (!tll_dev) {
		pr_err("%s: OMAP USB TLL not initialized\n", __func__);
		return  -ENODEV;
	}

	tll = dev_get_drvdata(tll_dev);
	spin_lock_irqsave(&tll->lock, flags);

	for (i = 0; i < tll->nch; i++) {
		if (mode_needs_tll(tll->pdata->port_mode[i])) {
			int r;
			r = clk_enable(tll->ch_clk[i]);
			if (r) {
				dev_err(tll_dev,
				  "%s : Error enabling ch %d clock: %d\n",
				  __func__, i, r);
			}
		}
	}

	i = pm_runtime_get_sync(tll_dev);
	spin_unlock_irqrestore(&tll->lock, flags);

	return i;
}
EXPORT_SYMBOL_GPL(omap_tll_enable);

int omap_tll_disable(void)
{
	struct usbtll_omap			*tll;
	unsigned long				flags;
	int i;

	if (!tll_dev) {
		pr_err("%s: OMAP USB TLL not initialized\n", __func__);
		return  -ENODEV;
	}

	tll = dev_get_drvdata(tll_dev);
	spin_lock_irqsave(&tll->lock, flags);

	for (i = 0; i < tll->nch; i++) {
		if (mode_needs_tll(tll->pdata->port_mode[i]))
			clk_disable(tll->ch_clk[i]);
	}

	i = pm_runtime_put_sync(tll_dev);
	spin_unlock_irqrestore(&tll->lock, flags);

	return i;
}
EXPORT_SYMBOL_GPL(omap_tll_disable);

MODULE_AUTHOR("Keshava Munegowda <keshava_mgowda@ti.com>");
MODULE_ALIAS("platform:" USBHS_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("usb tll driver for TI OMAP EHCI and OHCI controllers");

static int __init omap_usbtll_drvinit(void)
{
	return platform_driver_register(&usbtll_omap_driver);
}

/*
 * init before usbhs core driver;
 * The usbtll driver should be initialized before
 * the usbhs core driver probe function is called.
 */
fs_initcall(omap_usbtll_drvinit);

static void __exit omap_usbtll_drvexit(void)
{
	platform_driver_unregister(&usbtll_omap_driver);
}
module_exit(omap_usbtll_drvexit);
