/*
 * max77686.c - Regulator driver for the Maxim 77686
 *
 * Copyright (C) 2012 Samsung Electronics
 * Chiwoong Byun <woong.byun@smasung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.c
 */

#include <linux/module.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/max77686.h>
#include <linux/mfd/max77686-private.h>

#define MAX77686_OPMODE_BUCK156789_SHIFT	0x0
#define MAX77686_OPMODE_BUCK234_SHIFT		0x4
#define MAX77686_OPMODE_LDO_SHIFT		0x6
#define MAX77686_OPMODE_MASK			0x3
#define MAX77686_CLOCK_OPMODE_MASK		0x1

struct max77686_data {
	struct device *dev;
	struct max77686_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;
	int ramp_delay;		/* index of ramp_delay */
	int current_val[MAX77686_REG_MAX];	/* current regulator setting,
						   or -1 if unknown */

	/*GPIO-DVS feature is not enabled with the
	 *current version of MAX77686 driver.*/
};

struct voltage_map_desc {
	int min;
	int max;
	int step;
	unsigned int n_bits;
};

/* Voltage maps in mV */
static const struct voltage_map_desc ldo_voltage_map_desc = {
	.min = 800,	.max = 3950,	.step = 50,	.n_bits = 6,
};				/* LDO3 ~ 5, 9 ~ 14, 16 ~ 26 */

static const struct voltage_map_desc ldo_low_voltage_map_desc = {
	.min = 800,	.max = 2375,	.step = 25,	.n_bits = 6,
};				/* LDO1 ~ 2, 6 ~ 8, 15 */

static const struct voltage_map_desc buck_dvs_voltage_map_desc = {
	.min = 600000,	.max = 3787500,	.step = 12500,	.n_bits = 8,
};				/* Buck2, 3, 4 (uV) */

static const struct voltage_map_desc buck_voltage_map_desc = {
	.min = 750,	.max = 3900,	.step = 50,	.n_bits = 6,
};				/* Buck1, 5 ~ 9 */

static const struct voltage_map_desc *reg_voltage_map[] = {
	[MAX77686_LDO1] = &ldo_low_voltage_map_desc,
	[MAX77686_LDO2] = &ldo_low_voltage_map_desc,
	[MAX77686_LDO3] = &ldo_voltage_map_desc,
	[MAX77686_LDO4] = &ldo_voltage_map_desc,
	[MAX77686_LDO5] = &ldo_voltage_map_desc,
	[MAX77686_LDO6] = &ldo_low_voltage_map_desc,
	[MAX77686_LDO7] = &ldo_low_voltage_map_desc,
	[MAX77686_LDO8] = &ldo_low_voltage_map_desc,
	[MAX77686_LDO9] = &ldo_voltage_map_desc,
	[MAX77686_LDO10] = &ldo_voltage_map_desc,
	[MAX77686_LDO11] = &ldo_voltage_map_desc,
	[MAX77686_LDO12] = &ldo_voltage_map_desc,
	[MAX77686_LDO13] = &ldo_voltage_map_desc,
	[MAX77686_LDO14] = &ldo_voltage_map_desc,
	[MAX77686_LDO15] = &ldo_low_voltage_map_desc,
	[MAX77686_LDO16] = &ldo_voltage_map_desc,
	[MAX77686_LDO17] = &ldo_voltage_map_desc,
	[MAX77686_LDO18] = &ldo_voltage_map_desc,
	[MAX77686_LDO19] = &ldo_voltage_map_desc,
	[MAX77686_LDO20] = &ldo_voltage_map_desc,
	[MAX77686_LDO21] = &ldo_voltage_map_desc,
	[MAX77686_LDO22] = &ldo_voltage_map_desc,
	[MAX77686_LDO23] = &ldo_voltage_map_desc,
	[MAX77686_LDO24] = &ldo_voltage_map_desc,
	[MAX77686_LDO25] = &ldo_voltage_map_desc,
	[MAX77686_LDO26] = &ldo_voltage_map_desc,
	[MAX77686_BUCK1] = &buck_voltage_map_desc,
	[MAX77686_BUCK2] = &buck_dvs_voltage_map_desc,
	[MAX77686_BUCK3] = &buck_dvs_voltage_map_desc,
	[MAX77686_BUCK4] = &buck_dvs_voltage_map_desc,
	[MAX77686_BUCK5] = &buck_voltage_map_desc,
	[MAX77686_BUCK6] = &buck_voltage_map_desc,
	[MAX77686_BUCK7] = &buck_voltage_map_desc,
	[MAX77686_BUCK8] = &buck_voltage_map_desc,
	[MAX77686_BUCK9] = &buck_voltage_map_desc,
	[MAX77686_EN32KHZ_AP] = NULL,
	[MAX77686_EN32KHZ_CP] = NULL,
	[MAX77686_P32KH] = NULL,
};

static int max77686_get_voltage_unit(int rid)
{
	int unit = 0;

	switch (rid) {
	case MAX77686_BUCK2...MAX77686_BUCK4:
		unit = 1;	/* BUCK2,3,4 is uV */
		break;
	default:
		unit = 1000;
		break;
	}

	return unit;
}

static int max77686_list_voltage(struct regulator_dev *rdev,
				 unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int rid = rdev_get_id(rdev);
	int val;

	if (rid >= ARRAY_SIZE(reg_voltage_map) || rid < 0)
		return -EINVAL;

	desc = reg_voltage_map[rid];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val * max77686_get_voltage_unit(rid);
}

static int max77686_get_enable_register(struct regulator_dev *rdev,
					int *reg, int *mask, int *pattern)
{
	int rid = rdev_get_id(rdev);
	struct max77686_data *max77686 = rdev_get_drvdata(rdev);
	int i;
	int op_mode;
	for (i = 0; i < max77686->iodev->pdata->num_regulators; i++)
		if (rid == max77686->iodev->pdata->regulators[i].id)
			break;

	if (i == max77686->iodev->pdata->num_regulators) {
		dev_err(max77686->iodev->dev, "No matching regulators\n");
		return -ENODEV;
	}
	op_mode = max77686->iodev->pdata->regulators[i].reg_op_mode;

	switch (rid) {
	case MAX77686_LDO1...MAX77686_LDO26:
		*reg = MAX77686_REG_LDO1CTRL1 + (rid - MAX77686_LDO1);
		*mask = MAX77686_OPMODE_MASK << MAX77686_OPMODE_LDO_SHIFT;
		*pattern = op_mode << MAX77686_OPMODE_LDO_SHIFT;
		break;
	case MAX77686_BUCK1:
		*reg = MAX77686_REG_BUCK1CTRL;
		*mask = MAX77686_OPMODE_MASK << MAX77686_OPMODE_BUCK156789_SHIFT;
		*pattern = op_mode << MAX77686_OPMODE_BUCK156789_SHIFT;
		break;
	case MAX77686_BUCK2...MAX77686_BUCK4:
		*reg = MAX77686_REG_BUCK2CTRL1 + (rid - MAX77686_BUCK2) * 10;
		*mask = MAX77686_OPMODE_MASK << MAX77686_OPMODE_BUCK234_SHIFT;
		*pattern = op_mode << MAX77686_OPMODE_BUCK234_SHIFT;
		break;
	case MAX77686_BUCK5...MAX77686_BUCK9:
		*reg = MAX77686_REG_BUCK5CTRL + (rid - MAX77686_BUCK5) * 2;
		*mask = MAX77686_OPMODE_MASK << MAX77686_OPMODE_BUCK156789_SHIFT;
		*pattern = op_mode << MAX77686_OPMODE_BUCK156789_SHIFT;
		break;
	case MAX77686_EN32KHZ_AP...MAX77686_P32KH:
		*reg = MAX77686_REG_32KHZ_;
		*mask = MAX77686_CLOCK_OPMODE_MASK << (rid - MAX77686_EN32KHZ_AP);
		*pattern = MAX77686_CLOCK_OPMODE_MASK << (rid - MAX77686_EN32KHZ_AP);
		break;
	default:
		/* Not controllable or not exists */
		return -EINVAL;
	}

	return 0;
}

static int max77686_vreg_is_enabled(struct regulator_dev *rdev)
{
	struct max77686_data *max77686 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77686->iodev->i2c;
	int ret, reg, mask, pattern;
	u8 val;

	ret = max77686_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	ret = max77686_read_reg(i2c, reg, &val);
	if (ret)
		return ret;

	return (val & mask) == pattern;
}

static int max77686_vreg_enable(struct regulator_dev *rdev)
{
	struct max77686_data *max77686 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77686->iodev->i2c;
	int ret, reg, mask, pattern;

	ret = max77686_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77686_update_reg(i2c, reg, pattern, mask);
}

static int max77686_vreg_disable(struct regulator_dev *rdev)
{
	struct max77686_data *max77686 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77686->iodev->i2c;
	int ret, reg, mask, pattern;

	ret = max77686_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77686_update_reg(i2c, reg, ~mask, mask);
}

static int max77686_get_voltage_register(struct regulator_dev *rdev,
					 int *_reg, int *_shift, int *_mask)
{
	int rid = rdev_get_id(rdev);
	int reg, shift = 0, mask = 0x3f;

	switch (rid) {
	case MAX77686_LDO1...MAX77686_LDO26:
		reg = MAX77686_REG_LDO1CTRL1 + (rid - MAX77686_LDO1);
		break;
	case MAX77686_BUCK1:
		reg = MAX77686_REG_BUCK1OUT;
		break;
	case MAX77686_BUCK2:
		reg = MAX77686_REG_BUCK2DVS1;
		mask = 0xff;
		break;
	case MAX77686_BUCK3:
		reg = MAX77686_REG_BUCK3DVS1;
		mask = 0xff;
		break;
	case MAX77686_BUCK4:
		reg = MAX77686_REG_BUCK4DVS1;
		mask = 0xff;
		break;
	case MAX77686_BUCK5...MAX77686_BUCK9:
		reg = MAX77686_REG_BUCK5OUT + (rid - MAX77686_BUCK5) * 2;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;
	*_shift = shift;
	*_mask = mask;

	return 0;
}

static int max77686_get_voltage(struct regulator_dev *rdev)
{
	struct max77686_data *max77686 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77686->iodev->i2c;
	int rid = rdev_get_id(rdev);
	int reg, shift, mask, ret;
	u8 val;

	ret = max77686_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77686_read_reg(i2c, reg, &val);
	if (ret) {
		max77686->current_val[rid] = -1;
		return ret;
	}

	val >>= shift;
	val &= mask;

	max77686->current_val[rid] = val;

	return max77686_list_voltage(rdev, val);
}

static inline int max77686_get_voltage_proper_val(const struct voltage_map_desc
						  *desc, int min_vol,
						  int max_vol)
{
	int i = 0;

	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	while (desc->min + desc->step * i < min_vol &&
	       desc->min + desc->step * i < desc->max)
		i++;

	if (desc->min + desc->step * i > max_vol)
		return -EINVAL;

	if (i >= (1 << desc->n_bits))
		return -EINVAL;

	return i;
}

static int max77686_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct max77686_data *max77686 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77686->iodev->i2c;
	int min_vol = min_uV, max_vol = max_uV, unit = 0;
	const struct voltage_map_desc *desc;
	int rid = rdev_get_id(rdev);
	int reg, shift = 0, mask, ret;
	int i;
	int ramp[] = {13750, 27500, 55000, 100000};	/* ramp_rate in uV/uS */
	u8 org;

	unit = max77686_get_voltage_unit(rid);
	min_vol /= unit;
	max_vol /= unit;

	desc = reg_voltage_map[rid];

	i = max77686_get_voltage_proper_val(desc, min_vol, max_vol);
	if (i < 0)
		return i;

	ret = max77686_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	org = max77686->current_val[rid];
	if (org == -1) {
		ret = max77686_read_reg(i2c, reg, &org);
		if (ret)
			return ret;
		org = (org & mask) >> shift;
	}

	ret = max77686_update_reg(i2c, reg, i << shift, mask << shift);
	max77686->current_val[rid] = (ret) ? -1 : i;
	*selector = i;

	if (rid == MAX77686_BUCK2 || rid == MAX77686_BUCK3 ||
	    rid == MAX77686_BUCK4) {
		/* If the voltage is increasing */
		if (org < i)
			udelay(DIV_ROUND_UP(desc->step * (i - org),
					    ramp[max77686->ramp_delay]));
	}

	return ret;
}

static struct regulator_ops max77686_ops = {
	.list_voltage = max77686_list_voltage,
	.is_enabled = max77686_vreg_is_enabled,
	.enable = max77686_vreg_enable,
	.disable = max77686_vreg_disable,
	.get_voltage = max77686_get_voltage,
	.set_voltage = max77686_set_voltage,
	.set_suspend_enable = max77686_vreg_enable,
	.set_suspend_disable = max77686_vreg_disable,
};

static struct regulator_ops max77686_fixedvolt_ops = {
	.list_voltage = max77686_list_voltage,
	.is_enabled = max77686_vreg_is_enabled,
	.enable = max77686_vreg_enable,
	.disable = max77686_vreg_disable,
	.set_suspend_enable = max77686_vreg_enable,
	.set_suspend_disable = max77686_vreg_disable,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,		\
	.id		= MAX77686_LDO##num,	\
	.ops		= &max77686_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}
#define regulator_desc_buck(num)		{	\
	.name		= "BUCK"#num,		\
	.id		= MAX77686_BUCK##num,	\
	.ops		= &max77686_ops,	\
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
	regulator_desc_buck(1),
	regulator_desc_buck(2),
	regulator_desc_buck(3),
	regulator_desc_buck(4),
	regulator_desc_buck(5),
	regulator_desc_buck(6),
	regulator_desc_buck(7),
	regulator_desc_buck(8),
	regulator_desc_buck(9),
	{
		.name = "EN32KHz_AP",
		.id = MAX77686_EN32KHZ_AP,
		.ops = &max77686_fixedvolt_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}, {
		.name = "EN32KHz_CP",
		.id = MAX77686_EN32KHZ_CP,
		.ops = &max77686_fixedvolt_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}, {
		.name = "ENP32KHz",
		.id = MAX77686_P32KH,
		.ops = &max77686_fixedvolt_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_OF
static int max77686_pmic_dt_parse_pdata(struct max77686_dev *iodev,
					struct max77686_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct max77686_regulator_data *rdata;
	unsigned int i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "voltage-regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np)
		pdata->num_regulators++;

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
				"No configuration data for regulator %s\n",
				reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np);
		rdata->reg_node = reg_np;
		if (of_property_read_u32(reg_np, "reg_op_mode",
				&rdata->reg_op_mode)) {
			dev_warn(iodev->dev, "no op_mode property property at %s\n",
				reg_np->full_name);
			/*
			 * Set operating mode to NORMAL "ON" as default. The
			 * 32KHz clocks are being turned on and kept on by
			 * default, so the below mode setting does not impact
			 * it.
			 */
			rdata->reg_op_mode = MAX77686_OPMODE_MASK;
		}
		rdata++;
	}

	if (!of_property_read_u32(pmic_np,
		"max77686,buck_ramp_delay", &i))
		pdata->ramp_delay = i & 0xff;

	return 0;
}
#else
static int max8997_pmic_dt_parse_pdata(struct max8997_dev *iodev,
					struct max8997_platform_data *pdata)
{
	return 0;
}
#endif	/* CONFIG_OF */

#define RAMP_VALUE (max77686->ramp_delay << 6)

static __devinit int max77686_pmic_probe(struct platform_device *pdev)
{
	struct max77686_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77686_platform_data *pdata = iodev->pdata;
	struct regulator_dev **rdev;
	struct max77686_data *max77686;
	struct i2c_client *i2c = iodev->i2c;
	int i, ret, size;

	if (iodev->dev->of_node) {
		ret = max77686_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "platform data not found\n");
		return -ENODEV;
	}

	max77686 = kzalloc(sizeof(struct max77686_data), GFP_KERNEL);
	if (!max77686)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max77686->rdev = kzalloc(size, GFP_KERNEL);
	if (!max77686->rdev) {
		kfree(max77686);
		return -ENOMEM;
	}

	rdev = max77686->rdev;

	max77686->dev = &pdev->dev;
	max77686->iodev = iodev;
	max77686->num_regulators = pdata->num_regulators;

	for (i = 0; i < MAX77686_REG_MAX; i++)
		max77686->current_val[i] = -1;

	if (pdata->ramp_delay) {
		max77686->ramp_delay = pdata->ramp_delay;
		max77686_update_reg(i2c, MAX77686_REG_BUCK2CTRL1,
			RAMP_VALUE, RAMP_MASK);
		max77686_update_reg(i2c, MAX77686_REG_BUCK3CTRL1,
			RAMP_VALUE, RAMP_MASK);
		max77686_update_reg(i2c, MAX77686_REG_BUCK4CTRL1,
			RAMP_VALUE, RAMP_MASK);
	} else {
		/* Default/Reset value is 27.5 mV/uS */
		max77686->ramp_delay = MAX77686_RAMP_RATE_27MV;
	}

	platform_set_drvdata(pdev, max77686);

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct voltage_map_desc *desc;
		int id = pdata->regulators[i].id;

		desc = reg_voltage_map[id];
		if (desc)
			regulators[id].n_voltages =
			    (desc->max - desc->min) / desc->step + 1;

		rdev[i] = regulator_register(&regulators[id], max77686->dev,
					     pdata->regulators[i].initdata,
					     max77686,
					     pdata->regulators[i].reg_node);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(max77686->dev,
				"regulator init failed for id : %d\n", id);
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
 err:
	for (i = 0; i < max77686->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max77686->rdev);
	kfree(max77686);

	return ret;
}

static int __devexit max77686_pmic_remove(struct platform_device *pdev)
{
	struct max77686_data *max77686 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = max77686->rdev;
	int i;

	for (i = 0; i < max77686->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max77686->rdev);
	kfree(max77686);

	return 0;
}

static const struct platform_device_id max77686_pmic_id[] = {
	{"max77686-pmic", 0},
	{},
};

MODULE_DEVICE_TABLE(platform, max77686_pmic_id);

static struct platform_driver max77686_pmic_driver = {
	.driver = {
		   .name = "max77686-pmic",
		   .owner = THIS_MODULE,
		   },
	.probe = max77686_pmic_probe,
	.remove = __devexit_p(max77686_pmic_remove),
	.id_table = max77686_pmic_id,
};

static int __init max77686_pmic_init(void)
{
	return platform_driver_register(&max77686_pmic_driver);
}

subsys_initcall(max77686_pmic_init);

static void __exit max77686_pmic_cleanup(void)
{
	platform_driver_unregister(&max77686_pmic_driver);
}

module_exit(max77686_pmic_cleanup);

MODULE_DESCRIPTION("MAXIM 77686 Regulator Driver");
MODULE_AUTHOR("Chiwoong Byun <woong.byun@samsung.com>");
MODULE_LICENSE("GPL");
