/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_FRAME_MGR_H
#define FIMC_IS_FRAME_MGR_H

#define TRACE_FRAME

#define FRAMEMGR_MAX_REQUEST 8

enum fimc_is_frame_output {
	FIMC_IS_FOUT_META,
	FIMC_IS_FOUT_SCC,
	FIMC_IS_FOUT_SCP,
};

struct fimc_is_frame_shot {
	struct list_head list;
	u32 kvaddr_buffer;
	u32 dvaddr_buffer;
	u32 kvaddr_shot;
	u32 dvaddr_shot;
	u32 frame_number;
	u32 id;
};

struct fimc_is_framemgr {
	struct list_head shot_free_head;
	struct list_head shot_request_head;
	struct list_head shot_process_head;
	struct list_head shot_complete_head;

	spinlock_t mutex_free;
	spinlock_t mutex_request;
	spinlock_t mutex_process;
	spinlock_t mutex_complete;

	u32 shot_free_cnt;
	u32 shot_request_cnt;
	u32 shot_process_cnt;
	u32 shot_complete_cnt;

	unsigned long	output_image_flag;
	u32				output_image_cnt;
};

int fimc_is_frame_probe(struct fimc_is_framemgr *this);

int fimc_is_frame_s_shot_to_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_shot_from_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_free_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_shot_to_reqest_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_s_shot_to_reqest_tail(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_shot_from_request(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_request_list(struct fimc_is_framemgr *this);
int fimc_is_frame_g_request_cnt(struct fimc_is_framemgr *this);

int fimc_is_frame_s_shot_to_process(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_shot_from_process(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_process_list(struct fimc_is_framemgr *this);
int fimc_is_frame_g_process_cnt(struct fimc_is_framemgr *this);

int fimc_is_frame_s_shot_to_complete(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_shot_from_complete(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_complete_list(struct fimc_is_framemgr *this);

#endif
