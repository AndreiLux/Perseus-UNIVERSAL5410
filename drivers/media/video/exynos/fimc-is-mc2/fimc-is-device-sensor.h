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

#ifndef FIMC_IS_DEVICE_SENSOR_H
#define FIMC_IS_DEVICE_SENSOR_H

#include "fimc-is-framemgr.h"
#include "fimc-is-interface.h"
#include "fimc-is-metadata.h"
#include "fimc-is-video.h"

#define SENSOR_MAX_ENUM 10

#define AUTO_MODE

enum fimc_is_sensor_output_entity {
	FIMC_IS_SENSOR_OUTPUT_NONE = 0,
	FIMC_IS_SENSOR_OUTPUT_FRONT,
};

struct fimc_is_enum_sensor {
	u32 sensor;
	u32 width;
	u32 height;
	u32 margin_x;
	u32 margin_y;
	u32 max_framerate;
	u32 csi_ch;
	u32 flite_ch;
	u32 i2c_ch;
};

enum fimc_is_flite_state {
	FIMC_IS_FLITE_INIT,
	FIMC_IS_FLITE_START,
	FIMC_IS_FLITE_LAST_CAPTURE
};

struct fimc_is_flite_frame {
	u32 o_width;
	u32 o_height;
	u32 width;
	u32 height;
	u32 offs_h;
	u32 offs_v;
};

struct fimc_is_device_flite {
	unsigned long		state;
	u32					frame_count;
	wait_queue_head_t	wait_queue;
};

struct fimc_is_device_sensor {
	struct v4l2_subdev		sd;
	struct media_pad		pads;
	struct v4l2_mbus_framefmt	mbus_fmt;
	enum fimc_is_sensor_output_entity	output;
	int id_dual;			/* for dual camera scenario */
	int id_position;		/* 0 : rear camera, 1: front camera */
	u32 width;
	u32 height;
	u32 offset_x;
	u32 offset_y;
	int framerate_update;

	struct fimc_is_enum_sensor	enum_sensor[SENSOR_MAX_ENUM];
	struct fimc_is_enum_sensor	*active_sensor;

	struct fimc_is_interface	*interface;

	/*iky to do here*/
	struct fimc_is_framemgr		*framemgr;

	u32 flite_ch;
	u32 regs;

	u32 shot_buffer_index0;
	u32 shot_buffer_index1;
	bool last_capture;
	bool streaming;

	void *dev_data;

	struct fimc_is_device_flite flite;
};

int fimc_is_sensor_probe(struct fimc_is_device_sensor *this,
	void *data);
int fimc_is_sensor_open(struct fimc_is_device_sensor *this);
int fimc_is_sensor_close(struct fimc_is_device_sensor *this);
int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *sensor,
	struct fimc_is_video_common *video, u32 index);
int fimc_is_sensor_start_streaming(struct fimc_is_device_sensor *this,
	struct fimc_is_video_common *video);

int device_sensor_stream_on(struct fimc_is_device_sensor *sensor,
	struct fimc_is_interface *interface);

int device_sensor_stream_off(struct fimc_is_device_sensor *sensor,
	struct fimc_is_interface *interface);

int enable_mipi(void);
int start_fimc_lite(unsigned long mipi_reg_base,
	struct fimc_is_flite_frame *f_frame);
int stop_fimc_lite(int channel);
int start_mipi_csi(int channel, struct fimc_is_flite_frame *f_frame);
int stop_mipi_csi(int channel);

/*iky to do here*/
int flite_hw_get_status2(unsigned long flite_reg_base);
void flite_hw_set_status2(unsigned long flite_reg_base, u32 val);
int flite_hw_get_status1(unsigned long flite_reg_base);
void flite_hw_set_status1(unsigned long flite_reg_base, u32 val);
void flite_hw_set_start_addr(unsigned long flite_reg_base,
	u32 number, u32 addr);
void flite_hw_set_use_buffer(unsigned long flite_reg_base, u32 number);
void flite_hw_set_unuse_buffer(unsigned long flite_reg_base, u32 number);
int flite_hw_get_present_frame_buffer(unsigned long flite_reg_base);
void flite_hw_set_output_dma(unsigned long flite_reg_base, bool enable);
int init_fimc_lite(unsigned long mipi_reg_base);
void flite_hw_set_capture_start(unsigned long flite_reg_base);
void flite_hw_set_output_local(unsigned long flite_reg_base, bool enable);
u32 flite_hw_get_buffer_seq(unsigned long flite_reg_base);

#endif
