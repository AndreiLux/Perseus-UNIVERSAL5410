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
#include <linux/mfd/chromeos_ec_commands.h>
#include <linux/platform_device.h>
#include <linux/slab.h>


#define COMMAND_MAX_TRIES 3

static inline struct chromeos_ec_device *to_ec_dev(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	return i2c_get_clientdata(client);
}

static int cros_ec_command_xfer_noretry(struct chromeos_ec_device *ec_dev,
					struct chromeos_ec_msg *msg)
{
	int ret = -ENOMEM;
	int i;
	int packet_len;
	u8 res_code;
	u8 *out_buf = NULL;
	u8 *in_buf = NULL;
	u8 sum;
	struct i2c_msg i2c_msg[2];

	i2c_msg[0].addr = ec_dev->client->addr;
	i2c_msg[0].flags = 0;
	i2c_msg[1].addr = ec_dev->client->addr;
	i2c_msg[1].flags = I2C_M_RD;

	if (msg->in_len) {
		/* allocate larger packet
		 * (one byte for checksum, one for result code)
		 */
		packet_len = msg->in_len + 2;
		in_buf = kzalloc(packet_len, GFP_KERNEL);
		if (!in_buf)
			goto done;
		i2c_msg[1].len = packet_len;
		i2c_msg[1].buf = (char *)in_buf;
	} else {
		i2c_msg[1].len = 1;
		i2c_msg[1].buf = (char *)&res_code;
	}

	if (msg->out_len) {
		/* allocate larger packet
		 * (one byte for checksum, one for command code)
		 */
		packet_len = msg->out_len + 2;
		out_buf = kzalloc(packet_len, GFP_KERNEL);
		if (!out_buf)
			goto done;
		i2c_msg[0].len = packet_len;
		i2c_msg[0].buf = (char *)out_buf;
		out_buf[0] = msg->cmd;

		/* copy message payload and compute checksum */
		for (i = 0, sum = 0; i < msg->out_len; i++) {
			out_buf[i + 1] = msg->out_buf[i];
			sum += out_buf[i + 1];
		}
		out_buf[msg->out_len + 1] = sum;
	} else {
		i2c_msg[0].len = 1;
		i2c_msg[0].buf = (char *)&msg->cmd;
	}

	/* send command to EC and read answer */
	ret = i2c_transfer(ec_dev->client->adapter, i2c_msg, 2);
	if (ret < 0) {
		dev_err(ec_dev->dev, "i2c transfer failed: %d\n", ret);
		goto done;
	} else if (ret != 2) {
		dev_err(ec_dev->dev, "failed to get response: %d\n", ret);
		ret = -EIO;
		goto done;
	}

	/* check response error code */
	if (i2c_msg[1].buf[0]) {
		dev_warn(ec_dev->dev, "command 0x%02x returned an error %d\n",
			 msg->cmd, i2c_msg[1].buf[0]);
		ret = -EINVAL;
		goto done;
	}
	if (msg->in_len) {
		/* copy response packet payload and compute checksum */
		for (i = 0, sum = 0; i < msg->in_len; i++) {
			msg->in_buf[i] = in_buf[i + 1];
			sum += in_buf[i + 1];
		}
#ifdef DEBUG
		dev_dbg(ec_dev->dev, "packet: ");
		for (i = 0; i < i2c_msg[1].len; i++)
			printk(" %02x", in_buf[i]);
		printk(", sum = %02x\n", sum);
#endif
		if (sum != in_buf[msg->in_len + 1]) {
			dev_err(ec_dev->dev, "bad packet checksum\n");
			ret = -EBADMSG;
			goto done;
		}
	}

	ret = 0;
 done:
	kfree(in_buf);
	kfree(out_buf);
	return ret;
}

static int cros_ec_command_xfer(struct chromeos_ec_device *ec_dev,
				 struct chromeos_ec_msg *msg)
{
	int tries;
	int ret;
	/*
	 * Try the command a few times in case there are transmission errors.
	 * It is possible that this is overkill, but we don't completely trust
	 * i2c.
	 */
	for (tries = 0; tries < COMMAND_MAX_TRIES; tries++) {
		ret = cros_ec_command_xfer_noretry(ec_dev, msg);
		if (ret >= 0)
			return ret;
	}
	dev_err(ec_dev->dev, "ec_command failed with %d (%d tries)\n",
		ret, tries);
	return ret;
}

static int cros_ec_command_raw(struct chromeos_ec_device *ec_dev,
			       struct i2c_msg *msgs, int num)
{
	return i2c_transfer(ec_dev->client->adapter, msgs, num);
}

static int cros_ec_command_recv(struct chromeos_ec_device *ec_dev,
				char cmd, void *buf, int buf_len)
{
	struct chromeos_ec_msg msg;

	msg.cmd = cmd;
	msg.in_buf = buf;
	msg.in_len = buf_len;
	msg.out_buf = NULL;
	msg.out_len = 0;

	return cros_ec_command_xfer(ec_dev, &msg);
}

static int cros_ec_command_send(struct chromeos_ec_device *ec_dev,
				char cmd, void *buf, int buf_len)
{
	struct chromeos_ec_msg msg;

	msg.cmd = cmd;
	msg.out_buf = buf;
	msg.out_len = buf_len;
	msg.in_buf = NULL;
	msg.in_len = 0;

	return cros_ec_command_xfer(ec_dev, &msg);
}

static irqreturn_t ec_irq_thread(int irq, void *data)
{
	struct chromeos_ec_device *ec = data;

	if (device_may_wakeup(ec->dev))
		pm_wakeup_event(ec->dev, 0);

	blocking_notifier_call_chain(&ec->event_notifier, 1, ec);

	return IRQ_HANDLED;
}

static int __devinit check_protocol_version(struct chromeos_ec_device *ec)
{
	int ret;
	struct ec_response_proto_version data;

	ret = cros_ec_command_recv(ec, EC_CMD_PROTO_VERSION,
				   &data, sizeof(data));
	if (ret < 0)
		return ret;
	dev_info(ec->dev, "protocol version: %d\n", data.version);
	if (data.version != EC_PROTO_VERSION)
		return -EPROTONOSUPPORT;

	return 0;
}

static struct mfd_cell cros_devs[] = {
	{
		.name = "mkbp",
		.id = 1,
	},
	{
		.name = "cros_ec-fw",
		.id = 2,
	},
	{
		.name = "cros_ec-i2c",
		.id = 3,
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
	ec_dev->command_send = cros_ec_command_send;
	ec_dev->command_recv = cros_ec_command_recv;
	ec_dev->command_xfer = cros_ec_command_xfer;
	ec_dev->command_raw  = cros_ec_command_raw;

	BLOCKING_INIT_NOTIFIER_HEAD(&ec_dev->event_notifier);

	err = request_threaded_irq(ec_dev->irq, NULL, ec_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "chromeos-ec", ec_dev);
	if (err) {
		dev_err(dev, "request irq %d: error %d\n", ec_dev->irq, err);
		goto fail;
	}

	err = check_protocol_version(ec_dev);
	if (err < 0) {
		dev_err(dev, "protocol version check failed: %d\n", err);
		goto fail_irq;
	}

	err = mfd_add_devices(dev, 0, cros_devs,
			      ARRAY_SIZE(cros_devs),
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
	struct chromeos_ec_device *ec = to_ec_dev(dev);

	if (device_may_wakeup(dev))
		ec->wake_enabled = !enable_irq_wake(ec->irq);

	disable_irq(ec->irq);

	return 0;
}

static int cros_ec_resume(struct device *dev)
{
	struct chromeos_ec_device *ec = to_ec_dev(dev);

	enable_irq(ec->irq);

	if (ec->wake_enabled) {
		disable_irq_wake(ec->irq);
		ec->wake_enabled = 0;
	}

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
