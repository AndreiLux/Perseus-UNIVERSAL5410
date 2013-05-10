/*
 * drivers/media/video/exynos/mfc/s5p_mfc_opr_v6.c
 *
 * Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains hw related functions.
 *
 * Kamil Debski, Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/jiffies.h>

#include <linux/firmware.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/cma.h>

#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <plat/iovmm.h>
#endif

#include "s5p_mfc_common.h"

#include "s5p_mfc_cmd.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_intr.h"
#include "s5p_mfc_inst.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_dec.h"
#include "s5p_mfc_enc.h"

/* #define S5P_MFC_DEBUG_REGWRITE  */
#ifdef S5P_MFC_DEBUG_REGWRITE
#undef writel
#define writel(v, r)								\
	do {									\
		printk(KERN_ERR "MFCWRITE(%p): %08x\n", r, (unsigned int)v);	\
	__raw_writel(v, r);							\
	} while (0)
#endif /* S5P_MFC_DEBUG_REGWRITE */

#define READL(offset)		readl(dev->regs_base + (offset))
#define WRITEL(data, offset)	writel((data), dev->regs_base + (offset))
#define OFFSETA(x)		(((x) - dev->port_a) >> S5P_FIMV_MEM_OFFSET)
#define OFFSETB(x)		(((x) - dev->port_b) >> S5P_FIMV_MEM_OFFSET)

/* Allocate temporary buffers for decoding */
int s5p_mfc_alloc_dec_temp_buffers(struct s5p_mfc_ctx *ctx)
{
	/* NOP */

	return 0;
}

/* Release temproary buffers for decoding */
void s5p_mfc_release_dec_desc_buffer(struct s5p_mfc_ctx *ctx)
{
	/* NOP */
}

/* Allocate codec buffers */
int s5p_mfc_alloc_codec_buffers(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_enc *enc;
	unsigned int mb_width, mb_height;
	void *alloc_ctx;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	enc = ctx->enc_priv;
	alloc_ctx = dev->alloc_ctx[MFC_CMA_BANK1_ALLOC_CTX];

	mb_width = mb_width(ctx->img_width);
	mb_height = mb_height(ctx->img_height);

	if (ctx->type == MFCINST_DECODER) {
		mfc_debug(2, "Luma size:%d Chroma size:%d MV size:%d\n",
			  ctx->luma_size, ctx->chroma_size, ctx->mv_size);
		mfc_debug(2, "Totals bufs: %d\n", dec->total_dpb_count);
	} else if (ctx->type == MFCINST_ENCODER) {
		enc->tmv_buffer_size = ENC_TMV_SIZE(mb_width, mb_height);
		enc->tmv_buffer_size = ALIGN(enc->tmv_buffer_size, 32) * 2;
		enc->luma_dpb_size = ALIGN((mb_width * mb_height) * 256, 256);
		enc->chroma_dpb_size = ALIGN((mb_width * mb_height) * 128, 256);
		enc->me_buffer_size =
			(ENC_ME_SIZE(ctx->img_width, ctx->img_height,
						mb_width, mb_height));
		enc->me_buffer_size = ALIGN(enc->me_buffer_size, 256);

		mfc_debug(2, "recon luma size: %d chroma size: %d\n",
			  enc->luma_dpb_size, enc->chroma_dpb_size);
	} else {
		return -EINVAL;
	}

	/* Codecs have different memory requirements */
	switch (ctx->codec_mode) {
	case S5P_FIMV_CODEC_H264_DEC:
	case S5P_FIMV_CODEC_H264_MVC_DEC:
		dec->mv_count = dec->total_dpb_count;
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_H264_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				DEC_V65_H264_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size =
			ctx->scratch_buf_size +
			(dec->mv_count * ctx->mv_size);
		break;
	case S5P_FIMV_CODEC_MPEG4_DEC:
	case S5P_FIMV_CODEC_FIMV1_DEC:
	case S5P_FIMV_CODEC_FIMV2_DEC:
	case S5P_FIMV_CODEC_FIMV3_DEC:
	case S5P_FIMV_CODEC_FIMV4_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				DEC_V65_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_VC1RCV_DEC:
	case S5P_FIMV_CODEC_VC1_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_VC1_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				DEC_V65_VC1_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_MPEG2_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_MPEG2_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				DEC_V65_MPEG2_SCRATCH_SIZE(mb_width, mb_height);
		ctx->port_a_size = 0;
		ctx->port_b_size = 0;
		break;
	case S5P_FIMV_CODEC_H263_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				DEC_V65_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_VP8_DEC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				DEC_V61_VP8_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				DEC_V65_VP8_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size = ctx->scratch_buf_size;
		break;
	case S5P_FIMV_CODEC_H264_ENC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				ENC_V61_H264_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				ENC_V65_H264_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		ctx->port_b_size = 0;
		break;
	case S5P_FIMV_CODEC_MPEG4_ENC:
	case S5P_FIMV_CODEC_H263_ENC:
		if (mfc_version(dev) == 0x61)
			ctx->scratch_buf_size =
				ENC_V61_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		else
			ctx->scratch_buf_size =
				ENC_V65_MPEG4_SCRATCH_SIZE(mb_width, mb_height);
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		ctx->port_a_size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		ctx->port_b_size = 0;
		break;
	default:
		break;
	}

	if (ctx->is_drm)
		alloc_ctx = dev->alloc_ctx_drm;

	/* Allocate only if memory from bank 1 is necessary */
	if (ctx->port_a_size > 0) {
		ctx->port_a_buf = s5p_mfc_mem_alloc_priv(
				alloc_ctx, ctx->port_a_size);
		if (IS_ERR(ctx->port_a_buf)) {
			ctx->port_a_buf = 0;
			printk(KERN_ERR
			       "Buf alloc for decoding failed (port A).\n");
			return -ENOMEM;
		}
		ctx->port_a_phys = s5p_mfc_mem_daddr_priv(ctx->port_a_buf);
	}

	mfc_debug_leave();

	return 0;
}

/* Release buffers allocated for codec */
void s5p_mfc_release_codec_buffers(struct s5p_mfc_ctx *ctx)
{
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}
	if (ctx->port_a_buf) {
		s5p_mfc_mem_free_priv(ctx->port_a_buf);
		ctx->port_a_buf = 0;
		ctx->port_a_phys = 0;
		ctx->port_a_size = 0;
	}
}

/* Allocate memory for instance data buffer */
int s5p_mfc_alloc_instance_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf_size_v6 *buf_size;
	void *alloc_ctx;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	buf_size = dev->variant->buf_size->buf;
	alloc_ctx = dev->alloc_ctx[MFC_CMA_BANK1_ALLOC_CTX];

	switch (ctx->codec_mode) {
	case S5P_FIMV_CODEC_H264_DEC:
	case S5P_FIMV_CODEC_H264_MVC_DEC:
		ctx->ctx_buf_size = buf_size->h264_dec_ctx;
		break;
	case S5P_FIMV_CODEC_MPEG4_DEC:
	case S5P_FIMV_CODEC_H263_DEC:
	case S5P_FIMV_CODEC_VC1RCV_DEC:
	case S5P_FIMV_CODEC_VC1_DEC:
	case S5P_FIMV_CODEC_MPEG2_DEC:
	case S5P_FIMV_CODEC_VP8_DEC:
	case S5P_FIMV_CODEC_FIMV1_DEC:
	case S5P_FIMV_CODEC_FIMV2_DEC:
	case S5P_FIMV_CODEC_FIMV3_DEC:
	case S5P_FIMV_CODEC_FIMV4_DEC:
		ctx->ctx_buf_size = buf_size->other_dec_ctx;
		break;
	case S5P_FIMV_CODEC_H264_ENC:
		ctx->ctx_buf_size = buf_size->h264_enc_ctx;
		break;
	case S5P_FIMV_CODEC_MPEG4_ENC:
	case S5P_FIMV_CODEC_H263_ENC:
		ctx->ctx_buf_size = buf_size->other_enc_ctx;
		break;
	default:
		ctx->ctx_buf_size = 0;
		mfc_err("Codec type(%d) should be checked!\n", ctx->codec_mode);
		break;
	}

	if (ctx->is_drm)
		alloc_ctx = dev->alloc_ctx_drm;

	ctx->ctx.alloc = s5p_mfc_mem_alloc_priv(alloc_ctx, ctx->ctx_buf_size);
	if (IS_ERR(ctx->ctx.alloc)) {
		mfc_err("Allocating context buffer failed.\n");
		return PTR_ERR(ctx->ctx.alloc);
	}

	ctx->ctx.ofs = s5p_mfc_mem_daddr_priv(ctx->ctx.alloc);
	ctx->ctx.virt = s5p_mfc_mem_vaddr_priv(ctx->ctx.alloc);
	if (!ctx->ctx.virt) {
		s5p_mfc_mem_free_priv(ctx->ctx.alloc);
		ctx->ctx.alloc = NULL;
		ctx->ctx.ofs = 0;
		ctx->ctx.virt = NULL;

		mfc_err("Remapping context buffer failed.\n");
		return -ENOMEM;
	}

	mfc_debug_leave();

	return 0;
}

/* Release instance buffer */
void s5p_mfc_release_instance_buffer(struct s5p_mfc_ctx *ctx)
{
	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	if (ctx->ctx.alloc) {
		/*
		dma_unmap_single(ctx->dev->v4l2_dev.dev,
				ctx->ctx.dma, ctx->ctx_buf_size,
				DMA_TO_DEVICE);
		*/
		s5p_mfc_mem_free_priv(ctx->ctx.alloc);
		ctx->ctx.alloc = NULL;
		ctx->ctx.ofs = 0;
		ctx->ctx.virt = NULL;
	}

	mfc_debug_leave();
}

/* Allocate context buffers for SYS_INIT */
int s5p_mfc_alloc_dev_context_buffer(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_buf_size_v6 *buf_size;
	void *alloc_ctx;

	mfc_debug_enter();
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	buf_size = dev->variant->buf_size->buf;
	alloc_ctx = dev->alloc_ctx[MFC_CMA_BANK1_ALLOC_CTX];

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->num_drm_inst)
		alloc_ctx = dev->alloc_ctx_drm;
#endif
	dev->ctx_buf.alloc =
			s5p_mfc_mem_alloc_priv(alloc_ctx, buf_size->dev_ctx);
	if (IS_ERR(dev->ctx_buf.alloc)) {
		mfc_err("Allocating DESC buffer failed.\n");
		return PTR_ERR(dev->ctx_buf.alloc);
	}

	dev->ctx_buf.ofs = s5p_mfc_mem_daddr_priv(dev->ctx_buf.alloc);
	dev->ctx_buf.virt = s5p_mfc_mem_vaddr_priv(dev->ctx_buf.alloc);
	if (!dev->ctx_buf.virt) {
		s5p_mfc_mem_free_priv(dev->ctx_buf.alloc);
		dev->ctx_buf.alloc = NULL;
		dev->ctx_buf.ofs = 0;

		mfc_err("Remapping DESC buffer failed.\n");
		return -ENOMEM;
	}

	mfc_debug_leave();

	return 0;
}

/* Release context buffers for SYS_INIT */
void s5p_mfc_release_dev_context_buffer(struct s5p_mfc_dev *dev)
{
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}
	if (dev->ctx_buf.alloc) {
		s5p_mfc_mem_free_priv(dev->ctx_buf.alloc);
		dev->ctx_buf.alloc = NULL;
		dev->ctx_buf.ofs = 0;
		dev->ctx_buf.virt = NULL;
	}
}

static int calc_plane(int width, int height)
{
	int mbX, mbY;

	mbX = (width + 15)/16;
	mbY = (height + 15)/16;

	/* Alignment for interlaced processing */
	mbY = (mbY + 1) / 2 * 2;

	return (mbX * 16) * (mbY * 16);
}

void s5p_mfc_dec_calc_dpb_size(struct s5p_mfc_ctx *ctx)
{
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	ctx->buf_width = ALIGN(ctx->img_width, S5P_FIMV_NV12MT_HALIGN);
	ctx->buf_height = ALIGN(ctx->img_height, S5P_FIMV_NV12MT_VALIGN);
	mfc_info("SEQ Done: Movie dimensions %dx%d, "
			"buffer dimensions: %dx%d\n", ctx->img_width,
			ctx->img_height, ctx->buf_width, ctx->buf_height);

	ctx->luma_size = calc_plane(ctx->img_width, ctx->img_height);
	ctx->chroma_size = calc_plane(ctx->img_width, (ctx->img_height >> 1));
	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC) {
		ctx->mv_size = s5p_mfc_dec_mv_size(ctx->img_width,
				ctx->img_height);
		ctx->mv_size = ALIGN(ctx->mv_size, 16);
	} else {
		ctx->mv_size = 0;
	}
}

void s5p_mfc_enc_calc_src_size(struct s5p_mfc_ctx *ctx)
{
	unsigned int mb_width, mb_height;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	mb_width = mb_width(ctx->img_width);
	mb_height = mb_height(ctx->img_height);

	ctx->buf_width = ALIGN(ctx->img_width, S5P_FIMV_NV12M_HALIGN);
	ctx->luma_size = ALIGN((mb_width * mb_height) * 256, 256);
	ctx->chroma_size = ALIGN((mb_width * mb_height) * 128, 256);
}

/* Set registers for decoding stream buffer */
int s5p_mfc_set_dec_stream_buffer(struct s5p_mfc_ctx *ctx, dma_addr_t buf_addr,
		  unsigned int start_num_byte, unsigned int strm_size)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf_size *buf_size;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	buf_size = dev->variant->buf_size;
	mfc_debug(2, "inst_no: %d, buf_addr: 0x%08x, buf_size: 0x"
		"%08x (%d)\n",  ctx->inst_no, buf_addr, strm_size, strm_size);
	WRITEL(strm_size, S5P_FIMV_D_STREAM_DATA_SIZE);
	WRITEL(buf_addr, S5P_FIMV_D_CPB_BUFFER_ADDR);
	WRITEL(buf_size->cpb_buf, S5P_FIMV_D_CPB_BUFFER_SIZE);
	WRITEL(start_num_byte, S5P_FIMV_D_CPB_BUFFER_OFFSET);

	mfc_debug_leave();
	return 0;
}

/* Set decoding frame buffer */
int s5p_mfc_set_dec_frame_buffer(struct s5p_mfc_ctx *ctx)
{
	unsigned int frame_size, i;
	unsigned int frame_size_ch, frame_size_mv;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	size_t buf_addr1;
	int buf_size1;
	int align_gap;
	struct s5p_mfc_buf *buf;
	struct list_head *buf_queue;
	unsigned char *dpb_vir;
	unsigned int reg = 0;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	buf_addr1 = ctx->port_a_phys;
	buf_size1 = ctx->port_a_size;

	mfc_info("Buf1: %p (%d)\n", (void *)buf_addr1, buf_size1);
	mfc_info("Total DPB COUNT: %d\n", dec->total_dpb_count);
	mfc_info("Setting display delay to %d\n", dec->display_delay);

	WRITEL(dec->total_dpb_count, S5P_FIMV_D_NUM_DPB);
	WRITEL(ctx->luma_size, S5P_FIMV_D_LUMA_DPB_SIZE);
	WRITEL(ctx->chroma_size, S5P_FIMV_D_CHROMA_DPB_SIZE);

	WRITEL(buf_addr1, S5P_FIMV_D_SCRATCH_BUFFER_ADDR);
	WRITEL(ctx->scratch_buf_size, S5P_FIMV_D_SCRATCH_BUFFER_SIZE);
	buf_addr1 += ctx->scratch_buf_size;
	buf_size1 -= ctx->scratch_buf_size;

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC)
		WRITEL(ctx->mv_size, S5P_FIMV_D_MV_BUFFER_SIZE);

	if ((ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_DEC) &&
			FW_HAS_INITBUF_LOOP_FILTER(dev)) {
		if (dec->loop_filter_mpeg4) {
			if (dec->total_dpb_count >=
					(ctx->dpb_count + NUM_MPEG4_LF_BUF)) {
				if (!dec->internal_dpb) {
					dec->internal_dpb = NUM_MPEG4_LF_BUF;
					ctx->dpb_count += dec->internal_dpb;
				}
			} else {
				dec->loop_filter_mpeg4 = 0;
				mfc_info("failed to enable loop filter\n");
			}
		}
		reg |= (dec->loop_filter_mpeg4
				<< S5P_FIMV_D_OPT_LF_CTRL_SHIFT);
	}
	if ((ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) &&
			FW_HAS_INITBUF_TILE_MODE(dev))
		reg |= (0x1 << S5P_FIMV_D_OPT_TILE_MODE_SHIFT);
	WRITEL(reg, S5P_FIMV_D_INIT_BUFFER_OPTIONS);

	frame_size = ctx->luma_size;
	frame_size_ch = ctx->chroma_size;
	frame_size_mv = ctx->mv_size;
	mfc_debug(2, "Frame size: %d ch: %d mv: %d\n", frame_size, frame_size_ch,
								frame_size_mv);

	if (dec->dst_memtype == V4L2_MEMORY_USERPTR || dec->dst_memtype == V4L2_MEMORY_DMABUF)
		buf_queue = &ctx->dst_queue;
	else
		buf_queue = &dec->dpb_queue;

	i = 0;
	list_for_each_entry(buf, buf_queue, list) {
		mfc_debug(2, "Luma %x\n", buf->planes.raw.luma);
		WRITEL(buf->planes.raw.luma, S5P_FIMV_D_LUMA_DPB + i * 4);
		mfc_debug(2, "\tChroma %x\n", buf->planes.raw.chroma);
		WRITEL(buf->planes.raw.chroma, S5P_FIMV_D_CHROMA_DPB + i * 4);

		if ((i == 0) && (!ctx->is_drm)) {
			dpb_vir = vb2_plane_vaddr(&buf->vb, 0);
			if (dpb_vir)
				memset(dpb_vir, 0x0, ctx->luma_size);

			dpb_vir = vb2_plane_vaddr(&buf->vb, 1);
			if (dpb_vir)
				memset(dpb_vir, 0x80, ctx->chroma_size);

			s5p_mfc_mem_clean_vb(&buf->vb, 2);
		}
		i++;
	}

	WRITEL(dec->mv_count, S5P_FIMV_D_NUM_MV);
	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
			ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC) {
		for (i = 0; i < dec->mv_count; i++) {
			/* To test alignment */
			align_gap = buf_addr1;
			buf_addr1 = ALIGN(buf_addr1, 16);
			align_gap = buf_addr1 - align_gap;
			buf_size1 -= align_gap;

			mfc_debug(2, "\tBuf1: %x, size: %d\n", buf_addr1, buf_size1);
			WRITEL(buf_addr1, S5P_FIMV_D_MV_BUFFER + i * 4);
			buf_addr1 += frame_size_mv;
			buf_size1 -= frame_size_mv;
		}
	}

	mfc_debug(2, "Buf1: %u, buf_size1: %d (frames %d)\n",
			buf_addr1, buf_size1, dec->total_dpb_count);
	if (buf_size1 < 0) {
		mfc_debug(2, "Not enough memory has been allocated.\n");
		return -ENOMEM;
	}

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_CH_INIT_BUFS, NULL);

	mfc_debug(2, "After setting buffers.\n");
	return 0;
}

/* Set registers for encoding stream buffer */
int s5p_mfc_set_enc_stream_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr, unsigned int size)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	WRITEL(addr, S5P_FIMV_E_STREAM_BUFFER_ADDR); /* 16B align */
	WRITEL(size, S5P_FIMV_E_STREAM_BUFFER_SIZE);

	mfc_debug(2, "stream buf addr: 0x%08lx, size: 0x%08x(%d)",
		(unsigned long)addr, size, size);

	return 0;
}

void s5p_mfc_set_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t y_addr, dma_addr_t c_addr)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	WRITEL(y_addr, S5P_FIMV_E_SOURCE_LUMA_ADDR); /* 256B align */
	WRITEL(c_addr, S5P_FIMV_E_SOURCE_CHROMA_ADDR);

	mfc_debug(2, "enc src y buf addr: 0x%08lx", (unsigned long)y_addr);
	mfc_debug(2, "enc src c buf addr: 0x%08lx", (unsigned long)c_addr);
}

void s5p_mfc_get_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t *y_addr, dma_addr_t *c_addr)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long enc_recon_y_addr, enc_recon_c_addr;

	*y_addr = READL(S5P_FIMV_E_ENCODED_SOURCE_LUMA_ADDR);
	*c_addr = READL(S5P_FIMV_E_ENCODED_SOURCE_CHROMA_ADDR);

	enc_recon_y_addr = READL(S5P_FIMV_E_RECON_LUMA_DPB_ADDR);
	enc_recon_c_addr = READL(S5P_FIMV_E_RECON_CHROMA_DPB_ADDR);

	mfc_debug(2, "recon y addr: 0x%08lx", enc_recon_y_addr);
	mfc_debug(2, "recon c addr: 0x%08lx", enc_recon_c_addr);
}

/* Set encoding ref & codec buffer */
int s5p_mfc_set_enc_ref_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	size_t buf_addr1;
	int buf_size1;
	int i;

	mfc_debug_enter();

	buf_addr1 = ctx->port_a_phys;
	buf_size1 = ctx->port_a_size;

	mfc_debug(2, "Buf1: %p (%d)\n", (void *)buf_addr1, buf_size1);

	for (i = 0; i < ctx->dpb_count; i++) {
		WRITEL(buf_addr1, S5P_FIMV_E_LUMA_DPB + (4 * i));
		buf_addr1 += enc->luma_dpb_size;
		WRITEL(buf_addr1, S5P_FIMV_E_CHROMA_DPB + (4 * i));
		buf_addr1 += enc->chroma_dpb_size;
		WRITEL(buf_addr1, S5P_FIMV_E_ME_BUFFER + (4 * i));
		buf_addr1 += enc->me_buffer_size;
		buf_size1 -= (enc->luma_dpb_size + enc->chroma_dpb_size +
			enc->me_buffer_size);
	}

	WRITEL(buf_addr1, S5P_FIMV_E_SCRATCH_BUFFER_ADDR);
	WRITEL(ctx->scratch_buf_size, S5P_FIMV_E_SCRATCH_BUFFER_SIZE);
	buf_addr1 += ctx->scratch_buf_size;
	buf_size1 -= ctx->scratch_buf_size;

	WRITEL(buf_addr1, S5P_FIMV_E_TMV_BUFFER0);
	buf_addr1 += enc->tmv_buffer_size >> 1;
	WRITEL(buf_addr1, S5P_FIMV_E_TMV_BUFFER1);
	buf_addr1 += enc->tmv_buffer_size >> 1;
	buf_size1 -= enc->tmv_buffer_size;

	mfc_debug(2, "Buf1: %u, buf_size1: %d (ref frames %d)\n",
			buf_addr1, buf_size1, ctx->dpb_count);
	if (buf_size1 < 0) {
		mfc_debug(2, "Not enough memory has been allocated.\n");
		return -ENOMEM;
	}

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_CH_INIT_BUFS, NULL);

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_set_slice_mode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	/* multi-slice control */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES)
		WRITEL((enc->slice_mode + 0x4), S5P_FIMV_E_MSLICE_MODE);
	else
		WRITEL(enc->slice_mode, S5P_FIMV_E_MSLICE_MODE);

	/* multi-slice MB number or bit size */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
		WRITEL(enc->slice_size.mb, S5P_FIMV_E_MSLICE_SIZE_MB);
	} else if (enc->slice_mode == \
			V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
		WRITEL(enc->slice_size.bits, S5P_FIMV_E_MSLICE_SIZE_BITS);
	} else {
		WRITEL(0x0, S5P_FIMV_E_MSLICE_SIZE_MB);
		WRITEL(0x0, S5P_FIMV_E_MSLICE_SIZE_BITS);
	}

	return 0;

}

static int s5p_mfc_set_enc_params(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0;

	mfc_debug_enter();

	/* width */
	WRITEL(ctx->img_width, S5P_FIMV_E_FRAME_WIDTH); /* 16 align */
	/* height */
	WRITEL(ctx->img_height, S5P_FIMV_E_FRAME_HEIGHT); /* 16 align */

	/** cropped width */
	WRITEL(ctx->img_width, S5P_FIMV_E_CROPPED_FRAME_WIDTH);
	/** cropped height */
	WRITEL(ctx->img_height, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);
	/** cropped offset */
	WRITEL(0x0, S5P_FIMV_E_FRAME_CROP_OFFSET);

	/* pictype : IDR period */
	reg = 0;
	reg &= ~(0xffff);
	reg |= p->gop_size;
	WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);

	/* multi-slice control */
	/* multi-slice MB number or bit size */
	enc->slice_mode = p->slice_mode;

	WRITEL(0, S5P_FIMV_E_ENC_OPTIONS);

	if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
		enc->slice_size.mb = p->slice_mb;
	} else if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
		enc->slice_size.bits = p->slice_bit;
	} else {
		enc->slice_size.mb = 0;
		enc->slice_size.bits = 0;
	}

	s5p_mfc_set_slice_mode(ctx);

	/* cyclic intra refresh */
	WRITEL(p->intra_refresh_mb, S5P_FIMV_E_IR_SIZE);
	reg = READL(S5P_FIMV_E_ENC_OPTIONS);
	if (p->intra_refresh_mb == 0)
		reg &= ~(0x1 << 4);
	else
		reg |= (0x1 << 4);
	WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* 'NON_REFERENCE_STORE_ENABLE' for debugging */
	reg = READL(S5P_FIMV_E_ENC_OPTIONS);
	reg &= ~(0x1 << 9);
	WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* memory structure cur. frame */
	if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV12M) {
		/* 0: Linear, 1: 2D tiled*/
		reg = READL(S5P_FIMV_E_ENC_OPTIONS);
		reg &= ~(0x1 << 7);
		WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);
		/* 0: NV12(CbCr), 1: NV21(CrCb) */
		WRITEL(0x0, S5P_FIMV_PIXEL_FORMAT);
	} else if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV21M) {
		/* 0: Linear, 1: 2D tiled*/
		reg = READL(S5P_FIMV_E_ENC_OPTIONS);
		reg &= ~(0x1 << 7);
		WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);
		/* 0: NV12(CbCr), 1: NV21(CrCb) */
		WRITEL(0x1, S5P_FIMV_PIXEL_FORMAT);
	} else if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) {
		/* 0: Linear, 1: 2D tiled*/
		reg = READL(S5P_FIMV_E_ENC_OPTIONS);
		reg |= (0x1 << 7);
		WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);
		/* 0: NV12(CbCr), 1: NV21(CrCb) */
		WRITEL(0x0, S5P_FIMV_PIXEL_FORMAT);
	}

	/* memory structure recon. frame */
	/* 0: Linear, 1: 2D tiled */
	reg = READL(S5P_FIMV_E_ENC_OPTIONS);
	reg |= (0x1 << 8);
	WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* padding control & value */
	WRITEL(0x0, S5P_FIMV_E_PADDING_CTRL);
	if (p->pad) {
		reg = 0;
		/** enable */
		reg |= (1 << 31);
		/** cr value */
		reg &= ~(0xFF << 16);
		reg |= (p->pad_cr << 16);
		/** cb value */
		reg &= ~(0xFF << 8);
		reg |= (p->pad_cb << 8);
		/** y value */
		reg &= ~(0xFF);
		reg |= (p->pad_luma);
		WRITEL(reg, S5P_FIMV_E_PADDING_CTRL);
	}

	/* rate control config. */
	reg = 0;
	/** frame-level rate control */
	reg &= ~(0x1 << 9);
	reg |= (p->rc_frame << 9);
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* bit rate */
	if (p->rc_frame)
		WRITEL(p->rc_bitrate,
			S5P_FIMV_E_RC_BIT_RATE);
	else
		WRITEL(1, S5P_FIMV_E_RC_BIT_RATE);

	if (p->rc_frame) {
		if (FW_HAS_ADV_RC_MODE(dev)) {
			if (p->rc_reaction_coeff <= TIGHT_CBR_MAX)
				reg = S5P_FIMV_ENC_ADV_TIGHT_CBR;
			else
				reg = S5P_FIMV_ENC_ADV_CAM_CBR;
		} else {
			if (p->rc_reaction_coeff <= TIGHT_CBR_MAX)
				reg = S5P_FIMV_ENC_TIGHT_CBR;
			else
				reg = S5P_FIMV_ENC_LOOSE_CBR;
		}

		WRITEL(reg, S5P_FIMV_E_RC_MODE);
	}

	/* extended encoder ctrl */
	/** vbv buffer size */
	if (p->frame_skip_mode == V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT)
		WRITEL(p->vbv_buf_size, S5P_FIMV_E_VBV_BUFFER_SIZE);

	/** seq header ctrl */
	reg = READL(S5P_FIMV_E_ENC_OPTIONS);
	reg &= ~(0x1 << 2);
	reg |= (p->seq_hdr_mode << 2);
	/** frame skip mode */
	reg &= ~(0x3);
	reg |= (p->frame_skip_mode);
	WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* 'DROP_CONTROL_ENABLE', disable */
	reg = READL(S5P_FIMV_E_RC_CONFIG);
	reg &= ~(0x1 << 10);
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* setting for MV range [16, 256] */
	if (mfc_version(dev) == 0x61)
		reg = ENC_V61_MV_RANGE;
	else
		reg = ENC_V65_MV_RANGE;
	WRITEL(reg, S5P_FIMV_E_MV_HOR_RANGE);
	WRITEL(reg, S5P_FIMV_E_MV_VER_RANGE);

	WRITEL(0x0, S5P_FIMV_E_VBV_INIT_DELAY); /* SEQ_start Only */

	/* initialize for '0' only setting */
	WRITEL(0x0, S5P_FIMV_E_FRAME_INSERTION); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_ROI_BUFFER_ADDR); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_PARAM_CHANGE); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_RC_ROI_CTRL); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_PICTURE_TAG); /* NAL_start Only */

	WRITEL(0x0, S5P_FIMV_E_BIT_COUNT_ENABLE); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_MAX_BIT_COUNT); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_MIN_BIT_COUNT); /* NAL_start Only */

	WRITEL(0x0, S5P_FIMV_E_METADATA_BUFFER_ADDR); /* NAL_start Only */
	WRITEL(0x0, S5P_FIMV_E_METADATA_BUFFER_SIZE); /* NAL_start Only */

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_set_enc_params_h264(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_h264_enc_params *p_264 = &p->codec.h264;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* pictype : number of B */
	reg = READL(S5P_FIMV_E_GOP_CONFIG);
	/** num_b_frame - 0 ~ 2 */
	reg &= ~(0x3 << 16);
	reg |= (p->num_b_frame << 16);
	WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);

	/* profile & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_264->level << 8);
	/** profile - 0 ~ 3 */
	reg &= ~(0x3F);
	reg |= p_264->profile;
	WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* interlace */
	reg = 0;
	reg &= ~(0x1 << 3);
	reg |= (p_264->interlace << 3);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/** height */
	if (p_264->interlace) {
		WRITEL(ctx->img_height >> 1, S5P_FIMV_E_FRAME_HEIGHT); /* 32 align */
		/** cropped height */
		WRITEL(ctx->img_height >> 1, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);
	}

	/* loop filter ctrl */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x3 << 1);
	reg |= (p_264->loop_filter_mode << 1);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* loopfilter alpha offset */
	if (p_264->loop_filter_alpha < 0) {
		reg = 0x10;
		reg |= (0xFF - p_264->loop_filter_alpha) + 1;
	} else {
		reg = 0x00;
		reg |= (p_264->loop_filter_alpha & 0xF);
	}
	WRITEL(reg, S5P_FIMV_E_H264_LF_ALPHA_OFFSET);

	/* loopfilter beta offset */
	if (p_264->loop_filter_beta < 0) {
		reg = 0x10;
		reg |= (0xFF - p_264->loop_filter_beta) + 1;
	} else {
		reg = 0x00;
		reg |= (p_264->loop_filter_beta & 0xF);
	}
	WRITEL(reg, S5P_FIMV_E_H264_LF_BETA_OFFSET);

	/* entropy coding mode */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1);
	reg |= (p_264->entropy_mode);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* number of ref. picture */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 7);
	reg |= ((p_264->num_ref_pic_4p-1) << 7);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* 8x8 transform enable */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x3 << 12);
	reg |= (p_264->_8x8_transform << 12);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* rate control config. */
	reg = READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_264->rc_frame_qp;
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* frame rate */
	/* Fix value for H.264, H.263 in the driver */
	p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_264->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_264->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_264->rc_min_qp;
	WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* macroblock adaptive scaling features */
	WRITEL(0x0, S5P_FIMV_E_MB_RC_CONFIG);
	if (p->rc_mb) {
		reg = 0;
		/** dark region */
		reg &= ~(0x1 << 3);
		reg |= (p_264->rc_mb_dark << 3);
		/** smooth region */
		reg &= ~(0x1 << 2);
		reg |= (p_264->rc_mb_smooth << 2);
		/** static region */
		reg &= ~(0x1 << 1);
		reg |= (p_264->rc_mb_static << 1);
		/** high activity region */
		reg &= ~(0x1);
		reg |= p_264->rc_mb_activity;
		WRITEL(reg, S5P_FIMV_E_MB_RC_CONFIG);
	}

	WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 16);
		reg |= (p_264->rc_b_frame_qp << 16);
		reg &= ~(0x3f << 8);
		reg |= (p_264->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_264->rc_frame_qp;
		WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* sps pps control */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 29);
	reg |= (p_264->prepend_sps_pps_to_idr << 29);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/*
	 * CONSTRAINT_SET0_FLAG: all constraints specified in
	 * Baseline Profile
	 */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg |= (0x1 << 26);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* extended encoder ctrl */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 5);
	reg |= (p_264->ar_vui << 5);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	WRITEL(0x0, S5P_FIMV_E_ASPECT_RATIO);
	WRITEL(0x0, S5P_FIMV_E_EXTENDED_SAR);
	if (p_264->ar_vui) {
		/* aspect ration IDC */
		reg = 0;
		reg &= ~(0xff);
		reg |= p_264->ar_vui_idc;
		WRITEL(reg, S5P_FIMV_E_ASPECT_RATIO);
		if (p_264->ar_vui_idc == 0xFF) {
			/* sample  AR info. */
			reg = 0;
			reg &= ~(0xffffffff);
			reg |= p_264->ext_sar_width << 16;
			reg |= p_264->ext_sar_height;
			WRITEL(reg, S5P_FIMV_E_EXTENDED_SAR);
		}
	}

	/* intra picture period for H.264 open GOP */
	/** control */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 4);
	reg |= (p_264->open_gop << 4);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);
	/** value */
	WRITEL(0x0, S5P_FIMV_E_H264_I_PERIOD);
	if (p_264->open_gop) {
		reg = 0;
		reg &= ~(0xffff);
		reg |= p_264->open_gop_size;
		WRITEL(reg, S5P_FIMV_E_H264_I_PERIOD);
	}

	/* 'WEIGHTED_BI_PREDICTION' for B is disable */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x3 << 9);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* 'CONSTRAINED_INTRA_PRED_ENABLE' is disable */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 14);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* ASO enable */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	if (p_264->aso_enable)
		reg |= (0x1 << 6);
	else
		reg &= ~(0x1 << 6);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* VUI parameter disable */
	if (FW_HAS_VUI_PARAMS(dev)) {
		reg = READL(S5P_FIMV_E_H264_OPTIONS);
		reg &= ~(0x1 << 30);
		WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);
	}

	/* hier qp enable */
	reg = READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 8);
	reg |= ((p_264->hier_qp & 0x1) << 8);
	WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);
	reg = 0;
	if (p_264->hier_qp && p_264->hier_qp_layer) {
		reg |= (p_264->hier_qp_type & 0x1) << 0x3;
		reg |= p_264->hier_qp_layer & 0x7;
		WRITEL(reg, S5P_FIMV_E_H264_NUM_T_LAYER);
		/* QP value for each layer */
		for (i = 0; i < (p_264->hier_qp_layer & 0x7); i++)
			WRITEL(p_264->hier_qp_layer_qp[i],
				S5P_FIMV_E_H264_HIERARCHICAL_QP_LAYER0 + i * 4);
	}
	/* number of coding layer should be zero when hierarchical is disable */
	WRITEL(reg, S5P_FIMV_E_H264_NUM_T_LAYER);

	/* set frame pack sei generation */
	if (p_264->sei_gen_enable) {
		/* frame packing enable */
		reg = READL(S5P_FIMV_E_H264_OPTIONS);
		reg |= (1 << 25);
		WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

		/* set current frame0 flag & arrangement type */
		reg = 0;
		/** current frame0 flag */
		reg |= ((p_264->sei_fp_curr_frame_0 & 0x1) << 2);
		/** arrangement type */
		reg |= (p_264->sei_fp_arrangement_type - 3) & 0x3;
		WRITEL(reg, S5P_FIMV_E_H264_FRAME_PACKING_SEI_INFO);
	}

	if (p_264->fmo_enable) {
		switch (p_264->fmo_slice_map_type) {
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES:
			if (p_264->fmo_slice_num_grp > 4)
				p_264->fmo_slice_num_grp = 4;
			for (i = 0; i < (p_264->fmo_slice_num_grp & 0xF); i++)
				WRITEL(p_264->fmo_run_length[i] - 1,
				S5P_FIMV_E_H264_FMO_RUN_LENGTH_MINUS1_0 + i*4);
			break;
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_SCATTERED_SLICES:
			if (p_264->fmo_slice_num_grp > 4)
				p_264->fmo_slice_num_grp = 4;
			break;
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_RASTER_SCAN:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_WIPE_SCAN:
			if (p_264->fmo_slice_num_grp > 2)
				p_264->fmo_slice_num_grp = 2;
			WRITEL(p_264->fmo_sg_dir & 0x1,
				S5P_FIMV_E_H264_FMO_SLICE_GRP_CHANGE_DIR);
			/* the valid range is 0 ~ number of macroblocks -1 */
			WRITEL(p_264->fmo_sg_rate, S5P_FIMV_E_H264_FMO_SLICE_GRP_CHANGE_RATE_MINUS1);
			break;
		default:
			mfc_err("Unsupported map type for FMO: %d\n",
					p_264->fmo_slice_map_type);
			p_264->fmo_slice_map_type = 0;
			p_264->fmo_slice_num_grp = 1;
			break;
		}

		WRITEL(p_264->fmo_slice_map_type, S5P_FIMV_E_H264_FMO_SLICE_GRP_MAP_TYPE);
		WRITEL(p_264->fmo_slice_num_grp - 1, S5P_FIMV_E_H264_FMO_NUM_SLICE_GRP_MINUS1);
	} else {
		WRITEL(0, S5P_FIMV_E_H264_FMO_NUM_SLICE_GRP_MINUS1);
	}

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_set_enc_params_mpeg4(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_mpeg4_enc_params *p_mpeg4 = &p->codec.mpeg4;
	unsigned int reg = 0;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* pictype : number of B */
	reg = READL(S5P_FIMV_E_GOP_CONFIG);
	/** num_b_frame - 0 ~ 2 */
	reg &= ~(0x3 << 16);
	reg |= (p->num_b_frame << 16);
	WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);

	/* profile & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_mpeg4->level << 8);
	/** profile - 0 ~ 1 */
	reg &= ~(0x3F);
	reg |= p_mpeg4->profile;
	WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* quarter_pixel */
	/* WRITEL(p_mpeg4->quarter_pixel, S5P_FIMV_ENC_MPEG4_QUART_PXL); */

	/* qp */
	WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 16);
		reg |= (p_mpeg4->rc_b_frame_qp << 16);
		reg &= ~(0x3f << 8);
		reg |= (p_mpeg4->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_mpeg4->rc_frame_qp;
		WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* frame rate */
	if (p->rc_frame) {
		p->rc_frame_delta = p_mpeg4->vop_frm_delta;
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= (p_mpeg4->vop_time_res << 16);
		reg &= ~(0xffff);
		reg |= p_mpeg4->vop_frm_delta;
		WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	} else {
		p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	}

	/* rate control config. */
	reg = READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_frame_qp;
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_mpeg4->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_min_qp;
	WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* initialize for '0' only setting*/
	WRITEL(0x0, S5P_FIMV_E_MPEG4_OPTIONS); /* SEQ_start only */
	WRITEL(0x0, S5P_FIMV_E_MPEG4_HEC_PERIOD); /* SEQ_start only */

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_set_enc_params_h263(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_mpeg4_enc_params *p_mpeg4 = &p->codec.mpeg4;
	unsigned int reg = 0;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* profile & level */
	reg = 0;
	/** profile */
	reg |= (0x1 << 4);
	WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* qp */
	WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 8);
		reg |= (p_mpeg4->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_mpeg4->rc_frame_qp;
		WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* frame rate */
	/* Fix value for H.264, H.263 in the driver */
	p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_mpeg4->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* rate control config. */
	reg = READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_frame_qp;
	WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_mpeg4->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_min_qp;
	WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	mfc_debug_leave();

	return 0;
}

/* Initialize decoding */
int s5p_mfc_init_decode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	unsigned int reg = 0;
	int fmo_aso_ctrl = 0;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}
	mfc_info("InstNo: %d/%d\n", ctx->inst_no, S5P_FIMV_CH_SEQ_HEADER);
	mfc_info("BUFs: %08x %08x %08x\n",
		  READL(S5P_FIMV_D_CPB_BUFFER_ADDR),
		  READL(S5P_FIMV_D_CPB_BUFFER_ADDR),
		  READL(S5P_FIMV_D_CPB_BUFFER_ADDR));

	reg |= (dec->idr_decoding << S5P_FIMV_D_OPT_IDR_DECODING_SHFT);
	/* FMO_ASO_CTRL - 0: Enable, 1: Disable */
	reg |= (fmo_aso_ctrl << S5P_FIMV_D_OPT_FMO_ASO_CTRL_MASK);
	/* When user sets desplay_delay to 0,
	 * It works as "display_delay enable" and delay set to 0.
	 * If user wants display_delay disable, It should be
	 * set to negative value. */
	if (dec->display_delay >= 0) {
		reg |= (0x1 << S5P_FIMV_D_OPT_DDELAY_EN_SHIFT);
		WRITEL(dec->display_delay, S5P_FIMV_D_DISPLAY_DELAY);
	}
	/* Setup loop filter, for decoding this is only valid for MPEG4 */
	if ((ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_DEC) &&
			!FW_HAS_INITBUF_LOOP_FILTER(dev)) {
		mfc_debug(2, "Set loop filter to: %d\n", dec->loop_filter_mpeg4);
		reg |= (dec->loop_filter_mpeg4 << S5P_FIMV_D_OPT_LF_CTRL_SHIFT);
	}
	if ((ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) &&
			!FW_HAS_INITBUF_TILE_MODE(dev))
		reg |= (0x1 << S5P_FIMV_D_OPT_TILE_MODE_SHIFT);

	/* VC1 RCV: Discard to parse additional header as default */
	if (ctx->codec_mode == S5P_FIMV_CODEC_VC1RCV_DEC)
		reg |= (0x1 << S5P_FIMV_D_OPT_DISCARD_RCV_HEADER);

	WRITEL(reg, S5P_FIMV_D_DEC_OPTIONS);

	if (ctx->codec_mode == S5P_FIMV_CODEC_FIMV1_DEC) {
		mfc_debug(2, "Setting FIMV1 resolution to %dx%d\n",
					ctx->img_width, ctx->img_height);
		WRITEL(ctx->img_width, S5P_FIMV_D_SET_FRAME_WIDTH);
		WRITEL(ctx->img_height, S5P_FIMV_D_SET_FRAME_HEIGHT);
	}

	/* 0: NV12(CbCr), 1: NV21(CrCb) */
	if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV21M)
		WRITEL(0x1, S5P_FIMV_PIXEL_FORMAT);
	else
		WRITEL(0x0, S5P_FIMV_PIXEL_FORMAT);

	/* sei parse */
	reg = dec->sei_parse;
	/* Enable realloc interface if SEI is enabled */
	if (dec->sei_parse && FW_HAS_SEI_S3D_REALLOC(dev))
		reg |= (0x1 << S5P_FIMV_D_SEI_NEED_INIT_BUFFER_SHIFT);
	WRITEL(reg, S5P_FIMV_D_SEI_ENABLE);

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_CH_SEQ_HEADER, NULL);

	mfc_debug_leave();
	return 0;
}

static inline void s5p_mfc_set_flush(struct s5p_mfc_ctx *ctx, int flush)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int dpb;
	if (flush)
		dpb = READL(S5P_FIMV_SI_CH0_DPB_CONF_CTRL) | (1 << 14);
	else
		dpb = READL(S5P_FIMV_SI_CH0_DPB_CONF_CTRL) & ~(1 << 14);
	WRITEL(dpb, S5P_FIMV_SI_CH0_DPB_CONF_CTRL);
}

/* Decode a single frame */
int s5p_mfc_decode_one_frame(struct s5p_mfc_ctx *ctx, int last_frame)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}
	mfc_debug(2, "Setting flags to %08lx (free:%d WTF:%d)\n",
				dec->dpb_status, ctx->dst_queue_cnt,
						dec->dpb_queue_cnt);
	WRITEL(dec->dpb_status, S5P_FIMV_D_AVAILABLE_DPB_FLAG_LOWER);
	WRITEL(0x0, S5P_FIMV_D_AVAILABLE_DPB_FLAG_UPPER);
	WRITEL(dec->slice_enable, S5P_FIMV_D_SLICE_IF_ENABLE);

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	/* Issue different commands to instance basing on whether it
	 * is the last frame or not. */
	switch (last_frame) {
	case 0:
		s5p_mfc_cmd_host2risc(S5P_FIMV_CH_FRAME_START, NULL);
		break;
	case 1:
		s5p_mfc_cmd_host2risc(S5P_FIMV_CH_LAST_FRAME, NULL);
		break;
	}

	mfc_debug(2, "Decoding a usual frame.\n");
	return 0;
}

int s5p_mfc_init_encode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	mfc_debug(2, "++\n");

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)
		s5p_mfc_set_enc_params_h264(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_ENC)
		s5p_mfc_set_enc_params_mpeg4(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_H263_ENC)
		s5p_mfc_set_enc_params_h263(ctx);
	else {
		mfc_err("Unknown codec for encoding (%x).\n",
			ctx->codec_mode);
		return -EINVAL;
	}

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_CH_SEQ_HEADER, NULL);

	mfc_debug(2, "--\n");

	return 0;
}

int s5p_mfc_h264_set_aso_slice_order(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_h264_enc_params *p_264 = &p->codec.h264;
	int i;

	if (p_264->aso_enable) {
		for (i = 0; i < 8; i++)
			WRITEL(p_264->aso_slice_order[i],
				S5P_FIMV_E_H264_ASO_SLICE_ORDER_0 + i * 4);
	}
	return 0;
}

/* Encode a single frame */
int s5p_mfc_encode_one_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	mfc_debug(2, "++\n");

	/* memory structure cur. frame */
	/*
	if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV12M)
		WRITEL(0, S5P_FIMV_ENC_MAP_FOR_CUR);
	else if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV12MT)
		WRITEL(3, S5P_FIMV_ENC_MAP_FOR_CUR);
	*/
	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)
		s5p_mfc_h264_set_aso_slice_order(ctx);

	s5p_mfc_set_slice_mode(ctx);

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_CH_FRAME_START, NULL);

	mfc_debug(2, "--\n");

	return 0;
}

static inline int s5p_mfc_get_new_ctx(struct s5p_mfc_dev *dev)
{
	int new_ctx;
	int cnt;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	mfc_debug(2, "Previous context: %d (bits %08lx)\n", dev->curr_ctx,
							dev->ctx_work_bits);

	if (dev->preempt_ctx > MFC_NO_INSTANCE_SET)
		return dev->preempt_ctx;
	else
		new_ctx = (dev->curr_ctx + 1) % MFC_NUM_CONTEXTS;

	cnt = 0;
	while (!test_bit(new_ctx, &dev->ctx_work_bits)) {
		new_ctx = (new_ctx + 1) % MFC_NUM_CONTEXTS;
		cnt++;
		if (cnt > MFC_NUM_CONTEXTS) {
			/* No contexts to run */
			return -EAGAIN;
		}
	}

	return new_ctx;
}

static inline int s5p_mfc_run_dec_last_frames(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *temp_vb;
	unsigned long flags;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->irqlock, flags);

	/* Frames are being decoded */
	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "No src buffers.\n");
		s5p_mfc_set_dec_stream_buffer(ctx, 0, 0, 0);
	} else {
		/* Get the next source buffer */
		temp_vb = list_entry(ctx->src_queue.next,
					struct s5p_mfc_buf, list);
		temp_vb->used = 1;
		s5p_mfc_set_dec_stream_buffer(ctx,
			s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0), 0, 0);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_decode_one_frame(ctx, 1);

	return 0;
}

static inline int s5p_mfc_run_dec_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *temp_vb;
	unsigned long flags;
	int last_frame = 0;
	unsigned int index;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->irqlock, flags);

	/* Frames are being decoded */
	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "No src buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}
	if (ctx->dst_queue_cnt < ctx->dpb_count) {
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}
	/* Get the next source buffer */
	temp_vb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	temp_vb->used = 1;
	mfc_debug(2, "Temp vb: %p\n", temp_vb);
	mfc_debug(2, "Src Addr: 0x%08lx\n",
		(unsigned long)s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0));
	s5p_mfc_set_dec_stream_buffer(ctx,
			s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0),
			0, temp_vb->vb.v4l2_planes[0].bytesused);
	spin_unlock_irqrestore(&dev->irqlock, flags);

	index = temp_vb->vb.v4l2_buf.index;
	if (call_cop(ctx, set_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
		mfc_err("failed in set_buf_ctrls_val\n");

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);
	if (temp_vb->vb.v4l2_planes[0].bytesused == 0 ||
			temp_vb->vb.v4l2_buf.input == DEC_LAST_FRAME) {
		last_frame = 1;
		mfc_debug(2, "Setting ctx->state to FINISHING\n");
		ctx->state = MFCINST_FINISHING;
	}
	s5p_mfc_decode_one_frame(ctx, last_frame);

	return 0;
}

static inline int s5p_mfc_run_enc_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *dst_mb;
	struct s5p_mfc_buf *src_mb;
	dma_addr_t src_y_addr, src_c_addr, dst_addr;
	/*
	unsigned int src_y_size, src_c_size;
	*/
	unsigned int dst_size;
	unsigned int index;

	spin_lock_irqsave(&dev->irqlock, flags);

	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "no src buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	if (list_empty(&ctx->dst_queue)) {
		mfc_debug(2, "no dst buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	src_mb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	src_mb->used = 1;
	src_y_addr = s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, 0);
	src_c_addr = s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, 1);

	mfc_debug(2, "enc src y addr: 0x%08lx", (unsigned long)src_y_addr);
	mfc_debug(2, "enc src c addr: 0x%08lx", (unsigned long)src_c_addr);

	s5p_mfc_set_enc_frame_buffer(ctx, src_y_addr, src_c_addr);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_mb->used = 1;
	dst_addr = s5p_mfc_mem_plane_addr(ctx, &dst_mb->vb, 0);
	dst_size = vb2_plane_size(&dst_mb->vb, 0);

	s5p_mfc_set_enc_stream_buffer(ctx, dst_addr, dst_size);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	index = src_mb->vb.v4l2_buf.index;
	if (call_cop(ctx, set_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
		mfc_err("failed in set_buf_ctrls_val\n");

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_encode_one_frame(ctx);

	return 0;
}

static inline void s5p_mfc_run_init_dec(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	unsigned long flags;
	struct s5p_mfc_buf *temp_vb;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}
	/* Initializing decoding - parsing header */
	spin_lock_irqsave(&dev->irqlock, flags);
	mfc_info("Preparing to init decoding.\n");
	temp_vb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	mfc_info("Header size: %d\n", temp_vb->vb.v4l2_planes[0].bytesused);
	s5p_mfc_set_dec_stream_buffer(ctx,
			s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0),
			0, temp_vb->vb.v4l2_planes[0].bytesused);
	spin_unlock_irqrestore(&dev->irqlock, flags);
	dev->curr_ctx = ctx->num;
	mfc_debug(2, "Header addr: 0x%08lx\n",
		(unsigned long)s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0));
	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_init_decode(ctx);
}

static inline int s5p_mfc_run_init_enc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *dst_mb;
	dma_addr_t dst_addr;
	unsigned int dst_size;
	int ret;

	spin_lock_irqsave(&dev->irqlock, flags);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_addr = s5p_mfc_mem_plane_addr(ctx, &dst_mb->vb, 0);
	dst_size = vb2_plane_size(&dst_mb->vb, 0);
	s5p_mfc_set_enc_stream_buffer(ctx, dst_addr, dst_size);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	dev->curr_ctx = ctx->num;
	mfc_info("Header addr: 0x%08lx\n",
		(unsigned long)s5p_mfc_mem_plane_addr(ctx, &dst_mb->vb, 0));
	s5p_mfc_clean_ctx_int_flags(ctx);

	ret = s5p_mfc_init_encode(ctx);
	return ret;
}

static inline int s5p_mfc_run_init_dec_buffers(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	int ret;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	/* Initializing decoding - parsing header */
	/* Header was parsed now starting processing
	 * First set the output frame buffers
	 * s5p_mfc_alloc_dec_buffers(ctx); */

	if (ctx->capture_state != QUEUE_BUFS_MMAPED) {
		mfc_err("It seems that not all destionation buffers were "
			"mmaped.\nMFC requires that all destination are mmaped "
			"before starting processing.\n");
		return -EAGAIN;
	}

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);
	ret = s5p_mfc_set_dec_frame_buffer(ctx);
	if (ret) {
		mfc_err("Failed to alloc frame mem.\n");
		ctx->state = MFCINST_ERROR;
	}
	return ret;
}

static inline int s5p_mfc_run_init_enc_buffers(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret;

	ret = s5p_mfc_alloc_codec_buffers(ctx);
	if (ret) {
		mfc_err("Failed to allocate encoding buffers.\n");
		return -ENOMEM;
	}

	/* Header was generated now starting processing
	 * First set the reference frame buffers
	 */
	if (ctx->capture_state != QUEUE_BUFS_REQUESTED) {
		mfc_err("It seems that destionation buffers were not "
			"requested.\nMFC requires that header should be generated "
			"before allocating codec buffer.\n");
		return -EAGAIN;
	}

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);
	ret = s5p_mfc_set_enc_ref_buffer(ctx);
	if (ret) {
		mfc_err("Failed to alloc frame mem.\n");
		ctx->state = MFCINST_ERROR;
	}
	return ret;
}

static inline int s5p_mfc_abort_inst(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_CH_NAL_ABORT, NULL);

	return 0;
}

static inline int s5p_mfc_dec_dpb_flush(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	dev->curr_ctx = ctx->num;
	s5p_mfc_clean_ctx_int_flags(ctx);

	WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(S5P_FIMV_H2R_CMD_FLUSH, NULL);

	return 0;
}

static inline int s5p_mfc_ctx_ready(struct s5p_mfc_ctx *ctx)
{
	if (ctx->type == MFCINST_DECODER)
		return s5p_mfc_dec_ctx_ready(ctx);
	else if (ctx->type == MFCINST_ENCODER)
		return s5p_mfc_enc_ctx_ready(ctx);

	return 0;
}

/* Try running an operation on hardware */
void s5p_mfc_try_run(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_ctx *ctx;
	int new_ctx;
	unsigned int ret = 0;

	mfc_debug(1, "Try run dev: %p\n", dev);
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	spin_lock_irq(&dev->condlock);
	/* Check whether hardware is not running */
	if (dev->hw_lock != 0) {
		spin_unlock_irq(&dev->condlock);
		/* This is perfectly ok, the scheduled ctx should wait */
		mfc_debug(1, "Couldn't lock HW.\n");
		return;
	}

	/* Choose the context to run */
	new_ctx = s5p_mfc_get_new_ctx(dev);
	if (new_ctx < 0) {
		/* No contexts to run */
		spin_unlock_irq(&dev->condlock);
		mfc_debug(1, "No ctx is scheduled to be run.\n");
		return;
	}

	ctx = dev->ctx[new_ctx];
	if (!ctx) {
		spin_unlock_irq(&dev->condlock);
		mfc_err("no mfc context to run\n");
		return;
	}

	if (test_and_set_bit(ctx->num, &dev->hw_lock) != 0) {
		spin_unlock_irq(&dev->condlock);
		mfc_err("Failed to lock hardware.\n");
		return;
	}
	spin_unlock_irq(&dev->condlock);

	mfc_debug(1, "New context: %d\n", new_ctx);
	mfc_debug(1, "Seting new context to %p\n", ctx);

	/* Got context to run in ctx */
	mfc_debug(1, "ctx->dst_queue_cnt=%d ctx->dpb_count=%d ctx->src_queue_cnt=%d\n",
		ctx->dst_queue_cnt, ctx->dpb_count, ctx->src_queue_cnt);
	mfc_debug(1, "ctx->state=%d\n", ctx->state);
	/* Last frame has already been sent to MFC
	 * Now obtaining frames from MFC buffer */

	dev->curr_ctx_drm = ctx->is_drm;

	s5p_mfc_clock_on();

	if (ctx->type == MFCINST_DECODER) {
		switch (ctx->state) {
		case MFCINST_FINISHING:
			ret = s5p_mfc_run_dec_last_frames(ctx);
			break;
		case MFCINST_RUNNING:
			ret = s5p_mfc_run_dec_frame(ctx);
			break;
		case MFCINST_INIT:
			ret = s5p_mfc_open_inst(ctx);
			break;
		case MFCINST_RETURN_INST:
			ret = s5p_mfc_close_inst(ctx);
			break;
		case MFCINST_GOT_INST:
			s5p_mfc_run_init_dec(ctx);
			break;
		case MFCINST_HEAD_PARSED:
			ret = s5p_mfc_run_init_dec_buffers(ctx);
			break;
		case MFCINST_RES_CHANGE_INIT:
			ret = s5p_mfc_run_dec_last_frames(ctx);
			break;
		case MFCINST_RES_CHANGE_FLUSH:
			ret = s5p_mfc_run_dec_last_frames(ctx);
			break;
		case MFCINST_RES_CHANGE_END:
			mfc_debug(2, "Finished remaining frames after resolution change.\n");
			ctx->capture_state = QUEUE_FREE;
			mfc_debug(2, "Will re-init the codec`.\n");
			s5p_mfc_run_init_dec(ctx);
			break;
		case MFCINST_DPB_FLUSHING:
			ret = s5p_mfc_dec_dpb_flush(ctx);
			break;
		default:
			ret = -EAGAIN;
		}
	} else if (ctx->type == MFCINST_ENCODER) {
		switch (ctx->state) {
		case MFCINST_FINISHING:
		case MFCINST_RUNNING:
		case MFCINST_RUNNING_NO_OUTPUT:
			ret = s5p_mfc_run_enc_frame(ctx);
			break;
		case MFCINST_INIT:
			ret = s5p_mfc_open_inst(ctx);
			break;
		case MFCINST_RETURN_INST:
			ret = s5p_mfc_close_inst(ctx);
			break;
		case MFCINST_GOT_INST:
			ret = s5p_mfc_run_init_enc(ctx);
			break;
		case MFCINST_HEAD_PARSED: /* Only for MFC6.x */
			ret = s5p_mfc_run_init_enc_buffers(ctx);
			break;
		case MFCINST_ABORT_INST:
			ret = s5p_mfc_abort_inst(ctx);
			break;
		default:
			ret = -EAGAIN;
		}
	} else {
		mfc_err("invalid context type: %d\n", ctx->type);
		ret = -EAGAIN;
	}

	if (ret) {
		/* Check again the ctx condition and clear work bits
		 * if ctx is not available. */
		if (s5p_mfc_ctx_ready(ctx) == 0) {
			spin_lock_irq(&dev->condlock);
			clear_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}

		/* Free hardware lock */
		if (clear_hw_bit(ctx) == 0)
			mfc_err("Failed to unlock hardware.\n");

		s5p_mfc_clock_off();

		/* Trigger again if other instance's work is waiting */
		spin_lock_irq(&dev->condlock);
		if (dev->ctx_work_bits)
			queue_work(dev->sched_wq, &dev->sched_work);
		spin_unlock_irq(&dev->condlock);
	}
}

void s5p_mfc_cleanup_queue(struct list_head *lh, struct vb2_queue *vq)
{
	struct s5p_mfc_buf *b;
	int i;

	while (!list_empty(lh)) {
		b = list_entry(lh->next, struct s5p_mfc_buf, list);
		for (i = 0; i < b->vb.num_planes; i++)
			vb2_set_plane_payload(&b->vb, i, 0);
		vb2_buffer_done(&b->vb, VB2_BUF_STATE_ERROR);
		list_del(&b->list);
	}
}

void s5p_mfc_write_info(struct s5p_mfc_ctx *ctx, unsigned int data, unsigned int ofs)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	/* MFC 6.x uses SFR for information */
	if (dev->hw_lock) {
		WRITEL(data, ofs);
	} else {
		s5p_mfc_clock_on();
		WRITEL(data, ofs);
		s5p_mfc_clock_off();
	}
}

unsigned int s5p_mfc_read_info(struct s5p_mfc_ctx *ctx, unsigned int ofs)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret;

	/* MFC 6.x uses SFR for information */
	if (dev->hw_lock) {
		ret = READL(ofs);
	} else {
		s5p_mfc_clock_on();
		ret = READL(ofs);
		s5p_mfc_clock_off();
	}

	return ret;
}
