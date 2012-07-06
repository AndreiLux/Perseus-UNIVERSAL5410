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

int fimc_is_frame_s_shot_to_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	spin_lock(&this->mutex_free);

	list_add_tail(&item->list, &this->shot_free_head);
	this->shot_free_cnt++;

	spin_unlock(&this->mutex_free);

	return 0;
}


int fimc_is_frame_g_shot_from_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	spin_lock(&this->mutex_free);

	if (this->shot_free_cnt) {
		*item = container_of(this->shot_free_head.next,
			struct fimc_is_frame_shot, list);
		list_del(&(*item)->list);
		this->shot_free_cnt--;
	} else {
		*item = NULL;
	}

	spin_unlock(&this->mutex_free);

	return 0;
}

int fimc_is_frame_print_free_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;
	struct fimc_is_frame_shot *shot;

	list_for_each(temp, &this->shot_free_head) {
		shot = list_entry(temp, struct fimc_is_frame_shot, list);
		dbg_frame("free shot id = %d\n", shot->id);
	}

	return 0;
}

int fimc_is_frame_s_shot_to_reqest_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	spin_lock(&this->mutex_request);

	list_add(&item->list, &this->shot_request_head);
	this->shot_request_cnt++;

#ifdef TRACE_FRAME
	fimc_is_frame_print_request_list(this);
#endif

	spin_unlock(&this->mutex_request);

	return 0;
}

int fimc_is_frame_s_shot_to_reqest_tail(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	spin_lock(&this->mutex_request);

	list_add_tail(&item->list, &this->shot_request_head);
	this->shot_request_cnt++;

#ifdef TRACE_FRAME
	fimc_is_frame_print_request_list(this);
#endif

	spin_unlock(&this->mutex_request);

	return 0;
}

int fimc_is_frame_g_shot_from_request(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	spin_lock(&this->mutex_request);

	if (this->shot_request_cnt) {
		*item = container_of(this->shot_request_head.next,
			struct fimc_is_frame_shot, list);
		list_del(&(*item)->list);
		this->shot_request_cnt--;
	} else {
		*item = NULL;
	}

	spin_unlock(&this->mutex_request);

	return 0;
}

int fimc_is_frame_g_request_cnt(struct fimc_is_framemgr *this)
{
	return this->shot_request_cnt;
}

int fimc_is_frame_print_request_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;

	printk(KERN_DEBUG "[FRM] req(%d) :", this->shot_request_cnt);

	list_for_each(temp, &this->shot_request_head) {
		struct fimc_is_frame_shot *shot =
			list_entry(temp, struct fimc_is_frame_shot, list);
		printk(KERN_DEBUG "%02d->\n", shot->id);
	}

	return 0;
}

int fimc_is_frame_s_shot_to_process(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	spin_lock(&this->mutex_process);

	list_add_tail(&item->list, &this->shot_process_head);
	this->shot_process_cnt++;

	spin_unlock(&this->mutex_process);

	return 0;
}


int fimc_is_frame_g_shot_from_process(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	spin_lock(&this->mutex_process);

	if (this->shot_process_cnt) {
		*item = container_of(this->shot_process_head.next,
			struct fimc_is_frame_shot, list);
		list_del(&(*item)->list);
		this->shot_process_cnt--;
	} else {
		*item = NULL;
	}

	spin_unlock(&this->mutex_process);

	return 0;
}

int fimc_is_frame_g_process_cnt(struct fimc_is_framemgr *this)
{
	return this->shot_process_cnt;
}

int fimc_is_frame_print_process_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;

	printk(KERN_DEBUG "[FRM] Pro(%d) :", this->shot_process_cnt);

	list_for_each(temp, &this->shot_process_head) {
		struct fimc_is_frame_shot *shot =
			list_entry(temp, struct fimc_is_frame_shot, list);
		printk(KERN_DEBUG "%02d->\n", shot->id);
	}

	return 0;
}

int fimc_is_frame_s_shot_to_complete(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item)
{
	/*spin_lock(&this->mutex_complete);*/

	list_add_tail(&item->list, &this->shot_complete_head);
	this->shot_complete_cnt++;

	/*spin_unlock(&this->mutex_complete);*/

	return 0;
}


int fimc_is_frame_g_shot_from_complete(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item)
{
	spin_lock(&this->mutex_complete);

	if (this->shot_complete_cnt) {
		*item = container_of(this->shot_complete_head.next,
			struct fimc_is_frame_shot, list);
		list_del(&(*item)->list);
		this->shot_complete_cnt--;
	} else {
		*item = NULL;
	}

	spin_unlock(&this->mutex_complete);

	return 0;
}

int fimc_is_frame_print_complete_list(struct fimc_is_framemgr *this)
{
	struct list_head *temp;
	struct fimc_is_frame_shot *shot;

	list_for_each(temp, &this->shot_complete_head) {
		shot = list_entry(temp, struct fimc_is_frame_shot, list);
		dbg_frame("complete shot id = %d\n", shot->id);
	}

	return 0;
}

int fimc_is_frame_probe(struct fimc_is_framemgr *this)
{
	int ret = 0;
	struct fimc_is_frame_shot *shot;
	u32 i;

	spin_lock_init(&this->mutex_free);
	spin_lock_init(&this->mutex_request);
	spin_lock_init(&this->mutex_process);
	spin_lock_init(&this->mutex_complete);

	INIT_LIST_HEAD(&this->shot_free_head);
	INIT_LIST_HEAD(&this->shot_request_head);
	INIT_LIST_HEAD(&this->shot_process_head);
	INIT_LIST_HEAD(&this->shot_complete_head);

	this->shot_free_cnt = 0;
	this->shot_request_cnt = 0;
	this->shot_process_cnt = 0;
	this->shot_complete_cnt = 0;

	for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i) {
		shot = kmalloc(sizeof(struct fimc_is_frame_shot), GFP_KERNEL);
		shot->id = i;
		list_add_tail(&shot->list, &this->shot_free_head);
		this->shot_free_cnt++;
	}

	fimc_is_frame_print_free_list(this);

	return ret;
}
