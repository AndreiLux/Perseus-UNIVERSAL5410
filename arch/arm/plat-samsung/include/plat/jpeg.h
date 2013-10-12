/* linux/arch/arm/plat-samsung/include/plat/jpeg.h
 *
 * Copyright 201i Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __PLAT_JPEG_H
#define __PLAT_JPEG_H __FILE__

#include <linux/platform_device.h>

enum jpeg_ip_version {
	IP_VER_JPEG_4P,
	IP_VER_JPEG_5G,
	IP_VER_JPEG_5A,
	IP_VER_JPEG_HX_5A,
	IP_VER_JPEG_HX_5AR,
};

struct exynos_jpeg_platdata {
	int ip_ver;
	const char *gateclk;
	const char *extra_gateclk;
};

static inline int exynos_jpeg_ip_version(struct device *dev)
{
	struct exynos_jpeg_platdata *pdata = dev_get_platdata(dev);

	return pdata->ip_ver;
}

#endif /* __PLAT_JPEG_H */
