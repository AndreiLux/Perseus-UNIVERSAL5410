/*
 * arch/arm/mach-exynos/secmem.c
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/cma.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>

#include <asm/memory.h>
#include <asm/cacheflush.h>

#include <plat/devs.h>

#include <mach/secmem.h>
#include <linux/export.h>

#define SECMEM_DEV_NAME	"s5p-smem"
struct miscdevice secmem;
struct secmem_crypto_driver_ftn *crypto_driver;
#if defined(CONFIG_ION)
extern struct ion_device *ion_exynos;
#endif

static char *secmem_info[] = {
#if defined(CONFIG_SOC_EXYNOS5250)
	"mfc_sh",   	/* 0 */
	"msgbox_sh",	/* 1 */
	"fimd_video",  	/* 2 */
	"mfc_output",	/* 3 */
	"mfc_input",	/* 4 */
	"mfc_fw",	/* 5 */
	"sectbl",	/* 6 */
#endif
	NULL
};

static bool drm_onoff;

static long secmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case SECMEM_IOC_CHUNKINFO:
	{
		struct cma_info info;
		struct secchunk_info minfo;
		char **mname;
		int nbufs = 0;

		for (mname = secmem_info; *mname != NULL; mname++)
			nbufs++;

		if (nbufs == 0)
			return -ENOMEM;

		if (copy_from_user(&minfo, (void __user *)arg, sizeof(minfo)))
			return -EFAULT;

		if (minfo.index < 0)
			return -EINVAL;

		if (minfo.index >= nbufs) {
			minfo.index = -1; /* No more memory region */
		} else {

			if (cma_info(&info, secmem.this_device,
					secmem_info[minfo.index]))
				return -EINVAL;

			minfo.base = info.lower_bound;
			minfo.size = info.total_size;
		}

		if (copy_to_user((void __user *)arg, &minfo, sizeof(minfo)))
			return -EFAULT;
		break;
	}
#if defined(CONFIG_ION)
	case SECMEM_IOC_GET_FD_PHYS_ADDR:
	{
		struct ion_client *client;
		struct secfd_info fd_info;
		struct ion_fd_data data;
		size_t len;

		if (copy_from_user(&fd_info, (int __user *)arg,
					sizeof(fd_info)))
			return -EFAULT;

		client = ion_client_create(ion_exynos, -1, "DRM");
		if (IS_ERR(client))
			printk(KERN_ERR "%s: Failed to get ion_client of DRM\n",
				__func__);

		data.fd = fd_info.fd;
		data.handle = ion_import_dma_buf(client, data.fd);
		printk(KERN_DEBUG "%s: fd from user space = %d\n",
				__func__, fd_info.fd);
		if (IS_ERR(data.handle))
			printk(KERN_ERR "%s: Failed to get ion_handle of DRM\n",
				__func__);

		if (ion_phys(client, data.handle, &fd_info.phys, &len))
			printk(KERN_ERR "%s: Failed to get phys. addr of DRM\n",
				__func__);

		printk(KERN_DEBUG "%s: physical addr from kernel space = 0x%08x\n",
				__func__, (unsigned int)fd_info.phys);

		ion_free(client, data.handle);
		ion_client_destroy(client);

		if (copy_to_user((void __user *)arg, &fd_info, sizeof(fd_info)))
			return -EFAULT;
		break;
	}
#endif
	case SECMEM_IOC_GET_DRM_ONOFF:
		if (copy_to_user((void __user *)arg, &drm_onoff, sizeof(int)))
			return -EFAULT;
		break;
	case SECMEM_IOC_SET_DRM_ONOFF:
	{
		int val = 0;

		if (copy_from_user(&val, (int __user *)arg, sizeof(int)))
			return -EFAULT;

		if (val) {
			if (drm_onoff == false) {
				drm_onoff = true;
				pm_runtime_forbid(secmem.parent);
			} else
				printk(KERN_ERR "%s: DRM is already on\n", __func__);
		} else {
			if (drm_onoff == true) {
				drm_onoff = false;
				pm_runtime_allow(secmem.parent);
			} else
				printk(KERN_ERR "%s: DRM is already off\n", __func__);
		}
		break;
	}
	case SECMEM_IOC_GET_CRYPTO_LOCK:
	{
		int i;
		int ret;

		if (crypto_driver) {
			for (i = 0; i < 100; i++) {
				ret = crypto_driver->lock();
				if (ret == 0)
					break;
				printk(KERN_ERR "%s : Retry to get sync lock.\n",
					__func__);
			}
			return ret;
		}
		break;
	}
	case SECMEM_IOC_RELEASE_CRYPTO_LOCK:
	{
		if (crypto_driver)
			return crypto_driver->release();
		break;
	}
	default:
		return -ENOTTY;
	}

	return 0;
}

void secmem_crypto_register(struct secmem_crypto_driver_ftn *ftn)
{
	crypto_driver = ftn;
}
EXPORT_SYMBOL(secmem_crypto_register);

void secmem_crypto_deregister(void)
{
	crypto_driver = NULL;
}
EXPORT_SYMBOL(secmem_crypto_deregister);

static const struct file_operations secmem_fops = {
	.unlocked_ioctl = secmem_ioctl,
};

extern struct platform_device exynos5_device_gsc0;

struct miscdevice secmem = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= SECMEM_DEV_NAME,
	.fops	= &secmem_fops,
	.parent	= &exynos5_device_gsc0.dev,
};

static int __init secmem_init(void)
{
	int ret;

	ret = misc_register(&secmem);
	if (ret)
		printk(KERN_ERR "%s: SECMEM can't register misc on minor=%d\n",
			__func__, MISC_DYNAMIC_MINOR);
		return ret;

	crypto_driver = NULL;

	pm_runtime_enable(secmem.this_device);

	return 0;
}

static void __exit secmem_exit(void)
{
	__pm_runtime_disable(secmem.this_device, false);
	misc_deregister(&secmem);
}

module_init(secmem_init);
module_exit(secmem_exit);
