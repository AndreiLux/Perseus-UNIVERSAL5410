#include <linux/workqueue.h>

#include "fimc-is-core.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"

#include "fimc-is-helper.h"
#include "fimc-is-err.h"

#include "fimc-is-interface.h"

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
		.format = OTF_OUTPUT_FORMAT_YUV422,
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

static int init_request_barrier(struct fimc_is_interface *interface)
{
	mutex_init(&interface->request_barrier);

	return 0;
}

static int enter_request_barrier(struct fimc_is_interface *interface)
{
	mutex_lock(&interface->request_barrier);

	return 0;
}

static int exit_request_barrier(struct fimc_is_interface *interface)
{
	mutex_unlock(&interface->request_barrier);

	return 0;
}

static int init_process_barrier(struct fimc_is_interface *interface)
{
	spin_lock_init(&interface->process_barrier);

	return 0;
}

static int enter_process_barrier(struct fimc_is_interface *interface)
{
	spin_lock_irq(&interface->process_barrier);

	return 0;
}

static int exit_process_barrier(struct fimc_is_interface *interface)
{
	spin_unlock_irq(&interface->process_barrier);

	return 0;
}

static int init_state_barrier(struct fimc_is_interface *interface)
{
	mutex_init(&interface->state_barrier);

	return 0;
}

static int enter_state_barrier(struct fimc_is_interface *interface)
{
	mutex_lock(&interface->state_barrier);

	return 0;
}

static int exit_state_barrier(struct fimc_is_interface *interface)
{
	mutex_unlock(&interface->state_barrier);

	return 0;
}

static int init_wait_queue(struct fimc_is_interface *interface)
{
	init_waitqueue_head(&interface->wait_queue);

	return 0;
}

static int set_state(struct fimc_is_interface *interface,
	enum fimc_is_interface_state state)
{
	enter_state_barrier(interface);

	interface->state = state;

	exit_state_barrier(interface);

	return 0;
}

static int wait_state(struct fimc_is_interface *interface,
	enum fimc_is_interface_state state)
{
	s32 ret = 0;
	ret = wait_event_timeout(interface->wait_queue,
			(interface->state == state),
			FIMC_IS_SHUTDOWN_TIMEOUT);
	if (!ret) {
		err("wait_state timeout(state : %d, %d)\n",
			interface->state, state);
		ret = -EBUSY;
	}

	return ret;
}

static int waiting_is_ready(struct fimc_is_interface *interface)
{
	u32 cfg = readl(interface->regs + INTMSR0);
	u32 status = INTMSR0_GET_INTMSD0(cfg);
	while (status) {
		err("[MAIN] INTMSR1's 0 bit is not cleared.\n");
		cfg = readl(interface->regs + INTMSR0);
		status = INTMSR0_GET_INTMSD0(cfg);
	}

	return 0;
}

static void send_interrupt(struct fimc_is_interface *interface)
{
	writel(INTGR0_INTGD0, interface->regs + INTGR0);
}

static int fimc_is_set_cmd(struct fimc_is_interface *interface,
	struct fimc_is_interface_msg *msg)
{
	bool block_io, send_cmd;

	dbg_interface("TP#1\n");
	enter_request_barrier(interface);
	dbg_interface("TP#2\n");

	switch (msg->command) {
	case HIC_STREAM_ON:
		if (interface->streaming) {
			send_cmd = false;
		} else {
			send_cmd = true;
			block_io = true;
		}
		break;
	case HIC_STREAM_OFF:
		if (!interface->streaming) {
			send_cmd = false;
		} else {
			send_cmd = true;
			block_io = true;
		}
		break;
	case HIC_OPEN_SENSOR:
	case HIC_GET_SET_FILE_ADDR:
	case HIC_SET_PARAMETER:
	case HIC_PREVIEW_STILL:
	case HIC_PROCESS_START:
	case HIC_PROCESS_STOP:
	case HIC_SET_A5_MEM_ACCESS:
	case HIC_POWER_DOWN:
		send_cmd = true;
		block_io = true;
		break;
	case HIC_SHOT:
	case ISR_DONE:
		send_cmd = true;
		block_io = false;
		break;
	default:
		send_cmd = true;
		block_io = true;
		break;
	}

	if (send_cmd) {
		enter_process_barrier(interface);

		waiting_is_ready(interface);
		set_state(interface, IS_IF_STATE_BLOCK_IO);
		interface->com_regs->hicmd = msg->command;
		interface->com_regs->hic_sensorid = msg->instance;
		interface->com_regs->hic_param1 = msg->parameter1;
		interface->com_regs->hic_param2 = msg->parameter2;
		interface->com_regs->hic_param3 = msg->parameter3;
		interface->com_regs->hic_param4 = msg->parameter4;
		send_interrupt(interface);

		exit_process_barrier(interface);

		if (block_io)
			wait_state(interface, IS_IF_STATE_IDLE);

		switch (msg->command) {
		case HIC_STREAM_ON:
			interface->streaming = true;
			break;
		case HIC_STREAM_OFF:
			interface->streaming = false;
			break;
		default:
			break;
		}
	} else
		dbg_interface("skipped\n");

	exit_request_barrier(interface);

	return 0;
}

static int fimc_is_get_cmd(struct fimc_is_interface *interface,
	struct fimc_is_interface_msg *msg, u32 index)
{
	switch (index) {
	case INTR_GENERAL:
		msg->id = 0;
		msg->command = interface->com_regs->ihcmd;
		msg->instance = interface->com_regs->ihc_sensorid;
		msg->parameter1 = interface->com_regs->ihc_param1;
		msg->parameter2 = interface->com_regs->ihc_param2;
		msg->parameter3 = interface->com_regs->ihc_param3;
		msg->parameter4 = interface->com_regs->ihc_param4;
		break;
	case INTR_SCP_FDONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = interface->com_regs->scp_sensor_id;
		msg->parameter1 = interface->com_regs->scp_param1;
		msg->parameter2 = interface->com_regs->scp_param2;
		msg->parameter3 = interface->com_regs->scp_param3;
		msg->parameter4 = interface->com_regs->scp_param4;
		break;
	case INTR_META_DONE:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = interface->com_regs->meta_sensor_id;
		msg->parameter1 = interface->com_regs->meta_parameter1;
		msg->parameter2 = 0;
		msg->parameter3 = 0;
		msg->parameter4 = 0;
		break;
	default:
		msg->id = 0;
		msg->command = IHC_FRAME_DONE;
		msg->instance = 0;
		msg->parameter1 = 0;
		msg->parameter2 = 0;
		msg->parameter3 = 0;
		msg->parameter4 = 0;
		err("unknown command getting\n");
		break;
	}

	return 0;
}

static irqreturn_t interface_isr(int irq, void *data)
{
	struct fimc_is_interface *interface = (struct fimc_is_interface *)data;
	u32 status;

	status = readl(interface->regs + INTSR1);

	if (status & (1<<INTR_GENERAL)) {
		struct fimc_is_interface_msg *msg =
			&interface->work[INTR_GENERAL];
		fimc_is_get_cmd(interface, msg, INTR_GENERAL);

		switch (msg->command) {
		case IHC_GET_SENSOR_NUMBER:
			dbg_interface("IHC_GET_SENSOR_NUMBER\n");
			break;
		case ISR_DONE:
			/*dbg_interface("ISR_DONE\n");*/
			break;
		case ISR_NDONE:
			dbg_interface("ISR_NDONE\n");
			break;
		}

		schedule_work(&interface->work_queue[INTR_GENERAL]);

		writel((1<<INTR_GENERAL), interface->regs + INTCR1);
	}

	if (status & (1<<INTR_SCP_FDONE)) {
		struct fimc_is_interface_msg *msg =
			&interface->work[INTR_SCP_FDONE];
		fimc_is_get_cmd(interface, msg, INTR_SCP_FDONE);

		schedule_work(&interface->work_queue[INTR_SCP_FDONE]);

		writel((1<<INTR_SCP_FDONE), interface->regs + INTCR1);
	}

	if (status & (1<<INTR_META_DONE)) {
		struct fimc_is_interface_msg *msg =
			&interface->work[INTR_META_DONE];
		fimc_is_get_cmd(interface, msg, INTR_META_DONE);

		schedule_work(&interface->work_queue[INTR_META_DONE]);
		dbg_interface("meta done\n");

		writel((1<<INTR_META_DONE), interface->regs + INTCR1);
	}

	return IRQ_HANDLED;
}

static void wq_func_general(struct work_struct *data)
{
	struct fimc_is_interface *interface =
		container_of(data, struct fimc_is_interface,
			work_queue[INTR_GENERAL]);
	struct fimc_is_interface_msg *msg = &interface->work[INTR_GENERAL];

	switch (msg->command) {
	case IHC_GET_SENSOR_NUMBER:
		dbg_interface("version : %d\n", msg->parameter1);
		fimc_is_hw_enum(interface, 1);
		exit_request_barrier(interface);
		break;
	case ISR_DONE:
		switch (msg->parameter1) {
		case HIC_OPEN_SENSOR:
			dbg_interface("open done\n");
			break;
		case HIC_GET_SET_FILE_ADDR:
			dbg_interface("saddr(%p) done\n",
				(void *)msg->parameter2);
			break;
		case HIC_LOAD_SET_FILE:
			dbg_interface("setfile done\n");
			break;
		case HIC_SET_A5_MEM_ACCESS:
			dbg_interface("cfgmem done\n");
			break;
		case HIC_PROCESS_START:
			dbg_interface("process_on done\n");
			break;
		case HIC_PROCESS_STOP:
			dbg_interface("process_off done\n");
			break;
		case HIC_STREAM_ON:
			dbg_interface("stream_on done\n");
			break;
		case HIC_STREAM_OFF:
			dbg_interface("stream_off done\n");
			break;
		case HIC_SET_PARAMETER:
			dbg_interface("s_param done\n");
			break;
		case HIC_PREVIEW_STILL:
			dbg_interface("a_param(%dx%d) done\n", msg->parameter2,
				msg->parameter3);
			break;
		case HIC_SHOT:
			dbg_interface("shot done\n");
			break;
		default:
			err("unknown done is invokded\n");
			break;
		}

		set_state(interface, IS_IF_STATE_IDLE);
		break;
	case ISR_NDONE:
		dbg_interface("a command(%d) not done\n", msg->parameter1);

		set_state(interface, IS_IF_STATE_IDLE);
		break;
	default:
		dbg_interface("func0 unknown(%d) end\n", msg->command);
		break;
	}

	wake_up(&interface->wait_queue);
}

static void wq_func_scc(struct work_struct *data)
{
	struct fimc_is_interface *interface =
		container_of(data, struct fimc_is_interface,
			work_queue[INTR_SCC_FDONE]);
	struct fimc_is_interface_msg *msg = &interface->work[INTR_SCC_FDONE];

	switch (msg->command) {
	default:
		dbg_interface("func1 unknown end\n");
		break;
	}
}

static void wq_func_scp(struct work_struct *data)
{
	struct fimc_is_interface *interface =
		container_of(data, struct fimc_is_interface,
		work_queue[INTR_SCP_FDONE]);
	struct fimc_is_interface_msg *msg = &interface->work[INTR_SCP_FDONE];
	u32 buf_index = msg->parameter2;

	dbg_interface("P%d\n", buf_index);

	vb2_buffer_done(interface->video_scp->vbq.bufs[buf_index],
		VB2_BUF_STATE_DONE);
}

static void wq_func_meta(struct work_struct *data)
{
	struct fimc_is_interface *interface =
		container_of(data, struct fimc_is_interface,
			work_queue[INTR_META_DONE]);
	struct fimc_is_interface_msg *msg = &interface->work[INTR_META_DONE];
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame_shot *item;
	u32 frame_number;
	u32 buf_index = msg->parameter1;

	framemgr = interface->framemgr;

	fimc_is_frame_g_shot_from_process(framemgr, &item);

	if (item) {
		frame_number = msg->parameter1 ;
		buf_index = item->id;

		dbg_interface("S%d,%d,%d\n", buf_index,
			frame_number, item->frame_number);

		fimc_is_frame_s_shot_to_free(framemgr, item);
	}
}

int fimc_is_interface_probe(struct fimc_is_interface *this,
	struct fimc_is_framemgr *framemgr,
	struct resource *resource, u32 irq,
	struct is_region *addr1,
	struct is_share_region *addr2,
	void *data)
{
	int ret;
	struct fimc_is_core *core = (struct fimc_is_core *)data;

	this->streaming = false;
	init_request_barrier(this);
	init_process_barrier(this);
	init_state_barrier(this);
	init_wait_queue(this);

	INIT_WORK(&this->work_queue[INTR_GENERAL], wq_func_general);
	INIT_WORK(&this->work_queue[INTR_SCC_FDONE], wq_func_scc);
	INIT_WORK(&this->work_queue[INTR_SCP_FDONE], wq_func_scp);
	INIT_WORK(&this->work_queue[INTR_META_DONE], wq_func_meta);

	enter_request_barrier(this);

	set_state(this, IS_IF_STATE_IDLE);

	this->regs = (struct is_mcuctl_reg *)
		ioremap(resource->start, resource_size(resource));
	if (!this->regs)
		err("ioremap is failed!\n");

	this->com_regs = (struct is_common_reg *)
		(this->regs + ISSR0);

	ret = request_irq(irq, interface_isr,
				0, "MCUCTL", this);
	if (ret)
		err("request_irq failed\n");

	this->framemgr			= framemgr;
	this->is				= (void *)core;
	this->video_sensor		= &core->video_sensor.common;
	this->video_scp			= &core->video_scp.common;
	this->is_region			= addr1;
	this->is_shared_region	= addr2;

	return 0;
}

int fimc_is_hw_enum(struct fimc_is_interface *interface,
	u32 instances)
{
	struct fimc_is_interface_msg msg;

	dbg_interface("enum(%d)\n", instances);

	msg.id = 0;
	msg.command = ISR_DONE;
	msg.instance = 0;
	msg.parameter1 = IHC_GET_SENSOR_NUMBER;
	msg.parameter2 = instances;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	waiting_is_ready(interface);
	set_state(interface, IS_IF_STATE_BLOCK_IO);
	interface->com_regs->hicmd = msg.command;
	interface->com_regs->hic_sensorid = msg.instance;
	interface->com_regs->hic_param1 = msg.parameter1;
	interface->com_regs->hic_param2 = msg.parameter2;
	interface->com_regs->hic_param3 = msg.parameter3;
	interface->com_regs->hic_param4 = msg.parameter4;
	send_interrupt(interface);

	return 0;
}

int fimc_is_hw_saddr(struct fimc_is_interface *interface,
	u32 instance, u32 *setfile_addr)
{
	struct fimc_is_interface_msg msg;

	dbg_interface("saddr(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_GET_SET_FILE_ADDR;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	fimc_is_set_cmd(interface, &msg);
	*setfile_addr = interface->work[INTR_GENERAL].parameter2;

	return 0;
}

int fimc_is_hw_setfile(struct fimc_is_interface *interface,
	u32 instance)
{
	struct fimc_is_interface_msg msg;

	dbg_interface("setfile(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_LOAD_SET_FILE;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	fimc_is_set_cmd(interface, &msg);

	return 0;
}

int fimc_is_hw_open(struct fimc_is_interface *interface,
	u32 instance, u32 sensor, u32 channel)
{
	struct is_region *region;
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("open(%d)\n", sensor);

	wait_state(interface, IS_IF_STATE_IDLE);

	msg.id = 0;
	msg.command = HIC_OPEN_SENSOR;
	msg.instance = instance;
	msg.parameter1 = sensor;
	msg.parameter2 = channel;
	msg.parameter3 = ISS_PREVIEW_STILL;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	if (interface->is_region->shared[MAX_SHARED_COUNT-1]
		!= MAGIC_NUMBER) {
		err("MAGIC NUMBER error\n");
		ret = 1;
	}

	region = interface->is_region;

	dbg_interface("region addr : %p\n", region);

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

	return ret;
}

int fimc_is_hw_stream_on(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("stream_on(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_STREAM_ON;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_stream_off(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("stream_off(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_STREAM_OFF;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_process_on(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("process_on(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_PROCESS_START;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_process_off(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("process_off(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_PROCESS_STOP;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_s_param(struct fimc_is_interface *interface,
	u32 instance, u32 indexes, u32 lindex, u32 hindex)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("s_param(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_SET_PARAMETER;
	msg.instance = instance;
	msg.parameter1 = ISS_PREVIEW_STILL;
	msg.parameter2 = indexes;
	msg.parameter3 = lindex;
	msg.parameter4 = hindex;

	fimc_is_mem_cache_clean((void *)interface->is_region, IS_PARAM_SIZE);

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_a_param(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret = 0;
#if 0 /* will implement */
	struct fimc_is_interface_msg msg;

	dbg_interface("a_param(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_PREVIEW_STILL;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);
#endif
	return ret;
}

int fimc_is_hw_f_param(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret;
	struct fimc_is_interface_msg msg;
#ifdef DEBUG
	struct is_region	*region = interface->is_region;
	int navailable = 0;
#endif

	dbg_interface("f_param(%d)\n", instance);
	dbg_interface(" NAME    ON  BYPASS       SIZE  FORMAT\n");
	dbg_interface("ISP OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.isp.control.cmd,
		region->parameter.isp.control.bypass,
		region->parameter.isp.otf_output.width,
		region->parameter.isp.otf_output.height,
		region->parameter.isp.otf_output.format
		);
	dbg_interface("DRC OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass,
		region->parameter.drc.otf_input.width,
		region->parameter.drc.otf_input.height,
		region->parameter.drc.otf_input.format
		);
	dbg_interface("DRC OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass,
		region->parameter.drc.otf_output.width,
		region->parameter.drc.otf_output.height,
		region->parameter.drc.otf_output.format
		);
	dbg_interface("SCC OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.otf_input.width,
		region->parameter.scalerc.otf_input.height,
		region->parameter.scalerc.otf_input.format
		);
	dbg_interface("SCC OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.otf_output.width,
		region->parameter.scalerc.otf_output.height,
		region->parameter.scalerc.otf_output.format
		);
	dbg_interface("ODC OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.odc.control.cmd,
		region->parameter.odc.control.bypass,
		region->parameter.odc.otf_input.width,
		region->parameter.odc.otf_input.height,
		region->parameter.odc.otf_input.format
		);
	dbg_interface("ODC OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.odc.control.cmd,
		region->parameter.odc.control.bypass,
		region->parameter.odc.otf_output.width,
		region->parameter.odc.otf_output.height,
		region->parameter.odc.otf_output.format
		);
	dbg_interface("DIS OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass,
		region->parameter.dis.otf_input.width,
		region->parameter.dis.otf_input.height,
		region->parameter.dis.otf_input.format
		);
	dbg_interface("DIS OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass,
		region->parameter.dis.otf_output.width,
		region->parameter.dis.otf_output.height,
		region->parameter.dis.otf_output.format
		);
	dbg_interface("DNR OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.tdnr.control.cmd,
		region->parameter.tdnr.control.bypass,
		region->parameter.tdnr.otf_input.width,
		region->parameter.tdnr.otf_input.height,
		region->parameter.tdnr.otf_input.format
		);
	dbg_interface("DNR OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.tdnr.control.cmd,
		region->parameter.tdnr.control.bypass,
		region->parameter.tdnr.otf_output.width,
		region->parameter.tdnr.otf_output.height,
		region->parameter.tdnr.otf_output.format
		);
	dbg_interface("SCP OI : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.otf_input.width,
		region->parameter.scalerp.otf_input.height,
		region->parameter.scalerp.otf_input.format
		);
	dbg_interface("SCP DO : %2d    %4d  %04dx%04d    %d,%d\n",
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.dma_output.width,
		region->parameter.scalerp.dma_output.height,
		region->parameter.scalerp.dma_output.format,
		region->parameter.scalerp.dma_output.plane
		);
	dbg_interface("SCP OO : %2d    %4d  %04dx%04d      %d\n",
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.otf_output.width,
		region->parameter.scalerp.otf_output.height,
		region->parameter.scalerp.otf_output.format
		);
	dbg_interface(" NAME   CMD    IN_SZIE   OT_SIZE      CROP       POS\n");
	dbg_interface("SCC CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
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
	dbg_interface("SCC CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
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
	dbg_interface("SCP CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
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
	dbg_interface("SCP CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n",
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
	dbg_interface(" NAME   BUFS		MASK\n");
	dbg_interface("SCP DO : %2d    %04X\n",
		region->parameter.scalerp.dma_output.buffer_number,
		region->parameter.scalerp.dma_output.dma_out_mask
		);

	msg.id = 0;
	msg.command = HIC_PREVIEW_STILL;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_cfg_mem(struct fimc_is_interface *interface,
	u32 instance, u32 address, u32 size)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("cfg_mem(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_SET_A5_MEM_ACCESS;
	msg.instance = instance;
	msg.parameter1 = address;
	msg.parameter2 = size;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}

int fimc_is_hw_shot(struct fimc_is_interface *interface,
	u32 instance, u32 byaer, u32 shot, u32 frame)
{
	int ret = 0;
	struct fimc_is_interface_msg msg;

	dbg_interface("shot(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_SHOT;
	msg.instance = instance;
	msg.parameter1 = byaer;
	msg.parameter2 = shot;
	msg.parameter3 = frame;
	msg.parameter4 = 0;

#if 0
	ret = fimc_is_set_cmd(interface, &msg);
#else
	enter_process_barrier(interface);

	waiting_is_ready(interface);
	set_state(interface, IS_IF_STATE_BLOCK_IO);
	interface->com_regs->hicmd = msg.command;
	interface->com_regs->hic_sensorid = msg.instance;
	interface->com_regs->hic_param1 = msg.parameter1;
	interface->com_regs->hic_param2 = msg.parameter2;
	interface->com_regs->hic_param3 = msg.parameter3;
	interface->com_regs->hic_param4 = msg.parameter4;
	send_interrupt(interface);

	exit_process_barrier(interface);
#endif

	return ret;
}

int fimc_is_hw_power_down(struct fimc_is_interface *interface,
	u32 instance)
{
	int ret;
	struct fimc_is_interface_msg msg;

	dbg_interface("pwr_down(%d)\n", instance);

	msg.id = 0;
	msg.command = HIC_POWER_DOWN;
	msg.instance = instance;
	msg.parameter1 = 0;
	msg.parameter2 = 0;
	msg.parameter3 = 0;
	msg.parameter4 = 0;

	ret = fimc_is_set_cmd(interface, &msg);

	return ret;
}
