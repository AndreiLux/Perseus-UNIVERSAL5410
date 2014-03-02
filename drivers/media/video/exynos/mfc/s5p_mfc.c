/*
 * Samsung S5P Multi Format Codec V5/V6
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * Kamil Debski, <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define DEBUG

#include <linux/io.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/proc_fs.h>
#include <mach/videonode.h>
#include <mach/sysmmu.h>
#include <plat/sysmmu.h>
#include <media/videobuf2-core.h>

#include "s5p_mfc_common.h"

#include "s5p_mfc_intr.h"
#include "s5p_mfc_inst.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_reg.h"
#include "s5p_mfc_ctrl.h"
#include "s5p_mfc_dec.h"
#include "s5p_mfc_enc.h"
#include "s5p_mfc_pm.h"

#define S5P_MFC_NAME		"s5p-mfc"
#define S5P_MFC_DEC_NAME	"s5p-mfc-dec"
#define S5P_MFC_ENC_NAME	"s5p-mfc-enc"
#define S5P_MFC_DEC_DRM_NAME	"s5p-mfc-dec-secure"
#define S5P_MFC_ENC_DRM_NAME	"s5p-mfc-enc-secure"

int debug;
module_param(debug, int, S_IRUGO | S_IWUSR);

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
static struct proc_dir_entry *mfc_proc_entry;

#define MFC_PROC_ROOT			"mfc"
#define MFC_PROC_INSTANCE_NUMBER	"instance_number"
#define MFC_PROC_DRM_INSTANCE_NUMBER	"drm_instance_number"
#define MFC_PROC_FW_STATUS		"fw_status"

#define MFC_DRM_MAGIC_SIZE	0x10
#define MFC_DRM_MAGIC_CHUNK0	0x13cdbf16
#define MFC_DRM_MAGIC_CHUNK1	0x8b803342
#define MFC_DRM_MAGIC_CHUNK2	0x5e87f4f5
#define MFC_DRM_MAGIC_CHUNK3	0x3bd05317

#define MFC_SFR_AREA_COUNT	11
static void s5p_mfc_dump_regs(struct s5p_mfc_dev *dev)
{
	int i;
	int addr[MFC_SFR_AREA_COUNT][2] = {
		{ 0x0, 0x50 },
		{ 0x1000, 0x138 },
		{ 0x2000, 0x604 },
		{ 0x6000, 0xA0 },
		{ 0x7000, 0x220 },
		{ 0x8000, 0x764 },
		{ 0x9000, 0xA04 },
		{ 0xA000, 0x200 },
		{ 0xB000, 0x40 },
		{ 0xD000, 0x70 },
		{ 0xF000, 0x5FF },
	};

	pr_err("dumping registers (SFR base = %p)\n", dev->regs_base);

	for (i = 0; i < MFC_SFR_AREA_COUNT; i++) {
		printk("[%04X .. %04X]\n", addr[i][0], addr[i][0] + addr[i][1]);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, dev->regs_base + addr[i][0],
				addr[i][1], false);
		printk("...\n");
	}
}

static int s5p_mfc_sysmmu_fault_handler(struct device *dev, const char *mmuname,
		enum exynos_sysmmu_inttype itype, unsigned long pgtable_base,
		unsigned long fault_addr)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(pdev);

	pr_err("MFC PAGE FAULT occurred at 0x%lx (Page table base: 0x%lx)\n",
			fault_addr, pgtable_base);

	s5p_mfc_dump_regs(m_dev);

	pr_err("dumping device info...\n-----------------------\n");
	pr_err("num_inst: %d\nint_cond: %d\nint_type: %d\nint_err: %u\n"
		"hw_lock: %lu\ncurr_ctx: %d\npreempt_ctx: %d\n"
		"ctx_work_bits: %lu\nclk_state: %lu\ncurr_ctx_drm: %d\n"
		"fw_status: %d\nnum_drm_inst: %d\ncurr_rate: %d\n\n",
		m_dev->num_inst, m_dev->int_cond, m_dev->int_type,
		m_dev->int_err, m_dev->hw_lock, m_dev->curr_ctx,
		m_dev->preempt_ctx, m_dev->ctx_work_bits, m_dev->clk_state,
		m_dev->curr_ctx_drm, m_dev->fw_status, m_dev->num_drm_inst,
		m_dev->curr_rate);

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");
	BUG();

	return IRQ_HANDLED;
}

#ifndef CONFIG_MFC_USE_SECURE_NODE
static int check_magic(unsigned char *addr)
{
	if (((u32)*(u32 *)(addr) == MFC_DRM_MAGIC_CHUNK0) &&
	    ((u32)*(u32 *)(addr + 0x4) == MFC_DRM_MAGIC_CHUNK1) &&
	    ((u32)*(u32 *)(addr + 0x8) == MFC_DRM_MAGIC_CHUNK2) &&
	    ((u32)*(u32 *)(addr + 0xC) == MFC_DRM_MAGIC_CHUNK3))
		return 0;
	else if (((u32)*(u32 *)(addr + 0x10) == MFC_DRM_MAGIC_CHUNK0) &&
	    ((u32)*(u32 *)(addr + 0x14) == MFC_DRM_MAGIC_CHUNK1) &&
	    ((u32)*(u32 *)(addr + 0x18) == MFC_DRM_MAGIC_CHUNK2) &&
	    ((u32)*(u32 *)(addr + 0x1C) == MFC_DRM_MAGIC_CHUNK3))
		return 0x10;
	else
		return -1;
}
#endif

static inline void clear_magic(unsigned char *addr)
{
	memset((void *)addr, 0x00, MFC_DRM_MAGIC_SIZE);
}
#endif

/*
 * A framerate table determines framerate by the interval(us) of each frame.
 * Framerate is not accurate, just rough value to seperate overload section.
 * Base line of each section are selected from 25fps(40000us), 48fps(20833us)
 * and 100fps(10000us).
 *
 * interval(us) | 0           10000         20833         40000           |
 * framerate    |     120fps    |    60fps    |    30fps    |    25fps    |
 */

#define COL_FRAME_RATE		0
#define COL_FRAME_INTERVAL	1
static unsigned long framerate_table[][2] = {
	{ 25000, 40000 },
	{ 30000, 20833 },
	{ 60000, 10000 },
	{ 120000, 0 },
};

static inline unsigned long timeval_diff(struct timeval *to,
					struct timeval *from)
{
	return (to->tv_sec * USEC_PER_SEC + to->tv_usec)
		- (from->tv_sec * USEC_PER_SEC + from->tv_usec);
}

int get_framerate(struct timeval *to, struct timeval *from)
{
	int i;
	unsigned long interval;

	if (timeval_compare(to, from) <= 0)
		return 0;

	interval = timeval_diff(to, from);

	/* if the interval is too big (2sec), framerate set to 0 */
	if (interval > (2 * USEC_PER_SEC))
		return 0;

	for (i = 0; i < ARRAY_SIZE(framerate_table); i++) {
		if (interval > framerate_table[i][COL_FRAME_INTERVAL])
			return framerate_table[i][COL_FRAME_RATE];
	}

	return 0;
}

void mfc_sched_worker(struct work_struct *work)
{
	struct s5p_mfc_dev *dev;

	dev = container_of(work, struct s5p_mfc_dev, sched_work);

	if (dev)
		s5p_mfc_try_run(dev);
	else
		mfc_err("no mfc device to run\n");
}

inline int clear_hw_bit(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret = -1;

	if (!atomic_read(&dev->watchdog_run))
		ret = test_and_clear_bit(ctx->num, &dev->hw_lock);

	return ret;
}

/* Helper functions for interrupt processing */
/* Remove from hw execution round robin */
inline void clear_work_bit(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = NULL;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	spin_lock(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock(&dev->condlock);
}

/* Wake up context wait_queue */
static inline void wake_up_ctx(struct s5p_mfc_ctx *ctx, unsigned int reason,
			       unsigned int err)
{
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	ctx->int_cond = 1;
	ctx->int_type = reason;
	ctx->int_err = err;
	if (ctx->state != MFCINST_ABORT && ctx->state != MFCINST_FREE)
		wake_up_interruptible(&ctx->queue);
	else
		wake_up(&ctx->queue);
}

/* Wake up device wait_queue */
static inline void wake_up_dev(struct s5p_mfc_dev *dev, unsigned int reason,
			       unsigned int err)
{
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dev->int_cond = 1;
	dev->int_type = reason;
	dev->int_err = err;
	wake_up(&dev->queue);
}

void s5p_mfc_watchdog(unsigned long arg)
{
	struct s5p_mfc_dev *dev = (struct s5p_mfc_dev *)arg;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	spin_lock_irq(&dev->condlock);
	if (dev->hw_lock)
		atomic_inc(&dev->watchdog_cnt);
	spin_unlock_irq(&dev->condlock);

	if (atomic_read(&dev->watchdog_cnt) >= MFC_WATCHDOG_CNT) {
		/* This means that hw is busy and no interrupts were
		 * generated by hw for the Nth time of running this
		 * watchdog timer. This usually means a serious hw
		 * error. Now it is time to kill all instances and
		 * reset the MFC. */
		mfc_err("Time out during waiting for HW.\n");
		queue_work(dev->watchdog_wq, &dev->watchdog_work);
	}
	dev->watchdog_timer.expires = jiffies +
					msecs_to_jiffies(MFC_WATCHDOG_INTERVAL);
	add_timer(&dev->watchdog_timer);
}

static void s5p_mfc_watchdog_worker(struct work_struct *work)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_ctx *ctx;
	int i, ret;
	int mutex_locked;
	unsigned long flags;
	int ref_cnt;

	dev = container_of(work, struct s5p_mfc_dev, watchdog_work);

	if (atomic_read(&dev->watchdog_run)) {
		mfc_err("watchdog already running???\n");
		return;
	}

	atomic_set(&dev->watchdog_run, 1);

	mfc_err("Driver timeout error handling.\n");
	/* Lock the mutex that protects open and release.
	 * This is necessary as they may load and unload firmware. */
	mutex_locked = mutex_trylock(&dev->mfc_mutex);
	if (!mutex_locked)
		mfc_err("This is not good. Some instance may be "
							"closing/opening.\n");

	/* Call clock on/off to make ref count 0 */
	ref_cnt = s5p_mfc_get_clk_ref_cnt();
	mfc_info("Clock reference count: %d\n", ref_cnt);
	if (ref_cnt < 0) {
		for (i = ref_cnt; i < 0; i++)
			s5p_mfc_clock_on();
	} else if (ref_cnt > 0) {
		for (i = ref_cnt; i > 0; i--)
			s5p_mfc_clock_off();
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	for (i = 0; i < MFC_NUM_CONTEXTS; i++) {
		ctx = dev->ctx[i];
		if (ctx && (ctx->inst_no != MFC_NO_INSTANCE_SET)) {
			ctx->state = MFCINST_ERROR;
			ctx->inst_no = MFC_NO_INSTANCE_SET;
			s5p_mfc_qos_off(ctx);
			s5p_mfc_cleanup_queue(&ctx->dst_queue,
				&ctx->vq_dst);
			s5p_mfc_cleanup_queue(&ctx->src_queue,
				&ctx->vq_src);
			clear_work_bit(ctx);
			wake_up_ctx(ctx, S5P_FIMV_R2H_CMD_ERR_RET, 0);
		}
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	spin_lock_irq(&dev->condlock);
	dev->hw_lock = 0;
	spin_unlock_irq(&dev->condlock);

	/* Double check if there is at least one instance running.
	 * If no instance is in memory than no firmware should be present */
	if (dev->num_inst > 0) {
		ret = s5p_mfc_load_firmware(dev);
		if (ret != 0) {
			mfc_err("Failed to reload FW.\n");
			goto watchdog_exit;
		}

		ret = s5p_mfc_init_hw(dev);
		if (ret != 0) {
			mfc_err("Failed to reinit FW.\n");
			goto watchdog_exit;
		}
	}

watchdog_exit:
	if (mutex_locked)
		mutex_unlock(&dev->mfc_mutex);

	atomic_set(&dev->watchdog_run, 0);
}

static inline enum s5p_mfc_node_type s5p_mfc_get_node_type(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	enum s5p_mfc_node_type node_type;

	if (!vdev) {
		mfc_err("failed to get video_device");
		return MFCNODE_INVALID;
	}

	mfc_debug(2, "video_device index: %d\n", vdev->index);

	switch (vdev->index) {
	case 0:
		node_type = MFCNODE_DECODER;
		break;
	case 1:
		node_type = MFCNODE_ENCODER;
		break;
	case 2:
		node_type = MFCNODE_DECODER_DRM;
		break;
	case 3:
		node_type = MFCNODE_ENCODER_DRM;
		break;
	default:
		node_type = MFCNODE_INVALID;
		break;
	}

	return node_type;
}

static void s5p_mfc_handle_frame_all_extracted(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_buf *dst_buf;
	int index, is_first = 1;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dec = ctx->dec_priv;

	mfc_debug(2, "Decided to finish\n");
	ctx->sequence++;
	while (!list_empty(&ctx->dst_queue)) {
		dst_buf = list_entry(ctx->dst_queue.next,
				     struct s5p_mfc_buf, list);
		mfc_debug(2, "Cleaning up buffer: %d\n",
					  dst_buf->vb.v4l2_buf.index);
		vb2_set_plane_payload(&dst_buf->vb, 0, 0);
		vb2_set_plane_payload(&dst_buf->vb, 1, 0);
		list_del(&dst_buf->list);
		ctx->dst_queue_cnt--;
		dst_buf->vb.v4l2_buf.sequence = (ctx->sequence++);

		if (s5p_mfc_read_info(ctx, PIC_TIME_TOP) ==
			s5p_mfc_read_info(ctx, PIC_TIME_BOT))
			dst_buf->vb.v4l2_buf.field = V4L2_FIELD_NONE;
		else
			dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED;

		clear_bit(dst_buf->vb.v4l2_buf.index, &dec->dpb_status);

		index = dst_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
			mfc_err("failed in get_buf_ctrls_val\n");

		if (is_first) {
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				dec->stored_tag);
			is_first = 0;
		} else {
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				DEFAULT_TAG);
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_VIDEO_H264_SEI_FP_AVAIL,
				0);
		}

		vb2_buffer_done(&dst_buf->vb, VB2_BUF_STATE_DONE);
		mfc_debug(2, "Cleaned up buffer: %d\n",
			  dst_buf->vb.v4l2_buf.index);
	}
	if (ctx->state != MFCINST_ABORT && ctx->state != MFCINST_HEAD_PARSED &&
			ctx->state != MFCINST_RES_CHANGE_FLUSH)
		ctx->state = MFCINST_RUNNING;
	mfc_debug(2, "After cleanup\n");
}

/*
 * Used only when dynamic DPB is enabled.
 * Check released buffers are enqueued again.
 */
static void mfc_check_ref_frame(struct s5p_mfc_ctx *ctx,
			struct list_head *ref_list, int ref_index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *ref_buf, *tmp_buf;
	int index;

	list_for_each_entry_safe(ref_buf, tmp_buf, ref_list, list) {
		index = ref_buf->vb.v4l2_buf.index;
		if (index == ref_index) {
			list_del(&ref_buf->list);
			dec->ref_queue_cnt--;

			list_add_tail(&ref_buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;

			dec->assigned_fd[index] =
					ref_buf->vb.v4l2_planes[0].m.fd;
			clear_bit(index, &dec->dpb_status);
			mfc_debug(2, "Move buffer[%d], fd[%d] to dst queue\n",
					index, dec->assigned_fd[index]);
			break;
		}
	}
}

/* Process the released reference information */
static void mfc_handle_released_info(struct s5p_mfc_ctx *ctx,
				struct list_head *dst_queue_addr,
				unsigned int released_flag, int index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct dec_dpb_ref_info *refBuf;
	int t, ncount = 0;

	refBuf = &dec->ref_info[index];

	if (released_flag) {
		for (t = 0; t < MFC_MAX_DPBS; t++) {
			if (released_flag & (1 << t)) {
				mfc_debug(2, "Release FD[%d] = %03d !! ",
						t, dec->assigned_fd[t]);
				refBuf->dpb[ncount].fd[0] = dec->assigned_fd[t];
				dec->assigned_fd[t] = MFC_INFO_INIT_FD;
				ncount++;
				mfc_check_ref_frame(ctx, dst_queue_addr, t);
			}
		}
	}

	if (ncount != MFC_MAX_DPBS)
		refBuf->dpb[ncount].fd[0] = MFC_INFO_INIT_FD;
}


static void s5p_mfc_handle_frame_copy_timestamp(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_buf *dst_buf, *src_buf;
	dma_addr_t dec_y_addr = MFC_GET_ADR(DEC_DECODED_Y);

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	/* Copy timestamp from consumed src buffer to decoded dst buffer */
	src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	list_for_each_entry(dst_buf, &ctx->dst_queue, list) {
		if (s5p_mfc_mem_plane_addr(ctx, &dst_buf->vb, 0) ==
								dec_y_addr) {
			memcpy(&dst_buf->vb.v4l2_buf.timestamp,
					&src_buf->vb.v4l2_buf.timestamp,
					sizeof(struct timeval));
			break;
		}
	}
}

static void s5p_mfc_handle_frame_new(struct s5p_mfc_ctx *ctx, unsigned int err)
{
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *dst_buf;
	dma_addr_t dspl_y_addr = MFC_GET_ADR(DEC_DISPLAY_Y);
	unsigned int index;
	unsigned int frame_type;
	int mvc_view_id;
	unsigned int dst_frame_status;
	struct list_head *dst_queue_addr;
	unsigned int prev_flag, released_flag = 0;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	frame_type = s5p_mfc_get_disp_frame_type();
	mvc_view_id = s5p_mfc_get_mvc_disp_view_id();

	if (IS_MFCV6(dev) && ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC) {
		if (mvc_view_id == 0)
			ctx->sequence++;
	} else {
		ctx->sequence++;
	}

	if (dec->immediate_display == 1) {
		dspl_y_addr = MFC_GET_ADR(DEC_DECODED_Y);
		frame_type = s5p_mfc_get_dec_frame_type();
	}

	/* If frame is same as previous then skip and do not dequeue */
	if (frame_type == S5P_FIMV_DISPLAY_FRAME_NOT_CODED)
		return;

	if (dec->is_dynamic_dpb) {
		prev_flag = dec->dynamic_used;
		dec->dynamic_used = mfc_get_dec_used_flag();
		released_flag = prev_flag & (~dec->dynamic_used);

		mfc_debug(2, "Used flag = %08x, Released Buffer = %08x\n",
				dec->dynamic_used, released_flag);
	}

	/* The MFC returns address of the buffer, now we have to
	 * check which videobuf does it correspond to */
	if (dec->is_dynamic_dpb)
		dst_queue_addr = &dec->ref_queue;
	else
		dst_queue_addr = &ctx->dst_queue;

	list_for_each_entry(dst_buf, dst_queue_addr, list) {
		mfc_debug(2, "Listing: %d\n", dst_buf->vb.v4l2_buf.index);
		/* Check if this is the buffer we're looking for */
		mfc_debug(2, "0x%08lx, 0x%08x",
				(unsigned long)s5p_mfc_mem_plane_addr(
							ctx, &dst_buf->vb, 0),
				dspl_y_addr);
		if (s5p_mfc_mem_plane_addr(ctx, &dst_buf->vb, 0)
							== dspl_y_addr) {
			index = dst_buf->vb.v4l2_buf.index;
			list_del(&dst_buf->list);

			if (dec->is_dynamic_dpb)
				dec->ref_queue_cnt--;
			else
				ctx->dst_queue_cnt--;

			dst_buf->vb.v4l2_buf.sequence = ctx->sequence;

			if (s5p_mfc_read_info(ctx, PIC_TIME_TOP) ==
				s5p_mfc_read_info(ctx, PIC_TIME_BOT))
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_NONE;
			else
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED;

			vb2_set_plane_payload(&dst_buf->vb, 0, ctx->luma_size);
			vb2_set_plane_payload(&dst_buf->vb, 1, ctx->chroma_size);
			clear_bit(index, &dec->dpb_status);

			dst_buf->vb.v4l2_buf.flags &=
					~(V4L2_BUF_FLAG_KEYFRAME |
					V4L2_BUF_FLAG_PFRAME |
					V4L2_BUF_FLAG_BFRAME);

			switch (frame_type) {
			case S5P_FIMV_DISPLAY_FRAME_I:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_KEYFRAME;
				break;
			case S5P_FIMV_DISPLAY_FRAME_P:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_PFRAME;
				break;
			case S5P_FIMV_DISPLAY_FRAME_B:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_BFRAME;
				break;
			default:
				break;
			}

			if (s5p_mfc_err_dspl(err))
				mfc_err("Warning for displayed frame: %d\n",
							s5p_mfc_err_dspl(err));

			if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
				mfc_err("failed in get_buf_ctrls_val\n");

			if (dec->is_dynamic_dpb)
				mfc_handle_released_info(ctx, dst_queue_addr,
							released_flag, index);

			if (dec->immediate_display == 1) {
				dst_frame_status = s5p_mfc_get_dec_status()
					& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;

				call_cop(ctx, get_buf_update_val, ctx,
						&ctx->dst_ctrls[index],
						V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
						dst_frame_status);

				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
					dec->stored_tag);

				dec->immediate_display = 0;
			}

			/* Update frame tag for packed PB */
			if (dec->is_packedpb &&
					(dec->y_addr_for_pb == dspl_y_addr)) {
				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
					dec->stored_tag);
				dec->y_addr_for_pb = 0;
			}

			if (!dec->is_dts_mode) {
				mfc_debug(7, "timestamp: %ld %ld\n",
					dst_buf->vb.v4l2_buf.timestamp.tv_sec,
					dst_buf->vb.v4l2_buf.timestamp.tv_usec);
				mfc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
				ctx->last_framerate =
					(ctx->qos_ratio * get_framerate(
						&dst_buf->vb.v4l2_buf.timestamp,
						&ctx->last_timestamp)) / 100;

				memcpy(&ctx->last_timestamp,
					&dst_buf->vb.v4l2_buf.timestamp,
					sizeof(struct timeval));
			}

			vb2_buffer_done(&dst_buf->vb,
				s5p_mfc_err_dspl(err) ?
				VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);

			break;
		}
	}
}

static int s5p_mfc_find_start_code(unsigned char *src_mem, unsigned int remainSize)
{
	unsigned int index = 0;

	for (index = 0; index < remainSize - 3; index++) {
		if ((src_mem[index] == 0x00) && (src_mem[index+1] == 0x00) &&
				(src_mem[index+2] == 0x01))
			return index;
	}

	return -1;
}

static void s5p_mfc_handle_frame_error(struct s5p_mfc_ctx *ctx,
		unsigned int reason, unsigned int err)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_buf *src_buf;
	unsigned long flags;
	unsigned int index;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	mfc_err("Interrupt Error: %d\n", err);

	dec->dpb_flush = 0;
	dec->remained = 0;

	spin_lock_irqsave(&dev->irqlock, flags);
	if (!list_empty(&ctx->src_queue)) {
		src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
		index = src_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, recover_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err("failed in recover_buf_ctrls_val\n");

		mfc_debug(2, "MFC needs next buffer.\n");
		dec->consumed = 0;
		list_del(&src_buf->list);
		ctx->src_queue_cnt--;

		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err("failed in get_buf_ctrls_val\n");

		vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_ERROR);
	}

	mfc_debug(2, "Assesing whether this context should be run again.\n");
	/* This context state is always RUNNING */
	if (ctx->src_queue_cnt == 0 || ctx->dst_queue_cnt < ctx->dpb_count) {
		mfc_debug(2, "No need to run again.\n");
		clear_work_bit(ctx);
	}
	mfc_debug(2, "After assesing whether this context should be run again. %d\n", ctx->src_queue_cnt);

	s5p_mfc_clear_int_flags();
	if (clear_hw_bit(ctx) == 0)
		BUG();
	wake_up_ctx(ctx, reason, err);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	s5p_mfc_clock_off();

	queue_work(dev->sched_wq, &dev->sched_work);
}

static void s5p_mfc_handle_ref_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *dec_buf;
	dma_addr_t dec_addr, buf_addr;

	dec_buf = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);

	dec_addr = MFC_GET_ADR(DEC_DECODED_Y);
	buf_addr = s5p_mfc_mem_plane_addr(ctx, &dec_buf->vb, 0);

	if ((buf_addr == dec_addr) && (dec_buf->used == 1)) {
		mfc_debug(2, "Find dec buffer y = 0x%x\n", dec_addr);

		list_del(&dec_buf->list);
		ctx->dst_queue_cnt--;

		list_add_tail(&dec_buf->list, &dec->ref_queue);
		dec->ref_queue_cnt++;
	} else {
		mfc_debug(2, "Can't find buffer for addr = 0x%x\n", dec_addr);
		mfc_debug(2, "Expected addr = 0x%x, used = %d\n",
						buf_addr, dec_buf->used);
	}
}

/* Handle frame decoding interrupt */
static void s5p_mfc_handle_frame(struct s5p_mfc_ctx *ctx,
					unsigned int reason, unsigned int err)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	unsigned int dst_frame_status, sei_avail_status;
	struct s5p_mfc_buf *src_buf;
	unsigned long flags;
	unsigned int res_change;
	unsigned int index, remained;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	dst_frame_status = s5p_mfc_get_dspl_status()
				& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;
	res_change = (s5p_mfc_get_dspl_status()
				& S5P_FIMV_DEC_STATUS_RESOLUTION_MASK)
				>> S5P_FIMV_DEC_STATUS_RESOLUTION_SHIFT;
	sei_avail_status = s5p_mfc_get_sei_avail_status();

	if (dec->immediate_display == 1)
		dst_frame_status = s5p_mfc_get_dec_status()
				& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;

	mfc_debug(2, "Frame Status: %x\n", dst_frame_status);
	mfc_debug(2, "SEI available status: %x\n", sei_avail_status);
	mfc_debug(2, "Used flag: old = %08x, new = %08x\n",
				dec->dynamic_used, mfc_get_dec_used_flag());

	if (ctx->state == MFCINST_RES_CHANGE_INIT)
		ctx->state = MFCINST_RES_CHANGE_FLUSH;

	if (res_change) {
		mfc_info("Resolution change set to %d\n", res_change);
		ctx->state = MFCINST_RES_CHANGE_INIT;
		ctx->wait_state = WAIT_DECODING;
		mfc_debug(2, "Decoding waiting! : %d\n", ctx->wait_state);

		s5p_mfc_clear_int_flags();
		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);

		s5p_mfc_clock_off();

		queue_work(dev->sched_wq, &dev->sched_work);
		return;
	}
	if (dec->dpb_flush)
		dec->dpb_flush = 0;
	if (dec->remained)
		dec->remained = 0;

	spin_lock_irqsave(&dev->irqlock, flags);
	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC &&
		dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_ONLY &&
		FW_HAS_SEI_S3D_REALLOC(dev) && sei_avail_status) {
		mfc_info("Frame packing SEI exists for a frame.\n");
		mfc_info("Reallocate DPBs and issue init_buffer.\n");
		ctx->is_dpb_realloc = 1;
		ctx->state = MFCINST_HEAD_PARSED;
		ctx->capture_state = QUEUE_FREE;
		ctx->wait_state = WAIT_DECODING;
		s5p_mfc_handle_frame_all_extracted(ctx);
		goto leave_handle_frame;
	}

	/* All frames remaining in the buffer have been extracted  */
	if (dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_EMPTY) {
		if (ctx->state == MFCINST_RES_CHANGE_FLUSH) {
			mfc_debug(2, "Last frame received after resolution change.\n");
			s5p_mfc_handle_frame_all_extracted(ctx);
			ctx->state = MFCINST_RES_CHANGE_END;
			goto leave_handle_frame;
		} else {
			s5p_mfc_handle_frame_all_extracted(ctx);
		}
	}

	if (dec->is_dynamic_dpb) {
		switch (dst_frame_status) {
		case S5P_FIMV_DEC_STATUS_DECODING_ONLY:
			dec->dynamic_used |= mfc_get_dec_used_flag();
			/* Fall through */
		case S5P_FIMV_DEC_STATUS_DECODING_DISPLAY:
			s5p_mfc_handle_ref_frame(ctx);
			break;
		default:
			break;
		}
	}

	if (dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_DISPLAY ||
	    dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_ONLY)
		s5p_mfc_handle_frame_copy_timestamp(ctx);

	/* A frame has been decoded and is in the buffer  */
	if (dst_frame_status == S5P_FIMV_DEC_STATUS_DISPLAY_ONLY ||
	    dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_DISPLAY) {
		s5p_mfc_handle_frame_new(ctx, err);
	} else {
		mfc_debug(2, "No frame decode.\n");
	}
	/* Mark source buffer as complete */
	if (dst_frame_status != S5P_FIMV_DEC_STATUS_DISPLAY_ONLY
		&& !list_empty(&ctx->src_queue)) {
		src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf,
								list);
		mfc_debug(2, "Packed PB test. Size:%d, prev offset: %ld, this run:"
			" %d\n", src_buf->vb.v4l2_planes[0].bytesused,
			dec->consumed, s5p_mfc_get_consumed_stream());
		dec->consumed += s5p_mfc_get_consumed_stream();
		remained = src_buf->vb.v4l2_planes[0].bytesused - dec->consumed;

		if (dec->is_packedpb && remained > STUFF_BYTE &&
			dec->consumed < src_buf->vb.v4l2_planes[0].bytesused &&
			s5p_mfc_get_dec_frame_type() ==
						S5P_FIMV_DECODED_FRAME_P) {
			unsigned char *stream_vir;
			int offset = 0;

			/* Run MFC again on the same buffer */
			mfc_debug(2, "Running again the same buffer.\n");

			dec->y_addr_for_pb = MFC_GET_ADR(DEC_DECODED_Y);

			stream_vir = vb2_plane_vaddr(&src_buf->vb, 0);
			s5p_mfc_mem_inv_vb(&src_buf->vb, 1);

			offset = s5p_mfc_find_start_code(
					stream_vir + dec->consumed, remained);

#if 0
			if (offset > STUFF_BYTE)
				dec->consumed += offset;

			s5p_mfc_set_dec_stream_buffer(ctx,
				src_buf->planes.stream, dec->consumed,
				src_buf->vb.v4l2_planes[0].bytesused -
							dec->consumed);
			dev->curr_ctx = ctx->num;
			dev->curr_ctx_drm = ctx->is_drm;
			s5p_mfc_clean_ctx_int_flags(ctx);
			s5p_mfc_clear_int_flags();
			wake_up_ctx(ctx, reason, err);
			spin_unlock_irqrestore(&dev->irqlock, flags);
			s5p_mfc_decode_one_frame(ctx, 0);
			return;
#else
			dec->remained_size = src_buf->vb.v4l2_planes[0].bytesused -
							dec->consumed;
			/* Do not move src buffer to done_list */
#endif
		} else {
			index = src_buf->vb.v4l2_buf.index;
			if (call_cop(ctx, recover_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
				mfc_err("failed in recover_buf_ctrls_val\n");

			mfc_debug(2, "MFC needs next buffer.\n");
			dec->consumed = 0;
			dec->remained_size = 0;
			list_del(&src_buf->list);
			ctx->src_queue_cnt--;

			if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
				mfc_err("failed in get_buf_ctrls_val\n");

			vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
		}
	}
leave_handle_frame:
	mfc_debug(2, "Assesing whether this context should be run again.\n");
	if (!s5p_mfc_dec_ctx_ready(ctx)) {
		mfc_debug(2, "No need to run again.\n");
		clear_work_bit(ctx);
	}
	mfc_debug(2, "After assesing whether this context should be run again.\n");

	s5p_mfc_clear_int_flags();
	if (clear_hw_bit(ctx) == 0)
		BUG();
	wake_up_ctx(ctx, reason, err);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	s5p_mfc_clock_off();

	queue_work(dev->sched_wq, &dev->sched_work);
}

/* Error handling for interrupt */
static inline void s5p_mfc_handle_error(struct s5p_mfc_ctx *ctx,
	unsigned int reason, unsigned int err)
{
	struct s5p_mfc_dev *dev;
	unsigned long flags;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_err("Interrupt Error: %d\n", err);
	s5p_mfc_clear_int_flags();
	wake_up_dev(dev, reason, err);

	/* Error recovery is dependent on the state of context */
	switch (ctx->state) {
	case MFCINST_INIT:
		/* This error had to happen while acquireing instance */
	case MFCINST_GOT_INST:
	case MFCINST_RES_CHANGE_END:
		/* This error had to happen while parsing the header */
	case MFCINST_HEAD_PARSED:
		/* This error had to happen while setting dst buffers */
	case MFCINST_RETURN_INST:
		/* This error had to happen while releasing instance */
	case MFCINST_DPB_FLUSHING:
		/* This error had to happen while flushing DPB */
		clear_work_bit(ctx);
		if (clear_hw_bit(ctx) == 0)
			BUG();
		s5p_mfc_clock_off();
		wake_up_ctx(ctx, reason, err);

		break;
	case MFCINST_FINISHING:
	case MFCINST_FINISHED:
		/* It is higly probable that an error occured
		 * while decoding a frame */
		clear_work_bit(ctx);
		ctx->state = MFCINST_ERROR;
		/* Mark all dst buffers as having an error */
		spin_lock_irqsave(&dev->irqlock, flags);
		s5p_mfc_cleanup_queue(&ctx->dst_queue, &ctx->vq_dst);
		/* Mark all src buffers as having an error */
		s5p_mfc_cleanup_queue(&ctx->src_queue, &ctx->vq_src);
		spin_unlock_irqrestore(&dev->irqlock, flags);
		if (clear_hw_bit(ctx) == 0)
			BUG();

		s5p_mfc_clock_off();

		break;
	default:
		mfc_err("Encountered an error interrupt which had not been handled.\n");
		mfc_err("ctx->state = %d, ctx->inst_no = %d\n",
						ctx->state, ctx->inst_no);

		clear_work_bit(ctx);
		if (test_and_clear_bit(ctx->num, &dev->hw_lock) == 0)
			BUG();
		s5p_mfc_clock_off();

		break;
	}
	return;
}

/* Interrupt processing */
static irqreturn_t s5p_mfc_irq(int irq, void *priv)
{
	struct s5p_mfc_dev *dev = priv;
	struct s5p_mfc_buf *src_buf;
	struct s5p_mfc_ctx *ctx;
	struct s5p_mfc_dec *dec = NULL;
	struct s5p_mfc_enc *enc = NULL;
	unsigned int reason;
	unsigned int err;
	unsigned long flags;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		goto irq_cleanup_hw;
	}

	/* Reset the timeout watchdog */
	atomic_set(&dev->watchdog_cnt, 0);

	if ((s5p_mfc_get_clk_ref_cnt() == 0) ||
			(s5p_mfc_get_power_ref_cnt() == 0)) {
		mfc_err("Not available to access MFC H/W\n");
		goto irq_cleanup_err;

	}
	/* Get the reason of interrupt and the error code */
	reason = s5p_mfc_get_int_reason();
	err = s5p_mfc_get_int_err();
	mfc_debug(2, "Int reason: %d (err: %d)\n", reason, err);

	switch (reason) {
	case S5P_FIMV_R2H_CMD_SYS_INIT_RET:
	case S5P_FIMV_R2H_CMD_FW_STATUS_RET:
	case S5P_FIMV_R2H_CMD_SLEEP_RET:
	case S5P_FIMV_R2H_CMD_WAKEUP_RET:
		s5p_mfc_clear_int_flags();
		/* Initialize hw_lock */
		atomic_clear_mask(HW_LOCK_CLEAR_MASK, &dev->hw_lock);
		wake_up_dev(dev, reason, err);
		goto done;
	}

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		goto irq_cleanup_hw;
	}

	if (ctx->type == MFCINST_DECODER)
		dec = ctx->dec_priv;
	else if (ctx->type == MFCINST_ENCODER)
		enc = ctx->enc_priv;

	dev->preempt_ctx = MFC_NO_INSTANCE_SET;

	switch (reason) {
	case S5P_FIMV_R2H_CMD_ERR_RET:
		/* An error has occured */
		if (ctx->state == MFCINST_RUNNING) {
			if ((s5p_mfc_err_dec(err) >= S5P_FIMV_ERR_WARNINGS_START) &&
				(s5p_mfc_err_dec(err) <= S5P_FIMV_ERR_WARNINGS_END))
				s5p_mfc_handle_frame(ctx, reason, err);
			else
				s5p_mfc_handle_frame_error(ctx, reason, err);
		} else {
			s5p_mfc_handle_error(ctx, reason, err);
		}
		break;
	case S5P_FIMV_R2H_CMD_SLICE_DONE_RET:
	case S5P_FIMV_R2H_CMD_FIELD_DONE_RET:
	case S5P_FIMV_R2H_CMD_FRAME_DONE_RET:
	case S5P_FIMV_R2H_CMD_COMPLETE_SEQ_RET:
		if (ctx->type == MFCINST_DECODER) {
			s5p_mfc_handle_frame(ctx, reason, err);
		} else if (ctx->type == MFCINST_ENCODER) {
			if (reason == S5P_FIMV_R2H_CMD_SLICE_DONE_RET) {
				dev->preempt_ctx = ctx->num;
				enc->in_slice = 1;
			} else {
				enc->in_slice = 0;
			}

			if (ctx->c_ops->post_frame_start) {
				if (ctx->c_ops->post_frame_start(ctx))
					mfc_err("post_frame_start() failed\n");

				s5p_mfc_clear_int_flags();
				if (clear_hw_bit(ctx) == 0)
					BUG();
				wake_up_ctx(ctx, reason, err);

				s5p_mfc_clock_off();

				queue_work(dev->sched_wq, &dev->sched_work);
			}
		}
		break;
	case S5P_FIMV_R2H_CMD_SEQ_DONE_RET:
		if (ctx->type == MFCINST_ENCODER) {
			if (ctx->c_ops->post_seq_start(ctx))
				mfc_err("post_seq_start() failed\n");
		} else if (ctx->type == MFCINST_DECODER) {
			if (ctx->src_fmt->fourcc != V4L2_PIX_FMT_FIMV1) {
				ctx->img_width = s5p_mfc_get_img_width();
				ctx->img_height = s5p_mfc_get_img_height();
			}

			ctx->dpb_count = s5p_mfc_get_dpb_count();
			dec->internal_dpb = 0;
			s5p_mfc_dec_store_crop_info(ctx);
			if (ctx->img_width == 0 || ctx->img_height == 0)
				ctx->state = MFCINST_ERROR;
			else
				ctx->state = MFCINST_HEAD_PARSED;

			if (ctx->state == MFCINST_HEAD_PARSED)
				dec->is_interlaced =
					s5p_mfc_is_interlace_picture();

			if ((ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
				ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC) &&
					!list_empty(&ctx->src_queue)) {
				struct s5p_mfc_buf *src_buf;
				src_buf = list_entry(ctx->src_queue.next,
						struct s5p_mfc_buf, list);
				mfc_debug(2, "Check consumed size of header. ");
				mfc_debug(2, "source : %d, consumed : %d\n",
						s5p_mfc_get_consumed_stream(),
						src_buf->vb.v4l2_planes[0].bytesused);
				if (s5p_mfc_get_consumed_stream() <
						src_buf->vb.v4l2_planes[0].bytesused)
					dec->remained = 1;
			}
		}

		s5p_mfc_clear_int_flags();
		clear_work_bit(ctx);
		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);

		s5p_mfc_clock_off();

		queue_work(dev->sched_wq, &dev->sched_work);
		break;
	case S5P_FIMV_R2H_CMD_OPEN_INSTANCE_RET:
		ctx->inst_no = s5p_mfc_get_inst_no();
		ctx->state = MFCINST_GOT_INST;
		clear_work_bit(ctx);
		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET:
		ctx->state = MFCINST_FREE;
		clear_work_bit(ctx);
		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case S5P_FIMV_R2H_CMD_NAL_ABORT_RET:
		ctx->state = MFCINST_ABORT;
		clear_work_bit(ctx);
		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case S5P_FIMV_R2H_CMD_DPB_FLUSH_RET:
		ctx->state = MFCINST_ABORT;
		clear_work_bit(ctx);
		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case S5P_FIMV_R2H_CMD_INIT_BUFFERS_RET:
		s5p_mfc_clear_int_flags();
		ctx->int_type = reason;
		ctx->int_err = err;
		ctx->int_cond = 1;
		spin_lock(&dev->condlock);
		clear_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock(&dev->condlock);
		if (err != 0) {
			if (clear_hw_bit(ctx) == 0)
				BUG();
			wake_up_interruptible(&ctx->queue);

			s5p_mfc_clock_off();
			break;
		}

		ctx->state = MFCINST_RUNNING;
		if (ctx->type == MFCINST_DECODER) {
			if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
				if (!dec->dpb_flush && !dec->remained) {
					mfc_debug(2, "INIT_BUFFERS with dpb_flush - leaving image in src queue.\n");
					spin_lock_irqsave(&dev->irqlock, flags);
					if (!list_empty(&ctx->src_queue)) {
						src_buf = list_entry(ctx->src_queue.next,
								struct s5p_mfc_buf, list);
						list_del(&src_buf->list);
						ctx->src_queue_cnt--;
						vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
					}
					spin_unlock_irqrestore(&dev->irqlock, flags);
				} else {
					if (dec->dpb_flush)
						dec->dpb_flush = 0;
				}
			}
			if (ctx->wait_state == WAIT_DECODING) {
				ctx->wait_state = WAIT_INITBUF_DONE;
				mfc_debug(2, "INIT_BUFFER has done, but can't start decoding\n");
			}
			if (ctx->is_dpb_realloc)
				ctx->is_dpb_realloc = 0;
			if (s5p_mfc_dec_ctx_ready(ctx)) {
				spin_lock(&dev->condlock);
				set_bit(ctx->num, &dev->ctx_work_bits);
				spin_unlock(&dev->condlock);
			}
		} else if (ctx->type == MFCINST_ENCODER) {
			if (s5p_mfc_enc_ctx_ready(ctx)) {
				spin_lock(&dev->condlock);
				set_bit(ctx->num, &dev->ctx_work_bits);
				spin_unlock(&dev->condlock);
			}
		}

		if (clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_interruptible(&ctx->queue);

		s5p_mfc_clock_off();

		queue_work(dev->sched_wq, &dev->sched_work);
		break;
	default:
		mfc_debug(2, "Unknown int reason.\n");
		s5p_mfc_clear_int_flags();
	}

done:
	mfc_debug_leave();
	return IRQ_HANDLED;

irq_cleanup_hw:
	s5p_mfc_clear_int_flags();

	s5p_mfc_clock_off();

	if (dev)
		queue_work(dev->sched_wq, &dev->sched_work);

irq_cleanup_err:

	mfc_debug(2, "via irq_cleanup_hw\n");
	return IRQ_HANDLED;
}

/* Open an MFC node */
static int s5p_mfc_open(struct file *file)
{
	struct s5p_mfc_ctx *ctx = NULL;
	struct s5p_mfc_dev *dev = video_drvdata(file);
	int ret = 0;
	enum s5p_mfc_node_type node;
	struct video_device *vdev = NULL;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	int magic_offset = -1;
	enum s5p_mfc_inst_drm_type is_drm = MFCDRM_NONE;
#endif

	mfc_info("mfc driver open called\n");

	if (!dev) {
		mfc_err("no mfc device to run\n");
		goto err_no_device;
	}

	node = s5p_mfc_get_node_type(file);
	if (node == MFCNODE_INVALID) {
		mfc_err("cannot specify node type\n");
		ret = -ENOENT;
		goto err_node_type;
	}

	dev->num_inst++;	/* It is guarded by mfc_mutex in vfd */

	/* Allocate memory for context */
	ctx = kzalloc(sizeof *ctx, GFP_KERNEL);
	if (!ctx) {
		mfc_err("Not enough memory.\n");
		ret = -ENOMEM;
		goto err_ctx_alloc;
	}

	switch (node) {
	case MFCNODE_DECODER:
		vdev = dev->vfd_dec;
		break;
	case MFCNODE_ENCODER:
		vdev = dev->vfd_enc;
		break;
	case MFCNODE_DECODER_DRM:
		vdev = dev->vfd_dec_drm;
		break;
	case MFCNODE_ENCODER_DRM:
		vdev = dev->vfd_enc_drm;
		break;
	default:
		mfc_err("Invalid node(%d)\n", node);
		break;
	}
	if (!vdev)
		goto err_vdev;

	v4l2_fh_init(&ctx->fh, vdev);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->dev = dev;

	/* Get context number */
	ctx->num = 0;
	while (dev->ctx[ctx->num]) {
		ctx->num++;
		if (ctx->num >= MFC_NUM_CONTEXTS) {
			mfc_err("Too many open contexts.\n");
			ret = -EBUSY;
			goto err_ctx_num;
		}
	}

	/* Mark context as idle */
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);
	dev->ctx[ctx->num] = ctx;

	init_waitqueue_head(&ctx->queue);

	if (is_decoder_node(node))
		ret = s5p_mfc_init_dec_ctx(ctx);
	else
		ret = s5p_mfc_init_enc_ctx(ctx);
	if (ret)
		goto err_ctx_init;

	ret = call_cop(ctx, init_ctx_ctrls, ctx);
	if (ret) {
		mfc_err("failed int init_buf_ctrls\n");
		goto err_ctx_ctrls;
	}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	/* Multi-instance is supported for same DRM type */
#ifdef CONFIG_MFC_USE_SECURE_NODE
	if (is_drm_node(node))
		is_drm = MFCDRM_SECURE_NODE;
#else
	magic_offset = check_magic(dev->drm_info.virt);
	if (magic_offset >= 0)
		is_drm = MFCDRM_MAGIC_KEY;
#endif
	if (is_drm == MFCDRM_NONE) {
		if (dev->num_drm_inst) {
			mfc_err("Can not open non-DRM instance\n");
			mfc_err("DRM instance is already opened.\n");
			ret = -EINVAL;
			goto err_drm_start;
		}
	} else {
		if (is_drm == MFCDRM_MAGIC_KEY)
			clear_magic(dev->drm_info.virt + magic_offset);

		if (dev->num_drm_inst < MFC_MAX_DRM_CTX) {
			mfc_info("DRM instance opened\n");

			dev->num_drm_inst++;
			ctx->is_drm = 1;
		} else {
			mfc_err("Too many instance are opened for DRM\n");
			ret = -EINVAL;
			goto err_drm_start;
		}
		if (dev->num_inst != dev->num_drm_inst) {
			mfc_err("Can not open DRM instance\n");
			mfc_err("Non-DRM instance is already opened.\n");
			ret = -EINVAL;
			goto err_drm_inst;
		}
	}
#endif

	/* Load firmware if this is the first instance */
	if (dev->num_inst == 1) {
		dev->watchdog_timer.expires = jiffies +
					msecs_to_jiffies(MFC_WATCHDOG_INTERVAL);
		add_timer(&dev->watchdog_timer);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		if (ctx->is_drm) {
			if (!dev->fw_status) {
				ret = s5p_mfc_alloc_firmware(dev);
				if (ret)
					goto err_fw_alloc;
			}

			dev->fw_status = 1;
		} else {
			if (!dev->fw_status) {
				ret = s5p_mfc_alloc_firmware(dev);
				if (ret)
					goto err_fw_alloc;

				ret = s5p_mfc_load_firmware(dev);
				if (ret)
					goto err_fw_load;

				dev->fw_status = 1;
			}
		}
#else
		/* Load the FW */
		ret = s5p_mfc_alloc_firmware(dev);
		if (ret)
			goto err_fw_alloc;

		ret = s5p_mfc_load_firmware(dev);
		if (ret)
			goto err_fw_load;

#endif
		ret = s5p_mfc_alloc_dev_context_buffer(dev);
		if (ret)
			goto err_fw_load;

		mfc_debug(2, "power on\n");
		ret = s5p_mfc_power_on();
		if (ret < 0) {
			mfc_err("power on failed\n");
			goto err_pwr_enable;
		}

		dev->curr_ctx = ctx->num;
		dev->preempt_ctx = MFC_NO_INSTANCE_SET;
		dev->curr_ctx_drm = ctx->is_drm;

		/* Init the FW */
		ret = s5p_mfc_init_hw(dev);
		if (ret) {
			mfc_err("Failed to init mfc h/w\n");
			goto err_hw_init;
		}
	}

	mfc_info("MFC instance open completed\n");
	return ret;

	/* Deinit when failure occured */
err_hw_init:
	s5p_mfc_power_off();

err_pwr_enable:
	s5p_mfc_release_dev_context_buffer(dev);

err_fw_load:
	s5p_mfc_release_firmware(dev);
	dev->fw_status = 0;

err_fw_alloc:
	del_timer_sync(&dev->watchdog_timer);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
err_drm_inst:
	if (ctx->is_drm)
		dev->num_drm_inst--;

err_drm_start:
#endif
	call_cop(ctx, cleanup_ctx_ctrls, ctx);

err_ctx_ctrls:
	if (is_decoder_node(node)) {
		kfree(ctx->dec_priv->ref_info);
		kfree(ctx->dec_priv);
	} else {
		kfree(ctx->enc_priv);
	}

err_ctx_init:
	dev->ctx[ctx->num] = 0;

err_ctx_num:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
err_vdev:
	kfree(ctx);

err_ctx_alloc:
	dev->num_inst--;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
/* err_drm_playback: // unused label */
#endif
err_node_type:
err_no_device:
	mfc_info("mfc driver open finished\n");

	return ret;
}

#define need_to_wait_frame_start(ctx)		\
	(((ctx->state == MFCINST_FINISHING) ||	\
	  (ctx->state == MFCINST_RUNNING)) &&	\
	 test_bit(ctx->num, &ctx->dev->hw_lock))
/* Release MFC context */
static int s5p_mfc_release(struct file *file)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dev *dev = NULL;
	struct s5p_mfc_enc *enc = NULL;

	mfc_info("mfc driver release called\n");

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (need_to_wait_frame_start(ctx)) {
		ctx->state = MFCINST_ABORT;
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_FRAME_DONE_RET, 0))
			s5p_mfc_cleanup_timeout(ctx);
	}

	if (ctx->type == MFCINST_ENCODER) {
		enc = ctx->enc_priv;
		if (!enc) {
			mfc_err("no mfc encoder to run\n");
			return -EINVAL;
		}

		if (enc->in_slice) {
			ctx->state = MFCINST_ABORT_INST;
			spin_lock_irq(&dev->condlock);
			set_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
			s5p_mfc_try_run(dev);
			if (s5p_mfc_wait_for_done_ctx(ctx,
					S5P_FIMV_R2H_CMD_NAL_ABORT_RET,
					0))
				s5p_mfc_cleanup_timeout(ctx);

			enc->in_slice = 0;
		}
	}

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	s5p_mfc_qos_off(ctx);
#endif

	if (call_cop(ctx, cleanup_ctx_ctrls, ctx) < 0)
		mfc_err("failed in init_buf_ctrls\n");

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	/* Mark context as idle */
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	/* If instance was initialised then
	 * return instance and free reosurces */
	if (!atomic_read(&dev->watchdog_run) &&
		(ctx->inst_no != MFC_NO_INSTANCE_SET)) {
		ctx->state = MFCINST_RETURN_INST;
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);

		/* Wait for hw_lock == 0 for this context */
		wait_event_interruptible_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(MFC_INT_TIMEOUT));

		/* To issue the command 'CLOSE_INSTANCE' */
		s5p_mfc_try_run(dev);

		/* Wait until instance is returned or timeout occured */
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET, 0)) {
			dev->curr_ctx_drm = ctx->is_drm;
			test_and_set_bit(ctx->num, &dev->hw_lock);
			s5p_mfc_clock_on();
			s5p_mfc_close_inst(ctx);
			if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET, 0)) {
				mfc_err("Abnormal h/w state.\n");

				/* cleanup for the next open */
				if (dev->curr_ctx == ctx->num)
					clear_bit(ctx->num, &dev->hw_lock);
				if (ctx->is_drm)
					dev->num_drm_inst--;
				dev->num_inst--;

				return -EIO;
			}
		}

		ctx->inst_no = MFC_NO_INSTANCE_SET;
	}
	/* hardware locking scheme */
	if (dev->curr_ctx == ctx->num)
		clear_bit(ctx->num, &dev->hw_lock);

	if (ctx->is_drm)
		dev->num_drm_inst--;
	dev->num_inst--;

	if (dev->num_inst == 0) {
		s5p_mfc_deinit_hw(dev);

		/* reset <-> F/W release */
		s5p_mfc_release_firmware(dev);
		s5p_mfc_release_dev_context_buffer(dev);
		dev->fw_status = 0;

		del_timer_sync(&dev->watchdog_timer);

		flush_workqueue(dev->sched_wq);

		mfc_info("power off\n");
		s5p_mfc_power_off();
	}

	/* Free resources */
	vb2_queue_release(&ctx->vq_src);
	vb2_queue_release(&ctx->vq_dst);

	s5p_mfc_release_codec_buffers(ctx);
	s5p_mfc_release_instance_buffer(ctx);
	if (ctx->type == MFCINST_DECODER)
		s5p_mfc_release_dec_desc_buffer(ctx);

	if (ctx->type == MFCINST_DECODER) {
		dec_cleanup_user_shared_handle(ctx);
		kfree(ctx->dec_priv->ref_info);
		kfree(ctx->dec_priv);
	} else if (ctx->type == MFCINST_ENCODER) {
		kfree(ctx->enc_priv);
	}
	dev->ctx[ctx->num] = 0;
	kfree(ctx);

	mfc_info("mfc driver release finished\n");

	return 0;
}

/* Poll */
static unsigned int s5p_mfc_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	unsigned int ret = 0;
	enum s5p_mfc_node_type node_type;

	node_type = s5p_mfc_get_node_type(file);

	if (is_decoder_node(node_type))
		ret = vb2_poll(&ctx->vq_src, file, wait);
	else
		ret = vb2_poll(&ctx->vq_dst, file, wait);

	return ret;
}

/* Mmap */
static int s5p_mfc_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	int ret;

	mfc_debug_enter();

	if (offset < DST_QUEUE_OFF_BASE) {
		mfc_debug(2, "mmaping source.\n");
		ret = vb2_mmap(&ctx->vq_src, vma);
	} else {		/* capture */
		mfc_debug(2, "mmaping destination.\n");
		vma->vm_pgoff -= (DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
		ret = vb2_mmap(&ctx->vq_dst, vma);
	}
	mfc_debug_leave();
	return ret;
}

/* v4l2 ops */
static const struct v4l2_file_operations s5p_mfc_fops = {
	.owner = THIS_MODULE,
	.open = s5p_mfc_open,
	.release = s5p_mfc_release,
	.poll = s5p_mfc_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = s5p_mfc_mmap,
};

/* videodec structure */
static struct video_device s5p_mfc_dec_videodev = {
	.name = S5P_MFC_DEC_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

static struct video_device s5p_mfc_enc_videodev = {
	.name = S5P_MFC_ENC_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

#ifdef CONFIG_MFC_USE_SECURE_NODE
static struct video_device s5p_mfc_dec_drm_videodev = {
	.name = S5P_MFC_DEC_DRM_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

static struct video_device s5p_mfc_enc_drm_videodev = {
	.name = S5P_MFC_ENC_DRM_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};
#endif

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
static int proc_read_inst_number(char *buf, char **start,
		off_t off, int count, int *eof, void *data)
{
	struct s5p_mfc_dev *dev = (struct s5p_mfc_dev *)data;
	int len = 0;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	len += snprintf(buf + len, count - len, "%d\n", dev->num_inst);

	return len;
}

static int proc_read_drm_inst_number(char *buf, char **start,
		off_t off, int count, int *eof, void *data)
{
	struct s5p_mfc_dev *dev = (struct s5p_mfc_dev *)data;
	int len = 0;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	len += snprintf(buf + len, count - len, "%d\n", dev->num_drm_inst);

	return len;
}

static int proc_read_fw_status(char *buf, char **start,
		off_t off, int count, int *eof, void *data)
{
	struct s5p_mfc_dev *dev = (struct s5p_mfc_dev *)data;
	int len = 0;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	len += snprintf(buf + len, count - len, "%d\n", dev->fw_status);

	return len;
}
#endif

/* MFC probe function */
static int __devinit s5p_mfc_probe(struct platform_device *pdev)
{
	struct s5p_mfc_dev *dev;
	struct video_device *vfd;
	struct resource *res;
	int ret = -ENOENT;
	unsigned int alloc_ctx_num;
#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	int i;
#endif

	dev_dbg(&pdev->dev, "%s()\n", __func__);
	dev = kzalloc(sizeof *dev, GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "Not enough memory for MFC device.\n");
		return -ENOMEM;
	}

	spin_lock_init(&dev->irqlock);
	spin_lock_init(&dev->condlock);

	dev->device = &pdev->dev;
	dev->pdata = pdev->dev.platform_data;

#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ)
	/* initial clock rate should be min rate */
	dev->curr_rate = dev->min_rate = dev->pdata->min_rate;
#endif
	ret = s5p_mfc_init_pm(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to setup mfc clock & power\n");
		goto err_pm;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		ret = -ENOENT;
		goto err_res_mem;
	}
	dev->mfc_mem = request_mem_region(res->start, resource_size(res),
					pdev->name);
	if (dev->mfc_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req_mem;
	}
	dev->regs_base = ioremap(dev->mfc_mem->start,
				resource_size(dev->mfc_mem));
	if (dev->regs_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap address region\n");
		ret = -ENOENT;
		goto err_ioremap;
	}

	s5p_mfc_init_reg(dev->regs_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		ret = -ENOENT;
		goto err_res_irq;
	}
	dev->irq = res->start;
	ret = request_threaded_irq(dev->irq, NULL, s5p_mfc_irq, IRQF_ONESHOT, pdev->name,
									dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to install irq (%d)\n", ret);
		goto err_req_irq;
	}

	mutex_init(&dev->mfc_mutex);

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		goto err_v4l2_dev;

	init_waitqueue_head(&dev->queue);

	/* decoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_dec;
	}
	*vfd = s5p_mfc_dec_videodev;

	vfd->ioctl_ops = get_dec_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	snprintf(vfd->name, sizeof(vfd->name), "%s", s5p_mfc_dec_videodev.name);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, S5P_VIDEONODE_MFC_DEC);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_dec;
	}
	v4l2_info(&dev->v4l2_dev, "decoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_dec = vfd;

	video_set_drvdata(vfd, dev);

	/* encoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_enc;
	}
	*vfd = s5p_mfc_enc_videodev;

	vfd->ioctl_ops = get_enc_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	snprintf(vfd->name, sizeof(vfd->name), "%s", s5p_mfc_enc_videodev.name);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, S5P_VIDEONODE_MFC_ENC);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_enc;
	}
	v4l2_info(&dev->v4l2_dev, "encoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_enc = vfd;

	video_set_drvdata(vfd, dev);

#ifdef CONFIG_MFC_USE_SECURE_NODE
	/* secure decoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_dec;
	}
	*vfd = s5p_mfc_dec_drm_videodev;

	vfd->ioctl_ops = get_dec_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	snprintf(vfd->name, sizeof(vfd->name), "%s",
					s5p_mfc_dec_drm_videodev.name);

	/* FIXME: Added 2 for secure node */
	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
					S5P_VIDEONODE_MFC_DEC + 2);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_dec;
	}
	v4l2_info(&dev->v4l2_dev, "secure decoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_dec_drm = vfd;

	video_set_drvdata(vfd, dev);

	/* secure encoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_enc;
	}
	*vfd = s5p_mfc_enc_drm_videodev;

	vfd->ioctl_ops = get_enc_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	snprintf(vfd->name, sizeof(vfd->name), "%s",
					s5p_mfc_enc_drm_videodev.name);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
					S5P_VIDEONODE_MFC_ENC + 2);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_enc;
	}
	v4l2_info(&dev->v4l2_dev, "secure encoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_enc_drm = vfd;

	video_set_drvdata(vfd, dev);
	/* end of node setting */
#endif

	platform_set_drvdata(pdev, dev);

	dev->hw_lock = 0;
	dev->watchdog_wq =
		create_singlethread_workqueue("s5p_mfc/watchdog");
	if (!dev->watchdog_wq) {
		dev_err(&pdev->dev, "failed to create workqueue for watchdog\n");
		goto err_wq_watchdog;
	}
	INIT_WORK(&dev->watchdog_work, s5p_mfc_watchdog_worker);
	atomic_set(&dev->watchdog_cnt, 0);
	atomic_set(&dev->watchdog_run, 0);
	init_timer(&dev->watchdog_timer);
	dev->watchdog_timer.data = (unsigned long)dev;
	dev->watchdog_timer.function = s5p_mfc_watchdog;

	exynos_sysmmu_set_fault_handler(dev->device,
			s5p_mfc_sysmmu_fault_handler);

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	INIT_LIST_HEAD(&dev->qos_queue);
#endif
	dev->variant = (struct s5p_mfc_variant *)
		platform_get_device_id(pdev)->driver_data;

	/* default FW alloc is added */
	alloc_ctx_num = NUM_OF_ALLOC_CTX(dev);
	dev->alloc_ctx = (struct vb2_alloc_ctx **)
			s5p_mfc_mem_init_multi(&pdev->dev, alloc_ctx_num);

	if (IS_ERR(dev->alloc_ctx)) {
		mfc_err("Couldn't prepare allocator ctx.\n");
		ret = PTR_ERR(dev->alloc_ctx);
		goto alloc_ctx_fail;
	}

	dev->sched_wq = alloc_workqueue("s5p_mfc/sched", WQ_UNBOUND
					| WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (dev->sched_wq == NULL) {
		dev_err(&pdev->dev, "failed to create workqueue for scheduler\n");
		goto err_wq_sched;
	}
	INIT_WORK(&dev->sched_work, mfc_sched_worker);

#ifdef CONFIG_ION_EXYNOS
	dev->mfc_ion_client = ion_client_create(ion_exynos, -1, "mfc");
	if (IS_ERR(dev->mfc_ion_client)) {
		dev_err(&pdev->dev, "failed to ion_client_create\n");
		goto err_ion_client;
	}
#endif

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	dev->alloc_ctx_fw = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			IS_MFCV6(dev) ? SZ_4K : SZ_128K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_MFCFW |
			VB2ION_CTX_KVA_STATIC);
	if (IS_ERR(dev->alloc_ctx_fw)) {
		mfc_err("failed to prepare F/W allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_fw);
		goto alloc_ctx_fw_fail;
	}

	dev->alloc_ctx_sh = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			SZ_4K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_MFCSH |
			VB2ION_CTX_KVA_STATIC);
	if (IS_ERR(dev->alloc_ctx_sh)) {
		mfc_err("failed to prepare shared allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_sh);
		goto alloc_ctx_sh_fail;
	}

	dev->drm_info.alloc = s5p_mfc_mem_alloc_priv(dev->alloc_ctx_sh,
						PAGE_SIZE);
	if (IS_ERR(dev->drm_info.alloc)) {
		mfc_err("failed to allocate shared region\n");
		ret = PTR_ERR(dev->drm_info.alloc);
		goto shared_alloc_fail;
	}
	dev->drm_info.virt = s5p_mfc_mem_vaddr_priv(dev->drm_info.alloc);
	if (!dev->drm_info.virt) {
		mfc_err("failed to get vaddr for shared region\n");
		ret = -ENOMEM;
		goto shared_vaddr_fail;
	}

	dev->alloc_ctx_drm = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			SZ_4K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_VIDEO |
			VB2ION_CTX_KVA_STATIC);
	if (IS_ERR(dev->alloc_ctx_drm)) {
		mfc_err("failed to prepare DRM allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_drm);
		goto alloc_ctx_drm_fail;
	}

	mfc_proc_entry = proc_mkdir(MFC_PROC_ROOT, NULL);
	if (!mfc_proc_entry) {
		dev_err(&pdev->dev, "unable to create /proc/%s\n",
				MFC_PROC_ROOT);
		ret = -ENOMEM;
		goto err_proc_entry;
	}

	if (!create_proc_read_entry(MFC_PROC_INSTANCE_NUMBER,
				0,
				mfc_proc_entry,
				proc_read_inst_number,
				dev)) {
		dev_err(&pdev->dev, "unable to create /proc/%s/%s\n",
				MFC_PROC_ROOT, MFC_PROC_INSTANCE_NUMBER);
		ret = -ENOMEM;
		goto err_proc_number;
	}

	if (!create_proc_read_entry(MFC_PROC_DRM_INSTANCE_NUMBER,
				0,
				mfc_proc_entry,
				proc_read_drm_inst_number,
				dev)) {
		dev_err(&pdev->dev, "unable to create /proc/%s/%s\n",
				MFC_PROC_ROOT, MFC_PROC_DRM_INSTANCE_NUMBER);
		ret = -ENOMEM;
		goto err_proc_drm;
	}

	if (!create_proc_read_entry(MFC_PROC_FW_STATUS,
				0,
				mfc_proc_entry,
				proc_read_fw_status,
				dev)) {
		dev_err(&pdev->dev, "unable to create /proc/%s/%s\n",
				MFC_PROC_ROOT, MFC_PROC_FW_STATUS);
		ret = -ENOMEM;
		goto err_proc_fw;
	}
#endif

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	atomic_set(&dev->qos_req_cur, 0);

	dev->qos_req_cnt = kzalloc(dev->pdata->num_qos_steps * sizeof(atomic_t),
					GFP_KERNEL);
	if (!dev->qos_req_cnt) {
		dev_err(&pdev->dev, "failed to allocate QoS request count\n");
		ret = -ENOMEM;
		goto err_qos_cnt;
	}
	for (i = 0; i < dev->pdata->num_qos_steps; i++)
		atomic_set(&dev->qos_req_cnt[i], 0);
#endif

	pr_debug("%s--\n", __func__);
	return 0;

/* Deinit MFC if probe had failed */
#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
err_qos_cnt:
#endif
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
err_proc_fw:
	remove_proc_entry(MFC_PROC_DRM_INSTANCE_NUMBER, mfc_proc_entry);
err_proc_drm:
	remove_proc_entry(MFC_PROC_INSTANCE_NUMBER, mfc_proc_entry);
err_proc_number:
	remove_proc_entry(MFC_PROC_ROOT, NULL);
err_proc_entry:
	vb2_ion_destroy_context(dev->alloc_ctx_drm);
shared_vaddr_fail:
	s5p_mfc_mem_free_priv(dev->drm_info.alloc);
shared_alloc_fail:
alloc_ctx_drm_fail:
	vb2_ion_destroy_context(dev->alloc_ctx_sh);
alloc_ctx_sh_fail:
	vb2_ion_destroy_context(dev->alloc_ctx_fw);
alloc_ctx_fw_fail:
	destroy_workqueue(dev->sched_wq);
#endif
#ifdef CONFIG_ION_EXYNOS
	ion_client_destroy(dev->mfc_ion_client);
err_ion_client:
#endif

err_wq_sched:
	s5p_mfc_mem_cleanup_multi((void **)dev->alloc_ctx,
			alloc_ctx_num);
alloc_ctx_fail:
	destroy_workqueue(dev->watchdog_wq);
err_wq_watchdog:
	video_unregister_device(dev->vfd_enc);
reg_vdev_enc:
alloc_vdev_enc:
	video_unregister_device(dev->vfd_dec);
reg_vdev_dec:
alloc_vdev_dec:
	v4l2_device_unregister(&dev->v4l2_dev);
err_v4l2_dev:
	free_irq(dev->irq, dev);
err_req_irq:
err_res_irq:
	iounmap(dev->regs_base);
err_ioremap:
	release_mem_region(dev->mfc_mem->start, resource_size(dev->mfc_mem));
err_req_mem:
err_res_mem:
	s5p_mfc_final_pm(dev);
err_pm:
	kfree(dev);
	return ret;
}

/* Remove the driver */
static int __devexit s5p_mfc_remove(struct platform_device *pdev)
{
	struct s5p_mfc_dev *dev = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s++\n", __func__);
	v4l2_info(&dev->v4l2_dev, "Removing %s\n", pdev->name);
	del_timer_sync(&dev->watchdog_timer);
	flush_workqueue(dev->watchdog_wq);
	destroy_workqueue(dev->watchdog_wq);
	flush_workqueue(dev->sched_wq);
	destroy_workqueue(dev->sched_wq);
	video_unregister_device(dev->vfd_enc);
	video_unregister_device(dev->vfd_dec);
	v4l2_device_unregister(&dev->v4l2_dev);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	remove_proc_entry(MFC_PROC_FW_STATUS, mfc_proc_entry);
	remove_proc_entry(MFC_PROC_DRM_INSTANCE_NUMBER, mfc_proc_entry);
	remove_proc_entry(MFC_PROC_INSTANCE_NUMBER, mfc_proc_entry);
	remove_proc_entry(MFC_PROC_ROOT, NULL);
	vb2_ion_destroy_context(dev->alloc_ctx_drm);
	s5p_mfc_mem_free_priv(dev->drm_info.alloc);
	vb2_ion_destroy_context(dev->alloc_ctx_sh);
	vb2_ion_destroy_context(dev->alloc_ctx_fw);
#endif
#ifdef CONFIG_ION_EXYNOS
	ion_client_destroy(dev->mfc_ion_client);
#endif

	s5p_mfc_mem_cleanup_multi((void **)dev->alloc_ctx,
					NUM_OF_ALLOC_CTX(dev));
	mfc_debug(2, "Will now deinit HW\n");
	s5p_mfc_deinit_hw(dev);
	free_irq(dev->irq, dev);
	iounmap(dev->regs_base);
	release_mem_region(dev->mfc_mem->start, resource_size(dev->mfc_mem));
	s5p_mfc_final_pm(dev);
	kfree(dev);
	dev_dbg(&pdev->dev, "%s--\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
static int s5p_mfc_suspend(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (m_dev->num_inst == 0)
		return 0;

	ret = s5p_mfc_sleep(m_dev);

	return ret;
}

static int s5p_mfc_resume(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (m_dev->num_inst == 0)
		return 0;

	ret = s5p_mfc_wakeup(m_dev);

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int s5p_mfc_runtime_suspend(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int pre_power;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	pre_power = atomic_read(&m_dev->pm.power);
	atomic_set(&m_dev->pm.power, 0);

	return 0;
}

static int s5p_mfc_runtime_idle(struct device *dev)
{
	return 0;
}

static int s5p_mfc_runtime_resume(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int pre_power;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (!m_dev->alloc_ctx)
		return 0;

	pre_power = atomic_read(&m_dev->pm.power);
	atomic_set(&m_dev->pm.power, 1);

	return 0;
}
#endif

/* Power management */
static const struct dev_pm_ops s5p_mfc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(s5p_mfc_suspend, s5p_mfc_resume)
	SET_RUNTIME_PM_OPS(
			s5p_mfc_runtime_suspend,
			s5p_mfc_runtime_resume,
			s5p_mfc_runtime_idle
	)
};

struct s5p_mfc_buf_size_v5 mfc_buf_size_v5 = {
	.h264_ctx_buf		= PAGE_ALIGN(0x96000),	/* 600KB */
	.non_h264_ctx_buf	= PAGE_ALIGN(0x2800),	/*  10KB */
	.desc_buf		= PAGE_ALIGN(0x20000),	/* 128KB */
	.shared_buf		= PAGE_ALIGN(0x1000),	/*   4KB */
};

struct s5p_mfc_buf_size_v6 mfc_buf_size_v6 = {
	.dev_ctx	= PAGE_ALIGN(0x6400),	/*  25KB */
	.h264_dec_ctx	= PAGE_ALIGN(0x200000),	/* 1.6MB */
	.other_dec_ctx	= PAGE_ALIGN(0x5000),	/*  20KB */
	.h264_enc_ctx	= PAGE_ALIGN(0x19000),	/* 100KB */
	.other_enc_ctx	= PAGE_ALIGN(0x2800),	/*  10KB */
};

struct s5p_mfc_buf_size buf_size_v5 = {
	.firmware_code	= PAGE_ALIGN(0x60000),	/* 384KB */
	.cpb_buf	= PAGE_ALIGN(0x300000),	/*   3MB */
	.buf		= &mfc_buf_size_v5,
};

struct s5p_mfc_buf_size buf_size_v6 = {
	.firmware_code	= PAGE_ALIGN(0x100000),	/* 1MB */
	.cpb_buf	= PAGE_ALIGN(0x300000),	/* 3MB */
	.buf		= &mfc_buf_size_v6,
};

struct s5p_mfc_buf_align mfc_buf_align_v5 = {
	.mfc_base_align = 17,
};

struct s5p_mfc_buf_align mfc_buf_align_v6 = {
	.mfc_base_align = 0,
};

static struct s5p_mfc_variant mfc_drvdata_v5 = {
	.buf_size = &buf_size_v5,
	.buf_align = &mfc_buf_align_v5,
};

static struct s5p_mfc_variant mfc_drvdata_v6 = {
	.buf_size = &buf_size_v6,
	.buf_align = &mfc_buf_align_v6,
};

static struct platform_device_id mfc_driver_ids[] = {
	{
		.name = "s5p-mfc",
		.driver_data = (unsigned long)&mfc_drvdata_v6,
	}, {
		.name = "s5p-mfc-v5",
		.driver_data = (unsigned long)&mfc_drvdata_v5,
	}, {
		.name = "s5p-mfc-v6",
		.driver_data = (unsigned long)&mfc_drvdata_v6,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, mfc_driver_ids);

static struct platform_driver s5p_mfc_driver = {
	.probe	= s5p_mfc_probe,
	.remove	= __devexit_p(s5p_mfc_remove),
	.id_table = mfc_driver_ids,
	.driver	= {
		.name	= S5P_MFC_NAME,
		.owner	= THIS_MODULE,
		.pm	= &s5p_mfc_pm_ops
	},
};

static char banner[] __initdata =
			"EXYNOS MFC V4L2 Driver, (c) 2010 Samsung Electronics\n";

static int __init s5p_mfc_init(void)
{
	pr_info("%s", banner);
	if (platform_driver_register(&s5p_mfc_driver) != 0) {
		pr_err("Platform device registration failed..\n");
		return -1;
	}
	return 0;
}

static void __devexit s5p_mfc_exit(void)
{
	platform_driver_unregister(&s5p_mfc_driver);
}

module_init(s5p_mfc_init);
module_exit(s5p_mfc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamil Debski <k.debski@samsung.com>");

