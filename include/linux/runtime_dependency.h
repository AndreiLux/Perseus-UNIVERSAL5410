/*
 * runtime_dependency.c
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: February 2014
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/sysctl.h>
#include <linux/kernel.h>

extern u64 rt_dependency_state;

enum dependency_flag {
	DEPENDENCY_NONE = 0,
	GSC_REORDER,
	MTP_SAMSUNG,
};

enum dependency_mask {
	DEPENDENCY_NONE_MASK = 1 << DEPENDENCY_NONE,
	GSC_REORDER_MASK = 1 << GSC_REORDER,
	MTP_SAMSUNG_MASK = 1 << MTP_SAMSUNG,
};

int proc_rt_dependency_handler(ctl_table *table, int write, void __user *buffer,
				size_t *lenp, loff_t *ppos);
void rt_callback_register(void* callback, u64 mask);
bool rt_is_flag(int flag);
bool rt_is_mask(u64 mask);
