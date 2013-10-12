/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include "sx9500_reg.h"
#include "sensors_core.h"

#define VENDOR_NAME              "SEMTECH"
#define MODEL_NAME               "SX9500"
#define MODULE_NAME              "grip_sensor"
#define CALIBRATION_FILE_PATH    "/efs/grip_cal_data"

#define IDLE                     0
#define ACTIVE                   1

#define CAL_RET_ERROR            -1
#define CAL_RET_NONE             0
#define CAL_RET_EXIST            1
#define CAL_RET_SUCCESS          2

#define INIT_TOUCH_MODE          0
#define NORMAL_TOUCH_MODE        1

#define SX9500_MODE_SLEEP        0
#define SX9500_MODE_NORMAL       1

#define MAIN_SENSOR              0
#define REF_SENSOR               1

#define DEFAULT_THRESHOLD        0x11
#define LIMIT_PROXOFFSET         2550 /* 30pF */

#define STANDARD_CAP_MAIN        200000
#define DEFAULT_THRESHOLD_GAP    2000

#define	TOUCH_CHECK_THRESHOLD    (STANDARD_CAP_MAIN + DEFAULT_THRESHOLD_GAP)
#define	TOUCH_CHECK_REF_AMB      0 // 44523
#define	TOUCH_CHECK_SLOPE        0 // 50
#define	TOUCH_CHECK_MAIN_AMB     0 // 151282

/* CS0, CS1, CS2, CS3 */
#define TOTAL_BOTTON_COUNT       1
#define ENABLE_CSX               ((1 << MAIN_SENSOR) | (1 << REF_SENSOR))

#define CSX_STATUS_REG           SX9500_TCHCMPSTAT_TCHSTAT0_FLAG

#define IRQ_PROCESS_CONDITION   (SX9500_IRQSTAT_TOUCH_FLAG	\
				| SX9500_IRQSTAT_RELEASE_FLAG	\
				| SX9500_IRQSTAT_COMPDONE_FLAG)

struct sx9500_p {
	struct i2c_client *client;
	struct input_dev *input;
	struct device *factory_device;
	struct delayed_work init_work;
	struct wake_lock grip_wake_lock;

	bool calSuccessed;
	bool flagDataSkip;
	u8 touchTh;
	u8 releaseTh;
	int calData;
	int touchMode;

	int irq;
	int gpioNirq;
	int state[TOTAL_BOTTON_COUNT];

	atomic_t enable;
};

static int sx9500_get_nirq_state(struct sx9500_p *data)
{
	return gpio_get_value_cansleep(data->gpioNirq);
}

static int sx9500_i2c_write(struct sx9500_p *data, u8 addr, u8 val)
{
	char buffer[2];
	int ret;

	buffer[0] = addr;
	buffer[1] = val;

	ret = i2c_master_send(data->client, buffer, 2);
	if (ret < 0)
		pr_err("[SX9500]: %s - i2c write error %d\n", __func__, ret);

	return ret;
}

static int sx9500_i2c_read(struct sx9500_p *data, u8 addr, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(data->client, addr);
	if (ret < 0)
		pr_err("[SX9500]: %s - i2c read error %d\n", __func__, ret);
	else
		*val = ret & 0x000000ff;

	return ret;
}

static u8 sx9500_read_irqstate(struct sx9500_p *data)
{
	u8 val = 0;

	if (sx9500_i2c_read(data, SX9500_IRQSTAT_REG, &val) >= 0)
		return (val & 0x00FF);

	return 0;
}

static void sx9500_initialize_register(struct sx9500_p *data)
{
	u8 val = 0;
	int idx;

	for (idx = 0; idx < (sizeof(setup_reg) >> 1); idx++) {
		sx9500_i2c_write(data, setup_reg[idx].reg, setup_reg[idx].val);
		pr_info("[SX9500]: %s - Write Reg: 0x%x Value: 0x%x\n",
			__func__, setup_reg[idx].reg, setup_reg[idx].val);

		sx9500_i2c_read(data, setup_reg[idx].reg, &val);
		pr_info("[SX9500]: %s - Read Reg: 0x%x Value: 0x%x\n\n",
			__func__, setup_reg[idx].reg, val);
	}
}

static void sx9500_initialize_chip(struct sx9500_p *data)
{
	int cnt = 0;

	while((sx9500_get_nirq_state(data) == 0) && (cnt++ < 10)) {
		sx9500_read_irqstate(data);
		msleep(20);
	}

	if (cnt >= 10)
		pr_err("[SX9500]: %s - s/w reset fail(%d)\n", __func__, cnt);

	sx9500_initialize_register(data);
}

static int sx9500_set_offset_calibration(struct sx9500_p *data)
{
	int ret = 0;

	ret = sx9500_i2c_write(data, SX9500_IRQSTAT_REG, 0xFF);

	return ret;
}

static void send_event(struct sx9500_p *data, int cnt, u8 state)
{
	if (state == ACTIVE) {
		data->state[cnt] = ACTIVE;
		sx9500_i2c_write(data, SX9500_CPS_CTRL6_REG, data->releaseTh);
		pr_info("[SX9500]: %s - %d button touched\n", __func__, cnt);
	} else {
		data->touchMode = NORMAL_TOUCH_MODE;
		data->state[cnt] = IDLE;
		sx9500_i2c_write(data, SX9500_CPS_CTRL6_REG, data->touchTh);
		pr_info("[SX9500]: %s - %d button released\n", __func__, cnt);
	}

	if (data->flagDataSkip == true)
		return;

	switch (cnt) {
		case 0:
			if (state == ACTIVE)
				input_report_rel(data->input, REL_MISC, 1);
			else
				input_report_rel(data->input, REL_MISC, 2);
			break;
		case 1:
		case 2:
		case 3:
			pr_info("[SX9500]: %s - There is no defined event for"
				" button %d.\n", __func__, cnt);
			break;
		default:
			break;
	}

	input_sync(data->input);
}

static void sx9500_display_data_reg(struct sx9500_p *data)
{
	u8 val, reg;
	int i;

	for (i = 0; i < TOTAL_BOTTON_COUNT; i++) {
		sx9500_i2c_write(data, SX9500_REGSENSORSELECT, i);
		pr_info("[SX9500]: ############# %d button #############\n", i);
		for (reg = SX9500_REGUSEMSB; reg <= SX9500_REGOFFSETLSB; reg++)
		{
			sx9500_i2c_read(data, reg, &val);
			pr_info("[SX9500]: %s - Register(0x%2x) data(0x%2x)\n",
				__func__, reg, val);
		}
	}
}

static s32 sx9500_get_capMain(struct sx9500_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	u16 fullByte = 0;
	s32 capMain = 0;
	s32 capRef = 0;

	/* Calculate out the Main Cap information */
	sx9500_i2c_write(data, SX9500_REGSENSORSELECT, MAIN_SENSOR);
	sx9500_i2c_read(data, SX9500_REGUSEMSB, &msByte);
	sx9500_i2c_read(data, SX9500_REGUSELSB, &lsByte);

	capMain = (s32)msByte;
	capMain = (capMain << 8) | ((s32)lsByte);
	if (capMain > 32767)
		capMain -= 65536;

	sx9500_i2c_read(data, SX9500_REGOFFSETMSB, &msByte);
	sx9500_i2c_read(data, SX9500_REGOFFSETLSB, &lsByte);

	fullByte = (u16)msByte;
	fullByte = (fullByte << 8) | ((u16)lsByte);

	msByte = (u8)(fullByte >> 6);
	lsByte = (u8)(fullByte - (((u16)msByte) << 6));

	capMain = 2 * (((s32)msByte * 3600) + ((s32)lsByte * 225)) +
		(((s32)capMain * 50000) / (8 * 65536) );

	/* Calculate out the Reference Cap information */
	sx9500_i2c_write(data, SX9500_REGSENSORSELECT, REF_SENSOR);
	sx9500_i2c_read(data, SX9500_REGUSEMSB, &msByte);
	sx9500_i2c_read(data, SX9500_REGUSELSB, &lsByte);

	capRef = (s32)msByte;
	capRef = (capRef << 8) | ((s32)lsByte);
	if (capRef > 32767)
		capRef -= 65536;

	sx9500_i2c_read(data, SX9500_REGOFFSETMSB, &msByte);
	sx9500_i2c_read(data, SX9500_REGOFFSETLSB, &lsByte);

	fullByte = (u16)msByte;
	fullByte = (fullByte << 8) | ((u16)lsByte);
	msByte = (u8)(fullByte >> 6);
	lsByte = (u8)(fullByte - (((u16)msByte) << 6));

	capRef = 2 * (((s32)msByte * 3600) + ((s32)lsByte * 225)) +
		(((s32)capRef * 50000) / (8 * 65536) );

	capRef = (capRef - TOUCH_CHECK_REF_AMB) *
		TOUCH_CHECK_SLOPE + TOUCH_CHECK_MAIN_AMB;

	pr_info("[SX9500]: %s - Calculated Caps Main: %ld Ref: %ld\n",
		__func__, (long int)capMain, (long int)capRef);

	/* Calculate out the difference between the two */
	capMain = capMain - capRef;

	return capMain;
}

static void sx9500_touchCheckWithRefSensor(struct sx9500_p *data)
{
	s32 capMain;
	int cnt = 0;

	capMain = sx9500_get_capMain(data);

	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++) {
		if (MAIN_SENSOR != cnt) {
			pr_info("[SX9500]: %s - Looking For: %d cnt: %d\n",
				__func__, MAIN_SENSOR, cnt);
			continue;
		}

		if (data->state[cnt] == IDLE) {
			if (capMain >= (TOUCH_CHECK_THRESHOLD + data->calData))
				send_event(data, cnt, ACTIVE);
			else
				send_event(data, cnt, IDLE);
		} else {
			if (capMain < (TOUCH_CHECK_THRESHOLD + data->calData))
				send_event(data, cnt, IDLE);
			else
				send_event(data, cnt, ACTIVE);
		}
	}
}

static int sx9500_save_caldata(struct sx9500_p *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("[SX9500]: %s - Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)&data->calData,
		sizeof(int), &cal_filp->f_pos);
	if (ret != sizeof(int)) {
		pr_err("[SX9500]: %s - Can't write the cal data to file\n",
			__func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static void sx9500_open_caldata(struct sx9500_p *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		ret = PTR_ERR(cal_filp);
		if (ret != -ENOENT)
			pr_err("[SX9500]: %s - Can't open calibration file.\n",
				__func__);
		else {
			pr_info("[SX9500]: %s - There is no calibration file\n",
				__func__);
			/* calibration status init */
			data->calData = 0;
		}
		set_fs(old_fs);
		return;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)&data->calData,
		sizeof(int), &cal_filp->f_pos);
	if (ret != sizeof(int))
		pr_err("[SX9500]: %s - Can't read the cal data from file\n",
			__func__);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SX9500]: %s - (%d)", __func__, data->calData);
}

static int sx9500_set_mode(struct sx9500_p *data, unsigned char mode)
{
	int ret = -EINVAL;

	if (mode == SX9500_MODE_SLEEP) {
		ret = sx9500_i2c_write(data, SX9500_CPS_CTRL0_REG, 0x20);
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
	} else if (mode == SX9500_MODE_NORMAL) {
		ret = sx9500_i2c_write(data, SX9500_CPS_CTRL0_REG,
			0x20 | ENABLE_CSX);

		sx9500_set_offset_calibration(data);
		msleep(400);

		sx9500_touchCheckWithRefSensor(data);
		enable_irq(data->irq);
		enable_irq_wake(data->irq);

		/* make sure no interrupts are pending since enabling irq
		 * will only work on next falling edge */
		sx9500_read_irqstate(data);
	}

	pr_info("[SX9500]: %s - change the mode : %u\n", __func__, mode);
	return ret;
}

static int sx9500_do_calibrate(struct sx9500_p *data, bool do_calib)
{
	s32 capMain;

	if (do_calib == false) {
		data->calData = 0;
		goto exit;
	}

	if (atomic_read(&data->enable) == 0)
		sx9500_set_mode(data, SX9500_MODE_NORMAL);

	capMain = sx9500_get_capMain(data);
	data->calData = capMain - STANDARD_CAP_MAIN;

	if (atomic_read(&data->enable) == 0)
		sx9500_set_mode(data, SX9500_MODE_SLEEP);

exit:
	pr_info("[SX9500]: %s - (%d)\n", __func__, data->calData);
	return 0;
}

static int sx9500_check_offset(struct sx9500_p *data)
{
	u8 msByte = 0;
	u8 lsByte = 0;
	u16 fullByte = 0;

	sx9500_i2c_write(data, SX9500_REGSENSORSELECT, MAIN_SENSOR);
	sx9500_i2c_read(data, SX9500_REGOFFSETMSB, &msByte);
	sx9500_i2c_read(data, SX9500_REGOFFSETLSB, &lsByte);

	fullByte = ((u16)msByte << 8) | ((u16)lsByte);

	pr_info("[SX9500]: %s - PROXIOFFSET = %u\n", __func__, fullByte);

	if ((fullByte >= LIMIT_PROXOFFSET) || (fullByte == 0))
		return -1;

	return 0;
}

static void sx9500_set_enable(struct sx9500_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	if (enable) {
		if (pre_enable == 0) {
			data->touchMode = INIT_TOUCH_MODE;
			data->calSuccessed = false;

			sx9500_open_caldata(data);
			sx9500_set_mode(data, SX9500_MODE_NORMAL);
			atomic_set(&data->enable, 1);
		}
	} else {
		if (pre_enable == 1) {
			sx9500_set_mode(data, SX9500_MODE_SLEEP);
			atomic_set(&data->enable, 0);
		}
	}
}

static ssize_t sx9500_get_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	struct sx9500_p *data = dev_get_drvdata(dev);

	sx9500_i2c_read(data, SX9500_IRQSTAT_REG, &val);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sx9500_set_offset_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9500]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	if (val)
		sx9500_set_offset_calibration(data);

	return count;
}

static ssize_t sx9500_register_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0, val = 0;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d,%d", &regist, &val) != 2) {
		pr_err("[SX9500]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9500_i2c_write(data, (unsigned char)regist, (unsigned char)val);
	pr_info("[SX9500]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9500_register_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0;
	unsigned char val = 0;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &regist) != 1) {
		pr_err("[SX9500]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9500_i2c_read(data, (unsigned char)regist, &val);
	pr_info("[SX9500]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9500_read_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	sx9500_display_data_reg(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sx9500_sw_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == 1)
		sx9500_set_mode(data, SX9500_MODE_SLEEP);

	ret = sx9500_i2c_write(data, SX9500_SOFTRESET_REG, SX9500_SOFTRESET);
	msleep(300);

	sx9500_initialize_chip(data);

	if (atomic_read(&data->enable) == 1)
		sx9500_set_mode(data, SX9500_MODE_NORMAL);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t sx9500_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx9500_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx9500_touch_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->touchMode);
}

static ssize_t sx9500_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 msb, lsb;
	s16 useful;
	u16 offset;
	s32 capMain;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == 0)
		pr_err("[SX9500]: %s - SX9500 was not enabled\n", __func__);

	capMain = sx9500_get_capMain(data);

	sx9500_i2c_write(data, SX9500_REGSENSORSELECT, MAIN_SENSOR);
	sx9500_i2c_read(data, SX9500_REGUSEMSB, &msb);
	sx9500_i2c_read(data, SX9500_REGUSELSB, &lsb);
	useful = (s16)((msb << 8) | lsb);

	sx9500_i2c_read(data, SX9500_REGOFFSETMSB, &msb);
	sx9500_i2c_read(data, SX9500_REGOFFSETLSB, &lsb);
	offset = (u16)((msb << 8) | lsb);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u\n", capMain, useful, offset);
}

static ssize_t sx9500_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			TOUCH_CHECK_THRESHOLD + data->calData);
}

static ssize_t sx9500_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (strict_strtoul(buf, 10, &val)) {
		pr_err("[SX9500]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9500]: %s - touch threshold %lu\n", __func__, val);
	data->touchTh = data->releaseTh = (u8)val;

	return count;
}

static ssize_t sx9500_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->flagDataSkip);
}

static ssize_t sx9500_onoff_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9500_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		pr_err("[SX9500]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (val == 0)
		data->flagDataSkip = true;
	else
		data->flagDataSkip = false;

	pr_info("[SX9500]: %s -%u\n", __func__, val);
	return count;
}

static ssize_t sx9500_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if ((data->calSuccessed == false) && (data->calData == 0))
		ret = CAL_RET_NONE;
	else if ((data->calSuccessed == false) && (data->calData != 0))
		ret = CAL_RET_EXIST;
	else if ((data->calSuccessed == true) && (data->calData != 0))
		ret = CAL_RET_SUCCESS;
	else
		ret = CAL_RET_ERROR;

	pr_info("[SX9500]: %s - ret : %d\n", __func__, ret);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t sx9500_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool do_calib;
	int ret;
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1"))
		do_calib = true;
	else if (sysfs_streq(buf, "0"))
		do_calib = false;
	else {
		pr_info("[SX9500]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ret = sx9500_check_offset(data);
	if (ret < 0) {
		pr_err("[SX9500]: %s - offset fail(%d)\n", __func__, ret);
		goto exit;
	}

	ret = sx9500_do_calibrate(data, do_calib);
	if (ret < 0) {
		pr_err("[SX9500]: %s - sx9500_do_calibrate fail(%d)\n",
			__func__, ret);
		goto exit;
	}

	ret = sx9500_save_caldata(data);
	if (ret < 0) {
		pr_err("[SX9500]: %s - sx9500_save_caldata fail(%d)\n",
			__func__, ret);
		goto exit;
	}

	pr_info("[SX9500]: %s - %u success!\n", __func__, do_calib);

exit:

	if ((data->calData != 0) && (ret >= 0))
		data->calSuccessed = true;
	else
		data->calSuccessed = false;

	return count;
}

static DEVICE_ATTR(menual_calibrate, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9500_get_offset_calibration_show,
		sx9500_set_offset_calibration_store);
static DEVICE_ATTR(register_write, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, sx9500_register_write_store);
static DEVICE_ATTR(register_read, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, sx9500_register_read_store);
static DEVICE_ATTR(readback, S_IRUGO, sx9500_read_data_show, NULL);
static DEVICE_ATTR(reset, S_IRUGO, sx9500_sw_reset_show, NULL);

static DEVICE_ATTR(name, S_IRUGO, sx9500_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx9500_vendor_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, sx9500_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, sx9500_raw_data_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9500_calibration_show, sx9500_calibration_store);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9500_onoff_show, sx9500_onoff_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9500_threshold_show, sx9500_threshold_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_menual_calibrate,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_readback,
	&dev_attr_reset,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_mode,
	&dev_attr_raw_data,
	&dev_attr_threshold,
	&dev_attr_onoff,
	&dev_attr_calibration,
	NULL,
};

/*****************************************************************************/
static ssize_t sx9500_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9500_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9500]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SX9500]: %s - new_value = %u\n", __func__, enable);
	if ((enable == 0) || (enable == 1))
		sx9500_set_enable(data, (int)enable);

	return size;
}

static ssize_t sx9500_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9500_enable_show, sx9500_enable_store);

static struct attribute *sx9500_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group sx9500_attribute_group = {
	.attrs = sx9500_attributes
};

static void sx9500_touch_process(struct sx9500_p *data)
{
	u8 status = 0;
	int cnt;
	s32 capMain, initThd = TOUCH_CHECK_THRESHOLD + data->calData;

	capMain = sx9500_get_capMain(data);

	sx9500_i2c_read(data, SX9500_TCHCMPSTAT_REG, &status);
	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++) {
		if (data->state[cnt] == IDLE) {
			if (status & (CSX_STATUS_REG << cnt))
				send_event(data, cnt, ACTIVE);
			else
				pr_info("[SX9500]: %s - %d already released.\n",
					__func__, cnt);
		} else { /* User released button */
			if (!(status & (CSX_STATUS_REG << cnt))) {
				if ((data->touchMode == INIT_TOUCH_MODE)
					&& (capMain >= initThd))
					pr_info("[SX9500]: %s - IDLE SKIP\n",
						__func__);
				else
					send_event(data, cnt, IDLE);

			} else {
				pr_info("[SX9500]: %s - %d still touched\n",
					__func__, cnt);
			}
		}
	}
}

static void sx9500_process_interrupt(struct sx9500_p *data)
{
	u8 status = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	status = sx9500_read_irqstate(data);

	if (status & IRQ_PROCESS_CONDITION)
		sx9500_touch_process(data);
}

static void sx9500_init_work_func(struct work_struct *work)
{
	struct sx9500_p *data = container_of((struct delayed_work *)work,
		struct sx9500_p, init_work);

	sx9500_initialize_chip(data);
}

static irqreturn_t sx9500_interrupt_thread(int irq, void *pdata)
{
	struct sx9500_p *data = pdata;

	if (sx9500_get_nirq_state(data) == 0)
		sx9500_process_interrupt(data);
	else
		pr_err("[SX9500]: %s - nirq read high %d\n",
			__func__, sx9500_get_nirq_state(data));

	wake_lock_timeout(&data->grip_wake_lock, 3 * HZ);

	return IRQ_HANDLED;
}

static int sx9500_input_init(struct sx9500_p *data)
{
	int ret = 0;
	struct input_dev *dev = NULL;

	/* Create the input device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_MISC);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx9500_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&data->input->dev.kobj,
			data->input->name);
		input_unregister_device(dev);
		return ret;
	}

	/* save the input pointer and finish initialization */
	data->input = dev;

	return 0;
}

static int sx9500_setup_pin(struct sx9500_p *data)
{
	int ret;

	ret = gpio_request(data->gpioNirq, "SX9500_nIRQ");
	if (ret < 0) {
		pr_err("[SX9500]: %s - gpio %d request failed (%d)\n",
			__func__, data->gpioNirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpioNirq);
	if (ret < 0) {
		pr_err("[SX9500]: %s - failed to set gpio %d as input (%d)\n",
			__func__, data->gpioNirq, ret);
		gpio_free(data->gpioNirq);
		return ret;
	}

	data->irq = gpio_to_irq(data->gpioNirq);

	/* initailize interrupt reporting */
	ret = request_threaded_irq(data->irq, NULL, sx9500_interrupt_thread,
			IRQF_TRIGGER_FALLING , "sx9500_irq", data);
	if (ret < 0) {
		pr_err("[SX9500]: %s - failed to set request_threaded_irq %d"
			" as returning (%d)\n", __func__, data->irq, ret);
		free_irq(data->irq, data);
		gpio_free(data->gpioNirq);
		return ret;
	}

	disable_irq(data->irq);
	return 0;
}

static void sx9500_initialize_variable(struct sx9500_p *data)
{
	int cnt;

	for (cnt = 0; cnt < TOTAL_BOTTON_COUNT; cnt++)
		data->state[cnt] = IDLE;

	data->touchTh = DEFAULT_THRESHOLD;
	data->releaseTh = DEFAULT_THRESHOLD;
	data->touchMode = INIT_TOUCH_MODE;
	data->flagDataSkip = false;
	data->calSuccessed = false;
	data->calData = 0;

	atomic_set(&data->enable, 0);
}

static int sx9500_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx9500_p *data = NULL;

	pr_info("[SX9500]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SX9500]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	/* create memory for main struct */
	data = kzalloc(sizeof(struct sx9500_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SX9500]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	data->gpioNirq = client->irq;
	ret = sx9500_setup_pin(data);
	if (ret) {
		pr_err("[SX9500]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

	i2c_set_clientdata(client, data);
	data->client = client;

	/* read chip id */
	ret = sx9500_i2c_write(data, SX9500_SOFTRESET_REG, SX9500_SOFTRESET);
	if (ret < 0) {
		pr_err("[SX9500]: %s - chip reset failed %d\n", __func__, ret);
		goto exit_chip_reset;
	}

	ret = sx9500_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	wake_lock_init(&data->grip_wake_lock,
		WAKE_LOCK_SUSPEND, "grip_wake_lock");

	sensors_register(data->factory_device, data, sensor_attrs, MODULE_NAME);
	sx9500_initialize_variable(data);

	INIT_DELAYED_WORK(&data->init_work, sx9500_init_work_func);
	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));

	pr_info("[SX9500]: %s - Probe done!\n", __func__);

	return 0;

exit_input_init:
exit_chip_reset:
	free_irq(data->irq, data);
	gpio_free(data->gpioNirq);
exit_setup_pin:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[SX9500]: %s - Probe fail!\n", __func__);
	return ret;
}

static int __devexit sx9500_remove(struct i2c_client *client)
{
	struct sx9500_p *data = (struct sx9500_p *)i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == 1)
		sx9500_set_mode(data, SX9500_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	free_irq(data->irq, data);
	gpio_free(data->gpioNirq);

	wake_lock_destroy(&data->grip_wake_lock);
	sensors_unregister(data->factory_device, sensor_attrs);
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
	sysfs_remove_group(&data->input->dev.kobj, &sx9500_attribute_group);
	input_unregister_device(data->input);

	kfree(data);

	return 0;
}

static int sx9500_suspend(struct device *dev)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == 1) {
		pr_info("[SX9500]: %s\n", __func__);
		disable_irq(data->irq);
	}

	return 0;
}

static int sx9500_resume(struct device *dev)
{
	struct sx9500_p *data = dev_get_drvdata(dev);

	if (atomic_read(&data->enable) == 1) {
		pr_info("[SX9500]: %s\n", __func__);
		enable_irq(data->irq);
	}

	return 0;
}

static const struct i2c_device_id sx9500_id[] = {
	{ "sx9500-i2c", 0 },
	{ }
};

static const struct dev_pm_ops sx9500_pm_ops = {
	.suspend = sx9500_suspend,
	.resume = sx9500_resume,
};

static struct i2c_driver sx9500_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.pm = &sx9500_pm_ops
	},
	.probe		= sx9500_probe,
	.remove		= __devexit_p(sx9500_remove),
	.id_table	= sx9500_id,
};

static int __init sx9500_init(void)
{
	return i2c_add_driver(&sx9500_driver);
}

static void __exit sx9500_exit(void)
{
	i2c_del_driver(&sx9500_driver);
}

module_init(sx9500_init);
module_exit(sx9500_exit);

MODULE_DESCRIPTION("Semtech Corp. SX9500 Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
