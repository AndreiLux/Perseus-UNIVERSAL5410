/*
 * TI Palmas MFD Driver
 *
 * Copyright 2011 Texas Instruments Inc.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/palmas.h>

#include <linux/i2c/twl-rtc.h>

static struct resource gpadc_resource[] = {
	{
		.name = "EOC_SW",
		.start = PALMAS_GPADC_EOC_SW_IRQ,
		.end = PALMAS_GPADC_EOC_SW_IRQ,
		.flags = IORESOURCE_IRQ,
	}
};

static struct resource usb_resource[] = {
	{
		.name = "ID",
		.start = PALMAS_ID_OTG_IRQ,
		.end = PALMAS_ID_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "ID_WAKEUP",
		.start = PALMAS_ID_IRQ,
		.end = PALMAS_ID_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "VBUS",
		.start = PALMAS_VBUS_OTG_IRQ,
		.end = PALMAS_VBUS_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "VBUS_WAKEUP",
		.start = PALMAS_VBUS_IRQ,
		.end = PALMAS_VBUS_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource rtc_resource[] = {
	{
		.name = "RTC_ALARM",
		.start = PALMAS_RTC_ALARM_IRQ,
		.end = PALMAS_RTC_ALARM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource pwron_resource[] = {
	{
		.name = "PWRON_BUTTON",
		.start = PALMAS_PWRON_IRQ,
		.end = PALMAS_PWRON_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell palmas_children[] = {
	{
		.name = "palmas-gpadc",
		.num_resources = ARRAY_SIZE(gpadc_resource),
		.resources = gpadc_resource,

	},
	{
		.name = "palmas-pmic",
		.id = 1,
	},
	{
		.name = "palmas-usb",
		.num_resources = ARRAY_SIZE(usb_resource),
		.resources = usb_resource,
		.id = 2,
	},
	{
		.name = "palmas-gpio",
		.id = 3,
	},
	{
		.name = "palmas-resource",
		.id = 4,
	},
	{
		.name = "palmas_rtc",
		.num_resources = ARRAY_SIZE(rtc_resource),
		.resources = rtc_resource,
		.id = 5,
	},
	{
		.name = "palmas_pwron",
		.num_resources = ARRAY_SIZE(pwron_resource),
		.resources = pwron_resource,
		.id = 6,
	}
};

static int palmas_i2c_read_block(struct palmas *palmas, int ipblock, u8 reg,
		u8 *dest, int length)
{
	struct i2c_client *i2c;
	u8 addr;
	int ret;

	i2c = palmas->palmas_clients[(ipblock >> 8) - 1].client;
	addr = (ipblock & 0xFF) + reg;

	/* Write the Address */
	ret = i2c_master_send(i2c, &addr, 1);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EIO;

	/* Read the result */
	ret = i2c_master_recv(i2c, dest, length);

	if (ret < 0)
		return ret;
	else if (ret != length)
		return -EIO;
	return 0;
}

static int palmas_i2c_read(struct palmas *palmas, int ipblock, u8 reg,
		u8 *dest)
{
	return palmas_i2c_read_block(palmas, ipblock, reg, dest, 1);
}

static int palmas_i2c_write_block(struct palmas *palmas, int ipblock, u8 reg,
		u8 *data, int length)
{
	struct i2c_client *i2c;
	u8 addr;
	u8 buffer[PALMAS_MAX_BLOCK_TRANSFER + 1];
	int ret;

	if (length > PALMAS_MAX_BLOCK_TRANSFER) {
		dev_err(palmas->dev, "write bigger than buffer bytes %d\n",
				PALMAS_MAX_BLOCK_TRANSFER);
		return -EINVAL;
	}

	i2c = palmas->palmas_clients[(ipblock >> 8) - 1].client;
	addr = (ipblock & 0xFF) + reg;

	buffer[0] = addr;
	memcpy(buffer + 1, data, length);

	/* Write the Address */
	ret = i2c_master_send(i2c, buffer, length + 1);
	if (ret < 0)
		return ret;
	if (ret != (length + 1))
		return -EIO;

	return 0;
}

static int palmas_i2c_write(struct palmas *palmas, int ipblock, u8 reg,
		u8 data)
{
	return palmas_i2c_write_block(palmas, ipblock, reg, &data, 1);
}

/*
 * Keep the knowledge of the offsets and slave addresses for IP blocks in
 * the MFD device to make re-using IP blocks easier in future.
 */
#if defined(CONFIG_MFD_PALMAS_GPADC) || defined(CONFIG_MFD_PALMAS_GPADC_MODULE)
int palmas_gpadc_read(struct palmas *palmas, u8 reg, u8 *dest)
{
	return palmas->read(palmas, PALMAS_GPADC_BASE, reg, dest);
}
EXPORT_SYMBOL(palmas_gpadc_read);

int palmas_gpadc_trim_read_block(struct palmas *palmas, u8 reg, u8 *dest,
		int len)
{
	return palmas->read_block(palmas, PALMAS_GPADC_BASE, reg, dest, len);
}
EXPORT_SYMBOL(palmas_gpadc_trim_read_block);

int palmas_gpadc_write(struct palmas *palmas, u8 reg, u8 value)
{
	return palmas->write(palmas, PALMAS_GPADC_BASE, reg, value);
}
EXPORT_SYMBOL(palmas_gpadc_write);
#endif

#if defined(CONFIG_GPIO_PALMAS) || defined(CONFIG_GPIO_PALMAS_MODULE)
int palmas_gpio_read(struct palmas *palmas, u8 reg, u8 *dest)
{
	return palmas->read(palmas, PALMAS_GPIO_BASE, reg, dest);
}
EXPORT_SYMBOL(palmas_gpio_read);

int palmas_gpio_write(struct palmas *palmas, u8 reg, u8 value)
{
	return palmas->write(palmas, PALMAS_GPIO_BASE, reg, value);
}
EXPORT_SYMBOL(palmas_gpio_write);
#endif

#if defined(CONFIG_REGULATOR_PALMAS) || defined(CONFIG_REGULATOR_PALMAS_MODULE)
int palmas_smps_read(struct palmas *palmas, u8 reg, u8 *dest)
{
	return palmas->read(palmas, PALMAS_SMPS_BASE, reg, dest);
}
EXPORT_SYMBOL(palmas_smps_read);

int palmas_smps_write(struct palmas *palmas, u8 reg, u8 value)
{
	return palmas->write(palmas, PALMAS_SMPS_BASE, reg, value);
}
EXPORT_SYMBOL(palmas_smps_write);

int palmas_ldo_read(struct palmas *palmas, u8 reg, u8 *dest)
{
	return palmas->read(palmas, PALMAS_LDO_BASE, reg, dest);
}
EXPORT_SYMBOL(palmas_ldo_read);

int palmas_ldo_write(struct palmas *palmas, u8 reg, u8 value)
{
	return palmas->write(palmas, PALMAS_LDO_BASE, reg, value);
}
EXPORT_SYMBOL(palmas_ldo_write);
#endif

#if defined(CONFIG_MFD_PALMAS_RESOURCE) || \
	defined(CONFIG_MFD_PALMAS_RESOURCE_MODULE)
int palmas_resource_read(struct palmas *palmas, u8 reg, u8 *dest)
{
	return palmas->read(palmas, PALMAS_RESOURCE_BASE, reg, dest);
}
EXPORT_SYMBOL(palmas_resource_read);

int palmas_resource_write(struct palmas *palmas, u8 reg, u8 value)
{
	return palmas->write(palmas, PALMAS_RESOURCE_BASE, reg, value);
}
EXPORT_SYMBOL(palmas_resource_write);
#endif

#if defined(CONFIG_PALMAS_USB) || defined(CONFIG_PALMAS_USB_MODULE)
int palmas_usb_read(struct palmas *palmas, u8 reg, u8 *dest)
{
	return palmas->read(palmas, PALMAS_USB_OTG_BASE, reg, dest);
}
EXPORT_SYMBOL(palmas_usb_read);

int palmas_usb_write(struct palmas *palmas, u8 reg, u8 value)
{
	return palmas->write(palmas, PALMAS_USB_OTG_BASE, reg, value);
}
EXPORT_SYMBOL(palmas_usb_write);
#endif

static int palmas_rtc_read_block(void *mfd, u8 *dest, u8 reg, int no_regs)
{
	struct palmas *palmas = mfd;

	return palmas->read_block(palmas, PALMAS_RTC_BASE, reg, dest, no_regs);
}

static int palmas_rtc_write_block(void *mfd, u8 *data, u8 reg, int no_regs)
{
	struct palmas *palmas = mfd;

	/* twl added extra byte on beginning of block data */
	if (no_regs > 1)
		data++;

	return palmas->write_block(palmas, PALMAS_RTC_BASE, reg, data,
			no_regs);
}

static int palmas_rtc_init(struct palmas *palmas)
{
	struct twl_rtc_data *twl_rtc;

	twl_rtc = kzalloc(sizeof(*twl_rtc), GFP_KERNEL);

	twl_rtc->read_block = palmas_rtc_read_block;
	twl_rtc->write_block = palmas_rtc_write_block;
	twl_rtc->mfd = palmas;
	twl_rtc->chip_version = 6035;

	palmas_children[5].platform_data = twl_rtc;
	palmas_children[5].pdata_size = sizeof(*twl_rtc);

	return 0;
}

static int __devinit palmas_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct palmas *palmas;
	struct palmas_platform_data *mfd_platform_data;
	int ret = 0, i;
	u8 reg;

	mfd_platform_data = dev_get_platdata(&i2c->dev);
	if (!mfd_platform_data)
		return -EINVAL;

	palmas = kzalloc(sizeof(struct palmas), GFP_KERNEL);
	if (palmas == NULL)
		return -ENOMEM;

	palmas->palmas_clients = kcalloc(PALMAS_NUM_CLIENTS,
					sizeof(struct palmas_client),
					GFP_KERNEL);
	if (palmas->palmas_clients == NULL) {
		kfree(palmas);
		return -ENOMEM;
	}

	palmas->read = palmas_i2c_read;
	palmas->read_block = palmas_i2c_read_block;
	palmas->write = palmas_i2c_write;
	palmas->write_block = palmas_i2c_write_block;

	i2c_set_clientdata(i2c, palmas);
	palmas->dev = &i2c->dev;
	palmas->id = id->driver_data;
	mutex_init(&palmas->io_mutex);

	ret = irq_alloc_descs(-1, 0, PALMAS_NUM_IRQ, 0);
	if (ret < 0) {
		dev_err(&i2c->dev, "failed to allocate IRQ descs\n");
		goto err;
	}

	palmas->irq = i2c->irq;
	palmas->irq_base = ret;
	palmas->irq_end = ret + PALMAS_NUM_IRQ;

	for (i = 0; i < PALMAS_NUM_CLIENTS; i++) {
		palmas->palmas_clients[i].address = i2c->addr + i;
		if (i == 0)
			palmas->palmas_clients[i].client = i2c;
		else {
			palmas->palmas_clients[i].client =
					i2c_new_dummy(i2c->adapter,
					palmas->palmas_clients[i].address);
			if (!palmas->palmas_clients[i].client) {
				dev_err(&i2c->dev,
					"can't attach client %d\n", i);
				ret = -ENOMEM;
				goto err;
			}
		}
	}

	ret = palmas_irq_init(palmas);
	if (ret < 0)
		goto err;

	/* Discover the OTP Values and store them for later */
	ret = palmas->read(palmas, PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD1, &reg);
	if (ret)
		goto err;

	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_0))
		palmas->gpio_muxed |= 1 << 0;
	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_1_MASK))
		palmas->gpio_muxed |= 1 << 1;
	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_2_MASK))
		palmas->gpio_muxed |= 1 << 2;
	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_3))
		palmas->gpio_muxed |= 1 << 3;

	ret = palmas->read(palmas, PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD2, &reg);
	if (ret)
		goto err;

	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_4))
		palmas->gpio_muxed |= 1 << 4;
	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_5_MASK))
		palmas->gpio_muxed |= 1 << 5;
	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_6))
		palmas->gpio_muxed |= 1 << 6;
	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_7_MASK))
		palmas->gpio_muxed |= 1 << 7;

	reg = mfd_platform_data->power_ctrl;

	ret = palmas->write(palmas, PALMAS_PMU_CONTROL_BASE, PALMAS_POWER_CTRL,
			reg);
	if (ret)
		goto err;

	palmas_rtc_init(palmas);

	ret = mfd_add_devices(palmas->dev, -1,
			      palmas_children, ARRAY_SIZE(palmas_children),
			      NULL, palmas->irq_base);
	if (ret < 0)
		goto err;

	return ret;

err:
	mfd_remove_devices(palmas->dev);
	kfree(palmas->palmas_clients);
	kfree(palmas);
	return ret;
}

static int palmas_i2c_remove(struct i2c_client *i2c)
{
	struct palmas *palmas = i2c_get_clientdata(i2c);

	mfd_remove_devices(palmas->dev);
	palmas_irq_exit(palmas);
	kfree(palmas);

	return 0;
}

static const struct i2c_device_id palmas_i2c_id[] = {
	{ "twl6035", PALMAS_ID_TWL6035 },
	{ "tps65913", PALMAS_ID_TPS65913 },
};
MODULE_DEVICE_TABLE(i2c, palmas_i2c_id);

static struct of_device_id __devinitdata of_palmas_match_tbl[] = {
	{ .compatible = "ti,twl6035", },
	{ .compatible = "ti,tps65913", },
	{ /* end */ }
};

static struct i2c_driver palmas_i2c_driver = {
	.driver = {
		   .name = "palmas",
		   .of_match_table = of_palmas_match_tbl,
		   .owner = THIS_MODULE,
	},
	.probe = palmas_i2c_probe,
	.remove = palmas_i2c_remove,
	.id_table = palmas_i2c_id,
};

static int __init palmas_i2c_init(void)
{
	return i2c_add_driver(&palmas_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(palmas_i2c_init);

static void __exit palmas_i2c_exit(void)
{
	i2c_del_driver(&palmas_i2c_driver);
}
module_exit(palmas_i2c_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas chip family multi-function driver");
MODULE_LICENSE("GPL");
