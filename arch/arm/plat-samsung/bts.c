/* linux/arch/arm/plat-samsung/bts.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/list.h>
#if defined(CONFIG_PM_RUNTIME)
#include <linux/pm_runtime.h>
#endif
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/bts.h>
#include <mach/map.h>

/* BTS (Bus traffic shaper) register */
#define BTS_CONTROL 0x0
#define BTS_SHAPING_ON_OFF_REG0 0x4
#define BTS_MASTER_PRIORITY 0x8
#define BTS_SHAPING_ON_OFF_REG1 0x44
#define BTS_DEBLOCKING_SOURCE_SELECTION 0x50

/* BTS priority values */
#define BTS_PRIOR_HARDTIME 15
#define BTS_PRIOR_BESTEFFORT 8

/* Fields of BTS_CONTROL register */
#define BTS_ON_OFF (1<<0)
#define BLOCKING_ON_OFF (1<<2)
#define DEBLOCKING_ON_OFF (1<<7)

/* Fields of DEBLOCKING_SOURCE_SELECTION register */
#define SEL_GRP0 (1<<0)
#define SEL_LEFT0 (1<<4)
#define SEL_RIGHT0 (1<<5)
#define SEL_GRP1 (1<<8)
#define SEL_LEFT1 (1<<12)
#define SEL_RIGHT1 (1<<13)
#define SEL_GRP2 (1<<16)
#define SEL_LEFT2 (1<<20)
#define SEL_RIGHT2 (1<<21)

/* Shaping Value for Low priority */
#define LOW_SHAPING_VAL0 0x0
#define LOW_SHAPING_VAL1 0x3ff
#define MASTER_PRIOR_NUMBER (1<<16)

#define BTS_OFF 0
#define BTS_ON 1

static LIST_HEAD(fbm_list);
static LIST_HEAD(bts_list);

/* Structure for a physical BTS device  */
struct exynos_bts_local_data {
	void __iomem	*base;
	enum bts_priority def_priority;
	bool changable_prior;
};

/* Structure for a BTS driver.
 * A BTS driver handles several physical BTS devices
 * that are grouped together by clock and power control.
 * bts_local_data member contains the physical device list.
 */
struct exynos_bts_data {
	struct list_head node;
	struct device *dev;
	struct clk *clk;
	struct exynos_bts_local_data *bts_local_data;
	char *pd_name;
	enum bts_prior_change_action change_act;
	u32 listnum;
};

/* Structure for FBM devices */
struct exynos_fbm_data {
	struct exynos_fbm_resource fbm;
	struct list_head node;
};

/* find FBM group based on requested priority */
static enum bts_fbm_group find_fbm_group(enum bts_priority prior)
{
	struct exynos_fbm_data *fbm_data;
	enum bts_fbm_group fbm_group = 0;

	list_for_each_entry(fbm_data, &fbm_list, node) {
		if (prior == BTS_BE) {
			if (fbm_data->fbm.priority == BTS_HARDTIME)
				fbm_group |= fbm_data->fbm.fbm_group;
		} else if (prior == BTS_HARDTIME) {
			if ((fbm_data->fbm.priority == BTS_BE) ||
				(fbm_data->fbm.priority == BTS_HARDTIME))
				fbm_group |= fbm_data->fbm.fbm_group;
		}
	}

	return fbm_group;
}

/* basic control of a BTS device */
static void bts_set_control(void __iomem *base, enum bts_priority prior)
{
	u32 val = BTS_ON_OFF;

	if (prior == BTS_BE)
		val |= BLOCKING_ON_OFF|DEBLOCKING_ON_OFF;
	writel(val, base + BTS_CONTROL);
}

/* on/off a BTS device */
static void bts_onoff(void __iomem *base, bool on)
{
	u32 val = readl(base + BTS_CONTROL);
	if (on)
		val |= BTS_ON_OFF;
	else
		val &= ~BTS_ON_OFF;

	writel(val, base + BTS_CONTROL);
}

/* set priority to BTS device */
static void bts_set_master_priority(void __iomem *base, enum bts_priority prior)
{
	u32 val;
	u32 priority = BTS_PRIOR_BESTEFFORT;

	 if (prior == BTS_HARDTIME)
		priority = BTS_PRIOR_HARDTIME;

	val = MASTER_PRIOR_NUMBER | (priority<<8) | (priority<<4) | (priority);
	writel(val, base + BTS_MASTER_PRIORITY);
}

/* set the shaping value for best effort IPs to BTS device */
static void bts_set_besteffort_shaping(void __iomem *base)
{
	writel(LOW_SHAPING_VAL0, base + BTS_SHAPING_ON_OFF_REG0);
	writel(LOW_SHAPING_VAL1, base + BTS_SHAPING_ON_OFF_REG1);
}

/* set the deblocking source according to deblocking group to BTS device */
static void bts_set_deblocking(void __iomem *base,
	enum bts_fbm_group deblocking)
{
	u32 val = 0;

	if (deblocking & BTS_FBM_G0_L)
		val |= SEL_GRP0 | SEL_LEFT0;
	if (deblocking & BTS_FBM_G0_R)
		val |= SEL_GRP0 | SEL_RIGHT0;
	if (deblocking & BTS_FBM_G1_L)
		val |= SEL_GRP1 | SEL_LEFT1;
	if (deblocking & BTS_FBM_G1_R)
		val |= SEL_GRP1 | SEL_RIGHT1;
	if (deblocking & BTS_FBM_G2_L)
		val |= SEL_GRP2 | SEL_LEFT2;
	if (deblocking & BTS_FBM_G2_R)
		val |= SEL_GRP2 | SEL_RIGHT2;
	writel(val, base + BTS_DEBLOCKING_SOURCE_SELECTION);
}

/* initialize a BTS device in default setting */
static void bts_init_config(void __iomem *base, enum bts_priority prior)
{
	switch (prior) {
	case BTS_BE:
		bts_set_besteffort_shaping(base);
		bts_set_deblocking(base, find_fbm_group(prior));
		bts_set_master_priority(base, prior);
		bts_set_control(base, prior);
		break;
	case BTS_HARDTIME:
		bts_set_master_priority(base, prior);
		bts_set_control(base, prior);
		break;
	default:
		break;
	}
}

/* change FBM setting  */
static void bts_change_fbm_priority(bool on)
{
	struct exynos_bts_data *bts_data;
	struct exynos_bts_local_data *bts_local_data;
	enum bts_priority prior = (on) ? BTS_HARDTIME : BTS_BE;
	int i;

	list_for_each_entry(bts_data, &bts_list, node) {
		bts_local_data = bts_data->bts_local_data;
		for (i = 0; i < bts_data->listnum; i++) {
			if (bts_local_data->changable_prior) {
#if defined(CONFIG_PM_RUNTIME)
				pm_runtime_get_sync(bts_data->dev->parent);
#endif
				if (bts_data->clk)
					clk_enable(bts_data->clk);
				bts_onoff(bts_local_data->base, BTS_OFF);
				bts_set_deblocking(bts_local_data->base,
						find_fbm_group(prior));
				bts_onoff(bts_local_data->base, BTS_ON);
				if (bts_data->clk)
					clk_disable(bts_data->clk);
#if defined(CONFIG_PM_RUNTIME)
				pm_runtime_put_sync(bts_data->dev->parent);
#endif
			}
			bts_local_data++;
		}
	}
}

/* turn physical BTS devices on/off */
static void bts_devs_onoff(struct exynos_bts_data *bts_data, bool on)
{
	struct exynos_bts_local_data *bts_local_data;
	int i;

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_get_sync(bts_data->dev->parent);
#endif
	if (bts_data->clk)
		clk_enable(bts_data->clk);

	bts_local_data = bts_data->bts_local_data;
	for (i = 0; i < bts_data->listnum; i++) {
		bts_onoff(bts_local_data->base, on);
		bts_local_data++;
	}

	if (bts_data->clk)
		clk_disable(bts_data->clk);
#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_put_sync(bts_data->dev->parent);
#endif
}

/* init physical bts devices */
static void bts_devs_init(struct exynos_bts_data *bts_data)
{
	struct exynos_bts_local_data *bts_local_data;
	int i;

	if (bts_data->clk)
		clk_enable(bts_data->clk);

	bts_local_data = bts_data->bts_local_data;
	for (i = 0; i < bts_data->listnum; i++) {
		bts_init_config(bts_local_data->base,
				bts_local_data->def_priority);
		bts_local_data++;
	}

	if (bts_data->clk)
		clk_disable(bts_data->clk);
}

void exynos_bts_set_priority(struct device *dev, bool on)
{
	struct exynos_bts_data *bts_data;

	list_for_each_entry(bts_data, &bts_list, node) {
		if (bts_data->dev->parent == dev) {
			switch (bts_data->change_act) {
			case BTS_ACT_OFF:
				bts_devs_onoff(bts_data, !on);
				break;
			case BTS_ACT_CHANGE_FBM_PRIOR:
				bts_change_fbm_priority(on);
				break;
			default:
				dev_err(bts_data->dev, "unregistered case to change priority!\n");
				break;
			}
		}
	}
}

void exynos_bts_enable(char *pd_name)
{
	struct exynos_bts_data *bts_data;

	list_for_each_entry(bts_data, &bts_list, node) {
		if ((!pd_name && !bts_data->pd_name) ||
			(pd_name && !strcmp(bts_data->pd_name, pd_name)))
			bts_devs_init(bts_data);
	}
}

static int bts_probe(struct platform_device *pdev)
{
	struct exynos_bts_pdata *bts_pdata;
	struct exynos_fbm_resource *fbm_res;
	struct exynos_bts_data *bts_data;
	struct exynos_bts_local_data *bts_local_data, *bts_local_data_h;
	struct exynos_fbm_data *fbm_data = NULL, *next;
	struct resource *res = NULL;
	struct clk *clk = NULL;
	int i, ret = 0;

	bts_pdata = pdev->dev.platform_data;
	if (!bts_pdata) {
		dev_err(&pdev->dev, "platform data is missed!\n");
		return -ENODEV;
	}

	fbm_res = bts_pdata->fbm->res;

	if (list_empty(&fbm_list)) {
		for (i = 0; i < bts_pdata->fbm->res_num; i++) {
			fbm_data = kzalloc(sizeof(struct exynos_fbm_data),
						GFP_KERNEL);
			if (!fbm_data) {
				ret = -ENOMEM;
				goto probe_err1;
			}
			list_add_tail(&fbm_data->node, &fbm_list);
			fbm_data->fbm.fbm_group = fbm_res->fbm_group;
			fbm_data->fbm.priority = fbm_res->priority;
			fbm_res++;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "can't get resource!\n");
		ret = -ENODEV;
		goto probe_err1;
	}

	if (bts_pdata->clk_name) {
		clk = clk_get(pdev->dev.parent, bts_pdata->clk_name);
		if (IS_ERR(clk)) {
			ret = -EINVAL;
			goto probe_err1;
		}
		clk_enable(clk);
	}

	bts_data = kzalloc(sizeof(struct exynos_bts_data), GFP_KERNEL);
	if (!bts_data) {
		ret = -ENOMEM;
		goto probe_err2;
	}
	bts_data->listnum = bts_pdata->res_num;
	bts_data->change_act = bts_pdata->change_act;
	bts_local_data_h = bts_local_data =
		kzalloc(sizeof(struct exynos_bts_local_data)*bts_data->listnum,
				GFP_KERNEL);
	if (!bts_local_data_h) {
		ret = -ENOMEM;
		goto probe_err3;
	}

	for (i = 0; i < bts_data->listnum; i++) {
		bts_local_data->base = ioremap(res->start, resource_size(res));
		if (!bts_local_data->base) {
			ret = -ENXIO;
			goto probe_err4;
		}
		bts_local_data->def_priority = bts_pdata->def_priority;
		bts_local_data->changable_prior = bts_pdata->changable_prior;
		bts_init_config(bts_local_data->base,
				bts_local_data->def_priority);
		bts_local_data++;
		res++;
	}

	bts_data->bts_local_data = bts_local_data_h;
	bts_data->pd_name = bts_pdata->pd_name;
	bts_data->clk = clk;
	bts_data->dev = &pdev->dev;

	list_add_tail(&bts_data->node, &bts_list);
	pdev->dev.platform_data = bts_data;

	if (bts_pdata->clk_name)
		clk_disable(clk);

	return 0;

probe_err4:
	bts_local_data = bts_local_data_h;
	for (i = 0; i < bts_data->listnum; i++) {
		if (bts_local_data->base)
			iounmap(bts_local_data->base);
		bts_local_data++;
	}
	kfree(bts_local_data_h);

probe_err3:
	kfree(bts_data);

probe_err2:
	if (bts_pdata->clk_name) {
		clk_disable(clk);
		clk_put(bts_data->clk);
	}

probe_err1:
	list_for_each_entry_safe(fbm_data, next, &fbm_list, node) {
		if (fbm_data) {
			list_del(&fbm_data->node);
			kfree(fbm_data);
		}
	}

	return ret;
}

static int bts_remove(struct platform_device *pdev)
{
	struct exynos_fbm_data *fbm_data, *next;
	struct exynos_bts_data *bts_data = pdev->dev.platform_data;
	struct exynos_bts_local_data *bts_local_data;
	int i;

	bts_local_data = bts_data->bts_local_data;
	for (i = 0; i < bts_data->listnum; i++) {
		bts_local_data++;
		iounmap(bts_local_data->base);
	}
	kfree(bts_data->bts_local_data);
	list_del(&bts_data->node);

	if (bts_data->clk)
		clk_put(bts_data->clk);

	kfree(bts_data);

	if (!list_empty(&bts_list))
		list_for_each_entry_safe(fbm_data, next, &fbm_list, node) {
			list_del(&fbm_data->node);
			kfree(fbm_data);
		}

	return 0;
}

static struct platform_driver bts_driver = {
	.driver	= {
		.name = "exynos-bts"
	},
	.probe	= bts_probe,
	.remove	= bts_remove,
};

static int __init bts_init(void)
{
	return platform_driver_register(&bts_driver);
}
arch_initcall(bts_init);
