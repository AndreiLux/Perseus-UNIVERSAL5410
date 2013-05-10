/* linux/drivers/video/samsung/s3cfb_extdsp-ops.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Middle layer file for Samsung Display Controller (FIMD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#if defined(CONFIG_S5P_MEM_CMA)
#include <linux/cma.h>
#endif

#include "s3cfb_extdsp.h"
#define NOT_DEFAULT_WINDOW 99

int s3cfb_extdsp_enable_window(struct s3cfb_extdsp_global *fbdev, int id)
{
	struct s3cfb_extdsp_window *win = fbdev->fb[id]->par;

	if (!win->enabled)
		atomic_inc(&fbdev->enabled_win);

	win->enabled = 1;
	return 0;
}

int s3cfb_extdsp_disable_window(struct s3cfb_extdsp_global *fbdev, int id)
{
	struct s3cfb_extdsp_window *win = fbdev->fb[id]->par;

	if (win->enabled)
		atomic_dec(&fbdev->enabled_win);

	win->enabled = 0;
	return 0;
}

int s3cfb_extdsp_update_power_state(struct s3cfb_extdsp_global *fbdev, int id,
		int state)
{
	struct s3cfb_extdsp_window *win = fbdev->fb[id]->par;

	win->power_state = state;
	return 0;
}

int s3cfb_extdsp_init_global(struct s3cfb_extdsp_global *fbdev)
{
	fbdev->output = OUTPUT_RGB;
	fbdev->rgb_mode = MODE_RGB_P;
	mutex_init(&fbdev->lock);
	return 0;
}

int s3cfb_extdsp_check_var_window(struct s3cfb_extdsp_global *fbdev,
		struct fb_var_screeninfo *var, struct fb_info *fb)
{
	struct s3cfb_extdsp_window *win = fb->par;
	struct s3c_fb_pd_win *lcd = fbdev->lcd;

	dev_dbg(fbdev->dev, "[fb%d] check_var\n", 0);

	if (var->bits_per_pixel != 16 && var->bits_per_pixel != 24 &&
			var->bits_per_pixel != 32) {
		dev_err(fbdev->dev, "invalid bits per pixel\n");
		return -EINVAL;
	}

	if (var->xres > lcd->width)
		var->xres = lcd->width;
	if (var->yres > lcd->height)
		var->yres = lcd->height;
	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	if (var->xoffset > (var->xres_virtual - var->xres))
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yoffset + var->yres > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;
	if (win->x + var->xres > lcd->width)
		win->x = lcd->width - var->xres;
	if (win->y + var->yres > lcd->height)
		win->y = lcd->height - var->yres;
	if (var->pixclock != fbdev->fb[0]->var.pixclock) {
		dev_info(fbdev->dev, "pixclk is changed from %d Hz to %d Hz\n",
				fbdev->fb[0]->var.pixclock, var->pixclock);
	}

	return 0;
}

int s3cfb_extdsp_check_var(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	s3cfb_extdsp_check_var_window(fbdev, var, fb);
	return 0;
}

int s3cfb_extdsp_set_par_window(struct s3cfb_extdsp_global *fbdev, struct fb_info *fb)
{
	dev_dbg(fbdev->dev, "[fb%d] set_par\n", 0);
	return 0;
}

int s3cfb_extdsp_set_par(struct fb_info *fb)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	s3cfb_extdsp_set_par_window(fbdev, fb);
	return 0;
}

int s3cfb_extdsp_init_fbinfo(struct s3cfb_extdsp_global *fbdev, int id)
{
	struct fb_info *fb = fbdev->fb[id];
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	struct s3cfb_extdsp_window *win = fb->par;
	struct s3c_fb_pd_win *lcd = fbdev->lcd;

	memset(win, 0, sizeof(struct s3cfb_extdsp_window));
	platform_set_drvdata(to_platform_device(fbdev->dev), fb);
	strcpy(fix->id, S3CFB_EXTDSP_NAME);

	/* extdsp specific */
	s3cfb_extdsp_update_power_state(fbdev, 0, FB_BLANK_POWERDOWN);

	/* fbinfo */
	fb->fbops = &s3cfb_extdsp_ops;
	fb->flags = FBINFO_FLAG_DEFAULT;
	fb->pseudo_palette = &win->pseudo_pal;
#if (CONFIG_FB_S5P_EXTDSP_NR_BUFFERS != 1)
	fix->xpanstep = 2;
	fix->ypanstep = 1;
#else
	fix->xpanstep = 0;
	fix->ypanstep = 0;
#endif
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->accel = FB_ACCEL_NONE;
	fix->visual = FB_VISUAL_TRUECOLOR;
	var->xres = 1920;
	var->yres = 1080;

	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres * CONFIG_FB_S5P_EXTDSP_NR_BUFFERS;

	var->bits_per_pixel = 32;
	var->xoffset = 0;
	var->yoffset = 0;
	var->width = 0;
	var->height = 0;
	var->transp.length = 8;

	fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;
	fix->smem_len = fix->line_length * var->yres_virtual;

	var->nonstd = 0;
	var->activate = FB_ACTIVATE_NOW;
	var->vmode = FB_VMODE_NONINTERLACED;
	var->hsync_len = 0;
	var->vsync_len = 0;
	var->left_margin = 0;
	var->right_margin = 0;
	var->upper_margin = 0;
	var->lower_margin = 0;
	var->pixclock = 0;

#if 0
	var->red.length = 8;
	var->red.offset = 0;
	var->green.length = 8;
	var->green.offset = 8;
	var->blue.length = 8;
	var->blue.offset = 16;
	var->transp.length = 8;
	var->transp.offset = 24;
#else
        /* BGRA order */
	var->red.length = 8;
	var->red.offset = 16;
	var->green.length = 8;
	var->green.offset = 8;
	var->blue.length = 8;
	var->blue.offset = 0;
	var->transp.length = 8;
	var->transp.offset = 24;
#endif

	return 0;
}

int s3cfb_extdsp_alloc_framebuffer(struct s3cfb_extdsp_global *fbdev, int extdsp_id)
{
	int ret = 0;

	fbdev->fb = kmalloc(sizeof(struct fb_info *), GFP_KERNEL);
	if (!fbdev->fb) {
		dev_err(fbdev->dev, "not enough memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	fbdev->fb[0] = framebuffer_alloc(sizeof(struct s3cfb_extdsp_window),
			fbdev->dev);
	if (!fbdev->fb[0]) {
		dev_err(fbdev->dev, "not enough memory\n");
		ret = -ENOMEM;
		goto err_alloc_fb;
	}

	ret = s3cfb_extdsp_init_fbinfo(fbdev, 0);
	if (ret) {
		dev_err(fbdev->dev,
				"failed to allocate memory for fb%d\n", 0);
		ret = -ENOMEM;
		goto err_alloc_fb;
	}

	return 0;

err_alloc_fb:
	if (fbdev->fb[0])
		framebuffer_release(fbdev->fb[0]);
	kfree(fbdev->fb);

err_alloc:
	return ret;
}

int s3cfb_extdsp_open(struct fb_info *fb, int user)
{
	struct s3cfb_extdsp_window *win = fb->par;
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	int ret = 0;

	printk("[VFB] %s\n", __func__);

	mutex_lock(&fbdev->lock);

	if (atomic_read(&win->in_use)) {
		dev_dbg(fbdev->dev, "mutiple open fir default window\n");
		ret = 0;
	} else {
		atomic_inc(&win->in_use);
	}

	mutex_unlock(&fbdev->lock);

	return ret;
}

int s3cfb_extdsp_release_window(struct fb_info *fb)
{
	struct s3cfb_extdsp_window *win = fb->par;
	win->x = 0;
	win->y = 0;

	return 0;
}

int s3cfb_extdsp_release(struct fb_info *fb, int user)
{
	struct s3cfb_extdsp_window *win = fb->par;
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	int i;

	printk("[VFB] %s\n", __func__);

	s3cfb_extdsp_release_window(fb);

	mutex_lock(&fbdev->lock);
	atomic_dec(&win->in_use);
	mutex_unlock(&fbdev->lock);

	for (i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
		if (fbdev->buf_list[i].dma_buf) {
#if 0
			dma_buf_put(fbdev->buf_list[i].dma_buf);
			fbdev->buf_list[i].dma_buf = NULL;
			fbdev->buf_list[i].dma_buf_uv = NULL;
#endif
			printk("fbdev->buf_list[%d].dma_buf: %p\n", i, fbdev->buf_list[i].dma_buf);
		}
	}

	return 0;
}

int s3cfb_extdsp_unmap_fd(void)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	int i;

	printk("[VFB] %s\n", __func__);

	for (i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
		if (fbdev->buf_list[i].dma_buf) {
			dma_buf_put(fbdev->buf_list[i].dma_buf);
			fbdev->buf_list[i].dma_buf = NULL;
			fbdev->buf_list[i].dma_buf_uv = NULL;
			printk("fbdev->buf_list[%d].dma_buf: %p\n", i, fbdev->buf_list[i].dma_buf);
		}
	}
	return 0;
}

inline unsigned int __chan_to_field_extdsp(unsigned int chan, struct fb_bitfield bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf.length;

	return chan << bf.offset;
}

int s3cfb_extdsp_setcolreg(unsigned int regno, unsigned int red,
		unsigned int green, unsigned int blue,
		unsigned int transp, struct fb_info *fb)
{
	unsigned int *pal = (unsigned int *)fb->pseudo_palette;
	unsigned int val = 0;

	if (regno < 16) {
		/* fake palette of 16 colors */
		val |= __chan_to_field_extdsp(red, fb->var.red);
		val |= __chan_to_field_extdsp(green, fb->var.green);
		val |= __chan_to_field_extdsp(blue, fb->var.blue);
		val |= __chan_to_field_extdsp(transp, fb->var.transp);
		pal[regno] = val;
	}

	return 0;
}

int s3cfb_extdsp_blank(int blank_mode, struct fb_info *fb)
{
	struct s3cfb_extdsp_window *win = fb->par;
	struct s3cfb_extdsp_window *tmp_win;
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	int enabled_win = 0;
	int i;

	dev_dbg(fbdev->dev, "change blank mode\n");
	printk("[VFB] %s: blank_mode: %d\n", __func__, blank_mode);
	printk("FB_BLANK_UNBLANK: %d, FB_BLANK_NORMAL: %d, FB_BLANK_POWERDOWN: %d\n",
			FB_BLANK_UNBLANK, FB_BLANK_NORMAL, FB_BLANK_POWERDOWN);

	switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			if (!fb->fix.smem_start) {
				dev_info(fbdev->dev, "[fb%d] no allocated memory \
						for unblank\n", 0);
				break;
			}

			if (win->power_state == FB_BLANK_UNBLANK) {
				dev_info(fbdev->dev, "[fb%d] already in \
						FB_BLANK_UNBLANK\n", 0);
				break;
			} else {
				s3cfb_extdsp_update_power_state(fbdev, 0,
						FB_BLANK_UNBLANK);
			}

			enabled_win = atomic_read(&fbdev->enabled_win);
			if (enabled_win == 0) {
				s3cfb_extdsp_init_global(fbdev);
				for (i = 0; i < 1; i++) {
					tmp_win = fbdev->fb[i]->par;
				}
			}

			if (!win->enabled)	/* from FB_BLANK_NORMAL */
				s3cfb_extdsp_enable_window(fbdev, 0);
			break;

		case FB_BLANK_NORMAL:
			if (win->power_state == FB_BLANK_NORMAL) {
				dev_info(fbdev->dev, "[fb%d] already in FB_BLANK_NORMAL\n", 0);
				break;
			} else {
				s3cfb_extdsp_update_power_state(fbdev, 0,
						FB_BLANK_NORMAL);
			}

			enabled_win = atomic_read(&fbdev->enabled_win);
			if (enabled_win == 0) {
				s3cfb_extdsp_init_global(fbdev);

				for (i = 0; i < 1; i++) {
					tmp_win = fbdev->fb[i]->par;
				}
			}

			if (!win->enabled)	/* from FB_BLANK_POWERDOWN */
				s3cfb_extdsp_enable_window(fbdev, 0);
			break;

		case FB_BLANK_POWERDOWN:
			if (win->power_state == FB_BLANK_POWERDOWN) {
				dev_info(fbdev->dev, "[fb%d] already in FB_BLANK_POWERDOWN\n", 0);
				break;
			} else {
				s3cfb_extdsp_update_power_state(fbdev, 0,
						FB_BLANK_POWERDOWN);
			}

			s3cfb_extdsp_disable_window(fbdev, 0);
			break;

		case FB_BLANK_VSYNC_SUSPEND:	/* fall through */
		case FB_BLANK_HSYNC_SUSPEND:	/* fall through */
		default:
			dev_dbg(fbdev->dev, "unsupported blank mode\n");
			return -EINVAL;
	}

	return NOT_DEFAULT_WINDOW;
}

int s3cfb_extdsp_pan_display(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);

	if (var->yoffset + var->yres > var->yres_virtual) {
		dev_err(fbdev->dev, "invalid yoffset value\n");
		return -EINVAL;
	}

	fb->var.xoffset = var->xoffset;
	fb->var.yoffset = var->yoffset;

#if 0
	printk("[VFB] %s: xoffset: %d\n", __func__, var->xoffset);
	printk("[VFB] %s: yoffset: %d\n", __func__, var->yoffset);
#endif

	sysfs_notify(&fbdev->dev->kobj, NULL, "pandisplay");

	dev_dbg(fbdev->dev, "[fb%d] yoffset for pan display: %d\n", 0,
			var->yoffset);

	return 0;
}

int s3cfb_extdsp_cursor(struct fb_info *fb, struct fb_cursor *cursor)
{
	/* nothing to do for removing cursor */
	return 0;
}

struct extdsp_mmap_data {
    struct device *dev;
    struct dma_buf *dmabuf[3];
    struct dma_buf_attachment *attachment[3];
    struct sg_table *sgt[3];
    atomic_t cnt;
};

static void fb_vm_open(struct vm_area_struct *vma)
{
	struct extdsp_mmap_data *data = vma->vm_private_data;
	atomic_inc(&data->cnt);
}

static void fb_vm_close(struct vm_area_struct *vma)
{
	struct extdsp_mmap_data *data = vma->vm_private_data;
	atomic_dec_return(&data->cnt); // atomic_dec_and_test(&data->cnt);
#if 0
	if (atomic_dec_and_test(&data->cnt)) {
		int i;

		for (i = 0; i < 3; i++) {
			dma_unmap_sg(data->dev, data->sgt[i]->sgl, data->sgt[i]->nents, DMA_FROM_DEVICE);
			dma_buf_unmap_attachment(data->attachment[i], data->sgt[i], DMA_FROM_DEVICE);
			dma_buf_detach(data->dmabuf[i], data->attachment[i]);
			dma_buf_put(data->dmabuf[i]);
		}
		kfree(data);
	}
#endif
}

const struct vm_operations_struct fb_vm_ops = {
	.open = fb_vm_open,
	.close = fb_vm_close,
};

#if 0
int s3cfb_extdsp_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	printk("vma->vm_end - vma->vm_start: %d\n", vma->vm_end - vma->vm_start);
	printk("vma->vm_pgoff * PAGE_SIZE: %d\n", vma->vm_pgoff * PAGE_SIZE);
	printk("fbdev->buf_list[0].dma_buf: 0x%08x\n", fbdev->buf_list[0].dma_buf);
	printk("vma->vm_page_prot: 0x%08x\n", vma->vm_page_prot);

	return dma_buf_mmap(fbdev->buf_list[0].dma_buf, vma, 0);
}
#else
int s3cfb_extdsp_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	size_t vma_size = vma->vm_end - vma->vm_start;
	struct extdsp_mmap_data *data;
	int ret = 0;
	off_t off = vma->vm_pgoff * PAGE_SIZE;
	unsigned long cur_va = vma->vm_start;
	int i;

	printk("[VFB] %s: mmap requested size: %d\n", __func__, vma_size);

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (fbdev->buf_list[0].dma_buf == NULL ||
			fbdev->buf_list[1].dma_buf == NULL ||
			fbdev->buf_list[2].dma_buf == NULL) {
		printk("[VFB] %s: buf_list[] should be set before calling mmap\n", __func__);
		printk("fbdev->buf_list[0].dma_buf: %p\n", fbdev->buf_list[0].dma_buf);
		printk("fbdev->buf_list[1].dma_buf: %p\n", fbdev->buf_list[1].dma_buf);
		printk("fbdev->buf_list[2].dma_buf: %p\n", fbdev->buf_list[2].dma_buf);
		return -EINVAL;
	}

	/* size check */
	if (vma_size > (fbdev->buf_list[0].dma_buf->size +
				fbdev->buf_list[1].dma_buf->size +
				fbdev->buf_list[2].dma_buf->size -
				off))
		return -EINVAL;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for (i = 0; i < 3; i++) {
		struct dma_buf_attachment *attachment;
		struct sg_table *sgt;
		struct scatterlist *sg;

		get_dma_buf(fbdev->buf_list[i].dma_buf);

		attachment = dma_buf_attach(fbdev->buf_list[i].dma_buf, fbdev->dev);
		if (IS_ERR(attachment)) {
			ret = PTR_ERR(attachment);
			dma_buf_put(fbdev->buf_list[i].dma_buf);
			goto err_map;
		}

		sgt = dma_buf_map_attachment(attachment, DMA_FROM_DEVICE);
		if (IS_ERR(sgt)) {
			dma_buf_detach(fbdev->buf_list[i].dma_buf, attachment);
			dma_buf_put(fbdev->buf_list[i].dma_buf);
			ret = PTR_ERR(sgt);
			goto err_map;
		}

		for (sg = sgt->sgl; sg && (off >= sg->length); sg = sg_next(sg))
			off -= sg->length;

		for (; sg && (cur_va < vma->vm_end); sg = sg_next(sg)) {
			size_t len;
			len = sg->length - off;
			if (len > (vma->vm_end - cur_va))
				len = vma->vm_end - cur_va;

			ret = remap_pfn_range(vma, cur_va, (sg_phys(sg) + off) >> PAGE_SHIFT,
					len, vma->vm_page_prot);
			if (ret) {
				dma_buf_unmap_attachment(attachment, sgt, DMA_FROM_DEVICE);
				dma_buf_detach(fbdev->buf_list[i].dma_buf, attachment);
				dma_buf_put(fbdev->buf_list[i].dma_buf);
				goto err_map;
			}

			cur_va += len;
			off = 0;
		}

		dma_map_sg(fbdev->dev, sgt->sgl, sgt->nents, DMA_FROM_DEVICE);
		data->dmabuf[i] = fbdev->buf_list[i].dma_buf;
		data->attachment[i] = attachment;
		data->sgt[i] = sgt;
	}

	data->dev = fbdev->dev;
	atomic_set(&data->cnt, 0);
	vma->vm_private_data = data;
	vma->vm_ops = &fb_vm_ops;
	fb_vm_ops.open(vma);

	return 0;
err_map:
	while (i-- > 0) {
		dma_buf_unmap_attachment(data->attachment[i], data->sgt[i], DMA_FROM_DEVICE);
		dma_buf_detach(fbdev->buf_list[i].dma_buf, data->attachment[i]);
		dma_buf_put(fbdev->buf_list[i].dma_buf);
	}
	kfree(data);

	return ret;
}
#endif

int s3cfb_extdsp_ioctl(struct fb_info *fb, unsigned int cmd, unsigned long arg)
{
	struct s3cfb_extdsp_global *fbdev = get_extdsp_global(0);
	struct fb_var_screeninfo *var = &fb->var;
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct s3cfb_extdsp_window *win = fb->par;
	struct s3c_fb_pd_win *lcd = fbdev->lcd;
	struct s3cfb_extdsp_time_stamp time_stamp;
	struct s3cfb_extdsp_time_stamp time_stamp2;
	void *argp = (void *)arg;
	int ret = 0;
	dma_addr_t start_addr = 0;
	int i;
	struct dma_buf *cur_dmabuf;
	struct dma_buf *cur_dmabuf_uv;

	union {
		struct s3cfb_extdsp_user_window user_window;
		int vsync;
		int lock;
		struct s3cfb_extdsp_buf_list buf_list;
	} p;

	switch (cmd) {
		case S3CFB_EXTDSP_LOCK_BUFFER:
			if (get_user(p.lock, (int __user *)arg))
				ret = -EFAULT;
			else
				win->lock_status = p.lock;

			if (win->lock_status == 1) { /* Locked */
				win->lock_buf_offset = 0;
				for (i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
					if (fbdev->buf_list[i].buf_status == BUF_ACTIVE) {
						fbdev->buf_list[i].buf_status = BUF_LOCKED;
						break;
					}
				}
			} else { /* Unlocked */
				for (i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
					if (fbdev->buf_list[i].buf_status == BUF_LOCKED)
						fbdev->buf_list[i].buf_status = BUF_FREE;
				}
				win->lock_buf_offset = 0;
			}
			break;

		case S3CFB_EXTDSP_WIN_POSITION:
			if (copy_from_user(&p.user_window,
						(struct s3cfb_extdsp_user_window __user *)arg,
						sizeof(p.user_window)))
				ret = -EFAULT;
			else {
				if (p.user_window.x < 0)
					p.user_window.x = 0;

				if (p.user_window.y < 0)
					p.user_window.y = 0;

				if (p.user_window.x + var->xres > lcd->width)
					win->x = lcd->width - var->xres;
				else
					win->x = p.user_window.x;

				if (p.user_window.y + var->yres > lcd->height)
					win->y = lcd->height - var->yres;
				else
					win->y = p.user_window.y;
			}
			break;

		case S3CFB_EXTDSP_GET_FB_PHY_ADDR:
			time_stamp2.y_fd = -1;
			time_stamp2.uv_fd = -1;
			for (i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
				if (fbdev->buf_list[i].buf_status == BUF_LOCKED) {
					time_stamp2.y_fd = dma_buf_fd(fbdev->buf_list[i].dma_buf, O_CLOEXEC);
					time_stamp2.uv_fd =
						dma_buf_fd(fbdev->buf_list[i].dma_buf_uv, O_CLOEXEC);
					break;
				}
			}

			if (copy_to_user((struct s3cfb_extdsp_time_stamp __user
							*)arg, &time_stamp2, sizeof(time_stamp2))) {
				dev_err(fbdev->dev, "copy_to error\n");
				return -EFAULT;
			}
			break;

		case S3CFB_EXTDSP_PUT_FD:
			printk("[VFB] %s(S3CFB_EXTDSP_PUT_FD)\n", __func__);
			printk("time_stamp.y_fd: 0x%08x\n", time_stamp.y_fd);
			printk("time_stamp.uv_fd: 0x%08x\n", time_stamp.uv_fd);
			if (copy_from_user(&time_stamp,
						(struct s3cfb_extdsp_time_stamp __user *)arg,
						sizeof(time_stamp))) {
				dev_err(fbdev->dev, "copy_from_user error\n");
				return -EFAULT;
			}

			if (time_stamp.y_fd < 0) {
				printk("[VFB] %s:, y_fd=%d out of value\n", __func__, time_stamp.y_fd);
				cur_dmabuf = NULL;
			}
			else {
				cur_dmabuf = dma_buf_get(time_stamp.y_fd);
				if (IS_ERR_OR_NULL(cur_dmabuf)) {
					printk("[VFB] %s:, y_fd=%d\n", __func__, time_stamp.y_fd);
					return PTR_ERR(cur_dmabuf);
				}
			}

			if (time_stamp.uv_fd < 0) {
				printk("[VFB] %s:, uv_fd=%d out of value\n", __func__, time_stamp.uv_fd);
				cur_dmabuf_uv = NULL;
			}
			else {
				cur_dmabuf_uv = dma_buf_get(time_stamp.uv_fd);
				if (IS_ERR_OR_NULL(cur_dmabuf_uv)) {
					printk("[VFB] %s:, uv_fd=%d\n", __func__, time_stamp.uv_fd);
					dma_buf_put(cur_dmabuf);
					return PTR_ERR(cur_dmabuf_uv);
				}
			}

			do_gettimeofday(&time_stamp.time_marker);

			for(i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
				if (fbdev->buf_list[i].dma_buf == cur_dmabuf) {
					printk("[VFB] %s: 99. Exist!! Already exist in buf_lists[%d]\n", __func__, i);
					fbdev->buf_list[i].time_marker = time_stamp.time_marker;
					break;
				}
				if (!fbdev->buf_list[i].dma_buf) {
					printk("[VFB] %s: 00. First!! Set the one buf_list[%d]\n", __func__, i);
					fbdev->buf_list[i].dma_buf = cur_dmabuf;
					fbdev->buf_list[i].dma_buf_uv = cur_dmabuf_uv;
					fbdev->buf_list[i].time_marker = time_stamp.time_marker;
					break;
				}
			}
			break;

		case S3CFB_EXTDSP_UNMAP_FD:
			s3cfb_extdsp_unmap_fd();
			break;

		case FBIOGET_FSCREENINFO:
			ret = memcpy(argp, &fb->fix, sizeof(fb->fix)) ? 0 : -EFAULT;
			break;

		case FBIOGET_VSCREENINFO:
			ret = memcpy(argp, &fb->var, sizeof(fb->var)) ? 0 : -EFAULT;
			break;

		case FBIOPUT_VSCREENINFO:
			ret = s3cfb_extdsp_check_var((struct fb_var_screeninfo *)argp, fb);
			if (ret) {
				dev_err(fbdev->dev, "invalid vscreeninfo\n");
				break;
			}

			ret = memcpy(&fb->var, (struct fb_var_screeninfo *)argp,
					sizeof(fb->var)) ? 0 : -EFAULT;
			if (ret) {
				dev_err(fbdev->dev, "failed to put new vscreeninfo\n");
				break;
			}

			fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;
			fix->smem_len = fix->line_length * var->yres_virtual;
			break;

		case S3CFB_EXTDSP_GET_LCD_WIDTH:
			ret = memcpy(argp, &lcd->width, sizeof(int)) ? 0 : -EFAULT;
			if (ret) {
				dev_err(fbdev->dev, "failed to S3CFB_EXTDSP_GET_LCD_WIDTH\n");
				break;
			}

			break;

		case S3CFB_EXTDSP_GET_LCD_HEIGHT:
			ret = memcpy(argp, &lcd->height, sizeof(int)) ? 0 : -EFAULT;
			if (ret) {
				dev_err(fbdev->dev, "failed to S3CFB_EXTDSP_GET_LCD_HEIGHT\n");
				break;
			}

			break;

		case S3CFB_EXTDSP_SET_WIN_ON:
			s3cfb_extdsp_enable_window(fbdev, 0);
			break;

		case S3CFB_EXTDSP_SET_WIN_OFF:
			s3cfb_extdsp_disable_window(fbdev, 0);
			break;

		case S3CFB_EXTDSP_SET_WIN_ADDR:
			cur_dmabuf = dma_buf_get((int)argp);

			if (IS_ERR_OR_NULL(cur_dmabuf))
				return PTR_ERR(cur_dmabuf);

			for (i = 0; i < CONFIG_FB_S5P_EXTDSP_NR_BUFFERS; i++) {
				if (fbdev->buf_list[i].dma_buf == cur_dmabuf)
					fbdev->buf_list[i].buf_status = BUF_ACTIVE;
				else if (fbdev->buf_list[i].buf_status != BUF_LOCKED)
					fbdev->buf_list[i].buf_status = BUF_FREE;
			}
			break;
		default:
			break;
	}

	return ret;
}
