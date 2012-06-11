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

#include <linux/module.h>
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

#ifdef CONFIG_THERMAL_FRAMEWORK
#include <linux/thermal_framework.h>
#endif

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

#ifdef CONFIG_OMAP4460PLUS_TEMP_SENSOR
static void omap4plus_scm_client_register(struct temp_sensor_hwmon *tsh_ptr,
				   int i, const char *name)
{
	int ret;

	tsh_ptr->pdev = platform_device_alloc(name, i);

	if (tsh_ptr->pdev == NULL) {
		dev_err(tsh_ptr->scm_ptr->dev, "Failed to allocate %s\n", name);
		return;
	}

	tsh_ptr->pdev->dev.parent = tsh_ptr->scm_ptr->dev;
	platform_set_drvdata(tsh_ptr->pdev, tsh_ptr);
	ret = platform_device_add(tsh_ptr->pdev);
	if (ret != 0) {
		dev_err(tsh_ptr->scm_ptr->dev, "Failed to register %s: %d\n",
			name, ret);
		platform_device_put(tsh_ptr->pdev);
		tsh_ptr->pdev = NULL;
	}
}

static irqreturn_t talert_irq_handler(int irq, void *data)
{
	struct scm *scm_ptr;
	int t_hot = 0, t_cold, temp, i;
	struct omap4460plus_temp_sensor_registers *tsr;

	scm_ptr = data;
	/* Read the status of t_hot */
	for (i = 0; i < scm_ptr->cnt; i++) {
		tsr = scm_ptr->registers[i];
		t_hot = omap4plus_scm_readl(scm_ptr, tsr->bgap_status)
		    & tsr->status_hot_mask;

		/* Read the status of t_cold */
		t_cold = omap4plus_scm_readl(scm_ptr, tsr->bgap_status)
		    & tsr->status_cold_mask;

		temp = omap4plus_scm_readl(scm_ptr, tsr->bgap_mask_ctrl);
		/*
		 * One TALERT interrupt: Two sources
		 * If the interrupt is due to t_hot then mask t_hot and
		 * and unmask t_cold else mask t_cold and unmask t_hot
		 */
		if (t_hot) {
			temp &= ~tsr->mask_hot_mask;
			temp |= tsr->mask_cold_mask;
		} else if (t_cold) {
			temp &= ~tsr->mask_cold_mask;
			temp |= tsr->mask_hot_mask;
		}

		mutex_lock(&scm_ptr->scm_mutex);
		omap4plus_scm_writel(scm_ptr, temp, tsr->bgap_mask_ctrl);
		mutex_unlock(&scm_ptr->scm_mutex);

		/* read temperature */
		temp = omap4plus_scm_readl(scm_ptr, tsr->temp_sensor_ctrl);
		temp &= tsr->bgap_dtemp_mask;

		if (t_hot || t_cold) {
#ifdef CONFIG_THERMAL_FRAMEWORK
			scm_ptr->therm_fw[i]->current_temp =
			    adc_to_temp_conversion(scm_ptr, i, temp);
			thermal_sensor_set_temp(scm_ptr->therm_fw[i]);
#endif
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t omap4460_tshut_irq_handler(int irq, void *data)
{
	kernel_restart(NULL);

	return IRQ_HANDLED;
}
#endif

static int __devinit omap4plus_scm_probe(struct platform_device *pdev)
{
	struct omap4plus_scm_pdata *pdata = pdev->dev.platform_data;
	struct scm *scm_ptr;
	struct resource *mem;
	int ret = 0, i;

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
		goto destroy_mutex;
	}

	scm_ptr->irq = platform_get_irq_byname(pdev, "thermal_alert");
	if (scm_ptr->irq < 0) {
		dev_err(&pdev->dev, "get_irq_byname failed\n");
		ret = scm_ptr->irq;
		goto destroy_mutex;
	}

	scm_ptr->base = ioremap(mem->start, resource_size(mem));
	if (!scm_ptr->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto destroy_mutex;
	}

	scm_ptr->rev = pdata->rev;
	scm_ptr->accurate = pdata->accurate;

	scm_ptr->dev = &pdev->dev;

	dev_set_drvdata(&pdev->dev, scm_ptr);

#ifdef CONFIG_OMAP4460PLUS_TEMP_SENSOR
	scm_ptr->tsh_ptr = kzalloc(sizeof(*scm_ptr->tsh_ptr) * scm_ptr->cnt,
					GFP_KERNEL);
	if (!scm_ptr->tsh_ptr) {
		dev_err(&pdev->dev, "Failed to allocate memory for tsh_ptr\n");
		ret = -ENOMEM;
		goto unmap;
	}

	if (scm_ptr->rev == 1 && pdata->accurate) {
		scm_ptr->fclock = clk_get(&pdev->dev, "fck");
		ret = IS_ERR_OR_NULL(scm_ptr->fclock);
		if (ret) {
			dev_err(&pdev->dev, "Couldn't get functional clock\n");
			goto free_tsh;
		}

		scm_ptr->div_clk = clk_get(&pdev->dev, "div_ck");
		ret = IS_ERR_OR_NULL(scm_ptr->div_clk);
		if (ret) {
			dev_err(&pdev->dev, "Couldn't get div_ck clock\n");
			goto put_fclock;
		}
	} else if (scm_ptr->rev == 2) {
		scm_ptr->fclock = clk_get(&pdev->dev, "ts_fck");
		ret = IS_ERR_OR_NULL(scm_ptr->fclock);
		if (ret) {
			dev_err(&pdev->dev, "Couldn't get functional clock\n");
			goto free_tsh;
		}
	}

	ret = request_threaded_irq(scm_ptr->irq, NULL,
					talert_irq_handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"TAlert", scm_ptr);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n for TAlert\n",
				scm_ptr->irq);
		goto put_clocks;
	}
/* Initialize temperature sensors */
	if ((scm_ptr->accurate) || (scm_ptr->rev == 2)) {
		ret = omap4460plus_temp_sensor_init(scm_ptr);
		if (ret) {
			dev_err(&pdev->dev, "Temperature sensor init failed\n");
			goto free_irq;
		}

		if (scm_ptr->rev == 1) {
			scm_ptr->gpio_tshut = 86;

			ret = omap4460_tshut_init(scm_ptr);
			if (ret)
				goto sensor_deinit;

			ret = request_threaded_irq(gpio_to_irq(scm_ptr->gpio_tshut), NULL,
						   omap4460_tshut_irq_handler,
						   IRQF_TRIGGER_RISING, "tshut",
						   NULL);
			if (ret) {
				dev_err(&pdev->dev, "Failed to request IRQ %d for tshut\n",
						gpio_to_irq(scm_ptr->gpio_tshut));
				goto tshut_deinit;
			}
		}

		for (i = 0; i < scm_ptr->cnt; i++) {
			scm_ptr->tsh_ptr[i].scm_ptr = scm_ptr;
			omap4plus_scm_client_register(&(scm_ptr->tsh_ptr[i]),
						      i, "temp_sensor_hwmon");
		}

	}
#endif
	return 0;

#ifdef CONFIG_OMAP4460PLUS_TEMP_SENSOR
tshut_deinit:
	omap4460_tshut_deinit(scm_ptr);

sensor_deinit:
	omap4460plus_temp_sensor_deinit(scm_ptr);

free_irq:
	free_irq(scm_ptr->irq, scm_ptr);

put_clocks:
	if (scm_ptr->rev == 1)
		clk_put(scm_ptr->div_clk);
put_fclock:
	clk_put(scm_ptr->fclock);

free_tsh:
	kfree(scm_ptr->tsh_ptr);

unmap:
	iounmap(scm_ptr->base);
#endif

destroy_mutex:
	mutex_destroy(&scm_ptr->scm_mutex);
	kfree(scm_ptr);
	return ret;
}

static int __devexit omap4plus_scm_remove(struct platform_device *pdev)
{
	struct scm *scm_ptr = platform_get_drvdata(pdev);

#ifdef CONFIG_OMAP4460PLUS_TEMP_SENSOR
	if ((scm_ptr->accurate) || (scm_ptr->rev == 2)) {
		if (scm_ptr->rev == 1) {
			free_irq(gpio_to_irq(scm_ptr->gpio_tshut), NULL);
			omap4460_tshut_deinit(scm_ptr);
		}
		omap4460plus_temp_sensor_deinit(scm_ptr);
	}

	free_irq(scm_ptr->irq, scm_ptr);
	clk_disable(scm_ptr->fclock);
	if (scm_ptr->rev == 1)
		clk_put(scm_ptr->div_clk);
	clk_put(scm_ptr->fclock);
	kfree(scm_ptr->tsh_ptr);
#endif
	iounmap(scm_ptr->base);
	dev_set_drvdata(&pdev->dev, NULL);
	mutex_destroy(&scm_ptr->scm_mutex);
	kfree(scm_ptr);

	return 0;
}

#if (defined(CONFIG_PM) && defined(CONFIG_OMAP4460PLUS_TEMP_SENSOR))
static void omap4plus_scm_save_ctxt(struct scm *scm_ptr)
{
	int i;

	if (!((scm_ptr->accurate) || (scm_ptr->rev == 2)))
		return;

	for (i = 0; i < scm_ptr->cnt; i++) {

		pr_err("omap4plus_scm_save_ctxt: %d / %d\n", i + 1, scm_ptr->cnt);

		if (!scm_ptr->registers[i])
			continue;

		if (!scm_ptr->regval[i])
			continue;

		scm_ptr->regval[i]->bg_mode_ctrl =
			omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[i]->bgap_mode_ctrl);
		scm_ptr->regval[i]->bg_ctrl =
			omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[i]->bgap_mask_ctrl);
		scm_ptr->regval[i]->bg_counter =
			omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[i]->bgap_counter);
		scm_ptr->regval[i]->bg_threshold =
			omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[i]->bgap_threshold);
		scm_ptr->regval[i]->tshut_threshold =
			omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[i]->tshut_threshold);

		pr_err("omap4plus_scm_save_ctxt: %d done\n", i);
	}
}

static void omap_temp_sensor_force_single_read(struct scm *scm_ptr, int id)
{
	int temp = 0, counter = 1000;

	/* Select single conversion mode */
	temp = omap4plus_scm_readl(scm_ptr,
			scm_ptr->registers[id]->bgap_mode_ctrl);
	temp &= ~(1 << __ffs(scm_ptr->registers[id]->mode_ctrl_mask));
	omap4plus_scm_writel(scm_ptr, temp,
				scm_ptr->registers[id]->bgap_mode_ctrl);

	/* Start of Conversion = 1 */
	temp = omap4plus_scm_readl(scm_ptr,
			scm_ptr->registers[id]->temp_sensor_ctrl);
	temp |= 1 << __ffs(scm_ptr->registers[id]->bgap_soc_mask);
	omap4plus_scm_writel(scm_ptr, temp,
			scm_ptr->registers[id]->temp_sensor_ctrl);
	/* Wait until DTEMP is updated */
	temp = omap4plus_scm_readl(scm_ptr,
			scm_ptr->registers[id]->temp_sensor_ctrl);
		temp &= (scm_ptr->registers[id]->bgap_dtemp_mask);
		while ((temp == 0) && --counter) {
			temp = omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[id]->temp_sensor_ctrl);
			temp &= (scm_ptr->registers[id]->bgap_dtemp_mask);
	}
	/* Start of Conversion = 0 */
	temp = omap4plus_scm_readl(scm_ptr,
			scm_ptr->registers[id]->temp_sensor_ctrl);
	temp &= ~(1 << __ffs(scm_ptr->registers[id]->bgap_soc_mask));
	omap4plus_scm_writel(scm_ptr, temp,
			scm_ptr->registers[id]->temp_sensor_ctrl);
}

static void omap4plus_scm_restore_ctxt(struct scm *scm_ptr)
{
	int i, temp = 0;

	if (!((scm_ptr->accurate) || (scm_ptr->rev == 2)))
		return;

	for (i = 0; i < scm_ptr->cnt; i++) {
		if ((omap4plus_scm_readl(scm_ptr,
			scm_ptr->registers[i]->bgap_counter) == 0)) {
			omap4plus_scm_writel(scm_ptr,
				scm_ptr->regval[i]->bg_threshold,
				scm_ptr->registers[i]->bgap_threshold);
			omap4plus_scm_writel(scm_ptr,
				scm_ptr->regval[i]->tshut_threshold,
					scm_ptr->registers[i]->tshut_threshold);
			/* Force immediate temperature measurement and update
			 * of the DTEMP field
			 */
			omap_temp_sensor_force_single_read(scm_ptr, i);
			omap4plus_scm_writel(scm_ptr,
				scm_ptr->regval[i]->bg_counter,
				scm_ptr->registers[i]->bgap_counter);
			omap4plus_scm_writel(scm_ptr,
				scm_ptr->regval[i]->bg_mode_ctrl,
				scm_ptr->registers[i]->bgap_mode_ctrl);
			omap4plus_scm_writel(scm_ptr,
				scm_ptr->regval[i]->bg_ctrl,
				scm_ptr->registers[i]->bgap_mask_ctrl);
		} else {
			temp = omap4plus_scm_readl(scm_ptr,
				scm_ptr->registers[i]->temp_sensor_ctrl);
			temp &= (scm_ptr->registers[i]->bgap_dtemp_mask);
			if (temp == 0) {
				omap_temp_sensor_force_single_read(scm_ptr, i);
				temp = omap4plus_scm_readl(scm_ptr,
					scm_ptr->registers[i]->bgap_mask_ctrl);
				temp |= 1 <<
				__ffs(scm_ptr->registers[i]->mode_ctrl_mask);
				omap4plus_scm_writel(scm_ptr, temp,
					scm_ptr->registers[i]->bgap_mask_ctrl);
			}
		}
	}
}

static int omap4plus_scm_suspend(struct device *dev)
{
	struct scm *scm_ptr = dev_get_drvdata(dev);

	omap4plus_scm_save_ctxt(scm_ptr);
	clk_disable(scm_ptr->fclock);

	return 0;
}

static int omap4plus_scm_resume(struct device *dev)
{
	struct scm *scm_ptr = dev_get_drvdata(dev);

	clk_enable(scm_ptr->fclock);
	omap4plus_scm_restore_ctxt(scm_ptr);

	return 0;
}
static const struct dev_pm_ops omap4plus_scm_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(omap4plus_scm_suspend,
			omap4plus_scm_resume)
};

#define DEV_PM_OPS	(&omap4plus_scm_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif

static struct platform_driver omap4plus_scm_driver = {
	.probe = omap4plus_scm_probe,
	.remove = omap4plus_scm_remove,
	.driver = {
			.name = "omap4plus_scm",
			.pm = DEV_PM_OPS,
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
