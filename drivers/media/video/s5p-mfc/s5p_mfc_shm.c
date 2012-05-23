/*
 * linux/drivers/media/video/s5p-mfc/s5p_mfc_shm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef CONFIG_ARCH_EXYNOS4
#include <linux/dma-mapping.h>
#endif
#include <linux/io.h>
#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"

int s5p_mfc_init_shm(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	void *shm_alloc_ctx = dev->alloc_ctx[MFC_BANK1_ALLOC_CTX];
	struct s5p_mfc_buf_size_v5 *buf_size = dev->variant->buf_size->priv;

	ctx->shm.alloc = vb2_dma_contig_memops.alloc(shm_alloc_ctx,
							buf_size->shm);
	if (IS_ERR(ctx->shm.alloc)) {
		mfc_err("failed to allocate shared memory\n");
		return PTR_ERR(ctx->shm.alloc);
	}
	/* shared memory offset only keeps the offset from base (port a) */
	ctx->shm.ofs = s5p_mfc_mem_cookie(shm_alloc_ctx, ctx->shm.alloc)
								- dev->bank1;
	BUG_ON(ctx->shm.ofs & ((1 << MFC_BANK1_ALIGN_ORDER) - 1));
	ctx->shm.virt = vb2_dma_contig_memops.vaddr(ctx->shm.alloc);
	if (!ctx->shm.virt) {
		vb2_dma_contig_memops.put(ctx->shm.alloc);
		ctx->shm.alloc = NULL;
		ctx->shm.ofs = 0;
		mfc_err("failed to virt addr of shared memory\n");
		return -ENOMEM;
	}
	memset((void *)ctx->shm.virt, 0, buf_size->shm);
	wmb();
	return 0;
}

