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
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

#include "fimc-is-device-sensor.h"

/* PMU for FIMC-IS*/
#define MIPICSI0_REG_BASE	(S5P_VA_MIPICSI0)   /* phy : 0x13c2_0000 */
#define MIPICSI1_REG_BASE	(S5P_VA_MIPICSI1)   /* phy : 0x13c3_0000 */

/*MIPI*/
/* CSIS global control */
#define S5PCSIS_CTRL					(0x00)
#define S5PCSIS_CTRL_DPDN_DEFAULT			(0 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP				(1 << 31)
#define S5PCSIS_CTRL_ALIGN_32BIT			(1 << 20)
#define S5PCSIS_CTRL_UPDATE_SHADOW			(1 << 16)
#define S5PCSIS_CTRL_WCLK_EXTCLK			(1 << 8)
#define S5PCSIS_CTRL_RESET				(1 << 4)
#define S5PCSIS_CTRL_ENABLE				(1 << 0)

/* D-PHY control */
#define S5PCSIS_DPHYCTRL				(0x04)
#define S5PCSIS_DPHYCTRL_HSS_MASK			(0x1f << 27)
#define S5PCSIS_DPHYCTRL_ENABLE				(0x7 << 0)

#define S5PCSIS_CONFIG					(0x08)
#define S5PCSIS_CFG_FMT_YCBCR422_8BIT			(0x1e << 2)
#define S5PCSIS_CFG_FMT_RAW8				(0x2a << 2)
#define S5PCSIS_CFG_FMT_RAW10				(0x2b << 2)
#define S5PCSIS_CFG_FMT_RAW12				(0x2c << 2)
/* User defined formats, x = 1...4 */
#define S5PCSIS_CFG_FMT_USER(x)				((0x30 + x - 1) << 2)
#define S5PCSIS_CFG_FMT_MASK				(0x3f << 2)
#define S5PCSIS_CFG_NR_LANE_MASK			(3)

/* Interrupt mask. */
#define S5PCSIS_INTMSK					(0x10)
#define S5PCSIS_INTMSK_EN_ALL				(0xfc00103f)
#define S5PCSIS_INTSRC					(0x14)

/* Pixel resolution */
#define S5PCSIS_RESOL					(0x2c)
#define CSIS_MAX_PIX_WIDTH				(0xffff)
#define CSIS_MAX_PIX_HEIGHT				(0xffff)

static void s5pcsis_enable_interrupts(unsigned long mipi_reg_base, bool on)
{
	u32 val = readl(mipi_reg_base + S5PCSIS_INTMSK);

	val = on ? val | S5PCSIS_INTMSK_EN_ALL :
		   val & ~S5PCSIS_INTMSK_EN_ALL;
	writel(val, mipi_reg_base + S5PCSIS_INTMSK);
}

static void s5pcsis_reset(unsigned long mipi_reg_base)
{
	u32 val = readl(mipi_reg_base + S5PCSIS_CTRL);

	writel(val | S5PCSIS_CTRL_RESET, mipi_reg_base + S5PCSIS_CTRL);
	udelay(10);
}

static void s5pcsis_system_enable(unsigned long mipi_reg_base, int on)
{
	u32 val;

	val = readl(mipi_reg_base + S5PCSIS_CTRL);
	if (on) {
		val |= S5PCSIS_CTRL_ENABLE;
		val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	} else
		val &= ~S5PCSIS_CTRL_ENABLE;
	writel(val, mipi_reg_base + S5PCSIS_CTRL);

	val = readl(mipi_reg_base + S5PCSIS_DPHYCTRL);
	if (on)
		val |= S5PCSIS_DPHYCTRL_ENABLE;
	else
		val &= ~S5PCSIS_DPHYCTRL_ENABLE;
	writel(val, mipi_reg_base + S5PCSIS_DPHYCTRL);
}

/* Called with the state.lock mutex held */
static void __s5pcsis_set_format(unsigned long mipi_reg_base,
				struct fimc_is_frame_info *f_frame)
{
	u32 val;

	/* Color format */
	val = readl(mipi_reg_base + S5PCSIS_CONFIG);
	val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW10;
	writel(val, mipi_reg_base + S5PCSIS_CONFIG);

	/* Pixel resolution */
	val = (f_frame->o_width << 16) | f_frame->o_height;
	writel(val, mipi_reg_base + S5PCSIS_RESOL);
}

static void s5pcsis_set_hsync_settle(unsigned long mipi_reg_base)
{
	u32 val = readl(mipi_reg_base + S5PCSIS_DPHYCTRL);

	val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (0x6 << 28);
	writel(val, mipi_reg_base + S5PCSIS_DPHYCTRL);
}

static void s5pcsis_set_params(unsigned long mipi_reg_base,
				struct fimc_is_frame_info *f_frame)
{
	u32 val;

	val = readl(mipi_reg_base + S5PCSIS_CONFIG);
	val = (val & ~S5PCSIS_CFG_NR_LANE_MASK) | (2 - 1);
	writel(val, mipi_reg_base + S5PCSIS_CONFIG);

	__s5pcsis_set_format(mipi_reg_base, f_frame);
	s5pcsis_set_hsync_settle(mipi_reg_base);

	val = readl(mipi_reg_base + S5PCSIS_CTRL);
	val &= ~S5PCSIS_CTRL_ALIGN_32BIT;

	/* Not using external clock. */
	val &= ~S5PCSIS_CTRL_WCLK_EXTCLK;

	writel(val, mipi_reg_base + S5PCSIS_CTRL);

	/* Update the shadow register. */
	val = readl(mipi_reg_base + S5PCSIS_CTRL);
	writel(val | S5PCSIS_CTRL_UPDATE_SHADOW, mipi_reg_base + S5PCSIS_CTRL);
}

int enable_mipi(void)
{
	void __iomem *addr;
	u32 cfg;

	addr = S5P_MIPI_DPHY_CONTROL(0);

	cfg = __raw_readl(addr);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr);


	addr = S5P_MIPI_DPHY_CONTROL(1);

	cfg = __raw_readl(addr);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr);
	return 0;

}

int start_mipi_csi(int channel, struct fimc_is_frame_info *f_frame)
{
	unsigned long base_reg = (unsigned long)MIPICSI0_REG_BASE;

	if (channel == CSI_ID_A)
		base_reg = (unsigned long)MIPICSI0_REG_BASE;
	else if (channel == CSI_ID_B)
		base_reg = (unsigned long)MIPICSI1_REG_BASE;

	s5pcsis_reset(base_reg);
	s5pcsis_set_params(base_reg, f_frame);
	s5pcsis_system_enable(base_reg, true);
	s5pcsis_enable_interrupts(base_reg, true);

	return 0;
}

int stop_mipi_csi(int channel)
{
	unsigned long base_reg = (unsigned long)MIPICSI0_REG_BASE;

	if (channel == CSI_ID_A)
		base_reg = (unsigned long)MIPICSI0_REG_BASE;
	else if (channel == CSI_ID_B)
		base_reg = (unsigned long)MIPICSI1_REG_BASE;

	s5pcsis_enable_interrupts(base_reg, false);
	s5pcsis_system_enable(base_reg, false);

	return 0;
}

int fimc_is_sensor_probe(struct fimc_is_device_sensor *this,
	struct fimc_is_video_sensor *video,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_device_ischain *ischain,
	struct fimc_is_mem *mem)
{
	int ret = 0;
	struct fimc_is_enum_sensor *enum_sensor = this->enum_sensor;

	do {
		mutex_init(&this->state_barrier);

		/*sensor init*/
		this->state = 0;
		this->framemgr = framemgr;
		this->mem = mem;
		this->ischain = ischain;
		this->video = video;

		enum_sensor[SENSOR_NAME_S5K3H2].sensor = SENSOR_NAME_S5K3H2;
		enum_sensor[SENSOR_NAME_S5K3H2].pixel_width = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].pixel_height = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].active_width = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].active_height = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].max_framerate = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].csi_ch = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].flite_ch = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].i2c_ch = 0;

		enum_sensor[SENSOR_NAME_S5K6A3].sensor = SENSOR_NAME_S5K6A3;
		enum_sensor[SENSOR_NAME_S5K6A3].pixel_width = 1392 + 16;
		enum_sensor[SENSOR_NAME_S5K6A3].pixel_height = 1392 + 10;
		enum_sensor[SENSOR_NAME_S5K6A3].active_width = 1392;
		enum_sensor[SENSOR_NAME_S5K6A3].active_height = 1392;
		enum_sensor[SENSOR_NAME_S5K6A3].max_framerate = 30;
		enum_sensor[SENSOR_NAME_S5K6A3].csi_ch = 1;
		enum_sensor[SENSOR_NAME_S5K6A3].flite_ch = FLITE_ID_B;
		enum_sensor[SENSOR_NAME_S5K6A3].i2c_ch = 1;

		enum_sensor[SENSOR_NAME_S5K4E5].sensor = SENSOR_NAME_S5K4E5;
		enum_sensor[SENSOR_NAME_S5K4E5].pixel_width = 2560 + 16;
		enum_sensor[SENSOR_NAME_S5K4E5].pixel_height = 1920 + 10;
		enum_sensor[SENSOR_NAME_S5K4E5].active_width = 2560;
		enum_sensor[SENSOR_NAME_S5K4E5].active_height = 1920;
		enum_sensor[SENSOR_NAME_S5K4E5].max_framerate = 30;
		enum_sensor[SENSOR_NAME_S5K4E5].csi_ch = 0;
		enum_sensor[SENSOR_NAME_S5K4E5].flite_ch = FLITE_ID_A;
		enum_sensor[SENSOR_NAME_S5K4E5].i2c_ch = 0;

		enum_sensor[SENSOR_NAME_S5K3H7].sensor = SENSOR_NAME_S5K3H7;
		enum_sensor[SENSOR_NAME_S5K3H7].pixel_width = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].pixel_height = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].active_width = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].active_height = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].max_framerate = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].csi_ch = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].flite_ch = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].i2c_ch = 0;
	} while (0);

	fimc_is_flite_probe(&this->flite0,
		&video->common,
		framemgr,
		FLITE_ID_A,
		(u32)this);

	fimc_is_flite_probe(&this->flite1,
		&video->common,
		framemgr,
		FLITE_ID_B,
		(u32)this);

	return ret;
}

int fimc_is_sensor_open(struct fimc_is_device_sensor *this)
{
	int ret = 0;

	dbg_front("%s\n", __func__);

	this->state = 0;
	memset(&this->frame_desc, 0x0, sizeof(struct camera2_uctl));
	this->active_sensor = &this->enum_sensor[SENSOR_NAME_S5K4E5];

	fimc_is_frame_open(this->framemgr, 8);

	return ret;
}

int fimc_is_sensor_close(struct fimc_is_device_sensor *this)
{
	int ret = 0;

	fimc_is_frame_close(this->framemgr);

	return ret;
}

int fimc_is_sensor_s_active_sensor(struct fimc_is_device_sensor *this,
	u32 input)
{
	int ret = 0;

	this->active_sensor = &this->enum_sensor[input];
	if (this->active_sensor->flite_ch == FLITE_ID_A)
		this->active_flite = &this->flite0;
	else
		this->active_flite = &this->flite1;

	ret = fimc_is_flite_open(this->active_flite);

	return ret;
}

int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *this,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame_shot *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr = this->framemgr;
	frame = &framemgr->frame[index];

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_2 + index, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE)
		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	else {
		err("frame(%d) is not free state(%d)", index, frame->state);
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_2 + index, flags);

exit:
	return ret;
}

int fimc_is_sensor_buffer_finish(struct fimc_is_device_sensor *this,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame_shot *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr = this->framemgr;
	frame = &framemgr->frame[index];

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_3 + index, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_COMPLETE)
		fimc_is_frame_trans_com_to_fre(framemgr, frame);
	else {
		err("frame(%d) is not com state(%d)", index, frame->state);
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_3 + index, flags);

exit:
	return ret;
}

int fimc_is_sensor_back_start(struct fimc_is_device_sensor *this,
	struct fimc_is_video_common *video)
{
	int ret = 0;
	struct fimc_is_frame_info frame;
	struct fimc_is_enum_sensor *active_sensor;
	struct fimc_is_device_flite *active_flite;

	dbg_sensor("%s\n", __func__);

	if (!test_bit(FIMC_IS_SENSOR_BACK_START, &this->state)) {
		active_sensor = this->active_sensor;
		active_flite = this->active_flite;

		mutex_lock(&this->state_barrier);
		set_bit(FIMC_IS_SENSOR_BACK_START, &this->state);
		mutex_unlock(&this->state_barrier);

		frame.o_width = active_sensor->pixel_width;
		frame.o_height = active_sensor->pixel_height;
		frame.offs_h = 0;
		frame.offs_v = 0;
		frame.width = active_sensor->pixel_width;
		frame.height = active_sensor->pixel_height;

		/*start flite*/
		fimc_is_flite_start(active_flite, &frame, video);

		/*start mipi*/
		dbg_front("start flite (pos:%d) (port:%d) : %d x %d\n",
			active_sensor->sensor,
			active_sensor->flite_ch,
			frame.width, frame.height);
	}

	return ret;
}

int fimc_is_sensor_back_stop(struct fimc_is_device_sensor *this)
{
	int ret = 0;
	struct fimc_is_device_flite *active_flite;

	dbg_front("%s\n", __func__);

	if (test_bit(FIMC_IS_SENSOR_BACK_START, &this->state)) {
		active_flite = this->active_flite;

		mutex_lock(&this->state_barrier);
		clear_bit(FIMC_IS_SENSOR_BACK_START, &this->state);
		mutex_unlock(&this->state_barrier);

		ret = fimc_is_flite_stop(active_flite);
	}

	return ret;
}

int fimc_is_sensor_front_start(struct fimc_is_device_sensor *this)
{
	int ret = 0;
	struct fimc_is_frame_info frame;
	struct fimc_is_enum_sensor *active_sensor;

	dbg_front("%s\n", __func__);

	if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &this->state)) {
		active_sensor = this->active_sensor;

		mutex_lock(&this->state_barrier);
		set_bit(FIMC_IS_SENSOR_FRONT_START, &this->state);
		mutex_unlock(&this->state_barrier);

		frame.o_width = active_sensor->pixel_width;
		frame.o_height = active_sensor->pixel_height;
		frame.offs_h = 0;
		frame.offs_v = 0;
		frame.width = active_sensor->pixel_width;
		frame.height = active_sensor->pixel_height;

		start_mipi_csi(active_sensor->csi_ch, &frame);

		/*start mipi*/
		dbg_front("start mipi (pos:%d) (port:%d) : %d x %d\n",
			active_sensor->sensor,
			active_sensor->csi_ch,
			frame.width, frame.height);

		ret = fimc_is_itf_stream_on(this->ischain);
		if (ret)
			err("sensor stream on is failed(error %d)\n", ret);
		else
			dbg_front("sensor stream on\n");
	}

	return ret;
}

int fimc_is_sensor_front_stop(struct fimc_is_device_sensor *this)
{
	int ret = 0;
	struct fimc_is_enum_sensor *active_sensor;

	dbg_front("%s\n", __func__);

	if (test_bit(FIMC_IS_SENSOR_FRONT_START, &this->state)) {
		active_sensor = this->active_sensor;

		mutex_lock(&this->state_barrier);
		clear_bit(FIMC_IS_SENSOR_FRONT_START, &this->state);
		mutex_unlock(&this->state_barrier);

		ret = fimc_is_itf_stream_off(this->ischain);
		if (ret)
			err("sensor stream off is failed(error %d)\n", ret);
		else
			dbg_front("sensor stream off\n");

		stop_mipi_csi(active_sensor->csi_ch);
	}

	return ret;
}

