/* linux/drivers/media/video/samsung/fimg2d4x/fimg2d_ctx.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <plat/fimg2d.h>
#include "fimg2d.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

static int fimg2d_check_params(struct fimg2d_blit __user *u)
{
	int w, h, i;
	struct fimg2d_scale *s;
	struct fimg2d_param *p = &u->param;
	struct fimg2d_image *m, *um[MAX_IMAGES] = image_table(u);
	struct fimg2d_rect *r;

	if (!u->dst)
		return -1;

	/* DST op makes no effect */
	if (u->op < 0 || u->op == BLIT_OP_DST || u->op >= BLIT_OP_END)
		return -1;

	if (p->scaling.mode) {
		s = &p->scaling;
		if (!s->src_w || !s->src_h || !s->dst_w || !s->dst_h)
			return -1;
	}

	for (i = 0; i < MAX_IMAGES; i++) {
		if (!um[i])
			continue;

		m = um[i];
		r = &m->rect;
		w = m->width;
		h = m->height;

		/* 8000: max width & height */
		if (w > 8000 || h > 8000 ||
			r->x1 < 0 || r->x2 > w ||
			r->y1 < 0 || r->y2 > h ||
			r->x1 == r->x2 || r->y1 == r->y2) {
			return -1;
		}
	}

	return 0;
}

static void fimg2d_fixup_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_param *p = &cmd->param;
	struct fimg2d_scale *s;
	struct fimg2d_image *m;
	struct fimg2d_rect *r;

	m = &cmd->image[IDST];
	r = &m->rect;

	if (r->x2 > m->width)
		r->x2 = m->width;

	if (r->y2 > m->height)
		r->y2 = m->height;

	if (p->scaling.mode) {
		s = &p->scaling;
		if ((s->src_w == s->dst_w) && (s->src_h == s->dst_h))
			s->mode = NO_SCALING;
	}
}

static int fimg2d_check_dma_sync(struct fimg2d_bltcmd *cmd)
{
	struct mm_struct *mm = cmd->ctx->mm;
	struct fimg2d_image *m;
	struct fimg2d_rect *r;
	struct fimg2d_dma *c;
	enum pt_status pt;
	int clip_x, clip_w, clip_h, y, dir, i;
	unsigned long clip_start;

	for (i = 0; i < MAX_IMAGES; i++) {
		m = &cmd->image[i];
		c = &cmd->dma[i];
		r = &m->rect;

		if (m->addr.type == ADDR_NONE)
			continue;

		/* caculate horizontally clipped region */
		c->addr = m->addr.start + (m->stride * r->y1);
		c->size = m->stride * (r->y2 - r->y1);

		/* check pagetable */
		if (m->addr.type == ADDR_USER) {
			pt = fimg2d_check_pagetable(mm, c->addr, c->size);
			if (pt == PT_FAULT)
				return -1;
		}

		if (m->need_cacheopr && i != IMAGE_TMP) {
			c->cached = c->size;
			cmd->dma_all += c->cached;
		}
	}

#ifdef PERF_PROFILE
	perf_start(cmd->ctx, PERF_INNERCACHE);
#endif
	if (is_inner_flushall(cmd->dma_all))
		flush_all_cpu_caches();
	else {
		for (i = 0; i < MAX_IMAGES; i++) {
			m = &cmd->image[i];
			c = &cmd->dma[i];
			r = &m->rect;

			if (m->addr.type == ADDR_NONE)
				continue;

			if (c->cached) {
				if (i == IMAGE_DST)
					dir = DMA_BIDIRECTIONAL;
				else
					dir = DMA_TO_DEVICE;

				clip_w = width2bytes(r->x2 - r->x1, m->fmt);

				if (is_inner_flushrange(m->stride - clip_w))
					fimg2d_dma_sync_inner(c->addr, c->cached, dir);
				else {
					clip_x = pixel2offset(r->x1, m->fmt);
					clip_h = r->y2 - r->y1;
					for (y = 0; y < clip_h; y++) {
						clip_start = c->addr + (m->stride * y) + clip_x;
						fimg2d_dma_sync_inner(clip_start, clip_w, dir);
					}
				}
			}
		}
	}
#ifdef PERF_PROFILE
	perf_end(cmd->ctx, PERF_INNERCACHE);
#endif

#ifdef CONFIG_OUTER_CACHE
#ifdef PERF_PROFILE
	perf_start(cmd->ctx, PERF_OUTERCACHE);
#endif
	if (is_outer_flushall(cmd->dma_all))
		outer_flush_all();
	else {
		for (i = 0; i < MAX_IMAGES; i++) {
			m = &cmd->image[i];
			c = &cmd->dma[i];
			r = &m->rect;

			if (m->addr.type == ADDR_NONE)
				continue;

			/* clean pagetable */
			if (m->addr.type == ADDR_USER)
				fimg2d_clean_outer_pagetable(mm, c->addr, c->size);

			if (c->cached) {
				if (i == IMAGE_DST)
					dir = CACHE_FLUSH;
				else
					dir = CACHE_CLEAN;

				clip_w = width2bytes(r->x2 - r->x1, m->fmt);

				if (is_outer_flushrange(m->stride - clip_w))
					fimg2d_dma_sync_outer(mm, c->addr, c->cached, dir);
				else {
					clip_x = pixel2offset(r->x1, m->fmt);
					clip_h = r->y2 - r->y1;
					for (y = 0; y < clip_h; y++) {
						clip_start = c->addr + (m->stride * y) + clip_x;
						fimg2d_dma_sync_outer(mm, clip_start, clip_w, dir);
					}
				}
			}
		}
	}
#ifdef PERF_PROFILE
	perf_end(cmd->ctx, PERF_OUTERCACHE);
#endif
#endif

	return 0;
}

int fimg2d_add_command(struct fimg2d_control *info, struct fimg2d_context *ctx,
			struct fimg2d_blit __user *u)
{
	int i;
	struct fimg2d_bltcmd *cmd;
	struct fimg2d_image *um[MAX_IMAGES] = image_table(u);

#ifdef CONFIG_VIDEO_FIMG2D_DEBUG
	fimg2d_print_params(u);
#endif

	if (info->err) {
		printk(KERN_ERR "[%s] device error, do sw fallback\n", __func__);
		return -EFAULT;
	}

	if (fimg2d_check_params(u)) {
		printk(KERN_ERR "[%s] invalid params\n", __func__);
		fimg2d_print_params(u);
		return -EINVAL;
	}

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->ctx = ctx;
	cmd->op = u->op;
	cmd->seq_no = u->seq_no;
	cmd->sync = u->sync;

	if (copy_from_user(&cmd->param, &u->param, sizeof(cmd->param)))
		goto err_user;

	for (i = 0; i < MAX_IMAGES; i++) {
		if (!um[i])
			continue;

		if (copy_from_user(&cmd->image[i], um[i], sizeof(cmd->image[i])))
			goto err_user;
	}

	fimg2d_fixup_params(cmd);

	if (fimg2d_check_dma_sync(cmd))
		goto err_user;

	/* add command node and increase ncmd */
	spin_lock(&info->bltlock);
	if (atomic_read(&info->suspended)) {
		fimg2d_debug("fimg2d suspended, do sw fallback\n");
		spin_unlock(&info->bltlock);
		goto err_user;
	}
	atomic_inc(&ctx->ncmd);
	fimg2d_enqueue(&cmd->node, &info->cmd_q);
	fimg2d_debug("ctx %p pgd %p ncmd(%d) seq_no(%u)\n",
			cmd->ctx, (unsigned long *)cmd->ctx->mm->pgd,
			atomic_read(&ctx->ncmd), cmd->seq_no);
	spin_unlock(&info->bltlock);

	return 0;

err_user:
	kfree(cmd);
	return -EFAULT;
}

void fimg2d_add_context(struct fimg2d_control *info, struct fimg2d_context *ctx)
{
	atomic_set(&ctx->ncmd, 0);
	init_waitqueue_head(&ctx->wait_q);

	atomic_inc(&info->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&info->nctx));
}

void fimg2d_del_context(struct fimg2d_control *info, struct fimg2d_context *ctx)
{
	atomic_dec(&info->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&info->nctx));
}
