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

#include "fimc-is-device-sensor.h"

/* TODO: Apply TASKLIT */
static void fimc_is_sensor_tasklet_handler0(unsigned long data)
{
	struct fimc_is_core *dev = (struct fimc_is_core *)data;
	struct fimc_is_interface *interface = &dev->interface;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_video_sensor *video;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame_shot *item;
	u32 buf_index;

	sensor = &dev->sensor;
	video = &dev->video_sensor;
	framemgr = sensor->framemgr;
	sensor->flite.frame_count++;
	buf_index = sensor->shot_buffer_index0;

	fimc_is_frame_g_shot_from_request(framemgr, &item);

	if (item) {
		if (buf_index == item->id) {
			dbg_sensor("L%d\n", buf_index);

			item->frame_number = sensor->flite.frame_count;
			fimc_is_hw_shot(interface, 0,
				video->common.buf_dva[buf_index][0],
				video->common.buf_dva[buf_index][1],
				sensor->flite.frame_count);

			fimc_is_frame_s_shot_to_process(framemgr, item);

			vb2_buffer_done(
				video->common.vbq.bufs[buf_index],
				VB2_BUF_STATE_DONE);
		} else {
			err("buffer index is NOT matched(%d != %d)\n",
				sensor->shot_buffer_index0, item->id);
			fimc_is_frame_s_shot_to_reqest_head(framemgr, item);
		}
	} else
		err("Frame done(%d) is lost\n", buf_index);
}

DECLARE_TASKLET(sensor0_tl, fimc_is_sensor_tasklet_handler0,
						(unsigned long)NULL);

static irqreturn_t fimc_is_sensor_irq_handler0(int irq, void *dev_id)
{
	struct fimc_is_core *core = dev_id;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_flite *flite;
	struct fimc_is_video_sensor *video;
	int buf_index;
	unsigned int status1, status2, status3;

	sensor = &core->sensor;
	flite = &sensor->flite;
	video = &core->video_sensor;

	status1 = flite_hw_get_status1(sensor->regs);
	status2 = flite_hw_get_status2(sensor->regs);
	status3 = flite_hw_get_present_frame_buffer(sensor->regs);

	flite_hw_set_status1(sensor->regs, 0);

	if (status1 & (1<<4)) {
		buf_index =  status3 - 1;

#ifdef AUTO_MODE
		sensor->shot_buffer_index0 = buf_index;
		sensor0_tl.data = (unsigned long)core;
		tasklet_schedule(&sensor0_tl);
#else
		fimc_is_frame_g_shot_from_request(sensor, &item);

		if (item) {
			if (buf_index == item->id) {
				dbg_sensor("L%d\n", buf_index);
				fimc_is_frame_s_shot_to_process(sensor, item);
				vb2_buffer_done(video->vbq.bufs[buf_index],
					VB2_BUF_STATE_DONE);
			} else {
				err("buffer index is NOT matched(%d != %d)\n",
					buf_index, item->id);
				fimc_is_frame_s_shot_to_complete(sensor, item);
			}
		} else {
			err("Frame done(%d) is lost\n", buf_index);
		}
#endif
	}

	if (status1 & (1<<5))
		dbg_sensor("[CamIF_0]Frame start done\n");

	if (status1 & (0x1<<6)) {
		/*Last Frame Capture Interrupt*/
		err("[CamIF_0]Last Frame Capture\n");
		/*Clear LastCaptureEnd bit*/
		status2 &= ~(0x1 << 1);
		flite_hw_set_status2(sensor->regs, status2);

		set_bit(FIMC_IS_FLITE_LAST_CAPTURE, &flite->state);
		wake_up(&flite->wait_queue);
	}

	if (status1 & (1<<8)) {
		err("[CamIF_0]Overflow Cr\n");
		/*uCIWDOFST |= (0x1 << 14);*/
	}

	if (status1 & (1<<9)) {
		err("[CamIF_0]Overflow Cb\n");
		/*uCIWDOFST |= (0x1 << 15);*/
	}

	if (status1 & (1<<10)) {
		u32 ciwdofst;

		err("[CamIF_0]Overflow Y\n");
		ciwdofst = readl(sensor->regs + 0x10);
		ciwdofst  |= (0x1 << 30);
		writel(ciwdofst, sensor->regs + 0x10);
		/*uCIWDOFST |= (0x1 << 30);*/
	}

	return IRQ_HANDLED;
}

static irqreturn_t fimc_is_sensor_irq_handler1(int irq, void *dev_id)
{
#if 0 /* will impement for front camera*/
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

			dbg_sensor("L%d\n", status3);
		}

		vb2_buffer_done(video->vbq.bufs[buf_index],
			VB2_BUF_STATE_DONE);
	}

	if (status1 & (1<<5))
		dbg_sensor("[CamIF_1]Frame start done\n");

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
#endif
	return IRQ_HANDLED;
}

int fimc_is_sensor_probe(struct fimc_is_device_sensor *this,
	void *data)
{
	int ret;
	struct fimc_is_core *core = (struct fimc_is_core *)data;

	do {
		this->flite.frame_count = 0;
		this->framemgr = &core->framemgr;
		this->interface = &core->interface;

		ret = request_irq(IRQ_FIMC_LITE0,
			fimc_is_sensor_irq_handler0,
			0,
			"FIMC-LITE0",
			core);
		if (ret) {
			err("request_irq(L0) failed\n");
			break;
		}

		ret = request_irq(IRQ_FIMC_LITE1,
			fimc_is_sensor_irq_handler1,
			0,
			"FIMC-LITE1",
			core);
		if (ret) {
			err("request_irq(L1) failed\n");
			break;
		}
	} while (0);

	return ret;
}

int fimc_is_sensor_open(struct fimc_is_device_sensor *this)
{
	dbg_chain("%s\n", __func__);

	this->enum_sensor[SENSOR_NAME_S5K3H2].sensor = SENSOR_NAME_S5K3H2;
	this->enum_sensor[SENSOR_NAME_S5K3H2].width = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].height = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].margin_x = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].margin_x = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].max_framerate = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].csi_ch = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].flite_ch = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H2].i2c_ch = 0;

	this->enum_sensor[SENSOR_NAME_S5K6A3].sensor = SENSOR_NAME_S5K6A3;
	this->enum_sensor[SENSOR_NAME_S5K6A3].width = 1280;
	this->enum_sensor[SENSOR_NAME_S5K6A3].height = 720;
	this->enum_sensor[SENSOR_NAME_S5K6A3].margin_x = 16;
	this->enum_sensor[SENSOR_NAME_S5K6A3].margin_x = 10;
	this->enum_sensor[SENSOR_NAME_S5K6A3].max_framerate = 30;
	this->enum_sensor[SENSOR_NAME_S5K6A3].csi_ch = 1;
	this->enum_sensor[SENSOR_NAME_S5K6A3].flite_ch = 1;
	this->enum_sensor[SENSOR_NAME_S5K6A3].i2c_ch = 1;

	this->enum_sensor[SENSOR_NAME_S5K4E5].sensor = SENSOR_NAME_S5K4E5;
	this->enum_sensor[SENSOR_NAME_S5K4E5].width = 2560;
	this->enum_sensor[SENSOR_NAME_S5K4E5].height = 1920;
	this->enum_sensor[SENSOR_NAME_S5K4E5].margin_x = 16;
	this->enum_sensor[SENSOR_NAME_S5K4E5].margin_x = 10;
	this->enum_sensor[SENSOR_NAME_S5K4E5].max_framerate = 30;
	this->enum_sensor[SENSOR_NAME_S5K4E5].csi_ch = 0;
	this->enum_sensor[SENSOR_NAME_S5K4E5].flite_ch = 0;
	this->enum_sensor[SENSOR_NAME_S5K4E5].i2c_ch = 0;

	this->enum_sensor[SENSOR_NAME_S5K3H7].sensor = SENSOR_NAME_S5K3H7;
	this->enum_sensor[SENSOR_NAME_S5K3H7].width = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].height = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].margin_x = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].margin_x = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].max_framerate = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].csi_ch = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].flite_ch = 0;
	this->enum_sensor[SENSOR_NAME_S5K3H7].i2c_ch = 0;

	this->active_sensor = &this->enum_sensor[SENSOR_NAME_S5K4E5];
	this->streaming = false;

	set_bit(FIMC_IS_FLITE_INIT, &this->flite.state);
	init_waitqueue_head(&this->flite.wait_queue);

	return 0;
}

int fimc_is_sensor_close(struct fimc_is_device_sensor *this)
{
	int ret = 0;
	struct fimc_is_device_flite *flite = &this->flite;

	dbg_chain("%s\n", __func__);

	stop_fimc_lite(this->active_sensor->flite_ch);
	stop_mipi_csi(this->active_sensor->csi_ch);

	dbg_chain("waiting last capture\n");
	ret = wait_event_timeout(flite->wait_queue,
		test_bit(FIMC_IS_FLITE_LAST_CAPTURE, &flite->state),
		FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR);
	if (!ret) {
		err("last capture timeout:%s\n", __func__);
		stop_fimc_lite(this->active_sensor->flite_ch);
		stop_mipi_csi(this->active_sensor->csi_ch);
		msleep(60);
	}

	clear_bit(FIMC_IS_FLITE_INIT, &flite->state);
	clear_bit(FIMC_IS_FLITE_START, &flite->state);
	clear_bit(FIMC_IS_FLITE_LAST_CAPTURE, &flite->state);

	return 0;
}

int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *sensor,
	struct fimc_is_video_common *video, u32 index)
{
	struct fimc_is_frame_shot *item;
	struct fimc_is_framemgr *framemgr = sensor->framemgr;

	fimc_is_frame_g_shot_from_free(framemgr, &item);

	if (item) {
		item->dvaddr_buffer = (u32)video->buf_dva[index][0];
		item->dvaddr_shot = (u32)video->buf_dva[index][1];

		fimc_is_frame_s_shot_to_reqest_tail(framemgr, item);
	} else {
		fimc_is_frame_print_request_list(framemgr);
		fimc_is_frame_print_process_list(framemgr);
	}

	return 0;
}

int fimc_is_sensor_start_streaming(struct fimc_is_device_sensor *this,
	struct fimc_is_video_common *video)
{
	u32 i, buf_index;
	u32 width, height;
	struct fimc_is_flite_frame frame;

	width = this->active_sensor->width;
	height = this->active_sensor->height;

	init_fimc_lite(this->regs);

	for (i = 0; i < video->buffers; i++) {
		buf_index = i*video->frame.format.num_planes;
		dbg_chain("(%d)set buf(%d) = 0x%08x\n",
			buf_index, i, video->buf_dva[i][0]);

		flite_hw_set_use_buffer(this->regs, (i+1));
		flite_hw_set_start_addr(this->regs, (i+1),
			video->buf_dva[i][0]);

		fimc_is_hw_cfg_mem(this->interface, 0,
			video->buf_dva[i][1], 5*1024);
	}

	/*flite_hw_set_use_buffer(this->regs, 1);*/
	flite_hw_set_output_dma(this->regs, true);
	flite_hw_set_output_local(this->regs, false);

	frame.o_width = width + 16;
	frame.o_height = height + 10;
	frame.offs_h = 0;
	frame.offs_v = 0;
	frame.width = width + 16;
	frame.height = height + 10;

	dbg_chain("start_fimc(%dx%d)",
		(width + 16),
		(height + 10));

	start_fimc_lite(this->regs, &frame);

	/*start mipi*/
	dbg_chain("start mipi (pos:%d) (port:%d) : %d x %d\n",
		this->active_sensor->sensor,
		this->active_sensor->csi_ch,
		frame.width, frame.height);
	start_mipi_csi(this->active_sensor->csi_ch, &frame);

	/*fimc_is_hw_process_on(interface, 0);*/

	return 0;
}

int device_sensor_stream_on(struct fimc_is_device_sensor *sensor,
	struct fimc_is_interface *interface)
{
	return fimc_is_hw_stream_on(interface, 0);
}

int device_sensor_stream_off(struct fimc_is_device_sensor *sensor,
	struct fimc_is_interface *interface)
{
	return fimc_is_hw_stream_off(interface, 0);
}

/* PMU for FIMC-IS*/
#define FIMCLITE0_REG_BASE	(S5P_VA_FIMCLITE0)  /* phy : 0x13c0_0000 */
#define FIMCLITE1_REG_BASE	(S5P_VA_FIMCLITE1)  /* phy : 0x13c1_0000 */
#define MIPICSI0_REG_BASE	(S5P_VA_MIPICSI0)   /* phy : 0x13c2_0000 */
#define MIPICSI1_REG_BASE	(S5P_VA_MIPICSI1)   /* phy : 0x13c3_0000 */
#define FIMCLITE2_REG_BASE	(S5P_VA_FIMCLITE2)  /* phy : 0x13c9_0000 */

#define FLITE_MAX_RESET_READY_TIME	(20) /* 100ms */
#define FLITE_MAX_WIDTH_SIZE		(8192)
#define FLITE_MAX_HEIGHT_SIZE		(8192)


/*FIMCLite*/
/* Camera Source size */
#define FLITE_REG_CISRCSIZE				(0x00)
#define FLITE_REG_CISRCSIZE_SIZE_H(x)			((x) << 16)
#define FLITE_REG_CISRCSIZE_SIZE_V(x)			((x) << 0)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_YCBYCR		(0 << 14)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_YCRYCB		(1 << 14)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_CBYCRY		(2 << 14)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_CRYCBY		(3 << 14)

/* Global control */
#define FLITE_REG_CIGCTRL				0x04
#define FLITE_REG_CIGCTRL_YUV422_1P			(0x1E << 24)
#define FLITE_REG_CIGCTRL_RAW8				(0x2A << 24)
#define FLITE_REG_CIGCTRL_RAW10				(0x2B << 24)
#define FLITE_REG_CIGCTRL_RAW12				(0x2C << 24)
#define FLITE_REG_CIGCTRL_RAW14				(0x2D << 24)

/* User defined formats. x = 0...0xF. */
#define FLITE_REG_CIGCTRL_USER(x)			(0x30 + x - 1)
#define FLITE_REG_CIGCTRL_OLOCAL_DISABLE	(1 << 22)
#define FLITE_REG_CIGCTRL_SHADOWMASK_DISABLE		(1 << 21)
#define FLITE_REG_CIGCTRL_ODMA_DISABLE			(1 << 20)
#define FLITE_REG_CIGCTRL_SWRST_REQ			(1 << 19)
#define FLITE_REG_CIGCTRL_SWRST_RDY			(1 << 18)
#define FLITE_REG_CIGCTRL_SWRST				(1 << 17)
#define FLITE_REG_CIGCTRL_TEST_PATTERN_COLORBAR		(1 << 15)
#define FLITE_REG_CIGCTRL_INVPOLPCLK			(1 << 14)
#define FLITE_REG_CIGCTRL_INVPOLVSYNC			(1 << 13)
#define FLITE_REG_CIGCTRL_INVPOLHREF			(1 << 12)
#define FLITE_REG_CIGCTRL_IRQ_LASTEN0_ENABLE		(0 << 8)
#define FLITE_REG_CIGCTRL_IRQ_LASTEN0_DISABLE		(1 << 8)
#define FLITE_REG_CIGCTRL_IRQ_ENDEN0_ENABLE		(0 << 7)
#define FLITE_REG_CIGCTRL_IRQ_ENDEN0_DISABLE		(1 << 7)
#define FLITE_REG_CIGCTRL_IRQ_STARTEN0_ENABLE		(0 << 6)
#define FLITE_REG_CIGCTRL_IRQ_STARTEN0_DISABLE		(1 << 6)
#define FLITE_REG_CIGCTRL_IRQ_OVFEN0_ENABLE		(0 << 5)
#define FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE		(1 << 5)
#define FLITE_REG_CIGCTRL_SELCAM_MIPI			(1 << 3)

/* Image Capture Enable */
#define FLITE_REG_CIIMGCPT				(0x08)
#define FLITE_REG_CIIMGCPT_IMGCPTEN			(1 << 31)
#define FLITE_REG_CIIMGCPT_CPT_FREN			(1 << 25)
#define FLITE_REG_CIIMGCPT_CPT_FRPTR(x)			((x) << 19)
#define FLITE_REG_CIIMGCPT_CPT_MOD_FRCNT		(1 << 18)
#define FLITE_REG_CIIMGCPT_CPT_MOD_FREN			(0 << 18)
#define FLITE_REG_CIIMGCPT_CPT_FRCNT(x)			((x) << 10)

/* Capture Sequence */
#define FLITE_REG_CICPTSEQ				(0x0C)
#define FLITE_REG_CPT_FRSEQ(x)				((x) << 0)

/* Camera Window Offset */
#define FLITE_REG_CIWDOFST				(0x10)
#define FLITE_REG_CIWDOFST_WINOFSEN			(1 << 31)
#define FLITE_REG_CIWDOFST_CLROVIY			(1 << 31)
#define FLITE_REG_CIWDOFST_WINHOROFST(x)		((x) << 16)
#define FLITE_REG_CIWDOFST_HOROFF_MASK			(0x1fff << 16)
#define FLITE_REG_CIWDOFST_CLROVFICB			(1 << 15)
#define FLITE_REG_CIWDOFST_CLROVFICR			(1 << 14)
#define FLITE_REG_CIWDOFST_WINVEROFST(x)		((x) << 0)
#define FLITE_REG_CIWDOFST_VEROFF_MASK			(0x1fff << 0)

/* Cmaera Window Offset2 */
#define FLITE_REG_CIWDOFST2				(0x14)
#define FLITE_REG_CIWDOFST2_WINHOROFST2(x)		((x) << 16)
#define FLITE_REG_CIWDOFST2_WINVEROFST2(x)		((x) << 0)

/* Camera Output DMA Format */
#define FLITE_REG_CIODMAFMT				(0x18)
#define FLITE_REG_CIODMAFMT_1D_DMA			(1 << 15)
#define FLITE_REG_CIODMAFMT_2D_DMA			(0 << 15)
#define FLITE_REG_CIODMAFMT_PACK12			(1 << 14)
#define FLITE_REG_CIODMAFMT_NORMAL			(0 << 14)
#define FLITE_REG_CIODMAFMT_CRYCBY			(0 << 4)
#define FLITE_REG_CIODMAFMT_CBYCRY			(1 << 4)
#define FLITE_REG_CIODMAFMT_YCRYCB			(2 << 4)
#define FLITE_REG_CIODMAFMT_YCBYCR			(3 << 4)

/* Camera Output Canvas */
#define FLITE_REG_CIOCAN				(0x20)
#define FLITE_REG_CIOCAN_OCAN_V(x)			((x) << 16)
#define FLITE_REG_CIOCAN_OCAN_H(x)			((x) << 0)

/* Camera Output DMA Offset */
#define FLITE_REG_CIOOFF				(0x24)
#define FLITE_REG_CIOOFF_OOFF_V(x)			((x) << 16)
#define FLITE_REG_CIOOFF_OOFF_H(x)			((x) << 0)

/* Camera Output DMA Address */
#define FLITE_REG_CIOSA					(0x30)
#define FLITE_REG_CIOSA_OSA(x)				((x) << 0)

/* Camera Status */
#define FLITE_REG_CISTATUS				(0x40)
#define FLITE_REG_CISTATUS_MIPI_VVALID			(1 << 22)
#define FLITE_REG_CISTATUS_MIPI_HVALID			(1 << 21)
#define FLITE_REG_CISTATUS_MIPI_DVALID			(1 << 20)
#define FLITE_REG_CISTATUS_ITU_VSYNC			(1 << 14)
#define FLITE_REG_CISTATUS_ITU_HREFF			(1 << 13)
#define FLITE_REG_CISTATUS_OVFIY			(1 << 10)
#define FLITE_REG_CISTATUS_OVFICB			(1 << 9)
#define FLITE_REG_CISTATUS_OVFICR			(1 << 8)
#define FLITE_REG_CISTATUS_IRQ_SRC_OVERFLOW		(1 << 7)
#define FLITE_REG_CISTATUS_IRQ_SRC_LASTCAPEND		(1 << 6)
#define FLITE_REG_CISTATUS_IRQ_SRC_FRMSTART		(1 << 5)
#define FLITE_REG_CISTATUS_IRQ_SRC_FRMEND		(1 << 4)
#define FLITE_REG_CISTATUS_IRQ_CAM			(1 << 0)
#define FLITE_REG_CISTATUS_IRQ_MASK			(0xf << 4)

/* Camera Status2 */
#define FLITE_REG_CISTATUS2				(0x44)
#define FLITE_REG_CISTATUS2_LASTCAPEND			(1 << 1)
#define FLITE_REG_CISTATUS2_FRMEND			(1 << 0)

/* Camera Status3 */
#define FLITE_REG_CISTATUS3				0x48
#define FLITE_REG_CISTATUS3_PRESENT_MASK (0x3F)

/* Qos Threshold */
#define FLITE_REG_CITHOLD				(0xF0)
#define FLITE_REG_CITHOLD_W_QOS_EN			(1 << 30)
#define FLITE_REG_CITHOLD_WTH_QOS(x)			((x) << 0)

/* Camera General Purpose */
#define FLITE_REG_CIGENERAL				(0xFC)
#define FLITE_REG_CIGENERAL_CAM_A			(0 << 0)
#define FLITE_REG_CIGENERAL_CAM_B			(1 << 0)

#define FLITE_REG_CIFCNTSEQ				0x100

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

static void flite_hw_set_cam_source_size(unsigned long flite_reg_base,
					struct fimc_is_flite_frame *f_frame)
{
	u32 cfg = 0;

	cfg = readl(flite_reg_base + FLITE_REG_CISRCSIZE);

	cfg |= FLITE_REG_CISRCSIZE_SIZE_H(f_frame->o_width);
	cfg |= FLITE_REG_CISRCSIZE_SIZE_V(f_frame->o_height);

	writel(cfg, flite_reg_base + FLITE_REG_CISRCSIZE);

	cfg = readl(flite_reg_base + FLITE_REG_CIOCAN);
	cfg |= FLITE_REG_CIOCAN_OCAN_H(f_frame->o_width);
	cfg |= FLITE_REG_CIOCAN_OCAN_V(f_frame->o_height);

	writel(cfg, flite_reg_base + FLITE_REG_CIOCAN);
}

static void flite_hw_set_cam_channel(unsigned long flite_reg_base)
{
	u32 cfg = 0;

	if (flite_reg_base == (unsigned long)FIMCLITE0_REG_BASE) {
		cfg = FLITE_REG_CIGENERAL_CAM_A;
		writel(cfg, FIMCLITE0_REG_BASE + FLITE_REG_CIGENERAL);
		writel(cfg, FIMCLITE1_REG_BASE + FLITE_REG_CIGENERAL);
		writel(cfg, FIMCLITE2_REG_BASE + FLITE_REG_CIGENERAL);
	} else {
		cfg = FLITE_REG_CIGENERAL_CAM_B;
		writel(cfg, FIMCLITE0_REG_BASE + FLITE_REG_CIGENERAL);
		writel(cfg, FIMCLITE1_REG_BASE + FLITE_REG_CIGENERAL);
		writel(cfg, FIMCLITE2_REG_BASE + FLITE_REG_CIGENERAL);
	}
}

void flite_hw_set_capture_start(unsigned long flite_reg_base)
{
	u32 cfg = 0;

	cfg = readl(flite_reg_base + FLITE_REG_CIIMGCPT);
	cfg |= FLITE_REG_CIIMGCPT_IMGCPTEN;

	writel(cfg, flite_reg_base + FLITE_REG_CIIMGCPT);
}

static void flite_hw_set_capture_stop(unsigned long flite_reg_base)
{
	u32 cfg = 0;

	cfg = readl(flite_reg_base + FLITE_REG_CIIMGCPT);
	cfg &= ~FLITE_REG_CIIMGCPT_IMGCPTEN;

	writel(cfg, flite_reg_base + FLITE_REG_CIIMGCPT);
}

static int flite_hw_set_source_format(unsigned long flite_reg_base)
{
	u32 cfg = 0;

	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);
	cfg |= FLITE_REG_CIGCTRL_RAW10;
	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);

	return 0;
}

void flite_hw_set_output_dma(unsigned long flite_reg_base, bool enable)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);

	if (enable)
		cfg &= ~FLITE_REG_CIGCTRL_ODMA_DISABLE;
	else
		cfg |= FLITE_REG_CIGCTRL_ODMA_DISABLE;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}

void flite_hw_set_output_local(unsigned long flite_reg_base, bool enable)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);

	if (enable)
		cfg &= ~FLITE_REG_CIGCTRL_OLOCAL_DISABLE;
	else
		cfg |= FLITE_REG_CIGCTRL_OLOCAL_DISABLE;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}

/* will use for pattern generation testing
static void flite_hw_set_test_pattern_enable(void)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);
	cfg |= FLITE_REG_CIGCTRL_TEST_PATTERN_COLORBAR;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}
*/

static void flite_hw_set_config_irq(unsigned long flite_reg_base)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);
	cfg &= ~(FLITE_REG_CIGCTRL_INVPOLPCLK | FLITE_REG_CIGCTRL_INVPOLVSYNC
			| FLITE_REG_CIGCTRL_INVPOLHREF);

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}

static void flite_hw_set_interrupt_source(unsigned long flite_reg_base)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);

	/*
	for checking stop complete
	*/
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_LASTEN0_DISABLE;

	/*
	for checking frame done
	*/
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_ENDEN0_DISABLE;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}

static void flite_hw_set_interrupt_starten0_disable
					(unsigned long flite_reg_base)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);
	cfg |= FLITE_REG_CIGCTRL_IRQ_STARTEN0_DISABLE;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}

static void flite_hw_set_camera_type(unsigned long flite_reg_base)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);

	cfg |= FLITE_REG_CIGCTRL_SELCAM_MIPI;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}

static void flite_hw_set_window_offset(unsigned long flite_reg_base,
					struct fimc_is_flite_frame *f_frame)
{
	u32 cfg = 0;
	u32 hoff2, voff2;

	cfg = readl(flite_reg_base + FLITE_REG_CIWDOFST);
	cfg &= ~(FLITE_REG_CIWDOFST_HOROFF_MASK |
		FLITE_REG_CIWDOFST_VEROFF_MASK);
	cfg |= FLITE_REG_CIWDOFST_WINOFSEN |
		FLITE_REG_CIWDOFST_WINHOROFST(f_frame->offs_h) |
		FLITE_REG_CIWDOFST_WINVEROFST(f_frame->offs_v);

	writel(cfg, flite_reg_base + FLITE_REG_CIWDOFST);

	hoff2 = f_frame->o_width - f_frame->width - f_frame->offs_h;
	voff2 = f_frame->o_height - f_frame->height - f_frame->offs_v;
	cfg = FLITE_REG_CIWDOFST2_WINHOROFST2(hoff2) |
		FLITE_REG_CIWDOFST2_WINVEROFST2(voff2);

	writel(cfg, flite_reg_base + FLITE_REG_CIWDOFST2);
}

static void flite_hw_set_last_capture_end_clear(unsigned long flite_reg_base)
{
	u32 cfg = 0;

	cfg = readl(flite_reg_base + FLITE_REG_CISTATUS2);
	cfg &= ~FLITE_REG_CISTATUS2_LASTCAPEND;

	writel(cfg, flite_reg_base + FLITE_REG_CISTATUS2);
}

int flite_hw_get_present_frame_buffer(unsigned long flite_reg_base)
{
	u32 status = 0;

	status = readl(flite_reg_base + FLITE_REG_CISTATUS3);
	status &= FLITE_REG_CISTATUS3_PRESENT_MASK;

	return status;
}

int flite_hw_get_status2(unsigned long flite_reg_base)
{
	u32 status = 0;

	status = readl(flite_reg_base + FLITE_REG_CISTATUS2);

	return status;
}

void flite_hw_set_status1(unsigned long flite_reg_base, u32 val)
{
	writel(val, flite_reg_base + FLITE_REG_CISTATUS);
}

int flite_hw_get_status1(unsigned long flite_reg_base)
{
	u32 status = 0;

	status = readl(flite_reg_base + FLITE_REG_CISTATUS);

	return status;
}

void flite_hw_set_status2(unsigned long flite_reg_base, u32 val)
{
	writel(val, flite_reg_base + FLITE_REG_CISTATUS2);
}

void flite_hw_set_start_addr(unsigned long flite_reg_base, u32 number, u32 addr)
{
	u32 target;
	if (number == 1) {
		target = flite_reg_base + 0x30;
	} else if (number >= 2) {
		number -= 2;
		target = flite_reg_base + 0x200 + (0x4*number);
	} else
		target = 0;

	writel(addr, target);
}

void flite_hw_set_use_buffer(unsigned long flite_reg_base, u32 number)
{
	u32 buffer;
	buffer = readl(flite_reg_base + FLITE_REG_CIFCNTSEQ);
	buffer |= 1<<(number-1);
	writel(buffer, flite_reg_base + FLITE_REG_CIFCNTSEQ);
}

void flite_hw_set_unuse_buffer(unsigned long flite_reg_base, u32 number)
{
	u32 buffer;
	buffer = readl(flite_reg_base + FLITE_REG_CIFCNTSEQ);
	buffer &= ~(1<<(number-1));
	writel(buffer, flite_reg_base + FLITE_REG_CIFCNTSEQ);
}

u32 flite_hw_get_buffer_seq(unsigned long flite_reg_base)
{
	u32 buffer;
	buffer = readl(flite_reg_base + FLITE_REG_CIFCNTSEQ);
	return buffer;
}


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
				struct fimc_is_flite_frame *f_frame)
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
				struct fimc_is_flite_frame *f_frame)
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

int init_fimc_lite(unsigned long mipi_reg_base)
{
	int i;

	writel(0, mipi_reg_base + FLITE_REG_CIFCNTSEQ);

	for (i = 0; i < 32; i++)
		flite_hw_set_start_addr(mipi_reg_base , (i+1), 0xffffffff);

	return 0;
}

int start_fimc_lite(unsigned long mipi_reg_base,
	struct fimc_is_flite_frame *f_frame)
{

	flite_hw_set_cam_channel(mipi_reg_base);
	flite_hw_set_cam_source_size(mipi_reg_base, f_frame);
	flite_hw_set_camera_type(mipi_reg_base);
	flite_hw_set_source_format(mipi_reg_base);

	flite_hw_set_interrupt_source(mipi_reg_base);
	flite_hw_set_interrupt_starten0_disable(mipi_reg_base);
	flite_hw_set_config_irq(mipi_reg_base);
	flite_hw_set_window_offset(mipi_reg_base, f_frame);

	flite_hw_set_last_capture_end_clear(mipi_reg_base);
	flite_hw_set_capture_start(mipi_reg_base);

	return 0;
}

int stop_fimc_lite(int channel)
{
	unsigned long base_reg = (unsigned long)FIMCLITE0_REG_BASE;

	if (channel == FLITE_ID_A)
		base_reg = (unsigned long)FIMCLITE0_REG_BASE;
	else if (channel == FLITE_ID_B)
		base_reg = (unsigned long)FIMCLITE1_REG_BASE;

	flite_hw_set_capture_stop(base_reg);
	return 0;
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

int start_mipi_csi(int channel, struct fimc_is_flite_frame *f_frame)
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

/*
* will be move to setting file
*/
