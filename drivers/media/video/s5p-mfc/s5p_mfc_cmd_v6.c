/*
 * linux/drivers/media/video/s5p-mfc/s5p_mfc_cmd_v6.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "s5p_mfc_common.h"

#include "s5p_mfc_debug.h"
#include "s5p_mfc_cmd.h"
#include "s5p_mfc_intr.h"

int s5p_mfc_cmd_host2risc(struct s5p_mfc_dev *dev, int cmd,
						struct s5p_mfc_cmd_args *args)
{
	mfc_debug(2, "Issue the command: %d\n", cmd);

	/* Reset RISC2HOST command */
	mfc_write(dev, 0x0, S5P_FIMV_RISC2HOST_CMD);

	/* Issue the command */
	mfc_write(dev, cmd, S5P_FIMV_HOST2RISC_CMD);
	mfc_write(dev, 0x1, S5P_FIMV_HOST2RISC_INT);

	return 0;
}

int s5p_mfc_sys_init_cmd(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_cmd_args h2r_args;
	struct s5p_mfc_buf_size_v6 *buf_size = dev->variant->buf_size->priv;
	int ret;

	mfc_debug_enter();

	s5p_mfc_alloc_dev_context_buffer(dev);

	mfc_write(dev, dev->ctx_buf.dma, S5P_FIMV_CONTEXT_MEM_ADDR);
	mfc_write(dev, buf_size->dev_ctx, S5P_FIMV_CONTEXT_MEM_SIZE);

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_SYS_INIT, &h2r_args);

	mfc_debug_leave();

	return ret;
}

int s5p_mfc_sleep_cmd(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_SLEEP, &h2r_args);

	mfc_debug_leave();

	return ret;
}

int s5p_mfc_wakeup_cmd(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_WAKEUP, &h2r_args);

	mfc_debug_leave();

	return ret;
}

/* Open a new instance and get its number */
int s5p_mfc_open_inst_cmd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	mfc_debug(2, "Requested codec mode: %d\n", ctx->codec_mode);
	dev->curr_ctx = ctx->num;
	mfc_write(dev, ctx->codec_mode, S5P_FIMV_CODEC_TYPE);
	mfc_write(dev, ctx->ctx.dma, S5P_FIMV_CONTEXT_MEM_ADDR);
	mfc_write(dev, ctx->ctx_size, S5P_FIMV_CONTEXT_MEM_SIZE);
	mfc_write(dev, 0, S5P_FIMV_D_CRC_CTRL); /* no crc */

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_OPEN_INSTANCE, &h2r_args);

	mfc_debug_leave();

	return ret;
}

/* Close instance */
int s5p_mfc_close_inst_cmd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_cmd_args h2r_args;
	int ret = 0;

	mfc_debug_enter();

	dev->curr_ctx = ctx->num;
	if (ctx->state != MFCINST_FREE) {
		mfc_write(dev, ctx->inst_no, S5P_FIMV_INSTANCE_ID);

		ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_CLOSE_INSTANCE,
					    &h2r_args);
	} else {
		ret = -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}
