/*
 * tps65090_charger.c - Power supply consumer driver for the TI TPS65090
 *
 * Copyright (c) 2012 Google, Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/mfd/tps65090.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "tps65090-private.h"

struct charger_data {
	struct device *dev;
	struct power_supply charger;
};

static enum power_supply_property tps65090_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS, /* "FULL" or "NOT FULL" only. */
	POWER_SUPPLY_PROP_PRESENT, /* the presence of battery */
	POWER_SUPPLY_PROP_ONLINE, /* charger is active or not */
};

static inline struct device *to_tps65090_dev(struct charger_data *charger_data)
{
	return charger_data->dev->parent;
}

static int tps65090_charger_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct charger_data *charger_data = container_of(psy,
			struct charger_data, charger);
	struct device *parent = to_tps65090_dev(charger_data);
	int ret;
	u8 reg;

	ret = tps65090_read(parent, TPS65090_REG_IRQ1, &reg);
	if (ret)
		return ret;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (reg & TPS65090_IRQ1_VACG_MASK) {
			if (reg & TPS65090_IRQ1_CGACT_MASK)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			if (reg & TPS65090_IRQ1_VBATG_MASK)
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !!(reg & TPS65090_IRQ1_VSYSG_MASK);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = !!(reg & TPS65090_IRQ1_VACG_MASK);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct of_device_id charger_data_match[] = {
	{ .compatible = "ti,tps65090" },
	{ }
};
MODULE_DEVICE_TABLE(of, charger_data_match);

static __devinit int tps65090_charger_probe(struct platform_device *pdev)
{
	struct charger_data *charger_data;
	int irq = 0;
	int ret = 0;

	charger_data = kzalloc(sizeof(struct charger_data), GFP_KERNEL);
	if (!charger_data) {
		dev_err(&pdev->dev, "Failed to alloc driver structure\n");
		return -ENOMEM;
	}

	charger_data->dev = &pdev->dev;

	charger_data->charger.name = "tps65090-charger";
	charger_data->charger.type = POWER_SUPPLY_TYPE_MAINS;
	charger_data->charger.get_property = tps65090_charger_get_property;
	charger_data->charger.properties = tps65090_charger_props;
	charger_data->charger.num_properties =
		ARRAY_SIZE(tps65090_charger_props);

	/* Register the driver. */
	ret = power_supply_register(charger_data->dev, &charger_data->charger);
	if (ret) {
		dev_err(charger_data->dev, "failed: power supply register\n");
		goto err;
	}

	platform_set_drvdata(pdev, charger_data);
	return 0;
err:
	kfree(charger_data);
	return ret;
}

static int __devexit tps65090_charger_remove(struct platform_device *pdev)
{
	struct charger_data *charger_data = platform_get_drvdata(pdev);

	power_supply_unregister(&charger_data->charger);
	platform_set_drvdata(pdev, NULL);
	kfree(charger_data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tps65090_charger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct charger_data *charger_data = platform_get_drvdata(pdev);

	power_supply_changed(&charger_data->charger);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(tps65090_charger_pm_ops, NULL,
			 tps65090_charger_resume);

static struct platform_driver tps65090_charger_driver = {
	.driver = {
		.name = "tps65090-charger",
		.owner = THIS_MODULE,
		.pm = &tps65090_charger_pm_ops,
		.of_match_table = of_match_ptr(charger_data_match),
	},
	.probe = tps65090_charger_probe,
	.remove = __devexit_p(tps65090_charger_remove),
};

static int __init tps65090_charger_init(void)
{
	return platform_driver_register(&tps65090_charger_driver);
}
subsys_initcall(tps65090_charger_init);

static void __exit tps65090_charger_exit(void)
{
	platform_driver_unregister(&tps65090_charger_driver);
}
module_exit(tps65090_charger_exit);

MODULE_DESCRIPTION("TI tps65090 battery charger driver");
MODULE_AUTHOR("Simon Glass <sjg@chromium.org>");
MODULE_LICENSE("GPL");
