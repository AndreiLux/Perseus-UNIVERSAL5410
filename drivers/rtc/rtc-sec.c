/*
 * rtc-sec.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/rtc.h>
#include <linux/mfd/samsung/s2mps11.h>
#include <linux/mfd/samsung/irq.h>
#include <linux/regmap.h>
#if defined(CONFIG_RTC_ALARM_BOOT)
#include <linux/reboot.h>
#include <linux/wakelock.h>
#endif

struct s2mps11_rtc_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	struct rtc_device *rtc_dev;
	int irq;
#if defined(CONFIG_RTC_ALARM_BOOT)
	int irq1;
	bool lpm_mode;
	bool alarm_irq_flag;
	struct wake_lock alarm_wake_lock;
#endif
	int rtc_24hr_mode;
	bool wtsr_smpl;
};

static inline int s2mps11_rtc_calculate_wday(u8 shifted)
{
	int counter = -1;

	while (shifted) {
		shifted >>= 1;
		counter++;
	}
	return counter;
}

static void s2mps11_data_to_tm(u8 *data, struct rtc_time *tm,
				int rtc_24hr_mode)
{
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;

	if (rtc_24hr_mode) {
		tm->tm_hour = data[RTC_HOUR] & 0x1f;
	} else {
		tm->tm_hour = data[RTC_HOUR] & 0x0f;
		if (data[RTC_HOUR] & HOUR_PM_MASK)
			tm->tm_hour += 12;
	}

	tm->tm_wday = s2mps11_rtc_calculate_wday(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR1] & 0x7f)+100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static void s2mps11_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;

	if (tm->tm_hour >= 12)
		data[RTC_HOUR] = tm->tm_hour | HOUR_PM_MASK;
	else
		data[RTC_HOUR] = tm->tm_hour & ~HOUR_PM_MASK;

	data[RTC_WEEKDAY] = 1 << tm->tm_wday;
	data[RTC_DATE] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR1] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0 ;
}

static inline int s2mps11_rtc_set_time_reg(struct s2mps11_rtc_info *info,
					   int alarm_enable)
{
	int ret;
	unsigned int data;

	ret = sec_rtc_read(info->iodev, S2MPS11_RTC_UPDATE, &data);
	if (ret < 0)
		return ret;

	data |= RTC_WUDR_MASK;

	if (alarm_enable)
		data |= RTC_RUDR_MASK;
	else
		data &= ~RTC_RUDR_MASK;

	ret = sec_rtc_write(info->iodev, S2MPS11_RTC_UPDATE, (char)data);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
				__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}
	pr_info("%s: WUDR RUDR check(0x%x) alarm_enable(0x%x)", __func__, (char)data, alarm_enable);

	return ret;
}

static inline int s2mps11_rtc_read_time_reg(struct s2mps11_rtc_info *info)
{
	int ret;
	unsigned int data;

	ret = sec_rtc_read(info->iodev, S2MPS11_RTC_UPDATE, &data);
	if (ret < 0)
		return ret;

	data |= RTC_RUDR_MASK;
	data &= ~RTC_WUDR_MASK;

	ret = sec_rtc_write(info->iodev, S2MPS11_RTC_UPDATE, (char)data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}
	pr_info("%s: RUDR check(0x%x)", __func__, (char)data);

	return ret;
}

static int s2mps11_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	ret = s2mps11_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data, tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	return rtc_valid_tm(tm);
}

static int s2mps11_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	s2mps11_tm_to_data(tm, data);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 0);

	return ret;
}

static int s2mps11_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	unsigned int val;
	int ret, i;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data, &alrm->time, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour,
		alrm->time.tm_min, alrm->time.tm_sec,
		alrm->time.tm_wday);

	alrm->enabled = 0;

	for (i = 0; i < 7; i++) {
		if (data[i] & ALARM_ENABLE_MASK) {
			alrm->enabled = 1;
			break;
		}
	}

	alrm->pending = 0;
	ret = sec_reg_read(info->iodev, S2MPS11_REG_ST2, &val);
	if (ret < 0)
		return ret;

	if (val & RTCA0E)
		alrm->pending = 1;
	else
		alrm->pending = 0;

	return 0;
}

static int s2mps11_rtc_stop_alarm(struct s2mps11_rtc_info *info)
{
	u8 data[7];
	int ret, i;
	struct rtc_time tm;

	ret = s2mps11_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 1);
	return ret;
}

static int s2mps11_rtc_start_alarm(struct s2mps11_rtc_info *info)
{
	int ret;
	u8 data[7];
	struct rtc_time tm;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	data[RTC_SEC] |= ALARM_ENABLE_MASK;
	data[RTC_MIN] |= ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= ALARM_ENABLE_MASK;
	data[RTC_WEEKDAY] |= ALARM_ENABLE_MASK;
	if (data[RTC_DATE] & 0x1f)
		data[RTC_DATE] |= ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0xf)
		data[RTC_MONTH] |= ALARM_ENABLE_MASK;
	if (data[RTC_YEAR1] & 0x7f)
		data[RTC_YEAR1] |= ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 1);

	return ret;
}

static int s2mps11_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	s2mps11_tm_to_data(&alrm->time, data);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour, alrm->time.tm_min,
		alrm->time.tm_sec, alrm->time.tm_wday);

	ret = s2mps11_rtc_stop_alarm(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 1);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		ret = s2mps11_rtc_start_alarm(info);

	return ret;
}

#if defined(CONFIG_RTC_ALARM_BOOT)
static int s2mps11_rtc_stop_alarm_boot(struct s2mps11_rtc_info *info)
{
	u8 data[7];
	int ret, i;
	struct rtc_time tm;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 1);
	return ret;
}

static int s2mps11_rtc_start_alarm_boot(struct s2mps11_rtc_info *info)
{
	int ret;
	u8 data[7];
	struct rtc_time tm;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	data[RTC_SEC] |= ALARM_ENABLE_MASK;
	data[RTC_MIN] |= ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= ALARM_ENABLE_MASK;
	data[RTC_WEEKDAY] &= 0x00;
	if (data[RTC_DATE] & 0x1f)
		data[RTC_DATE] |= ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0xf)
		data[RTC_MONTH] |= ALARM_ENABLE_MASK;
	if (data[RTC_YEAR1] & 0x7f)
		data[RTC_YEAR1] |= ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 1);

	return ret;
}

static int s2mps11_rtc_set_alarm_boot(struct device *dev,
					struct rtc_wkalrm *alrm)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	unsigned int rtcwake;
	int ret;

	s2mps11_tm_to_data(&alrm->time, data);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour, alrm->time.tm_min,
		alrm->time.tm_sec, alrm->time.tm_wday);

	ret = s2mps11_rtc_stop_alarm_boot(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_read(info->iodev, S2MPS11_RTC_UPDATE, &rtcwake);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		rtcwake |= RTC_WAKE_MASK;
	else
		rtcwake &= ~RTC_WAKE_MASK;

	ret = sec_rtc_write(info->iodev, S2MPS11_RTC_UPDATE, (char)rtcwake);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
				__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}

	ret = sec_rtc_bulk_write(info->iodev, S2MPS11_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2mps11_rtc_set_time_reg(info, 1);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		ret = s2mps11_rtc_start_alarm_boot(info);

	return ret;
}

static int s2mps11_rtc_get_alarm_boot(struct device *dev,
				      struct rtc_wkalrm *alrm)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);

	if( info->alarm_irq_flag)
		alrm->enabled = 0x1;
	else
		alrm->enabled = 0x0;
	printk("max77686_rtc_get_alarm_boot : %d, %d\n",
						info->lpm_mode, alrm->enabled);
	return info->lpm_mode;
}
#endif

static int s2mps11_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct s2mps11_rtc_info *info = dev_get_drvdata(dev);

	if (enabled)
		return s2mps11_rtc_start_alarm(info);
	else
		return s2mps11_rtc_stop_alarm(info);
}

static irqreturn_t s2mps11_rtc_alarm_irq(int irq, void *data)
{
	struct s2mps11_rtc_info *info = data;

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}

#if defined(CONFIG_RTC_ALARM_BOOT)
static irqreturn_t s2mps11_rtc_alarm1_irq(int irq, void *data)
{
	struct s2mps11_rtc_info *info = data;

	dev_info(info->dev, "%s:irq(%d), lpm_mode:(%d)\n",
						__func__, irq, info->lpm_mode);

	if (info->lpm_mode) {
		wake_lock(&info->alarm_wake_lock);
		info->alarm_irq_flag = true;
	}

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}
#endif

static const struct rtc_class_ops s2mps11_rtc_ops = {
	.read_time = s2mps11_rtc_read_time,
	.set_time = s2mps11_rtc_set_time,
	.read_alarm = s2mps11_rtc_read_alarm,
	.set_alarm = s2mps11_rtc_set_alarm,
#if defined(CONFIG_RTC_ALARM_BOOT)
	.set_alarm_boot = s2mps11_rtc_set_alarm_boot,
	.get_alarm_boot = s2mps11_rtc_get_alarm_boot,
#endif
	.alarm_irq_enable = s2mps11_rtc_alarm_irq_enable,
};

static void s2mps11_rtc_enable_wtsr(struct s2mps11_rtc_info *info, bool enable)
{
	int ret;
	unsigned int val, mask;

	if (enable)
		val = WTSR_ENABLE_MASK | (WTSR_TIMER_10 << WTSR_TIMER_SHIFT);
	else
		val = 0;

	mask = WTSR_ENABLE_MASK | WTSR_TIMER_MASK;

	dev_info(info->dev, "%s: %s WTSR\n", __func__,
		 enable ? "enable" : "disable");

	ret = sec_rtc_update(info->iodev, S2MPS11_RTC_WTSR_SMPL, val, mask);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update WTSR reg(%d)\n",
			__func__, ret);
		return;
	}
}

static void s2mps11_rtc_enable_smpl(struct s2mps11_rtc_info *info, bool enable)
{
	int ret;
	unsigned int val, mask;

	if (enable)
		val = SMPL_ENABLE_MASK | (SMPL_TIMER_05 << SMPL_TIMER_SHIFT);
	else
		val = 0;

	mask = SMPL_ENABLE_MASK | SMPL_TIMER_MASK;

	dev_info(info->dev, "%s: %s SMPL\n", __func__,
			enable ? "enable" : "disable");

	ret = sec_rtc_update(info->iodev, S2MPS11_RTC_WTSR_SMPL, val, mask);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update SMPL reg(%d)\n",
				__func__, ret);
		return;
	}

	val = 0;
	sec_rtc_read(info->iodev, S2MPS11_RTC_WTSR_SMPL, &val);
	pr_info("%s: WTSR_SMPL(0x%02x)\n", __func__, val);
}

static int s2mps11_rtc_init_reg(struct s2mps11_rtc_info *info)
{
	unsigned int data, tp_read, tp_read1;
	int ret;
	struct rtc_time tm;
#if defined(CONFIG_RTC_ALARM_BOOT)
	u8 data_alm1[7];
	struct rtc_time alrm;

	info->rtc_24hr_mode = 1;

	ret = sec_rtc_bulk_read(info->iodev, S2MPS11_ALARM1_SEC, 7, data_alm1);
	if (ret < 0)
		return ret;

	s2mps11_data_to_tm(data_alm1, &alrm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm.tm_year, 1 + alrm.tm_mon,
		alrm.tm_mday, alrm.tm_hour,
		alrm.tm_min, alrm.tm_sec,
		alrm.tm_wday);
	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		data_alm1[6], data_alm1[5],
		data_alm1[4], data_alm1[2],
		data_alm1[1], data_alm1[0],
		data_alm1[3]);

	ret = sec_rtc_read(info->iodev, S2MPS11_RTC_UPDATE, &data);
	if (ret < 0)
		return ret;
	printk(KERN_INFO "%s: RTC_UPDATE(%d)\n", __func__, data);
#endif

	ret = sec_rtc_read(info->iodev, S2MPS11_RTC_CTRL, &tp_read);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read control reg(%d)\n",
			__func__, ret);
		return ret;
	}

	/* Set RTC control register : Binary mode, 24hour mdoe */
	data = (1 << MODEL24_SHIFT) | tp_read;
	data &= ~(1 << BCD_EN_SHIFT);

	info->rtc_24hr_mode = 1;
	ret = sec_rtc_read(info->iodev, S2MPS11_CAP_SEL, &tp_read1);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read cap sel reg(%d)\n",
			__func__, ret);
		return ret;
	}
	/* In first boot time, Set rtc time to 1/1/2013 00:00:00(SUN) */
	if ((tp_read & MODEL24_MASK) == 0 || (tp_read1 != 0xf8)) {
		dev_info(info->dev, "rtc init\n");
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;
		tm.tm_wday = 2;
		tm.tm_mday = 1;
		tm.tm_mon = 0;
		tm.tm_year = 113;
		tm.tm_yday = 0;
		tm.tm_isdst = 0;
		ret = s2mps11_rtc_set_time(info->dev, &tm);
	}

	ret = sec_rtc_update(info->iodev, S2MPS11_CAP_SEL, 0xf8, 0xff);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update cap sel reg(%d)\n",
			__func__, ret);
		return ret;
	}
#if !defined(CONFIG_RTC_ALARM_BOOT)
	ret = sec_rtc_update(info->iodev, S2MPS11_RTC_UPDATE, 0x00, 0x0C);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update rtc update reg(%d)\n",
			__func__, ret);
		return ret;
	}
#endif

	ret = sec_rtc_update(info->iodev, S2MPS11_RTC_CTRL,
		data, 0x3);

	s2mps11_rtc_set_time_reg(info, 1);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write control reg(%d)\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

static int __devinit s2mps11_rtc_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct s2mps11_rtc_info *s2mps11;
	int ret;

	s2mps11 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps11_rtc_info),
				GFP_KERNEL);
	if (!s2mps11)
		return -ENOMEM;

	s2mps11->dev = &pdev->dev;
	s2mps11->iodev = iodev;

	s2mps11->wtsr_smpl = pdata->wtsr_smpl;

	s2mps11->irq = pdata->irq_base + S2MPS11_IRQ_RTCA0;
#if defined(CONFIG_RTC_ALARM_BOOT)
	s2mps11->irq1 = pdata->irq_base + S2MPS11_IRQ_RTCA1;
#endif

	platform_set_drvdata(pdev, s2mps11);

	ret = s2mps11_rtc_init_reg(s2mps11);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to initialize RTC reg:%d\n", ret);
		goto out_rtc;
	}

	if (s2mps11->wtsr_smpl) {
		s2mps11_rtc_enable_wtsr(s2mps11, true);
		s2mps11_rtc_enable_smpl(s2mps11, true);
	}

	device_init_wakeup(&pdev->dev, true);

	s2mps11->rtc_dev = rtc_device_register("s2mps11-rtc", &pdev->dev,
			&s2mps11_rtc_ops, THIS_MODULE);

	if (IS_ERR(s2mps11->rtc_dev)) {
		ret = PTR_ERR(s2mps11->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out_rtc;
	}

	ret = request_threaded_irq(s2mps11->irq, NULL, s2mps11_rtc_alarm_irq, 0,
			"rtc-alarm0", s2mps11);

	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			s2mps11->irq, ret);

#if defined(CONFIG_RTC_ALARM_BOOT)
	ret = request_threaded_irq(s2mps11->irq1, NULL, s2mps11_rtc_alarm1_irq, 0,
			"rtc-alarm1", s2mps11);

	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			s2mps11->irq1, ret);

	s2mps11->lpm_mode = lpcharge;
	if(s2mps11->lpm_mode)
		wake_lock_init(&s2mps11->alarm_wake_lock, WAKE_LOCK_SUSPEND,
												"alarm_wake_lock");
#endif

	return 0;

out_rtc:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int __devexit s2mps11_rtc_remove(struct platform_device *pdev)
{
	struct s2mps11_rtc_info *s2mps11 = platform_get_drvdata(pdev);

	if (s2mps11) {
		free_irq(s2mps11->irq, s2mps11);
#if defined(CONFIG_RTC_ALARM_BOOT)
		free_irq(s2mps11->irq1, s2mps11);
#endif
		rtc_device_unregister(s2mps11->rtc_dev);
		kfree(s2mps11);
	}

	return 0;
}

static void s2mps11_rtc_shutdown(struct platform_device *pdev)
{
	struct s2mps11_rtc_info *info = platform_get_drvdata(pdev);
	int i;
	unsigned int val = 0;

	if (info->wtsr_smpl) {
		for (i = 0; i < 3; i++) {
			s2mps11_rtc_enable_wtsr(info, false);
			sec_rtc_read(info->iodev,
					S2MPS11_RTC_WTSR_SMPL, &val);
			pr_info("%s: WTSR_SMPL reg(0x%02x)\n", __func__, val);

			if (val & WTSR_ENABLE_MASK) {
				pr_emerg("%s: fail to disable WTSR\n",
				__func__);
			} else {
				pr_info("%s: success to disable WTSR\n",
					__func__);
				break;
			}
		}
	}

	/* Disable SMPL when power off */
	s2mps11_rtc_enable_smpl(info, false);

#if defined(CONFIG_RTC_ALARM_BOOT)
	/* Disable alarm0 interrupt */
	s2mps11_rtc_stop_alarm(info);
#endif
}

static const struct platform_device_id s2mps11_rtc_id[] = {
	{ "s2mps11-rtc", 0 },
};

static struct platform_driver s2mps11_rtc_driver = {
	.driver		= {
		.name	= "s2mps11-rtc",
		.owner	= THIS_MODULE,
	},
	.probe		= s2mps11_rtc_probe,
	.remove		= __devexit_p(s2mps11_rtc_remove),
	.shutdown	= s2mps11_rtc_shutdown,
	.id_table	= s2mps11_rtc_id,
};

static int __init s2mps11_rtc_init(void)
{
	return platform_driver_register(&s2mps11_rtc_driver);
}
module_init(s2mps11_rtc_init);

static void __exit s2mps11_rtc_exit(void)
{
	platform_driver_unregister(&s2mps11_rtc_driver);
}
module_exit(s2mps11_rtc_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("Samsung RTC driver");
MODULE_LICENSE("GPL");
