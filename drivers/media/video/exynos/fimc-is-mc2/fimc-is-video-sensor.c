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

/*************************************************************************/
/* video file opertation						 */
/************************************************************************/
int fimc_is_sensor_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)data;
	struct fimc_is_video_sensor *video = &core->video_sensor;

	dbg_sensor("%s\n", __func__);

	ret = fimc_is_video_probe(&video->common,
		data,
		video,
		FIMC_IS_VIDEO_BAYER_NAME,
		FIMC_IS_VIDEO_NUM_BAYER,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		&fimc_is_bayer_video_fops,
		&fimc_is_bayer_video_ioctl_ops,
		&fimc_is_bayer_qops);

	return ret;
}

static int fimc_is_bayer_video_open(struct file *file)
{
	struct fimc_is_core *core = video_drvdata(file);
	struct fimc_is_video_sensor *video = &core->video_sensor;
	struct fimc_is_device_sensor *sensor = &core->sensor;

	dbg_sensor("%s\n", __func__);

	file->private_data = video;
	fimc_is_video_open(&video->common, sensor);
	fimc_is_sensor_open(sensor);

	return 0;
}

static int fimc_is_bayer_video_close(struct file *file)
{
	struct fimc_is_video_sensor *video = file->private_data;
	struct fimc_is_device_sensor *sensor = video->common.device;

	dbg_sensor("%s\n", __func__);

	file->private_data = 0;
	fimc_is_sensor_close(sensor);
	fimc_is_video_close(&video->common);

	return 0;
}

static unsigned int fimc_is_bayer_video_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct fimc_is_core *isp = video_drvdata(file);

	dbg_sensor("%s\n", __func__);
	return vb2_poll(&isp->video_sensor.common.vbq, file, wait);
}

static int fimc_is_bayer_video_mmap(struct file *file,
					struct vm_area_struct *vma)
{
	struct fimc_is_core *is = video_drvdata(file);

	return vb2_mmap(&is->video_sensor.common.vbq, vma);
}

/*************************************************************************/
/* video ioctl operation						*/
/************************************************************************/

static int fimc_is_bayer_video_querycap(struct file *file, void *fh,
					struct v4l2_capability *cap)
{
	struct fimc_is_core *isp = video_drvdata(file);

	strncpy(cap->driver, isp->pdev->name, sizeof(cap->driver) - 1);

	dbg_sensor("%s(devname : %s)\n", __func__, cap->driver);
	strncpy(cap->card, isp->pdev->name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_STREAMING
				| V4L2_CAP_VIDEO_CAPTURE
				| V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	return 0;
}

static int fimc_is_bayer_video_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_get_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_set_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	struct fimc_is_core *dev = video_drvdata(file);
	struct fimc_is_video_sensor *video;
	struct v4l2_pix_format_mplane *pix;
	struct fimc_is_fmt *frame;

	dbg_sensor("%s\n", __func__);

	pix = &format->fmt.pix_mp;
	frame = fimc_is_find_format(&pix->pixelformat, NULL, 0);

	if (!frame)
		return -EINVAL;

	video = &dev->video_sensor;
	video->common.frame.format.pixelformat = frame->pixelformat;
	video->common.frame.format.mbus_code = frame->mbus_code;
	video->common.frame.format.num_planes = frame->num_planes;
	video->common.frame.width = pix->width;
	video->common.frame.height = pix->height;
	dbg_sensor("num_planes : %d\n", frame->num_planes);
	dbg_sensor("width : %d\n", pix->width);
	dbg_sensor("height : %d\n", pix->height);

	return 0;
}

static int fimc_is_bayer_video_try_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_cropcap(struct file *file, void *fh,
						struct v4l2_cropcap *cropcap)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_get_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_set_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_reqbufs(struct file *file, void *priv,
						struct v4l2_requestbuffers *buf)
{
	int ret;
	struct fimc_is_video_sensor *video = file->private_data;

	dbg_sensor("%s\n", __func__);

	ret = fimc_is_video_reqbufs(&video->common, buf);
	if (ret)
		err("fimc_is_video_reqbufs is fail(error %d)", ret);

	return ret;
}

static int fimc_is_bayer_video_querybuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_sensor *video = file->private_data;

	dbg_sensor("%s\n", __func__);
	ret = vb2_querybuf(&video->common.vbq, buf);

	return ret;
}

static int fimc_is_bayer_video_qbuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int vb_ret;
	struct fimc_is_video_sensor *video = file->private_data;

	dbg_sensor("%s\n", __func__);

	vb_ret = vb2_qbuf(&video->common.vbq, buf);

	return vb_ret;
}

static int fimc_is_bayer_video_dqbuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int vb_ret;
	struct fimc_is_video_sensor *video = file->private_data;

	vb_ret = vb2_dqbuf(&video->common.vbq, buf, file->f_flags & O_NONBLOCK);

	video->common.buf_mask &= ~(1<<buf->index);

	dbg_sensor("%s :: index(%d) mask(0x%08x)\n",
		__func__, buf->index, video->common.buf_mask);

	/*iky to do here*/
	/*doesn't work well with old masking method*/
#if 0
	IS_ISP_SET_PARAM_DMA_OUTPUT2_CMD(isp,
		DMA_OUTPUT_UPDATE_MASK_BITS);
	IS_ISP_SET_PARAM_DMA_OUTPUT2_MASK(isp,
		video->buf_mask);
	IS_SET_PARAM_BIT(isp, PARAM_ISP_DMA2_OUTPUT);
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
#endif

	return vb_ret;
}

static int fimc_is_bayer_video_streamon(struct file *file, void *priv,
						enum v4l2_buf_type type)
{
	struct fimc_is_core *dev = video_drvdata(file);
	struct fimc_is_video_sensor *video = &dev->video_sensor;

	dbg_sensor("%s\n", __func__);
	return vb2_streamon(&video->common.vbq, type);
}

static int fimc_is_bayer_video_streamoff(struct file *file, void *priv,
						enum v4l2_buf_type type)
{
	struct fimc_is_core *isp = video_drvdata(file);

	dbg_sensor("%s\n", __func__);
	return vb2_streamoff(&isp->video_sensor.common.vbq, type);
}

static int fimc_is_bayer_video_enum_input(struct file *file, void *priv,
						struct v4l2_input *input)
{
	struct fimc_is_core *isp = video_drvdata(file);
	struct exynos5_fimc_is_sensor_info *sensor_info;

	sensor_info = isp->pdata->sensor_info[input->index];

	dbg_sensor("index(%d) sensor(%s)\n",
		input->index, sensor_info->sensor_name);
	dbg_sensor("pos(%d) sensor_id(%d)\n",
		sensor_info->sensor_position, sensor_info->sensor_id);
	dbg_sensor("csi_id(%d) flite_id(%d)\n",
		sensor_info->csi_id, sensor_info->flite_id);
	dbg_sensor("i2c_ch(%d)\n", sensor_info->i2c_channel);

	if (input->index >= FIMC_IS_MAX_CAMIF_CLIENTS)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;

	strncpy(input->name, sensor_info->sensor_name,
					FIMC_IS_MAX_NAME_LEN);
	return 0;
}

static int fimc_is_bayer_video_g_input(struct file *file, void *priv,
						unsigned int *input)
{
	dbg_sensor("%s\n", __func__);
	return 0;
}

static int fimc_is_bayer_video_s_input(struct file *file, void *priv,
						unsigned int input)
{
	struct fimc_is_core *core = video_drvdata(file);
	struct fimc_is_device_sensor *sensor = &core->sensor;
	struct fimc_is_interface *interface = &core->interface;

	dbg_sensor("fimc_is_bayer_video_s_input(%d)\n", input);

	core->sensor.id_position = input;
	sensor->active_sensor = &sensor->enum_sensor[input];

	/* init fw */
	fimc_is_load_fw(core);

	/*TODO: will fixed*/
	fimc_is_fw_clear_irq1_all(core);


	fimc_is_hw_set_default_size(core, input);
	{
		u32 setfile_addr;
		fimc_is_hw_open(interface, 0, input,
			sensor->enum_sensor[input].i2c_ch);
		fimc_is_hw_saddr(interface, 0, &setfile_addr);
		core->setfile.base = setfile_addr;
		fimc_is_load_setfile(core);
		fimc_is_hw_setfile(interface, 0);
		fimc_is_hw_stream_off(interface, 0);
	}

	sensor->flite_ch = sensor->enum_sensor[input].flite_ch;
	if (sensor->flite_ch == FLITE_ID_A)
		sensor->regs = (unsigned long)S5P_VA_FIMCLITE0;
	else if (sensor->flite_ch == FLITE_ID_B)
		sensor->regs = (unsigned long)S5P_VA_FIMCLITE1;

	dbg_sensor("%s sensor info : pos(%d) 0x%08X\n", \
		__func__, input, sensor->regs);

	return 0;
}

static int fimc_is_bayer_video_s_ctrl(struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct fimc_is_core *core = video_drvdata(file);
	struct fimc_is_interface *interface = &core->interface;
	struct fimc_is_device_sensor *sensor = &core->sensor;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_IS_S_STREAM:
		if (ctrl->value == IS_ENABLE_STREAM) {
			sensor->streaming = true;
			ret = fimc_is_hw_stream_on(interface, 0);
		} else {
			sensor->streaming = false;
			ret = fimc_is_hw_stream_off(interface, 0);
		}
		break;
	}

	return ret;
}

const struct v4l2_file_operations fimc_is_bayer_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_bayer_video_open,
	.release	= fimc_is_bayer_video_close,
	.poll		= fimc_is_bayer_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_bayer_video_mmap,
};

const struct v4l2_ioctl_ops fimc_is_bayer_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_bayer_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane	= fimc_is_bayer_video_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= fimc_is_bayer_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= fimc_is_bayer_video_set_format_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= fimc_is_bayer_video_try_format_mplane,
	.vidioc_cropcap			= fimc_is_bayer_video_cropcap,
	.vidioc_g_crop			= fimc_is_bayer_video_get_crop,
	.vidioc_s_crop			= fimc_is_bayer_video_set_crop,
	.vidioc_reqbufs			= fimc_is_bayer_video_reqbufs,
	.vidioc_querybuf		= fimc_is_bayer_video_querybuf,
	.vidioc_qbuf			= fimc_is_bayer_video_qbuf,
	.vidioc_dqbuf			= fimc_is_bayer_video_dqbuf,
	.vidioc_streamon		= fimc_is_bayer_video_streamon,
	.vidioc_streamoff		= fimc_is_bayer_video_streamoff,
	.vidioc_enum_input		= fimc_is_bayer_video_enum_input,
	.vidioc_g_input			= fimc_is_bayer_video_g_input,
	.vidioc_s_input			= fimc_is_bayer_video_s_input,
	.vidioc_s_ctrl			= fimc_is_bayer_video_s_ctrl
};

static int fimc_is_bayer_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			unsigned int *num_buffers,
			unsigned int *num_planes, unsigned int sizes[],
			void *allocators[])
{

	struct fimc_is_video_sensor *video = vq->drv_priv;
	struct fimc_is_core	*dev = video->common.core;
	int i;

	*num_planes = video->common.frame.format.num_planes;
	fimc_is_set_plane_size(&video->common.frame, sizes);

	for (i = 0; i < *num_planes; i++)
		allocators[i] =  dev->alloc_ctx;

	dbg_sensor("%s(num_planes : %d)(size : %d)\n",
		__func__, (int)*num_planes, (int)sizes[0]);

	return 0;
}

static int fimc_is_bayer_buffer_prepare(struct vb2_buffer *vb)
{
	/*dbg_sensor("-%s\n", __func__);*/

	return 0;
}

static inline void fimc_is_bayer_lock(struct vb2_queue *vq)
{
	/*dbg_sensor("%s\n", __func__);*/
}

static inline void fimc_is_bayer_unlock(struct vb2_queue *vq)
{
	/*dbg_sensor("%s\n", __func__);*/
}

static int fimc_is_bayer_start_streaming(struct vb2_queue *q,
						unsigned int count)
{
	struct fimc_is_video_sensor *video = q->drv_priv;
	struct fimc_is_device_sensor *sensor = video->common.device;

	dbg_sensor("%s\n", __func__);

	if (test_bit(FIMC_IS_VIDEO_BUFFER_PREPARED, &video->common.state)) {
		/*fimc_is_ischain_start_streaming
		(&isp->ischain, &video->common);*/
		fimc_is_sensor_start_streaming(sensor, &video->common);

		set_bit(FIMC_IS_VIDEO_STREAM_ON, &video->common.state);
	}

	return 0;
}

static int fimc_is_bayer_stop_streaming(struct vb2_queue *q)
{
	struct fimc_is_video_sensor *sensor_video = q->drv_priv;
	struct fimc_is_core	*isp = sensor_video->common.core;
	int sensor_id, flite_ch;

	dbg_sensor("%s\n", __func__);

	sensor_id = isp->sensor.id_position;
	flite_ch = isp->pdata->sensor_info[sensor_id]->flite_id;
	stop_fimc_lite(flite_ch);

	clear_bit(FIMC_IS_STATE_BAYER_BUFFER_PREPARED, &isp->pipe_state);
	clear_bit(FIMC_IS_STATE_BAYER_STREAM_ON, &isp->pipe_state);

	return 0;
}

static void fimc_is_bayer_buffer_queue(struct vb2_buffer *vb)
{
	struct fimc_is_video_sensor *video = vb->vb2_queue->drv_priv;

	dbg_sensor("%s(%d)\n", __func__, vb->v4l2_buf.index);
	dbg_sensor("buffers : %d\n", video->common.buffers);

	fimc_is_video_buffer_queue(&video->common, vb);

	/*flush cache*/
	dbg_sensor("planes : %d\n", vb->num_planes);
	video->common.vb2->cache_flush(vb, vb->num_planes);

	/*insert request*/
	fimc_is_sensor_buffer_queue(
		video->common.device,
		&video->common,
		vb->v4l2_buf.index);

	if (!test_bit(FIMC_IS_VIDEO_STREAM_ON, &video->common.state))
		fimc_is_bayer_start_streaming(vb->vb2_queue,
				video->common.buffers);
}

const struct vb2_ops fimc_is_bayer_qops = {
	.queue_setup		= fimc_is_bayer_queue_setup,
	.buf_prepare		= fimc_is_bayer_buffer_prepare,
	.buf_queue		= fimc_is_bayer_buffer_queue,
	.wait_prepare		= fimc_is_bayer_unlock,
	.wait_finish		= fimc_is_bayer_lock,
	.start_streaming	= fimc_is_bayer_start_streaming,
	.stop_streaming	= fimc_is_bayer_stop_streaming,
};
