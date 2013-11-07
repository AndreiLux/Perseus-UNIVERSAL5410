/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include "issp_extern.h"
#include <linux/i2c/touchkey_i2c.h>

static unsigned int qos_cpu_freq = 600000;
static unsigned int qos_mif_freq = 800000;
static unsigned int qos_mif2_freq = 400000;
static unsigned int qos_int_freq = 200000;

module_param(qos_cpu_freq, uint, S_IWUSR | S_IRUGO);
module_param(qos_mif_freq, uint, S_IWUSR | S_IRUGO);
module_param(qos_mif2_freq, uint, S_IWUSR | S_IRUGO);
module_param(qos_int_freq, uint, S_IWUSR | S_IRUGO);

static int touchkey_keycode[] = { 0,
#if defined(TK_USE_4KEY_TYPE_ATT)
	KEY_MENU, KEY_HOMEPAGE, KEY_BACK, KEY_SEARCH,

#elif defined(TK_USE_4KEY_TYPE_NA)
	KEY_SEARCH, KEY_BACK, KEY_HOMEPAGE, KEY_MENU,

#elif defined(TK_USE_2KEY_TYPE_M0)
	KEY_BACK, KEY_MENU,

#else
	KEY_MENU, KEY_BACK,

#endif
};
static const int touchkey_count = sizeof(touchkey_keycode) / sizeof(int);

#if defined(TK_HAS_AUTOCAL)
static u16 raw_data0;
static u16 raw_data1;
static u16 raw_data2;
static u16 raw_data3;
static u8 idac0;
static u8 idac1;
static u8 idac2;
static u8 idac3;
static u8 touchkey_threshold;

static int touchkey_autocalibration(struct touchkey_i2c *tkey_i2c);
#endif

static int touchkey_i2c_check(struct touchkey_i2c *tkey_i2c);

static u16 menu_sensitivity;
static u16 back_sensitivity;
#if defined(TK_USE_4KEY)
static u8 home_sensitivity;
static u8 search_sensitivity;
#endif

static bool touchkey_probe;

static const struct i2c_device_id sec_touchkey_id[] = {
	{"sec_touchkey", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_touchkey_id);

extern int get_touchkey_firmware(char *version);
static int touchkey_led_status;
static int touchled_cmd_reversed;

#ifdef LED_LDO_WITH_REGULATOR
static void change_touch_key_led_voltage(int vol_mv)
{
	struct regulator *tled_regulator;

	tled_regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(tled_regulator)) {
		pr_err("%s: failed to get resource %s\n", __func__,
		       "touchkey_led");
		return;
	}
	regulator_set_voltage(tled_regulator, vol_mv * 1000, vol_mv * 1000);
	regulator_put(tled_regulator);
}

static ssize_t brightness_control(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int data;

	if (sscanf(buf, "%d\n", &data) == 1) {
		dev_err(dev, "%s: %d\n", __func__, data);
		change_touch_key_led_voltage(data);
	} else {
		dev_err(dev, "%s Error\n", __func__);
	}

	return size;
}
#endif

static int i2c_touchkey_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	int err = 0;
	int retry = 3;
#if !defined(TK_USE_GENERAL_SMBUS)
	struct i2c_msg msg[1];
#endif
	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if ((client == NULL) || !(tkey_i2c->enabled)) {
		dev_err(&client->dev, "Touchkey is not enabled. %d\n",
		       __LINE__);
		return -ENODEV;
	}

	while (retry--) {
#if defined(TK_USE_GENERAL_SMBUS)
		err = i2c_smbus_read_i2c_block_data(client,
				KEYCODE_REG, len, val);
#else
		msg->addr = client->addr;
		msg->flags = I2C_M_RD;
		msg->len = len;
		msg->buf = val;
		err = i2c_transfer(client->adapter, msg, 1);
#endif

		if (err >= 0)
			return 0;
		dev_err(&client->dev, "%s %d i2c transfer error\n",
		       __func__, __LINE__);
		mdelay(10);
	}
	return err;

}

static int i2c_touchkey_write(struct i2c_client *client,
		u8 *val, unsigned int len)
{
	int err = 0;
	int retry = 3;
#if !defined(TK_USE_GENERAL_SMBUS)
	struct i2c_msg msg[1];
#endif
	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if ((client == NULL) || !(tkey_i2c->enabled)) {
		dev_err(&client->dev, "Touchkey is not enabled. %d\n",
		       __LINE__);
		return -ENODEV;
	}

	while (retry--) {
#if defined(TK_USE_GENERAL_SMBUS)
		err = i2c_smbus_write_i2c_block_data(client,
				KEYCODE_REG, len, val);
#else
		msg->addr = client->addr;
		msg->flags = I2C_M_WR;
		msg->len = len;
		msg->buf = val;
		err = i2c_transfer(client->adapter, msg, 1);
#endif

		if (err >= 0)
			return 0;

		dev_err(&client->dev, "%s %d i2c transfer error\n",
		       __func__, __LINE__);
		mdelay(10);
	}
	return err;
}

#if defined(TK_HAS_AUTOCAL)
static int touchkey_autocalibration(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times.\n",
				__func__, retry);
			return ret;
		}
		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Send autocal Command */
		data[0] = 0x50;
		data[3] = 0x01;

		count = i2c_touchkey_write(tkey_i2c->client, data, 4);

		msleep(130);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);

		if (data[5] & TK_BIT_AUTOCAL) {
			dev_dbg(&tkey_i2c->client->dev, "%s: Run Autocal\n", __func__);
			break;
		} else {
			dev_err(&tkey_i2c->client->dev,	"%s: Error to set Autocal, retry %d\n",
			       __func__, retry);
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to Set the Autocalibration\n", __func__);

	return count;
}
#endif

#if defined(TK_INFORM_CHARGER)
static int touchkey_ta_setting(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times.\n",
				__func__, retry);
			return ret;
		}
		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Send autocal Command */

		if (tkey_i2c->charging_mode) {
			dev_info(&tkey_i2c->client->dev, "%s: TA connected!!!\n", __func__);
			data[0] = 0x90;
			data[3] = 0x10;
		} else {
			dev_info(&tkey_i2c->client->dev, "%s: TA disconnected!!!\n", __func__);
			data[0] = 0x90;
			data[3] = 0x20;
		}

		count = i2c_touchkey_write(tkey_i2c->client, data, 4);

		msleep(100);
		dev_dbg(&tkey_i2c->client->dev,
				"write data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);

		if (tkey_i2c->charging_mode) {
			if (data[5] & TK_BIT_TA_ON) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Enabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to enable TA mode, retry %d\n",
					__func__, retry);
			}
		} else {
			if (!(data[5] & TK_BIT_TA_ON)) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Disabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to disable TA mode, retry %d\n",
					__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the TA mode\n", __func__);

	return count;

}

static void touchkey_ta_cb(struct touchkey_callbacks *cb, bool ta_status)
{
	struct touchkey_i2c *tkey_i2c =
			container_of(cb, struct touchkey_i2c, callbacks);
	struct i2c_client *client = tkey_i2c->client;

	tkey_i2c->charging_mode = ta_status;

	if (tkey_i2c->enabled)
		touchkey_ta_setting(tkey_i2c);
}
#endif
#if defined(CONFIG_GLOVE_TOUCH)
static void touchkey_glove_change_work(struct work_struct *work)
{
	u8 data[6] = { 0, };
	int ret = 0;
	unsigned short retry = 0;
	bool value;

	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c,
			glove_change_work.work);

#ifdef TKEY_FLIP_MODE
	if (tkey_i2c->enabled_flip) {
		dev_info(&tkey_i2c->client->dev,"As flip cover mode enabled, skip glove mode set\n");
		return;
	}
#endif

	mutex_lock(&tkey_i2c->tsk_glove_lock);
	value = tkey_i2c->tsk_glove_mode_status;
	mutex_unlock(&tkey_i2c->tsk_glove_lock);

	if (!tkey_i2c->enabled)
		return;

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times.\n",
				__func__, retry);
			return;
		}

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		if (value) {
			/* Send glove Command */
				data[0] = 0xA0;
				data[3] = 0x30;
		} else {
				data[0] = 0xA0;
				data[3] = 0x40;
		}

		i2c_touchkey_write(tkey_i2c->client, data, 4);

		msleep(50);

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);

		if (value) {
			if (data[5] & TK_BIT_GLOVE) {
				dev_dbg(&tkey_i2c->client->dev, "%s: Glove mode is enabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to enable glove_mode, retry %d\n",
					__func__, retry);
			}
		} else {
			if (!(data[5] & TK_BIT_GLOVE)) {
				dev_dbg(&tkey_i2c->client->dev, "%s: Normal mode from Glove mode\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to disable glove mode, retry %d \n",
					__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the glove mode\n", __func__);
}

static struct touchkey_i2c *tkey_i2c_global;

void touchkey_glovemode(int value)
{
	struct touchkey_i2c *tkey_i2c = tkey_i2c_global;

	if (!touchkey_probe) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}

	mutex_lock(&tkey_i2c->tsk_glove_lock);
	cancel_delayed_work(&tkey_i2c->glove_change_work);

	if (value == 0) {
		tkey_i2c->tsk_glove_mode_status = false;
		if (tkey_i2c->tsk_glove_lock_status) {
			schedule_delayed_work(&tkey_i2c->glove_change_work,
				msecs_to_jiffies(TK_GLOVE_DWORK_TIME));
			tkey_i2c->tsk_glove_lock_status = false;
			dev_info(&tkey_i2c->client->dev, "Touchkey glove Off\n");
		}
	} else if (value == 1) {
		tkey_i2c->tsk_glove_mode_status = true;
		if (!tkey_i2c->tsk_glove_lock_status) {
			schedule_delayed_work(&tkey_i2c->glove_change_work,
				msecs_to_jiffies(TK_GLOVE_DWORK_TIME));
			tkey_i2c->tsk_glove_lock_status = true;
			dev_info(&tkey_i2c->client->dev, "Touchkey glove On\n");
		}
	}
	mutex_unlock(&tkey_i2c->tsk_glove_lock);
}
#endif

#ifdef TKEY_FLIP_MODE
void touchkey_flip_cover(int value)
{
	struct touchkey_i2c *tkey_i2c = tkey_i2c_global;
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	if (!touchkey_probe) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}

	if (!tkey_i2c->enabled)
		return;

	if (tkey_i2c->firmware_id != TK_MODULE_20065) {
		dev_err(&tkey_i2c->client->dev,
				"%s: Do not support old module\n",
				__func__);
		return;
	}

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times.\n",
				__func__, retry);
			return;
		}

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		if (value == 1) {
			/* Send filp mode Command */
				data[0] = 0xA0;
				data[3] = 0x50;
		} else {
				data[0] = 0xA0;
				data[3] = 0x40;
		}

		i2c_touchkey_write(tkey_i2c->client, data, 4);

		msleep(100);

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);

		dev_dbg(&tkey_i2c->client->dev,
				"data[5]=%x",data[5] & TK_BIT_FLIP);

		if (value) {
			if (data[5] & TK_BIT_FLIP) {
				dev_dbg(&tkey_i2c->client->dev, "%s: Flip mode is enabled\n", __func__);
				tkey_i2c->enabled_flip = true;
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to enable Flip mode, retry %d\n",
					__func__, retry);
			}
		} else {
			if (!(data[5] & TK_BIT_FLIP)) {
				dev_dbg(&tkey_i2c->client->dev, "%s: Normal mode form Flip mode\n", __func__);
				tkey_i2c->enabled_flip = false;
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to disable Flip mode, retry %d \n",
					__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the Flip mode\n", __func__);

	return;
}
#endif

#if defined(TK_HAS_AUTOCAL)
static ssize_t touchkey_raw_data0_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[26] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 26);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[14] =%d,data[15] = %d\n",
			__func__, data[14], data[15]);
	raw_data0 = ((0x00FF & data[14]) << 8) | data[15]; /* menu*/

	return sprintf(buf, "%d\n", raw_data0);
}

static ssize_t touchkey_raw_data1_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[26] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 26);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[16] =%d,data[17] = %d\n", __func__,
	       data[16], data[17]);
	raw_data1 = ((0x00FF & data[16]) << 8) | data[17]; /*back*/

	return sprintf(buf, "%d\n", raw_data1);
}

static ssize_t touchkey_raw_data2_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[26] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 26);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[22] =%d,data[23] = %d\n",
		__func__, data[14], data[15]);
	raw_data2 = ((0x00FF & data[14]) << 8) | data[15];

	return sprintf(buf, "%d\n", raw_data2);
}

static ssize_t touchkey_raw_data3_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[26] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 26);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[24] =%d,data[25] = %d\n",
			__func__, data[16], data[17]);
	raw_data3 = ((0x00FF & data[16]) << 8) | data[17];

	return sprintf(buf, "%d\n", raw_data3);
}

static ssize_t touchkey_idac0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[10];
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 10);
	dev_dbg(&tkey_i2c->client->dev, "called %s data[6] =%d\n", __func__, data[6]);
	idac0 = data[6];
	return sprintf(buf, "%d\n", idac0);
}

static ssize_t touchkey_idac1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[10];
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 10);
	dev_dbg(&tkey_i2c->client->dev, "called %s data[7] = %d\n", __func__, data[7]);
	idac1 = data[7];
	return sprintf(buf, "%d\n", idac1);
}

static ssize_t touchkey_idac2_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[10];
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 10);
	dev_dbg(&tkey_i2c->client->dev, "called %s data[8] =%d\n", __func__, data[8]);
	idac2 = data[8];
	return sprintf(buf, "%d\n", idac2);
}

static ssize_t touchkey_idac3_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[10];
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 10);
	dev_dbg(&tkey_i2c->client->dev, "called %s data[9] = %d\n", __func__, data[9]);
	idac3 = data[9];
	return sprintf(buf, "%d\n", idac3);
}

static ssize_t touchkey_threshold_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[10];
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 10);
	dev_dbg(&tkey_i2c->client->dev, "called %s data[4] = %d\n", __func__, data[4]);
	touchkey_threshold = data[4];
	return sprintf(buf, "%d\n", touchkey_threshold);
}
#endif

#ifdef TOUCHKEY_BOOSTER
static void touchkey_change_dvfs_lock(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
			container_of(work,
				struct touchkey_i2c, tsk_work_dvfs_chg.work);

	mutex_lock(&tkey_i2c->tsk_dvfs_lock);

	if (pm_qos_request_active(&tkey_i2c->tsk_mif_qos)) {
		pm_qos_update_request(&tkey_i2c->tsk_mif_qos, qos_mif2_freq); /* MIF 400MHz */
		dev_dbg(&tkey_i2c->client->dev, "change_mif_dvfs_lock");
	}

	mutex_unlock(&tkey_i2c->tsk_dvfs_lock);
}

static void touchkey_set_dvfs_off(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
				container_of(work,
					struct touchkey_i2c, tsk_work_dvfs_off.work);

	mutex_lock(&tkey_i2c->tsk_dvfs_lock);

	pm_qos_remove_request(&tkey_i2c->tsk_cpu_qos);
	pm_qos_remove_request(&tkey_i2c->tsk_mif_qos);
	pm_qos_remove_request(&tkey_i2c->tsk_int_qos);

	tkey_i2c->tsk_dvfs_lock_status = false;
	mutex_unlock(&tkey_i2c->tsk_dvfs_lock);

	dev_dbg(&tkey_i2c->client->dev, "TSP DVFS Off\n");
}

static void touchkey_set_dvfs_lock(struct touchkey_i2c *tkey_i2c,
					uint32_t on)
{
	mutex_lock(&tkey_i2c->tsk_dvfs_lock);
	if (on == 0) {
		if (tkey_i2c->tsk_dvfs_lock_status) {
			schedule_delayed_work(&tkey_i2c->tsk_work_dvfs_off,
				msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
		}
	} else if (on == 1) {
		cancel_delayed_work(&tkey_i2c->tsk_work_dvfs_off);
		if (!tkey_i2c->tsk_dvfs_lock_status) {
			pm_qos_add_request(&tkey_i2c->tsk_cpu_qos, PM_QOS_CPU_FREQ_MIN, qos_cpu_freq); /* CPU KFC 1.2GHz */
			pm_qos_add_request(&tkey_i2c->tsk_mif_qos, PM_QOS_BUS_THROUGHPUT, qos_mif_freq); /* MIF 800MHz */
			pm_qos_add_request(&tkey_i2c->tsk_int_qos, PM_QOS_DEVICE_THROUGHPUT, qos_int_freq); /* INT 200MHz */

			schedule_delayed_work(&tkey_i2c->tsk_work_dvfs_chg,
							msecs_to_jiffies(TOUCH_BOOSTER_CHG_TIME));

			tkey_i2c->tsk_dvfs_lock_status = true;
			dev_dbg(&tkey_i2c->client->dev, "Touchkey DVFS On\n");
		}
	} else if (on == 2) {
		if (tkey_i2c->tsk_dvfs_lock_status) {
			cancel_delayed_work(&tkey_i2c->tsk_work_dvfs_off);
			cancel_delayed_work(&tkey_i2c->tsk_work_dvfs_chg);
			schedule_work(&tkey_i2c->tsk_work_dvfs_off.work);
		}
	}
	mutex_unlock(&tkey_i2c->tsk_dvfs_lock);
}


static int touchkey_init_dvfs(struct touchkey_i2c *tkey_i2c)
{
	mutex_init(&tkey_i2c->tsk_dvfs_lock);

	INIT_DELAYED_WORK(&tkey_i2c->tsk_work_dvfs_off, touchkey_set_dvfs_off);
	INIT_DELAYED_WORK(&tkey_i2c->tsk_work_dvfs_chg, touchkey_change_dvfs_lock);

	tkey_i2c->tsk_dvfs_lock_status = false;
	return 0;
}
#endif

#if defined(TK_HAS_FIRMWARE_UPDATE)
static int touchkey_firmware_version_check(struct touchkey_i2c *tkey_i2c)
{
	int ret;
	char data[6];

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev,	"i2c read fail. firm update retry 1.\n");
		if(ISSP_main(tkey_i2c) == 0) {
			ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);
			if (ret < 0) {
		dev_err(&tkey_i2c->client->dev,	"i2c read fail. do not excute firm update.\n");
		data[1] = 0;
		data[2] = 0;
		return ret;
			}else{
				dev_dbg(&tkey_i2c->client->dev, "pass firm update.\n");
			}
		}else{
			dev_err(&tkey_i2c->client->dev, "fail firm update.\n");
			return ret;
		}
	}
	dev_info(&tkey_i2c->client->dev, "%s F/W version: 0x%x, Module version:0x%x\n",
			__func__, data[1], data[2]);

	tkey_i2c->firmware_ver = data[1];
	tkey_i2c->module_ver = data[2];

	if ((data[5] & TK_BIT_FW_ID_65)) {
		dev_info(&tkey_i2c->client->dev, "Firmware id 20065\n");
		tkey_i2c->firmware_id = TK_MODULE_20065;
	} else if ((data[5] & TK_BIT_FW_ID_55)) {
		dev_info(&tkey_i2c->client->dev, "Firmware id 20055\n");
		tkey_i2c->firmware_id = TK_MODULE_20055;
	} else {
		dev_info(&tkey_i2c->client->dev, "Firmware id 20045\n");
		tkey_i2c->firmware_id = TK_MODULE_20045;
	}

	return ret;
}

static int touchkey_firmware_update(struct touchkey_i2c *tkey_i2c)
{
	int ret;

	disable_irq(tkey_i2c->irq);

	ret = touchkey_firmware_version_check(tkey_i2c);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "i2c read fail.\n");
		enable_irq(tkey_i2c->irq);
		return TK_UPDATE_FAIL;
	}

#ifdef TKEY_FW_FORCEUPDATE
	if (tkey_i2c->firmware_ver != TK_FIRMWARE_VER_65 ) {
#else
	if (tkey_i2c->firmware_ver < TK_FIRMWARE_VER_65 && \
			tkey_i2c->firmware_id == TK_MODULE_20065) {
#endif
		int retry = 3;
		dev_info(&tkey_i2c->client->dev, "Firmware auto update excute\n");

		tkey_i2c->update_status = TK_UPDATE_DOWN;

		while (retry--) {
			if (ISSP_main(tkey_i2c) == 0) {
				dev_info(&tkey_i2c->client->dev, "Firmware update succeeded\n");
				tkey_i2c->update_status = TK_UPDATE_PASS;
				msleep(50);
				break;
			}
			msleep(50);
			dev_err(&tkey_i2c->client->dev, "Firmware update failed. retry\n");
		}
		if (retry <= 0) {
			tkey_i2c->pdata->power_on(0);
			tkey_i2c->update_status = TK_UPDATE_FAIL;
			dev_err(&tkey_i2c->client->dev, "Firmware update failed.\n");
		}
		ret = touchkey_i2c_check(tkey_i2c);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "i2c read fail.\n");
			return TK_UPDATE_FAIL;
		}
		dev_info(&tkey_i2c->client->dev, "Firm ver = %d, module ver = %d\n",
			tkey_i2c->firmware_ver, tkey_i2c->module_ver);
	} else {
		dev_info(&tkey_i2c->client->dev, "Firmware auto update do not excute\n");
		if (tkey_i2c->firmware_id == TK_MODULE_20065) {
			dev_info(&tkey_i2c->client->dev, "firmware_ver(banary=%d, current=%d)\n",
				   TK_FIRMWARE_VER_65, tkey_i2c->firmware_ver);
		} else if (tkey_i2c->firmware_id == TK_MODULE_20055) {
			dev_info(&tkey_i2c->client->dev, "firmware_ver(banary=%d, current=%d)\n",
				   TK_FIRMWARE_VER_55, tkey_i2c->firmware_ver);
		} else {
			dev_info(&tkey_i2c->client->dev, "firmware_ver(banary=%d, current=%d)\n",
				   TK_FIRMWARE_VER_45, tkey_i2c->firmware_ver);
		}

		dev_info(&tkey_i2c->client->dev, "module_ver(banary=%d, current=%d)\n",
		       TK_MODULE_VER, tkey_i2c->module_ver);
	}
	enable_irq(tkey_i2c->irq);
	return TK_UPDATE_PASS;
}
#endif

#ifndef TEST_JIG_MODE

static int mdnie_shortcut_enabled = 0;
module_param_named(mdnie_shortcut_enabled, mdnie_shortcut_enabled, int, S_IRUGO | S_IWUSR | S_IWGRP);

static inline int64_t get_time_inms(void) {
	int64_t tinms;
	struct timespec cur_time = current_kernel_time();
	tinms =  cur_time.tv_sec * MSEC_PER_SEC;
	tinms += cur_time.tv_nsec / NSEC_PER_MSEC;
	return tinms;
}

extern void mdnie_toggle_negative(void);
#define KEY_TRG_CNT 4
#define KEY_TRG_MS  300

static irqreturn_t touchkey_interrupt(int irq, void *dev_id)
{
	struct touchkey_i2c *tkey_i2c = dev_id;
	static int64_t trigger_lasttime = 0;
	static int trigger_count = -1;
	u8 data[3];
	int ret;
	int retry = 10;
	int keycode_type = 0;
	int pressed;

	retry = 3;
	while (retry--) {
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 3);
		if (!ret)
			break;
		else {
		dev_dbg(&tkey_i2c->client->dev, "I2c read failed, ret:%d, retry: %d\n",
			       ret, retry);
			continue;
		}
	}
	if (ret < 0)
		return IRQ_HANDLED;

	keycode_type = (data[0] & TK_BIT_KEYCODE);
	pressed = !(data[0] & TK_BIT_PRESS_EV);

	if (keycode_type <= 0 || keycode_type >= touchkey_count) {
		dev_dbg(&tkey_i2c->client->dev, "keycode_type err\n");
		return IRQ_HANDLED;
	}

	if ((touchkey_keycode[keycode_type] == KEY_MENU) && 
		pressed && mdnie_shortcut_enabled)
	{
		if ((get_time_inms() - trigger_lasttime) < KEY_TRG_MS) {
			if (++trigger_count >= KEY_TRG_CNT - 1) {
				mdnie_toggle_negative();
				trigger_count = 0;
			}
		} else {
			trigger_count = 0;
		}
		
		trigger_lasttime = get_time_inms();
	}

	input_report_key(tkey_i2c->input_dev,
			 touchkey_keycode[keycode_type], pressed);
	input_sync(tkey_i2c->input_dev);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_info(&tkey_i2c->client->dev, "keycode:%d pressed:%d %d\n",
	touchkey_keycode[keycode_type], pressed, tkey_i2c->tsk_glove_mode_status);
#else
	dev_dbg(&tkey_i2c->client->dev, "pressed:%d %d\n",
		pressed, tkey_i2c->tsk_glove_mode_status);
#endif
#ifdef TOUCHKEY_BOOSTER
	touchkey_set_dvfs_lock(tkey_i2c, !!pressed);
#endif
	return IRQ_HANDLED;
}
#else
static irqreturn_t touchkey_interrupt(int irq, void *dev_id)
{
	struct touchkey_i2c *tkey_i2c = dev_id;
	u8 data[18];
	int ret;
	int retry = 10;
	int keycode_type = 0;
	int pressed;

	retry = 3;
	while (retry--) {
		ret = i2c_touchkey_read(tkey_i2c->client,
				KEYCODE_REG, data, 18);
		if (!ret)
			break;
		else {
			dev_dbg(&tkey_i2c->client->dev,
			       "%s I2c read failed, ret:%d, retry: %d\n",
			       __func__, ret, retry);
			continue;
		}
	}
	if (ret < 0)
		return IRQ_HANDLED;

	menu_sensitivity = data[13];
	back_sensitivity = data[11];

	keycode_type = (data[0] & TK_BIT_KEYCODE);
	pressed = !(data[0] & TK_BIT_PRESS_EV);

	if (keycode_type <= 0 || keycode_type >= touchkey_count) {
		dev_dbg(&tkey_i2c->client->dev, "keycode_type err\n");
		return IRQ_HANDLED;
	}

	input_report_key(tkey_i2c->input_dev,
			 touchkey_keycode[keycode_type], pressed);
	input_sync(tkey_i2c->input_dev);

	if (keycode_type == 1)
		dev_dbg(&tkey_i2c->client->dev, "search key sensitivity = %d\n",
		       search_sensitivity);
	if (keycode_type == 2)
		dev_dbg(&tkey_i2c->client->dev, "back key sensitivity = %d\n",
		       back_sensitivity);

	return IRQ_HANDLED;
}
#endif

static int touchkey_stop(struct touchkey_i2c *tkey_i2c)
{
	int i;

	mutex_lock(&tkey_i2c->lock);

	if (!tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already disabled\n");
		goto out;
	}

	disable_irq(tkey_i2c->irq);

	/* release keys */
	for (i = 1; i < touchkey_count; ++i) {
		input_report_key(tkey_i2c->input_dev,
				 touchkey_keycode[i], 0);
	}
	input_sync(tkey_i2c->input_dev);

	/* disable ldo18 */
	tkey_i2c->pdata->led_power_on(0);

	/* disable ldo11 */
	tkey_i2c->pdata->power_on(0);

	tkey_i2c->enabled = false;
#ifdef TOUCHKEY_BOOSTER
	touchkey_set_dvfs_lock(tkey_i2c, 2);
#endif
#if defined(CONFIG_GLOVE_TOUCH)
	cancel_delayed_work(&tkey_i2c->glove_change_work);
#endif
out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

static int touchkey_start(struct touchkey_i2c *tkey_i2c)
{
#ifdef TEST_JIG_MODE
	unsigned char get_touch = 0x40;
#endif

	mutex_lock(&tkey_i2c->lock);

	if (tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already enabled\n");
		goto out;
	}

	/* enable ldo11 */
	tkey_i2c->pdata->power_on(1);
	msleep(50);
	tkey_i2c->pdata->led_power_on(1);

	tkey_i2c->enabled = true;

#if defined(TK_HAS_AUTOCAL)
	touchkey_autocalibration(tkey_i2c);
#endif

	if (touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
		i2c_touchkey_write(tkey_i2c->client,
			(u8 *) &touchkey_led_status, 1);
		dev_err(&tkey_i2c->client->dev, "%s: Turning LED is reserved\n", __func__);
		msleep(30);
	}

#ifdef TEST_JIG_MODE
	i2c_touchkey_write(tkey_i2c->client, &get_touch, 1);
#endif

#if defined(TK_INFORM_CHARGER)
	touchkey_ta_setting(tkey_i2c);
#endif

#if defined(CONFIG_GLOVE_TOUCH)
	tkey_i2c->tsk_glove_lock_status = false;
	touchkey_glovemode(tkey_i2c->tsk_glove_mode_status);
#endif
	enable_irq(tkey_i2c->irq);
out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

#ifdef TK_USE_OPEN_DWORK
static void touchkey_open_work(struct work_struct *work)
{
	int retval;
	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c,
			open_work.work);

	if (tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already enabled\n");
		return;
	}

	retval = touchkey_start(tkey_i2c);
	if (retval < 0)
		dev_err(&tkey_i2c->client->dev,
				"%s: Failed to start device\n", __func__);
}
#endif

static int touchkey_input_open(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);
	int ret;

	ret = wait_for_completion_interruptible_timeout(&data->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));

	if (ret < 0) {
		dev_err(&data->client->dev,
			"error while waiting for device to init (%d)\n", ret);
		ret = -ENXIO;
		goto err_open;
	}
	if (ret == 0) {
		dev_err(&data->client->dev,
			"timedout while waiting for device to init\n");
		ret = -ENXIO;
		goto err_open;
	}
#ifdef TK_USE_OPEN_DWORK
	schedule_delayed_work(&data->open_work,
					msecs_to_jiffies(TK_OPEN_DWORK_TIME));
#else
	ret = touchkey_start(data);
	if (ret)
		goto err_open;
#endif

	dev_dbg(&data->client->dev, "%s\n", __func__);

	return 0;

err_open:
	return ret;
}

static void touchkey_input_close(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);

#ifdef TK_USE_OPEN_DWORK
	cancel_delayed_work(&data->open_work);
#endif
	touchkey_stop(data);

	dev_dbg(&data->client->dev, "%s\n", __func__);
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
#define touchkey_suspend	NULL
#define touchkey_resume	NULL

static int sec_touchkey_early_suspend(struct early_suspend *h)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(h, struct touchkey_i2c, early_suspend);

	touchkey_stop(tkey_i2c);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}

static int sec_touchkey_late_resume(struct early_suspend *h)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(h, struct touchkey_i2c, early_suspend);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	touchkey_start(tkey_i2c);

	return 0;
}
#else
static int touchkey_suspend(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (touchkey_probe != true) {
		printk(KERN_ERR "%s Touchkey is not enabled. \n",
		       __func__);
		return 0;
	}
	mutex_lock(&tkey_i2c->input_dev->mutex);

	if (tkey_i2c->input_dev->users)
		touchkey_stop(tkey_i2c);

	mutex_unlock(&tkey_i2c->input_dev->mutex);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}

static int touchkey_resume(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (touchkey_probe != true) {
		printk(KERN_ERR "%s Touchkey is not enabled. \n",
		       __func__);
		return 0;
	}
	mutex_lock(&tkey_i2c->input_dev->mutex);

	if (tkey_i2c->input_dev->users)
		touchkey_start(tkey_i2c);

	mutex_unlock(&tkey_i2c->input_dev->mutex);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(touchkey_pm_ops, touchkey_suspend, touchkey_resume);

#endif

static int touchkey_i2c_check(struct touchkey_i2c *tkey_i2c)
{
	char data[3] = { 0, };
	int ret = 0;

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 3);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "Failed to read Module version\n");
		return ret;
	}

	tkey_i2c->firmware_ver = data[1];
	tkey_i2c->module_ver = data[2];

	return ret;
}

static ssize_t touchkey_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;
	int ret;
	static const int ledCmd[] = {TK_CMD_LED_OFF, TK_CMD_LED_ON};

	ret = sscanf(buf, "%d", &data);
	if (ret != 1) {
		dev_err(&tkey_i2c->client->dev, "%s, %d err\n",
			__func__, __LINE__);
		return size;
	}

	if (data != 0 && data != 1) {
		dev_err(&tkey_i2c->client->dev, "%s wrong cmd %x\n",
			__func__, data);
		return size;
	}

	data = ledCmd[data];

	if (!tkey_i2c->enabled) {
		touchled_cmd_reversed = 1;
		goto out;
	}

	ret = i2c_touchkey_write(tkey_i2c->client, (u8 *) &data, 1);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "%s: Error turn on led %d\n",
			__func__, ret);
		touchled_cmd_reversed = 1;
		goto out;
	}
	msleep(30);

out:
	touchkey_led_status = data;

	return size;
}

#if defined(TK_USE_4KEY)
static ssize_t touchkey_menu_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[10] =%d,data[11] = %d\n", __func__,
	       data[10], data[11]);
	menu_sensitivity = ((0x00FF & data[10]) << 8) | data[11];

	return sprintf(buf, "%d\n", menu_sensitivity);
}

static ssize_t touchkey_home_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[12] =%d,data[13] = %d\n", __func__,
	       data[12], data[13]);
	home_sensitivity = ((0x00FF & data[12]) << 8) | data[13];

	return sprintf(buf, "%d\n", home_sensitivity);
}

static ssize_t touchkey_back_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[14] =%d,data[15] = %d\n", __func__,
	       data[14], data[15]);
	back_sensitivity = ((0x00FF & data[14]) << 8) | data[15];

	return sprintf(buf, "%d\n", back_sensitivity);
}

static ssize_t touchkey_search_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[16] =%d,data[17] = %d\n", __func__,
	       data[16], data[17]);
	search_sensitivity = ((0x00FF & data[16]) << 8) | data[17];

	return sprintf(buf, "%d\n", search_sensitivity);
}
#else
static ssize_t touchkey_menu_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[14] = { 0, };
	int ret;

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 14);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[10] = %d, data[11] =%d\n", __func__,
			data[10], data[11]);
	menu_sensitivity = ((0x00FF & data[10]) << 8) | data[11];

	return sprintf(buf, "%d\n", menu_sensitivity);
}

static ssize_t touchkey_back_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[14] = { 0, };
	int ret;

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 14);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[12] = %d, data[13] =%d\n", __func__,
			data[12], data[13]);
	back_sensitivity = ((0x00FF & data[12]) << 8) | data[13];

	return sprintf(buf, "%d\n", back_sensitivity);
}
#endif

#if defined(TK_HAS_AUTOCAL)
static ssize_t autocalibration_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_dbg(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	if (data == 1)
		touchkey_autocalibration(tkey_i2c);

	return size;
}

static ssize_t autocalibration_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	u8 data[6];
	int ret;
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 6);
	if ((data[5] & TK_BIT_AUTOCAL))
		return sprintf(buf, "Enabled\n");
	else
		return sprintf(buf, "Disabled\n");

}
#endif

#if defined(CONFIG_GLOVE_TOUCH)
static ssize_t glove_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_dbg(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	touchkey_glovemode(data);

	return size;
}
#endif

#ifdef TKEY_FLIP_MODE
static ssize_t flip_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_info(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	touchkey_flip_cover(data);

	return size;
}
#endif

static ssize_t touch_sensitivity_control(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	unsigned char data = 0x40;
	i2c_touchkey_write(tkey_i2c->client, &data, 1);
	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);
	msleep(20);
	return size;
}

static ssize_t set_touchkey_firm_version_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (tkey_i2c->firmware_id == TK_MODULE_20045)
		return sprintf(buf, "0x%02x\n", TK_FIRMWARE_VER_45);
	else if (tkey_i2c->firmware_id == TK_MODULE_20055)
		return sprintf(buf, "0x%02x\n", TK_FIRMWARE_VER_55);
	else if (tkey_i2c->firmware_id == TK_MODULE_20065)
		return sprintf(buf, "0x%02x\n", TK_FIRMWARE_VER_65);
	else
		return sprintf(buf, "unknown\n");
}

static ssize_t set_touchkey_update_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count = 0;
	int retry = 3;

	tkey_i2c->update_status = TK_UPDATE_DOWN;

	disable_irq(tkey_i2c->irq);

	if (tkey_i2c->firmware_id != TK_MODULE_20065) {
		dev_info(&tkey_i2c->client->dev,
			 "Firmware auto update do not excute\n");
		tkey_i2c->update_status = TK_UPDATE_PASS;
		count = 1;
		enable_irq(tkey_i2c->irq);
		return count;
	}

#ifdef TEST_JIG_MODE
	unsigned char get_touch = 0x40;
#endif

	while (retry--) {
		if (ISSP_main(tkey_i2c) == 0) {
			dev_err(&tkey_i2c->client->dev,
				"Touchkey_update succeeded\n");
			tkey_i2c->update_status = TK_UPDATE_PASS;
			count = 1;
			msleep(50);
			break;
		}
		dev_err(&tkey_i2c->client->dev,
			"Touchkey_update failed... retry...\n");
	}
	if (retry <= 0) {
		/* disable ldo11 */
		tkey_i2c->pdata->power_on(0);
		count = 0;
		dev_err(&tkey_i2c->client->dev, "Touchkey_update fail\n");
		tkey_i2c->update_status = TK_UPDATE_FAIL;
		enable_irq(tkey_i2c->irq);
		return count;
	}

#ifdef TEST_JIG_MODE
	i2c_touchkey_write(tkey_i2c->client, &get_touch, 1);
#endif

	enable_irq(tkey_i2c->irq);
#if defined(TK_HAS_AUTOCAL)
	touchkey_autocalibration(tkey_i2c);
#endif
	return count;

}

static ssize_t set_touchkey_firm_version_read_show(struct device *dev,
						   struct device_attribute
						   *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	char data[3] = { 0, };
	int count;

	i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 3);
	count = sprintf(buf, "0x%02x\n", data[1]);

	dev_info(&tkey_i2c->client->dev, "Touch_version_read 0x%02x\n", data[1]);
	dev_info(&tkey_i2c->client->dev, "Module_version_read 0x%02x\n", data[2]);
	return count;
}

static ssize_t set_touchkey_firm_status_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count = 0;

	dev_info(&tkey_i2c->client->dev, "Touch_update_read: update_status %d\n",
	       tkey_i2c->update_status);

	if (tkey_i2c->update_status == TK_UPDATE_PASS)
		count = sprintf(buf, "PASS\n");
	else if (tkey_i2c->update_status == TK_UPDATE_DOWN)
		count = sprintf(buf, "Downloading\n");
	else if (tkey_i2c->update_status == TK_UPDATE_FAIL)
		count = sprintf(buf, "Fail\n");

	return count;
}

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   touchkey_led_control);
static DEVICE_ATTR(touchkey_menu, S_IRUGO | S_IWUSR | S_IWGRP,
		   touchkey_menu_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO | S_IWUSR | S_IWGRP,
		   touchkey_back_show, NULL);

#if defined(TK_USE_4KEY)
static DEVICE_ATTR(touchkey_home, S_IRUGO, touchkey_home_show, NULL);
static DEVICE_ATTR(touchkey_search, S_IRUGO, touchkey_search_show, NULL);
#endif

static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, touch_sensitivity_control);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, set_touchkey_update_store);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_status_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_version_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_version_read_show, NULL);
#ifdef LED_LDO_WITH_REGULATOR
static DEVICE_ATTR(touchkey_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, brightness_control);
#endif

#if defined(TK_HAS_AUTOCAL)
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, touchkey_raw_data1_show, NULL);
static DEVICE_ATTR(touchkey_raw_data2, S_IRUGO, touchkey_raw_data2_show, NULL);
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, touchkey_raw_data3_show, NULL);
static DEVICE_ATTR(touchkey_idac0, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_idac1, S_IRUGO, touchkey_idac1_show, NULL);
static DEVICE_ATTR(touchkey_idac2, S_IRUGO, touchkey_idac2_show, NULL);
static DEVICE_ATTR(touchkey_idac3, S_IRUGO, touchkey_idac3_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);
static DEVICE_ATTR(autocal_enable, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   autocalibration_enable);
static DEVICE_ATTR(autocal_stat, S_IRUGO | S_IWUSR | S_IWGRP,
		   autocalibration_status, NULL);
#endif
#if defined(CONFIG_GLOVE_TOUCH)
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   glove_mode_enable);
#endif
#ifdef TKEY_FLIP_MODE
static DEVICE_ATTR(flip_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   flip_cover_mode_enable);
#endif

static struct attribute *touchkey_attributes[] = {
	&dev_attr_brightness.attr,
	&dev_attr_touchkey_menu.attr,
	&dev_attr_touchkey_back.attr,
#if defined(TK_USE_4KEY)
	&dev_attr_touchkey_home.attr,
	&dev_attr_touchkey_search.attr,
#endif
	&dev_attr_touch_sensitivity.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
#ifdef LED_LDO_WITH_REGULATOR
	&dev_attr_touchkey_brightness.attr,
#endif
#if defined(TK_HAS_AUTOCAL)
	&dev_attr_touchkey_raw_data0.attr,
	&dev_attr_touchkey_raw_data1.attr,
	&dev_attr_touchkey_raw_data2.attr,
	&dev_attr_touchkey_raw_data3.attr,
	&dev_attr_touchkey_idac0.attr,
	&dev_attr_touchkey_idac1.attr,
	&dev_attr_touchkey_idac2.attr,
	&dev_attr_touchkey_idac3.attr,
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_autocal_enable.attr,
	&dev_attr_autocal_stat.attr,
#endif
#if defined(CONFIG_GLOVE_TOUCH)
	&dev_attr_glove_mode.attr,
#endif
#ifdef TKEY_FLIP_MODE
	&dev_attr_flip_mode.attr,
#endif
	NULL,
};

static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static int i2c_touchkey_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct touchkey_platform_data *pdata = client->dev.platform_data;
	struct touchkey_i2c *tkey_i2c;
	struct input_dev *input_dev;
	int i;
	int ret = 0;
#ifdef TKEY_FW_FORCEUPDATE
	int firmup_retry = 0;
#endif

	if (pdata == NULL) {
		dev_err(&client->dev, "%s: no pdata\n", __func__);
		return -ENODEV;
	}

	/*Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (ret == 0) {
		dev_err(&client->dev, "No I2C functionality found\n");
		return -ENODEV;
	}

	/*Obtain kernel memory space for touchkey i2c */
	tkey_i2c = kzalloc(sizeof(struct touchkey_i2c), GFP_KERNEL);
	if (NULL == tkey_i2c) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

#ifdef CONFIG_GLOVE_TOUCH
	tkey_i2c_global = tkey_i2c;
#endif

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_allocate_input_device;
	}

	input_dev->name = "sec_touchkey";
	input_dev->phys = "sec_touchkey/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;
	input_dev->open = touchkey_input_open;
	input_dev->close = touchkey_input_close;

	/*tkey_i2c*/
	tkey_i2c->pdata = pdata;
	tkey_i2c->input_dev = input_dev;
	tkey_i2c->client = client;
	tkey_i2c->irq = client->irq;
	tkey_i2c->name = "sec_touchkey";
	init_completion(&tkey_i2c->init_done);
	mutex_init(&(tkey_i2c->lock));
#ifdef TK_USE_OPEN_DWORK
	INIT_DELAYED_WORK(&tkey_i2c->open_work, touchkey_open_work);
#endif
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	set_bit(EV_KEY, input_dev->evbit);
#ifdef CONFIG_VT_TKEY_SKIP_MATCH
	set_bit(EV_TOUCHKEY, input_dev->evbit);
#endif

	for (i = 1; i < touchkey_count; i++)
		set_bit(touchkey_keycode[i], input_dev->keybit);

	input_set_drvdata(input_dev, tkey_i2c);
	i2c_set_clientdata(client, tkey_i2c);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_register_device;
	}

	tkey_i2c->pdata->power_on(1);
	msleep(50);

	tkey_i2c->enabled = true;

	/*sysfs*/
	tkey_i2c->dev = device_create(sec_class, NULL, 0, NULL, "sec_touchkey");

	ret = IS_ERR(tkey_i2c->dev);
	if (ret) {
		dev_err(&client->dev, "Failed to create device(tkey_i2c->dev)!\n");
		goto err_device_create;
	} else {
		dev_set_drvdata(tkey_i2c->dev, tkey_i2c);
		ret = sysfs_create_group(&tkey_i2c->dev->kobj,
					&touchkey_attr_group);
		if (ret) {
			dev_err(&client->dev, "Failed to create sysfs group\n");
			goto err_sysfs_init;
		}
	}

#ifdef TKEY_FW_FORCEUPDATE
	dev_err(&client->dev, "JA LCD type Print : 0x%06X\n", lcdtype);
	if (lcdtype == 0) {
		dev_err(&client->dev, "Device wasn't connected to board\n");
		goto err_i2c_check;
	}
#else
#if defined(TK_USE_LCDTYPE_CHECK)
	dev_err(&client->dev, "JA LCD type Print : 0x%06X\n", lcdtype);
	if (lcdtype == 0) {
		dev_err(&client->dev, "Device wasn't connected to board\n");
		goto err_i2c_check;
	}
#else
	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "i2c_check failed\n");
		goto err_i2c_check;
	}
#endif
#endif

#ifdef TOUCHKEY_BOOSTER
		ret = touchkey_init_dvfs(tkey_i2c);
		if (ret < 0) {
			dev_err(&client->dev, "Fail get dvfs level for touch booster\n");
			goto err_i2c_check;
		}
#endif

#if defined(CONFIG_GLOVE_TOUCH)
		mutex_init(&tkey_i2c->tsk_glove_lock);
		INIT_DELAYED_WORK(&tkey_i2c->glove_change_work, touchkey_glove_change_work);
		tkey_i2c->tsk_glove_mode_status = false;
#endif

	ret =
		request_threaded_irq(tkey_i2c->irq, NULL, touchkey_interrupt,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, tkey_i2c->name, tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to request irq(%d) - %d\n",
			tkey_i2c->irq, ret);
		goto err_request_threaded_irq;
	}

	tkey_i2c->pdata->led_power_on(1);

#if defined(TK_HAS_FIRMWARE_UPDATE)
tkey_firmupdate_retry_byreboot:
	ret = touchkey_firmware_update(tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "Failed firmware updating process (%d)\n",
			ret);
		#ifndef TKEY_FW_FORCEUPDATE
		goto err_firmware_update;
		#endif
	}
#ifdef TKEY_FW_FORCEUPDATE
	if(tkey_i2c->firmware_ver != TK_FIRMWARE_VER_65){
		tkey_i2c->pdata->power_on(0);
		msleep(70);
		tkey_i2c->pdata->power_on(1);
		msleep(50);
		firmup_retry++;
		if(firmup_retry < 4) goto tkey_firmupdate_retry_byreboot;
		else goto err_firmware_update;
	}
#endif
#endif

#if defined(TK_INFORM_CHARGER)
	tkey_i2c->callbacks.inform_charger = touchkey_ta_cb;
	if (tkey_i2c->pdata->register_cb) {
		dev_info(&client->dev, "Touchkey TA information\n");
		tkey_i2c->pdata->register_cb(&tkey_i2c->callbacks);
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	tkey_i2c->early_suspend.suspend =
		(void *)sec_touchkey_early_suspend;
	tkey_i2c->early_suspend.resume =
		(void *)sec_touchkey_late_resume;
	register_early_suspend(&tkey_i2c->early_suspend);
#endif

#if defined(TK_HAS_AUTOCAL)
	touchkey_autocalibration(tkey_i2c);
#endif

	touchkey_stop(tkey_i2c);
	complete_all(&tkey_i2c->init_done);
	touchkey_probe = true;

	return 0;

#if defined(TK_HAS_FIRMWARE_UPDATE)
err_firmware_update:
	tkey_i2c->pdata->led_power_on(0);
	disable_irq(tkey_i2c->irq);
        free_irq(tkey_i2c->irq, tkey_i2c);	
#endif
err_request_threaded_irq:
err_i2c_check:
	sysfs_remove_group(&tkey_i2c->dev->kobj, &touchkey_attr_group);
err_device_create:
err_sysfs_init:
	tkey_i2c->pdata->power_on(0);
	input_unregister_device(input_dev);
	input_dev = NULL;
err_register_device:
	input_free_device(input_dev);
err_allocate_input_device:
	kfree(tkey_i2c);
	return ret;
}

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "sec_touchkey_driver",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &touchkey_pm_ops,
#endif
	},
	.id_table = sec_touchkey_id,
	.probe = i2c_touchkey_probe,
};

static int __init touchkey_init(void)
{
#ifdef TEST_JIG_MODE
	unsigned char get_touch = 0x40;
#endif

	i2c_add_driver(&touchkey_i2c_driver);

#ifdef TEST_JIG_MODE
	i2c_touchkey_write(tkey_i2c->client, &get_touch, 1);
#endif
	return 0;
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&touchkey_i2c_driver);
	touchkey_probe = false;
}

late_initcall(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("touch keypad");
