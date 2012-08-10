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

#ifndef FIMC_IS_TIME_H
#define FIMC_IS_TIME_H

/*#define MEASURE_TIME*/

#define TM_FLITE_STR	0
#define TM_FLITE_END	1
#define TM_SHOT		2
#define TM_SHOT_D	3
#define TM_META_D	4
#define TM_MAX_INDEX	5

#ifdef MEASURE_TIME

extern struct timeval curr_time;

int g_shot_period(struct timeval *str);
int g_shot_time(struct timeval *str, struct timeval *end);
int g_meta_time(struct timeval *str, struct timeval *end);
#endif

#endif
