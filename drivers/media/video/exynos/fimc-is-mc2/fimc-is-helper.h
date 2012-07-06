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

#ifndef FIMC_IS_HELPER_H
#define FIMC_IS_HELPER_H

#include "fimc-is-core.h"

/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		(1280) /* sensor margin : 16 */
#define DEFAULT_PREVIEW_STILL_HEIGHT		(720) /* sensor margin : 12 */
#define DEFAULT_CAPTURE_VIDEO_WIDTH		(1920)
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		(1080)
#define DEFAULT_CAPTURE_STILL_WIDTH		(2560)
#define DEFAULT_CAPTURE_STILL_HEIGHT		(1920)
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	(2560)
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	(1440)
#define DEFAULT_PREVIEW_VIDEO_WIDTH		(640)
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		(480)

#define DEFAULT_4E5_PREVIEW_WIDTH		(1920)	/* sensor margin : 16 */
#define DEFAULT_4E5_PREVIEW_HEIGHT		(1080)	/* sensor margin : 12 */
#define DEFAULT_4E5_VIDEO_WIDTH			(1920)
#define DEFAULT_4E5_VIDEO_HEIGHT		(1080)
#define DEFAULT_4E5_STILLSHOT_WIDTH		(2560)
#define DEFAULT_4E5_STILLSHOT_HEIGHT		(1920)

#define DEFAULT_6A3_PREVIEW_WIDTH		(1280)	/* sensor margin : 16 */
#define DEFAULT_6A3_PREVIEW_HEIGHT		(720)	/* sensor margin : 12 */
#define DEFAULT_6A3_VIDEO_WIDTH			(1280)
#define DEFAULT_6A3_VIDEO_HEIGHT		(720)
#define DEFAULT_6A3_STILLSHOT_WIDTH		(1392)
#define DEFAULT_6A3_STILLSHOT_HEIGHT		(1392)

#define DEFAULT_PREVIEW_STILL_FRAMERATE		(30)
#define DEFAULT_CAPTURE_STILL_FRAMERATE		(15)
#define DEFAULT_PREVIEW_VIDEO_FRAMERATE		(30)
#define DEFAULT_CAPTURE_VIDEO_FRAMERATE		(30)

#define DEFAULT_DIS_MAX_HEIGHT			(1080)

int  fimc_is_fw_clear_irq2(struct fimc_is_core *dev);
int  fimc_is_fw_clear_irq1_all(struct fimc_is_core *dev);
int  fimc_is_fw_clear_irq1(struct fimc_is_core *dev, unsigned int intr_pos);
void fimc_is_hw_set_sensor_num(struct fimc_is_core *dev);
void fimc_is_hw_get_setfile_addr(struct fimc_is_core *dev);
void fimc_is_hw_load_setfile(struct fimc_is_core *dev);
int  fimc_is_hw_get_sensor_num(struct fimc_is_core *dev);
int  fimc_is_hw_set_param(struct fimc_is_core *dev);
void fimc_is_hw_set_intgr0_gd0(struct fimc_is_core *dev);
int  fimc_is_hw_wait_intsr0_intsd0(struct fimc_is_core *dev);
int  fimc_is_hw_wait_intmsr0_intmsd0(struct fimc_is_core *dev);
void fimc_is_hw_a5_power(struct fimc_is_core *dev, int on);
int  fimc_is_hw_a5_power_on(struct fimc_is_core *dev);
int  fimc_is_hw_a5_power_off(struct fimc_is_core *dev);
void fimc_is_hw_open_sensor(struct fimc_is_core *dev, u32 id, u32 sensor_index);
void fimc_is_hw_set_stream(struct fimc_is_core *dev, int on);
void fimc_is_hw_set_debug_level(struct fimc_is_core *dev,
				int target, int module, int level);
void fimc_is_hw_set_init(struct fimc_is_core *dev);
void fimc_is_hw_change_mode(struct fimc_is_core *dev, int val);
void fimc_is_hw_set_lite(struct fimc_is_core *dev, u32 width, u32 height);
void fimc_is_hw_diable_wdt(struct fimc_is_core *dev);
void fimc_is_hw_set_low_poweroff(struct fimc_is_core *dev, int on);
void fimc_is_hw_subip_poweroff(struct fimc_is_core *dev);
int  fimc_is_fw_clear_insr1(struct fimc_is_core *dev);
void fimc_is_hw_set_default_size(struct fimc_is_core *dev, int  sensor_id);
#endif
