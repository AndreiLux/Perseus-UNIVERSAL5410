/* linux/arch/arm/mach-exynos/asv.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4 - ASV(Adaptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/map.h>
#include <mach/asv.h>

static struct samsung_asv *exynos_asv;

static int __init exynos_asv_init(void)
{
	exynos_asv = kzalloc(sizeof(struct samsung_asv), GFP_KERNEL);
	if (!exynos_asv)
		return -ENOMEM;

	/* I will add asv driver of exynos4 series to regist */

	if (exynos_asv->check_vdd_arm) {
		if (exynos_asv->check_vdd_arm())
			goto out;
	}

	/* Get HPM Delay value */
	if (exynos_asv->get_hpm) {
		if (exynos_asv->get_hpm(exynos_asv))
			goto out;
	} else
		goto out;

	/* Get IDS ARM Value */
	if (exynos_asv->get_ids) {
		if (exynos_asv->get_ids(exynos_asv))
			goto out;
	} else
		goto out;

	if (exynos_asv->store_result) {
		if (exynos_asv->store_result(exynos_asv))
			goto out;
	} else
		goto out;

	return 0;
out:
	pr_err("EXYNOS : Fail to initialize ASV\n");

	kfree(exynos_asv);

	return -EINVAL;
}
device_initcall_sync(exynos_asv_init);
