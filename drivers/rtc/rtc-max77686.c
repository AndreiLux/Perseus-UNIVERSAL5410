/*
 * RTC driver for Maxim MAX77686
 *
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 *
 *  based on rtc-max8997.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/max77686-private.h>

#define DRIVER_NAME "rtc-max77686"
#define DRV_VERSION "0.1"

/* RTC Control Register */
#define RTCCNTL_BCD_DISABLE		(0 << 0)
#define RTCCNTL_BCD_ENABLE		(1 << 0)
#define RTCCNTL_HRMODE_24		(1 << 1)
/* RTC Update Register0 */
#define RTCUPDATE0_UDR_UPDATE		(1 << 0)
#define RTCUPDATE0_RBUDR_UPDATE		(1 << 4)
/* RTC Hour register */
#define RTCHOUR_PM_SEL			(1 << 6)
/* RTC Alarm Enable */
#define RTC_ALARM_ENABLE		(1 << 7)

#define MAX77686_RTC_UPDATE_DELAY	16

enum {
	RTC_SEC = 0,
	RTC_MIN,
	RTC_HOUR,
	RTC_WEEKDAY,
	RTC_MONTH,
	RTC_YEAR,
	RTC_DATE,
	RTC_NR_TIME
};

struct max77686_rtc_info {
	struct device		*dev;
	struct max77686_dev	*max77686;
	struct i2c_client	*rtc;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	int virq;
	int rtc_24hr_mode;
};

enum MAX77686_RTC_OP {
	MAX77686_RTC_WRITE,
	MAX77686_RTC_READ,
};

static int max77686_rtc_calculate_wday(u8 shifted)
{
	int counter = -1;
	while (shifted) {
		shifted >>= 1;
		counter++;
	}
	return counter;
}

static void max77686_rtc_data_to_tm(u8 *data, struct rtc_time *tm,
				   int rtc_24hr_mode)
{
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;
	if (rtc_24hr_mode)
		tm->tm_hour = data[RTC_HOUR] & 0x1f;
	else {
		tm->tm_hour = data[RTC_HOUR] & 0x0f;
		if (data[RTC_HOUR] & RTCHOUR_PM_SEL)
			tm->tm_hour += 12;
	}

	tm->tm_wday = max77686_rtc_calculate_wday(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR] & 0x7f) + 100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static int max77686_rtc_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;
	data[RTC_HOUR] = tm->tm_hour;
	data[RTC_WEEKDAY] = 1 << tm->tm_wday;
	data[RTC_DATE] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0 ;

	if (tm->tm_year < 100) {
		printk(KERN_ERR
		"MAX77686 RTC cannot handle the year %d. Assume it's 2000.\n",
		1900 + tm->tm_year);
		return -EINVAL;
	}
	return 0;
}

static int max77686_rtc_update(struct max77686_rtc_info *info,
	enum MAX77686_RTC_OP op)
{
	int ret;
	u8 data;

	switch (op) {
	case MAX77686_RTC_WRITE:
		data = RTCUPDATE0_UDR_UPDATE;
		break;
	case MAX77686_RTC_READ:
		data = RTCUPDATE0_RBUDR_UPDATE;
		break;
	default:
		BUG();
	}

	ret = max77686_update_reg(info->rtc, MAX77686_RTC_UPDATE0, data, data);
	if (ret < 0)
		dev_err(info->dev, "failed to write update register(%d)\n",
				ret);
	else {
		/*
		 * The Maxim 77686 UM specifies that the maximum transfer time
		 * from the timekeeper counters to the "Read Buffers" or from
		 * "Write Buffers" to the timekeeper counter is 16ms after the
		 * RBUDR/UDR bit is set. Refer to Pg 175 Rev of 0.7 Maxim77686
		 * PMIC datasheet.
		 */
		msleep(MAX77686_RTC_UPDATE_DELAY);
	}

	return ret;
}

static int max77686_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct max77686_rtc_info *info = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	int ret;

	mutex_lock(&info->lock);

	ret = max77686_rtc_update(info, MAX77686_RTC_READ);
	if (ret < 0)
		goto out;

	ret = max77686_bulk_read(info->rtc, MAX77686_RTC_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "failed to read time reg(%d)\n", ret);
		goto out;
	}

	max77686_rtc_data_to_tm(data, tm, info->rtc_24hr_mode);

	ret = rtc_valid_tm(tm);

	dev_dbg(info->dev, "read time %04d/%02d/%02d %02d:%02d:%02d(%d)\n",
	(1900 + tm->tm_year), (tm->tm_mon + 1), tm->tm_mday,
	tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int max77686_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct max77686_rtc_info *info = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	int ret;

	dev_dbg(info->dev, "set time %04d/%02d/%02d %02d:%02d:%02d(%d)\n",
	(1900 + tm->tm_year), (tm->tm_mon + 1), tm->tm_mday,
	tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	ret = max77686_rtc_tm_to_data(tm, data);
	if (ret < 0)
		return ret;

	mutex_lock(&info->lock);

	ret = max77686_bulk_write(info->rtc, MAX77686_RTC_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "failed to write time reg(%d)\n", ret);
		goto out;
	}

	ret = max77686_rtc_update(info, MAX77686_RTC_WRITE);

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int max77686_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct max77686_rtc_info *info = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	u8 val;
	int i, ret;

	mutex_lock(&info->lock);

	ret = max77686_rtc_update(info, MAX77686_RTC_READ);
	if (ret < 0)
		goto out;

	ret = max77686_bulk_read(info->rtc, MAX77686_ALARM1_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to read alarm reg(%d)\n",
				__func__, ret);
		goto out;
	}

	max77686_rtc_data_to_tm(data, &alrm->time, info->rtc_24hr_mode);

	alrm->enabled = 0;
	for (i = 0; i < RTC_NR_TIME; i++) {
		if (data[i] & RTC_ALARM_ENABLE) {
			alrm->enabled = 1;
			break;
		}
	}

	ret = max77686_read_reg(info->rtc, MAX77686_REG_STATUS1, &val);
	if (ret < 0) {
		dev_err(info->dev, "failed to read status1 reg(%d)\n", ret);
		goto out;
	}

	alrm->pending = val & (1 << 4) ? 1 : 0;

out:
	mutex_unlock(&info->lock);
	return 0;
}

static int max77686_rtc_stop_alarm(struct max77686_rtc_info *info)
{
	u8 data[RTC_NR_TIME];
	int ret, i;
	struct rtc_time tm;

	BUG_ON(!mutex_is_locked(&info->lock));

	ret = max77686_rtc_update(info, MAX77686_RTC_READ);
	if (ret < 0)
		goto out;

	ret = max77686_bulk_read(info->rtc, MAX77686_ALARM1_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to read alarm reg(%d)\n",
				__func__, ret);
		goto out;
	}

	max77686_rtc_data_to_tm(data, &tm, info->rtc_24hr_mode);

	for (i = 0; i < RTC_NR_TIME; i++)
		data[i] &= ~RTC_ALARM_ENABLE;

	ret = max77686_bulk_write(info->rtc, MAX77686_ALARM1_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to write alarm reg(%d)\n",
				__func__, ret);
		goto out;
	}

	ret = max77686_rtc_update(info, MAX77686_RTC_WRITE);
out:
	return ret;
}

static int max77686_rtc_start_alarm(struct max77686_rtc_info *info)
{
	u8 data[RTC_NR_TIME];
	int ret;
	struct rtc_time tm;

	BUG_ON(!mutex_is_locked(&info->lock));

	ret = max77686_rtc_update(info, MAX77686_RTC_READ);
	if (ret < 0)
		goto out;

	ret = max77686_bulk_read(info->rtc, MAX77686_ALARM1_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to read alarm reg(%d)\n",
				__func__, ret);
		goto out;
	}

	max77686_rtc_data_to_tm(data, &tm, info->rtc_24hr_mode);

	data[RTC_SEC] |= RTC_ALARM_ENABLE;
	data[RTC_MIN] |= RTC_ALARM_ENABLE;
	data[RTC_HOUR] |= RTC_ALARM_ENABLE;
	data[RTC_WEEKDAY] &= ~RTC_ALARM_ENABLE;
	if (data[RTC_MONTH] & 0xf)
		data[RTC_MONTH] |= RTC_ALARM_ENABLE;
	if (data[RTC_YEAR] & 0x7f)
		data[RTC_YEAR] |= RTC_ALARM_ENABLE;
	if (data[RTC_DATE] & 0x1f)
		data[RTC_DATE] |= RTC_ALARM_ENABLE;

	ret = max77686_bulk_write(info->rtc, MAX77686_ALARM1_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to write alarm reg(%d)\n",
				__func__, ret);
		goto out;
	}

	ret = max77686_rtc_update(info, MAX77686_RTC_WRITE);
out:
	return ret;
}

static int max77686_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct max77686_rtc_info *info = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	int ret;

	ret = max77686_rtc_tm_to_data(&alrm->time, data);
	if (ret < 0)
		return ret;

	mutex_lock(&info->lock);

	ret = max77686_rtc_stop_alarm(info);
	if (ret < 0)
		goto out;

	ret = max77686_bulk_write(info->rtc, MAX77686_ALARM1_SEC, RTC_NR_TIME,
					data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to write alarm reg(%d)\n",
				__func__, ret);
		goto out;
	}

	ret = max77686_rtc_update(info, MAX77686_RTC_WRITE);
	if (ret < 0)
		goto out;

	if (alrm->enabled)
		ret = max77686_rtc_start_alarm(info);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int max77686_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct max77686_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&info->lock);
	if (enabled)
		ret = max77686_rtc_start_alarm(info);
	else
		ret = max77686_rtc_stop_alarm(info);
	mutex_unlock(&info->lock);

	return ret;
}

static irqreturn_t max77686_rtc_alarm_irq(int irq, void *data)
{
	struct max77686_rtc_info *info = data;

	dev_dbg(info->dev, "%s: irq(%d)\n", __func__, irq);

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}

static const struct rtc_class_ops max77686_rtc_ops = {
	.read_time = max77686_rtc_read_time,
	.set_time = max77686_rtc_set_time,
	.read_alarm = max77686_rtc_read_alarm,
	.set_alarm = max77686_rtc_set_alarm,
	.alarm_irq_enable = max77686_rtc_alarm_irq_enable,
};

static int max77686_rtc_init_reg(struct max77686_rtc_info *info)
{
	u8 data[2];
	int ret;

	/* Set RTC control register : Binary mode, 24hour mdoe */
	data[0] = RTCCNTL_BCD_ENABLE | RTCCNTL_HRMODE_24;
	data[1] = RTCCNTL_BCD_DISABLE | RTCCNTL_HRMODE_24;

	info->rtc_24hr_mode = 1;

	ret = max77686_bulk_write(info->rtc, MAX77686_RTC_CONTROLM, 2, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to write controlm reg(%d)\n",
				__func__, ret);
		return ret;
	}

	ret = max77686_rtc_update(info, MAX77686_RTC_WRITE);
	return ret;
}

static int __devinit max77686_rtc_probe(struct platform_device *pdev)
{
	struct max77686_dev *max77686 = dev_get_drvdata(pdev->dev.parent);
	struct max77686_rtc_info *info;
	int ret;

	info = kzalloc(sizeof(struct max77686_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;
	info->max77686 = max77686;
	info->rtc = max77686->rtc;
	info->virq = IRQ_GPIO_END + 1 + MAX77686_RTCIRQ_RTCA1;
	platform_set_drvdata(pdev, info);

	ret = max77686_rtc_init_reg(info);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to initialize RTC reg:%d\n", ret);
		goto err_rtc;
	}

	device_init_wakeup(&pdev->dev, 1);

	info->rtc_dev = rtc_device_register(DRIVER_NAME, &pdev->dev,
			&max77686_rtc_ops, THIS_MODULE);

	if (IS_ERR(info->rtc_dev)) {
		printk(KERN_INFO "%s: fail\n", __func__);

		ret = PTR_ERR(info->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		if (ret == 0)
			ret = -EINVAL;
		goto err_rtc;
	}

	ret = request_threaded_irq(info->virq, NULL, max77686_rtc_alarm_irq, 0,
			"rtc-alarm0", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			info->virq, ret);
		goto err_rtc;
	}

	return 0;
err_rtc:
	kfree(info);
	return ret;
}

static int __devexit max77686_rtc_remove(struct platform_device *pdev)
{
	struct max77686_rtc_info *info = platform_get_drvdata(pdev);

	if (info) {
		free_irq(info->virq, info);
		rtc_device_unregister(info->rtc_dev);
		kfree(info);
	}

	return 0;
}

static const struct platform_device_id rtc_id[] = {
	{ "max77686-rtc", 0 },
	{},
};

static struct platform_driver max77686_rtc_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= max77686_rtc_probe,
	.remove		= __devexit_p(max77686_rtc_remove),
	.id_table	= rtc_id,
};

static int __init max77686_rtc_init(void)
{
	return platform_driver_register(&max77686_rtc_driver);
}
module_init(max77686_rtc_init);

static void __exit max77686_rtc_exit(void)
{
	platform_driver_unregister(&max77686_rtc_driver);
}
module_exit(max77686_rtc_exit);

MODULE_DESCRIPTION("Maxim MAX77686 RTC driver");
MODULE_AUTHOR("Chi-Woong Byun <woong.byun@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_VERSION(DRV_VERSION);
