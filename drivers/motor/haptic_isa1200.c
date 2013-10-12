/* drivers/motor/haptic_isa1200.c
 *
 * Copyright (C) 2011 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/haptic_isa1200.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#if defined(CONFIG_HAPTIC)
#include "haptic.h"
#endif

int isa1200_i2c_write(struct i2c_client *client,
					u8 addr, u8 val)
{
	int error = 0;
	error = i2c_smbus_write_byte_data(client, addr, val);
	if (error)
		printk(KERN_ERR
			"[VIB] Failed to write addr=[0x%x], val=[0x%x]\n",
			addr, val);

	return error;
}

static void isa1200_reg_write(struct isa1200_drvdata *data,
	u8 type)
{
	int i = 0, j = 0;

	for (i  = 0; i < ISA1200_REG_MAX; i++) {
		if (data->pdata->reg_data[i][0] == type)
			for (j = 2; j < data->pdata->reg_data[i][1]; j += 2)
				isa1200_i2c_write(data->client,
					data->pdata->reg_data[i][j],
					data->pdata->reg_data[i][j + 1]);
	}
}

static void isa1200_hw_init(struct isa1200_drvdata *data)
{
	isa1200_reg_write(data, ISA1200_REG_INIT);
}

static void isa1200_on(struct isa1200_drvdata *data, bool on)
{
	isa1200_reg_write(data,
		on ? ISA1200_REG_START : ISA1200_REG_STOP);
}

static enum hrtimer_restart isa1200_timer_func(struct hrtimer *_timer)
{
	struct isa1200_drvdata *data =
		container_of(_timer, struct isa1200_drvdata, timer);

	data->timeout = 0;

	schedule_work(&data->work);
	return HRTIMER_NORESTART;
}

static void isa1200_work(struct work_struct *_work)
{
	struct isa1200_drvdata *data =
		container_of(_work, struct isa1200_drvdata, work);

	if (0 == data->timeout) {
		if (!data->running)
			return ;

		data->running = false;
		isa1200_on(data, false);
		data->pdata->pwm_en(false);
		data->pdata->pwm_cfg(500);
	} else {
		if (data->running)
			return ;

		data->running = true;
		data->pdata->pwm_cfg(999);
		data->pdata->pwm_en(true);
		usleep_range(1000, 1500);
		isa1200_on(data, true);
	}
}

static int isa1200_get_time(struct timed_output_dev *_dev)
{
	struct isa1200_drvdata	*data =
		container_of(_dev, struct isa1200_drvdata, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void isa1200_enable(struct timed_output_dev *_dev, int value)
{
	struct isa1200_drvdata	*data =
		container_of(_dev, struct isa1200_drvdata, dev);
	unsigned long	flags;

#if defined(ISA1200_DEBUG_LOG)
	printk(KERN_DEBUG "[VIB] time = %dms\n", value);
#endif
	cancel_work_sync(&data->work);
	hrtimer_cancel(&data->timer);
	data->timeout = value;
	schedule_work(&data->work);
	spin_lock_irqsave(&data->lock, flags);
	if (value > 0) {
		if (value > data->max_timeout)
			value = data->max_timeout;

		hrtimer_start(&data->timer,
			ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

#if defined(CONFIG_HAPTIC)
static struct isa1200_drvdata *isa1200_ddata;
void isa1200_set_force(u8 index, int duty)
{
	if (NULL == isa1200_ddata) {
		printk(KERN_ERR "[VIB] driver is not ready\n");
		return ;
	}

	/* for the external pwm */
	/* if the isa1200 is used by internal clock,
	 * this function should be fixed. */
	isa1200_ddata->pdata->pwm_cfg(duty);
}

void isa1200_chip_enable(bool en)
{
	if (NULL == isa1200_ddata) {
		printk(KERN_ERR "[VIB] driver is not ready\n");
		return ;
	}
	isa1200_ddata->pdata->pwm_en(en);
	isa1200_on(isa1200_ddata, en);
}
#endif	/* CONFIG_HAPTIC */


static int __devinit isa1200_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct isa1200_pdata *pdata = client->dev.platform_data;
	struct isa1200_drvdata *ddata;
#if defined(CONFIG_HAPTIC)
	struct vibe_drvdata *vibe;
#endif
	int ret = 0;

	ddata = kzalloc(sizeof(struct isa1200_drvdata), GFP_KERNEL);
	if (NULL == ddata) {
		printk(KERN_ERR "[VIB] Failed to alloc memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	ddata->client = client;
	if (NULL == pdata) {
		printk(KERN_ERR "[VIB] the platform data is empty.\n");
		goto err_platform_data;
	} else
		ddata->pdata = pdata;
	ddata->max_timeout = pdata->max_timeout;

	ddata->dev.name = "vibrator";
	ddata->dev.get_time = isa1200_get_time;
	ddata->dev.enable = isa1200_enable;

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = isa1200_timer_func;
	INIT_WORK(&ddata->work, isa1200_work);
	spin_lock_init(&ddata->lock);

	i2c_set_clientdata(client, ddata);
	pdata->gpio_en(true);
	pdata->pwm_init();
	msleep(20);
	isa1200_hw_init(ddata);

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		printk(KERN_ERR "[VIB] Failed to register timed_output : -%d\n", ret);
		goto err_to_dev_reg;
	}

#if defined(CONFIG_HAPTIC)
	vibe = kzalloc(sizeof(struct vibe_drvdata), GFP_KERNEL);
	if (vibe) {
		isa1200_ddata = ddata;
		vibe->set_force = isa1200_set_force;
		vibe->chip_en = isa1200_chip_enable;
		vibe->num_actuators = 1;
		tspdrv_init(vibe);
	} else
		printk(KERN_ERR "[VIB] Failed to alloc vibe memory.\n");
#endif

	return 0;

err_to_dev_reg:
err_request_clk:
err_platform_data:
	kfree(ddata);
err_free_mem:
	return ret;
}

static int isa1200_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct isa1200_drvdata *ddata  = i2c_get_clientdata(client);
	ddata->pdata->gpio_en(false);
	return 0;
}

static int isa1200_resume(struct i2c_client *client)
{
	struct isa1200_drvdata *ddata  = i2c_get_clientdata(client);
	ddata->pdata->gpio_en(true);
	msleep(20);
	isa1200_hw_init(ddata);
	return 0;
}

static const struct i2c_device_id isa1200_device_id[] = {
	{ISA1200_DEVICE_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, isa1200_device_id);

static struct i2c_driver isa1200_i2c_driver = {
	.driver = {
		.name = ISA1200_DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe     = isa1200_i2c_probe,
	.id_table  = isa1200_device_id,
	.suspend	= isa1200_suspend,
	.resume	= isa1200_resume,
};

static int __init isa1200_init(void)
{
	return i2c_add_driver(&isa1200_i2c_driver);
}

static void __exit isa1200_exit(void)
{
	i2c_del_driver(&isa1200_i2c_driver);
}

module_init(isa1200_init);
module_exit(isa1200_exit);

MODULE_AUTHOR("junki671.min@samsung.com");
MODULE_DESCRIPTION("isa1200 driver for haptic solution");
MODULE_LICENSE("GPL");
