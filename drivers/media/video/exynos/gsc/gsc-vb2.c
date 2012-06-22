/* linux/drivers/media/video/exynos/gsc-vb2.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Videobuf2 allocator operations file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include "gsc-core.h"
#ifdef CONFIG_VIDEOBUF2_SDVMM
void *gsc_sdvmm_init(struct gsc_dev *gsc)
{
	struct vb2_vcm vb2_vcm;
	struct vb2_cma vb2_cma;
	char cma_name[16] = {0,};
	struct vb2_drv vb2_drv;

	gsc->vcm_id = VCM_DEV_GSC0 + gsc->id;

	vb2_vcm.vcm_id = gsc->vcm_id;
	vb2_vcm.size = SZ_64M;

	vb2_cma.dev = &gsc->pdev->dev;
	sprintf(cma_name, "gsc%d", gsc->id);
	vb2_cma.type = cma_name;
	vb2_cma.alignment = SZ_4K;

	vb2_drv.cacheable = true;
	vb2_drv.remap_dva = false;

	return vb2_sdvmm_init(&vb2_vcm, NULL, &vb2_drv);
}

static int gsc_sdvmm_cache_flush(struct vb2_buffer *vb, u32 num_planes)
{

	return vb2_sdvmm_cache_flush(NULL, vb, num_planes);
}

const struct gsc_vb2 gsc_vb2_sdvmm = {
	.ops            = &vb2_sdvmm_memops,
	.init           = gsc_sdvmm_init,
	.cleanup        = vb2_sdvmm_cleanup,
/*	.plane_addr     = vb2_sdvmm_plane_dvaddr, */
	.resume         = vb2_sdvmm_resume,
	.suspend        = vb2_sdvmm_suspend,
	.cache_flush    = gsc_sdvmm_cache_flush,
	.set_cacheable  = vb2_sdvmm_set_cacheable,
/*     .set_sharable   = vb2_sdvmm_set_sharable, */
};
#endif
