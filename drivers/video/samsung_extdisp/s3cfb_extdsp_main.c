/* linux/drivers/video/samsung/s3cfb_extdsp-main.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung Display Controller (FIMD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <mach/map.h>
#include "s3cfb_extdsp.h"

static struct s3cfb_extdsp_extdsp_desc *fbextdsp;

inline struct s3cfb_extdsp_global *get_extdsp_global(int id)
{
	struct s3cfb_extdsp_global *fbdev;
	fbdev = fbextdsp->fbdev[0];
	return fbdev;
}

static int s3cfb_extdsp_register_framebuffer(struct s3cfb_extdsp_global *fbdev)
{
	int ret;
	ret = register_framebuffer(fbdev->fb[0]);
	if (ret) {
		dev_err(fbdev->dev, "failed to register	\
				framebuffer device\n");
		return -EINVAL;
	}
	return 0;
}

static struct device_attribute dev_attr_pandisplay =
    __ATTR(pandisplay, S_IRUGO, NULL, NULL);

#define to_extdsp_plat(d)		(to_platform_device(d)->dev.platform_data)

static int s3cfb_extdsp_probe(struct platform_device *pdev)
{
	struct s3c_fb_pd_win *pdata = NULL;
	struct s3cfb_extdsp_global *fbdev[2];
	int ret = -1;

	fbextdsp = kzalloc(sizeof(struct s3cfb_extdsp_extdsp_desc), GFP_KERNEL);
	if (!fbextdsp)
		goto err;

	/* global structure */
	fbextdsp->fbdev[0] = kzalloc(sizeof(struct s3cfb_extdsp_global),
			GFP_KERNEL);
	fbdev[0] = fbextdsp->fbdev[0];
	if (!fbdev[0]) {
		dev_err(fbdev[0]->dev, "failed to allocate for "
				"global fb structure extdsp[%d]!\n", 0);
		goto err1;
	}

	fbdev[0]->dev = &pdev->dev;

	/* platform_data*/
	pdata = to_extdsp_plat(&pdev->dev);

	/* lcd setting */
	fbdev[0]->lcd = pdev->dev.platform_data;

	/* hw setting */
	s3cfb_extdsp_init_global(fbdev[0]);

	fbdev[0]->system_state = POWER_ON;

	/* alloc fb_info */
	if (s3cfb_extdsp_alloc_framebuffer(fbdev[0], 0)) {
		dev_err(fbdev[0]->dev, "alloc error extdsp[%d]\n", 0);
		goto err2;
	}

	/* register fb_info */
	if (s3cfb_extdsp_register_framebuffer(fbdev[0])) {
		dev_err(fbdev[0]->dev, "register error extdsp[%d]\n", 0);
		goto err2;
	}

	/* disable display as default */
	s3cfb_extdsp_disable_window(fbdev[0], 0);

	s3cfb_extdsp_update_power_state(fbdev[0], 0,
			FB_BLANK_POWERDOWN);

	ret = device_create_file(fbdev[0]->dev, &dev_attr_pandisplay);
	if (ret) {
		dev_err(fbdev[0]->dev, "failed to create pandisplay file\n");
		goto err_create_file;
	}

	dev_info(fbdev[0]->dev, "registered successfully\n");
	return 0;

err_create_file:
	device_remove_file(fbdev[0]->dev, &dev_attr_pandisplay);
err2:
	kfree(fbextdsp->fbdev[0]);
err1:
	kfree(fbextdsp);
err:
	return ret;
}

static int s3cfb_extdsp_remove(struct platform_device *pdev)
{
	struct s3cfb_extdsp_window *win;
	struct fb_info *fb;
	struct s3cfb_extdsp_global *fbdev[2];
	int j;

	fbdev[0] = fbextdsp->fbdev[0];

	for (j = 0; j < 1; j++) {
		fb = fbdev[0]->fb[j];

		/* free if exists */
		if (fb) {
			win = fb->par;
			framebuffer_release(fb);
		}
	}

	kfree(fbdev[0]->fb);
	kfree(fbdev[0]);

	return 0;
}

static struct platform_driver s3cfb_extdsp_driver = {
	.probe		= s3cfb_extdsp_probe,
	.remove		= s3cfb_extdsp_remove,
	.driver		= {
		.name	= S3CFB_EXTDSP_NAME,
		.owner	= THIS_MODULE,
		.pm	= NULL,
	},
};

struct fb_ops s3cfb_extdsp_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= s3cfb_extdsp_open,
	.fb_release	= s3cfb_extdsp_release,
	.fb_check_var	= s3cfb_extdsp_check_var,
	.fb_set_par	= s3cfb_extdsp_set_par,
	.fb_setcolreg	= s3cfb_extdsp_setcolreg,
	.fb_blank	= s3cfb_extdsp_blank,
	.fb_pan_display	= s3cfb_extdsp_pan_display,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_cursor	= s3cfb_extdsp_cursor,
	.fb_ioctl	= s3cfb_extdsp_ioctl,
	.fb_mmap        = s3cfb_extdsp_mmap,
};

static int s3cfb_extdsp_register(void)
{
	return platform_driver_register(&s3cfb_extdsp_driver);
}

static void s3cfb_extdsp_unregister(void)
{
	platform_driver_unregister(&s3cfb_extdsp_driver);
}

late_initcall(s3cfb_extdsp_register);
module_exit(s3cfb_extdsp_unregister);
