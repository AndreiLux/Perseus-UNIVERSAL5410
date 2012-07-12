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
				struct fimc_is_csi_frame *f_frame)
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
				struct fimc_is_csi_frame *f_frame)
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

int start_mipi_csi(int channel, struct fimc_is_csi_frame *f_frame)
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

#if 0
static irqreturn_t fimc_is_sensor_irq_handler1(int irq, void *dev_id)
{
	struct fimc_is_core *is = dev_id;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_video_sensor *video;
	int buf_index;
	unsigned int status1, status2, status3;
	u32 cfg;
	u32 status;

	sensor = &is->sensor;
	video = &is->video_sensor;

	status1 = flite_hw_get_status1(sensor->regs);
	status2 = flite_hw_get_status2(sensor->regs);
	status3 = flite_hw_get_present_frame_buffer(sensor->regs);

	flite_hw_set_status1(sensor->regs, 0);

	if (status1 & (1<<4)) {
		buf_index =  status3 - 1;
		if (sensor->streaming) {
			spin_lock(&is->mcu_slock);
			/*fimc_is_hw_wait_intmsr0_intmsd0(is);*/

			cfg = readl(is->regs + INTMSR0);
			status = INTMSR0_GET_INTMSD0(cfg);
			if (status) {
				err("[MAIN] INTMSR1's 0 bit is not cleared.\n");
				cfg = readl(is->regs + INTMSR0);
				status = INTMSR0_GET_INTMSD0(cfg);
			}

			writel(HIC_SHOT, is->regs + ISSR0);
			writel(sensor->id_dual, is->regs + ISSR1);
			writel(video->buf[buf_index][0],
				is->regs + ISSR2);
			fimc_is_hw_set_intgr0_gd0(is);
			spin_unlock(&is->mcu_slock);

			dbg_front("L%d\n", status3);
		}

		vb2_buffer_done(video->vbq.bufs[buf_index],
			VB2_BUF_STATE_DONE);
	}

	if (status1 & (1<<5))
		dbg_front("[CamIF_1]Frame start done\n");

	if (status1 & (0x1<<6)) {
		/*Last Frame Capture Interrupt*/
		err("[CamIF_1]Last Frame Capture\n");
		/*Clear LastCaptureEnd bit*/
		sensor->last_capture = true;
		status2 &= ~(0x1 << 1);
		flite_hw_set_status2(sensor->regs, status2);
	}

	if (status1 & (1<<8)) {
		err("[CamIF_1]Overflow Cr\n");
		/*uCIWDOFST |= (0x1 << 14);*/
	}

	if (status1 & (1<<9)) {
		err("[CamIF_1]Overflow Cb\n");
		/*uCIWDOFST |= (0x1 << 15);*/
	}

	if (status1 & (1<<10)) {
		u32 ciwdofst;

		err("[CamIF_1]Overflow Y\n");
		ciwdofst = readl(sensor->regs + 0x10);
		ciwdofst  |= (0x1 << 30);
		writel(ciwdofst, sensor->regs + 0x10);
		/*uCIWDOFST |= (0x1 << 30);*/
	}

	return IRQ_HANDLED;
}
#endif

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
		enum_sensor[SENSOR_NAME_S5K3H2].width = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].height = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].margin_x = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].margin_x = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].max_framerate = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].csi_ch = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].flite_ch = 0;
		enum_sensor[SENSOR_NAME_S5K3H2].i2c_ch = 0;

		enum_sensor[SENSOR_NAME_S5K6A3].sensor = SENSOR_NAME_S5K6A3;
		enum_sensor[SENSOR_NAME_S5K6A3].width = 1280;
		enum_sensor[SENSOR_NAME_S5K6A3].height = 720;
		enum_sensor[SENSOR_NAME_S5K6A3].margin_x = 16;
		enum_sensor[SENSOR_NAME_S5K6A3].margin_x = 10;
		enum_sensor[SENSOR_NAME_S5K6A3].max_framerate = 30;
		enum_sensor[SENSOR_NAME_S5K6A3].csi_ch = 1;
		enum_sensor[SENSOR_NAME_S5K6A3].flite_ch = 1;
		enum_sensor[SENSOR_NAME_S5K6A3].i2c_ch = 1;

		enum_sensor[SENSOR_NAME_S5K4E5].sensor = SENSOR_NAME_S5K4E5;
		enum_sensor[SENSOR_NAME_S5K4E5].width = 2560;
		enum_sensor[SENSOR_NAME_S5K4E5].height = 1920;
		enum_sensor[SENSOR_NAME_S5K4E5].margin_x = 16;
		enum_sensor[SENSOR_NAME_S5K4E5].margin_x = 10;
		enum_sensor[SENSOR_NAME_S5K4E5].max_framerate = 30;
		enum_sensor[SENSOR_NAME_S5K4E5].csi_ch = 0;
		enum_sensor[SENSOR_NAME_S5K4E5].flite_ch = 0;
		enum_sensor[SENSOR_NAME_S5K4E5].i2c_ch = 0;

		enum_sensor[SENSOR_NAME_S5K3H7].sensor = SENSOR_NAME_S5K3H7;
		enum_sensor[SENSOR_NAME_S5K3H7].width = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].height = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].margin_x = 0;
		enum_sensor[SENSOR_NAME_S5K3H7].margin_x = 0;
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

	ret = fimc_is_flite_open(&this->flite0);

	return ret;
}

int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *this,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame_shot *item;
	struct fimc_is_framemgr *framemgr = this->framemgr;
	struct fimc_is_device_ischain *ischain = this->ischain;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	spin_lock_irqsave(&framemgr->slock, flags);

	item = &framemgr->shot[index];

	if (item->state == FIMC_IS_SHOT_STATE_FREE) {
		memcpy(&ischain->req_frame_desc.sensor,
			&item->shot->ctl.sensor,
			sizeof(struct camera2_sensor_ctl));

		fimc_is_frame_trans_free_to_req(framemgr, item);
	} else {
		err("item(%d) is not free state(%d)\n", index, item->state);
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	spin_unlock_irqrestore(&framemgr->slock, flags);

exit:
	return ret;
}

int fimc_is_sensor_buffer_finish(struct fimc_is_device_sensor *this,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame_shot *item;
	struct fimc_is_framemgr *framemgr = this->framemgr;

	spin_lock_irqsave(&framemgr->slock, flags);

	fimc_is_frame_complete_head(framemgr, &item);
	if (item) {
		if (index == item->id)
			fimc_is_frame_trans_com_to_free(framemgr, item);
		else
			dbg_warning("buffer index is NOT matched(%d != %d)\n",
				index, item->id);
	} else {
		err("complete item is empty\n");
		fimc_is_frame_print_free_list(framemgr);
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
		fimc_is_frame_print_complete_list(framemgr);
	}

	spin_unlock_irqrestore(&framemgr->slock, flags);

	return ret;
}

int fimc_is_sensor_back_start(struct fimc_is_device_sensor *this,
	struct fimc_is_video_common *video)
{
	u32 width, height;
	struct fimc_is_csi_frame frame;

	dbg_sensor("%s\n", __func__);

	if (!test_bit(FIMC_IS_SENSOR_BACK_START, &this->state)) {
		mutex_lock(&this->state_barrier);
		set_bit(FIMC_IS_SENSOR_BACK_START, &this->state);
		mutex_unlock(&this->state_barrier);

		width = this->active_sensor->width;
		height = this->active_sensor->height;

		/*start flite*/
		fimc_is_flite_start(&this->flite0, video, width, height);

		/*start mipi*/
		dbg_front("start mipi (pos:%d) (port:%d) : %d x %d\n",
			this->active_sensor->sensor,
			this->active_sensor->csi_ch,
			width, height);

		frame.o_width = width + 16;
		frame.o_height = height + 10;
		frame.offs_h = 0;
		frame.offs_v = 0;
		frame.width = width + 16;
		frame.height = height + 10;

		start_mipi_csi(this->active_sensor->csi_ch, &frame);
	}

	return 0;
}

int fimc_is_sensor_back_stop(struct fimc_is_device_sensor *this)
{
	int ret = 0;

	dbg_front("%s\n", __func__);

	if (test_bit(FIMC_IS_SENSOR_BACK_START, &this->state)) {
		mutex_lock(&this->state_barrier);
		clear_bit(FIMC_IS_SENSOR_BACK_START, &this->state);
		mutex_unlock(&this->state_barrier);

		ret = fimc_is_flite_stop(&this->flite0);

		stop_mipi_csi(this->active_sensor->csi_ch);
	}

	return ret;
}

int fimc_is_sensor_front_start(struct fimc_is_device_sensor *this)
{
	int ret = 0;

	dbg_front("%s\n", __func__);

	mutex_lock(&this->state_barrier);
	set_bit(FIMC_IS_SENSOR_FRONT_START, &this->state);
	mutex_unlock(&this->state_barrier);

	ret = fimc_is_itf_stream_on(this->ischain);

	dbg_front("result : %d\n", ret);

	return ret;
}

int fimc_is_sensor_front_stop(struct fimc_is_device_sensor *this)
{
	int ret = 0;

	dbg_front("%s\n", __func__);

	mutex_lock(&this->state_barrier);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &this->state);
	mutex_unlock(&this->state_barrier);

	ret = fimc_is_itf_stream_off(this->ischain);

	dbg_front("result : %d\n", ret);

	return ret;
}

int fimc_is_sensor_callback(struct fimc_is_device_sensor *this,
	u32 fcount)
{
	int ret = 0;
	u64 fduration;
	u64 exposure;
	u32 sensitivity;

	fimc_is_itf_stream_ctl(this->ischain,
		fcount,
		&fduration,
		&exposure,
		&sensitivity);

	return ret;
}

