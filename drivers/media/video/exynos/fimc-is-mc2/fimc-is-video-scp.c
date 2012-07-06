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

/*************************************************************************/
/* video file opertation						 */
/************************************************************************/
int fimc_is_scp_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)data;
	struct fimc_is_video_scp *video = &core->video_scp;

	ret = fimc_is_video_probe(&video->common,
		data,
		video,
		FIMC_IS_VIDEO_SCALERP_NAME,
		FIMC_IS_VIDEO_NUM_SCALERP,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		&fimc_is_scalerp_video_fops,
		&fimc_is_scalerp_video_ioctl_ops,
		&fimc_is_scalerp_qops);

	return ret;
}

static int fimc_is_scalerp_video_open(struct file *file)
{
	struct fimc_is_core *dev = video_drvdata(file);
	struct fimc_is_video_scp *video = &dev->video_scp;
	struct fimc_is_device_ischain *ischain = &dev->ischain;

	dbg_scp("%s\n", __func__);

	file->private_data = video;
	fimc_is_video_open(&video->common, ischain);

	return 0;
}

static int fimc_is_scalerp_video_close(struct file *file)
{
	struct fimc_is_core *dev = video_drvdata(file);
	struct fimc_is_video_scp *video = &dev->video_scp;

	dbg("%s\n", __func__);

	file->private_data = 0;
	fimc_is_video_close(&video->common);

	return 0;

}

static unsigned int fimc_is_scalerp_video_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct fimc_is_video_scp *video = file->private_data;

	dbg("%s\n", __func__);
	return vb2_poll(&video->common.vbq, file, wait);

}

static int fimc_is_scalerp_video_mmap(struct file *file,
					struct vm_area_struct *vma)
{
	struct fimc_is_video_scp *video = file->private_data;

	dbg("%s\n", __func__);
	return vb2_mmap(&video->common.vbq, vma);

}

/*************************************************************************/
/* video ioctl operation						*/
/************************************************************************/

static int fimc_is_scalerp_video_querycap(struct file *file, void *fh,
						struct v4l2_capability *cap)
{
	struct fimc_is_core *isp = video_drvdata(file);

	strncpy(cap->driver, isp->pdev->name, sizeof(cap->driver) - 1);

	dbg("%s(devname : %s)\n", __func__, cap->driver);
	strncpy(cap->card, isp->pdev->name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_STREAMING
					| V4L2_CAP_VIDEO_CAPTURE
					| V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	return 0;
}

static int fimc_is_scalerp_video_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_get_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_set_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	struct fimc_is_video_scp *video = file->private_data;
	struct fimc_is_core *core = video_drvdata(file);
	struct fimc_is_device_ischain *ischain = &core->ischain;
	struct fimc_is_interface *interface = &core->interface;
	int ret = 0;

	dbg_scp("%s\n", __func__);

	ret = fimc_is_video_set_format_mplane(&video->common, format);

	dbg_scp("req w : %d req h : %d\n",
		video->common.frame.width,
		video->common.frame.height);

	/*device_sensor_stream_off(sensor, interface);*/

	fimc_is_ischain_s_chain1(ischain, interface,
		video->common.frame.width,
		video->common.frame.height);

	fimc_is_ischain_s_chain2(ischain, interface,
		video->common.frame.width,
		video->common.frame.height);

	fimc_is_ischain_s_chain3(ischain, interface,
		video->common.frame.width,
		video->common.frame.height);

	/*device_sensor_stream_on(sensor, interface);*/

	return ret;
}

static int fimc_is_scalerp_video_try_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_cropcap(struct file *file, void *fh,
						struct v4l2_cropcap *cropcap)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_get_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_set_crop(struct file *file, void *fh,
						struct v4l2_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_reqbufs(struct file *file, void *priv,
					struct v4l2_requestbuffers *buf)
{
	int ret;
	struct fimc_is_video_scp *video = file->private_data;

	ret = fimc_is_video_reqbufs(&video->common, buf);
	if (ret)
		err("fimc_is_video_reqbufs is fail(error %d)", ret);

	return ret;
}

static int fimc_is_scalerp_video_querybuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_scp *video = file->private_data;

	dbg("%s\n", __func__);
	ret = vb2_querybuf(&video->common.vbq, buf);

	return ret;
}

static int fimc_is_scalerp_video_qbuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int vb_ret;
	struct fimc_is_video_scp *video = file->private_data;

	/*
	if (test_bit(FIMC_IS_VIDEO_BUFFER_PREPARED, &video->common.state)) {
		video->common.buf_mask |= (1<<buf->index);

		dbg("index(%d) mask(0x%08x)\n", buf->index,
			video->common.buf_mask);
	} else
		dbg("index(%d)\n", buf->index);
	*/

	vb_ret = vb2_qbuf(&video->common.vbq, buf);

	return vb_ret;
}

static int fimc_is_scalerp_video_dqbuf(struct file *file, void *priv,
						struct v4l2_buffer *buf)
{
	int vb_ret;
	struct fimc_is_video_scp *video = file->private_data;

	vb_ret = vb2_dqbuf(&video->common.vbq, buf, file->f_flags & O_NONBLOCK);

	video->common.buf_mask &= ~(1<<buf->index);

	dbg("index(%d) mask(0x%08x)\n", buf->index, video->common.buf_mask);

	return vb_ret;
}

static int fimc_is_scalerp_video_streamon(struct file *file, void *priv,
						enum v4l2_buf_type type)
{
	struct fimc_is_video_scp *video = file->private_data;

	dbg("%s\n", __func__);

	/*TODO*/

	return vb2_streamon(&video->common.vbq, type);
}

static int fimc_is_scalerp_video_streamoff(struct file *file, void *priv,
						enum v4l2_buf_type type)
{
	struct fimc_is_video_scp *video = file->private_data;
	int result;

	dbg("%s\n", __func__);

	result = vb2_streamoff(&video->common.vbq, type);

	/*TODO:*/

	return result;
}

static int fimc_is_scalerp_video_enum_input(struct file *file, void *priv,
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

static int fimc_is_scalerp_video_g_input(struct file *file, void *priv,
						unsigned int *input)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_scalerp_video_s_input(struct file *file, void *priv,
					unsigned int input)
{
	struct fimc_is_core *core = video_drvdata(file);

	dbg_scp("fimc_is_scalerp_video_s_input\n");
	core->sensor.id_position = input;

	return 0;
}

static int fimc_is_scalerp_video_g_ctrl(struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct fimc_is_core *isp = video_drvdata(file);
	int ret = 0;

	switch (ctrl->id) {
	/* EXIF information */
	case V4L2_CID_IS_CAMERA_EXIF_EXPTIME:
	case V4L2_CID_CAMERA_EXIF_EXPTIME: /* Exposure Time */
		fimc_is_mem_cache_inv((void *)IS_HEADER(isp),
			(unsigned long)(sizeof(struct is_frame_header)*4));
		ctrl->value = isp->is_p_region->header[0].
			exif.exposure_time.den;
		break;
	case V4L2_CID_IS_CAMERA_EXIF_FLASH:
	case V4L2_CID_CAMERA_EXIF_FLASH: /* Flash */
		fimc_is_mem_cache_inv((void *)IS_HEADER(isp),
			(unsigned long)(sizeof(struct is_frame_header)*4));
		ctrl->value = isp->is_p_region->header[0].exif.flash;
		break;
	case V4L2_CID_IS_CAMERA_EXIF_ISO:
	case V4L2_CID_CAMERA_EXIF_ISO: /* ISO Speed Rating */
		fimc_is_mem_cache_inv((void *)IS_HEADER(isp),
			(unsigned long)(sizeof(struct is_frame_header)*4));
		ctrl->value = isp->is_p_region->header[0].
			exif.iso_speed_rating;
		break;
	case V4L2_CID_IS_CAMERA_EXIF_SHUTTERSPEED:
	case V4L2_CID_CAMERA_EXIF_TV: /* Shutter Speed */
		fimc_is_mem_cache_inv((void *)IS_HEADER(isp),
			(unsigned long)(sizeof(struct is_frame_header)*4));
		/* Exposure time = shutter speed by FW */
		ctrl->value = isp->is_p_region->header[0].
			exif.exposure_time.den;
		break;
	case V4L2_CID_IS_CAMERA_EXIF_BRIGHTNESS:
	case V4L2_CID_CAMERA_EXIF_BV: /* Brightness */
		fimc_is_mem_cache_inv((void *)IS_HEADER(isp),
			(unsigned long)(sizeof(struct is_frame_header)*4));
		ctrl->value = isp->is_p_region->header[0].exif.brightness.num;
		break;
	case V4L2_CID_CAMERA_EXIF_EBV: /* exposure bias */
		fimc_is_mem_cache_inv((void *)IS_HEADER(isp),
			(unsigned long)(sizeof(struct is_frame_header)*4));
		ctrl->value = isp->is_p_region->header[0].exif.brightness.den;
		break;
	/* Get x and y offset of sensor  */
	case V4L2_CID_IS_GET_SENSOR_OFFSET_X:
		ctrl->value = isp->sensor.offset_x;
		break;
	case V4L2_CID_IS_GET_SENSOR_OFFSET_Y:
		ctrl->value = isp->sensor.offset_y;
		break;
	case V4L2_CID_IS_FD_GET_DATA:
		ctrl->value = isp->fd_header.count;
		fimc_is_mem_cache_inv((void *)IS_FACE(isp),
		(unsigned long)(sizeof(struct is_face_marker)*MAX_FACE_COUNT));
		memcpy((void *)isp->fd_header.target_addr,
			&isp->is_p_region->face[isp->fd_header.index],
			(sizeof(struct is_face_marker)*isp->fd_header.count));
		break;
	/* AF result */
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		if (!is_af_use(isp))
			ctrl->value = 0x02;
		else
			ctrl->value = isp->af.af_lock_state;
		break;
	/* F/W debug region address */
	case V4L2_CID_IS_FW_DEBUG_REGION_ADDR:
		ctrl->value = isp->mem.base + FIMC_IS_DEBUG_REGION_ADDR;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int fimc_is_g_ext_ctrls_handler(struct fimc_is_core *dev,
	struct v4l2_ext_control *ctrl, int index)
{
	int ret = 0;
#if 0 /* will use later*/
	switch (ctrl->id) {
	/* Face Detection CID handler */
	/* 1. Overall information */
	case V4L2_CID_IS_FD_GET_FACE_COUNT:
		ctrl->value = dev->fd_header.count;
		break;
	case V4L2_CID_IS_FD_GET_FACE_FRAME_NUMBER:
		if (dev->fd_header.offset < dev->fd_header.count) {
			ctrl->value =
				dev->is_p_region->face[dev->fd_header.index
				+ dev->fd_header.offset].frame_number;
		} else {
			ctrl->value = 0;
			return -255;
		}
		break;
	case V4L2_CID_IS_FD_GET_FACE_CONFIDENCE:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].confidence;
		break;
	case V4L2_CID_IS_FD_GET_FACE_SMILE_LEVEL:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].smile_level;
		break;
	case V4L2_CID_IS_FD_GET_FACE_BLINK_LEVEL:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].blink_level;
		break;
	/* 2. Face information */
	case V4L2_CID_IS_FD_GET_FACE_TOPLEFT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].uiAfFace.offset_x;
		break;
	case V4L2_CID_IS_FD_GET_FACE_TOPLEFT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].uiAfFace.offset_y;
		break;
	case V4L2_CID_IS_FD_GET_FACE_BOTTOMRIGHT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].uiAfFace.offset_x
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].uiAfFace.width;
		break;
	case V4L2_CID_IS_FD_GET_FACE_BOTTOMRIGHT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].uiAfFace.offset_y
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].uiAfFace.height;
		break;
	/* 3. Left eye information */
	case V4L2_CID_IS_FD_GET_LEFT_EYE_TOPLEFT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].left_eye.offset_x;
		break;
	case V4L2_CID_IS_FD_GET_LEFT_EYE_TOPLEFT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].left_eye.offset_y;
		break;
	case V4L2_CID_IS_FD_GET_LEFT_EYE_BOTTOMRIGHT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].left_eye.offset_x
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].left_eye.width;
		break;
	case V4L2_CID_IS_FD_GET_LEFT_EYE_BOTTOMRIGHT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].left_eye.offset_y
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].left_eye.height;
		break;
	/* 4. Right eye information */
	case V4L2_CID_IS_FD_GET_RIGHT_EYE_TOPLEFT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].right_eye.offset_x;
		break;
	case V4L2_CID_IS_FD_GET_RIGHT_EYE_TOPLEFT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].right_eye.offset_y;
		break;
	case V4L2_CID_IS_FD_GET_RIGHT_EYE_BOTTOMRIGHT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].right_eye.offset_x
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].right_eye.width;
		break;
	case V4L2_CID_IS_FD_GET_RIGHT_EYE_BOTTOMRIGHT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].right_eye.offset_y
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].right_eye.height;
		break;
	/* 5. Mouth eye information */
	case V4L2_CID_IS_FD_GET_MOUTH_TOPLEFT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].mouth.offset_x;
		break;
	case V4L2_CID_IS_FD_GET_MOUTH_TOPLEFT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
				+ dev->fd_header.offset].mouth.offset_y;
		break;
	case V4L2_CID_IS_FD_GET_MOUTH_BOTTOMRIGHT_X:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].mouth.offset_x
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].mouth.width;
		break;
	case V4L2_CID_IS_FD_GET_MOUTH_BOTTOMRIGHT_Y:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].mouth.offset_y
			+ dev->is_p_region->face[dev->fd_header.index
			+ dev->fd_header.offset].mouth.height;
		break;
	/* 6. Angle information */
	case V4L2_CID_IS_FD_GET_ANGLE:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
				+ dev->fd_header.offset].roll_angle;
		break;
	case V4L2_CID_IS_FD_GET_YAW_ANGLE:
		ctrl->value = dev->is_p_region->face[dev->fd_header.index
				+ dev->fd_header.offset].yaw_angle;
		break;
	/* 7. Update next face information */
	case V4L2_CID_IS_FD_GET_NEXT:
		dev->fd_header.offset++;
		break;
	default:
		return 255;
		break;
	}
#endif
	return ret;
}

static int fimc_is_scalerp_video_g_ext_ctrl(struct file *file, void *priv,
					struct v4l2_ext_controls *ctrls)
{
	struct fimc_is_core *isp = video_drvdata(file);
	struct v4l2_ext_control *ctrl;
	int i, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&isp->slock, flags);
	ctrl = ctrls->controls;
	if (!ctrls->ctrl_class == V4L2_CTRL_CLASS_CAMERA)
		return -EINVAL;

	fimc_is_mem_cache_inv((void *)IS_FACE(isp),
		(unsigned long)(sizeof(struct is_face_marker)*MAX_FACE_COUNT));

	isp->fd_header.offset = 0;

	isp->fd_header.width = (s32)isp->sensor.width ;
	isp->fd_header.height = (s32)isp->sensor.height ;

	for (i = 0; i < ctrls->count; i++) {
		ctrl = ctrls->controls + i;
		ret = fimc_is_g_ext_ctrls_handler(isp, ctrl, i);
		if (ret > 0) {
			ctrls->error_idx = i;
			break;
		} else if (ret < 0) {
			ret = 0;
			break;
		}
	}

	isp->fd_header.index = 0;
	isp->fd_header.count = 0;
	spin_unlock_irqrestore(&isp->slock, flags);
	return ret;
}

static int fimc_is_scalerp_video_s_ctrl(struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct fimc_is_core *isp = video_drvdata(file);
	int ret = 0;

	dbg("fimc_is_scalerp_video_s_ctrl(%d)(%d)\n", ctrl->id, ctrl->value);
#if 1
	switch (ctrl->id) {
	case V4L2_CID_IS_CAMERA_SHOT_MODE_NORMAL:
		ret = fimc_is_v4l2_shot_mode(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_FRAME_RATE:
#ifdef FRAME_RATE_ENABLE
		/* FW partially supported it */
		ret = fimc_is_v4l2_frame_rate(isp, ctrl->value);
#else
		err("WARN(%s) FRAME_RATE is not available now.\n", __func__);
#endif
		break;
	/* Focus */
	case V4L2_CID_IS_CAMERA_OBJECT_POSITION_X:
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		isp->af.pos_x = ctrl->value;
		break;
	case V4L2_CID_IS_CAMERA_OBJECT_POSITION_Y:
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		isp->af.pos_y = ctrl->value;
		break;
	case V4L2_CID_CAMERA_FOCUS_MODE:
		ret = fimc_is_v4l2_af_mode(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		ret = fimc_is_v4l2_af_start_stop(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		ret = fimc_is_v4l2_touch_af_start_stop(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_CAF_START_STOP:
		ret = fimc_is_v4l2_caf_start_stop(isp, ctrl->value);
		break;
	/* AWB, AE Lock/Unlock */
	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
		ret = fimc_is_v4l2_ae_awb_lockunlock(isp, ctrl->value);
		break;
	/* FLASH */
	case V4L2_CID_CAMERA_FLASH_MODE:
		ret = fimc_is_v4l2_isp_flash_mode(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_AWB_MODE:
		ret = fimc_is_v4l2_awb_mode(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ret = fimc_is_v4l2_awb_mode_legacy(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ret = fimc_is_v4l2_isp_effect_legacy(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_IMAGE_EFFECT:
		ret = fimc_is_v4l2_isp_effect(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_ISO:
	case V4L2_CID_CAMERA_ISO:
		ret = fimc_is_v4l2_isp_iso(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ret = fimc_is_v4l2_isp_contrast_legacy(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_CONTRAST:
		ret = fimc_is_v4l2_isp_contrast(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_SATURATION:
	case V4L2_CID_CAMERA_SATURATION:
		ret = fimc_is_v4l2_isp_saturation(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_SHARPNESS:
	case V4L2_CID_CAMERA_SHARPNESS:
		ret = fimc_is_v4l2_isp_sharpness(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_EXPOSURE:
		ret = fimc_is_v4l2_isp_exposure(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		ret = fimc_is_v4l2_isp_exposure_legacy(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_BRIGHTNESS:
		ret = fimc_is_v4l2_isp_brightness(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_HUE:
		ret = fimc_is_v4l2_isp_hue(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_METERING:
		ret = fimc_is_v4l2_isp_metering_legacy(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_METERING:
		ret = fimc_is_v4l2_isp_metering(isp, ctrl->value);
		break;
	/* Only valid at SPOT Mode */
	case V4L2_CID_IS_CAMERA_METERING_POSITION_X:
		IS_ISP_SET_PARAM_METERING_WIN_POS_X(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_METERING_POSITION_Y:
		IS_ISP_SET_PARAM_METERING_WIN_POS_Y(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_METERING_WINDOW_X:
		IS_ISP_SET_PARAM_METERING_WIN_WIDTH(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_METERING_WINDOW_Y:
		IS_ISP_SET_PARAM_METERING_WIN_HEIGHT(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_ANTI_BANDING:
		ret = fimc_is_v4l2_isp_afc_legacy(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_AFC_MODE:
		ret = fimc_is_v4l2_isp_afc(isp, ctrl->value);
		break;
	case V4L2_CID_IS_FD_SET_MAX_FACE_NUMBER:
		/* TODO */
		/*
		if (ctrl->value >= 0) {
			IS_FD_SET_PARAM_FD_CONFIG_CMD(isp,
				FD_CONFIG_COMMAND_MAXIMUM_NUMBER);
			IS_FD_SET_PARAM_FD_CONFIG_MAX_NUMBER(isp, ctrl->value);
			IS_SET_PARAM_BIT(isp, PARAM_FD_CONFIG);
			IS_INC_PARAM_NUM(isp);
			fimc_is_mem_cache_clean((void *)isp->is_p_region,
				IS_PARAM_SIZE);
			fimc_is_hw_set_param(isp);
		}
		*/
		break;
	case V4L2_CID_IS_FD_SET_ROLL_ANGLE:
		ret = fimc_is_v4l2_fd_angle_mode(isp, ctrl->value);
		break;
	case V4L2_CID_IS_FD_SET_DATA_ADDRESS:
		isp->fd_header.target_addr = ctrl->value;
		break;
	case V4L2_CID_IS_SET_ISP:
		ret = fimc_is_v4l2_set_isp(isp, ctrl->value);
		break;
	case V4L2_CID_IS_SET_DRC:
		ret = fimc_is_v4l2_set_drc(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CMD_ISP:
		ret = fimc_is_v4l2_cmd_isp(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CMD_DRC:
		ret = fimc_is_v4l2_cmd_drc(isp, ctrl->value);
		break;
	case V4L2_CID_IS_CMD_FD:
#ifdef FD_ENABLE
		ret = fimc_is_v4l2_cmd_fd(isp, ctrl->value);
#endif
		break;
	case V4L2_CID_CAMERA_SCENE_MODE:
		err("V4L2_CID_CAMERA_SCENE_MODE is not available now.\n");
		break;
	case V4L2_CID_CAMERA_VT_MODE:
		isp->setfile.sub_index = ctrl->value;
		if (ctrl->value == 1)
			printk(KERN_INFO "VT mode is selected\n");
		break;
	case V4L2_CID_CAMERA_SET_ODC:
#ifdef ODC_ENABLE
		ret = fimc_is_ctrl_odc(isp, ctrl->value);
#else
		err("WARN(%s) ODC is not available now.\n", __func__);
#endif
		break;
	case V4L2_CID_CAMERA_SET_3DNR:
#ifdef TDNR_ENABLE
		ret = fimc_is_ctrl_3dnr(isp, ctrl->value);
#else
		err("WARN(%s) 3DNR is not available now.\n", __func__);
#endif
		break;
	case V4L2_CID_CAMERA_ZOOM:
		ret = fimc_is_digital_zoom(isp, ctrl->value);
		break;
	case V4L2_CID_CAMERA_SET_DIS:
#ifdef DIS_ENABLE
		/* FW partially supported it */
		ret = fimc_is_ctrl_dis(isp, ctrl->value);
#else
		err("WARN(%s) DIS is not available now.\n", __func__);
#endif
		break;
	case V4L2_CID_CAMERA_VGA_BLUR:
		break;
	default:
		err("Invalid control\n");
		return -EINVAL;
	}
#endif
	return ret;
}

const struct v4l2_file_operations fimc_is_scalerp_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_scalerp_video_open,
	.release	= fimc_is_scalerp_video_close,
	.poll		= fimc_is_scalerp_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_scalerp_video_mmap,
};

const struct v4l2_ioctl_ops fimc_is_scalerp_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_scalerp_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane
			= fimc_is_scalerp_video_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane
			= fimc_is_scalerp_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane
			= fimc_is_scalerp_video_set_format_mplane,
	.vidioc_try_fmt_vid_cap_mplane
			= fimc_is_scalerp_video_try_format_mplane,
	.vidioc_cropcap			= fimc_is_scalerp_video_cropcap,
	.vidioc_g_crop			= fimc_is_scalerp_video_get_crop,
	.vidioc_s_crop			= fimc_is_scalerp_video_set_crop,
	.vidioc_reqbufs			= fimc_is_scalerp_video_reqbufs,
	.vidioc_querybuf		= fimc_is_scalerp_video_querybuf,
	.vidioc_qbuf			= fimc_is_scalerp_video_qbuf,
	.vidioc_dqbuf			= fimc_is_scalerp_video_dqbuf,
	.vidioc_streamon		= fimc_is_scalerp_video_streamon,
	.vidioc_streamoff		= fimc_is_scalerp_video_streamoff,
	.vidioc_enum_input		= fimc_is_scalerp_video_enum_input,
	.vidioc_g_input			= fimc_is_scalerp_video_g_input,
	.vidioc_s_input			= fimc_is_scalerp_video_s_input,
	.vidioc_g_ctrl			= fimc_is_scalerp_video_g_ctrl,
	.vidioc_s_ctrl			= fimc_is_scalerp_video_s_ctrl,
	.vidioc_g_ext_ctrls		= fimc_is_scalerp_video_g_ext_ctrl,
};

static int fimc_is_scalerp_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			unsigned int *num_buffers,
			unsigned int *num_planes, unsigned int sizes[],
			void *allocators[])
{

	struct fimc_is_video_scp *video = vq->drv_priv;
	struct fimc_is_core	*dev = video->common.core;
	int i;

	*num_planes = video->common.frame.format.num_planes;
	fimc_is_set_plane_size(&video->common.frame, sizes);

	for (i = 0; i < *num_planes; i++)
		allocators[i] =  dev->alloc_ctx;

	dbg("(num_planes : %d)(size : %d)\n", (int)*num_planes, (int)sizes[0]);

	return 0;
}

static int fimc_is_scalerp_buffer_prepare(struct vb2_buffer *vb)
{
	dbg("--%s\n", __func__);
	return 0;
}


static inline void fimc_is_scalerp_lock(struct vb2_queue *vq)
{
	dbg("%s\n", __func__);
}

static inline void fimc_is_scalerp_unlock(struct vb2_queue *vq)
{
	dbg("%s\n", __func__);
}

static int fimc_is_scalerp_start_streaming(struct vb2_queue *q,
						unsigned int count)
{
	struct fimc_is_video_scp *video = q->drv_priv;
	struct fimc_is_core	*isp = video->common.core;
	struct fimc_is_interface *interface = &isp->interface;
	int i, j;
	int buf_index;
	struct scalerp_param *scp_param;
	u32 lindex, hindex, indexes;
	u32 planes;

	dbg_scp("fimc_is_scalerp_start_streaming\n");

	if (test_bit(FIMC_IS_VIDEO_BUFFER_PREPARED, &video->common.state)) {
		dbg_scp("stream_on\n");
		/* buffer addr setting */

		planes = video->common.frame.format.num_planes;
		for (i = 0; i < video->common.buffers; i++) {
			for (j = 0; j < planes; j++) {
				buf_index = i*planes + j;

				dbg_scp("(%d)set buf(%d:%d) = 0x%08x\n",
					buf_index, i, j,
					video->common.buf_dva[i][j]);

				isp->is_p_region->shared[400+buf_index]
					= video->common.buf_dva[i][j];
			}
		}

		dbg_scp("buf_num:%d buf_plane:%d shared[400] : 0x%p\n",
			video->common.buffers,
			video->common.frame.format.num_planes,
			isp->mem.kvaddr_shared + 400 * sizeof(u32));

		video->common.buf_mask = 0;
		for (i = 0; i < video->common.buffers; i++)
			video->common.buf_mask |= (1 << i);
		dbg_scp("initial buffer mask : 0x%08x\n",
			video->common.buf_mask);

		indexes = 0;
		lindex = hindex = 0;

		scp_param = &isp->is_p_region->parameter.scalerp;
		scp_param->dma_output.cmd = DMA_OUTPUT_COMMAND_ENABLE;
		scp_param->dma_output.dma_out_mask = video->common.buf_mask;
		scp_param->dma_output.buffer_number = video->common.buffers;
		scp_param->dma_output.buffer_address =
			(u32)isp->mem.dvaddr_shared + 400*sizeof(u32);

		lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
		hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
		indexes++;

		fimc_is_mem_cache_clean((void *)isp->is_p_region,
			IS_PARAM_SIZE);

		fimc_is_hw_s_param(interface, 0, indexes, lindex, hindex);
		fimc_is_hw_f_param(interface, 0);

		set_bit(FIMC_IS_VIDEO_STREAM_ON, &video->common.state);
	}

	return 0;
}

static int fimc_is_scalerp_stop_streaming(struct vb2_queue *q)
{
	struct fimc_is_video_scp *video = q->drv_priv;
	struct fimc_is_core	*isp = video->common.core;
	int ret;

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
	dbg_sensor("-soff\n");

	IS_SCALERP_SET_PARAM_DMA_OUTPUT_CMD(isp,
		DMA_OUTPUT_COMMAND_DISABLE);
	IS_SCALERP_SET_PARAM_DMA_OUTPUT_BUFFERNUM(isp,
		0);
	IS_SCALERP_SET_PARAM_DMA_OUTPUT_BUFFERADDR(isp,
		0);

	IS_SET_PARAM_BIT(isp, PARAM_SCALERP_DMA_OUTPUT);
	IS_INC_PARAM_NUM(isp);

	fimc_is_mem_cache_clean((void *)isp->is_p_region,
		IS_PARAM_SIZE);

	isp->scenario_id = ISS_PREVIEW_STILL;
	set_bit(IS_ST_INIT_PREVIEW_STILL,	&isp->state);
	clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &isp->state);

	dbg_sensor("+setP\n");
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
	dbg_sensor("-setP\n");

	dbg("IS change mode\n");
	clear_bit(IS_ST_RUN, &isp->state);
	set_bit(IS_ST_CHANGE_MODE, &isp->state);

	dbg_sensor("+mc\n");
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
	dbg_sensor("-mc\n");

	dbg("IS Stream On\n");
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

	if (!test_bit(FIMC_IS_STATE_SCALERC_STREAM_ON, &isp->pipe_state) &&
		!test_bit(FIMC_IS_STATE_3DNR_STREAM_ON, &isp->pipe_state) &&
		test_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state)) {
		clear_bit(IS_ST_STREAM_OFF, &isp->state);

		printk(KERN_INFO "ScalerP stream disable\n");
		fimc_is_hw_set_stream(isp, 0);
		mutex_lock(&isp->lock);
		ret = wait_event_timeout(isp->irq_queue,
			test_bit(IS_ST_STREAM_OFF, &isp->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		mutex_unlock(&isp->lock);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"wait timeout 4: %s\n", __func__);
			return -EBUSY;
		}
		dbg_sensor("-son\n");
		clear_bit(FIMC_IS_STATE_HW_STREAM_ON, &isp->pipe_state);
	}
	clear_bit(IS_ST_RUN, &isp->state);
	clear_bit(IS_ST_STREAM_ON, &isp->state);
	clear_bit(FIMC_IS_STATE_SCALERP_BUFFER_PREPARED, &isp->pipe_state);
	clear_bit(FIMC_IS_STATE_SCALERP_STREAM_ON, &isp->pipe_state);

	isp->setfile.sub_index = 0;

	return 0;
}

static void fimc_is_scalerp_buffer_queue(struct vb2_buffer *vb)
{
	struct fimc_is_video_scp *video = vb->vb2_queue->drv_priv;

	dbg_sensor("%s ++\n", __func__);

	fimc_is_video_buffer_queue(&video->common, vb);
	/*insert request*/
	/*
	fimc_is_sensor_buffer_queue(video->common.device,
		&video->common, vb->v4l2_buf.index);
	*/

	if (!test_bit(FIMC_IS_VIDEO_STREAM_ON, &video->common.state))
		fimc_is_scalerp_start_streaming(vb->vb2_queue,
					video->common.buffers);
}

const struct vb2_ops fimc_is_scalerp_qops = {
	.queue_setup		= fimc_is_scalerp_queue_setup,
	.buf_prepare		= fimc_is_scalerp_buffer_prepare,
	.buf_queue		= fimc_is_scalerp_buffer_queue,
	.wait_prepare		= fimc_is_scalerp_unlock,
	.wait_finish		= fimc_is_scalerp_lock,
	.start_streaming	= fimc_is_scalerp_start_streaming,
	.stop_streaming	= fimc_is_scalerp_stop_streaming,
};
