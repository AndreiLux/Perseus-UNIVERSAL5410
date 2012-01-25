/*
 * OMAP4 system control module driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: J Keerthy <j-keerthy@ti.com>
 * Author: Moiz Sonasath <m-sonasath@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <plat/omap_device.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <plat/scm.h>
#include <linux/mfd/omap4_scm.h>

u32 omap4plus_scm_readl(struct scm *scm_ptr, u32 reg)
{
	return __raw_readl(scm_ptr->base + reg);
}
EXPORT_SYMBOL_GPL(omap4plus_scm_readl);

void omap4plus_scm_writel(struct scm *scm_ptr, u32 val, u32 reg)
{
	__raw_writel(val, scm_ptr->base + reg);
}
EXPORT_SYMBOL_GPL(omap4plus_scm_writel);


static int __devinit omap4plus_scm_probe(struct platform_device *pdev)
{
	struct omap4plus_scm_pdata *pdata = pdev->dev.platform_data;
	struct scm *scm_ptr;
	struct resource *mem;
	int ret = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "platform data missing\n");
		return -EINVAL;
	}

		scm_ptr = kzalloc(sizeof(*scm_ptr), GFP_KERNEL);
	if (!scm_ptr) {
		dev_err(&pdev->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	scm_ptr->cnt = pdata->cnt;

	mutex_init(&scm_ptr->scm_mutex);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource\n");
			ret = -ENOMEM;
		goto get_res_err;
	}

	scm_ptr->irq = platform_get_irq_byname(pdev, "thermal_alert");
	if (scm_ptr->irq < 0) {
		dev_err(&pdev->dev, "get_irq_byname failed\n");
		ret = scm_ptr->irq;
		goto get_res_err;
	}

	scm_ptr->base = ioremap(mem->start, resource_size(mem));
	if (!scm_ptr->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto get_res_err;
	}

	scm_ptr->rev = pdata->rev;
	scm_ptr->accurate = pdata->accurate;

	scm_ptr->dev = &pdev->dev;

	dev_set_drvdata(&pdev->dev, scm_ptr);

	return ret;

get_res_err:
	mutex_destroy(&scm_ptr->scm_mutex);
	kfree(scm_ptr);

	return ret;
}

static int __devexit omap4plus_scm_remove(struct platform_device *pdev)
{
	struct scm *scm_ptr = platform_get_drvdata(pdev);

	free_irq(scm_ptr->irq, scm_ptr);
	clk_put(scm_ptr->fclock);
	clk_put(scm_ptr->div_clk);
	iounmap(scm_ptr->base);
	dev_set_drvdata(&pdev->dev, NULL);
	mutex_destroy(&scm_ptr->scm_mutex);
	kfree(scm_ptr);

	return 0;
}

static struct platform_driver omap4plus_scm_driver = {
	.probe = omap4plus_scm_probe,
	.remove = omap4plus_scm_remove,
	.driver = {
			.name = "omap4plus_scm",
		   },
};

static int __init omap4plus_scm_init(void)
{
	return platform_driver_register(&omap4plus_scm_driver);
}

module_init(omap4plus_scm_init);

static void __exit omap4plus_scm_exit(void)
{
	platform_driver_unregister(&omap4plus_scm_driver);
}

module_exit(omap4plus_scm_exit);

MODULE_DESCRIPTION("OMAP4 plus system control module Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("J Keerthy <j-keerthy@ti.com>");
