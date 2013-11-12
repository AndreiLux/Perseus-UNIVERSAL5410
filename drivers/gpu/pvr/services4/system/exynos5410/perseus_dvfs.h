/*
 * perseus_dvfs.c - Exynos 5410 SGX544MP3 DVFS driver
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: June 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#ifndef __SEC_DVFS_H__
#define __SEC_DVFS_H__

void sec_gpu_dvfs_init(void);
int sec_clock_change_up(int level, int step);
int sec_clock_change_down(int value, int step);
int sec_gpu_dvfs_level_from_clk_get(int clock);
void sec_gpu_dvfs_down_requirement_reset(void);
int sec_custom_threshold_set(void);
void sec_gpu_dvfs_handler(int utilization_value);

#endif /*__SEC_DVFS_H__*/
