/*
 *  wacom_i2c.c - Wacom G5 Digitizer Controller (I2C bus)
 *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/wacom_i2c.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include "wacom_i2c_func.h"
#include "wacom_i2c_firm.h"

#if defined(CONFIG_EPEN_WACOM_G9PL) \
	|| defined(CONFIG_EPEN_WACOM_G9PLL)
#include "w9002_flash.h"
#else
#include "wacom_i2c_flash.h"
#endif
#ifdef WACOM_IMPORT_FW_ALGO
#include "wacom_i2c_coord_tables.h"
#endif

#define WACOM_UMS_UPDATE
#define WACOM_FW_PATH "/sdcard/firmware/wacom_firm.bin"

unsigned char screen_rotate;
unsigned char user_hand = 1;

static void wacom_enable_irq(struct wacom_i2c *wac_i2c, bool enable)
{
	static int depth;

	if (enable) {
		if (depth) {
			--depth;
			enable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
			enable_irq(wac_i2c->irq_pdct);
#endif
		}
	} else {
		if (!depth) {
			++depth;
			disable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
			disable_irq(wac_i2c->irq_pdct);
#endif
		}
	}

#ifdef WACOM_IRQ_DEBUG
	printk(KERN_DEBUG"epen:Enable %d, depth %d\n", (int)enable, depth);
#endif
}

static void wacom_i2c_reset_hw(struct wacom_g5_platform_data *wac_pdata)
{
	/* Reset IC */
	wac_pdata->suspend_platform_hw();
	msleep(100);
	wac_pdata->resume_platform_hw();
	msleep(200);
}

static void wacom_power_on(struct wacom_i2c *wac_i2c)
{
	bool en = true;

	printk(KERN_DEBUG"epen: %s\n", __func__);

#ifdef BATTERY_SAVING_MODE
	if (wac_i2c->battery_saving_mode
		&& wac_i2c->pen_insert)
		en = false;
#endif

	if (wake_lock_active(&wac_i2c->wakelock)) {
		en = false;
		printk(KERN_DEBUG"epen:wake_lock active\n");
	}

	if (en) {
		wac_i2c->wac_pdata->resume_platform_hw();
		cancel_delayed_work_sync(&wac_i2c->resume_work);
		schedule_delayed_work(&wac_i2c->resume_work, HZ / 5);
	}
}

static void wacom_power_off(struct wacom_i2c *wac_i2c)
{
	if (wac_i2c->power_enable) {
		wacom_enable_irq(wac_i2c, false);

		/* release pen, if it is pressed */
		if (wac_i2c->pen_pressed || wac_i2c->side_pressed
			|| wac_i2c->pen_prox)
			forced_release(wac_i2c);

		if (!wake_lock_active(&wac_i2c->wakelock)) {
			wac_i2c->power_enable = false;
			wac_i2c->wac_pdata->suspend_platform_hw();
		} else
			printk(KERN_DEBUG"epen:wake_lock active\n");
	}
}

int wacom_i2c_get_ums_data(struct wacom_i2c *wac_i2c, u8 **ums_data)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
	unsigned int nSize;

	nSize = WACOM_FW_SIZE;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WACOM_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		printk(KERN_ERR "epen:failed to open %s.\n", WACOM_FW_PATH);
		ret = -ENOENT;
		set_fs(old_fs);
		return ret;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	printk(KERN_NOTICE
		"epen:start, file path %s, size %ld Bytes\n",
		WACOM_FW_PATH, fsize);
	
	if (fsize != nSize) {
		printk(KERN_ERR "epen:UMS firmware size is different\n");
		ret = -EFBIG;
		goto size_error;
	}

	*ums_data = kmalloc(fsize, GFP_KERNEL);
	if (IS_ERR(*ums_data)) {
		printk(KERN_ERR
			"epen:%s, kmalloc failed\n", __func__);
		ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)*ums_data,
		fsize, &fp->f_pos);
	printk(KERN_NOTICE "epen:nread %ld Bytes\n", nread);
	if (nread != fsize) {
		printk(KERN_ERR
			"epen:failed to read firmware file, nread %ld Bytes\n",
			nread);
		ret = -EIO;
		kfree(*ums_data);
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

 read_err:
 malloc_error:
 size_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

int wacom_i2c_fw_update_UMS(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	u8 *ums_data = NULL;

	printk(KERN_ERR "epen:Start firmware flashing (UMS).\n");

	/*read firmware data*/
	ret = wacom_i2c_get_ums_data(wac_i2c, &ums_data);
	if (ret < 0) {
		printk(KERN_DEBUG"epen:file op is failed\n");
		return 0;
	}

	/*start firm update*/
	wacom_i2c_set_firm_data(ums_data);

	ret = wacom_i2c_flash(wac_i2c);
	if (ret < 0) {
		printk(KERN_ERR
			"epen:failed to write firmware(%d)\n", ret);
		kfree(ums_data);
		wacom_i2c_set_firm_data(NULL);
		return ret;
	}

	wacom_i2c_set_firm_data(NULL);
	kfree(ums_data);

	return 0;
}

int wacom_i2c_firm_update(struct wacom_i2c *wac_i2c)
{
	int ret;
	int retry = 3;
	const struct firmware *firm_data = NULL;

	while (retry--) {
		ret =
		    request_firmware(&firm_data, fw_name,
				     &wac_i2c->client->dev);
		if (ret < 0) {
			printk(KERN_ERR
			       "epen:Unable to open firmware. ret %d retry %d\n",
			       ret, retry);
			continue;
		}
		wacom_i2c_set_firm_data((unsigned char *)firm_data->data);

		ret = wacom_i2c_flash(wac_i2c);

		if (ret == 0) {
			release_firmware(firm_data);
			break;
		}

		printk(KERN_ERR "epen:failed to write firmware(%d)\n", ret);
		release_firmware(firm_data);

		/* Reset IC */
		wacom_i2c_reset_hw(wac_i2c->wac_pdata);
	}

	if (ret < 0)
		return -1;

	return 0;
}

static irqreturn_t wacom_interrupt(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	wacom_i2c_coord(wac_i2c);
	return IRQ_HANDLED;
}

#if defined(WACOM_PDCT_WORK_AROUND)
static irqreturn_t wacom_interrupt_pdct(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	if (wac_i2c->query_status == false)
		return IRQ_HANDLED;

	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->wac_pdata->gpio_pendct);

	printk(KERN_DEBUG "epen:pdct %d(%d)\n",
		wac_i2c->pen_pdct, wac_i2c->pen_prox);

	if (wac_i2c->pen_pdct == PDCT_NOSIGNAL) {
		/* If rdy is 1, pen is still working*/
		if (wac_i2c->pen_prox == 0)
			forced_release(wac_i2c);
	} else if (wac_i2c->pen_prox == 0)
		forced_hover(wac_i2c);

	return IRQ_HANDLED;
}
#endif

#ifdef WACOM_PEN_DETECT
#ifdef CONFIG_MACH_T0
bool wacom_i2c_invert_by_switch_type(void)
{
	if (system_rev < WACOM_DETECT_SWITCH_HWID)
		return true;

	return false;
}
#endif

static void pen_insert_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, pen_insert_dwork.work);

	wac_i2c->pen_insert = !gpio_get_value(wac_i2c->gpio_pen_insert);
#if defined(CONFIG_MACH_T0)
	if (wac_i2c->invert_pen_insert)
		wac_i2c->pen_insert = !wac_i2c->pen_insert;
#endif

	printk(KERN_DEBUG "epen:%s : %d\n",
		__func__, wac_i2c->pen_insert);

	input_report_switch(wac_i2c->input_dev,
		SW_PEN_INSERT, !wac_i2c->pen_insert);
	input_sync(wac_i2c->input_dev);

#ifdef BATTERY_SAVING_MODE
	if (wac_i2c->pen_insert) {
		if (wac_i2c->battery_saving_mode)
			wacom_power_off(wac_i2c);
	} else
		wacom_power_on(wac_i2c);
#endif
}
static irqreturn_t wacom_pen_detect(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);
	schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 20);
	return IRQ_HANDLED;
}

static int init_pen_insert(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	int irq = gpio_to_irq(wac_i2c->gpio_pen_insert);

	INIT_DELAYED_WORK(&wac_i2c->pen_insert_dwork, pen_insert_work);

	ret =
		request_threaded_irq(
			irq, NULL,
			wacom_pen_detect,
			IRQF_DISABLED | IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"pen_insert", wac_i2c);
	if (ret < 0)
		printk(KERN_ERR
			"epen:failed to request pen insert irq\n");

	enable_irq_wake(irq);

	/* update the current status */
	schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 2);

	return 0;
}

#endif

static void stop_wacom(struct wacom_i2c *wac_i2c)
{
	printk(KERN_DEBUG "epen:%s.\n", __func__);
	wacom_power_off(wac_i2c);
}

static int wacom_i2c_input_open(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	int ret = 0;
	static bool init_insert = true;

	printk(KERN_DEBUG"epen:%s\n", __func__);
#ifdef WACOM_PEN_DETECT
	if (unlikely(init_insert)) {
		init_pen_insert(wac_i2c);
		init_insert = false;
	}
#endif

#if 0
	ret = wait_for_completion_interruptible_timeout(&wac_i2c->init_done,
		msecs_to_jiffies(1 * MSEC_PER_SEC));

	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
			"error while waiting for device to init (%d)\n", ret);
		ret = -ENXIO;
		goto err_open;
	}
	if (ret == 0) {
		dev_err(&wac_i2c->client->dev,
			"timedout while waiting for device to init\n");
		ret = -ENXIO;
		goto err_open;
	}
#endif
	wacom_power_on(wac_i2c);

err_open:
	return ret;
}

static void wacom_i2c_input_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	printk(KERN_DEBUG"epen:%s\n", __func__);

	stop_wacom(wac_i2c);

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);
}


static void wacom_i2c_set_input_values(struct i2c_client *client,
				       struct wacom_i2c *wac_i2c,
				       struct input_dev *input_dev)
{
	/*Set input values before registering input device */

	input_dev->name = "sec_e-pen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	input_dev->evbit[0] |= BIT_MASK(EV_SW);
	input_set_capability(input_dev, EV_SW, SW_PEN_INSERT);

	input_dev->open = wacom_i2c_input_open;
	input_dev->close = wacom_i2c_input_close;

	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
	__set_bit(BTN_STYLUS, input_dev->keybit);
	__set_bit(KEY_UNKNOWN, input_dev->keybit);
	__set_bit(KEY_PEN_PDCT, input_dev->keybit);

	/*  __set_bit(BTN_STYLUS2, input_dev->keybit); */
	/*  __set_bit(ABS_MISC, input_dev->absbit); */

	/*softkey*/
#ifdef WACOM_USE_SOFTKEY
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
#endif
}

static int wacom_check_emr_prox(struct wacom_g5_callbacks *cb)
{
	struct wacom_i2c *wac = container_of(cb, struct wacom_i2c, callbacks);
	printk(KERN_DEBUG "epen:%s:\n", __func__);

	return wac->pen_prox;
}

static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);
	free_irq(client->irq, wac_i2c);
	input_unregister_device(wac_i2c->input_dev);
	kfree(wac_i2c);

	return 0;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
static void wacom_i2c_early_suspend(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c =
	    container_of(h, struct wacom_i2c, early_suspend);
	printk(KERN_DEBUG "epen:%s.\n", __func__);
	wacom_power_off(wac_i2c);
}

static void wacom_i2c_late_resume(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c =
		container_of(h, struct wacom_i2c, early_suspend);

	printk(KERN_DEBUG "epen:%s.\n", __func__);
	wacom_power_on(wac_i2c);
}
#endif

static void wacom_i2c_resume_work(struct work_struct *work)
{
struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, resume_work.work);

#if defined(WACOM_PDCT_WORK_AROUND)
	irq_set_irq_type(wac_i2c->irq_pdct, IRQ_TYPE_EDGE_BOTH);
#endif

	wac_i2c->power_enable = true;
	wacom_enable_irq(wac_i2c, true);
	printk(KERN_DEBUG "epen:%s\n", __func__);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define wacom_i2c_suspend	NULL
#define wacom_i2c_resume	NULL

#else
static int wacom_i2c_suspend(struct device *dev)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	printk(KERN_DEBUG"%s\n", __func__);

	mutex_lock(&wac_i2c->input_dev->mutex);

	if (wac_i2c->input_dev->users)
		stop_wacom(wac_i2c);

	mutex_unlock(&wac_i2c->input_dev->mutex);

	return 0;
}

static int wacom_i2c_resume(struct device *dev)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	printk(KERN_DEBUG"%s\n", __func__);

	mutex_lock(&wac_i2c->input_dev->mutex);

	if (wac_i2c->input_dev->users)
		wacom_power_on(wac_i2c);

	mutex_unlock(&wac_i2c->input_dev->mutex);

	dev_dbg(&wac_i2c->client->dev, "%s\n", __func__);

	return 0;
}
static SIMPLE_DEV_PM_OPS(wacom_pm_ops, wacom_i2c_suspend, wacom_i2c_resume);
#endif

static ssize_t epen_firm_update_status_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	printk(KERN_DEBUG "epen:%s:(%d)\n", __func__,
	       wac_i2c->wac_feature->firm_update_status);

	if (wac_i2c->wac_feature->firm_update_status == 2)
		return sprintf(buf, "PASS\n");
	else if (wac_i2c->wac_feature->firm_update_status == 1)
		return sprintf(buf, "DOWNLOADING\n");
	else if (wac_i2c->wac_feature->firm_update_status == -1)
		return sprintf(buf, "FAIL\n");
	else
		return 0;
}

static ssize_t epen_firm_version_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	printk(KERN_DEBUG "epen:%s: 0x%x|0x%X\n", __func__,
	       wac_i2c->wac_feature->fw_version, fw_ver_file);

	return sprintf(buf, "%04X\t%04X\n",
			wac_i2c->wac_feature->fw_version,
			fw_ver_file);
}

#if defined(WACOM_IMPORT_FW_ALGO)
static ssize_t epen_tuning_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	printk(KERN_DEBUG "epen:%s: %s\n", __func__,
			tuning_version);

	return sprintf(buf, "%s_%s\n",
			tuning_model,
			tuning_version);
}

static ssize_t epen_rotation_store(struct device *dev,
struct device_attribute *attr,
	const char *buf, size_t count)
{
	static bool factory_test;
	static unsigned char last_rotation;
	unsigned int val;

	sscanf(buf, "%u", &val);

	/* Fix the rotation value to 0(Portrait) when factory test(15 mode) */
	if (val == 100 && !factory_test) {
		factory_test = true;
		screen_rotate = 0;
		printk(KERN_DEBUG"epen:%s, enter factory test mode\n",
			__func__);
	} else if (val == 200 && factory_test) {
		factory_test = false;
		screen_rotate = last_rotation;
		printk(KERN_DEBUG"epen:%s, exit factory test mode\n",
			__func__);
	}

	/* Framework use index 0, 1, 2, 3 for rotation 0, 90, 180, 270 */
	/* Driver use same rotation index */
	if (val >= 0 && val <= 3) {
		if (factory_test)
			last_rotation = val;
		else
			screen_rotate = val;
	}

	/* 0: Portrait 0, 1: Landscape 90, 2: Portrait 180 3: Landscape 270 */
	printk(KERN_DEBUG"epen:%s: rotate=%d\n", __func__, screen_rotate);

	return count;
}

static ssize_t epen_hand_store(struct device *dev,
struct device_attribute *attr, const char *buf,
	size_t count)
{
	unsigned int val;

	sscanf(buf, "%u", &val);

	if (val == 0 || val == 1)
		user_hand = (unsigned char)val;

	/* 0:Left hand, 1:Right Hand */
	printk(KERN_DEBUG"epen:%s: hand=%u\n", __func__, user_hand);

	return count;
}
#endif

static bool check_update_condition(struct wacom_i2c *wac_i2c, const char buf)
{
	u32 fw_ver_ic = wac_i2c->wac_feature->fw_version;
	bool bUpdate = false;

	switch (buf) {
	case 'I':
	case 'K':
		bUpdate = true;
		break;
	case 'R':
	case 'W':
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		if (fw_ver_ic !=
			fw_ver_file)
#else
		if (fw_ver_ic <
			fw_ver_file)
#endif
			bUpdate = true;
		break;
	default:
		printk(KERN_ERR "epen:wrong parameter\n");
		bUpdate = false;
		break;
	}

	return bUpdate;
}

static ssize_t epen_firmware_update_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret = 1;
	u32 fw_ver_ic = wac_i2c->wac_feature->fw_version;
	bool need_update = false;

	printk(KERN_DEBUG "epen:%s\n", __func__);

	need_update = check_update_condition(wac_i2c, *buf);
	if (need_update == false) {
		printk(KERN_DEBUG"epen:Pass Update."
			"Cmd %c, IC ver %04x, Ker ver %04x\n",
			*buf, fw_ver_ic, fw_ver_file);
		return count;
	}

	/*start firm update*/
	mutex_lock(&wac_i2c->lock);
	wacom_enable_irq(wac_i2c, false);
	wac_i2c->wac_feature->firm_update_status = 1;

	switch (*buf) {
	/*ums*/
	case 'I':
		ret = wacom_i2c_fw_update_UMS(wac_i2c);
		break;
	/*kernel*/
	case 'K':
		printk(KERN_ERR
			"epen:Start firmware flashing (kernel image).\n");
	case 'R':
		ret = wacom_i2c_firm_update(wac_i2c);
		break;
	default:
		/*There's no default case*/
		break;
	}

	if (ret < 0) {
		printk(KERN_ERR "epen:failed to flash firmware.\n");
		goto failure;
	}

	printk(KERN_ERR "epen:Finish firmware flashing.\n");

	wacom_i2c_query(wac_i2c);

	wac_i2c->wac_feature->firm_update_status = 2;
	wacom_enable_irq(wac_i2c, true);
	mutex_unlock(&wac_i2c->lock);

	return count;

 failure:
	wac_i2c->wac_feature->firm_update_status = -1;
	wacom_enable_irq(wac_i2c, true);
	mutex_unlock(&wac_i2c->lock);
	return -1;
}

static ssize_t epen_reset_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret;
	int val;

	sscanf(buf, "%d", &val);

	if (val == 1) {
		wacom_enable_irq(wac_i2c, false);

		/* Reset IC */
		wacom_i2c_reset_hw(wac_i2c->wac_pdata);
		/* I2C Test */
		wacom_i2c_query(wac_i2c);

		wacom_enable_irq(wac_i2c, true);

		printk(KERN_DEBUG "epen:%s, result %d\n", __func__,
		       wac_i2c->query_status);
	}

	return count;
}

static ssize_t epen_reset_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->query_status) {
		printk(KERN_DEBUG "epen:%s, PASS\n", __func__);
		return sprintf(buf, "PASS\n");
	} else {
		printk(KERN_DEBUG "epen:%s, FAIL\n", __func__);
		return sprintf(buf, "FAIL\n");
	}
}

#ifdef WACOM_USE_AVE_TRANSITION
static ssize_t epen_ave_store(struct device *dev,
struct device_attribute *attr,
	const char *buf, size_t count)
{
	int v1, v2, v3, v4, v5;
	int height;

	sscanf(buf, "%d%d%d%d%d%d", &height, &v1, &v2, &v3, &v4, &v5);

	if (height < 0 || height > 2) {
		printk(KERN_DEBUG"epen:Height err %d\n", height);
		return count;
	}

	g_aveLevel_C[height] = v1;
	g_aveLevel_X[height] = v2;
	g_aveLevel_Y[height] = v3;
	g_aveLevel_Trs[height] = v4;
	g_aveLevel_Cor[height] = v5;

	printk(KERN_DEBUG "epen:%s, v1 %d v2 %d v3 %d v4 %d\n", __func__,
		v1, v2, v3, v4);

	return count;
}

static ssize_t epen_ave_result_show(struct device *dev,
struct device_attribute *attr,
	char *buf)
{
	printk(KERN_DEBUG "epen:%s\n%d %d %d %d\n"
		"%d %d %d %d\n%d %d %d %d\n",
		__func__,
		g_aveLevel_C[0], g_aveLevel_X[0],
		g_aveLevel_Y[0], g_aveLevel_Trs[0],
		g_aveLevel_C[1], g_aveLevel_X[1],
		g_aveLevel_Y[1], g_aveLevel_Trs[1],
		g_aveLevel_C[2], g_aveLevel_X[2],
		g_aveLevel_Y[2], g_aveLevel_Trs[2]);
	return sprintf(buf, "%d %d %d %d\n%d %d %d %d\n%d %d %d %d\n",
		g_aveLevel_C[0], g_aveLevel_X[0],
		g_aveLevel_Y[0], g_aveLevel_Trs[0],
		g_aveLevel_C[1], g_aveLevel_X[1],
		g_aveLevel_Y[1], g_aveLevel_Trs[1],
		g_aveLevel_C[2], g_aveLevel_X[2],
		g_aveLevel_Y[2], g_aveLevel_Trs[2]);
}
#endif

static ssize_t epen_checksum_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val != 1) {
		printk(KERN_DEBUG"epen: wrong cmd %d\n", val);
		return count;
	}

	wacom_enable_irq(wac_i2c, false);
	wacom_checksum(wac_i2c);
	wacom_enable_irq(wac_i2c, true);

	printk(KERN_DEBUG "epen:%s, result %d\n",
		__func__, wac_i2c->checksum_result);

	return count;
}

static ssize_t epen_checksum_result_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->checksum_result) {
		printk(KERN_DEBUG "epen:checksum, PASS\n");
		return sprintf(buf, "PASS\n");
	} else {
		printk(KERN_DEBUG "epen:checksum, FAIL\n");
		return sprintf(buf, "FAIL\n");
	}
}

#ifdef BATTERY_SAVING_MODE
static ssize_t epen_saving_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	if (sscanf(buf, "%u", &val) == 1)
		wac_i2c->battery_saving_mode = !!val;

	if (wac_i2c->battery_saving_mode) {
		if (wac_i2c->pen_insert)
			wacom_power_off(wac_i2c);
	} else
		wacom_power_on(wac_i2c);
	return count;
}
#endif

/* firmware update */
static DEVICE_ATTR(epen_firm_update,
		   S_IWUSR | S_IWGRP, NULL, epen_firmware_update_store);
/* return firmware update status */
static DEVICE_ATTR(epen_firm_update_status,
		   S_IRUGO, epen_firm_update_status_show, NULL);
/* return firmware version */
static DEVICE_ATTR(epen_firm_version, S_IRUGO, epen_firm_version_show, NULL);
#if defined(WACOM_IMPORT_FW_ALGO)
/* return tuning data version */
static DEVICE_ATTR(epen_tuning_version, S_IRUGO,
			epen_tuning_version_show, NULL);
/* screen rotation */
static DEVICE_ATTR(epen_rotation, S_IWUSR | S_IWGRP, NULL, epen_rotation_store);
/* hand type */
static DEVICE_ATTR(epen_hand, S_IWUSR | S_IWGRP, NULL, epen_hand_store);
#endif

/* For SMD Test */
static DEVICE_ATTR(epen_reset, S_IWUSR | S_IWGRP, NULL, epen_reset_store);
static DEVICE_ATTR(epen_reset_result,
		   S_IRUSR | S_IRGRP, epen_reset_result_show, NULL);

/* For SMD Test. Check checksum */
static DEVICE_ATTR(epen_checksum, S_IWUSR | S_IWGRP, NULL, epen_checksum_store);
static DEVICE_ATTR(epen_checksum_result, S_IRUSR | S_IRGRP,
		   epen_checksum_result_show, NULL);

#ifdef WACOM_USE_AVE_TRANSITION
static DEVICE_ATTR(epen_ave, S_IWUSR | S_IWGRP, NULL, epen_ave_store);
static DEVICE_ATTR(epen_ave_result, S_IRUSR | S_IRGRP,
	epen_ave_result_show, NULL);
#endif

#ifdef WACOM_CONNECTION_CHECK
static DEVICE_ATTR(epen_connection,
		   S_IRUGO, epen_connection_show, NULL);
#endif

#ifdef BATTERY_SAVING_MODE
static DEVICE_ATTR(epen_saving_mode,
		   S_IWUSR | S_IWGRP, NULL, epen_saving_mode_store);
#endif

static struct attribute *epen_attributes[] = {
	&dev_attr_epen_firm_update.attr,
	&dev_attr_epen_firm_update_status.attr,
	&dev_attr_epen_firm_version.attr,
#if defined(WACOM_IMPORT_FW_ALGO)
	&dev_attr_epen_tuning_version.attr,
	&dev_attr_epen_rotation.attr,
	&dev_attr_epen_hand.attr,
#endif
	&dev_attr_epen_reset.attr,
	&dev_attr_epen_reset_result.attr,
	&dev_attr_epen_checksum.attr,
	&dev_attr_epen_checksum_result.attr,
#ifdef WACOM_USE_AVE_TRANSITION
	&dev_attr_epen_ave.attr,
	&dev_attr_epen_ave_result.attr,
#endif
#ifdef BATTERY_SAVING_MODE
	&dev_attr_epen_saving_mode.attr,
#endif
	NULL,
};

static struct attribute_group epen_attr_group = {
	.attrs = epen_attributes,
};

static void wacom_init_abs_params(struct wacom_i2c *wac_i2c)
{
	int min_x, min_y;
	int max_x, max_y;
	int pressure;

	min_x = 0;
	min_y = 0;

#ifdef WACOM_USE_QUERY_DATA
	pressure = wac_i2c->wac_feature->pressure_max;
	max_x = wac_i2c->wac_feature->x_max;
	max_y = wac_i2c->wac_feature->y_max;
#elif defined(WACOM_USE_PDATA)
	min_x = wac_i2c->wac_pdata->min_x;
	max_x = wac_i2c->wac_pdata->max_x;
	min_y = wac_i2c->wac_pdata->min_y;	
	max_y = wac_i2c->wac_pdata->max_y;
	pressure = wac_i2c->wac_pdata->max_pressure;
#endif

	if (wac_i2c->wac_pdata->xy_switch) {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, min_y,
			max_y, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, min_x,
			max_x, 4, 0);		
	} else {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, min_x,
			max_x, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, min_y,
			max_y, 4, 0);
	}
	input_set_abs_params(wac_i2c->input_dev, ABS_PRESSURE, 0,
		pressure, 0, 0);
}

#ifdef WACOM_IMPORT_FW_ALGO
static void wacom_init_fw_algo(struct wacom_i2c *wac_i2c)
{
#if defined(CONFIG_MACH_T0)
	int digitizer_type = 0;

	/*Set data by digitizer type*/
	digitizer_type = wacom_i2c_get_digitizer_type();

	if (digitizer_type == EPEN_DTYPE_B746) {
		printk(KERN_DEBUG"epen:Use Box filter\n");
		wac_i2c->use_aveTransition = true;
	} else if (digitizer_type == EPEN_DTYPE_B713) {
		printk(KERN_DEBUG"epen:Reset tilt for B713\n");

		/*Change tuning version for B713*/
		tuning_version = tuning_version_B713;

		memcpy(tilt_offsetX, tilt_offsetX_B713, sizeof(tilt_offsetX));
		memcpy(tilt_offsetY, tilt_offsetY_B713, sizeof(tilt_offsetY));
	} else if (digitizer_type == EPEN_DTYPE_B660) {
		printk(KERN_DEBUG"epen:Reset tilt and origin for B660\n");

		origin_offset[0] = EPEN_B660_ORG_X;
		origin_offset[1] = EPEN_B660_ORG_Y;
		memset(tilt_offsetX, 0, sizeof(tilt_offsetX));
		memset(tilt_offsetY, 0, sizeof(tilt_offsetY));
		wac_i2c->use_offset_table = false;
	}

	/*Set switch type*/
	wac_i2c->invert_pen_insert = wacom_i2c_invert_by_switch_type();
#endif

}
#endif

static int wacom_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct wacom_g5_platform_data *pdata = client->dev.platform_data;
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	int ret = 0;

	if (pdata == NULL) {
		printk(KERN_ERR "%s: no pdata\n", __func__);
		ret = -ENODEV;
		goto err_i2c_fail;
	}

	/*Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		printk(KERN_ERR "epen:No I2C functionality found\n");
		ret = -ENODEV;
		goto err_i2c_fail;
	}

	/*Obtain kernel memory space for wacom i2c */
	wac_i2c = kzalloc(sizeof(struct wacom_i2c), GFP_KERNEL);
	if (NULL == wac_i2c) {
		printk(KERN_ERR "epen:failed to allocate wac_i2c.\n");
		ret = -ENOMEM;
		goto err_freemem;
	}

	wac_i2c->client_boot = i2c_new_dummy(client->adapter,
		WACOM_I2C_BOOT);
	if (!wac_i2c->client_boot) {
		dev_err(&client->dev, "Fail to register sub client[0x%x]\n",
			 WACOM_I2C_BOOT);
	}

	input = input_allocate_device();
	if (NULL == input) {
		printk(KERN_ERR "epen:failed to allocate input device.\n");
		ret = -ENOMEM;
		goto err_input_allocate_device;
	}

	wacom_i2c_set_input_values(client, wac_i2c, input);

	wac_i2c->wac_feature = &wacom_feature_EMR;
	wac_i2c->wac_pdata = pdata;
	wac_i2c->input_dev = input;
	wac_i2c->client = client;
	wac_i2c->irq = client->irq;
	/*init_completion(&wac_i2c->init_done);*/
#ifdef WACOM_PDCT_WORK_AROUND
	wac_i2c->irq_pdct = gpio_to_irq(pdata->gpio_pendct);
	wac_i2c->pen_pdct = PDCT_NOSIGNAL;
#endif
#ifdef WACOM_PEN_DETECT
	wac_i2c->gpio_pen_insert = pdata->gpio_pen_insert;
#endif
#ifdef WACOM_IMPORT_FW_ALGO
	wac_i2c->use_offset_table = true;
	wac_i2c->use_aveTransition = false;
	wacom_init_fw_algo(wac_i2c);
#endif

	/*Change below if irq is needed */
	wac_i2c->irq_flag = 1;

	/*Register callbacks */
	wac_i2c->callbacks.check_prox = wacom_check_emr_prox;
	if (wac_i2c->wac_pdata->register_cb)
		wac_i2c->wac_pdata->register_cb(&wac_i2c->callbacks);

	/* Firmware Feature */
	wacom_i2c_init_firm_data();

	/* Power on */
	wac_i2c->wac_pdata->resume_platform_hw();
	msleep(200);
	wac_i2c->power_enable = true;

	wacom_i2c_query(wac_i2c);

	wacom_init_abs_params(wac_i2c);
	input_set_drvdata(input, wac_i2c);

	/*Change below if irq is needed */
	wac_i2c->irq_flag = 1;

	/*Set client data */
	i2c_set_clientdata(client, wac_i2c);
	i2c_set_clientdata(wac_i2c->client_boot, wac_i2c);

	/*Initializing for semaphor */
	mutex_init(&wac_i2c->lock);
	wake_lock_init(&wac_i2c->wakelock, WAKE_LOCK_SUSPEND, "wacom");
	INIT_DELAYED_WORK(&wac_i2c->resume_work, wacom_i2c_resume_work);

	/*Before registering input device, data in each input_dev must be set */
	ret = input_register_device(input);
	if (ret) {
		pr_err("epen:failed to register input device.\n");
		goto err_register_device;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	wac_i2c->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	wac_i2c->early_suspend.suspend = wacom_i2c_early_suspend;
	wac_i2c->early_suspend.resume = wacom_i2c_late_resume;
	register_early_suspend(&wac_i2c->early_suspend);
#endif

	wac_i2c->dev = device_create(sec_class, NULL, 0, NULL, "sec_epen");
	if (IS_ERR(wac_i2c->dev)) {
		printk(KERN_ERR "Failed to create device(wac_i2c->dev)!\n");
		goto err_sysfs_create_group;
	}

	dev_set_drvdata(wac_i2c->dev, wac_i2c);

	ret = sysfs_create_group(&wac_i2c->dev->kobj, &epen_attr_group);
	if (ret) {
		printk(KERN_ERR
			    "epen:failed to create sysfs group\n");
		goto err_sysfs_create_group;
	}

	/* firmware info */
	printk(KERN_NOTICE "epen:wacom fw ver : 0x%x, new fw ver : 0x%x\n",
	       wac_i2c->wac_feature->fw_version, fw_ver_file);

#ifdef CONFIG_SEC_TOUCHSCREEN_DVFS_LOCK
	INIT_DELAYED_WORK(&wac_i2c->dvfs_work, free_dvfs_lock);
	if (exynos_cpufreq_get_level(WACOM_DVFS_LOCK_FREQ,
			&wac_i2c->cpufreq_level))
		printk(KERN_ERR "epen:exynos_cpufreq_get_level Error\n");
#ifdef SEC_BUS_LOCK
	wac_i2c->dvfs_lock_status = false;
#if defined(CONFIG_MACH_P4NOTE)
	wac_i2c->bus_dev = dev_get("exynos-busfreq");
#endif	/* CONFIG_MACH_P4NOTE */
#endif	/* SEC_BUS_LOCK */
#endif	/* CONFIG_SEC_TOUCHSCREEN_DVFS_LOCK */

	/*Request IRQ */
	if (wac_i2c->irq_flag) {
		ret =
		    request_threaded_irq(wac_i2c->irq, NULL, wacom_interrupt,
					 IRQF_DISABLED | IRQF_TRIGGER_RISING |
					 IRQF_ONESHOT, wac_i2c->name, wac_i2c);
		if (ret < 0) {
			printk(KERN_ERR
			       "epen:failed to request irq(%d) - %d\n",
			       wac_i2c->irq, ret);
			goto err_request_irq;
		}

#if defined(WACOM_PDCT_WORK_AROUND)
		ret =
			request_threaded_irq(wac_i2c->irq_pdct, NULL,
					wacom_interrupt_pdct,
					IRQF_DISABLED | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					wac_i2c->name, wac_i2c);
		if (ret < 0) {
			printk(KERN_ERR
				"epen:failed to request irq(%d) - %d\n",
				wac_i2c->irq_pdct, ret);
			goto err_request_irq;
		}
#endif
	}

	/*complete_all(&wac_i2c->init_done);*/

	return 0;

 err_request_irq:
 err_sysfs_create_group:
 err_register_device:
	input_unregister_device(input);
 err_input_allocate_device:
	input_free_device(input);
 err_freemem:
	kfree(wac_i2c);
 err_i2c_fail:
	return ret;
}

static const struct i2c_device_id wacom_i2c_id[] = {
	{"wacom_g5sp_i2c", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

/*Create handler for wacom_i2c_driver*/
static struct i2c_driver wacom_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wacom_g5sp_i2c",
#ifdef CONFIG_PM
		.pm = &wacom_pm_ops,
#endif
		},
	.probe = wacom_i2c_probe,
	.remove = wacom_i2c_remove,
	.id_table = wacom_i2c_id,
};

static int __init wacom_i2c_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&wacom_i2c_driver);
	if (ret)
		printk(KERN_ERR "epen:fail to i2c_add_driver\n");
	return ret;
}

static void __exit wacom_i2c_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

late_initcall(wacom_i2c_init);
module_exit(wacom_i2c_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Driver for Wacom G5SP Digitizer Controller");

MODULE_LICENSE("GPL");
