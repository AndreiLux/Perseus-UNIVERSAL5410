/* linux/drivers/media/video/exynos/rotator/rotator-vb2.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Videobuf2 bridge driver file for Exynos Rotator driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include "rotator.h"

#if defined(CONFIG_VIDEOBUF2_ION)
void *rot_ion_init(struct rot_dev *rot)
{
	struct vb2_ion vb2_ion;
	struct vb2_drv vb2_drv = {0, };

	vb2_ion.dev = &rot->pdev->dev;
	vb2_ion.name = "Rotator";
	vb2_ion.contig = true;
	vb2_ion.cacheable = false;
	vb2_ion.align = SZ_1M;

	vb2_drv.use_mmu = true;

	return vb2_ion_init(&vb2_ion, &vb2_drv);
}

const struct rot_vb2 rot_vb2_ion = {
	.ops		= &vb2_ion_memops,
	.init		= rot_ion_init,
	.cleanup	= vb2_ion_cleanup,
	.plane_addr	= vb2_ion_plane_dvaddr,
	.resume		= vb2_ion_resume,
	.suspend	= vb2_ion_suspend,
	.cache_flush	= vb2_ion_cache_flush,
	.set_cacheable	= vb2_ion_set_cacheable,
	.set_sharable	= vb2_ion_set_sharable,
};
#endif
