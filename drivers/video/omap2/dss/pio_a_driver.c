/*
 * pio_a.c
 * programmable IO driver
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: mythripk <mythripk@ti.com>
 *	   Muralidhar Dixit <murali.dixit@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>

struct pio_a_i2c_data {
	struct i2c_client *pio_a_i2c_client;
	struct mutex xfer_lock;
}*pio_a_i2c_data;

static struct i2c_device_id pio_a_i2c_id[] = {
	{ "pio_a_i2c_driver", 0 },
};

static int pio_a_i2c_write_block(struct i2c_client *client,
					u8 *data, int len)
{
	struct i2c_msg msg;
	int i, r, msg_count = 1;

	if (len < 1 || len > 32) {
		dev_err(&client->dev,
			"too long syn_write_block len %d\n", len);
		return -EIO;
	}
	mutex_lock(&pio_a_i2c_data->xfer_lock);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;
	r = i2c_transfer(client->adapter, &msg, msg_count);
	mutex_unlock(&pio_a_i2c_data->xfer_lock);

	/*
	 * i2c_transfer returns:
	 * number of messages sent in case of success
	 * a negative error number in case of failure
	 */
	if (r != msg_count)
		goto err;

	/* In case of success */
	for (i = 0; i < len; i++)
		dev_dbg(&client->dev,
			"ad(struct i2c_client *clientdr %x bw 0x%02x[%d]: 0x%02x\n",
			client->addr, data[0] + i, i, data[i]);

	return 0;
err:
	dev_err(&client->dev, "pio_a_i2c_write error\n");
	return r;
}

int pio_a_i2c_write(u8 reg, u8 value)
{
	u8 data[2];

	data[0] = reg;
	data[1] = value;

	return pio_a_i2c_write_block(pio_a_i2c_data->pio_a_i2c_client, data, 2);
}
EXPORT_SYMBOL(pio_a_i2c_write);

static int pio_a_read_block(struct i2c_client *client, u8 reg, u8 *data, int len)
{
	struct i2c_msg msg[2];
	int i, r;

	struct pio_a_i2c_data *pio_a_i2c_data = i2c_get_clientdata(client);

	if (len < 1 || len > 32) {
		dev_err(&client->dev,
			"too long syn_read_block len %d\n", len);
		return -EIO;
	}

	mutex_lock(&pio_a_i2c_data->xfer_lock);

	msg[0].addr = client->addr;
	msg[0].len = 2;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data;

	r = i2c_transfer(client->adapter, msg, 2);
	mutex_unlock(&pio_a_i2c_data->xfer_lock);

	/*
	 * i2c_transfer returns:
	 * number of messages sent in case of success
	 * a negative error number in case of failure
	 */
	if (r != 2)
		return r ;

	/* In case of success */
	for (i = 0; i < len; i++)
		dev_dbg(&client->dev,
			"addr %x br 0x%02x[%d]: 0x%02x\n",
			client->addr, reg + i, i, data[i]);

	return 0;
}

int pio_a_read_byte(int reg)
{
	int r;
	u8 data;
	printk(KERN_DEBUG "pio_a_read_byte\n");

	if (pio_a_i2c_data->pio_a_i2c_client == NULL) {
		printk(KERN_DEBUG "pio_a_i2c_data->pio_a_i2c_client is null ??\n");
		return 0;
	} else {
	r = pio_a_read_block(pio_a_i2c_data->pio_a_i2c_client, reg, &data, 1);
	return ((int)(data));
	}
}
EXPORT_SYMBOL(pio_a_read_byte);

static int pio_a_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	printk(KERN_DEBUG "pio_a_i2c_probe called\n");
	pio_a_i2c_data = kzalloc(sizeof(struct pio_a_i2c_data), GFP_KERNEL);

	if (!pio_a_i2c_data)
		return -ENOMEM;

	mutex_init(&pio_a_i2c_data->xfer_lock);
	i2c_set_clientdata(client, pio_a_i2c_data);
	pio_a_i2c_data->pio_a_i2c_client = client;

	return 0;
}

static int pio_a_i2c_remove(struct i2c_client *client)
{
	kfree(pio_a_i2c_data);
	return 0;
}

static struct i2c_driver pio_a_i2c_driver = {
	.driver = {
		.name	= "pio_a_i2c_driver",
	},
	.probe		= pio_a_i2c_probe,
	.remove		= pio_a_i2c_remove,
	.id_table	= pio_a_i2c_id,
};

int pio_a_init(void)
{
	int r = 0;

	printk(KERN_DEBUG "pio_a_i2c_init called\n");
	r = i2c_add_driver(&pio_a_i2c_driver);
	if (r) {
		printk(KERN_WARNING "pio_a_i2c_driver" \
			" registration failed\n");
		return r;
	}

	return r;
}

void pio_a_exit(void)
{
	i2c_del_driver(&pio_a_i2c_driver);
}
