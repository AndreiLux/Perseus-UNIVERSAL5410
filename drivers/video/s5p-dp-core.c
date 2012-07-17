/*
 * Samsung SoC DP (Display Port) interface driver.
 *
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>

#include <video/s5p-dp.h>

#include <plat/cpu.h>

#include "s5p-dp-core.h"
#include "s5p-dp-reg.h"


static int s5p_dp_init_dp(struct s5p_dp_device *dp)
{
	s5p_dp_reset(dp);

	/* SW defined function Normal operation */
	s5p_dp_enable_sw_function(dp);

	if (!soc_is_exynos5250())
		s5p_dp_config_interrupt(dp);

	s5p_dp_init_analog_func(dp);

	s5p_dp_init_hpd(dp);
	s5p_dp_init_aux(dp);

	return 0;
}

static int s5p_dp_detect_hpd(struct s5p_dp_device *dp)
{
	int timeout_loop = 0;

	s5p_dp_init_hpd(dp);

	udelay(200);

	while (s5p_dp_get_plug_in_status(dp) != 0) {
		timeout_loop++;
		if (DP_TIMEOUT_LOOP_COUNT < timeout_loop) {
			dev_err(dp->dev, "failed to get hpd plug status\n");
			return -ETIMEDOUT;
		}
		udelay(10);
	}

	return 0;
}

static unsigned char s5p_dp_calc_edid_check_sum(unsigned char *edid_data)
{
	int i;
	unsigned char sum = 0;

	for (i = 0; i < EDID_BLOCK_LENGTH; i++)
		sum = sum + edid_data[i];

	return sum;
}

static int s5p_dp_read_edid(struct s5p_dp_device *dp)
{
	unsigned char edid[EDID_BLOCK_LENGTH * 2];
	unsigned int extend_block = 0;
	unsigned char sum;
	unsigned char test_vector;
	int retval;

	/*
	 * EDID device address is 0x50.
	 * However, if necessary, you must have set upper address
	 * into E-EDID in I2C device, 0x30.
	 */

	/* Read Extension Flag, Number of 128-byte EDID extension blocks */
	s5p_dp_read_byte_from_i2c(dp, I2C_EDID_DEVICE_ADDR,
				EDID_EXTENSION_FLAG,
				&extend_block);

	if (extend_block > 0) {
		dev_dbg(dp->dev, "EDID data includes a single extension!\n");

		/* Read EDID data */
		retval = s5p_dp_read_bytes_from_i2c(dp, I2C_EDID_DEVICE_ADDR,
						EDID_HEADER_PATTERN,
						EDID_BLOCK_LENGTH,
						&edid[EDID_HEADER_PATTERN]);
		if (retval != 0) {
			dev_err(dp->dev, "EDID Read failed!\n");
			return -EIO;
		}
		sum = s5p_dp_calc_edid_check_sum(edid);
		if (sum != 0) {
			dev_err(dp->dev, "EDID bad checksum!\n");
			return -EIO;
		}

		/* Read additional EDID data */
		retval = s5p_dp_read_bytes_from_i2c(dp,
				I2C_EDID_DEVICE_ADDR,
				EDID_BLOCK_LENGTH,
				EDID_BLOCK_LENGTH,
				&edid[EDID_BLOCK_LENGTH]);
		if (retval != 0) {
			dev_err(dp->dev, "EDID Read failed!\n");
			return -EIO;
		}
		sum = s5p_dp_calc_edid_check_sum(&edid[EDID_BLOCK_LENGTH]);
		if (sum != 0) {
			dev_err(dp->dev, "EDID bad checksum!\n");
			return -EIO;
		}

		s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_TEST_REQUEST,
					&test_vector);
		if (test_vector & DPCD_TEST_EDID_READ) {
			s5p_dp_write_byte_to_dpcd(dp,
				DPCD_ADDR_TEST_EDID_CHECKSUM,
				edid[EDID_BLOCK_LENGTH + EDID_CHECKSUM]);
			s5p_dp_write_byte_to_dpcd(dp,
				DPCD_ADDR_TEST_RESPONSE,
				DPCD_TEST_EDID_CHECKSUM_WRITE);
		}
	} else {
		dev_info(dp->dev, "EDID data does not include any extensions.\n");

		/* Read EDID data */
		retval = s5p_dp_read_bytes_from_i2c(dp,
				I2C_EDID_DEVICE_ADDR,
				EDID_HEADER_PATTERN,
				EDID_BLOCK_LENGTH,
				&edid[EDID_HEADER_PATTERN]);
		if (retval != 0) {
			dev_err(dp->dev, "EDID Read failed!\n");
			return -EIO;
		}
		sum = s5p_dp_calc_edid_check_sum(edid);
		if (sum != 0) {
			dev_err(dp->dev, "EDID bad checksum!\n");
			return -EIO;
		}

		s5p_dp_read_byte_from_dpcd(dp,
			DPCD_ADDR_TEST_REQUEST,
			&test_vector);
		if (test_vector & DPCD_TEST_EDID_READ) {
			s5p_dp_write_byte_to_dpcd(dp,
				DPCD_ADDR_TEST_EDID_CHECKSUM,
				edid[EDID_CHECKSUM]);
			s5p_dp_write_byte_to_dpcd(dp,
				DPCD_ADDR_TEST_RESPONSE,
				DPCD_TEST_EDID_CHECKSUM_WRITE);
		}
	}

	dev_err(dp->dev, "EDID Read success!\n");
	return 0;
}

static int s5p_dp_handle_edid(struct s5p_dp_device *dp)
{
	u8 buf[12];
	int i;
	int retval;

	/* Read DPCD DPCD_ADDR_DPCD_REV~RECEIVE_PORT1_CAP_1 */
	retval = s5p_dp_read_bytes_from_dpcd(dp,
		DPCD_ADDR_DPCD_REV,
		12, buf);

	for (i = 0 ; i < 12 ; i++)
		dev_info(dp->dev, "[DPCD Addr : %2d] : %2x\n", i, buf[i]);

	return retval;
}

static void s5p_dp_enable_rx_to_enhanced_mode(struct s5p_dp_device *dp,
						bool enable)
{
	u8 data;

	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_LANE_COUNT_SET, &data);

	if (enable)
		s5p_dp_write_byte_to_dpcd(dp, DPCD_ADDR_LANE_COUNT_SET,
			DPCD_ENHANCED_FRAME_EN |
			DPCD_LANE_COUNT_SET(data));
	else
		s5p_dp_write_byte_to_dpcd(dp, DPCD_ADDR_LANE_COUNT_SET,
			DPCD_LANE_COUNT_SET(data));
}

static int s5p_dp_is_enhanced_mode_available(struct s5p_dp_device *dp)
{
	u8 data;
	int retval;

	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_MAX_LANE_COUNT, &data);
	retval = DPCD_ENHANCED_FRAME_CAP(data);

	return retval;
}

static void s5p_dp_set_enhanced_mode(struct s5p_dp_device *dp)
{
	u8 data;

	data = s5p_dp_is_enhanced_mode_available(dp);
	s5p_dp_enable_rx_to_enhanced_mode(dp, data);
	s5p_dp_enable_enhanced_mode(dp, data);
}

static void s5p_dp_training_pattern_dis(struct s5p_dp_device *dp)
{
	s5p_dp_set_training_pattern(dp, DP_NONE);

	s5p_dp_write_byte_to_dpcd(dp,
		DPCD_ADDR_TRAINING_PATTERN_SET,
		DPCD_TRAINING_PATTERN_DISABLED);
}

static void s5p_dp_set_lane_lane_pre_emphasis(struct s5p_dp_device *dp,
					int pre_emphasis, int lane)
{
	u32 val;
	u32 reg_offset[4] = {
		S5P_DP_LN0_LINK_TRAINING_CTL,
		S5P_DP_LN1_LINK_TRAINING_CTL,
		S5P_DP_LN2_LINK_TRAINING_CTL,
		S5P_DP_LN3_LINK_TRAINING_CTL,
	};

	if (lane > dp->link_train.lane_count) {
		dev_err(dp->dev, "wrong lane count\n");
		return;
	}

	val = readl(dp->reg_base + reg_offset[lane]);
	val &= (0x3 << PRE_EMPHASIS_SET_SHIFT);
	val = pre_emphasis << PRE_EMPHASIS_SET_SHIFT;

	writel(val , dp->reg_base + reg_offset[lane]);
	return;
}

static void s5p_dp_link_start(struct s5p_dp_device *dp)
{
	u8 buf[5];
	int lane;
	int lane_count;

	lane_count = dp->link_train.lane_count;

	dp->link_train.lt_state = CLOCK_RECOVERY;
	dp->link_train.eq_loop = 0;

	for (lane = 0; lane < lane_count; lane++)
		dp->link_train.cr_loop[lane] = 0;

	/* Set sink to D0 (Sink Not Ready) mode. */
	s5p_dp_write_byte_to_dpcd(dp, DPCD_ADDR_SINK_POWER_STATE,
				DPCD_SET_POWER_STATE_D0);

	/* Set link rate and count as you want to establish*/
	s5p_dp_set_link_bandwidth(dp, dp->link_train.link_rate);
	s5p_dp_set_lane_count(dp, dp->link_train.lane_count);

	/* Setup RX configuration */
	buf[0] = dp->link_train.link_rate;
	buf[1] = dp->link_train.lane_count;
	s5p_dp_write_bytes_to_dpcd(dp, DPCD_ADDR_LINK_BW_SET,
				2, buf);

	/* Set TX pre-emphasis to minimum */
	for (lane = 0; lane < lane_count; lane++)
		s5p_dp_set_lane_lane_pre_emphasis(dp,
			PRE_EMPHASIS_LEVEL_0, lane);

	/* Set training pattern 1 */
	s5p_dp_set_training_pattern(dp, TRAINING_PTN1);

	buf[0] = DPCD_SCRAMBLING_DISABLED | DPCD_TRAINING_PATTERN_1;
	buf[1] = DPCD_PRE_EMPHASIS_PATTERN2_LEVEL0 |
			DPCD_VOLTAGE_SWING_PATTERN1_LEVEL0;
	buf[2] = DPCD_PRE_EMPHASIS_PATTERN2_LEVEL0 |
			DPCD_VOLTAGE_SWING_PATTERN1_LEVEL0;
	buf[3] = DPCD_PRE_EMPHASIS_PATTERN2_LEVEL0 |
			DPCD_VOLTAGE_SWING_PATTERN1_LEVEL0;
	buf[4] = DPCD_PRE_EMPHASIS_PATTERN2_LEVEL0 |
			DPCD_VOLTAGE_SWING_PATTERN1_LEVEL0;

	s5p_dp_write_bytes_to_dpcd(dp,
		DPCD_ADDR_TRAINING_PATTERN_SET,
		5, buf);
}

static void s5p_dp_set_lane_link_training(struct s5p_dp_device *dp,
					u8 training_lane_set, int lane)
{
	u32 val;
	u32 reg_offset[4] = {
		S5P_DP_LN0_LINK_TRAINING_CTL,
		S5P_DP_LN1_LINK_TRAINING_CTL,
		S5P_DP_LN2_LINK_TRAINING_CTL,
		S5P_DP_LN3_LINK_TRAINING_CTL
	};

	if (lane > dp->link_train.lane_count) {
		dev_err(dp->dev, "wrong lane count\n");
		return;
	}

	val = readl(dp->reg_base + reg_offset[lane]);
	val &= 0x3;
	val = training_lane_set;

	writel(val, dp->reg_base + reg_offset[lane]);
	return;
}


static unsigned int s5p_dp_get_lane_link_training(struct s5p_dp_device *dp,
						int lane)
{
	u32 val;
	u8 ret;
	u32 reg_offset[4] = {
		S5P_DP_LN0_LINK_TRAINING_CTL,
		S5P_DP_LN1_LINK_TRAINING_CTL,
		S5P_DP_LN2_LINK_TRAINING_CTL,
		S5P_DP_LN3_LINK_TRAINING_CTL,
	};

	if (lane > dp->link_train.lane_count) {
		dev_err(dp->dev, "wrong lane count\n");
		return -EINVAL;
	}

	val = readl(dp->reg_base + reg_offset[lane]);
	ret = (unsigned char)(0xff & val);

	return ret;
}


static int s5p_dp_read_dpcd_lane_stat(struct s5p_dp_device *dp,
		unsigned char lane, unsigned char *status)

{
	int retval = 0;
	unsigned char buf[2] = {2, };
	u32 i;
	u8 lane_stat[4] = {0, };
	u8 shift_val[4] = {0, 4, 0, 4};

	s5p_dp_read_bytes_from_dpcd(dp,
			DPCD_ADDR_LANE0_1_STATUS, 2, buf);

	for (i = 0; i < lane; i++) {
		lane_stat[i] = (buf[(i / 2)] >> shift_val[i]) & 0x0f;
		if (lane_stat[0] != lane_stat[i]) {
			dev_err(dp->dev, "[EDP:ERR]%s:Wrong lane status\n"
				, __func__);
			return -EIO;
		}
	}
	*status = lane_stat[0];
	return retval;
}


static int s5p_dp_read_dpcd_adj_req(struct s5p_dp_device *dp,
		unsigned char lane, unsigned char *sw, unsigned char *em)
{
	int retval;

	u32 dpcdaddr;
	u8 shift_val[4] = {0, 4, 0, 4};
	u8 buf;

	dpcdaddr = DPCD_ADDR_ADJUST_REQUEST_LANE0_1 + (lane / 2);

	retval = s5p_dp_read_byte_from_dpcd(dp, dpcdaddr, &buf);

	*sw = ((buf >> shift_val[lane]) & 0x03);
	*em = ((buf >> shift_val[lane]) & 0x0c) >> 2;

	return retval;
}


static int s5p_dp_process_clock_recovery(struct s5p_dp_device *dp)
{
	u32 i;
	u32 lane_cnt;
	u8 lane_stat;
	u8 buf[5];
	u8 dpcd_req_sw;
	u8 dpcd_req_em;
	u8 lt_ctl_val[4] = {0, };

	udelay(100);

	lane_cnt = dp->link_train.lane_count;

	s5p_dp_read_dpcd_lane_stat(dp, lane_cnt, &lane_stat);

	dev_info(dp->dev, "%s : lane status : %x\n", __func__, lane_stat);

	if (lane_stat & DPCD_LANE_CR_DONE) {
		/* set training pattern 2 for EQ */
		s5p_dp_set_training_pattern(dp, TRAINING_PTN2);

		for (i = 0; i < lane_cnt; i++) {
			s5p_dp_read_dpcd_adj_req(dp, i,
					&dpcd_req_sw, &dpcd_req_em);

			lt_ctl_val[i] = dpcd_req_em << 3 | dpcd_req_sw;

			if (dpcd_req_sw == DP_VOLTAGE_SWING_LEVEL_3)
				lt_ctl_val[i] |= DP_VOLTAGE_SWING_REACH_MAX;
			if (dpcd_req_em == DP_PRE_EMPHASIS_LEVEL_3)
				lt_ctl_val[i] |= DP_PRE_EMPHASIS_REACH_MAX;

			s5p_dp_set_lane_link_training(dp, lt_ctl_val[i], i);
		}

		buf[0] = DPCD_SCRAMBLING_DISABLED | DPCD_TRAINING_PATTERN_2;
		buf[1] = lt_ctl_val[0];
		buf[2] = lt_ctl_val[1];
		buf[3] = lt_ctl_val[2];
		buf[4] = lt_ctl_val[3];

		s5p_dp_write_bytes_to_dpcd(dp,
				DPCD_ADDR_TRAINING_PATTERN_SET, 5, buf);
		dev_info(dp->dev, "DP LT : clock recovery success\n");
		dp->link_train.lt_state = EQUALIZER_TRAINING;

	} else {
		for (i = 0; i < lane_cnt ; i++) {
			lt_ctl_val[i] = s5p_dp_get_lane_link_training(dp, i);
			s5p_dp_read_dpcd_adj_req(dp, i,
					&dpcd_req_sw, &dpcd_req_em);

			if ((dpcd_req_sw == DP_VOLTAGE_SWING_LEVEL_3) ||
				(dpcd_req_em == DP_PRE_EMPHASIS_LEVEL_3)) {
					dev_err(dp->dev,
						"dp voltage pre emphasis reach max level\n");
				goto reduce_bit_rate;
			}

			if ((DPCD_VOLTAGE_SWING_GET(lt_ctl_val[i])
				== dpcd_req_sw)
				&& (DPCD_PRE_EMPHASIS_GET(lt_ctl_val[i])
				== dpcd_req_em)) {
				dp->link_train.cr_loop[i]++;
				if (dp->link_train.cr_loop[i] == MAX_CR_LOOP) {
					dev_err(dp->dev,
						"dp cr loop error reach max\n");
					goto reduce_bit_rate;
				}
			}
			lt_ctl_val[i] = dpcd_req_em << 3 | dpcd_req_sw;

			if (dpcd_req_sw == DP_VOLTAGE_SWING_LEVEL_3)
				lt_ctl_val[i] |= DP_VOLTAGE_SWING_REACH_MAX;
			if (dpcd_req_em == DP_PRE_EMPHASIS_LEVEL_3)
				lt_ctl_val[i] |= DP_PRE_EMPHASIS_REACH_MAX;

			s5p_dp_set_lane_link_training(dp, lt_ctl_val[i], i);
		}
		s5p_dp_write_bytes_to_dpcd(dp,
				DPCD_ADDR_TRAINING_LANE0_SET, 4, lt_ctl_val);

	}

	return 0;

reduce_bit_rate:
	dev_err(dp->dev, "s5p dp does not support 1.6G\n");

	s5p_dp_training_pattern_dis(dp);
	s5p_dp_set_enhanced_mode(dp);

	dp->link_train.lt_state = FAILED;

	return -EIO;
}

static int s5p_dp_process_equalizer_training(struct s5p_dp_device *dp)
{
	u32 i;
	u32 lane_cnt;
	u8 lane_stat;
	u8 sink_stat;
	u8 dpcd_req_sw;
	u8 dpcd_req_em;
	u8 interlane_aligned = 0;
	u8 lt_ctl_val[4] = {0, };
	u8 bw, lc;

	udelay(400);

	lane_cnt = dp->link_train.lane_count;

	s5p_dp_read_dpcd_lane_stat(dp, lane_cnt, &lane_stat);

	dev_info(dp->dev, "%s : lane status : %x\n", __func__, lane_stat);

	if (lane_stat & DPCD_LANE_CR_DONE) {
		s5p_dp_read_byte_from_dpcd(dp,
			DPCD_ADDR_LANE_ALIGN_STATUS_UPDATED, &sink_stat);

		interlane_aligned = (sink_stat & DPCD_INTERLANE_ALIGN_DONE);
		for (i = 0; i < lane_cnt; i++) {
			s5p_dp_read_dpcd_adj_req(dp,
					i, &dpcd_req_sw, &dpcd_req_em);

			lt_ctl_val[i] = (dpcd_req_em << 3) | dpcd_req_sw;

			if (dpcd_req_sw == DP_VOLTAGE_SWING_LEVEL_3)
				lt_ctl_val[i] |= DP_VOLTAGE_SWING_REACH_MAX;
			if (dpcd_req_em == DP_PRE_EMPHASIS_LEVEL_3)
				lt_ctl_val[i] |= DP_PRE_EMPHASIS_REACH_MAX;
		}

		if (((lane_stat & DPCD_LANE_CHANNEL_EQ_DONE) &&
			(lane_stat & DPCD_LANE_SYMBOL_LOCKED)) &&
			(interlane_aligned == DPCD_INTERLANE_ALIGN_DONE)) {

			s5p_dp_training_pattern_dis(dp);

			dev_info(dp->dev, "Link Training success!\n");

			s5p_dp_get_link_bandwidth(dp, &bw);
			dp->link_train.link_rate = bw;

			s5p_dp_get_lane_count(dp, &lc);
			dp->link_train.lane_count = lc;

			/* set enhanced mode if available */
			s5p_dp_set_enhanced_mode(dp);
			dp->link_train.lt_state = FINISHED;
		} else {
			dp->link_train.eq_loop++;
			if (dp->link_train.eq_loop > MAX_EQ_LOOP) {
				dev_err(dp->dev, "exceed max eq loop try count\n");
				goto reduce_bit_rate;
			}

			for (i = 0; i < lane_cnt; i++)
				s5p_dp_set_lane_link_training(dp,
					lt_ctl_val[i], i);

			s5p_dp_write_bytes_to_dpcd(dp,
				DPCD_ADDR_TRAINING_LANE0_SET, 4, lt_ctl_val);
		}

	} else {
		goto reduce_bit_rate;
	}
	return 0;

reduce_bit_rate:
	dev_err(dp->dev, "s5p dp does not support 1.6G\n");
	s5p_dp_training_pattern_dis(dp);
	dev_err(dp->dev, "fail to set dp enhanced mode\n");
	dp->link_train.lt_state = FAILED;
	return -EIO;
}

static void s5p_dp_get_max_rx_bandwidth(struct s5p_dp_device *dp,
			u8 *bandwidth)
{
	u8 data;

	/*
	 * For DP rev.1.1, Maximum link rate of Main Link lanes
	 * 0x06 = 1.62 Gbps, 0x0a = 2.7 Gbps
	 */
	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_MAX_LINK_RATE, &data);
	*bandwidth = data;
}

static void s5p_dp_get_max_rx_lane_count(struct s5p_dp_device *dp,
			u8 *lane_count)
{
	u8 data;

	/*
	 * For DP rev.1.1, Maximum number of Main Link lanes
	 * 0x01 = 1 lane, 0x02 = 2 lanes, 0x04 = 4 lanes
	 */
	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_MAX_LANE_COUNT, &data);
	*lane_count = DPCD_MAX_LANE_COUNT(data);
}

static void s5p_dp_init_training(struct s5p_dp_device *dp,
			enum link_lane_count_type max_lane,
			enum link_rate_type max_rate)
{
	/*
	 * MACRO_RST must be applied after the PLL_LOCK to avoid
	 * the DP inter pair skew issue for at least 10 us
	 */
	s5p_dp_reset_macro(dp);

	/* Initialize by reading RX's DPCD */
	s5p_dp_get_max_rx_bandwidth(dp, &dp->link_train.link_rate);
	s5p_dp_get_max_rx_lane_count(dp, &dp->link_train.lane_count);

	if ((dp->link_train.link_rate != LINK_RATE_1_62GBPS) &&
	   (dp->link_train.link_rate != LINK_RATE_2_70GBPS)) {
		dev_err(dp->dev, "Rx Max Link Rate is abnormal :%x !\n",
			dp->link_train.link_rate);
		dp->link_train.link_rate = LINK_RATE_1_62GBPS;
	}

	if (dp->link_train.lane_count == 0) {
		dev_err(dp->dev, "Rx Max Lane count is abnormal :%x !\n",
			dp->link_train.lane_count);
		dp->link_train.lane_count = (u8)LANE_COUNT1;
	}

	/* Setup TX lane count & rate */
	if (dp->link_train.lane_count > max_lane)
		dp->link_train.lane_count = max_lane;
	if (dp->link_train.link_rate > max_rate)
		dp->link_train.link_rate = max_rate;

	/* All DP analog module power up */
	s5p_dp_set_analog_power_down(dp, POWER_ALL, 0);
}

static int s5p_dp_sw_link_training(struct s5p_dp_device *dp)
{
	int retval = 0;
	int training_finished = 0;

	dp->link_train.lt_state = START;

	/* Process here */
	while (!training_finished) {
		switch (dp->link_train.lt_state) {
		case START:
			s5p_dp_link_start(dp);
			break;
		case CLOCK_RECOVERY:
			retval = s5p_dp_process_clock_recovery(dp);
			if (retval)
				dev_err(dp->dev, "LT CR failed\n");
			break;
		case EQUALIZER_TRAINING:
			retval = s5p_dp_process_equalizer_training(dp);
			if (retval)
				dev_err(dp->dev, "LT EQ failed\n");
			break;
		case FINISHED:
			training_finished = 1;
			break;
		case FAILED:
			return -EREMOTEIO;
		}
	}

	return retval;
}

static int s5p_dp_set_link_train(struct s5p_dp_device *dp,
				u32 count,
				u32 bwtype)
{
	int retval;

	s5p_dp_init_training(dp, count, bwtype);

	retval = s5p_dp_sw_link_training(dp);
	if (retval)
		dev_err(dp->dev, "DP LT failed!\n");

	return retval;
}

static int s5p_dp_config_video(struct s5p_dp_device *dp,
			struct video_info *video_info)
{
	int retval = 0;
	int timeout_loop = 0;
	int done_count = 0;

	s5p_dp_config_video_slave_mode(dp, video_info);

	s5p_dp_set_video_color_format(dp, video_info->color_depth,
			video_info->color_space,
			video_info->dynamic_range,
			video_info->ycbcr_coeff);

	if (s5p_dp_get_pll_lock_status(dp) == PLL_UNLOCKED) {
		dev_err(dp->dev, "PLL is not locked yet.\n");
		return -EINVAL;
	}

	for (;;) {
		timeout_loop++;
		if (s5p_dp_is_slave_video_stream_clock_on(dp) == 0)
			break;
		if (DP_TIMEOUT_LOOP_COUNT < timeout_loop) {
			dev_err(dp->dev, "Timeout of video streamclk ok\n");
			return -ETIMEDOUT;
		}

		usleep_range(1, 1);
	}

	/* Set to use the register calculated M/N video */
	s5p_dp_set_video_cr_mn(dp, CALCULATED_M, 0, 0);

	/* For video bist, Video timing must be generated by register */
	s5p_dp_set_video_timing_mode(dp, VIDEO_TIMING_FROM_CAPTURE);

	/* Disable video mute */
	s5p_dp_enable_video_mute(dp, 0);

	/* Configure video slave mode */
	s5p_dp_enable_video_master(dp, 0);

	/* Enable video */
	s5p_dp_start_video(dp);

	timeout_loop = 0;

	for (;;) {
		timeout_loop++;
		if (s5p_dp_is_video_stream_on(dp) == 0) {
			done_count++;
			if (done_count > 10)
				break;
		} else if (done_count) {
			done_count = 0;
		}
		if (DP_TIMEOUT_LOOP_COUNT < timeout_loop) {
			dev_err(dp->dev, "Timeout of video streamclk ok\n");
			return -ETIMEDOUT;
		}

		usleep_range(1000, 1000);
	}

	if (retval != 0)
		dev_err(dp->dev, "Video stream is not detected!\n");

	return retval;
}

static void s5p_dp_enable_scramble(struct s5p_dp_device *dp, bool enable)
{
	u8 data;

	if (enable) {
		s5p_dp_enable_scrambling(dp);

		s5p_dp_read_byte_from_dpcd(dp,
			DPCD_ADDR_TRAINING_PATTERN_SET,
			&data);
		s5p_dp_write_byte_to_dpcd(dp,
			DPCD_ADDR_TRAINING_PATTERN_SET,
			(u8)(data & ~DPCD_SCRAMBLING_DISABLED));
	} else {
		s5p_dp_disable_scrambling(dp);

		s5p_dp_read_byte_from_dpcd(dp,
			DPCD_ADDR_TRAINING_PATTERN_SET,
			&data);
		s5p_dp_write_byte_to_dpcd(dp,
			DPCD_ADDR_TRAINING_PATTERN_SET,
			(u8)(data | DPCD_SCRAMBLING_DISABLED));
	}
}

static irqreturn_t s5p_dp_irq_handler(int irq, void *arg)
{
	struct s5p_dp_device *dp = arg;

	dev_err(dp->dev, "s5p_dp_irq_handler\n");
	return IRQ_HANDLED;
}

static int s5p_dp_enable(struct s5p_dp_device *dp)
{
	int ret = 0;
	struct s5p_dp_platdata *pdata = dp->dev->platform_data;

	mutex_lock(&dp->lock);

	if (dp->enabled)
		goto out;

	dp->enabled = 1;

	clk_enable(dp->clock);
	pm_runtime_get_sync(dp->dev);

	if (pdata->phy_init)
		pdata->phy_init();

	s5p_dp_init_dp(dp);

	if (!soc_is_exynos5250()) {
		ret = s5p_dp_detect_hpd(dp);
		if (ret) {
			dev_err(dp->dev, "unable to detect hpd\n");
			goto out;
		}
	}

	s5p_dp_handle_edid(dp);

	ret = s5p_dp_set_link_train(dp, dp->video_info->lane_count,
				dp->video_info->link_rate);
	if (ret) {
		dev_err(dp->dev, "unable to do link train\n");
		goto out;
	}

	if (soc_is_exynos5250()) {
		s5p_dp_enable_scramble(dp, 1);
		s5p_dp_enable_rx_to_enhanced_mode(dp, 1);
		s5p_dp_enable_enhanced_mode(dp, 1);
	} else {
		s5p_dp_enable_scramble(dp, 0);
		s5p_dp_enable_rx_to_enhanced_mode(dp, 0);
		s5p_dp_enable_enhanced_mode(dp, 0);
	}

	s5p_dp_set_lane_count(dp, dp->video_info->lane_count);
	s5p_dp_set_link_bandwidth(dp, dp->video_info->link_rate);

	s5p_dp_init_video(dp);
	ret = s5p_dp_config_video(dp, dp->video_info);
	if (ret) {
		dev_err(dp->dev, "unable to config video\n");
		goto out;
	}

	if (pdata->backlight_on)
		pdata->backlight_on();

out:
	mutex_unlock(&dp->lock);
	return ret;
}

static void s5p_dp_disable(struct s5p_dp_device *dp)
{
	struct s5p_dp_platdata *pdata = dp->dev->platform_data;

	mutex_lock(&dp->lock);

	if (!dp->enabled)
		goto out;

	dp->enabled = 0;

	if (pdata->backlight_off)
		pdata->backlight_off();

	if (pdata && pdata->phy_exit)
		pdata->phy_exit();

	clk_disable(dp->clock);
	pm_runtime_put_sync(dp->dev);

out:
	mutex_unlock(&dp->lock);
}

static int s5p_dp_set_power(struct lcd_device *lcd, int power)
{
	struct s5p_dp_device *dp = lcd_get_data(lcd);

	if (power == FB_BLANK_UNBLANK)
		s5p_dp_enable(dp);
	else
		s5p_dp_disable(dp);

	return 0;
}

struct lcd_ops s5p_dp_lcd_ops = {
	.set_power = s5p_dp_set_power,
};

static int __devinit s5p_dp_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct s5p_dp_device *dp;
	struct s5p_dp_platdata *pdata;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}

	dp = kzalloc(sizeof(struct s5p_dp_device), GFP_KERNEL);
	if (!dp) {
		dev_err(&pdev->dev, "no memory for device data\n");
		return -ENOMEM;
	}

	mutex_init(&dp->lock);

	dp->dev = &pdev->dev;

	dp->clock = clk_get(&pdev->dev, "dp");
	if (IS_ERR(dp->clock)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		ret = PTR_ERR(dp->clock);
		goto err_dp;
	}

	pm_runtime_enable(dp->dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get registers\n");
		ret = -EINVAL;
		goto err_clock;
	}

	res = request_mem_region(res->start, resource_size(res),
				dev_name(&pdev->dev));
	if (!res) {
		dev_err(&pdev->dev, "failed to request registers region\n");
		ret = -EINVAL;
		goto err_clock;
	}

	dp->res = res;

	dp->reg_base = ioremap(res->start, resource_size(res));
	if (!dp->reg_base) {
		dev_err(&pdev->dev, "failed to ioremap\n");
		ret = -ENOMEM;
		goto err_req_region;
	}

	dp->irq = platform_get_irq(pdev, 0);
	if (!dp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		ret = -ENODEV;
		goto err_ioremap;
	}

	ret = request_irq(dp->irq, s5p_dp_irq_handler, 0,
			"s5p-dp", dp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto err_ioremap;
	}

	dp->video_info = pdata->video_info;

	platform_set_drvdata(pdev, dp);

	dp->lcd = lcd_device_register("s5p_dp", &pdev->dev, dp, &s5p_dp_lcd_ops);
	if (IS_ERR(dp->lcd)) {
		ret = PTR_ERR(dp->lcd);
		goto err_irq;
	}

	ret = s5p_dp_enable(dp);
	if (ret)
		goto err_fb;

	return 0;

err_fb:
	lcd_device_unregister(dp->lcd);
err_irq:
	free_irq(dp->irq, dp);
err_ioremap:
	iounmap(dp->reg_base);
err_req_region:
	release_mem_region(res->start, resource_size(res));
err_clock:
	clk_put(dp->clock);
err_dp:
	mutex_destroy(&dp->lock);
	kfree(dp);

	return ret;
}

static int __devexit s5p_dp_remove(struct platform_device *pdev)
{
	struct s5p_dp_device *dp = platform_get_drvdata(pdev);

	free_irq(dp->irq, dp);

	lcd_device_unregister(dp->lcd);

	s5p_dp_disable(dp);

	iounmap(dp->reg_base);
	clk_put(dp->clock);

	release_mem_region(dp->res->start, resource_size(dp->res));

	pm_runtime_disable(dp->dev);

	kfree(dp);

	return 0;
}

static struct platform_driver s5p_dp_driver = {
	.probe		= s5p_dp_probe,
	.remove		= __devexit_p(s5p_dp_remove),
	.driver		= {
		.name	= "s5p-dp",
		.owner	= THIS_MODULE,
	},
};

static int __init s5p_dp_init(void)
{
	return platform_driver_probe(&s5p_dp_driver, s5p_dp_probe);
}

static void __exit s5p_dp_exit(void)
{
	platform_driver_unregister(&s5p_dp_driver);
}

#ifdef CONFIG_FB_EXYNOS_FIMD_MC
late_initcall(s5p_dp_init);
#else
module_init(s5p_dp_init);
#endif
module_exit(s5p_dp_exit);

MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC DP Driver");
MODULE_LICENSE("GPL");
