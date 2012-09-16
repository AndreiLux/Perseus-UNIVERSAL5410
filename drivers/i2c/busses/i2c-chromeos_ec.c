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
 * Expose an I2C passthrough to the ChromeOS EC.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/chromeos_ec.h>
#include <linux/mfd/chromeos_ec_commands.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct ec_i2c_device {
	struct device *dev;
	struct i2c_adapter adap;
	struct chromeos_ec_device *ec;
};

static int ec_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg i2c_msgs[],
		       int num)
{
	struct ec_i2c_device *bus = adap->algo_data;

	return bus->ec->command_i2c(bus->ec, i2c_msgs, num);
}

static u32 ec_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm ec_i2c_algorithm = {
	.master_xfer	= ec_i2c_xfer,
	.functionality	= ec_i2c_functionality,
};

static int __devinit ec_i2c_probe(struct platform_device *pdev)
{
	struct chromeos_ec_device *ec = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = ec->dev;
	struct ec_i2c_device *bus = NULL;
	int err;

	dev_dbg(dev, "EC I2C pass-through probing\n");

	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (bus == NULL) {
		err = -ENOMEM;
		dev_err(dev, "cannot allocate bus device\n");
		goto fail;
	}

	bus->ec = ec;
	bus->dev = dev;

	bus->adap.owner   = THIS_MODULE;
	bus->adap.retries = 3;
	bus->adap.nr = 0;
	strlcpy(bus->adap.name, "cros_ec_i2c", sizeof(bus->adap.name));
	bus->adap.algo = &ec_i2c_algorithm;
	bus->adap.algo_data = bus;
	bus->adap.dev.parent = &pdev->dev;
	err = i2c_add_adapter(&bus->adap);
	if (err) {
		dev_err(dev, "cannot register i2c adapter\n");
		goto fail_reg;
	}
	platform_set_drvdata(pdev, bus);

	dev_info(&pdev->dev, "%s: Chrome EC I2C pass-through adapter\n",
		 dev_name(bus->dev));
	return 0;
fail_reg:
	kfree(bus);
fail:
	return err;
}

static int __exit ec_i2c_remove(struct platform_device *dev)
{
	struct ec_i2c_device *bus = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	i2c_del_adapter(&bus->adap);
	kfree(bus);

	return 0;
}

static struct platform_driver ec_i2c_driver = {
	.probe = ec_i2c_probe,
	.remove = __exit_p(ec_i2c_remove),
	.driver = {
		.name = "cros_ec-i2c",
	},
};


module_platform_driver(ec_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EC I2C pass-through driver");
MODULE_ALIAS("platform:cros_ec-i2c");
