/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/cpuidle.h>
#include <mach/common.h>
#include <mach/cpuidle.h>
#include <mach/hardware.h>

static int imx5_cpuidle_enter(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	imx5_idle();

	return idx;
}

static struct cpuidle_driver imx5_cpuidle_driver = {
	.name			= "imx5_cpuidle",
	.owner			= THIS_MODULE,
	.en_core_tk_irqen	= 1,
	.states[0]	= {
		.enter			= imx5_cpuidle_enter,
		.exit_latency		= 20, /* max latency at 160MHz */
		.target_residency	= 1,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "IMX5 SRPG",
		.desc			= "CPU state retained,powered off",
	},
	.state_count		= 1,
};

int __init imx5_cpuidle_init(void)
{
	imx_cpuidle_set_driver(&imx5_cpuidle_driver);

	return 0;
}
