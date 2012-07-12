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
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

#include "fimc-is-device-ischain.h"

#define SDCARD_FW
#define FIMC_IS_FW_SDCARD "/sdcard/fimc_is_fw2.bin"
/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		(1280) /* sensor margin : 16 */
#define DEFAULT_PREVIEW_STILL_HEIGHT		(720) /* sensor margin : 12 */
#define DEFAULT_CAPTURE_VIDEO_WIDTH		(1920)
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		(1080)
#define DEFAULT_CAPTURE_STILL_WIDTH		(2560)
#define DEFAULT_CAPTURE_STILL_HEIGHT		(1920)
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	(2560)
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	(1440)
#define DEFAULT_PREVIEW_VIDEO_WIDTH		(640)
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		(480)

static const struct sensor_param init_sensor_param = {
	.frame_rate = {
		.frame_rate = 30,
	},
};

static const struct isp_param init_isp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_BAYER,
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.frametime_min = 0,
		.frametime_max = 33333,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0,
		.height = 0,
		.format = 0,
		.bitwidth = 0,
		.plane = 0,
		.order = 0,
		.buffer_number = 0,
		.buffer_address = 0,
		.err = 0,
	},
	.dma2_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.aa = {
		.cmd = ISP_AA_COMMAND_START,
		.target = ISP_AA_TARGET_AF | ISP_AA_TARGET_AE |
						ISP_AA_TARGET_AWB,
		.mode = 0,
		.scene = 0,
		.sleep = 0,
		.uiAfFace = 0,
		.touch_x = 0, .touch_y = 0,
		.manual_af_setting = 0,
		.err = ISP_AF_ERROR_NO,
	},
	.flash = {
		.cmd = ISP_FLASH_COMMAND_DISABLE,
		.redeye = ISP_FLASH_REDEYE_DISABLE,
		.err = ISP_FLASH_ERROR_NO,
	},
	.awb = {
		.cmd = ISP_AWB_COMMAND_AUTO,
		.illumination = 0,
		.err = ISP_AWB_ERROR_NO,
	},
	.effect = {
		.cmd = ISP_IMAGE_EFFECT_DISABLE,
		.err = ISP_IMAGE_EFFECT_ERROR_NO,
	},
	.iso = {
		.cmd = ISP_ISO_COMMAND_AUTO,
		.value = 0,
		.err = ISP_ISO_ERROR_NO,
	},
	.adjust = {
		.cmd = ISP_ADJUST_COMMAND_AUTO,
		.contrast = 0,
		.saturation = 0,
		.sharpness = 0,
		.exposure = 0,
		.brightness = 0,
		.hue = 0,
		.err = ISP_ADJUST_ERROR_NO,
	},
	.metering = {
		.cmd = ISP_METERING_COMMAND_CENTER,
		.win_pos_x = 0, .win_pos_y = 0,
		.win_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.win_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.err = ISP_METERING_ERROR_NO,
	},
	.afc = {
		.cmd = ISP_AFC_COMMAND_AUTO,
		.manual = 0, .err = ISP_AFC_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma1_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_out_mask = 0,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = DMA_INPUT_PLANE_1,
		.order = DMA_INPUT_ORDER_YCbCr,
		.buffer_number = 0,
		.buffer_address = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.dma2_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_BAYER,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_GB_BG,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xFFFFFFFF,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct drc_param init_drc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = DMA_INPUT_PLANE_1,
		.order = DMA_INPUT_ORDER_YCbCr,
		.buffer_number = 0,
		.buffer_address = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct scalerc_param init_scalerc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_CROP_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_CROP_HEIGHT,
		.in_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.in_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.out_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.out_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_CrYCbY,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xffff,
		.reserved[0] = 2, /* unscaled*/
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct odc_param init_odc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct dis_param init_dis_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};
static const struct tdnr_param init_tdnr_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.frame = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_2,
		.order = DMA_OUTPUT_ORDER_CbCr,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xffff,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct scalerp_param init_scalerp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.crop_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.in_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.in_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.out_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.out_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.crop_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.err = 0,
	},
	.rotation = {
		.cmd = 0,
		.err = 0,
	},
	.flip = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.uiCropOffsetX = 0,
		.uiCropOffsetX = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_3,
		.order = DMA_OUTPUT_ORDER_NO,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xffff,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct fd_param init_fd_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.config = {
		.cmd = FD_CONFIG_COMMAND_MAXIMUM_NUMBER |
			FD_CONFIG_COMMAND_ROLL_ANGLE |
			FD_CONFIG_COMMAND_YAW_ANGLE |
			FD_CONFIG_COMMAND_SMILE_MODE |
			FD_CONFIG_COMMAND_BLINK_MODE |
			FD_CONFIG_COMMAND_EYES_DETECT |
			FD_CONFIG_COMMAND_MOUTH_DETECT |
			FD_CONFIG_COMMAND_ORIENTATION |
			FD_CONFIG_COMMAND_ORIENTATION_VALUE,
		.max_number = 5,
		.roll_angle = FD_CONFIG_ROLL_ANGLE_FULL,
		.yaw_angle = FD_CONFIG_YAW_ANGLE_45_90,
		.smile_mode = FD_CONFIG_SMILE_MODE_DISABLE,
		.blink_mode = FD_CONFIG_BLINK_MODE_DISABLE,
		.eye_detect = FD_CONFIG_EYES_DETECT_ENABLE,
		.mouth_detect = FD_CONFIG_MOUTH_DETECT_DISABLE,
		.orientation = FD_CONFIG_ORIENTATION_DISABLE,
		.orientation_value = 0,
		.err = ERROR_FD_NO,
	},
};


/* Allocate firmware */
static int fimc_is_ischain_allocmem(struct fimc_is_device_ischain *this)
{
	void *fimc_is_bitproc_buf;
	int ret;

	dbg_ischain("Allocating memory for FIMC-IS firmware.\n");

	fimc_is_bitproc_buf = vb2_ion_private_alloc(this->mem->alloc_ctx,
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
				SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF +
				SIZE_ISP_INTERNAL_BUF * NUM_ISP_INTERNAL_BUF);
	if (IS_ERR(fimc_is_bitproc_buf)) {
		fimc_is_bitproc_buf = 0;
		err("Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	ret = vb2_ion_dma_address(fimc_is_bitproc_buf, &this->minfo.dvaddr);
	if ((ret < 0) || (this->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.\n");
		vb2_ion_private_free(fimc_is_bitproc_buf);
		this->minfo.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg_ischain("Device vaddr = %08x , size = %08x\n",
				this->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);

	this->minfo.kvaddr = (u32)vb2_ion_private_vaddr(fimc_is_bitproc_buf);
	if (IS_ERR((void *)this->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed\n");
		vb2_ion_private_free(fimc_is_bitproc_buf);
		this->minfo.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg_ischain("Virtual address for FW: %08lx\n",
			(long unsigned int)this->minfo.kvaddr);
	this->minfo.bitproc_buf = fimc_is_bitproc_buf;
	this->minfo.fw_cookie = fimc_is_bitproc_buf;

	return 0;
}

static int fimc_is_ishcain_initmem(struct fimc_is_device_ischain *this)
{
	int ret;
	u32 offset;

	dbg_ischain("fimc_is_init_mem - ION\n");

	ret = fimc_is_ischain_allocmem(this);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		return -EINVAL;
	}

	memset((void *)this->minfo.kvaddr, 0,
		FIMC_IS_A5_MEM_SIZE +
		SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
		SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
		SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF +
		SIZE_ISP_INTERNAL_BUF * NUM_ISP_INTERNAL_BUF);

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	this->minfo.dvaddr_region = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_region = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
	this->minfo.dvaddr_odc = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_odc = this->minfo.kvaddr + offset;

	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
	this->minfo.dvaddr_dis = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_dis = this->minfo.kvaddr + offset;

	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
	this->minfo.dvaddr_3dnr = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_3dnr = this->minfo.kvaddr + offset;

	offset += (SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF);
	this->minfo.dvaddr_isp = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_isp = this->minfo.kvaddr + offset;

	this->is_region =
		(struct is_region *)this->minfo.kvaddr_region;

	this->minfo.dvaddr_shared = this->minfo.dvaddr +
		((u32)&this->is_region->shared[0] - this->minfo.kvaddr);
	this->minfo.kvaddr_shared = this->minfo.kvaddr +
		((u32)&this->is_region->shared[0] - this->minfo.kvaddr);

	dbg_ischain("fimc_is_init_mem done\n");

	return 0;
}

static void fimc_is_ischain_cache_flush(struct fimc_is_device_ischain *this,
	u32 offset, u32 size)
{
	vb2_ion_sync_for_device(this->minfo.fw_cookie,
		offset,
		size,
		DMA_BIDIRECTIONAL);
}

static void fimc_is_ischain_region_invalid(struct fimc_is_device_ischain *this)
{
	vb2_ion_sync_for_device(
		this->minfo.fw_cookie,
		(this->minfo.kvaddr_region - this->minfo.kvaddr),
		sizeof(struct is_region),
		DMA_FROM_DEVICE);
}

static void fimc_is_ischain_region_flush(struct fimc_is_device_ischain *this)
{
	vb2_ion_sync_for_device(
		this->minfo.fw_cookie,
		(this->minfo.kvaddr_region - this->minfo.kvaddr),
		sizeof(struct is_region),
		DMA_TO_DEVICE);
}

static int fimc_is_ischain_loadfirm(struct fimc_is_device_ischain *this)
{
	int ret;
	struct firmware *fw_blob;
	u8 *buf = NULL;
#ifdef SDCARD_FW
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;

	dbg_ischain("%s\n", __func__);

	ret = 0;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(FIMC_IS_FW_SDCARD, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("failed to open %s\n", FIMC_IS_FW_SDCARD);
		goto request_fw;
	}
	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;
	dbg_ischain("start, file path %s, size %ld Bytes\n",
		FIMC_IS_FW_SDCARD, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		dev_err(&this->pdev->dev,
			"failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		dev_err(&this->pdev->dev,
			"failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}
	if (this->minfo.bitproc_buf == 0) {
		err("failed to load FIMC-IS F/W, FIMC-IS will not working\n");
	} else {
		memcpy((void *)this->minfo.kvaddr, (void *)buf, fsize);
		fimc_is_ischain_cache_flush(this, 0, fsize + 1);
		dbg_ischain("FIMC_IS F/W loaded successfully - size:%d\n",
				fw_blob->size);
	}

request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif
		ret = request_firmware((const struct firmware **)&fw_blob,
			FIMC_IS_FW, &this->pdev->dev);
		if (ret) {
			dev_err(&this->pdev->dev,
				"could not load firmware (err=%d)\n", ret);
			return -EINVAL;
		}

		if (this->minfo.bitproc_buf == 0) {
			dev_err(&this->pdev->dev,
				"failed to load FIMC-IS F/W\n");
			return -EINVAL;
		} else {
		    memcpy((void *)this->minfo.kvaddr, fw_blob->data,
			fw_blob->size);
		    fimc_is_ischain_cache_flush(this, 0, fw_blob->size + 1);
		    dbg_ischain("FIMC_IS F/W loaded successfully - size:%d\n",
				fw_blob->size);
		}

#if 0
#ifndef SDCARD_FW
		/*TODO: delete or not*/
		memcpy((void *)dev->fw.fw_info,
			(fw_blob->data + fw_blob->size - 0x1F), 0x17);
		dev->fw.fw_info[24] = '\0';
		memcpy((void *)dev->fw.fw_version,
			(fw_blob->data + fw_blob->size - 0x7), 0x6);
		dev->fw.state = 1;
#endif
#endif

		dbg_ischain("FIMC_IS F/W loaded successfully - size:%d\n",
			fw_blob->size);
		release_firmware(fw_blob);
#ifdef SDCARD_FW
	}
#endif

out:
#ifdef SDCARD_FW
	if (!fw_requested) {
		vfree(buf);
		filp_close(fp, current->files);
		set_fs(old_fs);
	}
#endif

	return ret;
}

static int fimc_is_ischain_loadsetf(struct fimc_is_device_ischain *this,
	u32 load_addr)
{
	int ret;
	void *address;
	struct firmware *fw_blob;

	ret = request_firmware((const struct firmware **)&fw_blob,
				FIMC_IS_SETFILE, &this->pdev->dev);
	if (ret) {
		dev_err(&this->pdev->dev,
			"could not load firmware (err=%d)\n", ret);
		return -EINVAL;
	}

	if (this->minfo.bitproc_buf == 0) {
		err("failed to load setfile\n");
	} else {
		address = (void *)(this->minfo.kvaddr + load_addr);
		memcpy(address, fw_blob->data, fw_blob->size);
		fimc_is_ischain_cache_flush(this, load_addr, fw_blob->size + 1);
	}

	dbg_ischain("FIMC_IS setfile loaded successfully - size:%d\n",
							fw_blob->size);
	release_firmware(fw_blob);

	dbg_ischain("Setfile base  = 0x%08x\n", load_addr);

	return ret;
}

static void fimc_is_ischain_lowpower(struct fimc_is_device_ischain *this,
	bool on)
{
	if (on) {
		printk(KERN_INFO "Set low poweroff mode\n");
		__raw_writel(0x0, PMUREG_ISP_ARM_OPTION);
		__raw_writel(0x1CF82000, PMUREG_ISP_LOW_POWER_OFF);
		this->lpower = true;
	} else {
		printk(KERN_INFO "Clear low poweroff mode\n");
		__raw_writel(0xFFFFFFFF, PMUREG_ISP_ARM_OPTION);
		__raw_writel(0x8, PMUREG_ISP_LOW_POWER_OFF);
		this->lpower = false;
	}
}

int fimc_is_ischain_power_on(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 timeout;

	dbg_ischain("%s\n", __func__);

	do {
		/* 1. force poweroff setting */
		fimc_is_ischain_lowpower(this, false);

		/* 2. isp power on */
		writel(0x7, PMUREG_ISP_CONFIGURATION);
		timeout = 1000;
		while ((__raw_readl(PMUREG_ISP_STATUS) & 0x7) != 0x7) {
			if (timeout == 0)
				err("A5 power on failed1\n");
			timeout--;
			udelay(1);
		}

		/* 3. mipi power on */
		enable_mipi();

		/* 4. config gpio */
/*		if (this->pdata->cfg_gpio) {
			this->pdata->cfg_gpio(this->pdev);
		} else {
			err("failed to init GPIO config\n");
			ret = -EINVAL;
			break;
		}*/

		/* 5. config clocks */
		if (this->pdata->clk_cfg) {
			this->pdata->clk_cfg(this->pdev);
		} else {
			dev_err(&this->pdev->dev, "failed to config clock\n");
			ret = -EINVAL;
			break;
		}

	if (this->pdata->sensor_power_on) {
		this->pdata->sensor_power_on(this->pdev, 0);
	} else {
		err("failed to sensor_power_on\n");
		return -EINVAL;
	}

		if (this->pdata->clk_on) {
			this->pdata->clk_on(this->pdev, 0);
		} else {
			dev_err(&this->pdev->dev, "failed to clock on\n");
			ret = -EINVAL;
			break;
		}

		/* 6. config system mmu */
		vb2_ion_attach_iommu(this->mem->alloc_ctx);

		/* 7. A5 start address setting */
		dbg_ischain("minfo.base(dvaddr) : 0x%08x\n",
			this->minfo.dvaddr);
		dbg_ischain("minfo.base(kvaddr) : 0x%08X\n",
			this->minfo.kvaddr);
		writel(this->minfo.dvaddr, this->regs + BBOAR);

		/* 8. A5 power on*/
		writel(0x1, PMUREG_ISP_ARM_CONFIGURATION);

		/* 9. enable A5 */
		writel(0x00018000, PMUREG_ISP_ARM_OPTION);
		timeout = 1000;
		while ((__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) != 0x1) {
			if (timeout == 0)
				err("A5 power on failed2\n");
			timeout--;
			udelay(1);
		}

		set_bit(FIMC_IS_ISCHAIN_POWER_ON, &this->state);
	} while (0);

	return ret;
}

int fimc_is_ischain_power_off(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 timeout;

	dbg("%s\n", __func__);

	do {
		/* 1. config system mmu */
		vb2_ion_detach_iommu(this->mem->alloc_ctx);

		/* 2. config clocks */
		if (this->pdata->clk_off) {
			this->pdata->clk_off(this->pdev, 0);
		} else {
			dev_err(&this->pdev->dev, "failed to clock on\n");
			ret = -EINVAL;
			break;
		}

		/* 3. disable A5 */
		writel(0x0, PMUREG_ISP_ARM_OPTION);

		/* 4. A5 power off*/
		writel(0x0, PMUREG_ISP_ARM_CONFIGURATION);

		/* 5. Check A5 power off status register */
		timeout = 1000;
		while (__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) {
			if (timeout == 0)
				err("A5 power off failed\n");
			timeout--;
			udelay(1);
		}

		/* 6. ISP Power down mode (LOWPWR) */
		writel(0x0, PMUREG_CMU_RESET_ISP_SYS_PWR_REG);

		writel(0x0, PMUREG_ISP_CONFIGURATION);

		timeout = 1000;
		while ((__raw_readl(PMUREG_ISP_STATUS) & 0x7)) {
			if (timeout == 0) {
				err("ISP power off failed --> Retry\n");
				/* Retry */
				__raw_writel(0x1CF82000,
					PMUREG_ISP_LOW_POWER_OFF);
				timeout = 1000;
				while ((__raw_readl(PMUREG_ISP_STATUS) & 0x7)) {
					if (timeout == 0)
						err("ISP power off failed\n");
					timeout--;
					udelay(1);
				}
			}
			timeout--;
			udelay(1);
		}
	} while (0);

	return ret;
}

void fimc_is_ischain_power(struct fimc_is_device_ischain *this, int on)
{
	u32 timeout;
	struct device *dev = &this->pdev->dev;

	printk(KERN_INFO "%s(%d)\n", __func__, on);

	if (on) {
		/* 1. Bus lock */
#if defined(CONFIG_BUSFREQ_OPP) && defined(CONFIG_CPU_EXYNOS5250)
		dev_lock(this->bus_dev, &this->pdev->dev,
			(FIMC_IS_FREQ_MIF * 1000) + FIMC_IS_FREQ_INT);
		dbg_ischain("busfreq locked on <%d/%d>MHz\n",
			FIMC_IS_FREQ_MIF, FIMC_IS_FREQ_INT);
#endif

		/* 2. force poweroff setting */
	/*		if (this->lpower)
			fimc_is_ischain_lowpower(this, false); */

		/* 3. FIMC-IS local power enable */
		pm_runtime_get_sync(dev);
		/* printk(KERN_INFO "PMUREG_ISP_STATUS = 0x%08x\n",
					__raw_readl(PMUREG_ISP_STATUS)); */

	if (this->pdata->clk_on) {
		this->pdata->clk_on(this->pdev, 0);
	} else {
		err("failed to clock on\n");
		return;
	}

		/* 4. config system mmu */
		vb2_ion_attach_iommu(this->mem->alloc_ctx);

		/* 5. A5 start address setting */
		dbg_ischain("minfo.base(dvaddr) : 0x%08x\n",
			this->minfo.dvaddr);
		dbg_ischain("minfo.base(kvaddr) : 0x%08X\n",
			this->minfo.kvaddr);
		writel(this->minfo.dvaddr, this->regs + BBOAR);

		/* 6. A5 power on*/
		writel(0x1, PMUREG_ISP_ARM_CONFIGURATION);

		/* 7. enable A5 */
		writel(0x00018000, PMUREG_ISP_ARM_OPTION);
		timeout = 1000;
		while ((__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) != 0x1) {
			if (timeout == 0)
				err("A5 power on failed\n");
			timeout--;
			udelay(1);
		}

		set_bit(FIMC_IS_ISCHAIN_POWER_ON, &this->state);

	/* HACK */
	fimc_is_runtime_resume(dev);
	} else {
		/* 1. A5 power down */
		/* 1). disable A5 */
#if 0
		if (this->lpower)
			writel(0x0, PMUREG_ISP_ARM_OPTION);
		else
			writel(0x10000, PMUREG_ISP_ARM_OPTION);
#endif
		/* 2). A5 power off*/
		writel(0x0, PMUREG_ISP_ARM_CONFIGURATION);

		/* 3). Check A5 power off status register */
		timeout = 1000;
		while (__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) {
			if (timeout == 0) {
				printk(KERN_ERR "A5 power off failed\n");
				fimc_is_ischain_lowpower(this, true);
			}
			timeout--;
			udelay(1);
		}

		writel(0x0, PMUREG_CMU_RESET_ISP_SYS_PWR_REG);

		/* 2. FIMC-IS local power down */
		pm_runtime_put_sync(dev);
		printk(KERN_INFO "PMUREG_ISP_STATUS = 0x%08x\n",
					__raw_readl(PMUREG_ISP_STATUS));
#if defined(CONFIG_BUSFREQ_OPP) && defined(CONFIG_CPU_EXYNOS5250)
		dev_unlock(this->bus_dev, &this->pdev->dev);
		printk(KERN_INFO "busfreq locked off\n");
#endif
	}
#if 0
	if (on) {
#if defined(CONFIG_BUSFREQ_OPP) && defined(CONFIG_CPU_EXYNOS5250)
		mutex_lock(&isp->busfreq_lock);
		isp->busfreq_num++;
		if (isp->busfreq_num == 1) {
			dev_lock(isp->bus_dev, &isp->pdev->dev,
				(FIMC_IS_FREQ_MIF * 1000) + FIMC_IS_FREQ_INT);
			dbg("busfreq locked on <%d/%d>MHz\n",
				FIMC_IS_FREQ_MIF, FIMC_IS_FREQ_INT);
		}
		mutex_unlock(&isp->busfreq_lock);
#endif
		fimc_is_hw_a5_power_on(isp);
	} else {
		fimc_is_hw_a5_power_off(isp);
#if defined(CONFIG_BUSFREQ_OPP) && defined(CONFIG_CPU_EXYNOS5250)
		mutex_lock(&isp->busfreq_lock);
		if (isp->busfreq_num == 1) {
			dev_unlock(isp->bus_dev, &isp->pdev->dev);
			printk(KERN_DEBUG "busfreq locked off\n");
		}
		isp->busfreq_num--;
		if (isp->busfreq_num < 0)
			isp->busfreq_num = 0;
		mutex_unlock(&isp->busfreq_lock);
#endif
	}
#endif
}

static int fimc_is_itf_s_param(struct fimc_is_device_ischain *this,
	u32 indexes, u32 lindex, u32 hindex)
{
	int ret = 0;

	fimc_is_ischain_region_flush(this);

	ret = fimc_is_hw_s_param(this->interface,
		this->instance,
		indexes,
		lindex,
		hindex);

	return ret;
}

static int fimc_is_itf_a_param(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	ret = fimc_is_hw_a_param(this->interface, this->instance);

	return ret;
}

static int fimc_is_itf_f_param(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	/* u32 navailable = 0; */

	/* struct is_region *region = this->is_region; */

	dbg_ischain(" NAME    ON  BYPASS       SIZE  FORMAT\n");
	dbg_ischain("ISP OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.isp.control.cmd,
		region->parameter.isp.control.bypass,
		region->parameter.isp.otf_output.width,
		region->parameter.isp.otf_output.height,
		region->parameter.isp.otf_output.format
		);
	dbg_ischain("DRC OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass,
		region->parameter.drc.otf_input.width,
		region->parameter.drc.otf_input.height,
		region->parameter.drc.otf_input.format
		);
	dbg_ischain("DRC OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass,
		region->parameter.drc.otf_output.width,
		region->parameter.drc.otf_output.height,
		region->parameter.drc.otf_output.format
		);
	dbg_ischain("SCC OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.otf_input.width,
		region->parameter.scalerc.otf_input.height,
		region->parameter.scalerc.otf_input.format
		);
	dbg_ischain("SCC OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.otf_output.width,
		region->parameter.scalerc.otf_output.height,
		region->parameter.scalerc.otf_output.format
		);
	dbg_ischain("ODC OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.odc.control.cmd,
		region->parameter.odc.control.bypass,
		region->parameter.odc.otf_input.width,
		region->parameter.odc.otf_input.height,
		region->parameter.odc.otf_input.format
		);
	dbg_ischain("ODC OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.odc.control.cmd,
		region->parameter.odc.control.bypass,
		region->parameter.odc.otf_output.width,
		region->parameter.odc.otf_output.height,
		region->parameter.odc.otf_output.format
		);
	dbg_ischain("DIS OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass,
		region->parameter.dis.otf_input.width,
		region->parameter.dis.otf_input.height,
		region->parameter.dis.otf_input.format
		);
	dbg_ischain("DIS OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass,
		region->parameter.dis.otf_output.width,
		region->parameter.dis.otf_output.height,
		region->parameter.dis.otf_output.format
		);
	dbg_ischain("DNR OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.tdnr.control.cmd,
		region->parameter.tdnr.control.bypass,
		region->parameter.tdnr.otf_input.width,
		region->parameter.tdnr.otf_input.height,
		region->parameter.tdnr.otf_input.format
		);
	dbg_ischain("DNR OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.tdnr.control.cmd,
		region->parameter.tdnr.control.bypass,
		region->parameter.tdnr.otf_output.width,
		region->parameter.tdnr.otf_output.height,
		region->parameter.tdnr.otf_output.format
		);
	dbg_ischain("SCP OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.otf_input.width,
		region->parameter.scalerp.otf_input.height,
		region->parameter.scalerp.otf_input.format
		);
	dbg_ischain("SCP DO : %2d    %4d  %04dx%04d    %d,%d\n",
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.dma_output.width,
		region->parameter.scalerp.dma_output.height,
		region->parameter.scalerp.dma_output.format,
		region->parameter.scalerp.dma_output.plane
		);
	dbg_ischain("SCP OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.otf_output.width,
		region->parameter.scalerp.otf_output.height,
		region->parameter.scalerp.otf_output.format
		);
	dbg_ischain(" NAME   CMD    IN_SZIE   OT_SIZE      CROP       POS\n");
	dbg_ischain("SCC CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
		region->parameter.scalerc.input_crop.cmd,
		region->parameter.scalerc.input_crop.in_width,
		region->parameter.scalerc.input_crop.in_height,
		region->parameter.scalerc.input_crop.out_width,
		region->parameter.scalerc.input_crop.out_height,
		region->parameter.scalerc.input_crop.crop_width,
		region->parameter.scalerc.input_crop.crop_height,
		region->parameter.scalerc.input_crop.pos_x,
		region->parameter.scalerc.input_crop.pos_y
		);
	dbg_ischain("SCC CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
		region->parameter.scalerc.output_crop.cmd,
		navailable,
		navailable,
		navailable,
		navailable,
		region->parameter.scalerc.output_crop.crop_width,
		region->parameter.scalerc.output_crop.crop_height,
		region->parameter.scalerc.output_crop.pos_x,
		region->parameter.scalerc.output_crop.pos_y
		);
	dbg_ischain("SCP CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
		region->parameter.scalerp.input_crop.cmd,
		region->parameter.scalerp.input_crop.in_width,
		region->parameter.scalerp.input_crop.in_height,
		region->parameter.scalerp.input_crop.out_width,
		region->parameter.scalerp.input_crop.out_height,
		region->parameter.scalerp.input_crop.crop_width,
		region->parameter.scalerp.input_crop.crop_height,
		region->parameter.scalerp.input_crop.pos_x,
		region->parameter.scalerp.input_crop.pos_y
		);
	dbg_ischain("SCP CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
		region->parameter.scalerp.output_crop.cmd,
		navailable,
		navailable,
		navailable,
		navailable,
		region->parameter.scalerp.output_crop.crop_width,
		region->parameter.scalerp.output_crop.crop_height,
		region->parameter.scalerp.output_crop.pos_x,
		region->parameter.scalerp.output_crop.pos_y
		);
	dbg_ischain(" NAME   BUFS		MASK\n");
	dbg_ischain("SCP DO : %2d    %04X\n",
		region->parameter.scalerp.dma_output.buffer_number,
		region->parameter.scalerp.dma_output.dma_out_mask
		);

	ret = fimc_is_hw_f_param(this->interface, this->instance);

	return ret;
}

static int fimc_is_itf_open(struct fimc_is_device_ischain *this,
	u32 input, u32 channel)
{
	int ret = 0;
	struct is_region *region = this->is_region;

	ret = fimc_is_hw_open(this->interface, this->instance, input, channel);

	if (region->shared[MAX_SHARED_COUNT-1] != MAGIC_NUMBER) {
		err("MAGIC NUMBER error\n");
		ret = 1;
		goto exit;
	}

	memset(&region->parameter, 0x0, sizeof(struct is_param_region));

	memcpy(&region->parameter.sensor, &init_sensor_param,
		sizeof(struct sensor_param));
	memcpy(&region->parameter.isp, &init_isp_param,
		sizeof(struct isp_param));
	memcpy(&region->parameter.drc, &init_drc_param,
		sizeof(struct drc_param));
	memcpy(&region->parameter.scalerc, &init_scalerc_param,
		sizeof(struct scalerc_param));
	memcpy(&region->parameter.odc, &init_odc_param,
		sizeof(struct odc_param));
	memcpy(&region->parameter.dis, &init_dis_param,
		sizeof(struct dis_param));
	memcpy(&region->parameter.tdnr, &init_tdnr_param,
		sizeof(struct tdnr_param));
	memcpy(&region->parameter.scalerp, &init_scalerp_param,
		sizeof(struct scalerp_param));
	memcpy(&region->parameter.fd, &init_fd_param,
		sizeof(struct fd_param));

exit:
	return ret;
}

static int fimc_is_itf_setfile(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 setfile_addr;

	ret = fimc_is_hw_saddr(this->interface, this->instance, &setfile_addr);
	fimc_is_ischain_loadsetf(this, setfile_addr);
	fimc_is_hw_setfile(this->interface, this->instance);

	return ret;
}

static int fimc_is_itf_s_camctrl(struct fimc_is_device_ischain *this,
	u32 address, u32 fnumber)
{
	int ret = 0;

	fimc_is_ischain_region_flush(this);

	ret = fimc_is_hw_s_camctrl_nblk(this->interface,
		this->instance,
		address,
		fnumber);

	return ret;
}

int fimc_is_itf_stream_on(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	ret = fimc_is_hw_stream_on(this->interface, this->instance);

	return ret;
}

int fimc_is_itf_stream_off(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	ret = fimc_is_hw_stream_off(this->interface, this->instance);

	return ret;
}

int fimc_is_itf_process_on(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	ret = fimc_is_hw_process_on(this->interface, this->instance);

	return ret;
}

int fimc_is_itf_process_off(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	ret = fimc_is_hw_process_off(this->interface, this->instance);

	return ret;
}

int fimc_is_itf_stream_ctl(struct fimc_is_device_ischain *this,
	u32 fcount,
	u64 *rfduration,
	u64 *rexposure,
	u32 *rsensitivity)
{
	int ret = 0;
	u32 updated = 0;
	struct fimc_is_interface *itf;
	struct camera2_uctl *frame_desc;
	struct camera2_uctl *req_desc;

	u64 fduration;
	u64 exposure;
	u32 sensitivity;

#ifdef DBG_STREAMING
	dbg_ischain("%s\n", __func__);
#endif

	itf = this->interface;
	frame_desc = &this->frame_desc;
	req_desc = &this->req_frame_desc;

	if (req_desc->sensor.frameDuration)
		fduration = req_desc->sensor.frameDuration;
	else
		fduration = itf->curr_fduration;

	if (frame_desc->sensor.frameDuration != fduration) {
		frame_desc->sensor.frameDuration = fduration;
		*rfduration = fduration;
		/*dbg_ischain("fduration : %d\n", (u32)duration);*/
		updated |= (1<<1);
	}

	if (req_desc->sensor.exposureTime)
		exposure = req_desc->sensor.exposureTime;
	else
		exposure = itf->curr_exposure;

	if (frame_desc->sensor.exposureTime != exposure) {
		frame_desc->sensor.exposureTime = exposure;
		*rexposure = exposure;
		/*dbg_ischain("exposure : %d\n", (u32)exposure);*/
		updated |= (1<<1);
	}

	if (req_desc->sensor.sensitivity)
		sensitivity = req_desc->sensor.sensitivity;
	else
		sensitivity = itf->curr_sensitivity;

	if (frame_desc->sensor.sensitivity != sensitivity) {
		frame_desc->sensor.sensitivity = sensitivity;
		*rsensitivity = sensitivity;
		/*dbg_ischain("sensitivity : %d\n", (u32)exposure);*/
		updated |= (1<<1);
	}

	if (updated) {
		frame_desc->uUpdateBitMap = updated;
		memcpy(this->is_region->shared,
			frame_desc, sizeof(struct camera2_uctl));
		fimc_is_itf_s_camctrl(this,
			this->minfo.dvaddr_shared,
			fcount);
	}

	return ret;
}

int fimc_is_itf_g_capability(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 metadata;
	u32 index;
	struct camera2_sm *capability;

	ret = fimc_is_hw_g_capability(this->interface, this->instance,
		(this->minfo.kvaddr_shared - this->minfo.kvaddr));

	fimc_is_ischain_region_invalid(this);

	memcpy(&this->capability, &this->is_region->shared,
		sizeof(struct camera2_sm));
	capability = &this->capability;

#if 1
	dbg_ischain("===ColorC================================\n");
	index = 0;
	metadata = capability->color.availableModes[index];
	while (metadata) {
		dbg_ischain("availableModes : %d\n", metadata);
		index++;
		metadata = capability->color.availableModes[index];
	}

	dbg_ischain("===ToneMapping===========================\n");
	metadata = capability->tonemap.maxCurvePoints;
	dbg_ischain("maxCurvePoints : %d\n", metadata);

	dbg_ischain("===Scaler================================\n");
	dbg_ischain("foramt : %d, %d, %d, %d\n",
		capability->scaler.availableFormats[0],
		capability->scaler.availableFormats[1],
		capability->scaler.availableFormats[2],
		capability->scaler.availableFormats[3]);

	dbg_ischain("===StatisTicsG===========================\n");
	index = 0;
	metadata = capability->statistics.availableFaceDetectModes[index];
	while (metadata) {
		dbg_ischain("availableFaceDetectModes : %d\n", metadata);
		index++;
		metadata = capability->statistics.
			availableFaceDetectModes[index];
	}
	dbg_ischain("maxFaceCount : %d\n",
		capability->statistics.maxFaceCount);
	dbg_ischain("histogrambucketCount : %d\n",
		capability->statistics.histogramBucketCount);
	dbg_ischain("maxHistogramCount : %d\n",
		capability->statistics.maxHistogramCount);
	dbg_ischain("sharpnessMapSize : %dx%d\n",
		capability->statistics.sharpnessMapSize[0],
		capability->statistics.sharpnessMapSize[1]);
	dbg_ischain("maxSharpnessMapValue : %d\n",
		capability->statistics.maxSharpnessMapValue);

	dbg_ischain("===3A====================================\n");
	index = 0;
	metadata = capability->aa.availableModes[index];
	while (metadata) {
		dbg_ischain("availableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.availableModes[index];
	}
	dbg_ischain("maxRegions : %d\n",
		capability->aa.maxRegions);
	index = 0;
	metadata = capability->aa.aeAvailableModes[index];
	while (metadata) {
		dbg_ischain("aeAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.aeAvailableModes[index];
	}
	dbg_ischain("aeCompensationStep : %d,%d\n",
		capability->aa.aeCompensationStep.num,
		capability->aa.aeCompensationStep.den);
	dbg_ischain("aeCompensationRange : %d ~ %d\n",
		capability->aa.aeCompensationRange[0],
		capability->aa.aeCompensationRange[1]);
	index = 0;
	metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	while (metadata) {
		dbg_ischain("TargetFpsRanges : %d ~ %d\n", metadata,
			capability->aa.aeAvailableTargetFpsRanges[index][1]);
		index++;
		metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	}
	index = 0;
	metadata = capability->aa.aeAvailableAntibandingModes[index];
	while (metadata) {
		dbg_ischain("aeAvailableAntibandingModes : %d\n", metadata);
		index++;
		metadata = capability->aa.aeAvailableAntibandingModes[index];
	}
	index = 0;
	metadata = capability->aa.awbAvailableModes[index];
	while (metadata) {
		dbg_ischain("awbAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.awbAvailableModes[index];
	}
	index = 0;
	metadata = capability->aa.afAvailableModes[index];
	while (metadata) {
		dbg_ischain("afAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.afAvailableModes[index];
	}
#endif

	return ret;
}

static int fimc_is_itf_power_down(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	ret = fimc_is_hw_power_down(this->interface, this->instance);

	return ret;
}

static int fimc_is_itf_shot(struct fimc_is_device_ischain *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

#if 0
	dbg_ischain("magic number : 0x%X\n", item->shot->magicNumber);
	dbg_ischain("crop %d, %d, %d\n",
		item->shot->ctl.scaler.cropRegion[0],
		item->shot->ctl.scaler.cropRegion[1],
		item->shot->ctl.scaler.cropRegion[2]);
	dbg_ischain("request : SCC(%d), SCP(%d)\n",
		item->shot_ext->request_scc, item->shot_ext->request_scp);
#endif

	if (item->shot->magicNumber != 0x23456789) {
		err("shot magic number error(0x%08X)\n",
			item->shot->magicNumber);
		ret = 1;
		goto exit;
	}

	ret = fimc_is_hw_shot_nblk(this->interface, this->instance,
		item->dvaddr_buffer,
		item->dvaddr_shot,
		item->shot->dm.sensor.frameCount);

exit:
	return ret;
}

int fimc_is_ischain_probe(struct fimc_is_device_ischain *this,
	struct fimc_is_interface *interface,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_mem *mem,
	struct platform_device *pdev,
	u32 regs)
{
#if defined(CONFIG_BUSFREQ_OPP) && defined(CONFIG_CPU_EXYNOS5250)
	this->bus_dev		= dev_get("exynos-busfreq");
#endif

	/*this->private_data = (u32)private_data;*/
	this->lpower		= false;
	this->interface		= interface;
	this->framemgr		= framemgr;
	this->mem		= mem;
	this->pdev		= pdev;
	this->pdata		= pdev->dev.platform_data;
	this->regs		= (void *)regs;

	this->chain0_width	= 2560;
	this->chain0_height	= 1920;
	this->chain1_width	= 1920;
	this->chain1_height	= 1080;
	this->chain2_width	= 1920;
	this->chain2_height	= 1080;
	this->chain3_width	= 1920;
	this->chain3_height	= 1080;

	/*hack*/
#if 1
	fimc_is_ishcain_initmem(this);
#endif

	clear_bit(FIMC_IS_ISCHAIN_LOADED, &this->state);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &this->state);
	clear_bit(FIMC_IS_ISCHAIN_RUN, &this->state);

	return 0;
}

int fimc_is_ischain_open(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	dbg_ischain("%s\n", __func__);

	do {
		mutex_init(&this->mutex_state);
		spin_lock_init(&this->slock_state);
		spin_lock_init(&this->scc.slock_state);
		spin_lock_init(&this->scp.slock_state);

		fimc_is_frame_open(this->framemgr, 8);
#if 0
		/* 1. init memory */
		fimc_is_ishcain_initmem(this);
#endif

		/* 2. Load IS firmware */
		ret = fimc_is_ischain_loadfirm(this);
		if (ret) {
			err("failed to fimc_is_request_firmware (%d)\n", ret);
			ret = -EINVAL;
			break;
		}

		set_bit(FIMC_IS_ISCHAIN_LOADED, &this->state);

		/* 3. A5 power on */
		fimc_is_ischain_power(this, 1);

		set_bit(FIMC_IS_ISCHAIN_POWER_ON, &this->state);
		dbg_ischain("fimc_is_load_fw end\n");

		/* bts_set_priority(&this->pdev->dev, 1); */

		memset(&this->frame_desc, 0,
			sizeof(struct camera2_uctl));
		memset(&this->capability, 0,
			sizeof(struct camera2_sm));

		this->scc.state = FIMC_IS_ISDEV_STOP;
		this->scp.state = FIMC_IS_ISDEV_STOP;
		clear_bit(FIMC_IS_ISCHAIN_RUN, &this->state);

		/*fimc_is_fw_clear_irq1_all(core);*/
	} while (0);

	return ret;
}

int fimc_is_ischain_close(struct fimc_is_device_ischain *this)
{
	int ret = 0;

	dbg_ischain("%s\n", __func__);

	ret = fimc_is_itf_power_down(this);
	if (ret) {
		err("power down is failed\n");
		fimc_is_ischain_lowpower(this, true);
	}

	fimc_is_ischain_power(this, 0);
	fimc_is_frame_close(this->framemgr);
	fimc_is_interface_close(this->interface);

	return ret;
}

int fimc_is_ischain_init(struct fimc_is_device_ischain *this,
	u32 input, u32 channel)
{
	int ret = 0;

	dbg_ischain("%s(input : %d, channel : %d)\n",
		__func__, input, channel);

	ret = fimc_is_itf_open(this, input, channel);
	if (ret) {
		err("open fail ret(%d)\n", ret);
		goto exit;
	}

	ret = fimc_is_itf_setfile(this);
	if (ret) {
		err("setfile fail\n");
		goto exit;
	}

	ret = fimc_is_itf_stream_off(this);
	if (ret) {
		err("streamoff fail\n");
		goto exit;
	}

	ret = fimc_is_itf_process_off(this);
	if (ret) {
		err("processoff fail\n");
		goto exit;
	}

exit:
	return ret;
}

int fimc_is_ischain_isp_start(struct fimc_is_device_ischain *this,
	struct fimc_is_video_common *video)
{
	int ret = 0;
	struct isp_param *isp_param;
	struct odc_param *odc_param;
	struct sensor_param *sensor_param;
	struct fimc_is_framemgr *framemgr;
	u32 width, height;
	u32 lindex, hindex, indexes;
	u32 i;

	dbg_front("%s\n", __func__);

	width = this->chain0_width;
	height = this->chain0_height;

	indexes = 0;
	lindex = hindex = 0;

	framemgr = this->framemgr;
	sensor_param = &this->is_region->parameter.sensor;
	isp_param = &this->is_region->parameter.isp;
	odc_param = &this->is_region->parameter.odc;

	for (i = 0; i < video->buffers; i++) {
		fimc_is_hw_cfg_mem(this->interface, 0,
			video->buf_dva[i][1], video->frame.size[1]);
	}

	sensor_param->frame_rate.frame_rate = 1;

	lindex |= LOWBIT_OF(PARAM_SENSOR_FRAME_RATE);
	hindex |= HIGHBIT_OF(PARAM_SENSOR_FRAME_RATE);
	indexes++;

	isp_param->control.cmd = CONTROL_COMMAND_START;
	isp_param->control.bypass = CONTROL_BYPASS_DISABLE;
	/*isp->is_p_region->parameter.isp.control.run_mode = 0;*/
	isp_param->control.run_mode = 1;

	lindex |= LOWBIT_OF(PARAM_ISP_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_ISP_CONTROL);
	indexes++;

	isp_param->otf_input.cmd = OTF_INPUT_COMMAND_DISABLE;
	isp_param->otf_input.width = width;
	isp_param->otf_input.height = height;
	isp_param->otf_input.format = OTF_INPUT_FORMAT_BAYER_DMA;
	isp_param->otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
	isp_param->otf_input.order = OTF_INPUT_ORDER_BAYER_GR_BG;
	isp_param->otf_input.frametime_max = 33333;

	lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_OTF_INPUT);
	indexes++;

	isp_param->dma1_input.cmd = DMA_INPUT_COMMAND_BUF_MNGR;
	isp_param->dma1_input.width = width;
	isp_param->dma1_input.height = height;

	isp_param->dma1_input.uiCropOffsetX = 0;
	isp_param->dma1_input.uiCropOffsetY = 0;
	isp_param->dma1_input.uiCropWidth = 0;
	isp_param->dma1_input.uiCropHeight = 0;
	isp_param->dma1_input.uiUserMinFrameTime = 0;
	isp_param->dma1_input.uiUserMaxFrameTime = 66666;
	isp_param->dma1_input.uiWideFrameGap = 1;
	isp_param->dma1_input.uiFrameGap = 4096;
	isp_param->dma1_input.uiLineGap = 45;

	isp_param->dma1_input.bitwidth = DMA_INPUT_BIT_WIDTH_10BIT;
	isp_param->dma1_input.order = DMA_INPUT_ORDER_GR_BG;
	isp_param->dma1_input.plane = 1;
	isp_param->dma1_input.buffer_number = 1;
	isp_param->dma1_input.buffer_address = 0;

	lindex |= LOWBIT_OF(PARAM_ISP_DMA1_INPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_DMA1_INPUT);
	indexes++;

	isp_param->dma1_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	lindex |= LOWBIT_OF(PARAM_ISP_DMA1_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_DMA1_OUTPUT);
	indexes++;

	isp_param->dma2_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	lindex |= LOWBIT_OF(PARAM_ISP_DMA2_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_DMA2_OUTPUT);
	indexes++;

	odc_param->control.cmd = CONTROL_COMMAND_START;
	odc_param->control.bypass = CONTROL_BYPASS_ENABLE;

	lindex |= LOWBIT_OF(PARAM_ODC_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_ODC_CONTROL);
	indexes++;

	lindex = 0xFFFFFFFF;
	hindex = 0xFFFFFFFF;

	fimc_is_itf_s_param(this , indexes, lindex, hindex);

	fimc_is_ischain_s_chain0(this,
		this->chain0_width, this->chain0_height);
	fimc_is_ischain_s_chain1(this,
		this->chain1_width, this->chain1_height);
	fimc_is_ischain_s_chain2(this,
		this->chain2_width, this->chain2_height);
	fimc_is_ischain_s_chain3(this,
		this->chain3_width, this->chain3_height);

	fimc_is_itf_f_param(this);

	ret = fimc_is_itf_g_capability(this);
	if (ret) {
		err("capability fail\n");
		goto exit;
	}

exit:
	return ret;
}

int fimc_is_ischain_scc_start(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 planes, i, j, buf_index;
	u32 indexes, lindex, hindex;
	struct scalerc_param *scc_param;
	struct fimc_is_ischain_dev *scc;
	struct fimc_is_video_common *video;

	dbg_ischain("%s\n", __func__);

	scc = &this->scc;
	video = &this->scc_video->common;

	planes = video->frame.format.num_planes;
	for (i = 0; i < video->buffers; i++) {
		for (j = 0; j < planes; j++) {
			buf_index = i*planes + j;

			/*dbg_ischain("(%d)set buf(%d:%d) = 0x%08x\n",
				buf_index, i, j, video->buf_dva[i][j]);*/

			this->is_region->shared[447+buf_index] =
				video->buf_dva[i][j];
		}
	}

	dbg_ischain("buf_num:%d buf_plane:%d shared[447] : 0x%X\n",
		video->buffers,
		video->frame.format.num_planes,
		this->minfo.kvaddr_shared + 447 * sizeof(u32));

	video->buf_mask = 0;
	for (i = 0; i < video->buffers; i++)
		video->buf_mask |= (1 << i);

	indexes = 0;
	lindex = hindex = 0;

	scc_param = &this->is_region->parameter.scalerc;
	scc_param->dma_output.cmd = DMA_OUTPUT_COMMAND_ENABLE;
	scc_param->dma_output.dma_out_mask = video->buf_mask;
	scc_param->dma_output.buffer_number = video->buffers;
	scc_param->dma_output.buffer_address =
		this->minfo.dvaddr_shared + 447*sizeof(u32);

	lindex |= LOWBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	indexes++;

	ret = fimc_is_itf_s_param(this, indexes, lindex, hindex);
	if (!ret)
		scc->state = FIMC_IS_ISDEV_START;
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

	spin_lock(&scc->slock_state);
	scc->state = FIMC_IS_ISDEV_START;
	spin_unlock(&scc->slock_state);

	return ret;
}

int fimc_is_ischain_scc_stop(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 indexes, lindex, hindex;
	struct scalerc_param *scc_param;
	struct fimc_is_ischain_dev *scc;

	dbg_ischain("%s\n", __func__);

	indexes = 0;
	lindex = hindex = 0;
	scc = &this->scc;

	scc_param = &this->is_region->parameter.scalerc;
	scc_param->dma_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	lindex |= LOWBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	indexes++;

	ret = fimc_is_itf_s_param(this, indexes, lindex, hindex);
	if (!ret)
		scc->state = FIMC_IS_ISDEV_STOP;
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

	spin_lock(&scc->slock_state);
	scc->state = FIMC_IS_ISDEV_STOP;
	spin_unlock(&scc->slock_state);

	return ret;
}

int fimc_is_ischain_scp_start(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 planes, i, j, buf_index;
	u32 indexes, lindex, hindex;
	struct scalerp_param *scp_param;
	struct fimc_is_ischain_dev *scp;
	struct fimc_is_video_common *video;

	dbg_ischain("%s\n", __func__);

	scp = &this->scp;
	video = &this->scp_video->common;

	planes = video->frame.format.num_planes;
	for (i = 0; i < video->buffers; i++) {
		for (j = 0; j < planes; j++) {
			buf_index = i*planes + j;

			/*dbg_ischain("(%d)set buf(%d:%d) = 0x%08x\n",
				buf_index, i, j, video->buf_dva[i][j]);*/

			this->is_region->shared[400+buf_index] =
				video->buf_dva[i][j];
		}
	}

	dbg_ischain("buf_num:%d buf_plane:%d shared[400] : 0x%X\n",
		video->buffers,
		video->frame.format.num_planes,
		this->minfo.kvaddr_shared + 400 * sizeof(u32));

	video->buf_mask = 0;
	for (i = 0; i < video->buffers; i++)
		video->buf_mask |= (1 << i);

	indexes = 0;
	lindex = hindex = 0;

	scp_param = &this->is_region->parameter.scalerp;
	scp_param->dma_output.cmd = DMA_OUTPUT_COMMAND_ENABLE;
	scp_param->dma_output.dma_out_mask = video->buf_mask;
	scp_param->dma_output.buffer_number = video->buffers;
	scp_param->dma_output.buffer_address =
		this->minfo.dvaddr_shared + 400*sizeof(u32);

	lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	indexes++;

	ret = fimc_is_itf_s_param(this, indexes, lindex, hindex);
	if (!ret)
		scp->state = FIMC_IS_ISDEV_START;
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

	spin_lock(&scp->slock_state);
	scp->state = FIMC_IS_ISDEV_START;
	spin_unlock(&scp->slock_state);

	return ret;
}

int fimc_is_ischain_scp_stop(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	u32 indexes, lindex, hindex;
	struct scalerp_param *scp_param;
	struct fimc_is_ischain_dev *scp;

	dbg_ischain("%s\n", __func__);

	indexes = 0;
	lindex = hindex = 0;
	scp = &this->scp;

	scp_param = &this->is_region->parameter.scalerp;
	scp_param->dma_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	indexes++;

	ret = fimc_is_itf_s_param(this, indexes, lindex, hindex);
	if (!ret)
		scp->state = FIMC_IS_ISDEV_STOP;
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

	spin_lock(&scp->slock_state);
	scp->state = FIMC_IS_ISDEV_STOP;
	spin_unlock(&scp->slock_state);

	return ret;
}

int fimc_is_ischain_buffer_queue(struct fimc_is_device_ischain *this,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame_shot *item;
	struct fimc_is_framemgr *framemgr;

	framemgr = this->framemgr;

#ifdef DBG_STREAMING
	dbg_ischain("%s\n", __func__);
#endif

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	spin_lock_irqsave(&framemgr->slock, flags);

	item = &framemgr->shot[index];
	if (item->state == FIMC_IS_SHOT_STATE_FREE) {
		item->request_flag = 0;
		set_bit(FIMC_IS_REQ_MDT, &item->request_flag);
		if (item->shot_ext->request_scc)
			set_bit(FIMC_IS_REQ_SCC, &item->request_flag);
		if (item->shot_ext->request_scp)
			set_bit(FIMC_IS_REQ_SCP, &item->request_flag);

		item->fnumber = item->shot->dm.sensor.frameCount;

		fimc_is_frame_trans_free_to_req(framemgr, item);
	} else {
		err("item(%d) is not free state(%d)\n", index, item->state);
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	spin_unlock_irqrestore(&framemgr->slock, flags);

	mutex_lock(&this->mutex_state);
	if (!test_bit(FIMC_IS_ISCHAIN_RUN, &this->state))
		fimc_is_ischain_callback(this);
	mutex_unlock(&this->mutex_state);

exit:
	return ret;
}

int fimc_is_ischain_buffer_finish(struct fimc_is_device_ischain *this,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame_shot *item;
	struct fimc_is_framemgr *framemgr = this->framemgr;

#ifdef DBG_STREAMING
	/*dbg_ischain("%s\n", __func__);*/
#endif

	spin_lock_irqsave(&framemgr->slock, flags);

	fimc_is_frame_complete_head(framemgr, &item);
	if (item) {
		if (index == item->id)
			fimc_is_frame_trans_com_to_free(framemgr, item);
		else {
			dbg_warning("buffer index is NOT matched(%d != %d)\n",
				index, item->id);
			fimc_is_frame_print_free_list(framemgr);
			fimc_is_frame_print_request_list(framemgr);
			fimc_is_frame_print_complete_list(framemgr);
		}
	} else {
		err("item is empty from free\n");
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	spin_unlock_irqrestore(&framemgr->slock, flags);

	return ret;
}

int fimc_is_ischain_s_chain0(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height)
{
	return 0;
}

int fimc_is_ischain_s_chain1(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height)
{
	struct scalerc_param *scc_param;
	struct odc_param *odc_param;
	struct dis_param *dis_param;

	u32 chain0_width, chain0_height, chain0_ratio;
	u32 chain1_width, chain1_height, chain1_ratio;
	u32 scc_crop_width, scc_crop_height;
	u32 scc_crop_x, scc_crop_y;
	u32 indexes, lindex, hindex;

	dbg_ischain("chain1 request size : %dx%d\n", width, height);

	scc_param = &this->is_region->parameter.scalerc;
	odc_param = &this->is_region->parameter.odc;
	dis_param = &this->is_region->parameter.dis;
	indexes = lindex = hindex = 0;

	/* CALCULATION */
	chain0_width = this->chain0_width;
	chain0_height = this->chain0_height;

	chain1_width = width;
	chain1_height = height;

	scc_crop_width = 0;
	scc_crop_height = 0;

	chain0_ratio = chain0_width * 1000 / chain0_height;
	chain1_ratio = chain1_width * 1000 / chain1_height;

	if (chain0_ratio == chain1_ratio) {
		scc_crop_width = chain0_width;
		scc_crop_height = chain0_height;
	} else if (chain0_ratio < chain1_ratio) {
		scc_crop_width = chain0_width;
		scc_crop_height =
			(chain0_width * (1000 * 100 / chain1_ratio)) / 100;
		scc_crop_width = ALIGN(scc_crop_width, 8);
		scc_crop_height = ALIGN(scc_crop_height, 8);
	} else if (chain0_ratio > chain1_ratio) {
		scc_crop_height = chain0_height;
		scc_crop_width =
			(chain0_height * (chain1_ratio * 100 / 1000)) / 100;
		scc_crop_width = ALIGN(scc_crop_width, 8);
		scc_crop_height = ALIGN(scc_crop_height, 8);
	}

	scc_crop_x = ((chain0_width - scc_crop_width) >> 1) & 0xFFFFFFFE;
	scc_crop_y = ((chain0_height - scc_crop_height) >> 1) & 0xFFFFFFFE;

	/* SCC OUTPUT */
	scc_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_param->input_crop.pos_x = scc_crop_x;
	scc_param->input_crop.pos_y = scc_crop_y;
	scc_param->input_crop.crop_width = scc_crop_width;
	scc_param->input_crop.crop_height = scc_crop_height;
	scc_param->input_crop.in_width = chain0_width;
	scc_param->input_crop.in_height = chain0_height;
	scc_param->input_crop.out_width = chain1_width;
	scc_param->input_crop.out_height = chain1_height;

	lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	indexes++;

	/*dbg_ischain("scc input crop pos : %dx%d\n", scc_crop_x, scc_crop_y);*/
	dbg_ischain("scc input crop size : %dx%d\n",
		scc_crop_width, scc_crop_height);

	scc_param->output_crop.cmd = SCALER_CROP_COMMAND_DISABLE;
	scc_param->output_crop.pos_x = 0;
	scc_param->output_crop.pos_y = 0;
	scc_param->output_crop.crop_width = chain1_width;
	scc_param->output_crop.crop_height = chain1_height;

	lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	indexes++;

	scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	scc_param->otf_output.width = chain1_width;
	scc_param->otf_output.height = chain1_height;

	lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	indexes++;

	/* ODC */
	odc_param->otf_input.width = chain1_width;
	odc_param->otf_input.height = chain1_height;

	lindex |= LOWBIT_OF(PARAM_ODC_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_ODC_OTF_INPUT);
	indexes++;

	odc_param->otf_output.width = chain1_width;
	odc_param->otf_output.height = chain1_height;

	lindex |= LOWBIT_OF(PARAM_ODC_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_ODC_OTF_OUTPUT);
	indexes++;

	/* DIS INPUT */
	dis_param->otf_input.width = chain1_width;
	dis_param->otf_input.height = chain1_height;

	lindex |= LOWBIT_OF(PARAM_DIS_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_DIS_OTF_INPUT);
	indexes++;

	fimc_is_itf_s_param(this, indexes, lindex, hindex);
	fimc_is_itf_a_param(this);

	this->chain1_width = chain1_width;
	this->chain1_height = chain1_height;

	return 0;
}

int fimc_is_ischain_s_chain2(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height)
{
	struct dis_param *dis_param;
	struct tdnr_param *tdnr_param;
	struct scalerp_param *scp_param;

	u32 chain2_width, chain2_height;
	u32 indexes, lindex, hindex;

	dbg_ischain("chain2 request size : %dx%d\n", width, height);

	dis_param = &this->is_region->parameter.dis;
	tdnr_param = &this->is_region->parameter.tdnr;
	scp_param = &this->is_region->parameter.scalerp;
	indexes = lindex = hindex = 0;

	/* CALCULATION */
	chain2_width = width;
	chain2_height = height;

	/* DIS OUTPUT */
	dis_param->otf_output.width = chain2_width;
	dis_param->otf_output.height = chain2_height;

	lindex |= LOWBIT_OF(PARAM_DIS_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_DIS_OTF_OUTPUT);
	indexes++;

	/* 3DNR */
	tdnr_param->otf_input.width = chain2_width;
	tdnr_param->otf_input.height = chain2_height;

	lindex |= LOWBIT_OF(PARAM_TDNR_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_TDNR_OTF_INPUT);
	indexes++;

	tdnr_param->otf_output.width = chain2_width;
	tdnr_param->otf_output.height = chain2_height;

	lindex |= LOWBIT_OF(PARAM_TDNR_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_TDNR_OTF_OUTPUT);
	indexes++;

	/* SCALERP INPUT */
	scp_param->otf_input.width = chain2_width;
	scp_param->otf_input.height = chain2_height;

	lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_INPUT);
	indexes++;

	fimc_is_itf_s_param(this, indexes, lindex, hindex);
	fimc_is_itf_a_param(this);

	this->chain2_width = chain2_width;
	this->chain2_height = chain2_height;

	return 0;
}

int fimc_is_ischain_s_chain3(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height)
{
	struct scalerp_param *scp_param;
	struct fd_param *fd_param;

	u32 chain2_width, chain2_height;
	u32 chain3_width, chain3_height;
	u32 scp_crop_width, scp_crop_height;
	u32 scp_crop_x, scp_crop_y;
	u32 indexes, lindex, hindex;

	dbg_ischain("chain3 request size : %dx%d\n", width, height);

	scp_param = &this->is_region->parameter.scalerp;
	fd_param = &this->is_region->parameter.fd;
	indexes = lindex = hindex = 0;

	chain2_width = this->chain2_width;
	chain2_height = this->chain2_height;

	chain3_width = width;
	chain3_height = height;

	scp_crop_x = 0;
	scp_crop_y = 0;
	scp_crop_width = chain2_width;
	scp_crop_height = chain2_height;

	scp_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scp_param->input_crop.pos_x = scp_crop_x;
	scp_param->input_crop.pos_y = scp_crop_y;
	scp_param->input_crop.crop_width = scp_crop_width;
	scp_param->input_crop.crop_height = scp_crop_height;
	scp_param->input_crop.in_width = chain2_width;
	scp_param->input_crop.in_height = chain2_height;
	scp_param->input_crop.out_width = chain3_width;
	scp_param->input_crop.out_height = chain3_height;

	lindex |= LOWBIT_OF(PARAM_SCALERP_INPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_INPUT_CROP);
	indexes++;

	scp_param->output_crop.crop_width = chain3_width;
	scp_param->output_crop.crop_height = chain3_height;

	lindex |= LOWBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
	indexes++;

	scp_param->otf_output.width = chain3_width;
	scp_param->otf_output.height = chain3_height;

	lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
	indexes++;

	scp_param->dma_output.width = chain3_width;
	scp_param->dma_output.height = chain3_height;

	lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	indexes++;

	/* FD */
	fd_param->otf_input.width = chain3_width;
	fd_param->otf_input.height = chain3_height;

	lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
	indexes++;

	fimc_is_itf_s_param(this, indexes, lindex, hindex);
	fimc_is_itf_a_param(this);

	this->chain3_width = chain3_width;
	this->chain3_height = chain3_height;

	return 0;
}

int fimc_is_ischain_g_capability(struct fimc_is_device_ischain *this,
	u32 user_ptr)
{
	int ret = 0;

	ret = copy_to_user((void *)user_ptr, &this->capability,
		sizeof(struct camera2_sm));

	return ret;
}

int fimc_is_ischain_callback(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame_shot *item;

#ifdef DBG_STREAMING
	dbg_ischain("%s\n", __func__);
#endif

	framemgr = this->framemgr;

	/* be careful of this code */
	/*
	1. buffer queue, all compoenent stop, so it's good
	2. interface callback, all component will be stop until new one
	is came
	therefore, i expect lock object is not necessary in here
	*/

	fimc_is_frame_request_head(framemgr, &item);
	if (item) {
		if (test_bit(FIMC_IS_REQ_SCC, &item->request_flag)) {
			if (this->scc.state == FIMC_IS_ISDEV_STOP)
				fimc_is_ischain_scc_start(this);
		} else {
			if (this->scc.state == FIMC_IS_ISDEV_START)
				fimc_is_ischain_scc_stop(this);
		}

		if (test_bit(FIMC_IS_REQ_SCP, &item->request_flag)) {
			if (this->scp.state == FIMC_IS_ISDEV_STOP)
				fimc_is_ischain_scp_start(this);
		} else {
			if (this->scp.state == FIMC_IS_ISDEV_START)
				fimc_is_ischain_scp_stop(this);
		}

		set_bit(FIMC_IS_ISCHAIN_RUN, &this->state);
		fimc_is_frame_trans_req_to_pro(framemgr, item);

		fimc_is_itf_shot(this, item);
	} else {
		clear_bit(FIMC_IS_ISCHAIN_RUN, &this->state);
#ifdef DBG_STREAMING
		dbg_warning("ischain is stopped\n");
#endif
	}

	return ret;
}

