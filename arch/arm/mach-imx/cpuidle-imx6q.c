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
#include <linux/cpuidle.h>
#include <linux/export.h>
#include <asm/cpuidle.h>
#include <mach/cpuidle.h>

static struct cpuidle_driver imx6q_cpuidle_driver = {
	.name			= "imx6q_cpuidle",
	.owner			= THIS_MODULE,
	.en_core_tk_irqen	= 1,
	.states[0]		= ARM_CPUIDLE_WFI_STATE,
	.state_count		= 1,
};

int __init imx6q_cpuidle_init(void)
{
	imx_cpuidle_set_driver(&imx6q_cpuidle_driver);

	return 0;
}
