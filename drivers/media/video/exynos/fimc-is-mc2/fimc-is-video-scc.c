/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <linux/cma.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>

#include "fimc-is-core.h"
#include "fimc-is-helper.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-misc.h"
#include "fimc-is-video.h"
#include "fimc-is-metadata.h"
#include "fimc-is-device-ischain.h"

int fimc_is_scc_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)data;
	struct fimc_is_video_scc *video = &core->video_scc;

	dbg_scc("%s\n", __func__);

	ret = fimc_is_video_probe(&video->common,
		data,
		video,
		FIMC_IS_VIDEO_SCALERC_NAME,
		FIMC_IS_VIDEO_NUM_SCALERC,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		&fimc_is_scalerc_video_fops,
		&fimc_is_scalerc_video_ioctl_ops,
		&fimc_is_scalerc_qops);

	return ret;
}

/*************************************************************************/
/* video file opertation */
/************************************************************************/

static int fimc_is_scalerc_video_open(struct file *file)
{
	struct fimc_is_core *core = video_drvdata(file);
	struct fimc_is_video_scc *video = &core->video_scc;
	struct fimc_is_device_ischain *ischain = &core->ischain;

	dbg_scc("%s\n", __func__);

	file->private_data = video;
	fimc_is_video_open(&video->common, ischain);

	return 0;
}

static int fimc_is_scalerc_video_close(struct file *file)
{
	struct fimc_is_video_scc *video = file->private_data;

	dbg("%s\n", __func__);

	file->private_data = 0;
	fimc_is_video_close(&video->common);

	return 0;
}

static unsigned int fimc_is_scalerc_video_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct fimc_is_video_scc *video = file->private_data;

	dbg("%s\n", __func__);
	return vb2_poll(&video->common.vbq, file, wait);

}

static int fimc_is_scalerc_video_mmap(struct file *file,
					struct vm_area_struct *vma)
{
	struct fimc_is_video_scc *video = file->private_data;

	dbg("%s\n", __func__);
	return vb2_mmap(&video->common.vbq, vma);

}

/*************************************************************************/
/* video ioctl operation						*/
/************************************************************************/

static int fimc_is_scalerc_video_querycap(struct file *file, void *fh,
						struct v4l2_capability *cap)
{
	struct fimc_is_core *isp = video_drvdata(file);

	strncpy(cap->driver, isp->pdev->name, sizeof(cap->driver) - 1);

	dbg("(devname : %s)\n", cap->driver);
	strncpy(cap->card, isp->pdev->name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_STREAMING
				| V4L2_CAP_VIDEO_CAPTURE
				| V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	return 0;
}

static int fimc_is_scalerc_video_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_get_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_set_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	struct fimc_is_video_scc *video = file->private_data;
	int ret = 0;

	dbg_scp("%s\n", __func__);

	ret = fimc_is_video_set_format_mplane(&video->common, format);

	dbg_scp("req w : %d req h : %d\n",
		video->common.frame.width,
		video->common.frame.width);

	return ret;
}

static int fimc_is_scalerc_video_try_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_cropcap(struct file *file, void *fh,
						struct v4l2_cropcap *cropcap)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_get_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_set_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_reqbufs(struct file *file, void *priv,
					struct v4l2_requestbuffers *buf)
{
	int ret;
	struct fimc_is_video_scc *video = file->private_data;

	dbg_scc("%s\n", __func__);

	ret = fimc_is_video_reqbufs(&video->common, buf);
	if (ret)
		err("fimc_is_video_reqbufs is fail(error %d)", ret);

	return ret;
}

static int fimc_is_scalerc_video_querybuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_scc *video = file->private_data;

	dbg("%s\n", __func__);
	ret = vb2_querybuf(&video->common.vbq, buf);

	return ret;
}

static int fimc_is_scalerc_video_qbuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int vb_ret;
	struct fimc_is_video_scc *video = file->private_data;
	struct fimc_is_core *isp = video_drvdata(file);

	if (test_bit(FIMC_IS_STATE_SCALERC_BUFFER_PREPARED, &isp->pipe_state)) {
		video->common.buf_mask |= (1<<buf->index);
		IS_INC_PARAM_NUM(isp);
	} else
		dbg("index(%d)\n", buf->index);

	vb_ret = vb2_qbuf(&video->common.vbq, buf);

	return vb_ret;
}

static int fimc_is_scalerc_video_dqbuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int vb_ret;
	struct fimc_is_video_scc *video = file->private_data;

	vb_ret = vb2_dqbuf(&video->common.vbq, buf, file->f_flags & O_NONBLOCK);

	video->common.buf_mask &= ~(1<<buf->index);

	dbg("index(%d) mask(0x%08x)\n", buf->index, video->common.buf_mask);

	return vb_ret;
}

static int fimc_is_scalerc_video_streamon(struct file *file, void *priv,
						enum v4l2_buf_type type)
{
	struct fimc_is_video_scc *video = file->private_data;

	dbg("%s\n", __func__);
	return vb2_streamon(&video->common.vbq, type);
}

static int fimc_is_scalerc_video_streamoff(struct file *file, void *priv,
						enum v4l2_buf_type type)
{
	struct fimc_is_video_scc *video = file->private_data;

	dbg("%s\n", __func__);
	return vb2_streamoff(&video->common.vbq, type);
}

static int fimc_is_scalerc_video_enum_input(struct file *file, void *priv,
						struct v4l2_input *input)
{
	struct fimc_is_core *isp = video_drvdata(file);
	struct exynos5_fimc_is_sensor_info *sensor_info
			= isp->pdata->sensor_info[input->index];

	dbg("index(%d) sensor(%s)\n",
		input->index, sensor_info->sensor_name);
	dbg("pos(%d) sensor_id(%d)\n",
		sensor_info->sensor_position, sensor_info->sensor_id);
	dbg("csi_id(%d) flite_id(%d)\n",
		sensor_info->csi_id, sensor_info->flite_id);
	dbg("i2c_ch(%d)\n", sensor_info->i2c_channel);

	if (input->index >= FIMC_IS_MAX_CAMIF_CLIENTS)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;

	strncpy(input->name, sensor_info->sensor_name,
					FIMC_IS_MAX_NAME_LEN);
	return 0;
}

static int fimc_is_scalerc_video_g_input(struct file *file, void *priv,
						unsigned int *input)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerc_video_s_input(struct file *file, void *priv,
						unsigned int input)
{
	struct fimc_is_core *core = video_drvdata(file);
	struct exynos5_fimc_is_sensor_info *sensor_info
			= core->pdata->sensor_info[input];

	core->sensor.id_position = input;

	fimc_is_hw_set_default_size(core, sensor_info->sensor_id);

	dbg("sensor info : pos(%d)\n",
			input);


	return 0;
}

const struct v4l2_file_operations fimc_is_scalerc_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_scalerc_video_open,
	.release	= fimc_is_scalerc_video_close,
	.poll		= fimc_is_scalerc_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_scalerc_video_mmap,
};

const struct v4l2_ioctl_ops fimc_is_scalerc_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_scalerc_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane
				= fimc_is_scalerc_video_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane
				= fimc_is_scalerc_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane
				= fimc_is_scalerc_video_set_format_mplane,
	.vidioc_try_fmt_vid_cap_mplane
				= fimc_is_scalerc_video_try_format_mplane,
	.vidioc_cropcap			= fimc_is_scalerc_video_cropcap,
	.vidioc_g_crop			= fimc_is_scalerc_video_get_crop,
	.vidioc_s_crop			= fimc_is_scalerc_video_set_crop,
	.vidioc_reqbufs			= fimc_is_scalerc_video_reqbufs,
	.vidioc_querybuf		= fimc_is_scalerc_video_querybuf,
	.vidioc_qbuf			= fimc_is_scalerc_video_qbuf,
	.vidioc_dqbuf			= fimc_is_scalerc_video_dqbuf,
	.vidioc_streamon		= fimc_is_scalerc_video_streamon,
	.vidioc_streamoff		= fimc_is_scalerc_video_streamoff,
	.vidioc_enum_input		= fimc_is_scalerc_video_enum_input,
	.vidioc_g_input			= fimc_is_scalerc_video_g_input,
	.vidioc_s_input			= fimc_is_scalerc_video_s_input,
};

static int fimc_is_scalerc_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			unsigned int *num_buffers,
			unsigned int *num_planes, unsigned int sizes[],
			void *allocators[])
{

	struct fimc_is_video_scc *video = vq->drv_priv;
	struct fimc_is_core	*dev = video->common.core;
	int i;

	*num_planes = video->common.frame.format.num_planes;
	fimc_is_set_plane_size(&video->common.frame, sizes);

	for (i = 0; i < *num_planes; i++)
		allocators[i] =  dev->alloc_ctx;

	dbg("(num_planes : %d)(size : %d)\n", (int)*num_planes, (int)sizes[0]);
	return 0;
}
static int fimc_is_scalerc_buffer_prepare(struct vb2_buffer *vb)
{
	dbg("--%s\n", __func__);
	return 0;
}


static inline void fimc_is_scalerc_lock(struct vb2_queue *vq)
{
	dbg("%s\n", __func__);
}

static inline void fimc_is_scalerc_unlock(struct vb2_queue *vq)
{
	dbg("%s\n", __func__);
}

static int fimc_is_scalerc_start_streaming(struct vb2_queue *q,
						unsigned int count)
{
#if 0
	struct fimc_is_video_scc *video = q->drv_priv;
	struct fimc_is_core	*isp = video->common.core;
	int ret;
	int i, j;
	int buf_index;

	dbg("(pipe_state : %d)\n", (int)isp->pipe_state);

	if (test_bit(FIMC_IS_STATE_FW_DOWNLOADED, &isp->pipe_state) &&
		!test_bit(FIMC_IS_STATE_SENSOR_INITIALIZED, &isp->pipe_state)) {

		printk(KERN_INFO "ScalerC mode change\n");
		set_bit(IS_ST_CHANGE_MODE, &isp->state);
		fimc_is_hw_change_mode(isp, IS_MODE_PREVIEW_STILL);
		mutex_lock(&isp->lock);
		ret = wait_event_timeout(isp->irq_queue,
			test_bit(IS_ST_CHANGE_MODE_DONE,
			&isp->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		mutex_unlock(&isp->lock);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"Mode change timeout:%s\n", __func__);
			return -EBUSY;
		}

		set_bit(FIMC_IS_STATE_SENSOR_INITIALIZED, &isp->pipe_state);
	}

	if (test_bit(FIMC_IS_STATE_SENSOR_INITIALIZED, &isp->pipe_state) &&
		test_bit(FIMC_IS_STATE_SCALERC_BUFFER_PREPARED,
			&isp->pipe_state) &&
		!test_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state)) {
		printk(KERN_INFO "ScalerC stream enable\n");
		fimc_is_hw_set_stream(isp, 1);
		mutex_lock(&isp->lock);
		ret = wait_event_timeout(isp->irq_queue,
			test_bit(IS_ST_STREAM_ON, &isp->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		mutex_unlock(&isp->lock);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		clear_bit(IS_ST_STREAM_ON, &isp->state);

		set_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state);
	}

	if (test_bit(FIMC_IS_STATE_SCALERC_BUFFER_PREPARED, &isp->pipe_state) &&
		test_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state) &&
		test_bit(FIMC_IS_STATE_SENSOR_INITIALIZED, &isp->pipe_state)) {

		/* buffer addr setting */
		for (i = 0; i < isp->video_scc.
							num_buf; i++)
			for (j = 0; j < isp->video_scc.
						frame.format.num_planes; j++) {
				buf_index
				= i * isp->video_scc.
						frame.format.num_planes + j;

				dbg("(%d)set buf(%d:%d) = 0x%08x\n",
					buf_index, i, j,
					isp->video_scc.
					buf[i][j]);

				isp->is_p_region->shared[447+buf_index]
					= isp->video_scc.
								buf[i][j];
		}

		dbg("buf_num:%d buf_plane:%d shared[447] : 0x%p\n",
			isp->video_scc.num_buf,
			isp->video_scc.
			frame.format.num_planes,
			isp->mem.kvaddr_shared + 447 * sizeof(u32));

		for (i = 0; i < isp->video_scc.
							num_buf; i++)
			isp->video_scc.buf_mask
								|= (1 << i);

		dbg("initial buffer mask : 0x%08x\n",
			isp->video_scc.buf_mask);

		IS_SCALERC_SET_PARAM_DMA_OUTPUT_CMD(isp,
			DMA_OUTPUT_COMMAND_ENABLE);
		IS_SCALERC_SET_PARAM_DMA_OUTPUT_MASK(isp,
			isp->video_scc.buf_mask);
		IS_SCALERC_SET_PARAM_DMA_OUTPUT_BUFFERNUM(isp,
			isp->video_scc.num_buf);
		IS_SCALERC_SET_PARAM_DMA_OUTPUT_BUFFERADDR(isp,
			(u32)isp->mem.dvaddr_shared + 447*sizeof(u32));

		IS_SET_PARAM_BIT(isp, PARAM_SCALERC_DMA_OUTPUT);
		IS_INC_PARAM_NUM(isp);

		fimc_is_mem_cache_clean((void *)isp->is_p_region,
			IS_PARAM_SIZE);

		isp->scenario_id = ISS_PREVIEW_STILL;
		set_bit(IS_ST_INIT_PREVIEW_STILL,	&isp->state);
		clear_bit(IS_ST_INIT_CAPTURE_STILL, &isp->state);
		clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &isp->state);
		fimc_is_hw_set_param(isp);
		mutex_lock(&isp->lock);
		ret = wait_event_timeout(isp->irq_queue,
			test_bit(IS_ST_INIT_PREVIEW_VIDEO, &isp->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		mutex_unlock(&isp->lock);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}

		set_bit(FIMC_IS_STATE_SCALERC_STREAM_ON, &isp->pipe_state);
	}
#endif
	return 0;
}

static int fimc_is_scalerc_stop_streaming(struct vb2_queue *q)
{
#if 0
	struct fimc_is_video_scc *video = q->drv_priv;
	int ret = 0;

	clear_bit(IS_ST_STREAM_OFF, &isp->state);
	fimc_is_hw_set_stream(isp, 0);
	mutex_lock(&isp->lock);
	ret = wait_event_timeout(isp->irq_queue,
		test_bit(IS_ST_STREAM_OFF, &isp->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	mutex_unlock(&isp->lock);
	if (!ret) {
		dev_err(&isp->pdev->dev,
				"wait timeout : %s\n", __func__);
		if (!ret)
			err("s_power off failed!!\n");
		return -EBUSY;
	}

	IS_SCALERC_SET_PARAM_DMA_OUTPUT_CMD(isp,
		DMA_OUTPUT_COMMAND_DISABLE);
	IS_SCALERC_SET_PARAM_DMA_OUTPUT_BUFFERNUM(isp,
		0);
	IS_SCALERC_SET_PARAM_DMA_OUTPUT_BUFFERADDR(isp,
		0);

	IS_SET_PARAM_BIT(isp, PARAM_SCALERC_DMA_OUTPUT);
	IS_INC_PARAM_NUM(isp);

	fimc_is_mem_cache_clean((void *)isp->is_p_region,
		IS_PARAM_SIZE);

	isp->scenario_id = ISS_PREVIEW_STILL;
	set_bit(IS_ST_INIT_PREVIEW_STILL,	&isp->state);
	clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &isp->state);
	fimc_is_hw_set_param(isp);

	mutex_lock(&isp->lock);
	ret = wait_event_timeout(isp->irq_queue,
		test_bit(IS_ST_INIT_PREVIEW_VIDEO, &isp->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	mutex_unlock(&isp->lock);
	if (!ret) {
		dev_err(&isp->pdev->dev,
			"wait timeout 2: %s\n", __func__);
		return -EBUSY;
	}

	dbg("IS change mode\n");
	clear_bit(IS_ST_RUN, &isp->state);
	set_bit(IS_ST_CHANGE_MODE, &isp->state);
	fimc_is_hw_change_mode(isp, IS_MODE_PREVIEW_STILL);
	mutex_lock(&isp->lock);
	ret = wait_event_timeout(isp->irq_queue,
		test_bit(IS_ST_CHANGE_MODE_DONE, &isp->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	mutex_unlock(&isp->lock);
	if (!ret) {
		dev_err(&isp->pdev->dev,
			"Mode change timeout:%s\n", __func__);
		return -EBUSY;
	}

	dbg("IS Stream On");
	fimc_is_hw_set_stream(isp, 1);

	mutex_lock(&isp->lock);
	ret = wait_event_timeout(isp->irq_queue,
		test_bit(IS_ST_STREAM_ON, &isp->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	mutex_unlock(&isp->lock);
	if (!ret) {
		dev_err(&isp->pdev->dev,
			"wait timeout : %s\n", __func__);
		return -EBUSY;
	}
	clear_bit(IS_ST_STREAM_ON, &isp->state);

	if (!test_bit(FIMC_IS_STATE_SCALERP_STREAM_ON, &isp->pipe_state) &&
		!test_bit(FIMC_IS_STATE_3DNR_STREAM_ON, &isp->pipe_state) &&
		test_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state)) {
		clear_bit(IS_ST_STREAM_OFF, &isp->state);

		printk(KERN_INFO "ScalerC stream disable\n");
		fimc_is_hw_set_stream(isp, 0);
		mutex_lock(&isp->lock);
		ret = wait_event_timeout(isp->irq_queue,
			test_bit(IS_ST_STREAM_OFF, &isp->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		mutex_unlock(&isp->lock);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"wait timeout4 : %s\n", __func__);
			return -EBUSY;
		}
		clear_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state);
	}

	clear_bit(IS_ST_RUN, &isp->state);
	clear_bit(IS_ST_STREAM_ON, &isp->state);
	clear_bit(FIMC_IS_STATE_SCALERC_BUFFER_PREPARED, &isp->pipe_state);
	clear_bit(FIMC_IS_STATE_SCALERC_STREAM_ON, &isp->pipe_state);
#endif
	return 0;
}

static void fimc_is_scalerc_buffer_queue(struct vb2_buffer *vb)
{
#if 0
	struct fimc_is_video_scc *video = vb->vb2_queue->drv_priv;
	unsigned int i;

	dbg("%s\n", __func__);

	isp->video_scc.frame.format.num_planes
							= vb->num_planes;

	if (!test_bit(FIMC_IS_STATE_SCALERC_BUFFER_PREPARED,
					&isp->pipe_state)) {
		for (i = 0; i < vb->num_planes; i++) {
			isp->video_scc.
				buf[vb->v4l2_buf.index][i]
				= isp->vb2->plane_addr(vb, i);

			dbg("index(%d)(%d) deviceVaddr(0x%08x)\n",
				vb->v4l2_buf.index, i,
				isp->video_scc.
				buf[vb->v4l2_buf.index][i]);
		}

		isp->video_scc.buf_ref_cnt++;

		if (isp->video_scc.num_buf
			== isp->video_scc.buf_ref_cnt)
			set_bit(FIMC_IS_STATE_SCALERC_BUFFER_PREPARED,
				&isp->pipe_state);
	}

	if (!test_bit(FIMC_IS_STATE_SCALERC_STREAM_ON, &isp->pipe_state))
		fimc_is_scalerc_start_streaming(vb->vb2_queue);
#endif
	return;
}

const struct vb2_ops fimc_is_scalerc_qops = {
	.queue_setup		= fimc_is_scalerc_queue_setup,
	.buf_prepare		= fimc_is_scalerc_buffer_prepare,
	.buf_queue		= fimc_is_scalerc_buffer_queue,
	.wait_prepare		= fimc_is_scalerc_unlock,
	.wait_finish		= fimc_is_scalerc_lock,
	.start_streaming	= fimc_is_scalerc_start_streaming,
	.stop_streaming	= fimc_is_scalerc_stop_streaming,
};
