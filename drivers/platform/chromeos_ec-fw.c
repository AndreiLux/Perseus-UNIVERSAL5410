/*
 *  Copyright (C) 2012 Google, Inc
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * Expose the ChromeOS EC firmware information.
 */

#include <linux/module.h>
#include <linux/mfd/chromeos_ec.h>
#include <linux/mfd/chromeos_ec_commands.h>
#include <linux/platform_device.h>

static ssize_t ec_fw_version_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct chromeos_ec_device *ec = dev_get_drvdata(dev);
	struct ec_response_get_version info;
	const char * const copy_name[] = {"?", "RO", "RW"};
	int ret;

	ret = ec->command_recv(ec, EC_CMD_GET_VERSION, &info,
			       sizeof(struct ec_response_get_version));
	if (ret < 0)
		return ret;

	if (info.current_image > EC_IMAGE_RW)
		info.current_image = EC_IMAGE_UNKNOWN;

	return scnprintf(buf, PAGE_SIZE, "Current: %s\nRO: %s\nRW: %s\n",
			 copy_name[info.current_image], info.version_string_ro,
			 info.version_string_rw);
}

static ssize_t ec_build_info_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct chromeos_ec_device *ec = dev_get_drvdata(dev);
	char info[EC_HOST_PARAM_SIZE];
	int ret;

	ret = ec->command_recv(ec, EC_CMD_GET_BUILD_INFO, info, sizeof(info));
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%s\n", info);
}

static ssize_t ec_chip_info_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct chromeos_ec_device *ec = dev_get_drvdata(dev);
	struct ec_response_get_chip_info info;
	int ret;

	ret = ec->command_recv(ec, EC_CMD_GET_CHIP_INFO, &info,
			       sizeof(struct ec_response_get_chip_info));
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%s %s %s\n",
			 info.vendor, info.name, info.revision);
}

static DEVICE_ATTR(fw_version, S_IRUGO, ec_fw_version_show, NULL);
static DEVICE_ATTR(build_info, S_IRUGO, ec_build_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, ec_chip_info_show, NULL);

static struct attribute *ec_fw_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_build_info.attr,
	&dev_attr_chip_info.attr,
	NULL
};

static const struct attribute_group ec_fw_attr_group = {
	.attrs = ec_fw_attrs,
};

static int __devinit ec_fw_probe(struct platform_device *pdev)
{
	struct chromeos_ec_device *ec = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = ec->dev;
	int err;

	err = sysfs_create_group(&dev->kobj, &ec_fw_attr_group);
	if (err) {
		dev_warn(dev, "error creating sysfs entries.\n");
		return err;
	}

	dev_dbg(dev, "Chrome EC Firmware Information\n");

	return 0;
}

static int __devexit ec_fw_remove(struct platform_device *pdev)
{
	struct chromeos_ec_device *ec = dev_get_drvdata(pdev->dev.parent);

	sysfs_remove_group(&ec->dev->kobj, &ec_fw_attr_group);

	return 0;
}

static struct platform_driver ec_fw_driver = {
	.probe = ec_fw_probe,
	.remove = __devexit_p(ec_fw_remove),
	.driver = {
		.name = "cros_ec-fw",
	},
};

module_platform_driver(ec_fw_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC firmware");
MODULE_ALIAS("platform:cros_ec-fw");
