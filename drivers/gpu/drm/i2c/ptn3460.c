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

#include "drmP.h"

#include "i2c/ptn3460.h"

#define PTN3460_EDID_EMULATION_ADDR		0x84
#define PTN3460_EDID_ENABLE_EMULATION		0
#define PTN3460_EDID_EMULATION_SELECTION	1

struct ptn3460_platform_data {
	struct device *dev;
	struct i2c_client *client;
	u8 addr;
	int gpio_pd_n;
	int gpio_rst_n;
	u32 edid_emulation;
	struct delayed_work ptn_work;
};

#define PTN3460_READY_BLOCK	0
#define PTN3460_READY_UNBLOCK	1

static atomic_t ptn3460_ready;
static wait_queue_head_t ptn3460_wait_queue;

int ptn3460_wait_until_ready(int timeout_ms)
{
	static atomic_t wait_queue_initialized;
	int ret;

	if (!of_find_compatible_node(NULL, NULL, "nxp,ptn3460"))
		return 0;

	if (atomic_add_return(1, &wait_queue_initialized) == 1)
		init_waitqueue_head(&ptn3460_wait_queue);

	ret = wait_event_timeout(ptn3460_wait_queue,
			atomic_read(&ptn3460_ready) == PTN3460_READY_UNBLOCK,
			msecs_to_jiffies(timeout_ms));
	if (!ret) {
		DRM_ERROR("Wait until ready timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int ptn3460_write_byte(struct ptn3460_platform_data *pd, char addr,
		char val)
{
	int ret;
	char buf[2];

	buf[0] = addr;
	buf[1] = val;

	ret = i2c_master_send(pd->client, buf, ARRAY_SIZE(buf));
	if (ret <= 0) {
		dev_err(pd->dev, "Failed to send i2c command, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int ptn3460_init_platform_data_from_dt(struct i2c_client *client)
{
	int ret;
	struct ptn3460_platform_data *pd;
	struct device *dev = &client->dev;

	dev->platform_data = devm_kzalloc(dev,
				sizeof(struct ptn3460_platform_data),
				GFP_KERNEL);
	if (!dev->platform_data) {
		dev_err(dev, "Can't allocate platform data\n");
		return -ENOMEM;
	}
	pd = dev->platform_data;
	pd->dev = dev;
	pd->client = client;

	/* Fill platform data with device tree data */
	pd->gpio_pd_n = of_get_named_gpio(dev->of_node, "powerdown-gpio", 0);
	pd->gpio_rst_n = of_get_named_gpio(dev->of_node, "reset-gpio", 0);

	ret = of_property_read_u32(dev->of_node, "edid-emulation",
			&pd->edid_emulation);
	if (ret) {
		dev_err(dev, "Can't read edid emulation value\n");
		return ret;
	}

	return 0;

}

static int ptn3460_select_edid(struct ptn3460_platform_data *pd)
{
	int ret;
	char val;

	val = 1 << PTN3460_EDID_ENABLE_EMULATION |
		pd->edid_emulation << PTN3460_EDID_EMULATION_SELECTION;

	ret = ptn3460_write_byte(pd, PTN3460_EDID_EMULATION_ADDR, val);
	if (ret) {
		dev_err(pd->dev, "Failed to write edid value, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static void ptn3460_work(struct work_struct *work)
{
	struct ptn3460_platform_data *pd =
		container_of(work, struct ptn3460_platform_data, ptn_work.work);
	int ret;

	if (!pd) {
		printk(KERN_ERR "pd is null\n");
		return;
	}

	ret = ptn3460_select_edid(pd);
	if (ret)
		dev_err(pd->dev, "Select edid failed ret=%d\n", ret);

	atomic_set(&ptn3460_ready, PTN3460_READY_UNBLOCK);
	wake_up(&ptn3460_wait_queue);
}

static int ptn3460_power_up(struct ptn3460_platform_data *pd)
{
	int ret;

	if (pd->gpio_pd_n > 0)
		gpio_set_value(pd->gpio_pd_n, 1);

	if (pd->gpio_rst_n > 0) {
		gpio_set_value(pd->gpio_rst_n, 0);
		udelay(10);
		gpio_set_value(pd->gpio_rst_n, 1);
	}

	ret = schedule_delayed_work(&pd->ptn_work, msecs_to_jiffies(90));
	if (ret < 0)
		dev_err(pd->dev, "Could not schedule work ret=%d\n", ret);

	return 0;
}

static int ptn3460_power_down(struct ptn3460_platform_data *pd)
{
	if (work_pending(&pd->ptn_work.work))
		flush_work_sync(&pd->ptn_work.work);

	atomic_set(&ptn3460_ready, PTN3460_READY_BLOCK);

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

	ret = ptn3460_init_platform_data_from_dt(client);
	if (ret)
		return ret;
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

	INIT_DELAYED_WORK(&pd->ptn_work, ptn3460_work);

	ret = ptn3460_power_up(pd);
	if (ret)
		goto err_pd;

	return 0;

err_pd:
	devm_kfree(dev, pd);
	return ret;
}

int ptn3460_remove(struct i2c_client *client)
{
	struct ptn3460_platform_data *pd = client->dev.platform_data;

	if (!pd)
		return 0;

	if (work_pending(&pd->ptn_work.work))
		flush_work_sync(&pd->ptn_work.work);

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
