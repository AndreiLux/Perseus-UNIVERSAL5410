/*
 * Copyright 2011 Calxeda, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>

#include <mach/pl320-ipc.h>

struct hb_keys_drvdata {
	struct input_dev *input;
	struct notifier_block nb;
};

int hb_keys_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct hb_keys_drvdata *ddata = container_of(nb, struct hb_keys_drvdata, nb);
	struct input_dev *input = ddata->input;
	u32 *d = data;
	u32 key = d[0];

	if (event != 0x1000 /*HB_IPC_KEY*/)
		return 0;

	input_event(input, EV_KEY, key, 1);
	input_event(input, EV_KEY, key, 0);
	input_sync(input);
	return 0;
}

static int hb_keys_open(struct input_dev *input)
{
	struct hb_keys_drvdata *ddata = input_get_drvdata(input);
	return pl320_ipc_register_notifier(&ddata->nb);
}

static void hb_keys_close(struct input_dev *input)
{
	struct hb_keys_drvdata *ddata = input_get_drvdata(input);
	pl320_ipc_unregister_notifier(&ddata->nb);
}

static int __devinit hb_keys_probe(struct platform_device *pdev)
{
	struct hb_keys_drvdata *ddata;
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	int error;

	ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	ddata->input = input;
	ddata->nb.notifier_call = hb_keys_notifier;

	input->name = pdev->name;
	input->phys = "highbank/input0";
	input->dev.parent = &pdev->dev;
	input->open = hb_keys_open;
	input->close = hb_keys_close;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	input_set_capability(input, EV_KEY, KEY_POWER);
	input_set_capability(input, EV_KEY, KEY_SLEEP);

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail2;
	}

	return 0;

 fail2:
	input_free_device(input);
 fail1:
	kfree(ddata);
	return error;
}

static int __devexit hb_keys_remove(struct platform_device *pdev)
{
	struct hb_keys_drvdata *ddata = platform_get_drvdata(pdev);
	input_unregister_device(ddata->input);
	kfree(ddata);
	return 0;
}

static struct of_device_id hb_keys_of_match[] = {
	{ .compatible = "calxeda,hb-keys", },
	{ },
};
MODULE_DEVICE_TABLE(of, hb_keys_of_match);

static struct platform_driver hb_keys_driver = {
	.probe		= hb_keys_probe,
	.remove		= __devexit_p(hb_keys_remove),
	.driver		= {
		.name	= "hb-keys",
		.of_match_table = hb_keys_of_match,
	}
};

module_platform_driver(hb_keys_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Calxeda, Inc.");
MODULE_DESCRIPTION("Keys driver for Calxeda Highbank");
