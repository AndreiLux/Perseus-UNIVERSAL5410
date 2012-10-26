/*
 * NXP PTN3460 DP/LVDS bridge driver
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>

struct ptn3460_platform_data {
	u8 addr;
	int gpio_pd_n;
	int gpio_rst_n;
	int free_me;
};

static int ptn3460_init_platform_data_from_dt(struct device *dev)
{
	struct ptn3460_platform_data *pd;

	dev->platform_data = devm_kzalloc(dev,
				sizeof(struct ptn3460_platform_data),
				GFP_KERNEL);
	if (!dev->platform_data) {
		dev_err(dev, "Can't allocate platform data\n");
		return -ENOMEM;
	}
	pd = dev->platform_data;

	/* Fill platform data with device tree data */
	pd->gpio_pd_n = of_get_named_gpio(dev->of_node, "powerdown-gpio", 0);
	pd->gpio_rst_n = of_get_named_gpio(dev->of_node, "reset-gpio", 0);
	pd->free_me = 1; /* Mark this to be freed later */

	return 0;

}

static int ptn3460_power_up(struct ptn3460_platform_data *pd)
{
	if (pd->gpio_pd_n > 0)
		gpio_set_value(pd->gpio_pd_n, 1);

	if (pd->gpio_rst_n > 0) {
		gpio_set_value(pd->gpio_rst_n, 0);
		udelay(10);
		gpio_set_value(pd->gpio_rst_n, 1);
	}
	return 0;
}

static int ptn3460_power_down(struct ptn3460_platform_data *pd)
{
	if (pd->gpio_rst_n > 0)
		gpio_set_value(pd->gpio_rst_n, 1);

	if (pd->gpio_pd_n > 0)
		gpio_set_value(pd->gpio_pd_n, 0);

	return 0;
}

int ptn3460_suspend(struct device *dev)
{
	return ptn3460_power_down(dev->platform_data);
}

int ptn3460_resume(struct device *dev)
{
	return ptn3460_power_up(dev->platform_data);
}

int ptn3460_probe(struct i2c_client *client, const struct i2c_device_id *device)
{
	struct device *dev = &client->dev;
	struct ptn3460_platform_data *pd;
	int ret;

	if (!dev->platform_data) {
		ret = ptn3460_init_platform_data_from_dt(dev);
		if (ret)
			return ret;
	}
	pd = dev->platform_data;

	if (pd->gpio_pd_n > 0) {
		ret = gpio_request_one(pd->gpio_pd_n, GPIOF_OUT_INIT_HIGH,
					"PTN3460_PD_N");
		if (ret)
			goto err_pd;
	}
	if (pd->gpio_rst_n > 0) {
		/*
		 * Request the reset pin low to avoid the bridge being
		 * initialized prematurely
		 */
		ret = gpio_request_one(pd->gpio_rst_n, GPIOF_OUT_INIT_LOW,
					"PTN3460_RST_N");
		if (ret)
			goto err_pd;
	}

	ret = ptn3460_power_up(pd);
	if (ret)
		goto err_pd;

	return 0;

err_pd:
	if (pd->free_me)
		devm_kfree(dev, pd);
	return ret;
}

int ptn3460_remove(struct i2c_client *client)
{
	struct ptn3460_platform_data *pd = client->dev.platform_data;
	if (pd && pd->free_me)
		devm_kfree(&client->dev, pd);
	return 0;
}

static const struct i2c_device_id ptn3460_ids[] = {
	{ "ptn3460", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, ptn3460_ids);

static SIMPLE_DEV_PM_OPS(ptn3460_pm_ops, ptn3460_suspend, ptn3460_resume);

static struct i2c_driver ptn3460_driver = {
	.id_table	= ptn3460_ids,
	.probe          = ptn3460_probe,
	.remove         = ptn3460_remove,
	.driver		= {
		.name	= "ptn3460",
		.owner	= THIS_MODULE,
		.pm	= &ptn3460_pm_ops,
	},
};
module_i2c_driver(ptn3460_driver);
