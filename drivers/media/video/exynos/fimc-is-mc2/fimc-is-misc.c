/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is misc functions(mipi, fimc-lite control)
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/memory.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <linux/jiffies.h>

#include <media/v4l2-subdev.h>
#include <media/exynos_fimc_is.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-helper.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-misc.h"

int fimc_is_ctrl_odc(struct fimc_is_core *dev, int value)
{
	int ret;

	if (value == CAMERA_ODC_ON) {
		/* buffer addr setting */
		dev->back.odc_on = 1;
		dev->is_p_region->shared[250] = (u32)dev->mem.dvaddr_odc;

		IS_ODC_SET_PARAM_CONTROL_BUFFERNUM(dev,
			SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
		IS_ODC_SET_PARAM_CONTROL_BUFFERADDR(dev,
			(u32)dev->mem.dvaddr_shared + 250 * sizeof(u32));
		IS_ODC_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_DISABLE);

	} else if (value == CAMERA_ODC_OFF) {
		dev->back.odc_on = 0;
		IS_ODC_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_ENABLE);
	} else {
		err("invalid ODC setting\n");
		return -1;
	}

	if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
		clear_bit(IS_ST_STREAM_OFF, &dev->state);
		fimc_is_hw_set_stream(dev, 0); /*stream off */
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_STREAM_OFF, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			if (!ret)
				err("s_power off failed!!\n");
			return -EBUSY;
		}
	}

	IS_SET_PARAM_BIT(dev, PARAM_ODC_CONTROL);
	IS_INC_PARAM_NUM(dev);

	fimc_is_mem_cache_clean((void *)dev->is_p_region,
		IS_PARAM_SIZE);

	dev->scenario_id = ISS_PREVIEW_STILL;
	set_bit(IS_ST_INIT_PREVIEW_STILL,	&dev->state);
	clear_bit(IS_ST_INIT_CAPTURE_STILL, &dev->state);
	clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state);
	fimc_is_hw_set_param(dev);
	ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		dev_err(&dev->pdev->dev,
			"wait timeout : %s\n", __func__);
		return -EBUSY;
	}

	if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
		clear_bit(IS_ST_RUN, &dev->state);
		set_bit(IS_ST_CHANGE_MODE, &dev->state);
		fimc_is_hw_change_mode(dev, IS_MODE_PREVIEW_STILL);
		ret = wait_event_timeout(dev->irq_queue,
				test_bit(IS_ST_CHANGE_MODE_DONE, &dev->state),
				(3*HZ)/*FIMC_IS_SHUTDOWN_TIMEOUT*/);

		if (!ret) {
			dev_err(&dev->pdev->dev,
				"Mode change timeout:%s\n", __func__);
			return -EBUSY;
		}

		clear_bit(IS_ST_STREAM_ON, &dev->state);
		fimc_is_hw_set_stream(dev, 1);

		ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_STREAM_ON, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
	}
	clear_bit(IS_ST_STREAM_ON, &dev->state);

	return 0;
}

int fimc_is_ctrl_dis(struct fimc_is_core *dev, int value)
{
	int ret;

	if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
		clear_bit(IS_ST_STREAM_OFF, &dev->state);
		fimc_is_hw_set_stream(dev, 0); /*stream off */
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_STREAM_OFF, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"wait timeout : %s\n", __func__);
			if (!ret)
				err("s_power off failed!!\n");
			return -EBUSY;
		}
	}

	if (value == CAMERA_DIS_ON) {
		/* buffer addr setting */
		dev->back.dis_on = 1;
		dev->is_p_region->shared[300] = (u32)dev->mem.dvaddr_dis;

		IS_DIS_SET_PARAM_CONTROL_BUFFERNUM(dev,
			SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
		IS_DIS_SET_PARAM_CONTROL_BUFFERADDR(dev,
			(u32)dev->mem.dvaddr_shared + 300 * sizeof(u32));
		IS_DIS_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_DISABLE);
	} else if (value == CAMERA_DIS_OFF) {
		dev->back.dis_on = 0;
		IS_DIS_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_ENABLE);
	} else {
		err("invalid DIS setting\n");
		return -1;
	}

	IS_SET_PARAM_BIT(dev, PARAM_DIS_CONTROL);
	IS_INC_PARAM_NUM(dev);

	fimc_is_mem_cache_clean((void *)dev->is_p_region,
		IS_PARAM_SIZE);

	dev->scenario_id = ISS_PREVIEW_STILL;
	set_bit(IS_ST_INIT_PREVIEW_STILL,	&dev->state);
	clear_bit(IS_ST_INIT_CAPTURE_STILL, &dev->state);
	clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state);
	fimc_is_hw_set_param(dev);
#ifdef DIS_ENABLE
	/* FW bug - should be wait */
	ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		dev_err(&dev->pdev->dev,
			"set param wait timeout : %s\n", __func__);
		return -EBUSY;
	}
#endif

	if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
		clear_bit(IS_ST_RUN, &dev->state);
		set_bit(IS_ST_CHANGE_MODE, &dev->state);
		fimc_is_hw_change_mode(dev, IS_MODE_PREVIEW_STILL);
		ret = wait_event_timeout(dev->irq_queue,
				test_bit(IS_ST_CHANGE_MODE_DONE, &dev->state),
				(3*HZ)/*FIMC_IS_SHUTDOWN_TIMEOUT*/);

		if (!ret) {
			dev_err(&dev->pdev->dev,
				"Mode change timeout:%s\n", __func__);
			return -EBUSY;
		}

		clear_bit(IS_ST_STREAM_ON, &dev->state);
		fimc_is_hw_set_stream(dev, 1);

		ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_STREAM_ON, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&dev->pdev->dev,
				"stream on wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		clear_bit(IS_ST_STREAM_ON, &dev->state);
	}

	return 0;
}

int fimc_is_ctrl_3dnr(struct fimc_is_core *dev, int value)
{
	int ret;

	if (value == CAMERA_3DNR_ON) {
		/* buffer addr setting */
		dev->back.tdnr_on = 1;
		dev->is_p_region->shared[350] = (u32)dev->mem.dvaddr_3dnr;
		dbg("3dnr buf:0x%08x size : 0x%08x\n",
			dev->is_p_region->shared[350],
			SIZE_3DNR_INTERNAL_BUF*NUM_3DNR_INTERNAL_BUF);

		IS_TDNR_SET_PARAM_CONTROL_BUFFERNUM(dev,
			SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF);
		IS_TDNR_SET_PARAM_CONTROL_BUFFERADDR(dev,
			(u32)dev->mem.dvaddr_shared + 350 * sizeof(u32));
		IS_TDNR_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_DISABLE);

	} else if (value == CAMERA_3DNR_OFF) {
		dbg("disable 3DNR\n");
		dev->back.tdnr_on = 0;
		IS_TDNR_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_ENABLE);
	} else {
		err("invalid ODC setting\n");
		return -1;
	}

	dbg("IS_ST_STREAM_OFF\n");
	clear_bit(IS_ST_STREAM_OFF, &dev->state);
	fimc_is_hw_set_stream(dev, 0); /*stream off */
	ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_STREAM_OFF, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		dev_err(&dev->pdev->dev,
			"wait timeout : %s\n", __func__);
		if (!ret)
			err("s_power off failed!!\n");
		return -EBUSY;
	}

	IS_SET_PARAM_BIT(dev, PARAM_TDNR_CONTROL);
	IS_INC_PARAM_NUM(dev);

	fimc_is_mem_cache_clean((void *)dev->is_p_region,
		IS_PARAM_SIZE);

	dev->scenario_id = ISS_PREVIEW_STILL;
	set_bit(IS_ST_INIT_PREVIEW_STILL,	&dev->state);
	clear_bit(IS_ST_INIT_CAPTURE_STILL, &dev->state);
	clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state);
	fimc_is_hw_set_param(dev);
	ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		dev_err(&dev->pdev->dev,
			"wait timeout : %s\n", __func__);
		return -EBUSY;
	}

	dbg("IS change mode\n");
	clear_bit(IS_ST_RUN, &dev->state);
	set_bit(IS_ST_CHANGE_MODE, &dev->state);
	fimc_is_hw_change_mode(dev, IS_MODE_PREVIEW_STILL);
	ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_CHANGE_MODE_DONE, &dev->state),
			(3*HZ)/*FIMC_IS_SHUTDOWN_TIMEOUT*/);

	if (!ret) {
		dev_err(&dev->pdev->dev,
			"Mode change timeout:%s\n", __func__);
		return -EBUSY;
	}

	dbg("IS_ST_STREAM_ON\n");
	clear_bit(IS_ST_STREAM_ON, &dev->state);
	fimc_is_hw_set_stream(dev, 1);

	ret = wait_event_timeout(dev->irq_queue,
	test_bit(IS_ST_STREAM_ON, &dev->state),
	FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		dev_err(&dev->pdev->dev,
			"wait timeout : %s\n", __func__);
		return -EBUSY;
	}
	clear_bit(IS_ST_STREAM_ON, &dev->state);

	return 0;
}

int fimc_is_digital_zoom(struct fimc_is_core *dev, int value)
{
	u32 front_width, front_height;
	u32 back_width, back_height;
	u32 dis_width, dis_height;
	u32 front_crop_ratio, back_crop_ratio;
	u32 crop_width = 0, crop_height = 0, crop_x, crop_y;
	u32 zoom;
	int ret;

	front_width = dev->front.width;
	front_height = dev->front.height;
	back_width = dev->back.width;
	back_height = dev->back.height;
	dis_width = dev->back.dis_width;
	dis_height = dev->back.dis_height;

	front_crop_ratio = front_width * 1000 / front_height;
	back_crop_ratio = back_width * 1000 / back_height;

	if (front_crop_ratio == back_crop_ratio) {
		crop_width = front_width;
		crop_height = front_height;

	} else if (front_crop_ratio < back_crop_ratio) {
		crop_width = front_width;
		crop_height = (front_width
				* (1000 * 100 / back_crop_ratio)) / 100;
		crop_width = ALIGN(crop_width, 8);
		crop_height = ALIGN(crop_height, 8);

	} else if (front_crop_ratio > back_crop_ratio) {
		crop_height = front_height;
		crop_width = (front_height
				* (back_crop_ratio * 100 / 1000)) / 100 ;
		crop_width = ALIGN(crop_width, 8);
		crop_height = ALIGN(crop_height, 8);
	}

	dbg("value: %d front_width: %d, front_height: %d\n",
		value, front_width,  front_height);
	dbg("value: %d dis_width: %d, dis_height: %d\n",
		value, dis_width, dis_height);

	zoom = value + 10;

	dbg("zoom value : %d\n", value);

	crop_width = crop_width * 10 / zoom;
	crop_height = crop_height * 10 / zoom;

	crop_width &= 0xffe;
	crop_height &= 0xffe;

	crop_x = (front_width - crop_width)/2;
	crop_y = (front_height - crop_height)/2;

	crop_x &= 0xffe;
	crop_y &= 0xffe;

	dbg("crop_width : %d crop_height: %d\n",
		crop_width, crop_height);
	dbg("crop_x:%d crop_y: %d\n", crop_x, crop_y);

	IS_SCALERC_SET_PARAM_INPUT_CROP_WIDTH(dev,
		crop_width);
	IS_SCALERC_SET_PARAM_INPUT_CROP_HEIGHT(dev,
		crop_height);
	IS_SCALERC_SET_PARAM_INPUT_CROP_POS_X(dev,
		crop_x);
	IS_SCALERC_SET_PARAM_INPUT_CROP_POS_Y(dev,
		crop_y);

	IS_SCALERC_SET_PARAM_INPUT_CROP_IN_WIDTH(dev,
		front_width);
	IS_SCALERC_SET_PARAM_INPUT_CROP_IN_HEIGHT(dev,
		front_height);
	IS_SCALERC_SET_PARAM_INPUT_CROP_OUT_WIDTH(dev,
		dis_width);
	IS_SCALERC_SET_PARAM_INPUT_CROP_OUT_HEIGHT(dev,
		dis_height);
	IS_SCALERC_SET_PARAM_INPUT_CROP_CMD(dev,
		SCALER_CROP_COMMAND_ENABLE);

	IS_SET_PARAM_BIT(dev, PARAM_SCALERC_INPUT_CROP);
	IS_INC_PARAM_NUM(dev);

	fimc_is_mem_cache_clean((void *)dev->is_p_region,
		IS_PARAM_SIZE);

	dev->scenario_id = ISS_PREVIEW_STILL;
	set_bit(IS_ST_INIT_PREVIEW_STILL,	&dev->state);

	clear_bit(IS_ST_INIT_CAPTURE_STILL, &dev->state);
	clear_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state);

	fimc_is_hw_set_param(dev);
	ret = wait_event_timeout(dev->irq_queue,
		test_bit(IS_ST_INIT_PREVIEW_VIDEO, &dev->state),
		FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		dev_err(&dev->pdev->dev,
			"wait timeout : %s\n", __func__);
		return -EBUSY;
	}

	return 0;
}

int fimc_is_af_face(struct fimc_is_core *dev)
{
#if 0

	int ret = 0, max_confidence = 0, i = 0;

	int width, height;
	u32 touch_x = 0, touch_y = 0;

	for (i = dev->fd_header.index;
		i < (dev->fd_header.index + dev->fd_header.count); i++) {
		if (max_confidence < dev->is_p_region->face[i].confidence) {
			max_confidence = dev->is_p_region->face[i].confidence;
			touch_x = dev->is_p_region->face[i].face.offset_x +
				(dev->is_p_region->face[i].face.width / 2);
			touch_y = dev->is_p_region->face[i].face.offset_y +
				(dev->is_p_region->face[i].face.height / 2);
		}
	}
	width = dev->sensor.width;
	height = dev->sensor.height;
	touch_x = 1024 *  touch_x / (u32)width;
	touch_y = 1024 *  touch_y / (u32)height;

	if ((touch_x == 0) || (touch_y == 0) || (max_confidence < 50))
		return ret;

	if (dev->af.prev_pos_x == 0 && dev->af.prev_pos_y == 0) {
		dev->af.prev_pos_x = touch_x;
		dev->af.prev_pos_y = touch_y;
	} else {
		if (abs(dev->af.prev_pos_x - touch_x) < 100 &&
			abs(dev->af.prev_pos_y - touch_y) < 100) {
			return ret;
		}
		dbg("AF Face level = %d\n", max_confidence);
		dbg("AF Face = <%d, %d>\n", touch_x, touch_y);
		dbg("AF Face = prev <%d, %d>\n",
				dev->af.prev_pos_x, dev->af.prev_pos_y);
		dev->af.prev_pos_x = touch_x;
		dev->af.prev_pos_y = touch_y;
	}

	IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
	IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
	IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_TOUCH);
	IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
	IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
	IS_ISP_SET_PARAM_AA_TOUCH_X(dev, touch_x);
	IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, touch_y);
	IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
	IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
	IS_INC_PARAM_NUM(dev);
	dev->af.af_state = FIMC_IS_AF_SETCONFIG;
	fimc_is_mem_cache_clean((void *)dev->is_p_region, IS_PARAM_SIZE);
	fimc_is_hw_set_param(dev);

	return ret;
#else
	return 0;
#endif

}

int fimc_is_v4l2_af_mode(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case FOCUS_MODE_AUTO:
		dev->af.mode = IS_FOCUS_MODE_AUTO;
		break;
	case FOCUS_MODE_MACRO:
		dev->af.mode = IS_FOCUS_MODE_MACRO;
		break;
	case FOCUS_MODE_INFINITY:
		dev->af.mode = IS_FOCUS_MODE_INFINITY;
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
		IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_MANUAL);
		IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
		IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
		IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
		IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
		IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		dev->af.af_state = FIMC_IS_AF_SETCONFIG;
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case FOCUS_MODE_CONTINUOUS:
		dev->af.mode = IS_FOCUS_MODE_CONTINUOUS;
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
		IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_CONTINUOUS);
		IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
		IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
		IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
		IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
		IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		dev->af.af_state = FIMC_IS_AF_SETCONFIG;
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		dev->af.af_lock_state = 0;
		dev->af.ae_lock_state = 0;
		dev->af.awb_lock_state = 0;
		dev->af.prev_pos_x = 0;
		dev->af.prev_pos_y = 0;
		break;
	case FOCUS_MODE_TOUCH:
#if 0
		dev->af.mode = IS_FOCUS_MODE_TOUCH;
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
		IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_TOUCH);
		IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
		IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
		IS_ISP_SET_PARAM_AA_TOUCH_X(dev, dev->af.pos_x);
		IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, dev->af.pos_y);
		IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		dev->af.af_state = FIMC_IS_AF_SETCONFIG;
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		dev->af.af_lock_state = 0;
		dev->af.ae_lock_state = 0;
		dev->af.awb_lock_state = 0;
#endif
		break;
	case FOCUS_MODE_FACEDETECT:
		dev->af.mode = IS_FOCUS_MODE_FACEDETECT;
		dev->af.af_lock_state = 0;
		dev->af.ae_lock_state = 0;
		dev->af.awb_lock_state = 0;
		dev->af.prev_pos_x = 0;
		dev->af.prev_pos_y = 0;
		break;
	default:
		return ret;
	}
	return ret;
}

int fimc_is_v4l2_af_start_stop(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case AUTO_FOCUS_OFF:
		if (!is_af_use(dev)) {
			/* 6A3 can't support AF */
			dev->af.af_state = FIMC_IS_AF_IDLE;
		} else {
			if (dev->af.af_state == FIMC_IS_AF_IDLE)
				return ret;
			/* Abort or lock AF */
			dev->af.af_state = FIMC_IS_AF_ABORT;
			IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_STOP);
			IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
			switch (dev->af.mode) {
			case IS_FOCUS_MODE_AUTO:
				IS_ISP_SET_PARAM_AA_MODE(dev,
					ISP_AF_SINGLE);
				IS_ISP_SET_PARAM_AA_SCENE(dev,
					ISP_AF_SCENE_NORMAL);
				IS_ISP_SET_PARAM_AA_SLEEP(dev,
					ISP_AF_SLEEP_OFF);
				IS_ISP_SET_PARAM_AA_FACE(dev,
					ISP_AF_FACE_DISABLE);
				IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
				IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
				IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
				IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
				IS_INC_PARAM_NUM(dev);
				fimc_is_mem_cache_clean(
					(void *)dev->is_p_region,
					IS_PARAM_SIZE);
				fimc_is_hw_set_param(dev);
					break;
			case IS_FOCUS_MODE_MACRO:
				IS_ISP_SET_PARAM_AA_MODE(dev,
					ISP_AF_SINGLE);
				IS_ISP_SET_PARAM_AA_SCENE(dev,
					ISP_AF_SCENE_MACRO);
				IS_ISP_SET_PARAM_AA_SLEEP(dev,
					ISP_AF_SLEEP_OFF);
				IS_ISP_SET_PARAM_AA_FACE(dev,
					ISP_AF_FACE_DISABLE);
				IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
				IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
				IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
				IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
				IS_INC_PARAM_NUM(dev);
				fimc_is_mem_cache_clean(
					(void *)dev->is_p_region,
					IS_PARAM_SIZE);
				fimc_is_hw_set_param(dev);
				ret = wait_event_timeout(dev->irq_queue,
				(dev->af.af_state == FIMC_IS_AF_IDLE), HZ/5);
				if (!ret) {
					dev_err(&dev->pdev->dev,
					"Focus change timeout:%s\n", __func__);
					return -EBUSY;
				}
				break;
			case IS_FOCUS_MODE_CONTINUOUS:
				IS_ISP_SET_PARAM_AA_MODE(dev,
					ISP_AF_CONTINUOUS);
				IS_ISP_SET_PARAM_AA_SCENE(dev,
					ISP_AF_SCENE_NORMAL);
				IS_ISP_SET_PARAM_AA_SLEEP(dev,
						ISP_AF_SLEEP_OFF);
				IS_ISP_SET_PARAM_AA_FACE(dev,
					ISP_AF_FACE_DISABLE);
				IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
				IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
				IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
				IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
				IS_INC_PARAM_NUM(dev);
				fimc_is_mem_cache_clean(
					(void *)dev->is_p_region,
					IS_PARAM_SIZE);
				fimc_is_hw_set_param(dev);
				ret = wait_event_timeout(dev->irq_queue,
				(dev->af.af_state == FIMC_IS_AF_IDLE), HZ/5);
				if (!ret) {
					dev_err(&dev->pdev->dev,
					"Focus change timeout:%s\n", __func__);
					return -EBUSY;
				}
				break;
			default:
				/* If other AF mode, there is no
				cancelation process*/
				break;
			}
			dev->af.mode = IS_FOCUS_MODE_IDLE;
		}
		break;
	case AUTO_FOCUS_ON:
		if (!is_af_use(dev)) {
			/* 6A3 can't support AF */
			dev->af.af_state = FIMC_IS_AF_LOCK;
			dev->af.af_lock_state = FIMC_IS_AF_LOCKED;
			dev->is_shared_region->af_status = 1;
			fimc_is_mem_cache_clean((void *)IS_SHARED(dev),
			(unsigned long)(sizeof(struct is_share_region)));
		} else {
			dev->af.af_lock_state = 0;
			dev->af.ae_lock_state = 0;
			dev->af.awb_lock_state = 0;
			dev->is_shared_region->af_status = 0;
			fimc_is_mem_cache_clean((void *)IS_SHARED(dev),
			(unsigned long)(sizeof(struct is_share_region)));
			IS_ISP_SET_PARAM_AA_CMD(dev,
			ISP_AA_COMMAND_START);
			IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
			IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_SINGLE);
			IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
			IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
			IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
			IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
			IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
			switch (dev->af.mode) {
			case IS_FOCUS_MODE_AUTO:
				IS_ISP_SET_PARAM_AA_SCENE(dev,
					ISP_AF_SCENE_NORMAL);
				IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
				IS_INC_PARAM_NUM(dev);
				dev->af.af_state =
						FIMC_IS_AF_SETCONFIG;
				fimc_is_mem_cache_clean(
					(void *)dev->is_p_region,
					IS_PARAM_SIZE);
				fimc_is_hw_set_param(dev);
				ret = wait_event_timeout(dev->irq_queue,
				(dev->af.af_state == FIMC_IS_AF_RUNNING), HZ/5);
				if (!ret) {
					dev_err(&dev->pdev->dev,
					"Focus change timeout:%s\n", __func__);
					return -EBUSY;
				}
				break;
			case IS_FOCUS_MODE_MACRO:
				IS_ISP_SET_PARAM_AA_SCENE(dev,
					ISP_AF_SCENE_MACRO);
				IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
				IS_INC_PARAM_NUM(dev);
				dev->af.af_state =
						FIMC_IS_AF_SETCONFIG;
				fimc_is_mem_cache_clean(
					(void *)dev->is_p_region,
					IS_PARAM_SIZE);
				fimc_is_hw_set_param(dev);
				ret = wait_event_timeout(dev->irq_queue,
				(dev->af.af_state == FIMC_IS_AF_RUNNING), HZ/5);
				if (!ret) {
					dev_err(&dev->pdev->dev,
					"Focus change timeout:%s\n", __func__);
					return -EBUSY;
				}
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}
	return ret;
}

int fimc_is_v4l2_touch_af_start_stop(struct fimc_is_core *dev, int value)
{
	int ret = 0;
#if 0
	switch (value) {
	case TOUCH_AF_STOP:
		if (!is_af_use(dev)) {
			/* 6A3 can't support AF */
			dev->af.af_state = FIMC_IS_AF_IDLE;
		} else {
			if (dev->af.af_state == FIMC_IS_AF_IDLE)
				return ret;
			/* Abort or lock CAF */
			dev->af.af_state = FIMC_IS_AF_ABORT;
			IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_STOP);
			IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);

			IS_ISP_SET_PARAM_AA_MODE(dev,
				ISP_AF_TOUCH);
			IS_ISP_SET_PARAM_AA_SCENE(dev,
				ISP_AF_SCENE_NORMAL);
			IS_ISP_SET_PARAM_AA_SLEEP(dev,
					ISP_AF_SLEEP_OFF);
			IS_ISP_SET_PARAM_AA_FACE(dev,
				ISP_AF_FACE_DISABLE);
			IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
			IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
			IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
			IS_INC_PARAM_NUM(dev);
			fimc_is_mem_cache_clean(
				(void *)dev->is_p_region,
				IS_PARAM_SIZE);
			fimc_is_hw_set_param(dev);
			ret = wait_event_timeout(dev->irq_queue,
			(dev->af.af_state == FIMC_IS_AF_IDLE), HZ/5);
			if (!ret) {
				dev_err(&dev->pdev->dev,
				"Focus change timeout:%s\n", __func__);
				return -EBUSY;
			}
		break;
	case TOUCH_AF_START:
		if (!is_af_use(dev)) {
			/* 6A3 can't support AF */
			dev->af.af_state = FIMC_IS_AF_LOCK;
			dev->af.af_lock_state = FIMC_IS_AF_LOCKED;
			dev->is_shared_region->af_status = 1;
			fimc_is_mem_cache_clean((void *)IS_SHARED(dev),
			(unsigned long)(sizeof(struct is_share_region)));
		} else {
			dev->af.mode = IS_FOCUS_MODE_TOUCH;
			IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
			IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
			IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_TOUCH);
			IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
			IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
			IS_ISP_SET_PARAM_AA_TOUCH_X(dev, dev->af.pos_x);
			IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, dev->af.pos_y);
			IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
			IS_INC_PARAM_NUM(dev);
			dev->af.af_state = FIMC_IS_AF_SETCONFIG;
			fimc_is_mem_cache_clean((void *)dev->is_p_region,
				IS_PARAM_SIZE);
			fimc_is_hw_set_param(dev);
			dev->af.af_lock_state = 0;
			dev->af.ae_lock_state = 0;
			dev->af.awb_lock_state = 0;
		}
		break;
	default:
		break;
	}
#endif
	return ret;
}

int fimc_is_v4l2_caf_start_stop(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case CAF_STOP:
		if (!is_af_use(dev)) {
			/* 6A3 can't support AF */
			dev->af.af_state = FIMC_IS_AF_IDLE;
		} else {
			if (dev->af.af_state == FIMC_IS_AF_IDLE)
				return ret;
			/* Abort or lock CAF */
			dev->af.af_state = FIMC_IS_AF_ABORT;
			IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_STOP);
			IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);

			IS_ISP_SET_PARAM_AA_MODE(dev,
				ISP_AF_CONTINUOUS);
			IS_ISP_SET_PARAM_AA_SCENE(dev,
				ISP_AF_SCENE_NORMAL);
			IS_ISP_SET_PARAM_AA_SLEEP(dev,
					ISP_AF_SLEEP_OFF);
			IS_ISP_SET_PARAM_AA_FACE(dev,
				ISP_AF_FACE_DISABLE);
			IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
			IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
			IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
			IS_INC_PARAM_NUM(dev);
			fimc_is_mem_cache_clean(
				(void *)dev->is_p_region,
				IS_PARAM_SIZE);
			fimc_is_hw_set_param(dev);
			ret = wait_event_timeout(dev->irq_queue,
			(dev->af.af_state == FIMC_IS_AF_IDLE), HZ/5);
			if (!ret) {
				dev_err(&dev->pdev->dev,
				"Focus change timeout:%s\n", __func__);
				return -EBUSY;
			}
		}
		break;
	case CAF_START:
		if (!is_af_use(dev)) {
			/* 6A3 can't support AF */
			dev->af.af_state = FIMC_IS_AF_LOCK;
			dev->af.af_lock_state = FIMC_IS_AF_LOCKED;
			dev->is_shared_region->af_status = 1;
			fimc_is_mem_cache_clean((void *)IS_SHARED(dev),
			(unsigned long)(sizeof(struct is_share_region)));
		} else {
			dev->af.mode = IS_FOCUS_MODE_CONTINUOUS;
			IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
			IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AF);
			IS_ISP_SET_PARAM_AA_MODE(dev, ISP_AF_CONTINUOUS);
			IS_ISP_SET_PARAM_AA_SLEEP(dev, ISP_AF_SLEEP_OFF);
			IS_ISP_SET_PARAM_AA_FACE(dev, ISP_AF_FACE_DISABLE);
			IS_ISP_SET_PARAM_AA_TOUCH_X(dev, 0);
			IS_ISP_SET_PARAM_AA_TOUCH_Y(dev, 0);
			IS_ISP_SET_PARAM_AA_MANUAL_AF(dev, 0);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
			IS_INC_PARAM_NUM(dev);
			dev->af.af_state = FIMC_IS_AF_SETCONFIG;
			fimc_is_mem_cache_clean((void *)dev->is_p_region,
				IS_PARAM_SIZE);
			fimc_is_hw_set_param(dev);
			dev->af.af_lock_state = 0;
			dev->af.ae_lock_state = 0;
			dev->af.awb_lock_state = 0;
			dev->af.prev_pos_x = 0;
			dev->af.prev_pos_y = 0;
		}
		break;
	default:
		break;
	}
	return ret;
}

int fimc_is_v4l2_isp_iso(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case ISO_AUTO:
		IS_ISP_SET_PARAM_ISO_CMD(dev, ISP_ISO_COMMAND_AUTO);
		IS_ISP_SET_PARAM_ISO_VALUE(dev, 0);
		break;
	case ISO_100:
		IS_ISP_SET_PARAM_ISO_CMD(dev, ISP_ISO_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_ISO_VALUE(dev, 100);
		break;
	case ISO_200:
		IS_ISP_SET_PARAM_ISO_CMD(dev, ISP_ISO_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_ISO_VALUE(dev, 200);
		break;
	case ISO_400:
		IS_ISP_SET_PARAM_ISO_CMD(dev, ISP_ISO_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_ISO_VALUE(dev, 400);
		break;
	case ISO_800:
		IS_ISP_SET_PARAM_ISO_CMD(dev, ISP_ISO_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_ISO_VALUE(dev, 800);
		break;
	case ISO_1600:
		IS_ISP_SET_PARAM_ISO_CMD(dev, ISP_ISO_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_ISO_VALUE(dev, 1600);
		break;
	default:
		return ret;
	}
	if (value >= ISO_AUTO && value < ISO_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ISO);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
							IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_effect(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_IMAGE_EFFECT_DISABLE:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev, ISP_IMAGE_EFFECT_DISABLE);
		break;
	case IS_IMAGE_EFFECT_MONOCHROME:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev, ISP_IMAGE_EFFECT_MONOCHROME);
		break;
	case IS_IMAGE_EFFECT_SEPIA:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev, ISP_IMAGE_EFFECT_SEPIA);
		break;
	case IS_IMAGE_EFFECT_NEGATIVE_COLOR:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			ISP_IMAGE_EFFECT_NEGATIVE_COLOR);
		break;
	case IS_IMAGE_EFFECT_EMBOSSING:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			ISP_IMAGE_EFFECT_EMBOSS);
		break;
	}
	/* only ISP effect in Pegasus */
	if (value >= 0 && value < 5) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_IMAGE_EFFECT);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_effect_legacy(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IMAGE_EFFECT_NONE:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev, ISP_IMAGE_EFFECT_DISABLE);
		break;
	case IMAGE_EFFECT_BNW:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev, ISP_IMAGE_EFFECT_MONOCHROME);
		break;
	case IMAGE_EFFECT_NEGATIVE:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev,
			ISP_IMAGE_EFFECT_NEGATIVE_COLOR);
		break;
	case IMAGE_EFFECT_SEPIA:
		IS_ISP_SET_PARAM_EFFECT_CMD(dev, ISP_IMAGE_EFFECT_SEPIA);
		break;
	default:
		return ret;
	}
	/* only ISP effect in Pegasus */
	if (value > IMAGE_EFFECT_BASE && value < IMAGE_EFFECT_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_IMAGE_EFFECT);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_flash_mode(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case FLASH_MODE_OFF:
		IS_ISP_SET_PARAM_FLASH_CMD(dev, ISP_FLASH_COMMAND_DISABLE);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev, ISP_FLASH_REDEYE_DISABLE);
		break;
	case FLASH_MODE_AUTO:
		IS_ISP_SET_PARAM_FLASH_CMD(dev, ISP_FLASH_COMMAND_AUTO);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev, ISP_FLASH_REDEYE_ENABLE);
		break;
	case FLASH_MODE_ON:
		IS_ISP_SET_PARAM_FLASH_CMD(dev, ISP_FLASH_COMMAND_MANUALON);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev, ISP_FLASH_REDEYE_DISABLE);
		break;
	case FLASH_MODE_TORCH:
		/* HACK : currently ISP_FLASH_COMMAND_TORCH is not supported */
		/* IS_ISP_SET_PARAM_FLASH_CMD(dev, ISP_FLASH_COMMAND_TORCH); */
		IS_ISP_SET_PARAM_FLASH_CMD(dev, ISP_FLASH_COMMAND_MANUALON);
		IS_ISP_SET_PARAM_FLASH_REDEYE(dev, ISP_FLASH_REDEYE_DISABLE);
		break;
	default:
		return ret;
	}
	if (value > FLASH_MODE_BASE && value < FLASH_MODE_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_FLASH);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_awb_mode(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_AWB_AUTO:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_AUTO);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev, 0);
		break;
	case IS_AWB_DAYLIGHT:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
				ISP_AWB_ILLUMINATION_DAYLIGHT);
		break;
	case IS_AWB_CLOUDY:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
				ISP_AWB_ILLUMINATION_CLOUDY);
		break;
	case IS_AWB_TUNGSTEN:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			ISP_AWB_ILLUMINATION_TUNGSTEN);
		break;
	case IS_AWB_FLUORESCENT:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			ISP_AWB_ILLUMINATION_FLUORESCENT);
		break;
	}
	if (value >= IS_AWB_AUTO && value < IS_AWB_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AWB);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_awb_mode_legacy(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case WHITE_BALANCE_AUTO:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_AUTO);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev, 0);
		break;
	case WHITE_BALANCE_SUNNY:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
				ISP_AWB_ILLUMINATION_DAYLIGHT);
		break;
	case WHITE_BALANCE_CLOUDY:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
				ISP_AWB_ILLUMINATION_CLOUDY);
		break;
	case WHITE_BALANCE_TUNGSTEN:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			ISP_AWB_ILLUMINATION_TUNGSTEN);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		IS_ISP_SET_PARAM_AWB_CMD(dev, ISP_AWB_COMMAND_ILLUMINATION);
		IS_ISP_SET_PARAM_AWB_ILLUMINATION(dev,
			ISP_AWB_ILLUMINATION_FLUORESCENT);
		break;
	}
	if (value > WHITE_BALANCE_BASE && value < WHITE_BALANCE_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AWB);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_contrast(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_CONTRAST_AUTO:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev, ISP_ADJUST_COMMAND_AUTO);
		break;
	case IS_CONTRAST_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, -2);
		break;
	case IS_CONTRAST_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, -1);
		break;
	case IS_CONTRAST_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, 0);
		break;
	case IS_CONTRAST_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, 1);
		break;
	case IS_CONTRAST_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, 2);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_CONTRAST_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_contrast_legacy(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case CONTRAST_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, -2);
		break;
	case CONTRAST_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, -1);
		break;
	case CONTRAST_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, 0);
		break;
	case CONTRAST_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, 1);
		break;
	case CONTRAST_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_CONTRAST);
		IS_ISP_SET_PARAM_ADJUST_CONTRAST(dev, 2);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < CONTRAST_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_saturation(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case SATURATION_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SATURATION);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev, -2);
		break;
	case SATURATION_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SATURATION);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev, -1);
		break;
	case SATURATION_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SATURATION);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev, 0);
		break;
	case SATURATION_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SATURATION);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev, 1);
		break;
	case SATURATION_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SATURATION);
		IS_ISP_SET_PARAM_ADJUST_SATURATION(dev, 2);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < SATURATION_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_sharpness(struct fimc_is_core *dev, int value)
{
	int ret = 0;

	switch (value) {
	case SHARPNESS_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SHARPNESS);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev, -2);
		break;
	case SHARPNESS_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SHARPNESS);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev, -1);
		break;
	case SHARPNESS_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SHARPNESS);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev, 0);
		break;
	case SHARPNESS_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SHARPNESS);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev, 1);
		break;
	case SHARPNESS_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_SHARPNESS);
		IS_ISP_SET_PARAM_ADJUST_SHARPNESS(dev, 2);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < SHARPNESS_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_exposure(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_EXPOSURE_MINUS_4:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, -4);
		break;
	case IS_EXPOSURE_MINUS_3:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, -3);
		break;
	case IS_EXPOSURE_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, -2);
		break;
	case IS_EXPOSURE_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, -1);
		break;
	case IS_EXPOSURE_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, 0);
		break;
	case IS_EXPOSURE_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, 1);
		break;
	case IS_EXPOSURE_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, 2);
		break;
	case IS_EXPOSURE_PLUS_3:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, 3);
		break;
	case IS_EXPOSURE_PLUS_4:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, 4);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_EXPOSURE_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_exposure_legacy(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	if (value >= -4 && value < 5) {
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_EXPOSURE);
		IS_ISP_SET_PARAM_ADJUST_EXPOSURE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_brightness(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_BRIGHTNESS_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev, -2);
		break;
	case IS_BRIGHTNESS_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev, -1);
		break;
	case IS_BRIGHTNESS_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev, 0);
		break;
	case IS_BRIGHTNESS_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev, 1);
		break;
	case IS_BRIGHTNESS_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS);
		IS_ISP_SET_PARAM_ADJUST_BRIGHTNESS(dev, 2);
		break;
	}
	if (value >= 0 && value < IS_BRIGHTNESS_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_hue(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_HUE_MINUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_HUE);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev, -2);
		break;
	case IS_HUE_MINUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_HUE);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev, -1);
		break;
	case IS_HUE_DEFAULT:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_HUE);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev, 0);
		break;
	case IS_HUE_PLUS_1:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_HUE);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev, 1);
		break;
	case IS_HUE_PLUS_2:
		IS_ISP_SET_PARAM_ADJUST_CMD(dev,
					ISP_ADJUST_COMMAND_MANUAL_HUE);
		IS_ISP_SET_PARAM_ADJUST_HUE(dev, 2);
		break;
	default:
		return ret;
	}
	if (value >= IS_HUE_MINUS_2 && value < IS_HUE_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_ADJUST);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_metering(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_METERING_AVERAGE:
		IS_ISP_SET_PARAM_METERING_CMD(dev,
			ISP_METERING_COMMAND_AVERAGE);
		break;
	case IS_METERING_SPOT:
		IS_ISP_SET_PARAM_METERING_CMD(dev, ISP_METERING_COMMAND_SPOT);
		break;
	case IS_METERING_MATRIX:
		IS_ISP_SET_PARAM_METERING_CMD(dev, ISP_METERING_COMMAND_MATRIX);
		break;
	case IS_METERING_CENTER:
		IS_ISP_SET_PARAM_METERING_CMD(dev, ISP_METERING_COMMAND_CENTER);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_METERING_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_METERING);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_metering_legacy(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case METERING_CENTER:
		IS_ISP_SET_PARAM_METERING_CMD(dev, ISP_METERING_COMMAND_CENTER);
		break;
	case METERING_SPOT:
		IS_ISP_SET_PARAM_METERING_CMD(dev, ISP_METERING_COMMAND_SPOT);
		break;
	case METERING_MATRIX:
		IS_ISP_SET_PARAM_METERING_CMD(dev, ISP_METERING_COMMAND_MATRIX);
		break;
	default:
		return ret;
	}
	if (value > METERING_BASE && value < METERING_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_METERING);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_afc(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_AFC_DISABLE:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_DISABLE);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, 0);
		break;
	case IS_AFC_AUTO:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_AUTO);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, 0);
		break;
	case IS_AFC_MANUAL_50HZ:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, ISP_AFC_MANUAL_50HZ);
		break;
	case IS_AFC_MANUAL_60HZ:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, ISP_AFC_MANUAL_60HZ);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_AFC_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AFC);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_isp_afc_legacy(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case ANTI_BANDING_OFF:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_DISABLE);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, 0);
		break;
	case ANTI_BANDING_AUTO:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_AUTO);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, 0);
		break;
	case ANTI_BANDING_50HZ:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, ISP_AFC_MANUAL_50HZ);
		break;
	case ANTI_BANDING_60HZ:
		IS_ISP_SET_PARAM_AFC_CMD(dev, ISP_AFC_COMMAND_MANUAL);
		IS_ISP_SET_PARAM_AFC_MANUAL(dev, ISP_AFC_MANUAL_60HZ);
		break;
	default:
		return ret;
	}
	if (value >= ANTI_BANDING_OFF && value <= ANTI_BANDING_60HZ) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AFC);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_fd_angle_mode(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_FD_ROLL_ANGLE_BASIC:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_ROLL_ANGLE);
		IS_FD_SET_PARAM_FD_CONFIG_ROLL_ANGLE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_YAW_ANGLE:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_YAW_ANGLE);
		IS_FD_SET_PARAM_FD_CONFIG_YAW_ANGLE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_SMILE_MODE:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_SMILE_MODE);
		IS_FD_SET_PARAM_FD_CONFIG_SMILE_MODE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_BLINK_MODE:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_BLINK_MODE);
		IS_FD_SET_PARAM_FD_CONFIG_BLINK_MODE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_EYE_DETECT_MODE:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_EYES_DETECT);
		IS_FD_SET_PARAM_FD_CONFIG_EYE_DETECT(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_MOUTH_DETECT_MODE:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_MOUTH_DETECT);
		IS_FD_SET_PARAM_FD_CONFIG_MOUTH_DETECT(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_ORIENTATION_MODE:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_ORIENTATION);
		IS_FD_SET_PARAM_FD_CONFIG_ORIENTATION(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case V4L2_CID_IS_FD_SET_ORIENTATION:
		IS_FD_SET_PARAM_FD_CONFIG_CMD(dev,
			FD_CONFIG_COMMAND_ORIENTATION_VALUE);
		IS_FD_SET_PARAM_FD_CONFIG_ORIENTATION_VALUE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONFIG);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	default:
		break;
	}
	return ret;
}

int fimc_is_v4l2_frame_rate(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	dbg("%s: value: %d\n", __func__, value);

	switch (value) {
	case 0: /* AUTO Mode */
		IS_SENSOR_SET_FRAME_RATE(dev, 30);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_FRAME_RATE);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
		if (!ret) {
			err("wait timeout 1 : %s\n", __func__);
			return -EINVAL;
		}
		if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
			IS_ISP_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_STOP);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
			IS_INC_PARAM_NUM(dev);
			fimc_is_mem_cache_clean((void *)dev->is_p_region,
								IS_PARAM_SIZE);
			clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
			fimc_is_hw_set_param(dev);
			ret = wait_event_timeout(dev->irq_queue,
				test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
					FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
			if (!ret) {
				err("wait timeout 2: %s\n", __func__);
				return -EINVAL;
			}
		}
		IS_ISP_SET_PARAM_OTF_INPUT_CMD(dev, OTF_INPUT_COMMAND_ENABLE);
		IS_ISP_SET_PARAM_OTF_INPUT_FRAMETIME_MIN(dev, 0);
		IS_ISP_SET_PARAM_OTF_INPUT_FRAMETIME_MAX(dev, 66666);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
							IS_PARAM_SIZE);
		clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
					FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
		if (!ret) {
			err("wait timeout 3: %s\n", __func__);
			return -EINVAL;
		}
		if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
			IS_ISP_SET_PARAM_CONTROL_CMD(dev,
						CONTROL_COMMAND_START);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
			IS_INC_PARAM_NUM(dev);
			fimc_is_mem_cache_clean((void *)dev->is_p_region,
								IS_PARAM_SIZE);
			clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
			fimc_is_hw_set_param(dev);
			ret = wait_event_timeout(dev->irq_queue,
				test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
					FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
			if (!ret) {
				err("wait timeout 4: %s\n", __func__);
				return -EINVAL;
			}
		}
		break;
	default:
		IS_SENSOR_SET_FRAME_RATE(dev, value);
		IS_SET_PARAM_BIT(dev, PARAM_SENSOR_FRAME_RATE);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
							IS_PARAM_SIZE);
		clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
			FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
		if (!ret) {
			err("wait timeout 1 : %s\n", __func__);
			return -EINVAL;
		}
#ifdef FPS_ENABLE
		if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
			IS_ISP_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_STOP);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
			IS_INC_PARAM_NUM(dev);
			fimc_is_mem_cache_clean((void *)dev->is_p_region,
								IS_PARAM_SIZE);
			clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
			fimc_is_hw_set_param(dev);
			ret = wait_event_timeout(dev->irq_queue,
				test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
					FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
			if (!ret) {
				err("wait timeout 2: %s\n", __func__);
				return -EINVAL;
			}
		}
		IS_ISP_SET_PARAM_OTF_INPUT_CMD(dev, OTF_INPUT_COMMAND_ENABLE);
		IS_ISP_SET_PARAM_OTF_INPUT_FRAMETIME_MIN(dev, 0);
		IS_ISP_SET_PARAM_OTF_INPUT_FRAMETIME_MAX(dev,
							(u32)(1000000/value));
		IS_SET_PARAM_BIT(dev, PARAM_ISP_OTF_INPUT);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
							IS_PARAM_SIZE);
		clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
		fimc_is_hw_set_param(dev);
		ret = wait_event_timeout(dev->irq_queue,
			test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
					FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
		if (!ret) {
			err("wait timeout 3: %s\n", __func__);
			return -EINVAL;
		}
		if (test_bit(FIMC_IS_STATE_HW_STREAM_ON, &dev->pipe_state)) {
			IS_ISP_SET_PARAM_CONTROL_CMD(dev,
						CONTROL_COMMAND_START);
			IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
			IS_INC_PARAM_NUM(dev);
			fimc_is_mem_cache_clean((void *)dev->is_p_region,
								IS_PARAM_SIZE);
			clear_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state);
			fimc_is_hw_set_param(dev);
			ret = wait_event_timeout(dev->irq_queue,
				test_bit(IS_ST_BLOCK_CMD_CLEARED, &dev->state),
					FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
			if (!ret) {
				err("wait timeout 4: %s\n", __func__);
				return -EINVAL;
			}
		}
#endif
	}
	return 0;
}

int fimc_is_v4l2_ae_awb_lockunlock(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case AE_UNLOCK_AWB_UNLOCK:
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AE |
						ISP_AA_TARGET_AWB);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
							IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case AE_LOCK_AWB_UNLOCK:
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_STOP);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AE);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AWB);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case AE_UNLOCK_AWB_LOCK:
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_START);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AE);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_STOP);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AWB);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	case AE_LOCK_AWB_LOCK:
		IS_ISP_SET_PARAM_AA_CMD(dev, ISP_AA_COMMAND_STOP);
		IS_ISP_SET_PARAM_AA_TARGET(dev, ISP_AA_TARGET_AE |
						ISP_AA_TARGET_AWB);
		IS_SET_PARAM_BIT(dev, PARAM_ISP_AA);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
		break;
	default:
		break;
	}
	return ret;
}

int fimc_is_v4l2_set_isp(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_ISP_BYPASS_DISABLE:
		IS_ISP_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_DISABLE);
		break;
	case IS_ISP_BYPASS_ENABLE:
		IS_ISP_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_ENABLE);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_ISP_BYPASS_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_set_drc(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_DRC_BYPASS_DISABLE:
		IS_DRC_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_DISABLE);
		IS_DRC_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_START);
		break;
	case IS_DRC_BYPASS_ENABLE:
		IS_DRC_SET_PARAM_CONTROL_BYPASS(dev, CONTROL_BYPASS_ENABLE);
		IS_DRC_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_START);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_DRC_BYPASS_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_DRC_CONTROL);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_cmd_isp(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_ISP_COMMAND_STOP:
		IS_ISP_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_STOP);
		break;
	case IS_ISP_COMMAND_START:
		IS_ISP_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_START);
		break;
	default:
		return ret;
	}
	if (value >= 0 && value < IS_ISP_COMMAND_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_ISP_CONTROL);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_cmd_drc(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_DRC_COMMAND_STOP:
		IS_DRC_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_STOP);
		break;
	case IS_DRC_COMMAND_START:
		IS_DRC_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_START);
		break;
	}
	if (value >= 0 && value < IS_ISP_COMMAND_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_DRC_CONTROL);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_cmd_fd(struct fimc_is_core *dev, int value)
{
	int ret = 0;
	switch (value) {
	case IS_FD_COMMAND_STOP:
		dbg("IS_FD_COMMAND_STOP\n");
		IS_FD_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_STOP);
		break;
	case IS_FD_COMMAND_START:
		dbg("IS_FD_COMMAND_START\n");
		IS_FD_SET_PARAM_CONTROL_CMD(dev, CONTROL_COMMAND_START);
		break;
	}
	if (value >= 0 && value < IS_ISP_COMMAND_MAX) {
		IS_SET_PARAM_BIT(dev, PARAM_FD_CONTROL);
		IS_INC_PARAM_NUM(dev);
		fimc_is_mem_cache_clean((void *)dev->is_p_region,
			IS_PARAM_SIZE);
		fimc_is_hw_set_param(dev);
	}
	return ret;
}

int fimc_is_v4l2_shot_mode(struct fimc_is_core *dev, int value)
{
	int ret = 0;

	dbg("%s\n", __func__);
	IS_SET_PARAM_GLOBAL_SHOTMODE_CMD(dev, value);
	IS_SET_PARAM_BIT(dev, PARAM_GLOBAL_SHOTMODE);
	IS_INC_PARAM_NUM(dev);
	fimc_is_mem_cache_clean((void *)dev->is_p_region, IS_PARAM_SIZE);
	fimc_is_hw_set_param(dev);
	return ret;
}
