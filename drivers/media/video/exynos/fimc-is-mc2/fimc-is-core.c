/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <linux/cma.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>

#include "fimc-is-core.h"
#include "fimc-is-helper.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-misc.h"



#if defined(CONFIG_VIDEOBUF2_ION)
static void *is_vb_cookie;
void *buf_start;

#endif

static struct fimc_is_core *to_fimc_is_dev_from_front_dev
				(struct fimc_is_front_dev *front_dev)
{
	return container_of(front_dev, struct fimc_is_core, front);
}

static struct fimc_is_device_sensor *to_fimc_is_device_sensor
				(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct fimc_is_device_sensor, sd);
}

static struct fimc_is_front_dev *to_fimc_is_front_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct fimc_is_front_dev, sd);
}

static struct fimc_is_back_dev *to_fimc_is_back_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct fimc_is_back_dev, sd);
}

static int fimc_is_sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	dbg("%s\n", __func__);
	return 0;
}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
void fimc_is_mem_cache_clean(const void *start_addr,
					unsigned long size)
{
	unsigned long paddr;

	dmac_map_area(start_addr, size, DMA_TO_DEVICE);
	/*
	 * virtual & phsical addrees mapped directly, so we can convert
	 * the address just using offset
	 */
	paddr = __pa((unsigned long)start_addr);
	outer_clean_range(paddr, paddr + size);
}

void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size)
{
	unsigned long paddr;
	paddr = __pa((unsigned long)start_addr);
	outer_inv_range(paddr, paddr + size);
	dmac_unmap_area(start_addr, size, DMA_FROM_DEVICE);
}

int fimc_is_init_mem(struct fimc_is_core *dev)
{
	struct cma_info mem_info;
	char			cma_name[16];
	int err;

	dbg("fimc_is_init_mem - ION\n");
	sprintf(cma_name, "%s%d", "fimc_is", 0);
	err = cma_info(&mem_info, &dev->pdev->dev, 0);
	dbg("%s : [cma_info] start_addr : 0x%x, end_addr : 0x%x, "
			__func__, mem_info.lower_bound, mem_info.upper_bound);
	dbg("total_size : 0x%x, free_size : 0x%x\n",
			mem_info.total_size, mem_info.free_size);
	if (err) {
		dev_err(&dev->pdev->dev, "%s: get cma info failed\n", __func__);
		return -EINVAL;
	}
	dev->mem.size = FIMC_IS_A5_MEM_SIZE;
	dev->mem.base = (dma_addr_t)cma_alloc
		(&dev->pdev->dev, cma_name, (size_t)dev->mem.size, 0);
	dev->is_p_region =
		(struct is_region *)(phys_to_virt(dev->mem.base +
				FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE));
	memset((void *)dev->is_p_region, 0,
		(unsigned long)sizeof(struct is_region));
	fimc_is_mem_cache_clean((void *)dev->is_p_region,
				FIMC_IS_REGION_SIZE+1);

	dbg("ctrl->mem.size = 0x%x\n", dev->mem.size);
	dbg("ctrl->mem.base = 0x%x\n", dev->mem.base);

	return 0;
}
#elif defined(CONFIG_VIDEOBUF2_ION)

void fimc_is_mem_init_mem_cleanup(void *alloc_ctxes)
{
	vb2_ion_destroy_context(alloc_ctxes);
}

void fimc_is_mem_resume(void *alloc_ctxes)
{
	vb2_ion_attach_iommu(alloc_ctxes);
}

void fimc_is_mem_suspend(void *alloc_ctxes)
{
	vb2_ion_detach_iommu(alloc_ctxes);
}

int fimc_is_cache_flush(struct fimc_is_core *dev,
				const void *start_addr, unsigned long size)
{
	vb2_ion_sync_for_device(dev->mem.fw_cookie,
		(unsigned long)start_addr - (unsigned long)dev->mem.kvaddr,
			size, DMA_BIDIRECTIONAL);
	return 0;
}

/* Allocate firmware */
int fimc_is_alloc_firmware(struct fimc_is_core *dev)
{
	void *fimc_is_bitproc_buf;
	int ret;

	dbg("Allocating memory for FIMC-IS firmware.\n");

	fimc_is_bitproc_buf = vb2_ion_private_alloc(dev->alloc_ctx,
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
				SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF +
				SIZE_ISP_INTERNAL_BUF * NUM_ISP_INTERNAL_BUF);
	if (IS_ERR(fimc_is_bitproc_buf)) {
		fimc_is_bitproc_buf = 0;
		err("Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	ret = vb2_ion_dma_address(fimc_is_bitproc_buf, &dev->mem.dvaddr);
	if ((ret < 0) || (dev->mem.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.\n");
		vb2_ion_private_free(fimc_is_bitproc_buf);
		dev->mem.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg("Device vaddr = %08x , size = %08x\n",
				dev->mem.dvaddr, FIMC_IS_A5_MEM_SIZE);

	dev->mem.kvaddr = vb2_ion_private_vaddr(fimc_is_bitproc_buf);
	if (IS_ERR(dev->mem.kvaddr)) {
		err("Bitprocessor memory remap failed\n");
		vb2_ion_private_free(fimc_is_bitproc_buf);
		dev->mem.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg("Virtual address for FW: %08lx\n",
			(long unsigned int)dev->mem.kvaddr);
	dev->mem.bitproc_buf = fimc_is_bitproc_buf;
	dev->mem.fw_cookie = fimc_is_bitproc_buf;

	is_vb_cookie = dev->mem.fw_cookie;
	buf_start = dev->mem.kvaddr;
	return 0;
}

void fimc_is_mem_cache_clean(const void *start_addr,
				unsigned long size)
{
	off_t offset;

	if (start_addr < buf_start) {
		err("Start address error\n");
		return;
	}
	size--;

	offset = start_addr - buf_start;

	vb2_ion_sync_for_device(is_vb_cookie, offset, size, DMA_TO_DEVICE);
}

void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size)
{
	off_t offset;

	if (start_addr < buf_start) {
		err("Start address error\n");
		return;
	}

	offset = start_addr - buf_start;

	vb2_ion_sync_for_device(is_vb_cookie, offset, size, DMA_FROM_DEVICE);
}

int fimc_is_init_mem(struct fimc_is_core *dev)
{
	int ret;

	dbg("fimc_is_init_mem - ION\n");
	ret = fimc_is_alloc_firmware(dev);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		return -EINVAL;
	}

	memset(dev->mem.kvaddr, 0,
		FIMC_IS_A5_MEM_SIZE +
		SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
		SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
		SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF +
		SIZE_ISP_INTERNAL_BUF * NUM_ISP_INTERNAL_BUF);

	dev->is_p_region =
		(struct is_region *)(dev->mem.kvaddr +
			FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE);


	dev->is_shared_region =
		(struct is_share_region *)(dev->mem.kvaddr +
				FIMC_IS_SHARED_REGION_ADDR);

	dev->mem.dvaddr_odc = (unsigned char *)(dev->mem.dvaddr +
						FIMC_IS_A5_MEM_SIZE);
	dev->mem.kvaddr_odc = dev->mem.kvaddr + FIMC_IS_A5_MEM_SIZE;

	dev->mem.dvaddr_dis = (unsigned char *)
				(dev->mem.dvaddr +
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
	dev->mem.kvaddr_dis = dev->mem.kvaddr  +
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF;

	dev->mem.dvaddr_3dnr = (unsigned char *)
				(dev->mem.dvaddr +
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
	dev->mem.kvaddr_3dnr = dev->mem.kvaddr +
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;

	dev->mem.dvaddr_isp = (unsigned char *)
				(dev->mem.dvaddr +
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
				SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF);
	dev->mem.kvaddr_isp = dev->mem.kvaddr +
				FIMC_IS_A5_MEM_SIZE +
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
				SIZE_3DNR_INTERNAL_BUF * NUM_3DNR_INTERNAL_BUF;

	dev->mem.dvaddr_shared = (unsigned char *)dev->mem.dvaddr +
			((unsigned char *)&dev->is_p_region->shared[0] -
			dev->mem.kvaddr);
	dev->mem.kvaddr_shared = dev->mem.kvaddr +
			((unsigned char *)&dev->is_p_region->shared[0] -
			dev->mem.kvaddr);

	if (fimc_is_cache_flush(dev, (void *)dev->is_p_region, IS_PARAM_SIZE)) {
		err("fimc_is_cache_flush-Err\n");
		return -EINVAL;
	}
	dbg("fimc_is_init_mem3\n");
	return 0;
}
#endif

/*#define SDCARD_FW*/
#define FIMC_IS_FW_SDCARD "/sdcard/fimc_is_fw2.bin"

static int fimc_is_request_firmware(struct fimc_is_core *dev)
{
	int ret;
	struct firmware *fw_blob;
#ifdef SDCARD_FW
	u8 *buf = NULL;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;

	ret = 0;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(FIMC_IS_FW_SDCARD, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		dbg("failed to open %s\n", FIMC_IS_FW_SDCARD);
		goto request_fw;
	}
	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;
	dbg("start, file path %s, size %ld Bytes\n", FIMC_IS_FW_SDCARD, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		err("failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	memcpy((void *)phys_to_virt(dev->mem.base), (void *)buf, fsize);
	fimc_is_mem_cache_clean((void *)phys_to_virt(dev->mem.base),
		fsize + 1);
	memcpy((void *)dev->fw.fw_info, (buf + fsize - 0x1F), 0x17);
	memcpy((void *)dev->fw.fw_version, (buf + fsize - 0x7), 0x6);
#elif defined(CONFIG_VIDEOBUF2_ION)
	if (dev->mem.bitproc_buf == 0) {
		err("failed to load FIMC-IS F/W, FIMC-IS will not working\n");
	} else {
		memcpy(dev->mem.kvaddr, (void *)buf, fsize);
		fimc_is_mem_cache_clean((void *)dev->mem.kvaddr, fsize + 1);
		/*memcpy((void *)dev->fw.fw_info, (buf + fsize - 0x1F), 0x17);
		memcpy((void *)dev->fw.fw_version, (buf + fsize - 0x7), 0x6);*/
	}
#endif
	dev->fw.state = 1;
request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif
		ret = request_firmware((const struct firmware **)&fw_blob,
			FIMC_IS_FW, &dev->pdev->dev);
		if (ret) {
			dev_err(&dev->pdev->dev,
				"could not load firmware (err=%d)\n", ret);
			return -EINVAL;
		}
#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
		memcpy((void *)phys_to_virt(dev->mem.base),
			fw_blob->data, fw_blob->size);
		fimc_is_mem_cache_clean((void *)phys_to_virt(dev->mem.base),
			fw_blob->size + 1);
#elif defined(CONFIG_VIDEOBUF2_ION)
		if (dev->mem.bitproc_buf == 0) {
			err("failed to load FIMC-IS F/W\n");
			return -EINVAL;
		} else {
		    memcpy(dev->mem.kvaddr, fw_blob->data, fw_blob->size);
		    fimc_is_mem_cache_clean(
				(void *)dev->mem.kvaddr, fw_blob->size + 1);
		    dbg("FIMC_IS F/W loaded successfully - size:%d\n",
				fw_blob->size);
		}
#endif

#ifndef SDCARD_FW
		dev->fw.state = 1;
#endif
		printk(KERN_INFO "FIMC_IS F/W loaded successfully - size:%d name : %s\n",
			fw_blob->size, FIMC_IS_FW);
		release_firmware(fw_blob);
#ifdef SDCARD_FW
	}
#endif

#ifdef SDCARD_FW
out:
	if (!fw_requested) {
		vfree(buf);
		filp_close(fp, current->files);
		set_fs(old_fs);
	}
#endif
	dbg(KERN_INFO "FIMC_IS FW loaded = 0x%08x\n", dev->mem.base);
	return ret;
}

int fimc_is_load_fw(struct fimc_is_core *dev)
{
	int ret;

	dbg("%s\n", __func__);
	if (test_bit(IS_ST_IDLE, &dev->state)) {
		/* 1. Load IS firmware */
		ret = fimc_is_request_firmware(dev);
		if (ret) {
			err("failed to fimc_is_request_firmware (%d)\n", ret);
			return -EINVAL;
		}

		set_bit(IS_ST_PWR_ON, &dev->state);

		/* 3. A5 power on */
		clear_bit(IS_ST_FW_DOWNLOADED, &dev->state);
		fimc_is_hw_a5_power(dev, 1);
		dbg("fimc_is_load_fw end\n");
	} else {
		dbg("IS FW was loaded before\n");
	}
	return 0;
}

int fimc_is_load_setfile(struct fimc_is_core *dev)
{
	int ret;
	struct firmware *fw_blob;

	ret = request_firmware((const struct firmware **)&fw_blob,
				FIMC_IS_SETFILE, &dev->pdev->dev);
	if (ret) {
		dev_err(&dev->pdev->dev,
			"could not load firmware (err=%d)\n", ret);
		return -EINVAL;
	}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	memcpy((void *)phys_to_virt(dev->mem.base + dev->setfile.base),
		fw_blob->data, fw_blob->size);
	fimc_is_mem_cache_clean(
		(void *)phys_to_virt(dev->mem.base + dev->setfile.base),
		fw_blob->size + 1);
#elif defined(CONFIG_VIDEOBUF2_ION)
	if (dev->mem.bitproc_buf == 0) {
		err("failed to load setfile\n");
	} else {
		memcpy((dev->mem.kvaddr + dev->setfile.base),
					fw_blob->data, fw_blob->size);
		fimc_is_mem_cache_clean(
				(void *)(dev->mem.kvaddr + dev->setfile.base),
				fw_blob->size + 1);
	}
#endif
	dev->setfile.state = 1;
	dbg("FIMC_IS setfile loaded successfully - size:%d\n",
							fw_blob->size);
	release_firmware(fw_blob);

	dbg("A5 mem base  = 0x%08x\n", dev->mem.base);
	dbg("Setfile base  = 0x%08x\n", dev->setfile.base);

	return ret;
}

static int fimc_is_front_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct fimc_is_front_dev *front_dev = to_fimc_is_front_dev(sd);
	struct fimc_is_core *isp = to_fimc_is_dev_from_front_dev(front_dev);
	int ret;

	if (enable) {
		dbg("fimc_is_front_s_stream : ON\n");
	} else {
		dbg("fimc_is_front_s_stream : OFF\n");
		fimc_is_hw_subip_poweroff(isp);
		ret = wait_event_timeout(isp->irq_queue,
			!test_bit(FIMC_IS_PWR_ST_POWER_ON_OFF, &isp->power),
			FIMC_IS_SHUTDOWN_TIMEOUT);
		if (!ret) {
			dev_err(&isp->pdev->dev,
				"wait timeout : %s\n", __func__);
			return -EBUSY;
		}
		fimc_is_hw_a5_power(isp, 0);
		isp->state = 0;
		isp->pipe_state = 0;
		stop_mipi_csi(isp->pdata->sensor_info[0]->csi_id);
		stop_mipi_csi(isp->pdata->sensor_info[1]->csi_id);
		stop_fimc_lite(isp->pdata->sensor_info[0]->flite_id);
		stop_fimc_lite(isp->pdata->sensor_info[1]->flite_id);

		if (isp->pdata->clk_off) {
			/* isp->pdata->clk_off(isp->pdev); */
		} else {
			err("#### failed to Clock On ####\n");
			return -EINVAL;
		}
		set_bit(IS_ST_IDLE , &isp->state);
		dbg("state(%d), pipe_state(%d)\n",
			(int)isp->state, (int)isp->pipe_state);
	}

	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_s_stream(struct v4l2_subdev *sd, int enable)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_get_fmt(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_set_fmt(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_get_crop(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_set_crop(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_get_fmt(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_set_fmt(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_get_crop(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_set_crop(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_get_fmt(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_set_fmt(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_get_crop(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_set_crop(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_crop *crop)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_s_ctrl(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_g_ctrl(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_s_ctrl(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_g_ctrl(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	dbg("%s\n", __func__);
	return 0;
}
static int fimc_is_back_s_ctrl(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_g_ctrl(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	dbg("%s\n", __func__);
	return 0;
}

static struct v4l2_subdev_pad_ops fimc_is_sensor_pad_ops = {
	.get_fmt	= fimc_is_sensor_subdev_get_fmt,
	.set_fmt	= fimc_is_sensor_subdev_set_fmt,
	.get_crop	= fimc_is_sensor_subdev_get_crop,
	.set_crop	= fimc_is_sensor_subdev_set_crop,
};

static struct v4l2_subdev_pad_ops fimc_is_front_pad_ops = {
	.get_fmt	= fimc_is_front_subdev_get_fmt,
	.set_fmt	= fimc_is_front_subdev_set_fmt,
	.get_crop	= fimc_is_front_subdev_get_crop,
	.set_crop	= fimc_is_front_subdev_set_crop,
};

static struct v4l2_subdev_pad_ops fimc_is_back_pad_ops = {
	.get_fmt	= fimc_is_back_subdev_get_fmt,
	.set_fmt	= fimc_is_back_subdev_set_fmt,
	.get_crop	= fimc_is_back_subdev_get_crop,
	.set_crop	= fimc_is_back_subdev_set_crop,
};

static struct v4l2_subdev_video_ops fimc_is_sensor_video_ops = {
	.s_stream	= fimc_is_sensor_s_stream,
};

static struct v4l2_subdev_video_ops fimc_is_front_video_ops = {
	.s_stream	= fimc_is_front_s_stream,
};

static struct v4l2_subdev_video_ops fimc_is_back_video_ops = {
	.s_stream	= fimc_is_back_s_stream,
};

static struct v4l2_subdev_core_ops fimc_is_sensor_core_ops = {
	.s_ctrl	= fimc_is_sensor_s_ctrl,
	.g_ctrl = fimc_is_sensor_g_ctrl,
};

static struct v4l2_subdev_core_ops fimc_is_front_core_ops = {
	.s_ctrl	= fimc_is_front_s_ctrl,
	.g_ctrl = fimc_is_front_g_ctrl,
};

static struct v4l2_subdev_core_ops fimc_is_back_core_ops = {
	.s_ctrl	= fimc_is_back_s_ctrl,
	.g_ctrl = fimc_is_back_g_ctrl,
};

static struct v4l2_subdev_ops fimc_is_sensor_subdev_ops = {
	.pad	= &fimc_is_sensor_pad_ops,
	.video	= &fimc_is_sensor_video_ops,
	.core	= &fimc_is_sensor_core_ops,
};

static struct v4l2_subdev_ops fimc_is_front_subdev_ops = {
	.pad	= &fimc_is_front_pad_ops,
	.video	= &fimc_is_front_video_ops,
	.core	= &fimc_is_front_core_ops,
};

static struct v4l2_subdev_ops fimc_is_back_subdev_ops = {
	.pad	= &fimc_is_back_pad_ops,
	.video	= &fimc_is_back_video_ops,
	.core	= &fimc_is_back_core_ops,
};

static int fimc_is_sensor_init_formats(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_close(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_sensor_subdev_registered(struct v4l2_subdev *sd)
{
	dbg("%s\n", __func__);
	return 0;
}

static void fimc_is_sensor_subdev_unregistered(struct v4l2_subdev *sd)
{
	dbg("%s\n", __func__);
}

static int fimc_is_front_init_formats(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_close(struct v4l2_subdev *sd,
					 struct v4l2_subdev_fh *fh)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_front_subdev_registered(struct v4l2_subdev *sd)
{
	dbg("%s\n", __func__);
	return 0;
}

static void fimc_is_front_subdev_unregistered(struct v4l2_subdev *sd)
{
	dbg("%s\n", __func__);
}

static int fimc_is_back_init_formats(struct v4l2_subdev *sd,
					 struct v4l2_subdev_fh *fh)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_close(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	dbg("%s\n", __func__);
	return 0;
}

static int fimc_is_back_subdev_registered(struct v4l2_subdev *sd)
{
	dbg("%s\n", __func__);
	return 0;
}

static void fimc_is_back_subdev_unregistered(struct v4l2_subdev *sd)
{
	dbg("%s\n", __func__);
}

static const struct v4l2_subdev_internal_ops
			fimc_is_sensor_v4l2_internal_ops = {
	.open = fimc_is_sensor_init_formats,
	.close = fimc_is_sensor_subdev_close,
	.registered = fimc_is_sensor_subdev_registered,
	.unregistered = fimc_is_sensor_subdev_unregistered,
};

static const struct v4l2_subdev_internal_ops fimc_is_front_v4l2_internal_ops = {
	.open = fimc_is_front_init_formats,
	.close = fimc_is_front_subdev_close,
	.registered = fimc_is_front_subdev_registered,
	.unregistered = fimc_is_front_subdev_unregistered,
};

static const struct v4l2_subdev_internal_ops fimc_is_back_v4l2_internal_ops = {
	.open = fimc_is_back_init_formats,
	.close = fimc_is_back_subdev_close,
	.registered = fimc_is_back_subdev_registered,
	.unregistered = fimc_is_back_subdev_unregistered,
};

static int fimc_is_sensor_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct fimc_is_device_sensor *fimc_is_sensor;

	dbg("++%s\n", __func__);
	dbg("local->index : %d\n", local->index);
	dbg("media_entity_type(remote->entity) : %d\n",
				media_entity_type(remote->entity));

	fimc_is_sensor = to_fimc_is_device_sensor(sd);

	switch (local->index | media_entity_type(remote->entity)) {
	case FIMC_IS_SENSOR_PAD_SOURCE_FRONT | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_sensor->output = FIMC_IS_SENSOR_OUTPUT_FRONT;
		else
			fimc_is_sensor->output = FIMC_IS_SENSOR_OUTPUT_NONE;
		break;

	default:
		v4l2_err(sd, "%s : ERR link\n", __func__);
		return -EINVAL;
	}
	dbg("--%s\n", __func__);
	return 0;
}

static int fimc_is_front_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct fimc_is_front_dev *fimc_is_front = to_fimc_is_front_dev(sd);

	dbg("++%s\n", __func__);
	dbg("local->index : %d\n", local->index);

	switch (local->index | media_entity_type(remote->entity)) {
	case FIMC_IS_FRONT_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		dbg("fimc_is_front sink pad\n");
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (fimc_is_front->input
				!= FIMC_IS_FRONT_INPUT_NONE) {
				dbg("BUSY\n");
				return -EBUSY;
			}
			if (remote->index == FIMC_IS_SENSOR_PAD_SOURCE_FRONT)
				fimc_is_front->input
					= FIMC_IS_FRONT_INPUT_SENSOR;
		} else {
			fimc_is_front->input = FIMC_IS_FRONT_INPUT_NONE;
		}
		break;

	case FIMC_IS_FRONT_PAD_SOURCE_BACK | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_front->output |= FIMC_IS_FRONT_OUTPUT_BACK;
		else
			fimc_is_front->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;

	case FIMC_IS_FRONT_PAD_SOURCE_BAYER | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_front->output |= FIMC_IS_FRONT_OUTPUT_BAYER;
		else
			fimc_is_front->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;
	case FIMC_IS_FRONT_PAD_SOURCE_SCALERC | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_front->output |= FIMC_IS_FRONT_OUTPUT_SCALERC;
		else
			fimc_is_front->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;

	default:
		v4l2_err(sd, "%s : ERR link\n", __func__);
		return -EINVAL;
	}
	dbg("--%s\n", __func__);
	return 0;
}

static int fimc_is_back_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct fimc_is_back_dev *fimc_is_back = to_fimc_is_back_dev(sd);

	dbg("++%s\n", __func__);
	switch (local->index | media_entity_type(remote->entity)) {
	case FIMC_IS_BACK_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		dbg("fimc_is_back sink pad\n");
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (fimc_is_back->input != FIMC_IS_BACK_INPUT_NONE) {
				dbg("BUSY\n");
				return -EBUSY;
			}
			if (remote->index == FIMC_IS_FRONT_PAD_SOURCE_BACK)
				fimc_is_back->input = FIMC_IS_BACK_INPUT_FRONT;
		} else {
			fimc_is_back->input = FIMC_IS_FRONT_INPUT_NONE;
		}
		break;
	case FIMC_IS_BACK_PAD_SOURCE_3DNR | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_back->output |= FIMC_IS_BACK_OUTPUT_3DNR;
		else
			fimc_is_back->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;
	case FIMC_IS_BACK_PAD_SOURCE_SCALERP | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			fimc_is_back->output |= FIMC_IS_BACK_OUTPUT_SCALERP;
		else
			fimc_is_back->output = FIMC_IS_FRONT_OUTPUT_NONE;
		break;
	default:
		v4l2_err(sd, "%s : ERR link\n", __func__);
		return -EINVAL;
	}
	dbg("--%s\n", __func__);
	return 0;
}

static const struct media_entity_operations fimc_is_sensor_media_ops = {
	.link_setup = fimc_is_sensor_link_setup,
};

static const struct media_entity_operations fimc_is_front_media_ops = {
	.link_setup = fimc_is_front_link_setup,
};

static const struct media_entity_operations fimc_is_back_media_ops = {
	.link_setup = fimc_is_back_link_setup,
};

int fimc_is_pipeline_s_stream_preview(struct media_entity *start_entity, int on)
{
	struct media_pad *pad = &start_entity->pads[0];
	struct v4l2_subdev *back_sd;
	struct v4l2_subdev *front_sd;
	struct v4l2_subdev *sensor_sd;
	int	ret;

	dbg("--%s\n", __func__);

	pad = media_entity_remote_source(pad);
	if (media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV
			|| pad == NULL)
		dbg("cannot find back entity\n");

	back_sd = media_entity_to_v4l2_subdev(pad->entity);

	pad = &pad->entity->pads[0];

	pad = media_entity_remote_source(pad);
	if (media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV
			|| pad == NULL)
		dbg("cannot find front entity\n");

	front_sd = media_entity_to_v4l2_subdev(pad->entity);

	pad = &pad->entity->pads[0];

	pad = media_entity_remote_source(pad);
	if (media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV
			|| pad == NULL)
		dbg("cannot find sensor entity\n");

	sensor_sd = media_entity_to_v4l2_subdev(pad->entity);

	if (on) {

		ret = v4l2_subdev_call(sensor_sd, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		ret = v4l2_subdev_call(front_sd, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		ret = v4l2_subdev_call(back_sd, video, s_stream, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

	} else {
		ret = v4l2_subdev_call(back_sd, video, s_stream, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(front_sd, video, s_stream, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
		ret = v4l2_subdev_call(sensor_sd, video, s_stream, 0);
	}

	return ret == -ENOIOCTLCMD ? 0 : ret;
}


static int fimc_is_suspend(struct device *dev)
{
	printk(KERN_INFO "FIMC_IS Suspend\n");
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	printk(KERN_INFO "FIMC_IS Resume\n");
	return 0;
}

static int fimc_is_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *isp
		= (struct fimc_is_core *)platform_get_drvdata(pdev);
	int ret = 0;

	printk(KERN_INFO "FIMC_IS runtime suspend\n");

#if defined(CONFIG_VIDEOBUF2_ION)
	if (isp->alloc_ctx)
		fimc_is_mem_suspend(isp->alloc_ctx);
#endif

	if (isp->pdata->clk_off) {
		isp->pdata->clk_off(isp->pdev, 0);
	} else {
		err("failed to clock on\n");
		return -EINVAL;
	}

	if (isp->pdata->sensor_power_off) {
		isp->pdata->sensor_power_off(isp->pdev,
					0);
	} else {
		err("failed to sensor_power_off\n");
		return -EINVAL;
	}

	return ret;
}

static int fimc_is_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *isp
		= (struct fimc_is_core *)platform_get_drvdata(pdev);
	int ret = 0;

	printk(KERN_INFO "FIMC_IS runtime resume\n");


	/* 1. Enable MIPI */
	enable_mipi();
	/* 2. Clock setting */
	if (isp->pdata->clk_cfg) {
		isp->pdata->clk_cfg(isp->pdev);
	} else {
		err("failed to config clock\n");
		return -EINVAL;
	}

	if (isp->pdata->sensor_power_on) {
		isp->pdata->sensor_power_on(isp->pdev,
					0);
	} else {
		err("failed to sensor_power_on\n");
		return -EINVAL;
	}
	if (isp->pdata->clk_on) {
		isp->pdata->clk_on(isp->pdev, 0);
	} else {
		err("failed to clock on\n");
		return -EINVAL;
	}

	return ret;
}

static int fimc_is_get_md_callback(struct device *dev, void *p)
{
	struct exynos_md **md_list = p;
	struct exynos_md *md = NULL;

	md = dev_get_drvdata(dev);

	if (md)
		*(md_list + md->id) = md;

	return 0; /* non-zero value stops iteration */
}

static struct exynos_md *fimc_is_get_md(enum mdev_node node)
{
	struct device_driver *drv;
	struct exynos_md *md[MDEV_MAX_NUM] = {NULL,};
	int ret;

	drv = driver_find(MDEV_MODULE_NAME, &platform_bus_type);
	if (!drv)
		return ERR_PTR(-ENODEV);

	ret = driver_for_each_device(drv, NULL, &md[0],
				     fimc_is_get_md_callback);

	return ret ? NULL : md[node];

}

static int fimc_is_probe(struct platform_device *pdev)
{
	struct resource *mem_res;
	struct resource *regs_res;
	struct fimc_is_core *isp;
	int ret = -ENODEV;

	dbg("fimc_is_front_probe\n");

	isp = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;

	isp->pdev = pdev;
	isp->pdata = pdev->dev.platform_data;
	isp->id = pdev->id;
	isp->pipe_state = 0;

	set_bit(FIMC_IS_STATE_IDLE, &isp->pipe_state);
	set_bit(IS_ST_IDLE , &isp->state);

	init_waitqueue_head(&isp->irq_queue);
	spin_lock_init(&isp->slock);
	mutex_init(&isp->vb_lock);
	mutex_init(&isp->lock);
	spin_lock_init(&isp->mcu_slock);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
				      pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	isp->regs_res = regs_res;
	isp->regs = ioremap(mem_res->start, resource_size(mem_res));
	if (!isp->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	isp->mdev = fimc_is_get_md(MDEV_ISP);
	if (IS_ERR_OR_NULL(isp->mdev))
		goto p_err3;

	dbg("fimc_is_front->mdev : 0x%p\n", isp->mdev);

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	isp->vb2 = &fimc_is_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
	isp->vb2 = &fimc_is_vb2_ion;
#endif

	isp->irq = platform_get_irq(pdev, 0);
	if (isp->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	fimc_is_sensor_probe(&isp->sensor, (void *)isp);

	/*sensor entity*/
	v4l2_subdev_init(&isp->sensor.sd, &fimc_is_sensor_subdev_ops);
	isp->sensor.sd.owner = THIS_MODULE;
	isp->sensor.sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(isp->sensor.sd.name, sizeof(isp->sensor.sd.name), "%s\n",
					FIMC_IS_SENSOR_ENTITY_NAME);

	isp->sensor.pads.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&isp->sensor.sd.entity, 1,
				&isp->sensor.pads, 0);
	if (ret < 0)
		goto p_err3;

	fimc_is_sensor_init_formats(&isp->sensor.sd, NULL);

	isp->sensor.sd.internal_ops = &fimc_is_sensor_v4l2_internal_ops;
	isp->sensor.sd.entity.ops = &fimc_is_sensor_media_ops;

	ret = v4l2_device_register_subdev(&isp->mdev->v4l2_dev,
						&isp->sensor.sd);
	if (ret)
		goto p_err3;
	/* This allows to retrieve the platform device id by the host driver */
	v4l2_set_subdevdata(&isp->sensor.sd, pdev);


	/*front entity*/
	v4l2_subdev_init(&isp->front.sd, &fimc_is_front_subdev_ops);
	isp->front.sd.owner = THIS_MODULE;
	isp->front.sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(isp->front.sd.name, sizeof(isp->front.sd.name), "%s\n",
					FIMC_IS_FRONT_ENTITY_NAME);

	isp->front.pads[FIMC_IS_FRONT_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	isp->front.pads[FIMC_IS_FRONT_PAD_SOURCE_BACK].flags
							= MEDIA_PAD_FL_SOURCE;
	isp->front.pads[FIMC_IS_FRONT_PAD_SOURCE_BAYER].flags
							= MEDIA_PAD_FL_SOURCE;
	isp->front.pads[FIMC_IS_FRONT_PAD_SOURCE_SCALERC].flags
							= MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&isp->front.sd.entity, FIMC_IS_FRONT_PADS_NUM,
				isp->front.pads, 0);
	if (ret < 0)
		goto p_err3;

	fimc_is_front_init_formats(&isp->front.sd, NULL);

	isp->front.sd.internal_ops = &fimc_is_front_v4l2_internal_ops;
	isp->front.sd.entity.ops = &fimc_is_front_media_ops;

	ret = v4l2_device_register_subdev(&isp->mdev->v4l2_dev,
							&isp->front.sd);
	if (ret)
		goto p_err3;
	/* This allows to retrieve the platform device id by the host driver */
	v4l2_set_subdevdata(&isp->front.sd, pdev);


	/*back entity*/
	v4l2_subdev_init(&isp->back.sd, &fimc_is_back_subdev_ops);
	isp->back.sd.owner = THIS_MODULE;
	isp->back.sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(isp->back.sd.name, sizeof(isp->back.sd.name), "%s\n",
					FIMC_IS_BACK_ENTITY_NAME);

	isp->back.pads[FIMC_IS_BACK_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	isp->back.pads[FIMC_IS_BACK_PAD_SOURCE_3DNR].flags
							= MEDIA_PAD_FL_SOURCE;
	isp->back.pads[FIMC_IS_BACK_PAD_SOURCE_SCALERP].flags
							= MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&isp->back.sd.entity, FIMC_IS_BACK_PADS_NUM,
				isp->back.pads, 0);
	if (ret < 0)
		goto p_err3;

	fimc_is_front_init_formats(&isp->back.sd, NULL);

	isp->back.sd.internal_ops = &fimc_is_back_v4l2_internal_ops;
	isp->back.sd.entity.ops = &fimc_is_back_media_ops;

	ret = v4l2_device_register_subdev(&isp->mdev->v4l2_dev, &isp->back.sd);
	if (ret)
		goto p_err3;

	v4l2_set_subdevdata(&isp->back.sd, pdev);


	/*real start*/
	fimc_is_frame_probe(&isp->framemgr);

	/*front video entity - scalerC */
	fimc_is_scc_video_probe(isp);

	/* back video entity - scalerP*/
	fimc_is_scp_video_probe(isp);

	/* back video entity - bayer*/
	fimc_is_sensor_video_probe(isp);

	/* back video entity - isp*/
	fimc_is_isp_video_probe(isp);

	platform_set_drvdata(pdev, isp);

	/* register subdev nodes*/
	ret = v4l2_device_register_subdev_nodes(&isp->mdev->v4l2_dev);
	if (ret)
		err("v4l2_device_register_subdev_nodes failed\n");

	/* init vb2*/
	isp->alloc_ctx = isp->vb2->init(isp);
	if (IS_ERR(isp->alloc_ctx)) {
		ret = PTR_ERR(isp->alloc_ctx);
		goto p_err1;
	}

	/* init memory*/
	ret = fimc_is_init_mem(isp);
	if (ret) {
		dbg("failed to fimc_is_init_mem (%d)\n", ret);
		goto p_err3;
	}

	fimc_is_interface_probe(&isp->interface,
		&isp->framemgr,
		mem_res,
		isp->irq,
		isp->is_p_region,
		isp->is_shared_region,
		isp);

	device_is_probe(&isp->ischain, isp);

#if defined(CONFIG_EXYNOS_DEV_PD) && defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

	printk(KERN_INFO "fimc_is_mc2 probe success\n");
	return 0;

p_err3:
	iounmap(isp->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(isp);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	dbg("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_runtime_suspend,
	.runtime_resume		= fimc_is_runtime_resume,
};

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove	= __devexit_p(fimc_is_remove),
	.driver = {
		.name	= FIMC_IS_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
	}
};

static int __init fimc_is_init(void)
{
	int ret = platform_driver_register(&fimc_is_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);
	return ret;
}

static void __exit fimc_is_exit(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
module_init(fimc_is_init);
module_exit(fimc_is_exit);

MODULE_AUTHOR("Jiyoung Shin<idon.shin@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
