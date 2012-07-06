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

#ifndef FIMC_IS_DEVICE_IS_H
#define FIMC_IS_DEVICE_IS_H

struct fimc_is_device_ischain {
	struct fimc_is_interface	*interface;
	struct is_region			*is_region;
	struct is_share_region		*is_shared_region;

	/*isp ~ scc*/
	u32							chain0_width;
	u32							chain0_height;
	/*scc ~ dis*/
	u32							chain1_width;
	u32							chain1_height;
	/*dis ~ scp*/
	u32							chain2_width;
	u32							chain2_height;
	/*scp ~ fd*/
	u32							chain3_width;
	u32							chain3_height;

	u32							private_data;
};

int device_is_probe(struct fimc_is_device_ischain *ischain, void *data);
int fimc_is_ischain_open(struct fimc_is_device_ischain *this);
int fimc_is_ischain_close(struct fimc_is_device_ischain *this);
int fimc_is_ischain_start_streaming(struct fimc_is_device_ischain *this,
	struct fimc_is_video_common *video);

int fimc_is_ischain_s_chain0(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
	u32 width,
	u32 height);

int fimc_is_ischain_s_chain1(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
	u32 width,
	u32 height);

int fimc_is_ischain_s_chain2(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
	u32 width,
	u32 height);

int fimc_is_ischain_s_chain3(struct fimc_is_device_ischain *ischain,
	struct fimc_is_interface *interface,
	u32 width,
	u32 height);

#endif
