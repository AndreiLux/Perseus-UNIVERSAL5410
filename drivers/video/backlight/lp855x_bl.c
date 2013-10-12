/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/lp855x.h>
#include <linux/pwm.h>
#include "../secfb_notify.h"

/* Registers */
#define LP855X_BRIGHTNESS_CTRL		0x00
#define LP855X_DEVICE_CTRL		0x01
#define LP855X_EEPROM_START		0xA0
#define LP855X_EEPROM_END		0xA7
#define LP8556_EPROM_START		0x98
#define LP8556_EPROM_END		0xAF

#define DEFAULT_BL_NAME		"lcd-backlight"
#define MAX_BRIGHTNESS		255

enum lp855x_brightness_ctrl_mode {
	PWM_BASED = 1,
	REGISTER_BASED,
};

struct lp855x {
	const char *chipname;
	enum lp855x_chip_id chip_id;
	enum lp855x_brightness_ctrl_mode mode;
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	struct lp855x_platform_data *pdata;
	struct pwm_device *pwm;
	int power;

	int lth_brightness;
	int uth_brightness;
	int min_brightness;
	int pre_duty;
};

static int lp855x_read_byte(struct lp855x *lp, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	*data = (u8)ret;
	return 0;
}

static int lp855x_write_byte(struct lp855x *lp, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(lp->client, reg, data);
}

static bool lp855x_is_valid_rom_area(struct lp855x *lp, u8 addr)
{
	u8 start, end;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
		start = LP855X_EEPROM_START;
		end = LP855X_EEPROM_END;
		break;
	case LP8556:
		start = LP8556_EPROM_START;
		end = LP8556_EPROM_END;
		break;
	default:
		return false;
	}

	return (addr >= start && addr <= end);
}

static int lp855x_init_registers(struct lp855x *lp)
{
	u8 val, addr;
	int i, ret;
	struct lp855x_platform_data *pd = lp->pdata;

	val = pd->initial_brightness;
	ret = lp855x_write_byte(lp, LP855X_BRIGHTNESS_CTRL, val);
	if (ret)
		return ret;

	val = pd->device_control;
	ret = lp855x_write_byte(lp, LP855X_DEVICE_CTRL, val);
	if (ret)
		return ret;

	if (pd->load_new_rom_data && pd->size_program) {
		for (i = 0; i < pd->size_program; i++) {
			addr = pd->rom_data[i].addr;
			val = pd->rom_data[i].val;
			if (!lp855x_is_valid_rom_area(lp, addr))
				continue;

			ret = lp855x_write_byte(lp, addr, val);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void lp855x_pwm_ctrl(struct lp855x *lp, int br, int max_br)
{
	unsigned int period = lp->pdata->period_ns;
	unsigned int duty = br * period / max_br;

	if (br <= 0) {
		pwm_config(lp->pwm, 0, period);
		pwm_disable(lp->pwm);
	}
	else {
		if (br < lp->min_brightness)
			br = lp->min_brightness;

		duty = lp->lth_brightness +
			( (br - lp->min_brightness)
			* (lp->uth_brightness - lp->lth_brightness)
			/ (max_br - lp->min_brightness) );

		dev_dbg(lp->dev, "lth=%d, uth=%d, min=%d, max=%d, duty=%d, pre_duty = %d\n",
			lp->lth_brightness, lp->uth_brightness,
			lp->min_brightness, max_br, duty, lp->pre_duty);


		if(abs(lp->pre_duty - duty) > (10000000/period*5)) {
			lp->pre_duty = duty;
		pwm_config(lp->pwm, duty, period);
		pwm_enable(lp->pwm);
		}
		else
			dev_dbg(lp->dev, "pwm is not changed pre_duty =%d, duty = %d\n", lp->pre_duty, duty);
		

#ifdef CONFIG_LCD_LSL122DL01
		duty = (duty*100) / period;
		secfb_notifier_call_chain(SECFB_EVENT_BL_UPDATE, &duty);
#endif
	}
}

static int lp855x_bl_update_status(struct backlight_device *bl)
{
	int lp855x_brightness;
	struct lp855x *lp = bl_get_data(bl);

	lp855x_brightness = bl->props.brightness;

	if (bl->props.state & BL_CORE_SUSPENDED ||
	    bl->props.state & BL_CORE_FBBLANK) {
		lp855x_brightness = 0;
		lp->pre_duty = 0;
	}

	dev_info(lp->dev, "%s : state = %x, brightness = %d, real_br = %d\n",
			__func__, bl->props.state, bl->props.brightness,
			lp855x_brightness);

	if (lp->pdata->set_power &&
	    lp->power == 0 && lp855x_brightness == 0)
		return 0;

	if (lp->pdata->set_power && lp->power == 0) {
		lp->pdata->set_power(1);
		lp855x_init_registers(lp);
		lp->power = 1;
	}

	if (lp->mode == PWM_BASED) {
		int br = lp855x_brightness;
		int max_br = bl->props.max_brightness;
		lp855x_pwm_ctrl(lp, br, max_br);

	} else if (lp->mode == REGISTER_BASED) {
		u8 val = lp855x_brightness;
		lp855x_write_byte(lp, LP855X_BRIGHTNESS_CTRL, val);
	}

	if (lp->pdata->set_power && lp855x_brightness == 0) {
		lp->pdata->set_power(0);
		lp->power = 0;
	}

	return 0;
}

static int lp855x_bl_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops lp855x_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lp855x_bl_update_status,
	.get_brightness = lp855x_bl_get_brightness,
};

static int lp855x_backlight_register(struct lp855x *lp)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct lp855x_platform_data *pdata = lp->pdata;
	const char *name = pdata->name ? : DEFAULT_BL_NAME;

	props.type = BACKLIGHT_PLATFORM;
	
	if (pdata->max_brightness)
		props.max_brightness = pdata->max_brightness;
	else
		props.max_brightness = MAX_BRIGHTNESS;

	lp->min_brightness = pdata->min_brightness;
	if (lp->min_brightness < 0 ||
	    lp->min_brightness > props.max_brightness)
		lp->min_brightness = 0;

	lp->lth_brightness = pdata->lth_brightness *
			(pdata->period_ns / props.max_brightness);
	lp->uth_brightness = pdata->uth_brightness *
			(pdata->period_ns / props.max_brightness);

	if(lp->uth_brightness == 0 || lp->uth_brightness > pdata->period_ns)
		lp->uth_brightness = pdata->period_ns;

	if (pdata->initial_brightness > props.max_brightness)
		pdata->initial_brightness = props.max_brightness;

	props.brightness = pdata->initial_brightness;

	bl = backlight_device_register(name, lp->dev, lp,
				       &lp855x_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	lp->bl = bl;
	lp->pre_duty = 0;

	return 0;
}

static void lp855x_backlight_unregister(struct lp855x *lp)
{
	if (lp->bl)
		backlight_device_unregister(lp->bl);
}

static ssize_t lp855x_get_chip_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", lp->chipname);
}

static ssize_t lp855x_get_bl_ctl_mode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	char *strmode = NULL;

	if (lp->mode == PWM_BASED)
		strmode = "pwm based";
	else if (lp->mode == REGISTER_BASED)
		strmode = "register based";

	return scnprintf(buf, PAGE_SIZE, "%s\n", strmode);
}

static DEVICE_ATTR(chip_id, S_IRUGO, lp855x_get_chip_id, NULL);
static DEVICE_ATTR(bl_ctl_mode, S_IRUGO, lp855x_get_bl_ctl_mode, NULL);

static struct attribute *lp855x_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_bl_ctl_mode.attr,
	NULL,
};

static const struct attribute_group lp855x_attr_group = {
	.attrs = lp855x_attributes,
};

static int lp855x_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lp855x *lp;
	struct lp855x_platform_data *pdata = cl->dev.platform_data;
	int ret;

	dev_dbg(&cl->dev, "%s\n", __func__);

	if (!pdata) {
		dev_err(&cl->dev, "no platform data supplied\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp855x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	if (pdata->period_ns > 0)
		lp->mode = PWM_BASED;
	else
		lp->mode = REGISTER_BASED;

	lp->client = cl;
	lp->dev = &cl->dev;
	lp->pdata = pdata;
	lp->chipname = id->name;
	lp->chip_id = id->driver_data;
	i2c_set_clientdata(cl, lp);

	if (lp->mode == PWM_BASED) {
		char *name = pdata->name ? : DEFAULT_BL_NAME;
		lp->pwm = pwm_request(pdata->pwm_id, name);
		if (IS_ERR(lp->pwm)) {
			dev_err(&cl->dev, "unable to request legacy PWM\n");
			ret = PTR_ERR(lp->pwm);
			goto err_dev;
		}
		dev_dbg(&cl->dev, "got pwm for backlight\n");
	}

	if (pdata->set_power)
		pdata->set_power(1);
	lp->power = 1;

	ret = lp855x_init_registers(lp);
	if (ret) {
		dev_err(lp->dev, "i2c communication err: %d", ret);
		goto err_dev;
	}

	ret = lp855x_backlight_register(lp);
	if (ret) {
		dev_err(lp->dev,
			"failed to register backlight. err: %d\n", ret);
		goto err_dev;
	}

	ret = sysfs_create_group(&lp->dev->kobj, &lp855x_attr_group);
	if (ret) {
		dev_err(lp->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_sysfs;
	}

	backlight_update_status(lp->bl);
	if (pdata->post_init_device)
		pdata->post_init_device(lp->dev);

	return 0;

err_sysfs:
	lp855x_backlight_unregister(lp);
err_dev:
	return ret;
}

static int __devexit lp855x_remove(struct i2c_client *cl)
{
	struct lp855x *lp = i2c_get_clientdata(cl);

	lp->bl->props.brightness = 0;
	backlight_update_status(lp->bl);
	sysfs_remove_group(&lp->dev->kobj, &lp855x_attr_group);
	lp855x_backlight_unregister(lp);

	return 0;
}

static const struct i2c_device_id lp855x_ids[] = {
	{"lp8550", LP8550},
	{"lp8551", LP8551},
	{"lp8552", LP8552},
	{"lp8553", LP8553},
	{"lp8556", LP8556},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static struct i2c_driver lp855x_driver = {
	.driver = {
		.name = "lp855x",
		.owner		= THIS_MODULE,
	},
	.probe = lp855x_probe,
	.remove = __devexit_p(lp855x_remove),
	.id_table = lp855x_ids,
};

static int __init lp855x_init(void)
{
	int rval;

	rval = i2c_add_driver(&lp855x_driver);
	if (rval)
		printk(KERN_INFO "%s: failed registering LP855X\n",
		       __func__);

	return rval;
}

static void __exit lp855x_exit(void)
{
	i2c_del_driver(&lp855x_driver);
}

#if defined(CONFIG_FB_EXYNOS_FIMD_MC)
late_initcall_sync(lp855x_init);
module_exit(lp855x_exit);
#else
module_i2c_driver(lp855x_driver);
#endif

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
