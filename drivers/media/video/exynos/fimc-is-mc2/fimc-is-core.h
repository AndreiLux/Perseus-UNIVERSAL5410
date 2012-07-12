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

#ifndef FIMC_IS_CORE_H
#define FIMC_IS_CORE_H


/*#define DEBUG*/
/*#define FIMC_IS_A5_DEBUG_ON	1*/
/*#define DBG_STREAMING*/

#define FRAME_RATE_ENABLE
/*#define ODC_ENABLE*/
#define TDNR_ENABLE
#define DIS_ENABLE
#define FD_ENABLE
#define FW_SUPPORT_FACE_AF
/*#define TASKLET*/
#define FIMCLITE
/*#ifdef AE_AWB_LOCK_UNLOCK*/

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/exynos_fimc_is.h>
#include <media/v4l2-ioctl.h>
#include <media/exynos_mc.h>
#include <media/videobuf2-core.h>
#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif defined(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif
#include "fimc-is-param.h"

#include "fimc-is-device-sensor.h"
#include "fimc-is-interface.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-device-ischain.h"

#include "fimc-is-video-sensor.h"
#include "fimc-is-video-isp.h"
#include "fimc-is-video-scc.h"
#include "fimc-is-video-scp.h"
#include "fimc-is-mem.h"

#define FIMC_IS_MODULE_NAME			"exynos5-fimc-is2"
#define FIMC_IS_SENSOR_ENTITY_NAME		"exynos5-fimc-is2-sensor"
#define FIMC_IS_FRONT_ENTITY_NAME		"exynos5-fimc-is2-front"
#define FIMC_IS_BACK_ENTITY_NAME		"exynos5-fimc-is2-back"
#define FIMC_IS_VIDEO_BAYER_NAME		"exynos5-fimc-is2-bayer"
#define FIMC_IS_VIDEO_ISP_NAME			"exynos5-fimc-is2-isp"
#define FIMC_IS_VIDEO_SCALERC_NAME		"exynos5-fimc-is2-scalerc"
#define FIMC_IS_VIDEO_3DNR_NAME			"exynos5-fimc-is2-3dnr"
#define FIMC_IS_VIDEO_SCALERP_NAME		"exynos5-fimc-is2-scalerp"

#define FIMC_IS_DEBUG_LEVEL			(3)
/*#define FPS_ENABLE				(1)*/

#define MAX_I2H_ARG				(4)

#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_SETFILE				"setfile2.bin"

#define FIMC_IS_SHUTDOWN_TIMEOUT		(10*HZ)
#define FIMC_IS_SHUTDOWN_TIMEOUT_SENSOR		(3*HZ)

#define FIMC_IS_A5_MEM_SIZE			(0x00A00000)
#define FIMC_IS_REGION_SIZE			(0x5000)
#define FIMC_IS_SETFILE_SIZE			(0xc0d8)
#define DRC_SETFILE_SIZE			(0x140)
#define FD_SETFILE_SIZE				(0x88*2)
#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_TDNR_MEM_SIZE			(1920*1080*4)
#define FIMC_IS_DEBUG_REGION_ADDR		(0x00840000)
#define FIMC_IS_SHARED_REGION_ADDR		(0x008C0000)

#define FIMC_IS_SENSOR_MAX_ENTITIES		(1)
#define FIMC_IS_SENSOR_PAD_SOURCE_FRONT	(0)
#define FIMC_IS_SENSOR_PADS_NUM		(1)

#define FIMC_IS_FRONT_MAX_ENTITIES		(1)
#define FIMC_IS_FRONT_PAD_SINK			(0)
#define FIMC_IS_FRONT_PAD_SOURCE_BACK		(1)
#define FIMC_IS_FRONT_PAD_SOURCE_BAYER		(2)
#define FIMC_IS_FRONT_PAD_SOURCE_SCALERC	(3)
#define FIMC_IS_FRONT_PADS_NUM			(4)

#define FIMC_IS_BACK_MAX_ENTITIES		(1)
#define FIMC_IS_BACK_PAD_SINK			(0)
#define FIMC_IS_BACK_PAD_SOURCE_3DNR		(1)
#define FIMC_IS_BACK_PAD_SOURCE_SCALERP	(2)
#define FIMC_IS_BACK_PADS_NUM			(3)

#define FIMC_IS_MAX_SENSOR_NAME_LEN  (16)
#define is_af_use(dev)				((dev->af.use_af) ? 1 : 0)

#define err(fmt, args...) \
	printk(KERN_ERR "%s:%d: " fmt "\n", __func__, __LINE__, ##args)

/*#define AUTO_MODE*/



#ifdef DEBUG
#define dbg(fmt, args...) \
	/*printk(KERN_DEBUG "%s:%d: " fmt "\n", __func__, __LINE__, ##args)*/

#define dbg_warning(fmt, args...) \
	printk(KERN_INFO "%s[WAR] Warning! " fmt, __func__, ##args)

#define dbg_sensor(fmt, args...) \
	printk(KERN_INFO "[SEN] " fmt, ##args)

#define dbg_isp(fmt, args...) \
	printk(KERN_INFO "[ISP] " fmt, ##args)

#define dbg_scp(fmt, args...) \
	printk(KERN_INFO "[SCP] " fmt, ##args)

#define dbg_scc(fmt, args...) \
	printk(KERN_INFO "[SCC] " fmt, ##args)

#define dbg_front(fmt, args...) \
	printk(KERN_INFO "[FRT] " fmt, ##args)

#define dbg_ischain(fmt, args...) \
	printk(KERN_INFO "[ISC] " fmt, ##args)

#define dbg_core(fmt, args...) \
	printk(KERN_INFO "[COR] " fmt, ##args)

#ifdef DBG_STREAMING
#define dbg_interface(fmt, args...) \
	printk(KERN_INFO "[ITF] " fmt, ##args)
#define dbg_frame(fmt, args...) \
	printk(KERN_INFO "[FRM] " fmt, ##args)
#else
#define dbg_interface(fmt, args...)
#define dbg_frame(fmt, args...)
#endif
#else
#define dbg(fmt, args...)
#define dbg_warning(fmt, args...)
#define dbg_sensor(fmt, args...)
#define dbg_isp(fmt, args...)
#define dbg_scp(fmt, args...)
#define dbg_scc(fmt, args...)
#define dbg_front(fmt, args...)
#define dbg_ischain(fmt, args...)
#define dbg_core(fmt, args...)
#define dbg_interface(fmt, args...)
#define dbg_frame(fmt, args...)
#endif

enum fimc_is_debug_device {
	FIMC_IS_DEBUG_MAIN = 0,
	FIMC_IS_DEBUG_EC,
	FIMC_IS_DEBUG_SENSOR,
	FIMC_IS_DEBUG_ISP,
	FIMC_IS_DEBUG_DRC,
	FIMC_IS_DEBUG_FD,
	FIMC_IS_DEBUG_SDK,
	FIMC_IS_DEBUG_SCALERC,
	FIMC_IS_DEBUG_ODC,
	FIMC_IS_DEBUG_DIS,
	FIMC_IS_DEBUG_TDNR,
	FIMC_IS_DEBUG_SCALERP
};

enum fimc_is_debug_target {
	FIMC_IS_DEBUG_UART = 0,
	FIMC_IS_DEBUG_MEMORY,
	FIMC_IS_DEBUG_DCC3
};

enum fimc_is_front_input_entity {
	FIMC_IS_FRONT_INPUT_NONE = 0,
	FIMC_IS_FRONT_INPUT_SENSOR,
};

enum fimc_is_front_output_entity {
	FIMC_IS_FRONT_OUTPUT_NONE = 0,
	FIMC_IS_FRONT_OUTPUT_BACK,
	FIMC_IS_FRONT_OUTPUT_BAYER,
	FIMC_IS_FRONT_OUTPUT_SCALERC,
};

enum fimc_is_back_input_entity {
	FIMC_IS_BACK_INPUT_NONE = 0,
	FIMC_IS_BACK_INPUT_FRONT,
};

enum fimc_is_back_output_entity {
	FIMC_IS_BACK_OUTPUT_NONE = 0,
	FIMC_IS_BACK_OUTPUT_3DNR,
	FIMC_IS_BACK_OUTPUT_SCALERP,
};

enum fimc_is_front_state {
	FIMC_IS_FRONT_ST_POWERED = 0,
	FIMC_IS_FRONT_ST_STREAMING,
	FIMC_IS_FRONT_ST_SUSPENDED,
};

enum fimc_is_video_dev_num {
	FIMC_IS_VIDEO_NUM_BAYER = 0,
	FIMC_IS_VIDEO_NUM_ISP,
	FIMC_IS_VIDEO_NUM_SCALERC,
	FIMC_IS_VIDEO_NUM_3DNR,
	FIMC_IS_VIDEO_NUM_SCALERP,
	FIMC_IS_VIDEO_MAX_NUM,
};

enum af_state {
	FIMC_IS_AF_IDLE		= 0,
	FIMC_IS_AF_SETCONFIG	= 1,
	FIMC_IS_AF_RUNNING	= 2,
	FIMC_IS_AF_LOCK		= 3,
	FIMC_IS_AF_ABORT	= 4,
};

enum af_lock_state {
	FIMC_IS_AF_UNLOCKED	= 0,
	FIMC_IS_AF_LOCKED	= 0x02
};

enum ae_lock_state {
	FIMC_IS_AE_UNLOCKED	= 0,
	FIMC_IS_AE_LOCKED	= 1
};

enum awb_lock_state {
	FIMC_IS_AWB_UNLOCKED	= 0,
	FIMC_IS_AWB_LOCKED	= 1
};

enum fimc_is_power {
	FIMC_IS_PWR_ST_BASE = 0,
	FIMC_IS_PWR_ST_POWER_ON_OFF,
	FIMC_IS_PWR_ST_STREAMING,
	FIMC_IS_PWR_ST_SUSPENDED,
	FIMC_IS_PWR_ST_RESUMED,
};

struct fimc_is_core;

struct fimc_is_front_dev {
	struct v4l2_subdev		sd;
	struct media_pad		pads[FIMC_IS_FRONT_PADS_NUM];
	struct v4l2_mbus_framefmt	mbus_fmt[FIMC_IS_FRONT_PADS_NUM];
	enum fimc_is_front_input_entity	input;
	enum fimc_is_front_output_entity	output;
	u32 width;
	u32 height;

};

struct fimc_is_back_dev {
	struct v4l2_subdev		sd;
	struct media_pad		pads[FIMC_IS_BACK_PADS_NUM];
	struct v4l2_mbus_framefmt	mbus_fmt[FIMC_IS_BACK_PADS_NUM];
	enum fimc_is_back_input_entity	input;
	enum fimc_is_back_output_entity	output;
	int	dis_on;
	int	odc_on;
	int	tdnr_on;
	u32 width;
	u32 height;
	u32 dis_width;
	u32 dis_height;
};

struct is_meminfo {
	dma_addr_t	base;		/* buffer base */
	size_t		size;		/* total length */
	dma_addr_t	vaddr_base;	/* buffer base */
	dma_addr_t	vaddr_curr;	/* current addr */
	void		*bitproc_buf;
	size_t		dvaddr;
	unsigned char	*kvaddr;
	unsigned char	*dvaddr_shared;
	unsigned char	*kvaddr_shared;
	unsigned char	*dvaddr_odc;
	unsigned char	*kvaddr_odc;
	unsigned char	*dvaddr_dis;
	unsigned char	*kvaddr_dis;
	unsigned char	*dvaddr_3dnr;
	unsigned char	*kvaddr_3dnr;
	unsigned char	*dvaddr_isp;
	unsigned char	*kvaddr_isp;
	void		*fw_cookie;

};

struct is_fw {
	const struct firmware	*info;
	int			state;
	int			ver;
};

struct is_setfile {
	const struct firmware	*info;
	int			state;
	u32			sub_index;
	u32			base;
	u32			size;
};

struct is_to_host_cmd {
	u32	cmd;
	u32	sensor_id;
	u16	num_valid_args;
	u32	arg[MAX_I2H_ARG];
};

struct is_fd_result_header {
	u32 offset;
	u32 count;
	u32 index;
	u32 target_addr;
	s32 width;
	s32 height;
};

struct is_af_info {
	u16 mode;
	u32 af_state;
	u32 af_lock_state;
	u32 ae_lock_state;
	u32 awb_lock_state;
	u16 pos_x;
	u16 pos_y;
	u16 prev_pos_x;
	u16 prev_pos_y;
	u16 use_af;
};

struct fimc_is_core {
	struct platform_device			*pdev;
	struct exynos5_platform_fimc_is	*pdata; /* depended on isp */
	struct exynos_md			*mdev;
	spinlock_t				slock;
	struct mutex				lock;

	/*iky*/
	spinlock_t				mcu_slock;

	struct fimc_is_mem			mem;
	struct fimc_is_framemgr			framemgr_sensor;
	struct fimc_is_framemgr			framemgr_ischain;
	struct fimc_is_interface		interface;

	struct fimc_is_device_sensor	sensor;
	struct fimc_is_device_ischain	ischain;
	struct fimc_is_front_dev		front;
	struct fimc_is_back_dev			back;

	/* 0-bayer, 1-scalerC, 2-3DNR, 3-scalerP */
	struct fimc_is_video_sensor		video_sensor;
	struct fimc_is_video_isp		video_isp;
	struct fimc_is_video_scc		video_scc;
	struct fimc_is_video_scp		video_scp;

	/*struct vb2_alloc_ctx			*alloc_ctx;*/

	struct resource				*regs_res;
	void __iomem				*regs;
	int					irq;
	unsigned long				state;
	unsigned long				power;
	unsigned long				pipe_state;
	wait_queue_head_t			irq_queue;
	u32					id;
	struct is_fw				fw;
	struct is_setfile			setfile;
	struct is_to_host_cmd			i2h_cmd;
	struct is_fd_result_header		fd_header;

	/* Shared parameter region */
	atomic_t				p_region_num;
	unsigned long				p_region_index1;
	unsigned long				p_region_index2;
	struct is_region			*is_p_region;
	struct is_share_region		*is_shared_region;
	u32					scenario_id;
	u32					frame_count;
	u32					sensor_num;
	struct is_af_info			af;
	int					low_power_mode;
};

extern const struct v4l2_file_operations fimc_is_bayer_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_bayer_video_ioctl_ops;
extern const struct vb2_ops fimc_is_bayer_qops;

extern const struct v4l2_file_operations fimc_is_isp_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_isp_video_ioctl_ops;
extern const struct vb2_ops fimc_is_isp_qops;

extern const struct v4l2_file_operations fimc_is_scalerc_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_scalerc_video_ioctl_ops;
extern const struct vb2_ops fimc_is_scalerc_qops;

extern const struct v4l2_file_operations fimc_is_scalerp_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_scalerp_video_ioctl_ops;
extern const struct vb2_ops fimc_is_scalerp_qops;

extern const struct v4l2_file_operations fimc_is_3dnr_video_fops;
extern const struct v4l2_ioctl_ops fimc_is_3dnr_video_ioctl_ops;
extern const struct vb2_ops fimc_is_3dnr_qops;

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
extern const struct fimc_is_vb2 fimc_is_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
extern const struct fimc_is_vb2 fimc_is_vb2_ion;
#endif

void fimc_is_mem_suspend(void *alloc_ctxes);
void fimc_is_mem_resume(void *alloc_ctxes);
void fimc_is_mem_cache_clean(const void *start_addr, unsigned long size);
void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size);
int fimc_is_pipeline_s_stream_preview
	(struct media_entity *start_entity, int on);
int fimc_is_init_set(struct fimc_is_core *dev , u32 val);
int fimc_is_load_fw(struct fimc_is_core *dev);
int fimc_is_load_setfile(struct fimc_is_core *dev);

#endif /* FIMC_IS_CORE_H_ */
