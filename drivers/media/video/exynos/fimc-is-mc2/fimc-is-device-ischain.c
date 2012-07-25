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
#define FIMC_IS_SETFILE_SDCARD "/sdcard/setfile.bin"
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

	dbg_ischain("Allocating memory for FIMC-IS firmware.(alloc_ctx: 0x%08x)\n",
		(unsigned int)this->mem->alloc_ctx);

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
	fp = filp_open(FIMC_IS_SETFILE_SDCARD, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("failed to open %s\n", FIMC_IS_SETFILE_SDCARD);
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
		address = (void *)(this->minfo.kvaddr + load_addr);
		memcpy((void *)address, (void *)buf, fsize);
		fimc_is_ischain_cache_flush(this, load_addr, fsize + 1);
		dbg_ischain("FIMC_IS F/W loaded successfully - size:%d\n",
				fw_blob->size);
	}

request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif

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
			fimc_is_ischain_cache_flush(this, load_addr,
				fw_blob->size + 1);
		}

		dbg_ischain("FIMC_IS setfile loaded successfully - size:%d\n",
								fw_blob->size);
		release_firmware(fw_blob);

		dbg_ischain("Setfile base  = 0x%08x\n", load_addr);
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
			err("failed to config clock\n");
			ret = -EINVAL;
			break;
		}

		if (this->pdata->sensor_power_on) {
			this->pdata->sensor_power_on(this->pdev, 1);
		} else {
			err("failed to sensor_power_on\n");
			return -EINVAL;
		}

		if (this->pdata->clk_on) {
			this->pdata->clk_on(this->pdev, 1);
		} else {
			err("failed to clock on\n");
			ret = -EINVAL;
			break;
		}

		/* 6. config system mmu */
		printk(KERN_INFO "alloc_ctx : 0x%08x\n",
			(unsigned int)this->mem->alloc_ctx);
		if (this->mem->alloc_ctx)
			vb2_ion_attach_iommu(this->mem->alloc_ctx);

		/* 7. A5 start address setting */
		/*
		dbg_ischain("minfo.base(dvaddr) : 0x%08x\n",
			this->minfo.dvaddr);
		dbg_ischain("minfo.base(kvaddr) : 0x%08X\n",
			this->minfo.kvaddr);
		*/
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
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(this->pdev);
	struct fimc_is_device_sensor *sensor = &core->sensor;
	struct fimc_is_enum_sensor *sensor_info
		= &sensor->enum_sensor[sensor->id_position];

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

		printk(KERN_INFO "sensor channel : %d id : %d\n",
			sensor_info->flite_ch, sensor->id_position);

		if (this->pdata->clk_on) {
			this->pdata->clk_on(this->pdev, sensor_info->flite_ch);
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
		fimc_is_ischain_power_on(this);
	} else {
		fimc_is_ischain_power_off(this);
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

	if (lindex || hindex) {
		ret = fimc_is_hw_s_param(this->interface,
			this->instance,
			indexes,
			lindex,
			hindex);
	}

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
	u32 navailable = 0;

	struct is_region *region = this->is_region;

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
		region->parameter.scalerp.dma_output.cmd,
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

	ret = fimc_is_hw_open(this->interface, this->instance, input, channel,
		&this->margin_width, &this->margin_height);
	/*hack*/
	this->margin_width = 16;
	this->margin_height = 10;

	if (!ret)
		dbg_ischain("margin %dx%d\n",
			this->margin_width,
			this->margin_height);

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
#if 0
	index = 0;
	metadata = capability->color.availableModes[index];
	while (metadata) {
		dbg_ischain("availableModes : %d\n", metadata);
		index++;
		metadata = capability->color.availableModes[index];
	}
#endif

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
	metadata = capability->stats.availableFaceDetectModes[index];
	while (metadata) {
		dbg_ischain("availableFaceDetectModes : %d\n", metadata);
		index++;
		metadata = capability->stats.availableFaceDetectModes[index];
	}
	dbg_ischain("maxFaceCount : %d\n",
		capability->stats.maxFaceCount);
	dbg_ischain("histogrambucketCount : %d\n",
		capability->stats.histogramBucketCount);
	dbg_ischain("maxHistogramCount : %d\n",
		capability->stats.maxHistogramCount);
	dbg_ischain("sharpnessMapSize : %dx%d\n",
		capability->stats.sharpnessMapSize[0],
		capability->stats.sharpnessMapSize[1]);
	dbg_ischain("maxSharpnessMapValue : %d\n",
		capability->stats.maxSharpnessMapValue);

	dbg_ischain("===3A====================================\n");
#if 0
	index = 0;
	metadata = capability->aa.availableModes[index];
	while (metadata) {
		dbg_ischain("availableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.availableModes[index];
	}
#endif

	dbg_ischain("maxRegions : %d\n", capability->aa.maxRegions);

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
	struct fimc_is_frame_shot *frame)
{
	int ret = 0;
	void *cookie;

	this->fcount++;
	if (frame->shot->dm.request.frameCount != this->fcount) {
		err("shot frame count mismatch(%d, %d)",
			frame->shot->dm.request.frameCount, this->fcount);
		this->fcount = frame->shot->dm.request.frameCount;
		this->interface->fcount = this->fcount - 1;
	}

	/*flush cache*/
	cookie = vb2_plane_cookie(frame->vb, 1);
	vb2_ion_sync_for_device(cookie, 0, frame->shot_size, DMA_TO_DEVICE);

#if 0
	dbg_ischain("magic number : 0x%X\n", frame->shot->magicNumber);
	dbg_ischain("crop %d, %d, %d\n",
		frame->shot->ctl.scaler.cropRegion[0],
		frame->shot->ctl.scaler.cropRegion[1],
		frame->shot->ctl.scaler.cropRegion[2]);
	dbg_ischain("request : SCC(%d), SCP(%d)\n",
		frame->shot_ext->request_scc, frame->shot_ext->request_scp);
#endif

	if (frame->shot->magicNumber != 0x23456789) {
		err("shot magic number error(0x%08X)\n",
			frame->shot->magicNumber);
		ret = 1;
		goto exit;
	}

	ret = fimc_is_hw_shot_nblk(this->interface, this->instance,
		frame->dvaddr_buffer[0],
		frame->dvaddr_shot,
		frame->shot->dm.request.frameCount);

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
	struct fimc_is_ischain_dev *scc, *scp;

	scc = &this->scc;
	scp = &this->scp;

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

	/*hack*/
#if 1
	fimc_is_ishcain_initmem(this);
#endif

	clear_bit(FIMC_IS_ISCHAIN_LOADED, &this->state);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &this->state);
	clear_bit(FIMC_IS_ISCHAIN_RUN, &this->state);

	/*scc probe*/
	fimc_is_frame_probe(&scc->framemgr, FRAMEMGR_ID_SCC);

	/*scp probe*/
	fimc_is_frame_probe(&scp->framemgr, FRAMEMGR_ID_SCP);

	return 0;
}

int fimc_is_ischain_open(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	struct fimc_is_ischain_dev *scc, *scp;

	dbg_ischain("%s\n", __func__);

	do {
		scc = &this->scc;
		scp = &this->scp;

		mutex_init(&this->mutex_state);
		spin_lock_init(&this->slock_state);

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

		memset(&this->cur_peri_ctl, 0,
			sizeof(struct camera2_uctl));
		memset(&this->peri_ctls, 0,
			sizeof(struct camera2_uctl)*SENSOR_MAX_CTL);
		memset(&this->capability, 0,
			sizeof(struct camera2_sm));

		/* initial state, it's real apply to setting when opening*/
		this->chain0_width	= 0;
		this->chain0_height	= 0;
		this->chain1_width	= 0;
		this->chain1_height	= 0;
		this->chain2_width	= 0;
		this->chain2_height	= 0;
		this->chain3_width	= 0;
		this->chain3_height	= 0;

		this->fcount = 0;

		/*isp open*/
		fimc_is_frame_open(this->framemgr, 8);

		clear_bit(FIMC_IS_ISDEV_DSTART, &this->scc.state);
		clear_bit(FIMC_IS_ISDEV_BYPASS, &this->scc.state);

		clear_bit(FIMC_IS_ISDEV_DSTART, &this->scp.state);
		clear_bit(FIMC_IS_ISDEV_BYPASS, &this->scp.state);

		fimc_is_ischain_dev_open(&this->dis, NULL);
		set_bit(FIMC_IS_ISDEV_BYPASS, &this->dis.state);

		fimc_is_ischain_dev_open(&this->dnr, NULL);
		set_bit(FIMC_IS_ISDEV_BYPASS, &this->dnr.state);

		clear_bit(FIMC_IS_ISCHAIN_RUN, &this->state);
		/*fimc_is_fw_clear_irq1_all(core);*/
	} while (0);

	return ret;
}

int fimc_is_ischain_close(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	struct fimc_is_ischain_dev *scc, *scp;

	dbg_ischain("%s\n", __func__);

	scc = &this->scc;
	scp = &this->scp;

	ret = fimc_is_itf_power_down(this);
	if (ret) {
		err("power down is failed\n");
		fimc_is_ischain_lowpower(this, true);
	}

	fimc_is_ischain_power(this, 0);
	fimc_is_interface_close(this->interface);

	/*isp close*/
	fimc_is_frame_close(this->framemgr);

	/*scc close*/
	fimc_is_frame_close(&scc->framemgr);

	/*scp close*/
	fimc_is_frame_close(&scp->framemgr);

	return ret;
}

int fimc_is_ischain_init(struct fimc_is_device_ischain *this,
	u32 input, u32 channel)
{
	int ret = 0;

	dbg_ischain("%s(input : %d, channel : %d)\n",
		__func__, input, channel);

	if (input == SENSOR_NAME_S5K4E5) {
		this->margin_width = 16;
		this->margin_height = 10;
		this->chain0_width = 2560;
		this->chain0_height = 1920;
		this->chain1_width = 1920;
		this->chain1_height = 1080;
		this->chain2_width = 1920;
		this->chain2_height = 1080;
		this->chain3_width = 1920;
		this->chain3_height = 1080;
	} else {
		this->margin_width = 16;
		this->margin_height = 10;
		this->chain0_width = 1392;
		this->chain0_height = 1392;
		this->chain1_width = 1280;
		this->chain1_height = 720;
		this->chain2_width = 1280;
		this->chain2_height = 720;
		this->chain3_width = 1280;
		this->chain3_height = 720;
	}

	ret = fimc_is_itf_open(this, input, channel);
	if (ret) {
		err("open fail\n");
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

static int fimc_is_ischain_s_chain0_size(struct fimc_is_device_ischain *this,
	u32 width, u32 height)
{
	int ret = 0;
	struct isp_param *isp_param;
	struct drc_param *drc_param;
	struct scalerc_param *scc_param;

	u32 chain0_width, chain0_height;
	u32 indexes, lindex, hindex;

	isp_param = &this->is_region->parameter.isp;
	drc_param = &this->is_region->parameter.drc;
	scc_param = &this->is_region->parameter.scalerc;
	indexes = lindex = hindex = 0;
	chain0_width = width;
	chain0_height = height;

	dbg_ischain("request chain0 size : %dx%d\n",
		chain0_width, chain0_height);
	dbg_ischain("margin size : %dx%d\n",
		this->margin_width, this->margin_height);

	/* ISP */
	isp_param->otf_input.cmd = OTF_INPUT_COMMAND_DISABLE;
	isp_param->otf_input.width = chain0_width;
	isp_param->otf_input.height = chain0_height;
	isp_param->otf_input.format = OTF_INPUT_FORMAT_BAYER_DMA;
	isp_param->otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
	isp_param->otf_input.order = OTF_INPUT_ORDER_BAYER_GR_BG;
	isp_param->otf_input.frametime_max = 33333;
	lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_OTF_INPUT);
	indexes++;

	isp_param->dma1_input.cmd = DMA_INPUT_COMMAND_DISABLE;
	isp_param->dma1_input.width = chain0_width;
	isp_param->dma1_input.height = chain0_height;
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

	isp_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	isp_param->otf_output.width = chain0_width;
	isp_param->otf_output.height = chain0_height;
	isp_param->otf_output.format = OTF_OUTPUT_FORMAT_YUV444;
	isp_param->otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	isp_param->otf_output.order = OTF_INPUT_ORDER_BAYER_GR_BG;
	lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_OTF_OUTPUT);
	indexes++;

	isp_param->dma1_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	lindex |= LOWBIT_OF(PARAM_ISP_DMA1_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_DMA1_OUTPUT);
	indexes++;

	isp_param->dma2_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	lindex |= LOWBIT_OF(PARAM_ISP_DMA2_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_DMA2_OUTPUT);
	indexes++;

	/* DRC */
	drc_param->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	drc_param->otf_input.width = chain0_width;
	drc_param->otf_input.height = chain0_height;
	lindex |= LOWBIT_OF(PARAM_DRC_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_DRC_OTF_INPUT);
	indexes++;

	drc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	drc_param->otf_output.width = chain0_width;
	drc_param->otf_output.height = chain0_height;
	lindex |= LOWBIT_OF(PARAM_DRC_OTF_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_DRC_OTF_OUTPUT);
	indexes++;

	/* SCC */
	scc_param->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	scc_param->otf_input.width = chain0_width;
	scc_param->otf_input.height = chain0_height;
	lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_INPUT);
	indexes++;

	return ret;
}

static int fimc_is_ischain_s_chain1_size(struct fimc_is_device_ischain *this,
	u32 width, u32 height)
{
	int ret = 0;
	struct scalerc_param *scc_param;
	struct odc_param *odc_param;
	struct dis_param *dis_param;

	u32 chain0_width, chain0_height, chain0_ratio;
	u32 chain1_width, chain1_height, chain1_ratio;
	u32 scc_crop_width, scc_crop_height;
	u32 scc_crop_x, scc_crop_y;
	u32 indexes, lindex, hindex;

	scc_param = &this->is_region->parameter.scalerc;
	odc_param = &this->is_region->parameter.odc;
	dis_param = &this->is_region->parameter.dis;
	indexes = lindex = hindex = 0;
	chain0_width = this->chain0_width;
	chain0_height = this->chain0_height;
	chain1_width = width;
	chain1_height = height;

	dbg_ischain("current chain0 size : %dx%d\n",
		chain0_width, chain0_height);
	dbg_ischain("current chain1 size : %dx%d\n",
		this->chain1_width, this->chain1_height);
	dbg_ischain("request chain1 size : %dx%d\n",
		chain1_width, chain1_height);

	if (!chain0_width) {
		err("chain0 width is zero");
		ret = -EINVAL;
		goto exit;
	}

	if (!chain0_height) {
		err("chain0 height is zero");
		ret = -EINVAL;
		goto exit;
	}

	if (!chain1_width) {
		err("chain1 width is zero");
		ret = -EINVAL;
		goto exit;
	}

	if (!chain1_height) {
		err("chain1 height is zero");
		ret = -EINVAL;
		goto exit;
	}

	/* CALCULATION */
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

	dbg_ischain("scc input crop pos : %dx%d\n", scc_crop_x, scc_crop_y);
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

	dbg_ischain("scc output crop size : %dx%d\n",
		chain1_width, chain1_height);

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

	this->lindex |= lindex;
	this->hindex |= hindex;
	this->indexes += indexes;

exit:
	return ret;
}

static int fimc_is_ischain_s_chain2_size(struct fimc_is_device_ischain *this,
	u32 width, u32 height)
{
	int ret = 0;
	struct dis_param *dis_param;
	struct tdnr_param *tdnr_param;
	struct scalerp_param *scp_param;

	u32 chain2_width, chain2_height;
	u32 indexes, lindex, hindex;

	dbg_ischain("request chain2 size : %dx%d\n", width, height);
	dbg_ischain("current chain2 size : %dx%d\n",
		this->chain2_width, this->chain2_height);

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

	tdnr_param->dma_output.width = chain2_width;
	tdnr_param->dma_output.height = chain2_height;
	lindex |= LOWBIT_OF(PARAM_TDNR_DMA_OUTPUT);
	hindex |= HIGHBIT_OF(PARAM_TDNR_DMA_OUTPUT);
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

	this->lindex |= lindex;
	this->hindex |= hindex;
	this->indexes += indexes;

	return ret;
}

static int fimc_is_ischain_s_chain3_size(struct fimc_is_device_ischain *this,
	u32 width, u32 height)
{
	int ret = 0;
	struct scalerp_param *scp_param;
	struct fd_param *fd_param;
	u32 chain2_width, chain2_height;
	u32 chain3_width, chain3_height;
	u32 scp_crop_width, scp_crop_height;
	u32 scp_crop_x, scp_crop_y;
	u32 indexes, lindex, hindex;

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

	dbg_ischain("request chain3 size : %dx%d\n", width, height);
	dbg_ischain("current chain3 size : %dx%d\n",
		this->chain3_width, this->chain3_height);


	/*SCALERP*/
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

	this->lindex |= lindex;
	this->hindex |= hindex;
	this->indexes += indexes;

	return ret;
}

#if 0
static int fimc_is_ischain_s_chain1_stop(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	struct scalerc_param *scc_param;
	struct odc_param *odc_param;
	struct dis_param *dis_param;

	u32 indexes, lindex, hindex;

	scc_param = &this->is_region->parameter.scalerc;
	odc_param = &this->is_region->parameter.odc;
	dis_param = &this->is_region->parameter.dis;
	indexes = lindex = hindex = 0;

	/* SCC OUTPUT */
	scc_param->control.cmd = CONTROL_COMMAND_STOP;
	lindex |= LOWBIT_OF(PARAM_SCALERC_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_CONTROL);
	indexes++;

	/* ODC */
	odc_param->control.cmd = CONTROL_COMMAND_STOP;
	lindex |= LOWBIT_OF(PARAM_ODC_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_ODC_CONTROL);
	indexes++;

	/* DIS INPUT */
	dis_param->control.cmd = CONTROL_COMMAND_STOP;
	lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	indexes++;

	this->lindex |= lindex;
	this->hindex |= hindex;
	this->indexes += indexes;

	return ret;
}

static int fimc_is_ischain_s_chain1_start(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	struct scalerc_param *scc_param;
	struct odc_param *odc_param;
	struct dis_param *dis_param;
	u32 indexes, lindex, hindex;

	scc_param = &this->is_region->parameter.scalerc;
	odc_param = &this->is_region->parameter.odc;
	dis_param = &this->is_region->parameter.dis;
	indexes = lindex = hindex = 0;

	/* SCC OUTPUT */
	scc_param->control.cmd = CONTROL_COMMAND_START;
	lindex |= LOWBIT_OF(PARAM_SCALERC_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_CONTROL);
	indexes++;

	/* ODC */
	odc_param->control.cmd = CONTROL_COMMAND_START;
	lindex |= LOWBIT_OF(PARAM_ODC_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_ODC_CONTROL);
	indexes++;

	/* DIS INPUT */
	dis_param->control.cmd = CONTROL_COMMAND_START;
	lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	indexes++;

	this->lindex |= lindex;
	this->hindex |= hindex;
	this->indexes += indexes;

	return ret;
}
#endif

static int fimc_is_ischain_dis_bypass(struct fimc_is_device_ischain *this,
	bool bypass)
{
	int ret = 0;
	u32 chain1_width, chain1_height;
	struct dis_param *dis_param;

	dbg_ischain("%s(%d)\n", __func__, bypass);

	dis_param = &this->is_region->parameter.dis;

	ret = fimc_is_itf_process_off(this);
	if (ret) {
		err("fimc_is_itf_process_off is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	if (bypass) {
		chain1_width = this->chain1_width;
		chain1_height = this->chain1_height;
	} else {
		chain1_width = ALIGN(this->chain1_width*125/100, 4);
		chain1_height = ALIGN(this->chain1_height*125/100, 2);
	}

	this->lindex = this->hindex = this->indexes = 0;
	fimc_is_ischain_s_chain1_size(this, chain1_width, chain1_height);

	if (bypass)
		dis_param->control.bypass = CONTROL_BYPASS_ENABLE;
	else {
		dis_param->control.bypass = CONTROL_BYPASS_DISABLE;
		dis_param->control.buffer_number =
			SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;
		dis_param->control.buffer_address =
			this->minfo.dvaddr_shared + 300 * sizeof(u32);
		this->is_region->shared[300] = this->minfo.dvaddr_dis;
	}

	this->lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	this->hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	this->indexes++;

	ret = fimc_is_itf_s_param(this,
		this->indexes, this->lindex, this->hindex);
	if (ret) {
		err("fimc_is_itf_s_param is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_itf_a_param(this);
	if (ret) {
		err("fimc_is_itf_a_param is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_itf_process_on(this);
	if (ret) {
		err("fimc_is_itf_process_on is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	if (bypass) {
		set_bit(FIMC_IS_ISDEV_BYPASS, &this->dis.state);
		dbg_ischain("DIS off\n");
	} else {
		clear_bit(FIMC_IS_ISDEV_BYPASS, &this->dis.state);
		this->dis.skip_frames = 2;
		dbg_ischain("DIS on\n");
	}

exit:
	return ret;
}

static int fimc_is_ischain_dnr_bypass(struct fimc_is_device_ischain *this,
	bool bypass)
{
	int ret = 0;
	struct tdnr_param *dnr_param;
	u32 indexes, lindex, hindex;

	dbg_ischain("%s\n", __func__);

	dnr_param = &this->is_region->parameter.tdnr;
	indexes = lindex = hindex = 0;

	if (bypass)
		dnr_param->control.bypass = CONTROL_BYPASS_ENABLE;
	else {
		dnr_param->control.bypass = CONTROL_BYPASS_DISABLE;
		dnr_param->control.buffer_number =
			SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF;
		dnr_param->control.buffer_address =
			this->minfo.dvaddr_shared + 350 * sizeof(u32);
		this->is_region->shared[350] = this->minfo.dvaddr_3dnr;
	}

	lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	indexes++;

	ret = fimc_is_itf_s_param(this, indexes, lindex, hindex);
	if (ret) {
		err("fimc_is_itf_s_param is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	if (bypass)
		set_bit(FIMC_IS_ISDEV_BYPASS, &this->dnr.state);
	else
		clear_bit(FIMC_IS_ISDEV_BYPASS, &this->dnr.state);

exit:
	return ret;
}

int fimc_is_ischain_isp_start(struct fimc_is_device_ischain *this,
	struct fimc_is_video_common *video)
{
	int ret = 0;
	struct isp_param *isp_param;
	struct fimc_is_framemgr *framemgr;
	u32 lindex, hindex, indexes;
	u32 i;

	dbg_front("%s\n", __func__);

	indexes = lindex = hindex = 0;

	framemgr = this->framemgr;
	isp_param = &this->is_region->parameter.isp;

	for (i = 0; i < video->buffers; i++) {
		fimc_is_hw_cfg_mem(this->interface, 0,
			video->buf_dva[i][1], video->frame.size[1]);
	}

	fimc_is_ischain_s_chain0_size(this,
		this->chain0_width, this->chain0_height);

	fimc_is_ischain_s_chain1_size(this,
		this->chain1_width, this->chain1_height);

	fimc_is_ischain_s_chain2_size(this,
		this->chain2_width, this->chain2_height);

	fimc_is_ischain_s_chain3_size(this,
		this->chain3_width, this->chain3_height);

	isp_param->control.cmd = CONTROL_COMMAND_START;
	isp_param->control.bypass = CONTROL_BYPASS_DISABLE;
	isp_param->control.run_mode = 1;
	lindex |= LOWBIT_OF(PARAM_ISP_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_ISP_CONTROL);
	indexes++;

	isp_param->otf_input.cmd = OTF_INPUT_COMMAND_DISABLE;
	isp_param->otf_input.format = OTF_INPUT_FORMAT_BAYER_DMA;
	isp_param->otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
	isp_param->otf_input.order = OTF_INPUT_ORDER_BAYER_GR_BG;
	isp_param->otf_input.frametime_max = 33333;
	lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_ISP_OTF_INPUT);
	indexes++;

	isp_param->dma1_input.cmd = DMA_INPUT_COMMAND_BUF_MNGR;
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

	lindex = 0xFFFFFFFF;
	hindex = 0xFFFFFFFF;

	ret = fimc_is_itf_s_param(this , indexes, lindex, hindex);
	if (ret) {
		err("fimc_is_itf_s_param is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_itf_f_param(this);
	if (ret) {
		err("fimc_is_itf_f_param is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_itf_g_capability(this);
	if (ret) {
		err("fimc_is_itf_g_capability is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_itf_process_on(this);
	if (ret) {
		err("fimc_is_itf_process_on is fail\n");
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

int fimc_is_ischain_isp_stop(struct fimc_is_device_ischain *this)
{
	int ret = 0;
	struct fimc_is_interface *itf;
	struct fimc_is_framemgr *framemgr;

	itf = this->interface;
	framemgr = this->framemgr;

	while (framemgr->frame_request_cnt) {
		printk(KERN_INFO "%d frame reqs waiting...\n",
			framemgr->frame_request_cnt);
		mdelay(1);
	}

	while (framemgr->frame_process_cnt) {
		printk(KERN_INFO "%d frame pros waiting...\n",
			framemgr->frame_process_cnt);
		mdelay(1);
	}

	while (itf->nblk_shot.work_request_cnt) {
		printk(KERN_INFO "%d shot reqs waiting...\n",
			itf->nblk_shot.work_request_cnt);
		mdelay(1);
	}

	ret = fimc_is_itf_process_off(this);
	if (ret) {
		err("fimc_is_itf_process_off is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	fimc_is_frame_close(this->framemgr);
	fimc_is_frame_open(this->framemgr, NUM_ISP_DMA_BUF);

exit:
	return ret;
}

int fimc_is_ischain_isp_s_format(struct fimc_is_device_ischain *this,
		u32 width, u32 height)
{
	int ret = 0;

	this->chain0_width = width - this->margin_width;
	this->chain0_height = height - this->margin_height;

	return ret;
}

int fimc_is_ischain_isp_buffer_queue(struct fimc_is_device_ischain *this,
	u32 index)
{
	int ret = 0;
	struct fimc_is_frame_shot *frame;
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

	framemgr_e_barrier_irq(framemgr, index);

	frame = &framemgr->frame[index];
	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		if (frame->req_flag) {
			dbg_warning("%d request flag is not clear(%08X)\n",
				frame->index, (u32)frame->req_flag);
			frame->req_flag = 0;
		}

		set_bit(FIMC_IS_REQ_MDT, &frame->req_flag);
		if (frame->shot_ext->request_scc)
			set_bit(FIMC_IS_REQ_SCC, &frame->req_flag);
		if (frame->shot_ext->request_scp)
			set_bit(FIMC_IS_REQ_SCP, &frame->req_flag);

		frame->fcount = frame->shot->dm.request.frameCount;

		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		err("frame(%d) is not free state(%d)\n", index, frame->state);
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	framemgr_x_barrier_irq(framemgr, index);

	mutex_lock(&this->mutex_state);
	if (!test_bit(FIMC_IS_ISCHAIN_RUN, &this->state))
		fimc_is_ischain_callback(this);
	mutex_unlock(&this->mutex_state);

exit:
	return ret;
}

int fimc_is_ischain_isp_buffer_finish(struct fimc_is_device_ischain *this,
	u32 index)
{
	int ret = 0;
	struct fimc_is_frame_shot *item;
	struct fimc_is_framemgr *framemgr = this->framemgr;

#ifdef DBG_STREAMING
	/*dbg_ischain("%s\n", __func__);*/
#endif

	framemgr_e_barrier_irq(framemgr, index+0xf0);

	fimc_is_frame_complete_head(framemgr, &item);
	if (item) {
		if (index == item->index)
			fimc_is_frame_trans_com_to_fre(framemgr, item);
		else {
			dbg_warning("buffer index is NOT matched(%d != %d)\n",
				index, item->index);
			fimc_is_frame_print_free_list(framemgr);
			fimc_is_frame_print_request_list(framemgr);
			fimc_is_frame_print_process_list(framemgr);
			fimc_is_frame_print_complete_list(framemgr);
		}
	} else {
		fimc_is_frame_process_head(framemgr, &item);
		if (item) {
			if (item->req_flag)
				dbg_warning("request flag(%X) is not clr\n",
					(u32)item->req_flag);
			else {
				err("item is empty from complete0");
				fimc_is_frame_print_free_list(framemgr);
				fimc_is_frame_print_request_list(framemgr);
				fimc_is_frame_print_process_list(framemgr);
				fimc_is_frame_print_complete_list(framemgr);
			}
		} else {
			err("item is empty from complete1");
			fimc_is_frame_print_free_list(framemgr);
			fimc_is_frame_print_request_list(framemgr);
			fimc_is_frame_print_process_list(framemgr);
			fimc_is_frame_print_complete_list(framemgr);
		}
	}

	framemgr_x_barrier_irq(framemgr, index+0xf0);

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
	video = scc->video;

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
		set_bit(FIMC_IS_ISDEV_DSTART, &scc->state);
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

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
		clear_bit(FIMC_IS_ISDEV_DSTART, &scc->state);
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

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
	video = scp->video;

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
		set_bit(FIMC_IS_ISDEV_DSTART, &scp->state);
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

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
		clear_bit(FIMC_IS_ISDEV_DSTART, &scp->state);
	else
		err("fimc_is_itf_s_param is fail\n");

	if (!this->interface->streaming)
		fimc_is_itf_a_param(this);

	return ret;
}

int fimc_is_ischain_scp_s_format(struct fimc_is_device_ischain *this,
	u32 width, u32 height)
{
	int ret = 0;

	this->chain1_width = width;
	this->chain1_height = height;
	this->chain2_width = width;
	this->chain2_height = height;
	this->chain3_width = width;
	this->chain3_height = height;

	return ret;
}

int fimc_is_ischain_dev_open(struct fimc_is_ischain_dev *this,
	struct fimc_is_video_common *video)
{
	int ret = 0;

	spin_lock_init(&this->slock_state);
	fimc_is_frame_open(&this->framemgr, 8);

	clear_bit(FIMC_IS_ISDEV_DSTART, &this->state);
	clear_bit(FIMC_IS_ISDEV_BYPASS, &this->state);

	this->skip_frames = 0;
	this->video = video;

	return ret;
}

int fimc_is_ischain_dev_buffer_queue(struct fimc_is_ischain_dev *this,
	u32 index)
{
	int ret = 0;
	struct fimc_is_frame_shot *frame;
	struct fimc_is_framemgr *framemgr;

#ifdef DBG_STREAMING
	dbg_ischain("%s\n", __func__);
#endif

	framemgr = &this->framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr_e_barrier_irq(framemgr, index);

	frame = &framemgr->frame[index];
	if (frame->state == FIMC_IS_FRAME_STATE_FREE)
		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	else {
		err("frame(%d) is not free state(%d)\n", index, frame->state);
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	framemgr_x_barrier_irq(framemgr, index);

exit:
	return ret;
}

int fimc_is_ischain_dev_buffer_finish(struct fimc_is_ischain_dev *this,
	u32 index)
{
	int ret = 0;
	struct fimc_is_frame_shot *frame;
	struct fimc_is_framemgr *framemgr;

#ifdef DBG_STREAMING
	/*dbg_ischain("%s\n", __func__);*/
#endif

	framemgr = &this->framemgr;

	framemgr_e_barrier_irq(framemgr, index);

	fimc_is_frame_complete_head(framemgr, &frame);
	if (frame) {
		if (index == frame->index)
			fimc_is_frame_trans_com_to_fre(framemgr, frame);
		else {
			dbg_warning("buffer index is NOT matched(%d != %d)\n",
				index, frame->index);
			fimc_is_frame_print_free_list(framemgr);
			fimc_is_frame_print_request_list(framemgr);
			fimc_is_frame_print_process_list(framemgr);
			fimc_is_frame_print_complete_list(framemgr);
		}
	} else {
		err("frame is empty from complete");
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	framemgr_x_barrier_irq(framemgr, index);

	return ret;
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
	bool scc_req, scp_req;
	unsigned long flags;
	struct fimc_is_framemgr *isp_framemgr, *scc_framemgr, *scp_framemgr;
	struct fimc_is_frame_shot *isp_frame, *scc_frame, *scp_frame;
	struct fimc_is_ischain_dev *scc, *scp;

#ifdef DBG_STREAMING
	dbg_ischain("%s\n", __func__);
#endif

	scc = &this->scc;
	scp = &this->scp;
	isp_framemgr = this->framemgr;
	scc_framemgr = &scc->framemgr;
	scp_framemgr = &scp->framemgr;
	scc_req = scp_req = false;
	/* be careful of this code */
	/*
	1. buffer queue, all compoenent stop, so it's good
	2. interface callback, all component will be stop until new one
	is came
	therefore, i expect lock object is not necessary in here
	*/

	fimc_is_frame_request_head(isp_framemgr, &isp_frame);
	if (!isp_frame) {
		clear_bit(FIMC_IS_ISCHAIN_RUN, &this->state);
#ifdef DBG_STREAMING
		dbg_warning("ischain is stopped\n");
#endif
		goto exit;
	}

	if (test_bit(FIMC_IS_REQ_SCC, &isp_frame->req_flag)) {
		if (!test_bit(FIMC_IS_ISDEV_DSTART, &this->scc.state))
			fimc_is_ischain_scc_start(this);

		scc_req = true;
	} else {
		if (test_bit(FIMC_IS_ISDEV_DSTART, &this->scc.state))
			fimc_is_ischain_scc_stop(this);
	}

	if (test_bit(FIMC_IS_REQ_SCP, &isp_frame->req_flag)) {
		if (!test_bit(FIMC_IS_ISDEV_DSTART, &this->scp.state))
			fimc_is_ischain_scp_start(this);

		scp_req = true;
	} else {
		if (test_bit(FIMC_IS_ISDEV_DSTART, &this->scp.state))
			fimc_is_ischain_scp_stop(this);
	}

	if (isp_frame->shot_ext->dis_bypass) {
		if (!test_bit(FIMC_IS_ISDEV_BYPASS, &this->dis.state))
			fimc_is_ischain_dis_bypass(this, true);
	} else {
		if (test_bit(FIMC_IS_ISDEV_BYPASS, &this->dis.state))
			fimc_is_ischain_dis_bypass(this, false);
	}

	if (isp_frame->shot_ext->dnr_bypass) {
		if (!test_bit(FIMC_IS_ISDEV_BYPASS, &this->dnr.state))
			fimc_is_ischain_dnr_bypass(this, true);
	} else {
		if (test_bit(FIMC_IS_ISDEV_BYPASS, &this->dnr.state))
			fimc_is_ischain_dnr_bypass(this, false);
	}
	set_bit(FIMC_IS_ISCHAIN_RUN, &this->state);
	fimc_is_frame_trans_req_to_pro(isp_framemgr, isp_frame);

	if (scc_req) {
		framemgr_e_barrier_irqs(scc_framemgr, FMGR_IDX_8, flags);

		fimc_is_frame_request_head(scc_framemgr, &scc_frame);
		if (scc_frame) {
			isp_frame->shot->uctl.scalerUd.sccTargetAddress[0] =
				scc_frame->dvaddr_buffer[0];
			isp_frame->shot->uctl.scalerUd.sccTargetAddress[1] =
				scc_frame->dvaddr_buffer[1];
			isp_frame->shot->uctl.scalerUd.sccTargetAddress[2] =
				scc_frame->dvaddr_buffer[2];

			fimc_is_frame_trans_req_to_pro(scc_framemgr, scc_frame);
		} else {
			isp_frame->shot->uctl.scalerUd.sccTargetAddress[0] = 0;
			isp_frame->shot->uctl.scalerUd.sccTargetAddress[1] = 0;
			isp_frame->shot->uctl.scalerUd.sccTargetAddress[2] = 0;
			isp_frame->shot_ext->request_scc = 0;
			clear_bit(FIMC_IS_REQ_SCC, &isp_frame->req_flag);
			err("scc %d frame is drop", isp_frame->fcount);
		}

		framemgr_x_barrier_irqr(scc_framemgr, FMGR_IDX_8, flags);
	} else {
		isp_frame->shot->uctl.scalerUd.sccTargetAddress[0] = 0;
		isp_frame->shot->uctl.scalerUd.sccTargetAddress[1] = 0;
		isp_frame->shot->uctl.scalerUd.sccTargetAddress[2] = 0;
		isp_frame->shot_ext->request_scc = 0;
		clear_bit(FIMC_IS_REQ_SCC, &isp_frame->req_flag);
	}

	if (scp_req) {
		framemgr_e_barrier_irqs(scp_framemgr, FMGR_IDX_9, flags);

		fimc_is_frame_request_head(scp_framemgr, &scp_frame);
		if (scp_frame) {
			isp_frame->shot->uctl.scalerUd.scpTargetAddress[0] =
				scp_frame->dvaddr_buffer[0];
			isp_frame->shot->uctl.scalerUd.scpTargetAddress[1] =
				scp_frame->dvaddr_buffer[1];
			isp_frame->shot->uctl.scalerUd.scpTargetAddress[2] =
				scp_frame->dvaddr_buffer[2];

			fimc_is_frame_trans_req_to_pro(scp_framemgr, scp_frame);
		} else {
			isp_frame->shot->uctl.scalerUd.scpTargetAddress[0] = 0;
			isp_frame->shot->uctl.scalerUd.scpTargetAddress[1] = 0;
			isp_frame->shot->uctl.scalerUd.scpTargetAddress[2] = 0;
			isp_frame->shot_ext->request_scp = 0;
			clear_bit(FIMC_IS_REQ_SCP, &isp_frame->req_flag);
			err("scp %d frame is drop", isp_frame->fcount);
		}

		framemgr_x_barrier_irqr(scp_framemgr, FMGR_IDX_9, flags);
	} else {
		isp_frame->shot->uctl.scalerUd.scpTargetAddress[0] = 0;
		isp_frame->shot->uctl.scalerUd.scpTargetAddress[1] = 0;
		isp_frame->shot->uctl.scalerUd.scpTargetAddress[2] = 0;
		isp_frame->shot_ext->request_scp = 0;
		clear_bit(FIMC_IS_REQ_SCP, &isp_frame->req_flag);
	}

	/*skip frame check*/
	if (this->dis.skip_frames) {
		this->dis.skip_frames--;
		isp_frame->shot_ext->request_scc = 0;
		isp_frame->shot_ext->request_scp = 0;
		/*clear_bit(FIMC_IS_REQ_SCC, &isp_frame->req_flag);
		clear_bit(FIMC_IS_REQ_SCP, &isp_frame->req_flag);*/
	}

	fimc_is_itf_shot(this, isp_frame);
exit:
	return ret;
}

int fimc_is_ischain_camctl(struct fimc_is_device_ischain *this,
	struct fimc_is_frame_shot *frame,
	u32 fcount)
{
	int ret = 0;
	u32 updated = 0;
	struct fimc_is_interface *itf;
	struct camera2_sensor_ctl *cur_sensor_ctl;
	struct camera2_sensor_ctl *isp_sensor_ctl;
	struct camera2_sensor_ctl *req_sensor_ctl;

	u32 index;
	u64 frameDuration;
	u64 exposureTime;
	u32 sensitivity;

#ifdef DBG_STREAMING
	dbg_ischain("%s\n", __func__);
#endif

	itf = this->interface;
	cur_sensor_ctl = &this->cur_peri_ctl.sensorUd.ctl;
	isp_sensor_ctl = &itf->isp_peri_ctl.sensorUd.ctl;

	if (frame)
		req_sensor_ctl = &frame->shot->ctl.sensor;
	else
		req_sensor_ctl = NULL;

	if (req_sensor_ctl && req_sensor_ctl->frameDuration)
		frameDuration = req_sensor_ctl->frameDuration;
	else
		frameDuration = isp_sensor_ctl->frameDuration;

	if (cur_sensor_ctl->frameDuration != frameDuration) {
		cur_sensor_ctl->frameDuration = frameDuration;
		/*dbg_ischain("fduration : %d\n", (u32)duration);*/
		updated |= (1<<1);
	}

	if (req_sensor_ctl && req_sensor_ctl->exposureTime)
		exposureTime = req_sensor_ctl->exposureTime;
	else
		exposureTime = isp_sensor_ctl->exposureTime;

	if (cur_sensor_ctl->exposureTime != exposureTime) {
		cur_sensor_ctl->exposureTime = exposureTime;
		/*dbg_ischain("exposure : %d\n", (u32)exposure);*/
		updated |= (1<<1);
	}

	if (req_sensor_ctl && req_sensor_ctl->sensitivity)
		sensitivity = req_sensor_ctl->sensitivity;
	else
		sensitivity = isp_sensor_ctl->sensitivity;

	if (cur_sensor_ctl->sensitivity != sensitivity) {
		cur_sensor_ctl->sensitivity = sensitivity;
		/*dbg_ischain("sensitivity : %d\n", (u32)exposure);*/
		updated |= (1<<1);
	}

	if (updated) {
		this->cur_peri_ctl.uUpdateBitMap = updated;
		memcpy(this->is_region->shared, &this->cur_peri_ctl,
			sizeof(struct camera2_uctl));
		fimc_is_itf_s_camctrl(this, this->minfo.dvaddr_shared, fcount);
	}

	index = SENSOR_MAX_CTL_MASK & (fcount + 3);
	memcpy(&this->peri_ctls[index], &this->cur_peri_ctl,
		sizeof(struct camera2_uctl));

	return ret;
}

int fimc_is_ischain_tag(struct fimc_is_device_ischain *ischain,
	struct fimc_is_frame_shot *frame)
{
	int ret = 0;
	struct camera2_uctl *applied_ctl;
	struct timeval curtime;
	u32 fcount;

	fcount = frame->fcount;
	applied_ctl = &ischain->peri_ctls[fcount & SENSOR_MAX_CTL_MASK];

	do_gettimeofday(&curtime);

	frame->shot->dm.sensor.exposureTime =
		applied_ctl->sensorUd.ctl.exposureTime;
	frame->shot->dm.sensor.sensitivity =
		applied_ctl->sensorUd.ctl.sensitivity;
	frame->shot->dm.sensor.frameDuration =
		applied_ctl->sensorUd.ctl.frameDuration;
	frame->shot->dm.sensor.timeStamp =
		curtime.tv_sec*1000000 + curtime.tv_usec;
	frame->shot->dm.request.frameCount = fcount;

	return ret;
}
