/* sec_thermistor.c
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <plat/adc.h>
#include <mach/sec_thermistor.h>

#define ADC_SAMPLING_CNT	7

struct sec_therm_info {
	struct device *dev;
	struct sec_therm_platform_data *pdata;
	struct s3c_adc_client *padc;
	struct mutex therm_mutex;
};

static int sec_therm_get_adc_data(struct sec_therm_info *info)
{
	int adc_ch, adc_data;
	int adc_max = 0, adc_min = 0, adc_total = 0;
	int i;

	adc_ch = info->pdata->adc_channel;

	mutex_lock(&info->therm_mutex);
	for (i = 0; i < ADC_SAMPLING_CNT; i++) {
		adc_data = s3c_adc_read(info->padc, adc_ch);

		if (adc_data < 0) {
			dev_err(info->dev, "%s : err(%d) returned, skip read\n",
				__func__, adc_data);
			mutex_unlock(&info->therm_mutex);
			return adc_data;
		}

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}
	mutex_unlock(&info->therm_mutex);

	return (adc_total - adc_max - adc_min) / (ADC_SAMPLING_CNT - 2);
}

static int convert_adc_to_temper(struct sec_therm_info *info, unsigned int adc)
{
	int low = 0;
	int high = 0;
	int mid = 0;

	if (!info->pdata->adc_table || !info->pdata->adc_arr_size) {
		/* using fake temp */
		return 300;
	}

	high = info->pdata->adc_arr_size - 1;

	if (adc >= info->pdata->adc_table[high].adc)
		return info->pdata->adc_table[high].temperature;

	while (low <= high) {
		mid = (low + high) / 2;
		if (info->pdata->adc_table[mid].adc > adc)
			high = mid - 1;
		else if (info->pdata->adc_table[mid + 1].adc <= adc)
			low = mid + 1;
		else
			break;
	}
	return info->pdata->adc_table[mid].temperature;
}

static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;

	adc = sec_therm_get_adc_data(info);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(info, adc);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc;

	adc = sec_therm_get_adc_data(info);

	return sprintf(buf, "%d\n", adc);
}

static DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature, NULL);
static DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL);

static struct attribute *sec_therm_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temp_adc.attr,
	NULL
};

static const struct attribute_group sec_therm_group = {
	.attrs = sec_therm_attributes,
};

static __devinit int sec_therm_probe(struct platform_device *pdev)
{
	struct sec_therm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct sec_therm_info *info;
	int ret = 0;

	dev_dbg(&pdev->dev, "%s: SEC Thermistor Driver Loading\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	info->pdata = pdata;

	mutex_init(&info->therm_mutex);

	info->padc = s3c_adc_register(pdev, NULL, NULL, 0);

	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_group);

	if (ret) {
		dev_err(info->dev,
			"failed to create sysfs attribute group\n");
	}

	return ret;
}

static int __devexit sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);

	if (!info)
		return 0;

	sysfs_remove_group(&info->dev->kobj, &sec_therm_group);

	s3c_adc_release(info->padc);
	kfree(info);

	return 0;
}

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		   .name = "sec-thermistor",
		   .owner = THIS_MODULE,
	},
	.probe = sec_therm_probe,
	.remove = __devexit_p(sec_therm_remove),
};

static int __init sec_therm_init(void)
{
	return platform_driver_register(&sec_thermistor_driver);
}
module_init(sec_therm_init);

static void __exit sec_therm_exit(void)
{
	platform_driver_unregister(&sec_thermistor_driver);
}
module_exit(sec_therm_exit);

MODULE_AUTHOR("ms925.kim@samsung.com");
MODULE_DESCRIPTION("sec thermistor driver");
MODULE_LICENSE("GPL");
