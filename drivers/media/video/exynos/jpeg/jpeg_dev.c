/* linux/drivers/media/video/exynos/jpeg/jpeg_dev.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung Jpeg v2.x Interface driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include <asm/page.h>

#include <mach/irqs.h>

#include <plat/cpu.h>

#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif

#include <media/v4l2-ioctl.h>

#include "jpeg_core.h"
#include "jpeg_dev.h"

#include "jpeg_mem.h"
#include "jpeg_regs.h"
#include "regs_jpeg_v2_x.h"

static int jpeg_dec_queue_setup(struct vb2_queue *vq,
					const struct v4l2_format *fmt, unsigned int *num_buffers,
					unsigned int *num_planes, unsigned int sizes[],
					void *allocators[])
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vq);

	int i;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		*num_planes = ctx->param.dec_param.in_plane;
		for (i = 0; i < ctx->param.dec_param.in_plane; i++) {
			sizes[i] = ctx->param.dec_param.mem_size;
			allocators[i] = ctx->dev->alloc_ctx;
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		*num_planes = ctx->param.dec_param.out_plane;
		for (i = 0; i < ctx->param.dec_param.out_plane; i++) {
			sizes[i] = (ctx->param.dec_param.out_width *
				ctx->param.dec_param.out_height *
				ctx->param.dec_param.out_depth[i]) / 8;
			allocators[i] = ctx->dev->alloc_ctx;
		}
	}

	return 0;
}

static int jpeg_dec_buf_prepare(struct vb2_buffer *vb)
{
	int i;
	int num_plane = 0;

	struct jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		num_plane = ctx->param.dec_param.in_plane;
		ctx->dev->vb2->buf_prepare(vb);
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		num_plane = ctx->param.dec_param.out_plane;
		ctx->dev->vb2->buf_prepare(vb);
	}

	for (i = 0; i < num_plane; i++)
		vb2_set_plane_payload(vb, i, ctx->payload[i]);

	return 0;
}

static int jpeg_dec_buf_finish(struct vb2_buffer *vb)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->dev->vb2->buf_finish(vb);
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->dev->vb2->buf_finish(vb);
	}

	return 0;
}

static void jpeg_dec_buf_queue(struct vb2_buffer *vb)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (ctx->m2m_ctx)
		v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);
}

static void jpeg_dec_lock(struct vb2_queue *vq)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_lock(&ctx->dev->lock);
}

static void jpeg_dec_unlock(struct vb2_queue *vq)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_unlock(&ctx->dev->lock);
}

static int jpeg_enc_queue_setup(struct vb2_queue *vq,
					const struct v4l2_format *fmt, unsigned int *num_buffers,
					unsigned int *num_planes, unsigned int sizes[],
					void *allocators[])
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vq);

	int i;
	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		*num_planes = ctx->param.enc_param.in_plane;
		for (i = 0; i < ctx->param.enc_param.in_plane; i++) {
			sizes[i] = (ctx->param.enc_param.in_width *
				ctx->param.enc_param.in_height *
				ctx->param.enc_param.in_depth[i]) / 8;
			allocators[i] = ctx->dev->alloc_ctx;
		}

	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		*num_planes = ctx->param.enc_param.out_plane;
		for (i = 0; i < ctx->param.enc_param.in_plane; i++) {
			sizes[i] = (ctx->param.enc_param.out_width *
				ctx->param.enc_param.out_height *
				ctx->param.enc_param.out_depth * 2) / 8;
			allocators[i] = ctx->dev->alloc_ctx;
		}
	}

	return 0;
}

static int jpeg_enc_buf_prepare(struct vb2_buffer *vb)
{
	int i;
	int num_plane = 0;

	struct jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		num_plane = ctx->param.enc_param.in_plane;
		ctx->dev->vb2->buf_prepare(vb);
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		num_plane = ctx->param.enc_param.out_plane;
		ctx->dev->vb2->buf_prepare(vb);
	}

	for (i = 0; i < num_plane; i++)
		vb2_set_plane_payload(vb, i, ctx->payload[i]);

	return 0;
}

static int jpeg_enc_buf_finish(struct vb2_buffer *vb)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->dev->vb2->buf_finish(vb);
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->dev->vb2->buf_finish(vb);
	}

	return 0;
}

static void jpeg_enc_buf_queue(struct vb2_buffer *vb)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (ctx->m2m_ctx)
		v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);
}

static void jpeg_enc_lock(struct vb2_queue *vq)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_lock(&ctx->dev->lock);
}

static void jpeg_enc_unlock(struct vb2_queue *vq)
{
	struct jpeg_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_unlock(&ctx->dev->lock);
}

static struct vb2_ops jpeg_enc_vb2_qops = {
	.queue_setup		= jpeg_enc_queue_setup,
	.buf_prepare		= jpeg_enc_buf_prepare,
	.buf_finish		= jpeg_enc_buf_finish,
	.buf_queue		= jpeg_enc_buf_queue,
	.wait_prepare		= jpeg_enc_unlock,
	.wait_finish		= jpeg_enc_lock,
};

static struct vb2_ops jpeg_dec_vb2_qops = {
	.queue_setup		= jpeg_dec_queue_setup,
	.buf_prepare		= jpeg_dec_buf_prepare,
	.buf_finish		= jpeg_dec_buf_finish,
	.buf_queue		= jpeg_dec_buf_queue,
	.wait_prepare		= jpeg_dec_unlock,
	.wait_finish		= jpeg_dec_lock,
};

static inline enum jpeg_node_type jpeg_get_node_type(struct file *file)
{
	struct video_device *vdev = video_devdata(file);

	if (!vdev) {
		jpeg_err("failed to get video_device\n");
		return JPEG_NODE_INVALID;
	}

	jpeg_dbg("video_device index: %d\n", vdev->num);

	if (vdev->num == JPEG_NODE_DECODER)
		return JPEG_NODE_DECODER;
	else if (vdev->num == JPEG_NODE_ENCODER)
		return JPEG_NODE_ENCODER;
	else
		return JPEG_NODE_INVALID;
}

static int queue_init_dec(void *priv, struct vb2_queue *src_vq,
		      struct vb2_queue *dst_vq)
{
	struct jpeg_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &jpeg_dec_vb2_qops;
	src_vq->mem_ops = ctx->dev->vb2->ops;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &jpeg_dec_vb2_qops;
	dst_vq->mem_ops = ctx->dev->vb2->ops;

	return vb2_queue_init(dst_vq);
}

static int queue_init_enc(void *priv, struct vb2_queue *src_vq,
		      struct vb2_queue *dst_vq)
{
	struct jpeg_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &jpeg_enc_vb2_qops;
	src_vq->mem_ops = ctx->dev->vb2->ops;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &jpeg_enc_vb2_qops;
	dst_vq->mem_ops = ctx->dev->vb2->ops;

	return vb2_queue_init(dst_vq);
}
static int jpeg_m2m_open(struct file *file)
{
	struct jpeg_dev *dev = video_drvdata(file);
	struct jpeg_ctx *ctx = NULL;
	int ret = 0;
	enum jpeg_node_type node;

	node = jpeg_get_node_type(file);

	if (node == JPEG_NODE_INVALID) {
		jpeg_err("cannot specify node type\n");
		ret = -ENOENT;
		goto err_node_type;
	}

	ctx = kzalloc(sizeof *ctx, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	file->private_data = ctx;
	ctx->dev = dev;

	spin_lock_init(&ctx->slock);

	if (node == JPEG_NODE_DECODER)
		ctx->m2m_ctx =
			v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
				queue_init_dec);
	else
		ctx->m2m_ctx =
			v4l2_m2m_ctx_init(dev->m2m_dev_enc, ctx,
				queue_init_enc);

	if (IS_ERR(ctx->m2m_ctx)) {
		int err = PTR_ERR(ctx->m2m_ctx);
		kfree(ctx);
		return err;
	}

#ifdef CONFIG_PM_RUNTIME
	if (soc_is_exynos5250() || soc_is_exynos5410()) {
		if (soc_is_exynos5410())
			clk_enable(dev->sclk_clk);
		clk_enable(dev->clk);

	dev->vb2->resume(dev->alloc_ctx);
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_lock(dev->bus_dev, &dev->plat_dev->dev, BUSFREQ_400MHZ);
#endif
	} else
	pm_runtime_get_sync(&dev->plat_dev->dev);
#else
	if (soc_is_exynos5410())
		clk_enable(dev->sclk_clk);
	clk_enable(dev->clk);
	dev->vb2->resume(dev->alloc_ctx);
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_lock(dev->bus_dev, &dev->plat_dev->dev, BUSFREQ_400MHZ);
#endif
#endif

	return 0;

err_node_type:
	kfree(ctx);
	return ret;
}

static int jpeg_m2m_release(struct file *file)
{
	struct jpeg_ctx *ctx = file->private_data;

	v4l2_m2m_ctx_release(ctx->m2m_ctx);

#ifdef CONFIG_PM_RUNTIME
	if (soc_is_exynos5250() || soc_is_exynos5410()) {
	ctx->dev->vb2->suspend(ctx->dev->alloc_ctx);
#ifdef CONFIG_BUSFREQ_OPP
	/* Unlock bus frequency */
	dev_unlock(ctx->dev->bus_dev, &ctx->dev->plat_dev->dev);
#endif
		clk_disable(ctx->dev->clk);
	if (soc_is_exynos5410())
		clk_disable(ctx->dev->sclk_clk);
	} else
	pm_runtime_put_sync(&ctx->dev->plat_dev->dev);
#else
	ctx->dev->vb2->suspend(ctx->dev->alloc_ctx);
#ifdef CONFIG_BUSFREQ_OPP
	/* Unlock bus frequency */
	dev_unlock(ctx->dev->bus_dev, &ctx->dev->plat_dev->dev);
#endif
	clk_disable(ctx->dev->clk);
	if (soc_is_exynos5410())
		clk_disable(ctx->dev->sclk_clk);
#endif

	kfree(ctx);

	return 0;
}

static unsigned int jpeg_m2m_poll(struct file *file,
				     struct poll_table_struct *wait)
{
	struct jpeg_ctx *ctx = file->private_data;

	return v4l2_m2m_poll(file, ctx->m2m_ctx, wait);
}


static int jpeg_m2m_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct jpeg_ctx *ctx = file->private_data;

	return v4l2_m2m_mmap(file, ctx->m2m_ctx, vma);
}

static const struct v4l2_file_operations jpeg_fops = {
	.owner		= THIS_MODULE,
	.open		= jpeg_m2m_open,
	.release		= jpeg_m2m_release,
	.poll			= jpeg_m2m_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap		= jpeg_m2m_mmap,
};

static struct video_device jpeg_enc_videodev = {
	.name = JPEG_ENC_NAME,
	.fops = &jpeg_fops,
	.minor = 12,
	.release = video_device_release,
};

static struct video_device jpeg_dec_videodev = {
	.name = JPEG_DEC_NAME,
	.fops = &jpeg_fops,
	.minor = 11,
	.release = video_device_release,
};

static void jpeg_device_enc_run(void *priv)
{
	struct jpeg_ctx *ctx = priv;
	struct jpeg_dev *dev = ctx->dev;
	struct jpeg_enc_param enc_param;
	struct vb2_buffer *vb = NULL;
	unsigned long flags;

	dev = ctx->dev;
	spin_lock_irqsave(&ctx->slock, flags);

	dev->mode = ENCODING;
	enc_param = ctx->param.enc_param;

	jpeg_sw_reset(dev->reg_base);
	jpeg_set_interrupt(dev->reg_base);
	jpeg_set_huf_table_enable(dev->reg_base, 1);
	jpeg_set_enc_tbl(dev->reg_base, enc_param.quality);
	jpeg_set_encode_tbl_select(dev->reg_base, enc_param.quality);
	jpeg_set_stream_size(dev->reg_base,
		enc_param.in_width, enc_param.in_height);
	jpeg_set_enc_out_fmt(dev->reg_base, enc_param.out_fmt);
	jpeg_set_enc_in_fmt(dev->reg_base, enc_param.in_fmt);
	vb = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	jpeg_set_stream_buf_address(dev->reg_base, dev->vb2->plane_addr(vb, 0));

	vb = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	jpeg_set_frame_buf_address(dev->reg_base,
	enc_param.in_fmt, dev->vb2->plane_addr(vb, 0), enc_param.in_width, enc_param.in_height);

	jpeg_set_encode_hoff_cnt(dev->reg_base, enc_param.out_fmt);

	jpeg_set_timer_count(dev->reg_base, enc_param.in_width * enc_param.in_height * 32 + 0xff);
	jpeg_set_enc_dec_mode(dev->reg_base, ENCODING);

	spin_unlock_irqrestore(&ctx->slock, flags);
}

static void jpeg_device_dec_run(void *priv)
{
	struct jpeg_ctx *ctx = priv;
	struct jpeg_dev *dev = ctx->dev;
	struct jpeg_dec_param dec_param;
	struct vb2_buffer *vb = NULL;
	unsigned long flags;

	dev = ctx->dev;

	spin_lock_irqsave(&ctx->slock, flags);

	dev->mode = DECODING;
	dec_param = ctx->param.dec_param;

	jpeg_sw_reset(dev->reg_base);
	jpeg_set_interrupt(dev->reg_base);

	jpeg_set_encode_tbl_select(dev->reg_base, 0);

	vb = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	jpeg_set_stream_buf_address(dev->reg_base, dev->vb2->plane_addr(vb, 0));

	vb = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	jpeg_set_frame_buf_address(dev->reg_base,
	dec_param.out_fmt, dev->vb2->plane_addr(vb, 0), dec_param.in_width, dec_param.in_height);

	if (dec_param.out_width > 0 && dec_param.out_height > 0) {
		if ((dec_param.out_width * 2 == dec_param.in_width) &&
			(dec_param.out_height * 2 == dec_param.in_height))
			jpeg_set_dec_scaling(dev->reg_base, JPEG_SCALE_2, JPEG_SCALE_2);
		else if ((dec_param.out_width * 4 == dec_param.in_width) &&
			(dec_param.out_height * 4 == dec_param.in_height))
			jpeg_set_dec_scaling(dev->reg_base, JPEG_SCALE_4, JPEG_SCALE_4);
		else
			jpeg_set_dec_scaling(dev->reg_base, JPEG_SCALE_NORMAL, JPEG_SCALE_NORMAL);
	}

	jpeg_set_dec_out_fmt(dev->reg_base, dec_param.out_fmt);
	jpeg_set_dec_bitstream_size(dev->reg_base, dec_param.size);
	jpeg_set_timer_count(dev->reg_base, dec_param.in_width * dec_param.in_height * 8 + 0xff);
	jpeg_set_enc_dec_mode(dev->reg_base, DECODING);

	spin_unlock_irqrestore(&ctx->slock, flags);
}

static void jpeg_job_enc_abort(void *priv) { }

static void jpeg_job_dec_abort(void *priv) { }

static struct v4l2_m2m_ops jpeg_m2m_enc_ops = {
	.device_run	= jpeg_device_enc_run,
	.job_abort	= jpeg_job_enc_abort,
};

static struct v4l2_m2m_ops jpeg_m2m_dec_ops = {
	.device_run	= jpeg_device_dec_run,
	.job_abort	= jpeg_job_dec_abort,
};

int jpeg_int_pending(struct jpeg_dev *ctrl)
{
	unsigned int	int_status;

	int_status = jpeg_get_int_status(ctrl->reg_base);
	jpeg_dbg("state(%d)\n", int_status);

	return int_status;
}

static irqreturn_t jpeg_irq(int irq, void *priv)
{
	unsigned int int_status;
	struct vb2_buffer *src_vb, *dst_vb;
	struct jpeg_dev *ctrl = priv;
	struct jpeg_ctx *ctx;
	unsigned long payload_size = 0;

	jpeg_clean_interrupt(ctrl->reg_base);

	if (ctrl->mode == ENCODING)
		ctx = v4l2_m2m_get_curr_priv(ctrl->m2m_dev_enc);
	else
		ctx = v4l2_m2m_get_curr_priv(ctrl->m2m_dev_dec);

	if (ctx == 0) {
		printk(KERN_ERR "ctx is null.\n");
		jpeg_sw_reset(ctrl->reg_base);
		goto ctx_err;
	}

	spin_lock(&ctx->slock);

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	int_status = jpeg_int_pending(ctrl);

	if (int_status) {
		switch (int_status & 0x1ff) {
		case 0x1:
			ctrl->irq_ret = ERR_PROT;
			break;
		case 0x2:
			ctrl->irq_ret = OK_ENC_OR_DEC;
			break;
		case 0x4:
			ctrl->irq_ret = ERR_DEC_INVALID_FORMAT;
			break;
		case 0x8:
			ctrl->irq_ret = ERR_MULTI_SCAN;
			break;
		case 0x10:
			ctrl->irq_ret = ERR_FRAME;
			break;
		case 0x20:
			ctrl->irq_ret = ERR_TIME_OUT;
			break;
		default:
			ctrl->irq_ret = ERR_UNKNOWN;
			break;
		}
	} else {
		ctrl->irq_ret = ERR_UNKNOWN;
	}

	if (ctrl->irq_ret == OK_ENC_OR_DEC) {
		if (ctrl->mode == ENCODING) {
			payload_size = jpeg_get_stream_size(ctrl->reg_base);
			vb2_set_plane_payload(dst_vb, 0, payload_size);
		}
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);
	} else {
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);
	}

	if (ctrl->mode == ENCODING)
		v4l2_m2m_job_finish(ctrl->m2m_dev_enc, ctx->m2m_ctx);
	else
		v4l2_m2m_job_finish(ctrl->m2m_dev_dec, ctx->m2m_ctx);

	spin_unlock(&ctx->slock);
ctx_err:
	return IRQ_HANDLED;
}

static int jpeg_setup_controller(struct jpeg_dev *ctrl)
{
	mutex_init(&ctrl->lock);
	init_waitqueue_head(&ctrl->wq);

	return 0;
}

static int jpeg_probe(struct platform_device *pdev)
{
	struct jpeg_dev *dev;
	struct video_device *vfd;
	struct resource *res;
	int ret;

	/* global structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "%s: not enough memory\n",
			__func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

	dev->plat_dev = pdev;

	/* setup jpeg control */
	ret = jpeg_setup_controller(dev);
	if (ret) {
		jpeg_err("failed to setup controller\n");
		goto err_setup;
	}

	/* memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		jpeg_err("failed to get jpeg memory region resource\n");
		ret = -ENOENT;
		goto err_res;
	}

	res = request_mem_region(res->start, resource_size(res),
				pdev->name);
	if (!res) {
		jpeg_err("failed to request jpeg io memory region\n");
		ret = -ENOMEM;
		goto err_region;
	}

	/* ioremap */
	dev->reg_base = ioremap(res->start, resource_size(res));
	if (!dev->reg_base) {
		jpeg_err("failed to remap jpeg io region\n");
		ret = -ENOENT;
		goto err_map;
	}

	/* irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		jpeg_err("failed to request jpeg irq resource\n");
		ret = -ENOENT;
		goto err_irq;
	}

	dev->irq_no = res->start;
	ret = request_irq(dev->irq_no, (void *)jpeg_irq,
			IRQF_DISABLED, pdev->name, dev);
	if (ret != 0) {
		jpeg_err("failed to jpeg request irq\n");
		ret = -ENOENT;
		goto err_irq;
	}

	/* clock */
	dev->clk = clk_get(&pdev->dev, "jpeg");
	if (IS_ERR(dev->clk)) {
		jpeg_err("failed to find jpeg clock source\n");
		ret = -ENOENT;
		goto err_clk;
	}
	if (soc_is_exynos5410()) {
		dev->sclk_clk = clk_get(&pdev->dev, "sclk_jpeg");
		if (IS_ERR(dev->sclk_clk)) {
			jpeg_err("failed to find jpeg clock source for sclk_jpeg\n");
			ret = -ENOENT;
			goto err_sclk_clk;
		}
	}
#ifdef CONFIG_PM_RUNTIME
	if (soc_is_exynos4212() || soc_is_exynos4412())
		pm_runtime_enable(&pdev->dev);
	else if (soc_is_exynos5250() || soc_is_exynos5410()) {
		pm_runtime_enable(&pdev->dev);
		pm_runtime_get_sync(&pdev->dev);
	}
#endif

	/* clock enable */
	if (soc_is_exynos5410())
		clk_enable(dev->sclk_clk);
	clk_enable(dev->clk);

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register v4l2 device\n");
		goto err_v4l2;
	}

	/* encoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_vd_alloc_enc;
	}

	*vfd = jpeg_enc_videodev;
	vfd->ioctl_ops = get_jpeg_enc_v4l2_ioctl_ops();
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, 12);
	if (ret) {
		v4l2_err(&dev->v4l2_dev,
			 "%s(): failed to register video device\n", __func__);
		video_device_release(vfd);
		goto err_vd_alloc_enc;
	}
	v4l2_info(&dev->v4l2_dev,
		"JPEG driver is registered to /dev/video%d\n", vfd->num);

	dev->vfd_enc = vfd;
	dev->m2m_dev_enc = v4l2_m2m_init(&jpeg_m2m_enc_ops);
	if (IS_ERR(dev->m2m_dev_enc)) {
		v4l2_err(&dev->v4l2_dev,
			"failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(dev->m2m_dev_enc);
		goto err_m2m_init_enc;
	}
	video_set_drvdata(vfd, dev);

	/* decoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_vd_alloc_dec;
	}

	*vfd = jpeg_dec_videodev;
	vfd->ioctl_ops = get_jpeg_dec_v4l2_ioctl_ops();
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, 11);
	if (ret) {
		v4l2_err(&dev->v4l2_dev,
			 "%s(): failed to register video device\n", __func__);
		video_device_release(vfd);
		goto err_vd_alloc_dec;
	}
	v4l2_info(&dev->v4l2_dev,
		"JPEG driver is registered to /dev/video%d\n", vfd->num);

	dev->vfd_dec = vfd;
	dev->m2m_dev_dec = v4l2_m2m_init(&jpeg_m2m_dec_ops);
	if (IS_ERR(dev->m2m_dev_dec)) {
		v4l2_err(&dev->v4l2_dev,
			"failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(dev->m2m_dev_dec);
		goto err_m2m_init_dec;
	}
	video_set_drvdata(vfd, dev);

	platform_set_drvdata(pdev, dev);

#ifdef CONFIG_VIDEOBUF2_CMA_PHYS
	dev->vb2 = &jpeg_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
	dev->vb2 = &jpeg_vb2_ion;
#endif
	dev->alloc_ctx = dev->vb2->init(dev);

	if (IS_ERR(dev->alloc_ctx)) {
		ret = PTR_ERR(dev->alloc_ctx);
		goto err_video_reg;
	}

#ifdef CONFIG_BUSFREQ_OPP
	/* To lock bus frequency in OPP mode */
	dev->bus_dev = dev_get("exynos-busfreq");
#endif

	/* clock disable */
	clk_disable(dev->clk);
	if (soc_is_exynos5410())
		clk_disable(dev->sclk_clk);

	return 0;

err_video_reg:
	v4l2_m2m_release(dev->m2m_dev_dec);
err_m2m_init_dec:
	video_unregister_device(dev->vfd_dec);
	video_device_release(dev->vfd_dec);
err_vd_alloc_dec:
	v4l2_m2m_release(dev->m2m_dev_enc);
err_m2m_init_enc:
	video_unregister_device(dev->vfd_enc);
	video_device_release(dev->vfd_enc);
err_vd_alloc_enc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_v4l2:
	if (soc_is_exynos5410()) {
		clk_disable(dev->sclk_clk);
		clk_put(dev->sclk_clk);
		clk_disable(dev->clk);
	} else {
		clk_disable(dev->clk);
		clk_put(dev->clk);
	}
	if (soc_is_exynos5410()) {
err_sclk_clk:
		clk_put(dev->clk);
	}
err_clk:
	free_irq(dev->irq_no, NULL);
err_irq:
	iounmap(dev->reg_base);
err_map:
err_region:
	kfree(res);
err_res:
	mutex_destroy(&dev->lock);
err_setup:
	kfree(dev);
err_alloc:
	return ret;

}

static int jpeg_remove(struct platform_device *pdev)
{
	struct jpeg_dev *dev = platform_get_drvdata(pdev);

	v4l2_m2m_release(dev->m2m_dev_enc);
	video_unregister_device(dev->vfd_enc);

	v4l2_m2m_release(dev->m2m_dev_dec);
	video_unregister_device(dev->vfd_dec);

	v4l2_device_unregister(&dev->v4l2_dev);

	dev->vb2->cleanup(dev->alloc_ctx);

	free_irq(dev->irq_no, pdev);
	mutex_destroy(&dev->lock);
	iounmap(dev->reg_base);

	clk_put(dev->clk);
	if (soc_is_exynos5410())
		clk_put(dev->sclk_clk);
#ifdef CONFIG_PM_RUNTIME
	if (soc_is_exynos5250() || soc_is_exynos5410()) {
		pm_runtime_put_sync(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_unlock(dev->bus_dev, &pdev->dev);
#endif
	} else {
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	}
#else
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_unlock(dev->bus_dev, &pdev->dev);
#endif
#endif
	kfree(dev);
	return 0;
}

static int jpeg_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_PM_RUNTIME
	if (soc_is_exynos5250() || soc_is_exynos5410()) {
	struct jpeg_dev *dev = platform_get_drvdata(pdev);

	if (dev->ctx) {
		dev->vb2->suspend(dev->alloc_ctx);
		clk_disable(dev->clk);
		if (soc_is_exynos5410())
			clk_disable(dev->sclk_clk);
	}
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_unlock(dev->bus_dev, &pdev->dev);
#endif
	} else
	pm_runtime_put_sync(&pdev->dev);
#else
	struct jpeg_dev *dev = platform_get_drvdata(pdev);

	if (dev->ctx) {
		dev->vb2->suspend(dev->alloc_ctx);
		clk_disable(dev->clk);
		if (soc_is_exynos5410())
			clk_disable(dev->sclk_clk);
	}
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_unlock(dev->bus_dev, &pdev->dev);
#endif
#endif
	return 0;
}

static int jpeg_resume(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
	if (soc_is_exynos5250() || soc_is_exynos5410()) {
	struct jpeg_dev *dev = platform_get_drvdata(pdev);

	if (dev->ctx) {
		if (soc_is_exynos5410())
			clk_enable(dev->sclk_clk);
		clk_enable(dev->clk);
		dev->vb2->resume(dev->alloc_ctx);
	}
	} else
	pm_runtime_get_sync(&pdev->dev);
#else
	struct jpeg_dev *dev = platform_get_drvdata(pdev);

	if (dev->ctx) {
		if (soc_is_exynos5410())
			clk_enable(dev->sclk_clk);
		clk_enable(dev->clk);
		dev->vb2->resume(dev->alloc_ctx);
	}
#endif
	return 0;
}

int jpeg_suspend_pd(struct device *dev)
{
	struct platform_device *pdev;
	int ret;
	pm_message_t state;

	state.event = 0;
	pdev = to_platform_device(dev);
	ret = jpeg_suspend(pdev, state);

	return 0;
}

int jpeg_resume_pd(struct device *dev)
{
	struct platform_device *pdev;
	int ret;

	pdev = to_platform_device(dev);
	ret = jpeg_resume(pdev);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int jpeg_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct jpeg_dev *jpeg_drv = platform_get_drvdata(pdev);
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_unlock(jpeg_drv->bus_dev, dev);
#endif
	jpeg_drv->vb2->suspend(jpeg_drv->alloc_ctx);
	/* clock disable */
	clk_disable(jpeg_drv->clk);
	if (soc_is_exynos5410())
		clk_disable(jpeg_drv->sclk_clk);
	return 0;
}

static int jpeg_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct jpeg_dev *jpeg_drv = platform_get_drvdata(pdev);
#ifdef CONFIG_BUSFREQ_OPP
	/* lock bus frequency */
	dev_lock(jpeg_drv->bus_dev, &jpeg_drv->plat_dev->dev, BUSFREQ_400MHZ);
#endif
	clk_enable(jpeg_drv->clk);
	if (soc_is_exynos5410())
		clk_enable(jpeg_drv->sclk_clk);
	jpeg_drv->vb2->resume(jpeg_drv->alloc_ctx);
	return 0;
}
#endif

static const struct dev_pm_ops jpeg_pm_ops = {
	.suspend	= jpeg_suspend_pd,
	.resume		= jpeg_resume_pd,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = jpeg_runtime_suspend,
	.runtime_resume = jpeg_runtime_resume,
#endif
};
static struct platform_driver jpeg_driver = {
	.probe		= jpeg_probe,
	.remove		= jpeg_remove,
#if defined (CONFIG_SOC_EXYNOS5250) || (CONFIG_SOC_EXYNOS5410)
	.suspend	= jpeg_suspend,
	.resume		= jpeg_resume,
#else
#ifndef CONFIG_PM_RUNTIME
	.suspend	= jpeg_suspend,
	.resume		= jpeg_resume,
#endif
#endif
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= JPEG_NAME,
#ifdef CONFIG_PM_RUNTIME
#if defined (CONFIG_SOC_EXYNOS5250) || (CONFIG_SOC_EXYNOS5410)
		.pm = NULL,
#else
		.pm = &jpeg_pm_ops,
#endif
#else
		.pm = NULL,
#endif
	},
};

static int __init jpeg_init(void)
{
	int ret;

	printk(KERN_CRIT "Initialize JPEG driver\n");

	ret = platform_driver_register(&jpeg_driver);

	if (ret)
		jpeg_err("failed to register jpeg driver\n");

	return ret;
}

static void __exit jpeg_exit(void)
{
	platform_driver_unregister(&jpeg_driver);
}

module_init(jpeg_init);
module_exit(jpeg_exit);

MODULE_AUTHOR("ym.song@samsung.com>");
MODULE_DESCRIPTION("JPEG v2.x H/W Device Driver");
MODULE_LICENSE("GPL");
