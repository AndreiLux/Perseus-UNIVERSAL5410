/**
 * sata.c - The ahci sata device init functions
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <plat/omap_device.h>
#include <linux/dma-mapping.h>
#include <linux/ahci_platform.h>
#include <linux/clk.h>
#include <plat/sata.h>

#if defined(CONFIG_SATA_AHCI_PLATFORM) || \
	defined(CONFIG_SATA_AHCI_PLATFORM_MODULE)

#define OMAP_SATA_HWMODNAME	"sata"
#define OMAP_OCP2SCP3_HWMODNAME	"ocp2scp3"
#define AHCI_PLAT_DEVNAME	"ahci"

#define OMAP_SATA_PLL_CONTROL			0x00
#define OMAP_SATA_PLL_STATUS			0x04
#define OMAP_SATA_PLL_STATUS_LOCK_SHIFT		1
#define OMAP_SATA_PLL_STATUS_LOCK		1

#define OMAP_SATA_PLL_GO			0x08

#define OMAP_SATA_PLL_CFG1			0x0c
#define OMAP_SATA_PLL_CFG1_M_MASK		0xfff
#define OMAP_SATA_PLL_CFG1_M_SHIFT		9
#define OMAP_SATA_PLL_CFG1_N_MASK		0xf
#define OMAP_SATA_PLL_CFG1_N_SHIFT		1

#define OMAP_SATA_PLL_CFG2			0x10
#define OMAP_SATA_PLL_CFG2_SELFREQDCO_MASK	7
#define OMAP_SATA_PLL_CFG2_SELFREQDCO_SHIFT	1

#define OMAP_SATA_PLL_CFG3			0x14
#define OMAP_SATA_PLL_CFG3_SD_MASK		0xf
#define OMAP_SATA_PLL_CFG3_SD_SHIF		10

#define OMAP_SATA_PLL_CFG4			0x20
#define OMAP_SATA_PLL_CFG4_REGM_F_MASK		0x1ffff


/*
 * Enable the pll clk of 1.5 Ghz
 * since the sys_clk is 19.2 MHz, for 1.5 Ghz
 * following are the M and N values
 */
#define OMAP_SATA_PLL_CFG1_M			625
#define OMAP_SATA_PLL_CFG1_N			7

/*
 * for sata; the DCO frequencey range 1500 Mhz
 * hence the SELFREQDCO should be 4
 */
#define OMAP_SATA_PLL_CFG2_SELFREQDCO		4
#define OMAP_SATA_PLL_CFG3_SD			6
#define OMAP_SATA_PLL_CFG4_REGM_F		0

#define OMAP_OCP2SCP3_SYSCONFIG			0x10
#define OMAP_OCP2SCP3_SYSCONFIG_RESET_MASK	1
#define OMAP_OCP2SCP3_SYSCONFIG_RESET_SHIFT	1
#define OMAP_OCP2SCP3_SYSCONFIG_RESET		1

#define OMAP_OCP2SCP3_SYSSTATUS			0x14
#define OMAP_OCP2SCP3_SYSSTATUS_RESETDONE	1

#define OMAP_OCP2SCP3_TIMING			0x18
#define OMAP_OCP2SCP3_TIMING_SYNC2_MASK		7
#define OMAP_OCP2SCP3_TIMING_SYNC2		7


#define OMAP_SATA_PHY_PWR
/* - FIXME -
 * The sata phy power enable belongs to control module
 * for now it will be part of this driver; it should
 * seperated from the sata configuration.
 */
#define OMAP_CTRL_MODULE_CORE			0x4a002000
#define OMAP_CTRL_MODULE_CORE_SIZE		2048

#define OMAP_CTRL_SATA_PHY_POWER		0x374
#define SATA_PWRCTL_CLK_FREQ_SHIFT		22
#define SATA_PWRCTL_CLK_CMD_SHIFT		14

#define OMAP_CTRL_SATA_EXT_MODE			0x3ac

/* Enable the 19.2 Mhz frequency */
#define SATA_PWRCTL_CLK_FREQ			19

/* Enable Tx and Rx phys */
#define SATA_PWRCTL_CLK_CMD			3

static u64				sata_dmamask = DMA_BIT_MASK(32);
static struct ahci_platform_data	sata_pdata;
static struct clk			*sata_ref_clk;

static struct omap_device_pm_latency omap_sata_latency[] = {
	  {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	  },
};

static inline void omap_sata_writel(void __iomem *base, u32 reg, u32 val)
{
	__raw_writel(val, base + reg);
}

static inline u32 omap_sata_readl(void __iomem *base, u32 reg)
{
	return __raw_readl(base + reg);
}

#ifdef OMAP_SATA_PHY_PWR
static void __init sata_phy_pwr_on(void)
{
	void __iomem	*base;

	base = ioremap(OMAP_CTRL_MODULE_CORE, OMAP_CTRL_MODULE_CORE_SIZE);
	if (base) {
		omap_sata_writel(base, OMAP_CTRL_SATA_PHY_POWER,
			((SATA_PWRCTL_CLK_FREQ << SATA_PWRCTL_CLK_FREQ_SHIFT)|
			 (SATA_PWRCTL_CLK_CMD << SATA_PWRCTL_CLK_CMD_SHIFT)));
		omap_sata_writel(base, OMAP_CTRL_SATA_EXT_MODE, 1);
		iounmap(base);
	}
}
#endif

/*
 * - FIXME -
 * Following PLL configuration will be removed in future,
 * to a seperate platform driver
 */
static int __init sata_phy_init(struct device *dev)
{
	void __iomem		*pll;
	void __iomem		*ocp2scp3;
	struct resource		*res;
	struct platform_device	*pdev;
	u32			reg;
	int			ret;
	unsigned long		timeout;

	pdev =	container_of(dev, struct platform_device, dev);

	/*Enable the sata reference clock */
	sata_ref_clk = clk_get(dev, "ref_clk");
	if (IS_ERR(sata_ref_clk)) {
		ret = PTR_ERR(sata_ref_clk);
		dev_err(dev, "ref_clk failed:%d\n", ret);
		return ret;
	}
	clk_enable(sata_ref_clk);

#ifdef OMAP_SATA_PHY_PWR
	sata_phy_pwr_on();
#endif
	res =  platform_get_resource_byname(pdev, IORESOURCE_MEM, "ocp2scp3");
	if (!res) {
		dev_err(dev, "ocp2scp3 get resource failed\n");
		goto pll;
	}

	ocp2scp3 = ioremap(res->start, resource_size(res));
	if (!ocp2scp3) {
		dev_err(dev, "can't map ocp2scp3 0x%X\n", res->start);
		return -ENOMEM;
	}

	/*
	 * set the ocp2scp3 values;
	 * as per OMAP5 TRM, division ratio should not modified
	 * SYNC2 should use the value 0x6 or 7;
	 * SYNC1 is undefined; hence default value is used
	 */
	reg  = omap_sata_readl(ocp2scp3, OMAP_OCP2SCP3_TIMING);
	reg &= ~OMAP_OCP2SCP3_TIMING_SYNC2_MASK;
	reg |= OMAP_OCP2SCP3_TIMING_SYNC2;
	omap_sata_writel(ocp2scp3, OMAP_OCP2SCP3_TIMING, reg);

	/* do the soft reset of ocp2scp3 */
	reg  = omap_sata_readl(ocp2scp3, OMAP_OCP2SCP3_SYSCONFIG);
	reg &= ~(OMAP_OCP2SCP3_SYSCONFIG_RESET_MASK <<
		OMAP_OCP2SCP3_SYSCONFIG_RESET_SHIFT);
	reg |= (OMAP_OCP2SCP3_SYSCONFIG_RESET <<
		OMAP_OCP2SCP3_SYSCONFIG_RESET_SHIFT);
	omap_sata_writel(ocp2scp3, OMAP_OCP2SCP3_SYSCONFIG, reg);

	/* wait for the reset to complete */
	timeout = jiffies + msecs_to_jiffies(1000);
	while (!(omap_sata_readl(ocp2scp3, OMAP_OCP2SCP3_SYSSTATUS)
		& OMAP_OCP2SCP3_SYSSTATUS_RESETDONE)) {
		cpu_relax();

		if (time_after(jiffies, timeout)) {
			dev_err(dev, "ocp2scp3 reset timed out\n");
			break;
		}
	}
	iounmap(ocp2scp3);
pll:

	res =  platform_get_resource_byname(pdev, IORESOURCE_MEM, "pll");
	if (!res) {
		dev_err(dev, "pll get resource failed\n");
		goto end;
	}

	pll = ioremap(res->start, resource_size(res));
	if (!pll) {
		dev_err(dev, "can't map pll 0x%X\n", res->start);
		return -ENOMEM;
	}

	/*
	 * set the configuration 1; The phy clocks
	 * set M and N values for Phy clocks
	 */
	reg  = omap_sata_readl(pll, OMAP_SATA_PLL_CFG1);
	reg &= ~((OMAP_SATA_PLL_CFG1_M_MASK << OMAP_SATA_PLL_CFG1_M_SHIFT) |
		(OMAP_SATA_PLL_CFG1_N_MASK << OMAP_SATA_PLL_CFG1_N_SHIFT));
	reg |= ((OMAP_SATA_PLL_CFG1_M << OMAP_SATA_PLL_CFG1_M_SHIFT) |
		(OMAP_SATA_PLL_CFG1_N << OMAP_SATA_PLL_CFG1_N_SHIFT));
	omap_sata_writel(pll, OMAP_SATA_PLL_CFG1, reg);

	/* set the dco frequence range */
	reg  = omap_sata_readl(pll, OMAP_SATA_PLL_CFG2);
	reg &= ~(OMAP_SATA_PLL_CFG2_SELFREQDCO_MASK <<
		OMAP_SATA_PLL_CFG2_SELFREQDCO_SHIFT);
	reg |= (OMAP_SATA_PLL_CFG2_SELFREQDCO <<
		OMAP_SATA_PLL_CFG2_SELFREQDCO_SHIFT);
	omap_sata_writel(pll, OMAP_SATA_PLL_CFG2, reg);

	reg  = omap_sata_readl(pll, OMAP_SATA_PLL_CFG3);
	reg &= ~(OMAP_SATA_PLL_CFG3_SD_MASK << OMAP_SATA_PLL_CFG3_SD_SHIF);
	reg |= (OMAP_SATA_PLL_CFG3_SD << OMAP_SATA_PLL_CFG3_SD_SHIF);
	omap_sata_writel(pll, OMAP_SATA_PLL_CFG3, reg);


	reg  = omap_sata_readl(pll, OMAP_SATA_PLL_CFG4);
	reg &= ~OMAP_SATA_PLL_CFG4_REGM_F_MASK;
	reg |= OMAP_SATA_PLL_CFG4_REGM_F;
	omap_sata_writel(pll, OMAP_SATA_PLL_CFG4, reg);

	omap_sata_writel(pll, OMAP_SATA_PLL_GO, 1);

	/* Poll for the PLL lock */
	timeout = jiffies + msecs_to_jiffies(1000);
	while (!(omap_sata_readl(pll, OMAP_SATA_PLL_STATUS) &
		(OMAP_SATA_PLL_STATUS_LOCK <<
		OMAP_SATA_PLL_STATUS_LOCK_SHIFT))) {
		cpu_relax();

		if (time_after(jiffies, timeout)) {
			dev_err(dev, "sata phy pll lock timed out\n");
			break;
		}
	}
	iounmap(pll);
end:
	return 0;
}

static void sata_phy_exit(void)
{
	clk_disable(sata_ref_clk);
	clk_put(sata_ref_clk);
}

static int __init omap_ahci_plat_init(struct device *dev, void __iomem *base)
{
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	sata_phy_init(dev);
	return 0;
}

static void omap_ahci_plat_exit(struct device *dev)
{
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	sata_phy_exit();
}

void __init omap_sata_init(void)
{
	struct omap_hwmod	*hwmod[2];
	struct omap_device	*od;
	struct platform_device	*pdev;
	struct device		*dev;
	int			oh_cnt = 1;

	/* For now sata init works only for omap5 */
	if (!cpu_is_omap54xx())
		return;

	sata_pdata.init		= omap_ahci_plat_init;
	sata_pdata.exit		= omap_ahci_plat_exit;

	hwmod[0] = omap_hwmod_lookup(OMAP_SATA_HWMODNAME);
	if (!hwmod[0]) {
		pr_err("Could not look up %s\n", OMAP_SATA_HWMODNAME);
		return;
	}

	hwmod[1] = omap_hwmod_lookup(OMAP_OCP2SCP3_HWMODNAME);
	if (hwmod[1])
		oh_cnt++;
	else
		pr_err("Could not look up %s\n", OMAP_OCP2SCP3_HWMODNAME);

	od = omap_device_build_ss(AHCI_PLAT_DEVNAME, -1, hwmod, oh_cnt,
				(void *) &sata_pdata, sizeof(sata_pdata),
				omap_sata_latency,
				ARRAY_SIZE(omap_sata_latency), false);
	if (IS_ERR(od)) {
		pr_err("Could not build hwmod device %s\n",
					OMAP_SATA_HWMODNAME);
		return;
	}
	pdev = &od->pdev;
	dev = &pdev->dev;
	get_device(dev);
	dev->dma_mask = &sata_dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	put_device(dev);
}
#else

void __init omap_sata_init(void)
{
}

#endif
