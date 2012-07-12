/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_ISCHAIN_H
#define FIMC_IS_DEVICE_ISCHAIN_H

#include "fimc-is-mem.h"

#define FIMC_IS_A5_MEM_SIZE		(0x00A00000)
#define FIMC_IS_REGION_SIZE		(0x5000)
#define FIMC_IS_SETFILE_SIZE		(0xc0d8)
#define DRC_SETFILE_SIZE		(0x140)
#define FD_SETFILE_SIZE			(0x88*2)
#define FIMC_IS_FW_BASE_MASK		((1 << 26) - 1)
#define FIMC_IS_TDNR_MEM_SIZE		(1920*1080*4)
#define FIMC_IS_DEBUG_REGION_ADDR	(0x00840000)
#define FIMC_IS_SHARED_REGION_ADDR	(0x008C0000)

#define MAX_ISP_INTERNAL_BUF_WIDTH	(2560)  /* 4808 in HW */
#define MAX_ISP_INTERNAL_BUF_HEIGHT	(1920)  /* 3356 in HW */
#define SIZE_ISP_INTERNAL_BUF \
	(MAX_ISP_INTERNAL_BUF_WIDTH * MAX_ISP_INTERNAL_BUF_HEIGHT * 3)

#define MAX_ODC_INTERNAL_BUF_WIDTH	(2560)  /* 4808 in HW */
#define MAX_ODC_INTERNAL_BUF_HEIGHT	(1920)  /* 3356 in HW */
#define SIZE_ODC_INTERNAL_BUF \
	(MAX_ODC_INTERNAL_BUF_WIDTH * MAX_ODC_INTERNAL_BUF_HEIGHT * 3)

#define MAX_DIS_INTERNAL_BUF_WIDTH	(2400)
#define MAX_DIS_INTERNAL_BUF_HEIGHT	(1360)
#define SIZE_DIS_INTERNAL_BUF \
	(MAX_DIS_INTERNAL_BUF_WIDTH * MAX_DIS_INTERNAL_BUF_HEIGHT * 2)

#define MAX_3DNR_INTERNAL_BUF_WIDTH	(1920)
#define MAX_3DNR_INTERNAL_BUF_HEIGHT	(1088)
#define SIZE_3DNR_INTERNAL_BUF \
	(MAX_3DNR_INTERNAL_BUF_WIDTH * MAX_3DNR_INTERNAL_BUF_HEIGHT * 2)

#define NUM_ISP_INTERNAL_BUF		(3)
#define NUM_ODC_INTERNAL_BUF		(2)
#define NUM_DIS_INTERNAL_BUF		(3)
#define NUM_3DNR_INTERNAL_BUF		(2)

/*global state*/
enum fimc_is_ischain_state {
	FIMC_IS_ISCHAIN_LOADED,
	FIMC_IS_ISCHAIN_POWER_ON,
	FIMC_IS_ISCHAIN_RUN
};

/*device state*/
enum fimc_is_isdev_state {
	FIMC_IS_ISDEV_STOP,
	FIMC_IS_ISDEV_START
};

struct fimc_is_ishcain_mem {
	dma_addr_t	base;		/* buffer base */
	size_t		size;		/* total length */
	dma_addr_t	vaddr_base;	/* buffer base */
	dma_addr_t	vaddr_curr;	/* current addr */
	void		*bitproc_buf;
	void		*fw_cookie;

	u32	dvaddr;
	u32	kvaddr;
	u32	dvaddr_region;
	u32	kvaddr_region;
	u32	dvaddr_shared; /*shared region of is region*/
	u32	kvaddr_shared;
	u32	dvaddr_odc;
	u32	kvaddr_odc;
	u32	dvaddr_dis;
	u32	kvaddr_dis;
	u32	dvaddr_3dnr;
	u32	kvaddr_3dnr;
	u32	dvaddr_isp;
	u32	kvaddr_isp;
};

struct fimc_is_ischain_dev {
	u32					state;
	spinlock_t				slock_state;
};

struct fimc_is_device_ischain {
	struct platform_device			*pdev;
	struct device				*bus_dev;
	struct exynos5_platform_fimc_is		*pdata;
	void __iomem				*regs;

	struct fimc_is_ishcain_mem		minfo;

	struct fimc_is_interface		*interface;
	struct fimc_is_framemgr			*framemgr;
	struct fimc_is_mem			*mem;

	struct is_region			*is_region;

	bool					lpower;
	unsigned long				state;
	struct mutex				mutex_state;
	spinlock_t				slock_state;

	u32					instance;

	struct camera2_sm			capability;
	struct camera2_uctl			req_frame_desc;
	struct camera2_uctl			frame_desc;

	/*isp ~ scc*/
	u32					chain0_width;
	u32					chain0_height;

	/*scc ~ dis*/
	u32					chain1_width;
	u32					chain1_height;
	struct fimc_is_ischain_dev		scc;
	struct fimc_is_video_scc		*scc_video;

	/*dis ~ scp*/
	u32					chain2_width;
	u32					chain2_height;

	/*scp ~ fd*/
	u32					chain3_width;
	u32					chain3_height;
	struct fimc_is_ischain_dev		scp;
	struct fimc_is_video_scp		*scp_video;

	u32					private_data;
};

int fimc_is_ischain_probe(struct fimc_is_device_ischain *this,
	struct fimc_is_interface *interface,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_mem *mem,
	struct platform_device *pdev,
	u32 regs);
int fimc_is_ischain_open(struct fimc_is_device_ischain *this);
int fimc_is_ischain_close(struct fimc_is_device_ischain *this);
int fimc_is_ischain_init(struct fimc_is_device_ischain *this,
	u32 input, u32 channel);
int fimc_is_ischain_isp_start(struct fimc_is_device_ischain *this,
	struct fimc_is_video_common *video);
int fimc_is_ischain_scp_start(struct fimc_is_device_ischain *this);
int fimc_is_ischain_scp_stop(struct fimc_is_device_ischain *this);
int fimc_is_ischain_buffer_queue(struct fimc_is_device_ischain *this,
	u32 index);
int fimc_is_ischain_buffer_finish(struct fimc_is_device_ischain *this,
	u32 index);
int fimc_is_ischain_g_capability(struct fimc_is_device_ischain *this,
	u32 user_ptr);

int fimc_is_ischain_s_chain0(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height);

int fimc_is_ischain_s_chain1(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height);

int fimc_is_ischain_s_chain2(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height);

int fimc_is_ischain_s_chain3(struct fimc_is_device_ischain *this,
	u32 width,
	u32 height);

/*special api for sensor*/
int fimc_is_ischain_callback(struct fimc_is_device_ischain *this);

int fimc_is_itf_stream_on(struct fimc_is_device_ischain *this);
int fimc_is_itf_stream_off(struct fimc_is_device_ischain *this);
int fimc_is_itf_stream_ctl(struct fimc_is_device_ischain *this,
	u32 fcount,
	u64 *rfduration,
	u64 *rexposure,
	u32 *rsensitivity);
int fimc_is_runtime_resume(struct device *dev);
#endif
