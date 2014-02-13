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

#include <linux/slab.h>
#include <linux/runtime_dependency.h>

u64 rt_dependency_state = DEPENDENCY_NONE;
static bool initialized = false;

struct registree {
	u64			bitmask;
	void 			(*callback)(void);
	struct registree* 	next;
};

struct registree* callback_chain = NULL;

void rt_callback_register(void* callback, u64 mask)
{
	struct registree *new = kzalloc(sizeof(struct registree), GFP_KERNEL);
	struct registree *pos;

	if (callback_chain == NULL) {
		callback_chain = new;
		pos = new;
	} else {
		pos = callback_chain;
		while (pos->next != NULL)
			pos = pos->next;
	}

	pos->next = new;
	new->bitmask = mask;
	new->callback = callback;
}

bool rt_is_flag(int flag)
{
	return (rt_dependency_state & 1 << flag);
}

bool rt_is_mask(u64 mask)
{
	return (rt_dependency_state == mask);
}

int proc_rt_dependency_handler(ctl_table *table, int write, void __user *buffer,
			   size_t *lenp, loff_t *ppos)
{
	struct registree *registree;
	int error;

	error = proc_dointvec(table, write, buffer, lenp, ppos);
	if (error)
		return error;

	if (write && ((registree = callback_chain) != NULL) && !initialized) {
		do {
			registree->callback();
			registree = registree->next;
		} while (registree != NULL);

		initialized = true;
	}

	return 0;
}
