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

#define FRAMEMGR_ID_SENSOR	0x1
#define FRAMEMGR_ID_ISCHAIN	0x2

/*#define TRACE_FRAME*/
#define TRACE_ID		(FRAMEMGR_ID_SENSOR + FRAMEMGR_ID_ISCHAIN)

#define FRAMEMGR_MAX_REQUEST 20

enum fimc_is_frame_output {
	FIMC_IS_FOUT_META,
	FIMC_IS_FOUT_SCC,
	FIMC_IS_FOUT_SCP,
};

enum fimc_is_frame_shot_state {
	FIMC_IS_SHOT_STATE_FREE,
	FIMC_IS_SHOT_STATE_REQUEST,
	FIMC_IS_SHOT_STATE_PROCESS,
	FIMC_IS_SHOT_STATE_COMPLETE,
	FIMC_IS_SHOT_STATE_INVALID
};

enum fimc_is_frame_reqeust {
	FIMC_IS_REQ_MDT,
	FIMC_IS_REQ_SCC,
	FIMC_IS_REQ_SCP
};

struct fimc_is_frame_shot {
	struct list_head list;
	struct camera2_shot *shot;
	struct camera2_shot_ext *shot_ext;

	unsigned long request_flag;

	u32 kvaddr_buffer;
	u32 dvaddr_buffer;

	u32 kvaddr_shot;
	u32 dvaddr_shot;
	u32 state;

	u32 fnumber;
	u32 id;

	/*spinlock_t slock;*/
};

struct fimc_is_framemgr {
	u32				shots;
	struct fimc_is_frame_shot	shot[FRAMEMGR_MAX_REQUEST];

	struct list_head		shot_free_head;
	struct list_head		shot_request_head;
	struct list_head		shot_process_head;
	struct list_head		shot_complete_head;

	spinlock_t			slock;

	u32				shot_free_cnt;
	u32				shot_request_cnt;
	u32				shot_process_cnt;
	u32				shot_complete_cnt;

	unsigned long			output_image_flag;
	u32				output_image_cnt;

	u32				opened;
	u32				id;
};

int fimc_is_frame_probe(struct fimc_is_framemgr *this, u32 id);
int fimc_is_frame_open(struct fimc_is_framemgr *this, u32 buffers);
int fimc_is_frame_close(struct fimc_is_framemgr *this);

int fimc_is_frame_s_free_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_free_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_free_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_free_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_request_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_request_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_request_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_request_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_process_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_process_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_process_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_process_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_complete_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_g_complete_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_complete_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot **item);
int fimc_is_frame_print_complete_list(struct fimc_is_framemgr *this);

int fimc_is_frame_trans_free_to_req(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_trans_free_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_trans_req_to_pro(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_trans_req_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_trans_pro_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_trans_pro_to_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);
int fimc_is_frame_trans_com_to_free(struct fimc_is_framemgr *this,
	struct fimc_is_frame_shot *item);

#endif
