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

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-helper.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-misc.h"
#include "fimc-is-video.h"

#include "fimc-is-device-ischain.h"

int device_is_probe(struct fimc_is_device_ischain *ischain,
	void *data)
{
	struct fimc_is_core *core = (struct fimc_is_core *)data;

	ischain->private_data = (u32)data;
	ischain->interface = &core->interface;

	ischain->is_region = core->is_p_region;
	ischain->is_shared_region = core->is_shared_region;

	ischain->chain0_width = 2560;
	ischain->chain0_height = 1920;
	ischain->chain1_width = 0;
	ischain->chain1_height = 0;
	ischain->chain2_width = 0;
	ischain->chain2_height = 0;

	return 0;
}

int fimc_is_ischain_open(struct fimc_is_device_ischain *this)
{
	dbg_ischain("%s\n", __func__);

	return 0;
}

int fimc_is_ischain_close(struct fimc_is_device_ischain *this)
{
	struct fimc_is_core *core = (struct fimc_is_core *)this->private_data;
	struct fimc_is_interface *interface = this->interface;

	dbg_ischain("%s\n", __func__);

	fimc_is_hw_power_down(interface, 0);
	fimc_is_hw_a5_power(core, 0);

	return 0;
}

int fimc_is_ischain_start_streaming(struct fimc_is_device_ischain *this,
	struct fimc_is_video_common *video)
{
	int ret = 0;
	struct isp_param *isp_param;
	struct odc_param *odc_param;
	struct sensor_param *sensor_param;
	u32 width, height;
	u32 lindex, hindex, indexes;

	dbg_chain("%s\n", __func__);

	width = this->chain0_width;
	height = this->chain0_height;

	indexes = 0;
	lindex = hindex = 0;

	sensor_param = &this->is_region->parameter.sensor;
	isp_param = &this->is_region->parameter.isp;
	odc_param = &this->is_region->parameter.odc;

	sensor_param->frame_rate.frame_rate = 10;

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

	fimc_is_hw_s_param(this->interface, 0, indexes, lindex, hindex);
	fimc_is_hw_a_param(this->interface, 0);

	return ret;
}

int fimc_is_ischain_s_chain0(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
	u32 width,
	u32 height)
{
	return 0;
}

int fimc_is_ischain_s_chain1(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
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

	scc_param = &ischain->is_region->parameter.scalerc;
	odc_param = &ischain->is_region->parameter.odc;
	dis_param = &ischain->is_region->parameter.dis;
	indexes = lindex = hindex = 0;

	/* CALCULATION */
	chain0_width = ischain->chain0_width;
	chain0_height = ischain->chain0_height;

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

	fimc_is_hw_s_param(interface, 0, indexes, lindex, hindex);
	fimc_is_hw_a_param(interface, 0);

	ischain->chain1_width = chain1_width;
	ischain->chain1_height = chain1_height;

	return 0;
}

int fimc_is_ischain_s_chain2(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
	u32 width,
	u32 height)
{
	struct dis_param *dis_param;
	struct tdnr_param *tdnr_param;
	struct scalerp_param *scp_param;

	u32 chain2_width, chain2_height;
	u32 indexes, lindex, hindex;

	dbg_ischain("chain2 request size : %dx%d\n", width, height);

	dis_param = &ischain->is_region->parameter.dis;
	tdnr_param = &ischain->is_region->parameter.tdnr;
	scp_param = &ischain->is_region->parameter.scalerp;
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

	fimc_is_hw_s_param(interface, 0, indexes, lindex, hindex);
	fimc_is_hw_a_param(interface, 0);

	ischain->chain2_width = chain2_width;
	ischain->chain2_height = chain2_height;

	return 0;
}

int fimc_is_ischain_s_chain3(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
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

	scp_param = &ischain->is_region->parameter.scalerp;
	fd_param = &ischain->is_region->parameter.fd;
	indexes = lindex = hindex = 0;

	chain2_width = ischain->chain2_width;
	chain2_height = ischain->chain2_height;

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

	fimc_is_hw_s_param(interface, 0, indexes, lindex, hindex);
	fimc_is_hw_a_param(interface, 0);

	ischain->chain3_width = chain3_width;
	ischain->chain3_height = chain3_height;

	return 0;
}
