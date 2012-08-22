/*
 * Regulator driver for tps65090 power management chip.
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/tps65090.h>
#include <linux/of.h>

/* TPS65090 has 3 DCDC-regulators and 7 FETs. */

#define MAX_REGULATORS		10
#define MAX_CTRL_READ_TRIES	5

#define CTRL_EN_BIT		0 /* Regulator enable bit, active high */
#define CTRL_WT_BIT		2 /* Regulator wait time 0 bit */
#define CTRL_PG_BIT		4 /* Regulator power good bit, 1=good */
#define CTRL_TO_BIT		7 /* Regulator timeout bit, 1=wait */

#define MAX_OVERCURRENT_WAIT	3 /* Overcurrent wait must be less than this */

struct tps65090_regulator {
	/* Regulator register address.*/
	u32		reg_en_reg;
	u32		reg_overcurrent_wait;

	/* used by regulator core */
	struct regulator_desc	desc;

	struct regulator_dev	*rdev;
};

struct tps65090_regulator_drvdata {
	struct tps65090_regulator *regulators[MAX_REGULATORS];
};

static inline struct device *to_tps65090_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int tps65090_reg_read_ctrl(struct regulator_dev *rdev, uint8_t *ctrl)
{
	struct tps65090_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps65090_dev(rdev);
	int ret;

	ret = tps65090_read(parent, ri->reg_en_reg, ctrl);
	if (ret < 0)
		dev_err(&rdev->dev, "Error in reading reg 0x%x\n",
			ri->reg_en_reg);
	return ret;
}

static int tps65090_reg_is_enabled(struct regulator_dev *rdev)
{
	int ret;
	uint8_t control;

	ret = tps65090_reg_read_ctrl(rdev, &control);
	if (ret < 0)
		return ret;

	return (((control >> CTRL_EN_BIT) & 1) == 1);
}

static int tps65090_reg_set_overcurrent_wait(struct regulator_dev *rdev)
{
	struct tps65090_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps65090_dev(rdev);
	int ret;

	if (ri->reg_overcurrent_wait > MAX_OVERCURRENT_WAIT)
		return 0;

	ret = tps65090_update_bits(parent, ri->reg_en_reg,
			MAX_OVERCURRENT_WAIT << CTRL_WT_BIT,
			ri->reg_overcurrent_wait << CTRL_WT_BIT);
	if (ret)
		dev_err(&rdev->dev, "Error updating overcurrent wait 0x%x\n",
			ri->reg_en_reg);

	return ret;
}

static int tps65090_reg_enable(struct regulator_dev *rdev)
{
	struct tps65090_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps65090_dev(rdev);
	int ret, i;
	uint8_t control;

	ret = tps65090_reg_set_overcurrent_wait(rdev);
	if (ret)
		return ret;

	ret = tps65090_set_bits(parent, ri->reg_en_reg, CTRL_EN_BIT);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in updating reg 0x%x\n",
			ri->reg_en_reg);
		return ret;
	}

	for (i = 0; i < MAX_CTRL_READ_TRIES; i++) {
		ret = tps65090_reg_read_ctrl(rdev, &control);
		if (ret < 0)
			return ret;

		if (!(control & (1 << CTRL_TO_BIT)))
			break;

		usleep_range(1000, 1500);
	}
	if (!(control & (1 << CTRL_PG_BIT))) {
		dev_err(&rdev->dev, "reg 0x%x enable failed\n", ri->reg_en_reg);
		return -EIO;
	}

	return 0;
}

static int tps65090_reg_disable(struct regulator_dev *rdev)
{
	struct tps65090_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps65090_dev(rdev);
	int ret;

	ret = tps65090_clr_bits(parent, ri->reg_en_reg, CTRL_EN_BIT);
	if (ret < 0)
		dev_err(&rdev->dev, "Error in updating reg 0x%x\n",
			ri->reg_en_reg);

	return ret;
}

static int tps65090_set_voltage(struct regulator_dev *rdev, int min,
				int max, unsigned *sel)
{
	/*
	 * Only needed for the core code to set constraints; the voltage
	 * isn't actually adjustable on tps65090.
	 */
	return 0;
}

static struct regulator_ops tps65090_ops = {
	.enable		= tps65090_reg_enable,
	.disable	= tps65090_reg_disable,
	.set_voltage	= tps65090_set_voltage,
	.is_enabled	= tps65090_reg_is_enabled,
};

static void tps65090_unregister_regulators(struct tps65090_regulator *regs[])
{
	int i;

	for (i = 0; i < MAX_REGULATORS; i++)
		if (regs[i]) {
			regulator_unregister(regs[i]->rdev);
			kfree(regs[i]->rdev->desc->name);
			kfree(regs[i]->rdev);
		}
}


static int __devinit tps65090_regulator_probe(struct platform_device *pdev)
{
	struct tps65090_regulator_drvdata *drvdata;
	struct tps65090_regulator *reg;
	struct device_node *mfdnp, *regnp, *np;
	struct regulator_init_data *ri;
	u32 id;

	mfdnp = pdev->dev.parent->of_node;

	if (!mfdnp) {
		dev_err(&pdev->dev, "no device tree data available\n");
		return -EINVAL;
	}

	regnp = of_find_node_by_name(mfdnp, "voltage-regulators");
	if (!regnp) {
		dev_err(&pdev->dev, "no OF regulator data found at %s\n",
			mfdnp->full_name);
		return -EINVAL;
	}

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		of_node_put(regnp);
		return -ENOMEM;
	}

	id = 0;
	for_each_child_of_node(regnp, np) {
		ri = of_get_regulator_init_data(&pdev->dev, np);
		if (!ri) {
			dev_err(&pdev->dev, "regulator_init_data failed for %s\n",
				np->full_name);
			goto out;
		}

		reg = devm_kzalloc(&pdev->dev,
				   sizeof(struct tps65090_regulator),
				   GFP_KERNEL);
		reg->desc.name = kstrdup(of_get_property(np, "regulator-name",
							 NULL), GFP_KERNEL);
		if (!reg->desc.name) {
			dev_err(&pdev->dev,
				"no regulator-name specified at %s\n", np->full_name);
			goto out;
		}

		if (of_property_read_u32(np, "tps65090-control-reg-offset",
					 &reg->reg_en_reg)) {
			dev_err(&pdev->dev,
				"no control-reg-offset property at %s\n",
				np->full_name);
			kfree(reg->desc.name);
			goto out;
		}

		if (of_property_read_u32(np, "tps65090-overcurrent-wait",
					 &reg->reg_overcurrent_wait)) {
			/* Overcurrent wait is optional, set to invalid */
			reg->reg_overcurrent_wait = MAX_OVERCURRENT_WAIT + 1;
		}

		reg->desc.id = id;
		reg->desc.ops = &tps65090_ops;
		reg->desc.type = REGULATOR_VOLTAGE;
		reg->desc.owner = THIS_MODULE;
		reg->rdev = regulator_register(&reg->desc, &pdev->dev,
					       ri, reg, np);
		drvdata->regulators[id] = reg;
		id++;
	}

	platform_set_drvdata(pdev, drvdata);
	of_node_put(regnp);

	return 0;

out:
	dev_err(&pdev->dev, "bad OF regulator data in %s\n", regnp->full_name);
	tps65090_unregister_regulators(drvdata->regulators);
	of_node_put(regnp);
	return -EINVAL;
}

static int __devexit tps65090_regulator_remove(struct platform_device *pdev)
{
	struct tps65090_regulator_drvdata *drvdata = platform_get_drvdata(pdev);

	tps65090_unregister_regulators(drvdata->regulators);

	return 0;
}

static struct platform_driver tps65090_regulator_driver = {
	.driver	= {
		.name	= "tps65090-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= tps65090_regulator_probe,
	.remove		= __devexit_p(tps65090_regulator_remove),
};

static int __init tps65090_regulator_init(void)
{
	return platform_driver_register(&tps65090_regulator_driver);
}
subsys_initcall(tps65090_regulator_init);

static void __exit tps65090_regulator_exit(void)
{
	platform_driver_unregister(&tps65090_regulator_driver);
}
module_exit(tps65090_regulator_exit);

MODULE_DESCRIPTION("tps65090 regulator driver");
MODULE_AUTHOR("Venu Byravarasu <vbyravarasu@nvidia.com>");
MODULE_LICENSE("GPL v2");
