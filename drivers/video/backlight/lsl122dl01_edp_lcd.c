/* linux/drivers/video/backlight/lsl122dl01_edp_lcd.c
 *
 * Samsung SoC LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <video/s5p-dp.h>
#include "../s5p-dp-core.h"
#include "../secfb_notify.h"

#ifdef CONFIG_N1A
#include "n1_power_save.h"
#else
#include "v1_power_save.h"
#endif

#define LCD_NAME	"panel"
#define BL_NAME		"tcon"

#ifdef CONFIG_N1A
#define PANEL_NAME	"INH_LSL101DL01"
#else
#define PANEL_NAME	"INH_LSL122DL01"
#endif

static struct class *tcon_class;

struct lsl122dl01 {
	const char *chipname;
	struct device *dev;
	struct i2c_client *client;
	struct lcd_device *lcd;
	struct backlight_device *bl;
	struct device *tcon_dev;
	struct s5p_dp_device *dp;
	struct notifier_block secfb_notif;
	
	int dblc_mode;
	int dblc_lux;
	int dblc_auto_br;
	int dblc_power_save;
	int dblc_duty;

	int i2c_slave;

	struct mutex ops_lock;
};

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", PANEL_NAME);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0 0 0\n");
}

static DEVICE_ATTR(lcd_type, S_IRUGO, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, S_IRUGO, window_type_show, NULL);

static struct attribute *lcd_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	NULL,
};

static const struct attribute_group lcd_attr_group = {
	.attrs = lcd_attributes,
};

static int lsl122dl01_lcd_match(struct lcd_device *lcd, struct fb_info *info)
{
	struct lsl122dl01 *plcd = lcd_get_data(lcd);
#if 0
	return plcd->dev->parent == info->device;
#else
	return 0;
#endif
}

static struct lcd_ops lsl122dl01_lcd_ops = {
	.check_fb	= lsl122dl01_lcd_match,
};

/**********************************
 * lcd tcon 
 *********************************/
static int lsl122dl01_i2c_read(struct i2c_client *client, u16 reg, u8 *data)
{
	int ret;
	struct i2c_msg msg[2];
	u8 buf1[] = { reg >> 8, reg & 0xFF };

	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 2;
	msg[0].buf   = buf1;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = 1;
	msg[1].buf   = data;

	ret = i2c_transfer(client->adapter, msg, 2);

	if  (ret == 2)
		dev_dbg(&client->dev, "%s ok", __func__);
	else
		dev_err(&client->dev, "%s fail err = %d", __func__, ret);

	return ret;
}

static int lsl122dl01_i2c_write(struct i2c_client *client, u16 reg, u8 data)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 buf1[3] = { reg >> 8, reg & 0xFF,  data};

	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = sizeof(buf1);
	msg[0].buf   = buf1;

	ret = i2c_transfer(client->adapter, msg, 1);

	dev_dbg(&client->dev, "write addr : 0x%x data : 0x%x", reg, data);

	if  (ret == 1)
		dev_dbg(&client->dev, "%s ok", __func__);
	else
		dev_err(&client->dev, "%s fail err = %d", __func__, ret);

	return ret;
}

static int tcon_i2c_slave_enable(struct lsl122dl01 *plcd)
{
	int ret = 0;
	u8 cmd1_buf[3] = {0x03, 0x13, 0xBB};
	u8 cmd2_buf[3] = {0x03, 0x14, 0xBB};

	mutex_lock(&plcd->dp->lock);
	
	if (!plcd->dp->enabled) {
		dev_err(plcd->dev, "%s: DP state power off\n", __func__);
		goto err_dp;
	}
	
	ret = s5p_dp_write_bytes_to_dpcd(plcd->dp, 0x491,
			ARRAY_SIZE(cmd1_buf), cmd1_buf);
	if (ret < 0)
		goto err_dp;
	
	ret = s5p_dp_write_bytes_to_dpcd(plcd->dp, 0x491,
			ARRAY_SIZE(cmd2_buf), cmd2_buf);
	if (ret < 0)
		goto err_dp;

	mutex_unlock(&plcd->dp->lock);
	return 0;

err_dp:
	mutex_unlock(&plcd->dp->lock);
	return -EINVAL;
}

static int tcon_black_frame_bl_on(struct lsl122dl01 *plcd)
{
	struct tcon_reg_info *tune_value;
	int loop;

	tune_value = &TCON_BLACK_IMAGE_BLU_ENABLE;
	for(loop = 0; loop < tune_value->reg_cnt; loop++) {
		lsl122dl01_i2c_write(plcd->client,
			  tune_value->addr[loop], tune_value->data[loop]);
	}

	//Enable double bufferd regiset
	lsl122dl01_i2c_write(plcd->client, 0x0F10, 0x80);

	return 0;
}

#ifdef CONFIG_TCON_SET_MNBL
static int tcon_set_under_mnbl_duty(struct lsl122dl01 *plcd, unsigned int duty)
{
	struct tcon_reg_info *tune_value;

	if (duty <= MN_BL) {
		lsl122dl01_i2c_write(plcd->client, 0x0DB9, 0x7F);
		lsl122dl01_i2c_write(plcd->client, 0x0DBA, 0xFF);
	} else {
		tune_value = tcon_tune_value[plcd->dblc_auto_br][plcd->dblc_lux][plcd->dblc_mode];

		if (!tune_value) {
			dev_err(plcd->dev, "%s: tcon value is null\n", __func__);
			return -EINVAL;
		}

		lsl122dl01_i2c_write(plcd->client,
					tune_value->addr[MNBL_INDEX1], tune_value->data[MNBL_INDEX1]);
		lsl122dl01_i2c_write(plcd->client,
					tune_value->addr[MNBL_INDEX2], tune_value->data[MNBL_INDEX2]);
	}

	//Enable double bufferd regiset
	lsl122dl01_i2c_write(plcd->client, 0x0F10, 0x80);

	return 0;
}
#else
static int tcon_set_under_mnbl_duty(struct lsl122dl01 *plcd, unsigned int duty)
{
	return 0;
}
#endif

static int tcon_tune_write(struct lsl122dl01 *plcd)
{
	struct tcon_reg_info *tune_value;
	int loop;

	if (!plcd->i2c_slave) {
		dev_err(plcd->dev, "%s: tcon i2c is not slave\n", __func__);
		return -EIO;
	}

	dev_info(plcd->dev, "%s: at=%d, lx=%d, md=%d, ps=%d\n", __func__,
			plcd->dblc_auto_br, plcd->dblc_lux, plcd->dblc_mode,
			plcd->dblc_power_save);

	if (plcd->dblc_power_save) {
		switch (plcd->dblc_mode) {
		case TCON_MODE_VIDEO:
		case TCON_MODE_VIDEO_WARM:
		case TCON_MODE_VIDEO_COLD:
			tune_value = &TCON_VIDEO;
			break;
		default:
			tune_value = &TCON_POWER_SAVE;
			break;
		}
	} else {
		tune_value = tcon_tune_value[plcd->dblc_auto_br][plcd->dblc_lux][plcd->dblc_mode];
	}

	if (!tune_value) {
		dev_err(plcd->dev, "%s: tcon value is null\n", __func__);
		return -EINVAL;
	}

	for(loop = 0; loop < tune_value->reg_cnt; loop++) {
		lsl122dl01_i2c_write(plcd->client,
			  tune_value->addr[loop], tune_value->data[loop]);
	}
	
	if (plcd->dblc_duty <= MN_BL)
		tcon_set_under_mnbl_duty(plcd, plcd->dblc_duty);

	//Enable double bufferd regiset
	lsl122dl01_i2c_write(plcd->client, 0x0F10, 0x80);

	return 0;
}

static ssize_t tcon_mode_store(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
  	struct lsl122dl01 *plcd = dev_get_drvdata(dev);
	int ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);

	if (ret)
		return ret;

	dev_info(plcd->dev, "%s: value = %d\n\n", __func__, value);

	if (value >= TCON_MODE_MAX) {
		dev_err(plcd->dev, "undef tcon mode value : %d\n\n", value);
		return count;
	}

	mutex_lock(&plcd->ops_lock);
	if (value != plcd->dblc_mode) {
		plcd->dblc_mode = value;
		ret = tcon_tune_write(plcd);

		if (ret)
			dev_err(plcd->dev, "failed to tune tcon\n");
	}
	mutex_unlock(&plcd->ops_lock);

	return count;
}

static ssize_t tcon_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lsl122dl01 *plcd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", plcd->dblc_mode);
}

static ssize_t tcon_lux_store(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
  	struct lsl122dl01 *plcd = dev_get_drvdata(dev);
	int ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);

	if (ret)
		return ret;

	dev_info(plcd->dev, "%s: value = %d\n\n", __func__, value);

	if (value >= TCON_LEVEL_MAX) {
		dev_err(plcd->dev, "undef tcon illumiate value : %d\n\n", value);
		return count;
	}

	mutex_lock(&plcd->ops_lock);
	if (value != plcd->dblc_lux) {
		plcd->dblc_lux = value;
		ret = tcon_tune_write(plcd);

		if (ret)
			dev_err(plcd->dev, "failed to tune tcon\n");
	}
	mutex_unlock(&plcd->ops_lock);

	return count;
}


static ssize_t tcon_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lsl122dl01 *plcd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", plcd->dblc_lux);
}

static ssize_t tcon_auto_br_store(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
  	struct lsl122dl01 *plcd = dev_get_drvdata(dev);
	int ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);

	if (ret)
		return ret;

	dev_info(plcd->dev, "%s: value = %d\n\n", __func__, value);

	if (value >= TCON_LEVEL_MAX) {
		dev_err(plcd->dev, "undef tcon auto br value : %d\n", value);
		return count;
	}

	mutex_lock(&plcd->ops_lock);
	if (value != plcd->dblc_auto_br) {
		plcd->dblc_auto_br = value;
		ret = tcon_tune_write(plcd);

		if (ret)
			dev_err(plcd->dev, "failed to tune tcon\n");
	}
	mutex_unlock(&plcd->ops_lock);

	return count;
}


static ssize_t tcon_auto_br_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lsl122dl01 *plcd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", plcd->dblc_auto_br);
}

static ssize_t tcon_power_save_store(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
  	struct lsl122dl01 *plcd = dev_get_drvdata(dev);
	int ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);

	if (ret)
		return ret;

	dev_info(plcd->dev, "%s: value = %d\n\n", __func__, value);

	if (value > 1) {
		dev_err(plcd->dev, "undef tcon dblc_power_save value : %d\n\n", value);
		return count;
	}

	mutex_lock(&plcd->ops_lock);
	if (value != plcd->dblc_power_save) {
		plcd->dblc_power_save = value;

		ret = tcon_tune_write(plcd);
		if (ret)
			dev_err(plcd->dev, "failed to tune tcon\n");
	}
	mutex_unlock(&plcd->ops_lock);

	return count;
}

static ssize_t tcon_power_save_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lsl122dl01 *plcd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", plcd->dblc_power_save);
}

static ssize_t tcon_black_test_store(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
  	struct lsl122dl01 *plcd = dev_get_drvdata(dev);
	int ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);

	if (ret)
		return ret;

	dev_info(plcd->dev, "%s: value = %d\n\n", __func__, value);

	if (value > 1) {
		dev_err(plcd->dev, "undef tcon black_test value : %d\n\n", value);
		return count;
	}

	mutex_lock(&plcd->ops_lock);
	
	if (value)
		ret = tcon_black_frame_bl_on(plcd);
	else
		ret = tcon_tune_write(plcd);

	if (ret)
		dev_err(plcd->dev, "failed to tune tcon\n");

	mutex_unlock(&plcd->ops_lock);

	return count;
}

static struct device_attribute tcon_device_attributes[] = {
	__ATTR(mode, 0664, tcon_mode_show, tcon_mode_store),
	__ATTR(lux, 0664, tcon_lux_show, tcon_lux_store),
	__ATTR(auto_br, 0664, tcon_auto_br_show, tcon_auto_br_store),
	__ATTR(power_save, 0664, tcon_power_save_show, tcon_power_save_store),
	__ATTR(black_test, 0664, NULL, tcon_black_test_store),
	__ATTR_NULL,
};

static int tcon_bl_update_status(struct backlight_device *bl)
{
	struct lsl122dl01 *plcd = bl_get_data(bl);
	unsigned int bl_fbstate;
	int ret;
	
	bl_fbstate = bl->props.state & BL_CORE_FBBLANK;
	
	if (bl_fbstate) {
		plcd->i2c_slave = 0;
		return 0;
	} else if (!plcd->i2c_slave && !bl_fbstate) {
		ret = tcon_i2c_slave_enable(plcd);
		if (ret < 0) {
			dev_err(plcd->dev,
				"failed to set tcon_i2c_slave_enable!\n");
			return ret;
		}
		plcd->i2c_slave = 1;
		dev_info(plcd->dev, "%s tcon set slave\n", __func__);
	}

	mutex_lock(&plcd->ops_lock);

	ret = tcon_tune_write(plcd);
	if (ret)
		dev_err(plcd->dev, "failed to tune tcon\n");

	mutex_unlock(&plcd->ops_lock);

	return ret;
}

static const struct backlight_ops tcon_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = tcon_bl_update_status,
};

static int tcon_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct lsl122dl01 *plcd;
	unsigned int *value;
	int ret;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != SECFB_EVENT_CABC_READ &&
	    event != SECFB_EVENT_CABC_WRITE &&
	    event != SECFB_EVENT_BL_UPDATE)
		return 0;

	plcd = container_of(self, struct lsl122dl01, secfb_notif);

	mutex_lock(&plcd->ops_lock);

	switch(event) {
	case SECFB_EVENT_CABC_WRITE:
		value = (unsigned int*) data;
		
		if (*value == plcd->dblc_power_save)
			break;

		plcd->dblc_power_save = *value;
		ret = tcon_tune_write(plcd);
		if (ret)
			dev_err(plcd->dev, "failed to tune tcon\n");
	
		break;

	case SECFB_EVENT_CABC_READ:
		value = (unsigned int*) data;

		*value = plcd->dblc_power_save;

		break;

	case SECFB_EVENT_BL_UPDATE:
		value = (unsigned int*) data;

		dev_dbg(plcd->dev," SECFB_EVENT_BL_UPDATE  duty = %d\n", *value);
		
		if (*value) {
			int curr_bl = (*value <= MN_BL) ? 1 : 0;
			int pre_bl = (plcd->dblc_duty <= MN_BL) ? 1 : 0;

			if (pre_bl != curr_bl)
				tcon_set_under_mnbl_duty(plcd, *value);
		}
		plcd->dblc_duty = *value;
		break;

	default:
		dev_err(plcd->dev, "invalid event\n");
		break;
	}

	mutex_unlock(&plcd->ops_lock);

	return 0;
}

static int lsl122dl01_probe(struct i2c_client *cl,
			    const struct i2c_device_id *id)
{
	struct lsl122dl01 *plcd;
	struct platform_device * pdev_dp;
	struct backlight_device *bl;
	struct backlight_properties props;
	int ret;

	dev_dbg(&cl->dev, "%s\n", __func__);

	if (!cl->dev.platform_data) {
		dev_err(&cl->dev, "no platform data supplied\n");
		return -EINVAL;
	}

	pdev_dp = to_platform_device(cl->dev.platform_data);
	if (!pdev_dp->dev.driver) {
		dev_err(&cl->dev, "no DP driver supplied\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	plcd = devm_kzalloc(&cl->dev, sizeof(struct lsl122dl01), GFP_KERNEL);
	if (!plcd)
		return -ENOMEM;

	mutex_init(&plcd->ops_lock);

	plcd->client = cl;
	plcd->dev = &cl->dev;
	plcd->dp = platform_get_drvdata(pdev_dp);

	plcd->lcd = lcd_device_register(LCD_NAME, &cl->dev,
					plcd, &lsl122dl01_lcd_ops);

	if (IS_ERR(plcd->lcd)) {
		dev_err(&cl->dev, "cannot register lcd device\n");
		ret = PTR_ERR(plcd->lcd);
		goto err_lcddev;
	}

	props.type = BACKLIGHT_PLATFORM;
	plcd->bl = backlight_device_register(BL_NAME, &cl->dev, plcd,
				       &tcon_bl_ops, &props);
	if (IS_ERR(plcd->bl)) {
		dev_err(&cl->dev, "cannot register backlight device\n");
		ret = PTR_ERR(plcd->bl);
		goto err_bldev;
	}

	plcd->tcon_dev = device_create(tcon_class, NULL, 0, plcd, BL_NAME);

	if (IS_ERR(plcd->tcon_dev)) {
		pr_err("%s:%s= Failed to create device(tcon)!\n",
				__FILE__, __func__);
		ret = PTR_ERR(plcd->tcon_dev);
		goto err_cdev;
	}

	ret = sysfs_create_group(&plcd->lcd->dev.kobj, &lcd_attr_group);
	if (ret < 0) {
		dev_err(&cl->dev, "Sysfs registration failed\n");
		goto err_sysfs;
	}

#ifdef CONFIG_FB_S3C
	memset(&plcd->secfb_notif, 0, sizeof(plcd->secfb_notif));
	plcd->secfb_notif.notifier_call = tcon_notifier_callback;
	secfb_register_client(&plcd->secfb_notif);
#endif

	i2c_set_clientdata(cl, plcd);

	backlight_update_status(plcd->bl);
	
	return 0;

err_sysfs:
	device_unregister(plcd->tcon_dev);
err_cdev:
	backlight_device_unregister(plcd->bl);
err_bldev:
	lcd_device_unregister(plcd->lcd);	
err_lcddev:
	return ret;
}

static int __devexit lsl122dl01_remove(struct i2c_client *cl)
{
	struct lsl122dl01 *plcd = i2c_get_clientdata(cl);

#ifdef CONFIG_FB_S3C
	secfb_unregister_client(&plcd->secfb_notif);
#endif
	sysfs_remove_group(&plcd->lcd->dev.kobj, &lcd_attr_group);
	lcd_device_unregister(plcd->lcd);
	backlight_device_unregister(plcd->bl);
	device_unregister(plcd->tcon_dev);
	return 0;
}

static const struct i2c_device_id lsl122dl01_ids[] = {
	{"lsl122dl01", 0},
	{ }
};

static struct i2c_driver lsl122dl01_driver = {
	.driver = {
		.name = "lsl122dl01",
		.owner		= THIS_MODULE,
	},
	.probe = lsl122dl01_probe,
	.remove = __devexit_p(lsl122dl01_remove),
	.id_table = lsl122dl01_ids,
};

static int __init lsl122dl01_init(void)
{
	int retval;

	tcon_class = class_create(THIS_MODULE, BL_NAME);
	if (IS_ERR(tcon_class)) {
		printk(KERN_WARNING "Unable to create tcon class; errno = %ld\n",
				PTR_ERR(tcon_class));
		return PTR_ERR(tcon_class);
	}

	tcon_class->dev_attrs = tcon_device_attributes;

	retval = i2c_add_driver(&lsl122dl01_driver);
	if (retval) {
	  	class_destroy(tcon_class);
		printk(KERN_INFO "%s: failed registering lsl122dl01\n",
		       __func__);
	}

	return retval;
}

static void __exit lsl122dl01_exit(void)
{
	i2c_del_driver(&lsl122dl01_driver);
	class_destroy(tcon_class);
}

late_initcall_sync(lsl122dl01_init);
module_exit(lsl122dl01_exit);

MODULE_DESCRIPTION("LSL122DL01 LCD T-CON Driver");
MODULE_LICENSE("GPL");
