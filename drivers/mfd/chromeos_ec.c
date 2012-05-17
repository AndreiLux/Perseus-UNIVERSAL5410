/*
 *  Copyright (C) 2012 Google, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * The ChromeOS EC multi function device is used to mux all the requests
 * to the EC device for its multiple features : keyboard controller,
 * battery charging and regulator control, firmware update.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/chromeos_ec.h>
#include <linux/platform_device.h>
#include <linux/slab.h>


#define MKBP_MAX_TRIES 3

/* Send a one-byte command to the keyboard and receive a response of length
 * BUF_LEN.  Return BUF_LEN, or a negative error code.
 */
static int mkbp_command_noretry(struct chromeos_ec_device *ec_dev,
				char cmd, uint8_t *buf, int buf_len)
{
	int ret;
	int i;
	int packet_len;
	uint8_t *packet;
	uint8_t sum;

	/* allocate larger packet (one extra byte for checksum) */
	packet_len = buf_len + MKBP_MSG_PROTO_BYTES;
	packet = kzalloc(packet_len, GFP_KERNEL);
	if (!packet)
		return -ENOMEM;

	/* send command to EC */
	ret = i2c_master_send(ec_dev->client, &cmd, 1);
	if (ret < 0) {
		dev_err(ec_dev->dev, "i2c send failed: %d\n", ret);
		goto done;
	}
	/* receive response */
	ret = i2c_master_recv(ec_dev->client, packet, packet_len);
	if (ret < 0) {
		dev_err(ec_dev->dev, "i2c receive failed: %d\n", ret);
		goto done;
	} else if (ret != packet_len) {
		dev_err(ec_dev->dev, "expected %d bytes, got %d\n",
			packet_len, ret);
		ret = -EIO;
		goto done;
	}
	/* copy response packet payload and compute checksum */
	for (i = 0, sum = 0; i < buf_len; i++) {
		buf[i] = packet[i];
		sum += buf[i];
	}
#ifdef DEBUG
	dev_dbg(ec_dev->dev, "packet: ");
	for (i = 0; i < packet_len; i++) {
		printk(" %02x", packet[i]);
	}
	printk(", sum = %02x\n", sum);
#endif
	if (sum != packet[packet_len - 1]) {
		dev_err(ec_dev->dev, "bad keyboard packet checksum\n");
		ret = -EIO;
		goto done;
	}
	ret = buf_len;
 done:
	kfree(packet);
	return ret;
}

static int mkbp_command(struct chromeos_ec_device *ec_dev,
			char cmd, uint8_t *buf, int buf_len)
{
	int try;
	int ret;
	/*
	 * Try the command a few times in case there are transmission errors.
	 * It is possible that this is overkill, but we don't completely trust
	 * i2c.
	 */
	for (try = 0; try < MKBP_MAX_TRIES; try++) {
		ret = mkbp_command_noretry(ec_dev, cmd, buf, buf_len);
		if (ret >= 0)
			return ret;
	}
	dev_err(ec_dev->dev, "mkbp_command failed with %d (%d tries)\n",
		ret, try);
	return ret;
}

static irqreturn_t mkbp_isr(int irq, void *data)
{
	struct chromeos_ec_device *ec = data;

	atomic_notifier_call_chain(&ec->event_notifier, 1, ec);

	return IRQ_HANDLED;
}

static int __devinit mkbp_check_protocol_version(struct chromeos_ec_device *ec)
{
	int ret;
	char buf[4];
	static char expected_version[4] = {1, 0, 0, 0};
	int i;

	ret = mkbp_command(ec, MKBP_CMDC_PROTO_VER, buf, sizeof(buf));
	if (ret < 0)
		return ret;
	for (i = 0; i < sizeof(expected_version); i++) {
		if (buf[i] != expected_version[i])
			return -EPROTONOSUPPORT;
	}
	return 0;
}

static struct mfd_cell cros_devs[] = {
	{
		.name = "mkbp",
		.id = 1,
	},
};

static int __devinit cros_ec_probe(struct i2c_client *client,
				   const struct i2c_device_id *dev_id)
{
	struct device *dev = &client->dev;
	struct chromeos_ec_device *ec_dev = NULL;
	int err;

	dev_dbg(dev, "probing\n");

	ec_dev = kzalloc(sizeof(*ec_dev), GFP_KERNEL);
	if (ec_dev == NULL) {
		err = -ENOMEM;
		dev_err(dev, "cannot allocate\n");
		goto fail;
	}

	ec_dev->client = client;
	ec_dev->dev = dev;
	i2c_set_clientdata(client, ec_dev);
	ec_dev->irq = client->irq;
	ec_dev->send_command = mkbp_command;

	ATOMIC_INIT_NOTIFIER_HEAD(&ec_dev->event_notifier);

	err = request_irq(ec_dev->irq, mkbp_isr,
			  IRQF_TRIGGER_FALLING, "chromeos-ec", ec_dev);
	if (err) {
		dev_err(dev, "request irq %d: error %d\n", ec_dev->irq, err);
		goto fail;
	}

	err = mkbp_check_protocol_version(ec_dev);
	if (err < 0) {
		dev_err(dev, "protocol version check failed: %d\n", err);
		goto fail_irq;
	}

	err = mfd_add_devices(dev, 0, cros_devs, ARRAY_SIZE(cros_devs),
			      NULL, ec_dev->irq);
	if (err)
		goto fail_irq;

	return 0;
fail_irq:
	free_irq(ec_dev->irq, ec_dev);
fail:
	kfree(ec_dev);
	return err;
}

#ifdef CONFIG_PM_SLEEP
static int cros_ec_suspend(struct device *dev)
{
	return 0;
}

static int cros_ec_resume(struct device *dev)
{
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(cros_ec_pm_ops, cros_ec_suspend, cros_ec_resume);

static const struct i2c_device_id cros_ec_i2c_id[] = {
	{ "chromeos-ec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mkbp_i2c_id);

static struct i2c_driver cros_ec_driver = {
	.driver	= {
		.name	= "chromeos-ec",
		.owner	= THIS_MODULE,
		.pm	= &cros_ec_pm_ops,
	},
	.probe		= cros_ec_probe,
	.id_table	= cros_ec_i2c_id,
};

module_i2c_driver(cros_ec_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC multi function device");
