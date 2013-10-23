/*
 * s2mps11.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps11.h>
#include <mach/sec_debug.h>

#define MANUAL_RESET_CONTROL
#ifdef MANUAL_RESET_CONTROL
#define MANUAL_RESET_ENABLE (1 << 3)
#define MANUAL_RESET_DISABLE ~(1 << 3)
#endif

struct s2mps11_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;
	struct sec_opmode_data *opmode_data;

	int ramp_delay2;
	int ramp_delay34;
	int ramp_delay5;
	int ramp_delay16;
	int ramp_delay7810;
	int ramp_delay9;

	bool buck6_ramp;
	bool buck2_ramp;
	bool buck3_ramp;
	bool buck4_ramp;
	bool buck5_ramp;
#ifdef MANUAL_RESET_CONTROL
	int mrstb_enabled;
	int mrstb_status;
#endif
};

struct s2mps11_voltage_desc {
	int max;
	int min;
	int step;
};

static const struct s2mps11_voltage_desc buck_voltage_val1 = {
	.max = 2000000,
	.min =  600000,
	.step =   6250,
};

static const struct s2mps11_voltage_desc buck_voltage_val2 = {
	.max = 3550000,
	.min =  750000,
	.step =  12500,
};

static const struct s2mps11_voltage_desc buck_voltage_val3 = {
	.max = 3775000,
	.min = 3000000,
	.step =  25000,
};

static const struct s2mps11_voltage_desc ldo_voltage_val1 = {
	.max = 3950000,
	.min =  800000,
	.step =  50000,
};

static const struct s2mps11_voltage_desc ldo_voltage_val2 = {
	.max = 2375000,
	.min =  800000,
	.step =  25000,
};

static const struct s2mps11_voltage_desc *reg_voltage_map[] = {
	[S2MPS11_LDO1] = &ldo_voltage_val2,
	[S2MPS11_LDO2] = &ldo_voltage_val1,
	[S2MPS11_LDO3] = &ldo_voltage_val1,
	[S2MPS11_LDO4] = &ldo_voltage_val1,
	[S2MPS11_LDO5] = &ldo_voltage_val1,
	[S2MPS11_LDO6] = &ldo_voltage_val2,
	[S2MPS11_LDO7] = &ldo_voltage_val1,
	[S2MPS11_LDO8] = &ldo_voltage_val1,
	[S2MPS11_LDO9] = &ldo_voltage_val1,
	[S2MPS11_LDO10] = &ldo_voltage_val1,
	[S2MPS11_LDO11] = &ldo_voltage_val2,
	[S2MPS11_LDO12] = &ldo_voltage_val1,
	[S2MPS11_LDO13] = &ldo_voltage_val1,
	[S2MPS11_LDO14] = &ldo_voltage_val1,
	[S2MPS11_LDO15] = &ldo_voltage_val1,
	[S2MPS11_LDO16] = &ldo_voltage_val1,
	[S2MPS11_LDO17] = &ldo_voltage_val1,
	[S2MPS11_LDO18] = &ldo_voltage_val1,
	[S2MPS11_LDO19] = &ldo_voltage_val1,
	[S2MPS11_LDO20] = &ldo_voltage_val1,
	[S2MPS11_LDO21] = &ldo_voltage_val1,
	[S2MPS11_LDO22] = &ldo_voltage_val2,
	[S2MPS11_LDO23] = &ldo_voltage_val2,
	[S2MPS11_LDO24] = &ldo_voltage_val1,
	[S2MPS11_LDO25] = &ldo_voltage_val1,
	[S2MPS11_LDO26] = &ldo_voltage_val1,
	[S2MPS11_LDO27] = &ldo_voltage_val2,
	[S2MPS11_LDO28] = &ldo_voltage_val1,
	[S2MPS11_LDO29] = &ldo_voltage_val1,
	[S2MPS11_LDO30] = &ldo_voltage_val1,
	[S2MPS11_LDO31] = &ldo_voltage_val1,
	[S2MPS11_LDO32] = &ldo_voltage_val1,
	[S2MPS11_LDO33] = &ldo_voltage_val1,
	[S2MPS11_LDO34] = &ldo_voltage_val1,
	[S2MPS11_LDO35] = &ldo_voltage_val2,
	[S2MPS11_LDO36] = &ldo_voltage_val1,
	[S2MPS11_LDO37] = &ldo_voltage_val1,
	[S2MPS11_LDO38] = &ldo_voltage_val1,
	[S2MPS11_BUCK1] = &buck_voltage_val1,
	[S2MPS11_BUCK2] = &buck_voltage_val1,
	[S2MPS11_BUCK3] = &buck_voltage_val1,
	[S2MPS11_BUCK4] = &buck_voltage_val1,
	[S2MPS11_BUCK5] = &buck_voltage_val1,
	[S2MPS11_BUCK5V123] = &buck_voltage_val1,
	[S2MPS11_BUCK6] = &buck_voltage_val1,
	[S2MPS11_BUCK7] = NULL,
	[S2MPS11_BUCK8] = NULL,
	[S2MPS11_BUCK9] = &buck_voltage_val3,
	[S2MPS11_BUCK10] = &buck_voltage_val2,
};

static int s2mps11_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct s2mps11_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int val;

	if (reg_id >= ARRAY_SIZE(reg_voltage_map) || reg_id < 0)
		return -EINVAL;

	desc = reg_voltage_map[reg_id];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val;
}

unsigned int s2mps11_opmode_reg[][3] = {
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x0, 0x0, 0x0},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x3, 0x1, 0x1},
	{0x1, 0x0, 0x0},
	{0x1, 0x0, 0x0},
	{0x1, 0x0, 0x0},
};

static int s2mps11_get_register(struct regulator_dev *rdev,
	unsigned int *reg, int *pmic_en)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int mode;
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);

	switch (reg_id) {
	case S2MPS11_LDO1 ... S2MPS11_LDO38:
		*reg = S2MPS11_REG_L1CTRL + (reg_id - S2MPS11_LDO1);
		break;
	case S2MPS11_BUCK1 ... S2MPS11_BUCK4:
		*reg = S2MPS11_REG_B1CTRL1 + (reg_id - S2MPS11_BUCK1) * 2;
		break;
	case S2MPS11_BUCK5:
		*reg = S2MPS11_REG_B5CTRL1;
		break;
	case S2MPS11_BUCK5V123:
		mode = s2mps11->opmode_data[reg_id].mode;
		*reg = S2MPS11_REG_BUCK5_SW;
		*pmic_en = s2mps11_opmode_reg[reg_id][mode] << S2MPS11_PMIC_EN_B5V1_SHIFT |
		s2mps11_opmode_reg[reg_id][mode] << S2MPS11_PMIC_EN_B5V2_SHIFT |
		s2mps11_opmode_reg[reg_id][mode] << S2MPS11_PMIC_EN_B5V3_SHIFT;
		return 0;
		break;
	case S2MPS11_BUCK6 ... S2MPS11_BUCK8:
		*reg = S2MPS11_REG_B6CTRL1 + (reg_id - S2MPS11_BUCK6) * 2;
		break;
	case S2MPS11_BUCK9 ... S2MPS11_BUCK10:
		*reg = S2MPS11_REG_B9CTRL1 + (reg_id - S2MPS11_BUCK9) * 2;
		break;
	case S2MPS11_AP_EN32KHZ ... S2MPS11_BT_EN32KHZ:
		*reg = S2MPS11_REG_CTRL1;
		*pmic_en = 0x01 << (reg_id - S2MPS11_AP_EN32KHZ);
		return 0;
	default:
		return -EINVAL;
	}

	mode = s2mps11->opmode_data[reg_id].mode;
	*pmic_en = s2mps11_opmode_reg[reg_id][mode] << S2MPS11_PMIC_EN_SHIFT;

	return 0;
}

static int s2mps11_reg_is_enabled(struct regulator_dev *rdev)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;
	int ret, mask = 0xc0, pmic_en;

	ret = s2mps11_get_register(rdev, &reg, &pmic_en);
	if (ret == -EINVAL)
		return 1;
	else if (ret)
		return ret;

	ret = sec_reg_read(s2mps11->iodev, reg, &val);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS11_LDO1 ... S2MPS11_BUCK5:
		mask = 0xc0;
		break;
	case S2MPS11_BUCK5V123:
		mask = 0x3f;
		break;
	case S2MPS11_BUCK6 ... S2MPS11_BUCK10:
		mask = 0xc0;
		break;
	case S2MPS11_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPS11_CP_EN32KHZ:
		mask = 0x02;
		break;
	case S2MPS11_BT_EN32KHZ:
		mask = 0x04;
		break;
	default:
		return -EINVAL;
	}

	return (val & mask) == pmic_en;
}

static int s2mps11_reg_enable(struct regulator_dev *rdev)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mps11_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS11_LDO1 ... S2MPS11_BUCK5:
		mask = 0xc0;
		break;
	case S2MPS11_BUCK5V123:
		mask = 0x3f;
		break;
	case S2MPS11_BUCK6 ... S2MPS11_BUCK10:
		mask = 0xc0;
		break;
	case S2MPS11_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPS11_CP_EN32KHZ:
		mask = 0x02;
		break;
	case S2MPS11_BT_EN32KHZ:
		mask = 0x04;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mps11->iodev, reg, pmic_en, mask);
}

static int s2mps11_reg_disable(struct regulator_dev *rdev)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mps11_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS11_LDO1 ... S2MPS11_BUCK5:
		mask = 0xc0;
		break;
	case S2MPS11_BUCK5V123:
		mask = 0x3f;
		break;
	case S2MPS11_BUCK6 ... S2MPS11_BUCK10:
		mask = 0xc0;
		break;
	case S2MPS11_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPS11_CP_EN32KHZ:
		mask = 0x02;
		break;
	case S2MPS11_BT_EN32KHZ:
		mask = 0x04;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mps11->iodev, reg, ~mask, mask);
}

static int s2mps11_get_voltage_register(struct regulator_dev *rdev, unsigned int *_reg)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;

	switch (reg_id) {
	case S2MPS11_LDO1 ... S2MPS11_LDO38:
		reg = S2MPS11_REG_L1CTRL + (reg_id - S2MPS11_LDO1);
		break;
	case S2MPS11_BUCK1 ... S2MPS11_BUCK4:
		reg = S2MPS11_REG_B1CTRL2 + (reg_id - S2MPS11_BUCK1) * 2;
		break;
	case S2MPS11_BUCK5 ... S2MPS11_BUCK5V123:
		reg = S2MPS11_REG_B5CTRL2;
		break;
	case S2MPS11_BUCK6 ... S2MPS11_BUCK8:
		reg = S2MPS11_REG_B6CTRL2 + (reg_id - S2MPS11_BUCK6) * 2;
		break;
	case S2MPS11_BUCK9 ... S2MPS11_BUCK10:
		reg = S2MPS11_REG_B9CTRL2 + (reg_id - S2MPS11_BUCK9) * 2;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;

	return 0;
}

static int s2mps11_get_voltage_sel(struct regulator_dev *rdev)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	int mask = 0xff, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;

	ret = s2mps11_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS11_BUCK1 ... S2MPS11_BUCK8:
	case S2MPS11_BUCK10:
		break;
	case S2MPS11_LDO1 ... S2MPS11_LDO38:
		mask = 0x3f;
		break;
	case S2MPS11_BUCK9:
		mask = 0x1f;
		break;
	default:
		return -EINVAL;
	}

	ret = sec_reg_read(s2mps11->iodev, reg, &val);
	if (ret)
		return ret;

	val &= mask;

	return val;
}

static inline int s2mps11_convert_voltage_to_sel(
		const struct s2mps11_voltage_desc *desc,
		int min_vol, int max_vol)
{
	int selector = 0;

	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	selector = (min_vol - desc->min) / desc->step;

	if (desc->min + desc->step * selector > max_vol)
		return -EINVAL;

	return selector;
}

static int s2mps11_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	int min_vol = min_uV, max_vol = max_uV;
	const struct s2mps11_voltage_desc *desc;
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int reg, mask;
	int sel;

	mask = (reg_id < S2MPS11_BUCK1) ? 0x3f : 0xff;

	desc = reg_voltage_map[reg_id];

	sel = s2mps11_convert_voltage_to_sel(desc, min_vol, max_vol);
	if (sel < 0)
		return sel;

	ret = s2mps11_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	ret = sec_reg_update(s2mps11->iodev, reg, sel, mask);
	*selector = sel;

	return ret;
}

static int s2mps11_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	const struct s2mps11_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int ramp_delay = 0;

	switch (reg_id) {
	case S2MPS11_BUCK1:
	case S2MPS11_BUCK6:
		ramp_delay = s2mps11->ramp_delay16;
		break;
	case S2MPS11_BUCK2:
		ramp_delay = s2mps11->ramp_delay2;
		break;
	case S2MPS11_BUCK3 ... S2MPS11_BUCK4:
		ramp_delay = s2mps11->ramp_delay34;
		break;
	case S2MPS11_BUCK5 ... S2MPS11_BUCK5V123:
		ramp_delay = s2mps11->ramp_delay5;
		break;
	case S2MPS11_BUCK7 ... S2MPS11_BUCK8:
	case S2MPS11_BUCK10:
		ramp_delay = s2mps11->ramp_delay7810;
		break;
	case S2MPS11_BUCK9:
		ramp_delay = s2mps11->ramp_delay9;
		break;
	default:
		return -EINVAL;
	}

	desc = reg_voltage_map[reg_id];

	if (((old_sel < new_sel) && (reg_id > S2MPS11_LDO38)) && ramp_delay) {
		return DIV_ROUND_UP(desc->step * (new_sel - old_sel),
			ramp_delay * 1000);
	}

	return 0;
}

static int s2mps11_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);
	int mask = 0xff, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	unsigned int pmic_id, val;
	int cnt = 10000; /* PWM mode transition count */

	ret = s2mps11_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS11_BUCK1 ... S2MPS11_BUCK8:
	case S2MPS11_BUCK10:
		break;
	case S2MPS11_LDO1 ... S2MPS11_LDO38:
		mask = 0x3f;
		break;
	case S2MPS11_BUCK9:
		mask = 0x1f;
		break;
	default:
		return -EINVAL;
	}

	if (reg_id == S2MPS11_BUCK5V123) {
		sec_reg_read(s2mps11->iodev, 0x00, &pmic_id);		/* read pmic_id */
		if (pmic_id < 0x82) {
			sec_reg_update(s2mps11->iodev, 0x2D, 0x0, 0x8); /* PWM mode */

			do {
				sec_reg_read(s2mps11->iodev, 0x2D, &val);
				val = val & 0x01;
			} while(val == 0x00 && cnt-- > 0);
		}
	 }

	ret = sec_reg_update(s2mps11->iodev, reg, selector, mask);

	if (reg_id == S2MPS11_BUCK5V123) {
		if (pmic_id < 0x82)
			sec_reg_update(s2mps11->iodev, 0x2D, 0x8, 0x8);	/* auto mode */
	}

	return ret;
}

#ifdef MANUAL_RESET_CONTROL
extern int get_sec_debug_level(void);

static int s2mps11_set_mrstb_en(struct s2mps11_info *s2mps11, int enable)
{
	int ret= 0;
	unsigned int val;

	if (!(s2mps11->mrstb_enabled))
		return -1;

	sec_reg_read(s2mps11->iodev, S2MPS11_REG_CTRL1, &val);
	pr_info("%s : read S2MPS11_REG_CTRL1 = 0x%x\n", __func__, val);

	if (enable) {
		val= val | MANUAL_RESET_ENABLE;
		s2mps11->mrstb_status = 1;
	} else {
		val= val & MANUAL_RESET_DISABLE;
		s2mps11->mrstb_status = 0;
	}

	pr_info("%s : set S2MPS11_REG_CTRL1 = 0x%x\n", __func__, val);

	ret = sec_reg_update(s2mps11->iodev, S2MPS11_REG_CTRL1, val, 0xF);

	return ret;
}

static ssize_t s2mps11_show_mrstb(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps11_info *s2mps11 = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", s2mps11->mrstb_status);
}

static ssize_t s2mps11_store_mrstb(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t n)
{
	int ret = 0;
	int enable;
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps11_info *s2mps11 = platform_get_drvdata(pdev);

	ret = sscanf(buf, "%d", &enable);

	if (ret != 1 || enable > 1) {
		pr_err("%s : invaild paramater\n",__func__);
		return -EINVAL;
	}

	ret = s2mps11_set_mrstb_en(s2mps11, enable);
	if (ret) {
		pr_err("%s : error : s2mps11_set_mrstb_en\n",__func__);
		return -EPERM;
	}

	pr_info("%s : mrstb_en = %d\n", __func__, enable);

	return n;
}

static DEVICE_ATTR(mrstb, S_IWUSR | S_IRUGO, s2mps11_show_mrstb, \
				 s2mps11_store_mrstb);

static struct attribute *s2mps11_attributes[] = {
	&dev_attr_mrstb.attr,
	NULL
};

static const struct attribute_group s2mps11_group = {
	.attrs = s2mps11_attributes,
};
#endif

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static struct regulator_ops s2mps11_ldo_ops = {
	.list_voltage		= s2mps11_list_voltage,
	.is_enabled		= s2mps11_reg_is_enabled,
	.enable			= s2mps11_reg_enable,
	.disable		= s2mps11_reg_disable,
	.get_voltage_sel	= s2mps11_get_voltage_sel,
	.set_voltage		= s2mps11_set_voltage,
	.set_voltage_time_sel	= s2mps11_set_voltage_time_sel,
};

static struct regulator_ops s2mps11_buck_ops = {
	.list_voltage		= s2mps11_list_voltage,
	.is_enabled		= s2mps11_reg_is_enabled,
	.enable			= s2mps11_reg_enable,
	.disable		= s2mps11_reg_disable,
	.get_voltage_sel	= s2mps11_get_voltage_sel,
	.set_voltage_sel	= s2mps11_set_voltage_sel,
	.set_voltage_time_sel	= s2mps11_set_voltage_time_sel,
};

static struct regulator_ops s2mps11_others_ops = {
	.is_enabled		= s2mps11_reg_is_enabled,
	.enable			= s2mps11_reg_enable,
	.disable		= s2mps11_reg_disable,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,		\
	.id		= S2MPS11_LDO##num,	\
	.ops		= &s2mps11_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}
#define regulator_desc_buck(num)	{	\
	.name		= "BUCK"#num,		\
	.id		= S2MPS11_BUCK##num,	\
	.ops		= &s2mps11_buck_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}

static struct regulator_desc regulators[] = {
	regulator_desc_ldo(1),
	regulator_desc_ldo(2),
	regulator_desc_ldo(3),
	regulator_desc_ldo(4),
	regulator_desc_ldo(5),
	regulator_desc_ldo(6),
	regulator_desc_ldo(7),
	regulator_desc_ldo(8),
	regulator_desc_ldo(9),
	regulator_desc_ldo(10),
	regulator_desc_ldo(11),
	regulator_desc_ldo(12),
	regulator_desc_ldo(13),
	regulator_desc_ldo(14),
	regulator_desc_ldo(15),
	regulator_desc_ldo(16),
	regulator_desc_ldo(17),
	regulator_desc_ldo(18),
	regulator_desc_ldo(19),
	regulator_desc_ldo(20),
	regulator_desc_ldo(21),
	regulator_desc_ldo(22),
	regulator_desc_ldo(23),
	regulator_desc_ldo(24),
	regulator_desc_ldo(25),
	regulator_desc_ldo(26),
	regulator_desc_ldo(27),
	regulator_desc_ldo(28),
	regulator_desc_ldo(29),
	regulator_desc_ldo(30),
	regulator_desc_ldo(31),
	regulator_desc_ldo(32),
	regulator_desc_ldo(33),
	regulator_desc_ldo(34),
	regulator_desc_ldo(35),
	regulator_desc_ldo(36),
	regulator_desc_ldo(37),
	regulator_desc_ldo(38),
	regulator_desc_buck(1),
	regulator_desc_buck(2),
	regulator_desc_buck(3),
	regulator_desc_buck(4),
	regulator_desc_buck(5),
	{
		.name	= "BUCK5V123",
		.id	= S2MPS11_BUCK5V123,
		.ops	= &s2mps11_buck_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	},
	regulator_desc_buck(6),
	regulator_desc_buck(7),
	regulator_desc_buck(8),
	regulator_desc_buck(9),
	regulator_desc_buck(10),
	{
		.name	= "EN32KHz AP",
		.id	= S2MPS11_AP_EN32KHZ,
		.ops	= &s2mps11_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	}, {
		.name	= "EN32KHz CP",
		.id	= S2MPS11_CP_EN32KHZ,
		.ops	= &s2mps11_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	}, {
		.name	= "EN32KHz BT",
		.id	= S2MPS11_BT_EN32KHZ,
		.ops	= &s2mps11_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	},
};

static __devinit int s2mps11_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct s2mps11_info *s2mps11;
	int i, ret, size;
	unsigned int ramp_enable, ramp_reg = 0;

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps11 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps11_info),
				GFP_KERNEL);
	if (!s2mps11)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	s2mps11->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!s2mps11->rdev)
		return -ENOMEM;

	rdev = s2mps11->rdev;
	s2mps11->dev = &pdev->dev;
	s2mps11->iodev = iodev;
	s2mps11->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, s2mps11);

	s2mps11->ramp_delay2 = pdata->buck2_ramp_delay;
	s2mps11->ramp_delay34 = pdata->buck34_ramp_delay;
	s2mps11->ramp_delay5 = pdata->buck5_ramp_delay;
	s2mps11->ramp_delay16 = pdata->buck16_ramp_delay;
	s2mps11->ramp_delay7810 = pdata->buck7810_ramp_delay;
	s2mps11->ramp_delay9 = pdata->buck9_ramp_delay;

	s2mps11->buck6_ramp = pdata->buck6_ramp_enable;
	s2mps11->buck2_ramp = pdata->buck2_ramp_enable;
	s2mps11->buck3_ramp = pdata->buck3_ramp_enable;
	s2mps11->buck4_ramp = pdata->buck4_ramp_enable;
	s2mps11->buck5_ramp = pdata->buck5_ramp_enable;
	s2mps11->opmode_data = pdata->opmode_data;

	ramp_enable = (s2mps11->buck2_ramp << 3) | (s2mps11->buck3_ramp << 2) |
		(s2mps11->buck4_ramp << 1) | s2mps11->buck6_ramp ;

	if (ramp_enable) {
		if (s2mps11->buck2_ramp)
			ramp_reg |= get_ramp_delay(s2mps11->ramp_delay2) << 6;
		if (s2mps11->buck3_ramp || s2mps11->buck4_ramp)
			ramp_reg |= get_ramp_delay(s2mps11->ramp_delay34) << 4;
		sec_reg_update(s2mps11->iodev, S2MPS11_REG_RAMP,
			ramp_reg | ramp_enable, 0xff);
	}

	ramp_reg &= 0x00;
	ramp_reg |= get_ramp_delay(s2mps11->ramp_delay5) << 6;
	ramp_reg |= get_ramp_delay(s2mps11->ramp_delay16) << 4;
	ramp_reg |= get_ramp_delay(s2mps11->ramp_delay7810) << 2;
	ramp_reg |= get_ramp_delay(s2mps11->ramp_delay9);
	sec_reg_update(s2mps11->iodev, S2MPS11_REG_RAMP_BUCK, ramp_reg, 0xff);

	sec_reg_update(s2mps11->iodev, 0x24, 0x03, 0x03);
	sec_reg_update(s2mps11->iodev, 0x63, 0xdd, 0xff);
	sec_reg_update(s2mps11->iodev, 0x64, 0xee, 0xff);
	sec_reg_update(s2mps11->iodev, 0x65, 0xe0, 0xff);
	sec_reg_update(s2mps11->iodev, 0x6a, 0xff, 0xff);
	sec_reg_update(s2mps11->iodev, 0x6b, 0xdf, 0xff);
	sec_reg_update(s2mps11->iodev, 0x6c, 0xff, 0xff);
	sec_reg_update(s2mps11->iodev, 0x6d, 0xff, 0xff);
	sec_reg_update(s2mps11->iodev, 0x79, 0xef, 0xff);
	sec_reg_update(s2mps11->iodev, 0x7a, 0xde, 0xff);
	sec_reg_update(s2mps11->iodev, 0x7b, 0x3f, 0xff);
	sec_reg_update(s2mps11->iodev, 0x7c, 0xc2, 0xff);
	sec_reg_update(s2mps11->iodev, 0x7d, 0xff, 0xff);
	sec_reg_update(s2mps11->iodev, 0x80, 0xff, 0xff);

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct s2mps11_voltage_desc *desc;
		int id = pdata->regulators[i].id;

		desc = reg_voltage_map[id];
		if (desc)
			regulators[id].n_voltages =
				(desc->max - desc->min) / desc->step + 1;

		rdev[i] = regulator_register(&regulators[id], s2mps11->dev,
				pdata->regulators[i].initdata, s2mps11, NULL);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(s2mps11->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

#ifdef MANUAL_RESET_CONTROL
	if(!get_sec_debug_level()) {
		s2mps11->mrstb_enabled = 1;
		s2mps11->mrstb_status = 1;
		ret = sysfs_create_group(&s2mps11->dev->kobj, &s2mps11_group);

		if (ret) {
			dev_err(s2mps11->dev,
				"%s : failed to create sysfs attribute group\n",__func__);
			goto err;
		}
	} else {
		s2mps11->mrstb_enabled = 0;
		s2mps11->mrstb_status = 0;
	}
#endif

	return 0;
err:
	for (i = 0; i < s2mps11->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return ret;
}

static int __devexit s2mps11_pmic_remove(struct platform_device *pdev)
{
	struct s2mps11_info *s2mps11 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = s2mps11->rdev;
	int i;

	for (i = 0; i < s2mps11->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return 0;
}

static const struct platform_device_id s2mps11_pmic_id[] = {
	{ "s2mps11-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps11_pmic_id);

static struct platform_driver s2mps11_pmic_driver = {
	.driver = {
		.name = "s2mps11-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mps11_pmic_probe,
	.remove = __devexit_p(s2mps11_pmic_remove),
	.id_table = s2mps11_pmic_id,
};

static int __init s2mps11_pmic_init(void)
{
	return platform_driver_register(&s2mps11_pmic_driver);
}
subsys_initcall(s2mps11_pmic_init);

static void __exit s2mps11_pmic_exit(void)
{
	platform_driver_unregister(&s2mps11_pmic_driver);
}
module_exit(s2mps11_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS11 Regulator Driver");
MODULE_LICENSE("GPL");
