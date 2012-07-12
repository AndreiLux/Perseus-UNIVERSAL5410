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

int fimc_is_frame_s_free_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (item) {
		item->state = FIMC_IS_SHOT_STATE_FREE;

		list_add_tail(&item->list, &this->shot_free_head);
		this->shot_free_cnt++;

#ifdef TRACE_FRAME
		fimc_is_frame_print_free_list(this);
#endif
	} else {
		ret = EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}


int fimc_is_frame_g_free_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (item) {
		if (this->shot_free_cnt) {
			*item = container_of(this->shot_free_head.next,
				struct fimc_is_frame_shot, list);
			list_del(&(*item)->list);
			this->shot_free_cnt--;

			(*item)->state = FIMC_IS_SHOT_STATE_INVALID;
		} else {
			*item = NULL;
		}
	} else {
		ret = EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

int fimc_is_frame_free_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (this->shot_free_cnt)
		*item = container_of(this->shot_free_head.next,
			struct fimc_is_frame_shot, list);
	else
		*item = NULL;

	return ret;
}

int fimc_is_frame_print_free_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;
	struct fimc_is_frame_shot *shot;

	if (!(TRACE_ID & this->id))
		return 0;

	printk(KERN_CONT "[FRM] fre(%d, %d) :", this->id, this->shot_free_cnt);

	list_for_each(temp, &this->shot_free_head) {
		shot = list_entry(temp, struct fimc_is_frame_shot, list);
		printk(KERN_CONT "%d->", shot->id);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

int fimc_is_frame_s_request_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	dbg_frame("%s(%x) req_cnt(%d)\n", __func__,
		(unsigned int)this, this->shot_request_cnt);

	if (item) {
		list_add_tail(&item->list, &this->shot_request_head);
		this->shot_request_cnt++;

		item->state = FIMC_IS_SHOT_STATE_REQUEST;

#ifdef TRACE_FRAME
		fimc_is_frame_print_request_list(this);
#endif
	} else {
		ret = EFAULT;
		err("item is null ptr\n");
	}

	dbg_frame("%s END req_cnt(%d)\n", __func__, this->shot_request_cnt);

	return ret;
}

int fimc_is_frame_g_request_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	dbg_frame("%s(%x) req_cnt(%d)\n", __func__,
		(unsigned int)this, this->shot_request_cnt);

	if (item) {
		if (this->shot_request_cnt) {
			*item = container_of(this->shot_request_head.next,
				struct fimc_is_frame_shot, list);
			list_del(&(*item)->list);
			this->shot_request_cnt--;

			(*item)->state = FIMC_IS_SHOT_STATE_INVALID;
		} else {
			*item = NULL;
		}
	} else {
		ret = EFAULT;
		err("item is null ptr\n");
	}

	dbg_frame("%s END req_cnt(%d)\n", __func__, this->shot_request_cnt);
	return ret;
}

int fimc_is_frame_request_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (this->shot_request_cnt)
		*item = container_of(this->shot_request_head.next,
			struct fimc_is_frame_shot, list);
	else
		*item = NULL;

	return ret;
}

int fimc_is_frame_print_request_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;
	struct fimc_is_frame_shot *shot;

	if (!(TRACE_ID & this->id))
		return 0;

	dbg_frame("[FRM] req(%d, %d) :",
		this->id, this->shot_request_cnt);

	list_for_each(temp, &this->shot_request_head) {
		shot = list_entry(temp, struct fimc_is_frame_shot, list);
		dbg_frame("%02d->", shot->id);
	}

	dbg_frame("END\n");

	return 0;
}

int fimc_is_frame_s_process_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (item) {
		list_add_tail(&item->list, &this->shot_process_head);
		this->shot_process_cnt++;

		item->state = FIMC_IS_SHOT_STATE_PROCESS;

#ifdef TRACE_FRAME
		fimc_is_frame_print_process_list(this);
#endif
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

int fimc_is_frame_g_process_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (item) {
		if (this->shot_process_cnt) {
			*item = container_of(this->shot_process_head.next,
				struct fimc_is_frame_shot, list);
			list_del(&(*item)->list);
			this->shot_process_cnt--;

			(*item)->state = FIMC_IS_SHOT_STATE_INVALID;
		} else {
			*item = NULL;
		}
	} else {
		ret = EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

int fimc_is_frame_process_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (this->shot_process_cnt)
		*item = container_of(this->shot_process_head.next,
			struct fimc_is_frame_shot, list);
	else
		*item = NULL;

	return ret;
}

int fimc_is_frame_print_process_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;
	struct fimc_is_frame_shot *shot;

	if (!(TRACE_ID & this->id))
		return 0;

	printk(KERN_CONT "[FRM] pro(%d, %d) :",
		this->id, this->shot_process_cnt);

	list_for_each(temp, &this->shot_process_head) {
		shot = list_entry(temp, struct fimc_is_frame_shot, list);
		printk(KERN_CONT "%d->", shot->id);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

int fimc_is_frame_s_complete_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (item) {
		list_add_tail(&item->list, &this->shot_complete_head);
		this->shot_complete_cnt++;

		item->state = FIMC_IS_SHOT_STATE_COMPLETE;

#ifdef TRACE_FRAME
		fimc_is_frame_print_complete_list(this);
#endif
	} else {
		ret = -EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}


int fimc_is_frame_g_complete_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (item) {
		if (this->shot_complete_cnt) {
			*item = container_of(this->shot_complete_head.next,
				struct fimc_is_frame_shot, list);
			list_del(&(*item)->list);
			this->shot_complete_cnt--;

			(*item)->state = FIMC_IS_SHOT_STATE_INVALID;
		} else {
			*item = NULL;
		}
	} else {
		ret = EFAULT;
		err("item is null ptr\n");
	}

	return ret;
}

int fimc_is_frame_complete_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	int ret = 0;

	if (this->shot_complete_cnt)
		*item = container_of(this->shot_complete_head.next,
			struct fimc_is_frame_shot, list);
	else
		*item = NULL;

	return ret;
}

int fimc_is_frame_print_complete_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;
	struct fimc_is_frame_shot *shot;

	if (!(TRACE_ID & this->id))
		return 0;

	dbg_frame("[FRM] com(%d, %d) :",
		this->id, this->shot_complete_cnt);

	list_for_each(temp, &this->shot_complete_head) {
		shot = list_entry(temp, struct fimc_is_frame_shot, list);
		dbg_frame("%02d->", shot->id);
	}

	dbg_frame("END\n");

	return 0;
}

int fimc_is_frame_trans_free_to_req(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_free_cnt) {
		err("shot free count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_free_cnt--;

	fimc_is_frame_s_request_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_trans_req_to_pro(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_request_cnt) {
		err("shot request count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_request_cnt--;

	fimc_is_frame_s_process_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_trans_req_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_request_cnt) {
		err("shot request count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_request_cnt--;

	fimc_is_frame_s_complete_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_trans_pro_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_process_cnt) {
		err("shot process count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_process_cnt--;

	fimc_is_frame_s_complete_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_trans_pro_to_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_process_cnt) {
		err("shot process count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_process_cnt--;

	fimc_is_frame_s_free_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_trans_free_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_free_cnt) {
		err("shot free count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_free_cnt--;

	fimc_is_frame_s_complete_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_trans_com_to_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	int ret = 0;

	if (!this->shot_complete_cnt) {
		err("shot complete count is zero\n");
		ret = 1;
		goto exit;
	}

	list_del(&item->list);
	this->shot_complete_cnt--;

	fimc_is_frame_s_free_shot(this, item);

exit:
	return ret;
}

int fimc_is_frame_probe(struct fimc_is_framemgr *this, u32 id)
{
	int ret = 0;
	u32 i;

	this->opened = 0;
	this->id = id;

	for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i)
		this->shot[i].state = FIMC_IS_SHOT_STATE_INVALID;

	return ret;
}

int fimc_is_frame_open(struct fimc_is_framemgr *this, u32 buffers)
{
	int ret = 0;
	u32 i;

	dbg_frame("%s opened(%d) req_cnt(%d)\n", __func__,
		this->opened, this->shot_request_cnt);

	if (!this->opened) {
		spin_lock_init(&this->slock);

		INIT_LIST_HEAD(&this->shot_free_head);
		INIT_LIST_HEAD(&this->shot_request_head);
		INIT_LIST_HEAD(&this->shot_process_head);
		INIT_LIST_HEAD(&this->shot_complete_head);

		this->shot_free_cnt = 0;
		this->shot_request_cnt = 0;
		this->shot_process_cnt = 0;
		this->shot_complete_cnt = 0;

		for (i = 0; i < buffers; ++i) {
			this->shot[i].id = i;
			this->shot[i].fnumber = 0;
			this->shot[i].request_flag = 0;
			this->shot[i].shot = NULL;
			this->shot[i].shot_ext = NULL;
			this->shot[i].kvaddr_buffer = 0;
			this->shot[i].dvaddr_buffer = 0;

			this->shot[i].kvaddr_shot = 0;
			this->shot[i].dvaddr_shot = 0;
			fimc_is_frame_s_free_shot(this, &this->shot[i]);
		}

		fimc_is_frame_print_free_list(this);

		this->opened = 1;
	}
	dbg_frame("%s END req_cnt(%d)\n", __func__, this->shot_request_cnt);
	return ret;
}

int fimc_is_frame_close(struct fimc_is_framemgr *this)
{
	int ret = 0;

	this->opened = 0;

	return ret;
}
