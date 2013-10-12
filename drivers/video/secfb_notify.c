/*
 *  linux/drivers/video/secfb_notify.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */
#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(secfb_notifier_list);

/**
 *	secfb_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int secfb_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&secfb_notifier_list, nb);
}

/**
 *	secfb_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int secfb_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&secfb_notifier_list, nb);
}

/**
 * secfb_notifier_call_chain - notify clients of fb_events
 *
 */
int secfb_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&secfb_notifier_list, val, v);
}
