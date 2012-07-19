/*
 *
 * (C) COPYRIGHT 2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file mali_kbase_sync_user.c
 *
 */

#ifdef CONFIG_SYNC

#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/anon_inodes.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <kbase/src/linux/mali_kbase_sync.h>
#include <kbase/mali_base_kernel_sync.h>

static long kbase_stream_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct sync_timeline * tl;

	tl = (struct sync_timeline *)f->private_data;
	BUG_ON(!tl);
	if (cmd == STREAM_IOC_FENCE_INSERT)
	{
		struct sync_pt * pt;
		struct sync_fence * fence;
		struct files_struct * files;
		struct fdtable * fdt;

		int fd;

		pt = kbase_sync_pt_alloc(tl);
		if (!pt)
		{
			return -EFAULT;
		}

		fence = sync_fence_create("mali_fence", pt);
		if (!fence)
		{
			sync_pt_free(pt);
			return -EFAULT;
		}

		/* from here the fence owns the sync_pt */

		/* create a fd representing the fence */
		fd = get_unused_fd();
		if (fd < 0)
		{
			sync_fence_put(fence);
			return -EFAULT;
		}

		files = current->files;
		spin_lock(&files->file_lock);
		fdt = files_fdtable(files);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		__set_close_on_exec(fd, fdt);
#else
		FD_SET(fd, fdt->close_on_exec);
#endif
		spin_unlock(&files->file_lock);

		/* bind fence to the new fd */
		sync_fence_install(fence, fd);

		return fd;
	}

	return -EINVAL;
}

static int kbase_stream_close(struct inode * inode, struct file * file)
{
	struct sync_timeline * tl;
	tl = (struct sync_timeline*)file->private_data;
	BUG_ON(!tl);
	sync_timeline_destroy(tl);
	return 0;
}

static struct file_operations stream_fops =
{
	.owner = THIS_MODULE,
	.release = kbase_stream_close,
	.unlocked_ioctl = kbase_stream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = kbase_stream_ioctl
#endif /* CONFIG_COMPAT */
};

mali_error kbase_stream_create(const char * name, int * out_fd)
{
	struct sync_timeline * tl;
	BUG_ON(!out_fd);

	tl = kbase_sync_timeline_alloc(name);
	if (!tl)
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}

	*out_fd = anon_inode_getfd(name, &stream_fops, tl, O_RDONLY | O_CLOEXEC);

	if (*out_fd < 0)
	{
		sync_timeline_destroy(tl);
		return MALI_ERROR_FUNCTION_FAILED;
	}
	else
	{
		return MALI_ERROR_NONE;
	}
}

mali_error kbase_fence_validate(int fd)
{
	struct sync_fence * fence;
	fence = sync_fence_fdget(fd);
	if (NULL != fence)
	{
		sync_fence_put(fence);
		return MALI_ERROR_NONE;
	}
	else
	{
		return MALI_ERROR_FUNCTION_FAILED;
	}
}

#endif /* CONFIG_SYNC */
