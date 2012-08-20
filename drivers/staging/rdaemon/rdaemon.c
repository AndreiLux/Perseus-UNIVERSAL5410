/*
 * drivers/staging/rdaemon/rdaemon.c
 *
 * Copyright (C) 2012 Texas Instruments
 * Author: Guillaume Aubertin <g-aubertin@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include "rdaemon.h"

/* debugfs parent dir */
static struct dentry *rdaemon_dbg;
static u32 ping_count;

static ssize_t rdaemon_ping_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	struct rdaemon_msg_frame frame;
	struct rpmsg_channel *rpdev = filp->private_data;

	/* create message */
	frame.msg_type = RDAEMON_PING;
	frame.data = ping_count;

	/* send ping msg */
	ret = rpmsg_send(rpdev, (void *)(&frame),
			 sizeof(struct rdaemon_msg_frame));
	if (ret) {
		printk(KERN_ERR"rpmsg_send failed: %d", ret);
		return ret;
	}

	ping_count++;

	return count;
}

static const struct file_operations rdaemon_ping_ops = {
	.write = rdaemon_ping_write,
	.open = simple_open,
	.llseek	= generic_file_llseek,
};

static void rdaemon_create_debug_tree(struct rpmsg_channel *rpdev)
{
	struct device *dev = &rpdev->dev;
	struct dentry *d;

	d = debugfs_create_dir(dev_name(dev), rdaemon_dbg);

	ping_count = 0;

	debugfs_create_file("ping", 0400, d,
				    rpdev, &rdaemon_ping_ops);
}

static int rpmsg_probe(struct rpmsg_channel *_rpdev)
{
	/* create debugfs */
	rdaemon_create_debug_tree(_rpdev);

	return 0;
}

static void rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
		int len, void *priv, u32 src)
{
	printk(KERN_ERR"len=%d, src=%d", len, src);
}

static void __devexit rpmsg_remove(struct rpmsg_channel *_rpdev)
{
	rdaemon_dbg = NULL;
}


static struct rpmsg_device_id rpmsg_id_table[] = {
		{ .name = "rpmsg-rdaemon" },
		{ },
};

static struct rpmsg_driver rpmsg_driver = {
		.drv.name       = KBUILD_MODNAME,
		.drv.owner      = THIS_MODULE,
		.id_table       = rpmsg_id_table,
		.probe          = rpmsg_probe,
		.callback       = rpmsg_cb,
		.remove         = __devexit_p(rpmsg_remove),
};

static int __init rdaemon_init(void)
{
	if (debugfs_initialized()) {
		rdaemon_dbg = debugfs_create_dir(KBUILD_MODNAME, NULL);
		if (!rdaemon_dbg) {
			pr_err("can't create debugfs dir\n");
			return -1;
		}
	}
	return register_rpmsg_driver(&rpmsg_driver);
}

static void __exit rdaemon_fini(void)
{
	if (rdaemon_dbg)
		debugfs_remove_recursive(rdaemon_dbg);

	unregister_rpmsg_driver(&rpmsg_driver);
}

module_init(rdaemon_init);
module_exit(rdaemon_fini);

MODULE_AUTHOR("Guillaume Aubertin <g-aubertin@ti.com>");
MODULE_DESCRIPTION("OMAP remote daemon for rpmsg");
MODULE_LICENSE("GPL v2");
