/*
 *
 * OMAP4460 Plus bandgap on die sensor hwmon driver.
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * J Keerthy <j-keerthy@ti.com>
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
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c/twl.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl4030-madc.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/types.h>
#include <plat/scm.h>
#include <linux/mfd/omap4_scm.h>

static const char *names[3] = { "mpu", "gpu", "core"};

static ssize_t show_name(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id;

	return sprintf(buf, names[id]);
}

static ssize_t show_temp_max(struct device *dev,
			     struct device_attribute *devattr, char *buf)
{
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id;
	int temp;

	temp = omap4460plus_scm_show_temp_max(tsh->scm_ptr, id);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t set_temp_max(struct device *dev,
			    struct device_attribute *devattr,
			    const char *buf, size_t count)
{
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id, ret;
	long val;

	if (strict_strtol(buf, 10, &val))
		return -EINVAL;
	ret = omap4460plus_scm_set_temp_max(tsh->scm_ptr, id, val);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t show_temp_max_hyst(struct device *dev,
				  struct device_attribute *devattr, char *buf)
{
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id;
	int temp;

	temp = omap4460plus_scm_show_temp_max_hyst(tsh->scm_ptr, id);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t set_temp_max_hyst(struct device *dev,
				 struct device_attribute *devattr,
				 const char *buf, size_t count)
{
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id, ret;
	long val;

	if (strict_strtol(buf, 10, &val))
		return -EINVAL;
	ret = omap4460plus_scm_set_temp_max_hyst(tsh->scm_ptr, id, val);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t show_update_interval(struct device *dev,
				    struct device_attribute *devattr, char *buf)
{
	int time = 0;
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id;

	time = omap4460plus_scm_show_update_interval(tsh->scm_ptr, id);

	return sprintf(buf, "%d\n", time);
}

static ssize_t set_update_interval(struct device *dev,
				   struct device_attribute *devattr,
				   const char *buf, size_t count)
{
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	omap4460plus_scm_set_update_interval(tsh->scm_ptr, val, id);
	return count;
}

static int read_temp(struct device *dev,
		     struct device_attribute *devattr, char *buf)
{
	struct temp_sensor_hwmon *tsh = dev_get_drvdata(dev);
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	int id = pdev->id;
	int temp;

	temp = omap4460plus_scm_read_temp(tsh->scm_ptr, id);

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, read_temp, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, show_temp_max,
			  set_temp_max, 0);
static SENSOR_DEVICE_ATTR(temp1_max_hyst, S_IWUSR | S_IRUGO, show_temp_max_hyst,
			  set_temp_max_hyst, 0);
static SENSOR_DEVICE_ATTR(update_interval, S_IWUSR | S_IRUGO,
			  show_update_interval, set_update_interval, 0);

static struct attribute *temp_sensor_attributes[] = {
	&dev_attr_name.attr,

	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&sensor_dev_attr_update_interval.dev_attr.attr,
	NULL
};

static const struct attribute_group temp_sensor_group = {
	.attrs = temp_sensor_attributes,
};

static int __devinit omap4460plus_temp_sensor_probe(struct platform_device
							*pdev)
{
	int ret;
	struct device *hwmon;

	ret = sysfs_create_group(&pdev->dev.kobj, &temp_sensor_group);
	if (ret)
		goto err_sysfs;
	hwmon = hwmon_device_register(&pdev->dev);
	if (IS_ERR(hwmon)) {
		dev_err(&pdev->dev, "hwmon_device_register failed.\n");
		ret = PTR_ERR(hwmon);
		goto err_reg;
	}

	return 0;

err_reg:
	sysfs_remove_group(&pdev->dev.kobj, &temp_sensor_group);
err_sysfs:

	return ret;
}

static int __devexit omap4460plus_temp_sensor_remove(struct platform_device
						     *pdev)
{
	hwmon_device_unregister(&pdev->dev);
	sysfs_remove_group(&pdev->dev.kobj, &temp_sensor_group);

	return 0;
}

static struct platform_driver omap4460plus_temp_sensor_driver = {
	.probe = omap4460plus_temp_sensor_probe,
	.remove = omap4460plus_temp_sensor_remove,
	.driver = {
		   .name = "temp_sensor_hwmon",
		   },
};

static int __init omap4460plus_hwmon_temp_sensor_init(void)
{
	return platform_driver_register(&omap4460plus_temp_sensor_driver);
}

module_init(omap4460plus_hwmon_temp_sensor_init);

static void __exit omap4460plus_hwmon_temp_sensor_exit(void)
{
	platform_driver_unregister(&omap4460plus_temp_sensor_driver);
}

module_exit(omap4460plus_hwmon_temp_sensor_exit);

MODULE_DESCRIPTION("OMAP446X temperature sensor Hwmon Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("J Keerthy <j-keerthy@ti.com>");
