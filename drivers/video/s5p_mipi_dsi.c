/* linux/drivers/video/s5p_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * InKi Dae, <inki.dae@samsung.com>
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
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>

#include <plat/fb.h>
#include <plat/regs-mipidsim.h>
#include <plat/dsim.h>
#include <plat/cpu.h>
#include <plat/clock.h>

#include <mach/map.h>
#ifdef CONFIG_FB_MIPI_DSIM_DBG
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#endif

#include "s5p_mipi_dsi_lowlevel.h"
#include "s5p_mipi_dsi.h"

static DEFINE_MUTEX(dsim_rd_wr_mutex);
static DECLARE_COMPLETION(dsim_wr_comp);
static DECLARE_COMPLETION(dsim_rd_comp);
#ifdef CONFIG_FB_MIPI_DSIM_DBG
extern void print_reg_pm_disp_5410(void);
static void s5p_mipi_dsi_d_phy_onoff(struct mipi_dsim_device *dsim, unsigned int enable);
#endif

#define MIPI_WR_TIMEOUT msecs_to_jiffies(35)
#define MIPI_RD_TIMEOUT msecs_to_jiffies(35)

#define DSIM_RX_FIFO_READ_DONE		0x30800002
#define DSIM_MAX_RX_FIFO			64

#define BOOT_LCD_INIT		1
#define DSIM_TRUE		1
#define DSIM_FALSE		0

static unsigned int dpll_table[15] = {
	100, 120, 170, 220, 270,
	320, 390, 450, 510, 560,
	640, 690, 770, 870, 950
};

static int s5p_mipi_clk_state(struct clk *clk)
{
	return clk->usage;
}

#ifdef CONFIG_FB_MIPI_DSIM_DBG
static int s5p_mipi_d_phy_state(struct mipi_dsim_device *dsim)
{
	unsigned int reg;
#ifdef CONFIG_S5P_DEV_MIPI_DSIM0
	reg = readl(S5P_MIPI_DPHY_CONTROL(0));
#else
	reg = readl(S5P_MIPI_DPHY_CONTROL(1));
#endif
	reg &= 0x5 << 0;

	if (reg != 0x5) {
		dev_info(dsim->dev, "MIPI D_DPY value = 0x%x\n", reg);
		return false;
	}
	return true;
}

static int s5p_mipi_dsi_debuging_info(struct mipi_dsim_device *dsim)
{

	if (++dsim->timeout_cnt < 5) return 0;

	dev_err(dsim->dev, "__DUMP MIPI-DSIM SFR (Start)\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dsim->reg_base, 0x0070, false);
	dev_err(dsim->dev, "__DUMP MIPI-DSIM SFR (End)\n");
	print_reg_pm_disp_5410();

	dsim->timeout_cnt = 0;
	return 0;
}
#endif

static int s5p_mipi_en_state(struct mipi_dsim_device *dsim)
{
	int ret = (!pm_runtime_suspended(dsim->dev) && s5p_mipi_clk_state(dsim->clock));

#ifdef CONFIG_FB_MIPI_DSIM_DBG
	ret = ret && s5p_mipi_d_phy_state(dsim);
#endif
	return ret;
}

static int s5p_mipi_force_enable(struct mipi_dsim_device *dsim)
{
	if (pm_runtime_suspended(dsim->dev))
		pm_runtime_get_sync(dsim->dev);
	if (!s5p_mipi_clk_state(dsim->clock))
		clk_enable(dsim->clock);
#ifdef CONFIG_FB_MIPI_DSIM_DBG
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);
#endif
	return 0;
}

static int s5p_mipi_dsi_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct mipi_dsim_device *dsim;

	dsim = container_of(self, struct mipi_dsim_device, fb_notif);

	switch (event) {
	case FB_EVENT_RESUME:
	case FB_BLANK_UNBLANK:
	default:
		break;
	}

	return 0;
}

static int s5p_mipi_dsi_register_fb(struct mipi_dsim_device *dsim)
{
	memset(&dsim->fb_notif, 0, sizeof(dsim->fb_notif));
	dsim->fb_notif.notifier_call = s5p_mipi_dsi_fb_notifier_callback;

	return fb_register_client(&dsim->fb_notif);
}

int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim,
	u8 cmd, const u8 *buf, u32 len)
{
	int i;
	unsigned char tempbuf[2] = {0, };
	int remind, temp;
	unsigned int payload;

	if (dsim->enabled == false) {
		dev_dbg(dsim->dev, "MIPI DSIM is disabled.\n");
		return -EINVAL;
	}

	if (dsim->state != DSIM_STATE_ULPS && dsim->state != DSIM_STATE_HSCLKEN) {
		dev_dbg(dsim->dev, "MIPI DSIM is not ready.\n");
		return -EINVAL;
	}

	if (!s5p_mipi_en_state(dsim)) {
		s5p_mipi_force_enable(dsim);
		dev_warn(dsim->dev, "%s: MIPI state check!!\n", __func__);
	} else
		dev_dbg(dsim->dev, "MIPI state is valied.\n");

	mutex_lock(&dsim_rd_wr_mutex);

	memset(tempbuf, 0, sizeof(tempbuf));

	if (len <= 2) {
		for (i = 0; i < len; i++)
			tempbuf[i] = buf[i];
	} else {
		tempbuf[0] = len & 0xff;
		tempbuf[1] = (len >> 8) & 0xff;
	}
	switch (cmd) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
		break;

	/* general command */
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
		break;

	/* packet types for video data */
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_END_OF_TRANSMISSION:
		break;

	/* short and response packet types for command */
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		s5p_mipi_dsi_clear_all_interrupt(dsim);
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
		/* process response func should be implemented. */
		break;

	/* long packet type and null packet */
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
		break;

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		INIT_COMPLETION(dsim_wr_comp);
		s5p_mipi_dsi_clear_interrupt(dsim, INTSRC_SFR_FIFO_EMPTY);
		temp = 0;
		remind = len;
		for (i = 0; i < (len/4); i++) {
			temp = i * 4;
			payload = buf[temp] |
						buf[temp + 1] << 8 |
						buf[temp + 2] << 16 |
						buf[temp + 3] << 24;
			remind -= 4;
			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		}
		if (remind) {
			payload = 0;
			temp = len-remind;
			for (i = 0; i < remind; i++)
				payload |= buf[temp + i] << (i * 8);

			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		}
		/* put data into header fifo */
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);

		if (!wait_for_completion_interruptible_timeout(&dsim_wr_comp,
			MIPI_WR_TIMEOUT)) {
				dev_err(dsim->dev, "MIPI DSIM write Timeout!\n");
				#ifdef CONFIG_FB_MIPI_DSIM_DBG
				s5p_mipi_dsi_debuging_info(dsim);
				#endif
				mutex_unlock(&dsim_rd_wr_mutex);
				return -ETIMEDOUT;
		}
		#ifdef CONFIG_FB_MIPI_DSIM_DBG
		dsim->timeout_cnt = 0;
		#endif
		break;
	}

	/* packet typo for video data */
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		break;
	default:
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n", cmd);

		mutex_unlock(&dsim_rd_wr_mutex);
		return -EINVAL;
	}

	mutex_unlock(&dsim_rd_wr_mutex);
	return len;
}

static void s5p_mipi_dsi_rx_err_handler(struct mipi_dsim_device *dsim,
	u32 rx_fifo)
{
	/* Parse error report bit*/
	if (rx_fifo & (1 << 8))
		dev_err(dsim->dev, "SoT error!\n");
	if (rx_fifo & (1 << 9))
		dev_err(dsim->dev, "SoT sync error!\n");
	if (rx_fifo & (1 << 10))
		dev_err(dsim->dev, "EoT error!\n");
	if (rx_fifo & (1 << 11))
		dev_err(dsim->dev, "Escape mode entry command error!\n");
	if (rx_fifo & (1 << 12))
		dev_err(dsim->dev, "Low-power transmit sync error!\n");
	if (rx_fifo & (1 << 13))
		dev_err(dsim->dev, "HS receive timeout error!\n");
	if (rx_fifo & (1 << 14))
		dev_err(dsim->dev, "False control error!\n");
	/* Bit 15 is reserved*/
	if (rx_fifo & (1 << 16))
		dev_err(dsim->dev, "ECC error, single-bit(detected and corrected)!\n");
	if (rx_fifo & (1 << 17))
		dev_err(dsim->dev, "ECC error, multi-bit(detected, not corrected)!\n");
	if (rx_fifo & (1 << 18))
		dev_err(dsim->dev, "Checksum error(long packet only)!\n");
	if (rx_fifo & (1 << 19))
		dev_err(dsim->dev, "DSI data type not recognized!\n");
	if (rx_fifo & (1 << 20))
		dev_err(dsim->dev, "DSI VC ID invalid!\n");
	if (rx_fifo & (1 << 21))
		dev_err(dsim->dev, "Invalid transmission length!\n");
	/* Bit 22 is reserved */
	if (rx_fifo & (1 << 23))
		dev_err(dsim->dev, "DSI protocol violation!\n");
}



int s5p_mipi_dsi_rd_data(struct mipi_dsim_device *dsim, u8 data_id,
	 u8 addr, u8 count, u8 *buf, u8 rxfifo_done)
{
	u32 rx_fifo, rx_size = 0;
	int i, j, retry;
	unsigned int temp;
	unsigned char res;

	INIT_COMPLETION(dsim_rd_comp);
	/* TODO: need to check power, clock state here. */
	/* It's checked at s5p_mipi_dsi_wr_data currently. */

	/* Set the maximum packet size returned*/
	s5p_mipi_dsi_wr_data(dsim,
	    MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, &count, 1);

	/* Read request */
	s5p_mipi_dsi_wr_data(dsim, data_id, &addr, 1);
	if (!wait_for_completion_interruptible_timeout(&dsim_rd_comp,
		    MIPI_RD_TIMEOUT)) {
		dev_err(dsim->dev, "MIPI DSIM read Timeout!\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&dsim_rd_wr_mutex);

	retry = DSIM_MAX_RX_FIFO;
check_rx_header:
	if (!retry--) {
		dev_err(dsim->dev, "dsim read fail.. can't not recovery rx data\n");
		goto read_fail;
	}

	temp = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
	res = (unsigned char)temp & 0x000000ff;
	/* Parse the RX packet data types */
	switch (res) {

	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		s5p_mipi_dsi_rx_err_handler(dsim, temp);
		goto read_fail;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dev_dbg(dsim->dev, "EoTp was received from LCD module.\n");
		break;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		dev_dbg(dsim->dev, "Short Packet was received from LCD module.\n");
		for (i = 0; i < count; i++)
			buf[i] = (temp >> (8 + i * 8)) & 0xff;
		rx_size = count;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		rx_size = (temp >> 8) & 0x0000ffff;
		dev_info(dsim->dev, "rx fifo : %8x, response : %x, rx_size : %d\n",
				temp, res, rx_size);
		if (count != rx_size) {
			dev_err(dsim->dev, "wrong rx size ..\n");
			goto check_rx_header;
		}
		dev_dbg(dsim->dev, "Long Packet was received from LCD module.\n");
		/* Read data from RX packet payload */
		for (i = 0; i < rx_size >> 2; i++) {
			rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			for (j = 0; j < 4; j++)
				buf[(i*4)+j] = (u8)(rx_fifo >> (j * 8)) & 0xff;
		}
		if (rx_size % 4) {
			rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			for (j = 0; j < rx_size%4; j++)
				buf[(4*i)+j] = (u8)(rx_fifo >> (j*8)) & 0xff;
		}
		break;
	default:
		dev_err(dsim->dev, "Invalid packet header\n");
		goto check_rx_header;
		break;
	}

	/* for Magna DDI Temporary patch */
	if (!rxfifo_done) {
		mutex_unlock(&dsim_rd_wr_mutex);
		return rx_size;
	}

	retry = DSIM_MAX_RX_FIFO;
check_rx_done:
	if (!retry--) {
		dev_err(dsim->dev, "can't not received rx done ..\n");
		goto read_fail;
	}
	temp = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
	if (temp != DSIM_RX_FIFO_READ_DONE) {
		dev_warn(dsim->dev, "[DSIM:WARN]:%s Can't found RX FIFO READ DONE FLAG : %x\n", __func__, temp);
		goto check_rx_done;
	}

	mutex_unlock(&dsim_rd_wr_mutex);
	return rx_size;

read_fail:
	dev_err(dsim->dev, "############ Dump DSIM RX FIFO ############\n");
	for (i = 0; i < DSIM_MAX_RX_FIFO; i++) {
		temp = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
		dev_err(dsim->dev, "Rx FIFO[%d] : %8x\n", i, temp);
	}
	mutex_unlock(&dsim_rd_wr_mutex);
	return 0;
}

int s5p_mipi_dsi_pll_on(struct mipi_dsim_device *dsim, unsigned int enable)
{
	int sw_timeout;

	if (enable) {
		sw_timeout = 100;

		s5p_mipi_dsi_clear_interrupt(dsim, INTSRC_PLL_STABLE);
		s5p_mipi_dsi_enable_pll(dsim, 1);
		while (1) {
			usleep_range(1000, 1000);
			sw_timeout--;
			if (s5p_mipi_dsi_is_pll_stable(dsim))
				return 0;
			if (sw_timeout == 0) {
				pr_err("mipi pll on fail!!!\n");
				return -EINVAL;
			}
		}
	} else
		s5p_mipi_dsi_enable_pll(dsim, 0);

	return 0;
}

unsigned long s5p_mipi_dsi_change_pll(struct mipi_dsim_device *dsim,
	unsigned int pre_divider, unsigned int main_divider,
	unsigned int scaler)
{
	unsigned long dfin_pll, dfvco, dpll_out;
	unsigned int i, freq_band = 0xf;

	dfin_pll = (FIN_HZ / pre_divider);

	if (soc_is_exynos5250()) {
		if (dfin_pll < DFIN_PLL_MIN_HZ || dfin_pll > DFIN_PLL_MAX_HZ) {
			dev_warn(dsim->dev, "fin_pll range should be 6MHz ~ 12MHz\n");
			s5p_mipi_dsi_enable_afc(dsim, 0, 0);
		} else {
			if (dfin_pll < 7 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x1);
			else if (dfin_pll < 8 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x0);
			else if (dfin_pll < 9 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x3);
			else if (dfin_pll < 10 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x2);
			else if (dfin_pll < 11 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x5);
			else
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x4);
		}
	}
	dfvco = dfin_pll * main_divider;
	dev_dbg(dsim->dev, "dfvco = %lu, dfin_pll = %lu, main_divider = %d\n",
				dfvco, dfin_pll, main_divider);

	if (soc_is_exynos5250()) {
		if (dfvco < DFVCO_MIN_HZ || dfvco > DFVCO_MAX_HZ)
			dev_warn(dsim->dev, "fvco range should be 500MHz ~ 1000MHz\n");
	}

	dpll_out = dfvco / (1 << scaler);
	dev_dbg(dsim->dev, "dpll_out = %lu, dfvco = %lu, scaler = %d\n",
		dpll_out, dfvco, scaler);

	if (soc_is_exynos5250()) {
		for (i = 0; i < ARRAY_SIZE(dpll_table); i++) {
			if (dpll_out < dpll_table[i] * MHZ) {
				freq_band = i;
				break;
			}
		}
	}

	dev_dbg(dsim->dev, "freq_band = %d\n", freq_band);

	s5p_mipi_dsi_pll_freq(dsim, pre_divider, main_divider, scaler);

	s5p_mipi_dsi_hs_zero_ctrl(dsim, 0);
	s5p_mipi_dsi_prep_ctrl(dsim, 0);

	if (soc_is_exynos5250()) {
		/* Freq Band */
		s5p_mipi_dsi_pll_freq_band(dsim, freq_band);
	}
	/* Stable time */
	s5p_mipi_dsi_pll_stable_time(dsim, dsim->dsim_config->pll_stable_time);

	/* Enable PLL */
	dev_dbg(dsim->dev, "FOUT of mipi dphy pll is %luMHz\n",
		(dpll_out / MHZ));

	return dpll_out;
}

static int s5p_mipi_dsi_set_clock(struct mipi_dsim_device *dsim,
	unsigned int byte_clk_sel, unsigned int enable)
{
	unsigned int esc_div;
	unsigned long esc_clk_error_rate;

	if (enable) {
		dsim->e_clk_src = byte_clk_sel;

		/* Escape mode clock and byte clock source */
		s5p_mipi_dsi_set_byte_clock_src(dsim, byte_clk_sel);

		/* DPHY, DSIM Link : D-PHY clock out */
		if (byte_clk_sel == DSIM_PLL_OUT_DIV8) {
			dsim->hs_clk = s5p_mipi_dsi_change_pll(dsim,
				dsim->dsim_config->p, dsim->dsim_config->m,
				dsim->dsim_config->s);
			if (dsim->hs_clk == 0) {
				dev_err(dsim->dev,
					"failed to get hs clock.\n");
				return -EINVAL;
			}
			if (!soc_is_exynos5250()) {
				s5p_mipi_dsi_set_b_dphyctrl(dsim, 0x0AF);
				s5p_mipi_dsi_set_timing_register0(dsim, 0x06, 0x0b);
				s5p_mipi_dsi_set_timing_register1(dsim, 0x07, 0x27,
									0x0d, 0x08);
				s5p_mipi_dsi_set_timing_register2(dsim, 0x09, 0x0d,
									0x0b);
			}
			dsim->byte_clk = dsim->hs_clk / 8;
			s5p_mipi_dsi_enable_pll_bypass(dsim, 0);
			s5p_mipi_dsi_pll_on(dsim, 1);
		/* DPHY : D-PHY clock out, DSIM link : external clock out */
		} else if (byte_clk_sel == DSIM_EXT_CLK_DIV8)
			dev_warn(dsim->dev,
				"this project is not support external clock source for MIPI DSIM\n");
		else if (byte_clk_sel == DSIM_EXT_CLK_BYPASS)
			dev_warn(dsim->dev,
				"this project is not support external clock source for MIPI DSIM\n");

		/* escape clock divider */
		esc_div = dsim->byte_clk / (dsim->dsim_config->esc_clk);
		dev_dbg(dsim->dev,
			"esc_div = %d, byte_clk = %lu, esc_clk = %lu\n",
			esc_div, dsim->byte_clk, dsim->dsim_config->esc_clk);

		if (soc_is_exynos5250()) {
			if ((dsim->byte_clk / esc_div) >= (20 * MHZ) ||
					(dsim->byte_clk / esc_div) >
						dsim->dsim_config->esc_clk)
				esc_div += 1;
		} else {
			if ((dsim->byte_clk / esc_div) >= (10 * MHZ) ||
				(dsim->byte_clk / esc_div) >
					dsim->dsim_config->esc_clk)
				esc_div += 1;
		}
		dsim->escape_clk = dsim->byte_clk / esc_div;
		dev_info(dsim->dev,
			"escape_clk = %lu, byte_clk = %lu, esc_div = %d\n",
			dsim->escape_clk, dsim->byte_clk, esc_div);

		/* enable byte clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_ON);

		/* enable escape clock */
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 1, esc_div);
		/* escape clock on lane */
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 1);

		dev_dbg(dsim->dev, "byte clock is %luMHz\n",
			(dsim->byte_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock that user's need is %lu\n",
			(dsim->dsim_config->esc_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock divider is %x\n", esc_div);
		dev_dbg(dsim->dev, "escape clock is %luMHz\n",
			((dsim->byte_clk / esc_div) / MHZ));

		if ((dsim->byte_clk / esc_div) > dsim->escape_clk) {
			esc_clk_error_rate = dsim->escape_clk /
				(dsim->byte_clk / esc_div);
			dev_warn(dsim->dev, "error rate is %lu over.\n",
				(esc_clk_error_rate / 100));
		} else if ((dsim->byte_clk / esc_div) < (dsim->escape_clk)) {
			esc_clk_error_rate = (dsim->byte_clk / esc_div) /
				dsim->escape_clk;
			dev_warn(dsim->dev, "error rate is %lu under.\n",
				(esc_clk_error_rate / 100));
		}
	} else {
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 0, 0);

		/* disable escape clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_OFF);

		if (byte_clk_sel == DSIM_PLL_OUT_DIV8)
			s5p_mipi_dsi_pll_on(dsim, 0);
	}

	return 0;
}

static void s5p_mipi_dsi_d_phy_onoff(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	if (dsim->pd->init_d_phy)
		dsim->pd->init_d_phy(dsim, enable);
}

extern unsigned int system_rev;

static int s5p_mipi_dsi_init_dsim(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	switch (dsim->dsim_config->e_no_data_lane) {
	case DSIM_DATA_LANE_1:
		dsim->data_lane = DSIM_LANE_DATA0;
		break;
	case DSIM_DATA_LANE_2:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1;
		break;
	case DSIM_DATA_LANE_3:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2;
		break;
	case DSIM_DATA_LANE_4:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2 | DSIM_LANE_DATA3;
		break;
	default:
		dev_info(dsim->dev, "data lane is invalid.\n");
		return -EINVAL;
	};

	s5p_mipi_dsi_sw_reset(dsim);
	s5p_mipi_dsi_dp_dn_swap(dsim, 0);

	return 0;
}

#if 0
static int s5p_mipi_dsi_enable_frame_done_int(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	/* enable only frame done interrupt */
	s5p_mipi_dsi_set_interrupt_mask(dsim, INTMSK_FRAME_DONE, enable);

	return 0;
}
#endif

static int s5p_mipi_dsi_set_display_mode(struct mipi_dsim_device *dsim,
	struct mipi_dsim_config *dsim_config)
{
	struct fb_videomode *lcd_video = NULL;
	struct s3c_fb_pd_win *pd;
	unsigned int width = 0, height = 0;
	pd = (struct s3c_fb_pd_win *)dsim->dsim_config->lcd_panel_info;
	lcd_video = (struct fb_videomode *)&pd->win_mode;

	width = dsim->pd->dsim_lcd_config->lcd_size.width;
	height = dsim->pd->dsim_lcd_config->lcd_size.height;

	/* in case of VIDEO MODE (RGB INTERFACE) */
	if (dsim->dsim_config->e_interface == (u32) DSIM_VIDEO) {
		s5p_mipi_dsi_set_main_disp_vporch(dsim,
			dsim->pd->dsim_lcd_config->rgb_timing.cmd_allow,
			dsim->pd->dsim_lcd_config->rgb_timing.stable_vfp,
			dsim->pd->dsim_lcd_config->rgb_timing.upper_margin);
		s5p_mipi_dsi_set_main_disp_hporch(dsim,
			dsim->pd->dsim_lcd_config->rgb_timing.right_margin,
			dsim->pd->dsim_lcd_config->rgb_timing.left_margin);
		s5p_mipi_dsi_set_main_disp_sync_area(dsim,
			dsim->pd->dsim_lcd_config->rgb_timing.vsync_len,
			dsim->pd->dsim_lcd_config->rgb_timing.hsync_len);
	}

	s5p_mipi_dsi_set_main_disp_resol(dsim, height, width);
	s5p_mipi_dsi_display_config(dsim);
	return 0;
}

static int s5p_mipi_dsi_init_link(struct mipi_dsim_device *dsim)
{
	unsigned int time_out = 100;
	unsigned int id;
	id = dsim->id;
	switch (dsim->state) {
	case DSIM_STATE_INIT:
		s5p_mipi_dsi_sw_reset(dsim);
		s5p_mipi_dsi_init_fifo_pointer(dsim, 0x1f);

		/* dsi configuration */
		s5p_mipi_dsi_init_config(dsim);
		s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 1);
		s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 1);

		/* set clock configuration */
		s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 1);

		/* check clock and data lane state are stop state */
		while (!(s5p_mipi_dsi_is_lane_state(dsim))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"DSI Master is not stop state.\n");
				dev_err(dsim->dev,
					"Check initialization process\n");

				return -EINVAL;
			}
		}

		if (time_out != 0) {
			dev_info(dsim->dev,
				"DSI Master driver has been completed.\n");
			dev_info(dsim->dev, "DSI Master state is stop state\n");
		}

		dsim->state = DSIM_STATE_STOP;

		/* BTA sequence counters */
		s5p_mipi_dsi_set_stop_state_counter(dsim,
			dsim->dsim_config->stop_holding_cnt);
		s5p_mipi_dsi_set_bta_timeout(dsim,
			dsim->dsim_config->bta_timeout);
		s5p_mipi_dsi_set_lpdr_timeout(dsim,
			dsim->dsim_config->rx_timeout);

		return 0;
	default:
		dev_info(dsim->dev, "DSI Master is already init.\n");
		return 0;
	}

	return 0;
}

static int s5p_mipi_dsi_set_hs_enable(struct mipi_dsim_device *dsim)
{
	if (dsim->state == DSIM_STATE_STOP) {
		if (dsim->e_clk_src != DSIM_EXT_CLK_BYPASS) {
			dsim->state = DSIM_STATE_HSCLKEN;

			 /* set LCDC and CPU transfer mode to HS. */
			s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
			s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);

			s5p_mipi_dsi_enable_hs_clock(dsim, 1);

			return 0;
		} else
			dev_warn(dsim->dev,
				"clock source is external bypass.\n");
	} else
		dev_warn(dsim->dev, "DSIM is not stop state.\n");

	return 0;
}

static int s5p_mipi_dsi_set_data_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int mode)
{
	if (mode) {
		if (dsim->state != DSIM_STATE_HSCLKEN) {
			dev_err(dsim->dev, "HS Clock lane is not enabled.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
	} else {
		if (dsim->state == DSIM_STATE_INIT || dsim->state ==
			DSIM_STATE_ULPS) {
			dev_err(dsim->dev,
				"DSI Master is not STOP or HSDT state.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);
	}
	return 0;
}

#if 0
static int s5p_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim)
{
	return _s5p_mipi_dsi_get_frame_done_status(dsim);
}

static int s5p_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim)
{
	_s5p_mipi_dsi_clear_frame_done(dsim);

	return 0;
}
#endif

static irqreturn_t s5p_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	unsigned int int_msk = 0;
	unsigned int int_src = 0;
	struct mipi_dsim_device *dsim = dev_id;

	spin_lock(&dsim->slock);
	int_src = readl(dsim->reg_base + S5P_DSIM_INTSRC);
	int_msk = readl(dsim->reg_base + S5P_DSIM_INTMSK);

	int_src = ~(int_msk) & int_src;

	/* Test bit */
	if (int_src & SFR_PL_FIFO_EMPTY)
		complete(&dsim_wr_comp);
	if (int_src & RX_DAT_DONE)
		complete(&dsim_rd_comp);
	if (int_src & ERR_RX_ECC)
		dev_err(dsim->dev, "RX ECC Multibit error was detected!\n");
	s5p_mipi_dsi_clear_interrupt(dsim, int_src);
	spin_unlock(&dsim->slock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int s5p_mipi_dsi_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	dev_info(dsim->dev, "+%s\n", __func__);

	dsim->enabled = false;
	dsim->dsim_lcd_drv->suspend(dsim);
	dsim->state = DSIM_STATE_SUSPEND;
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);
	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 0);
	pm_runtime_put_sync(dev);
	clk_disable(dsim->clock);

	dev_info(dsim->dev, "-%s\n", __func__);
	return 0;
}

static int s5p_mipi_dsi_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	dev_info(dsim->dev, "+%s\n", __func__);

	pm_runtime_get_sync(&pdev->dev);
	clk_enable(dsim->clock);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 1);
	if (dsim->dsim_lcd_drv->resume)
		dsim->dsim_lcd_drv->resume(dsim);
	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	dsim->enabled = true;
	dsim->dsim_lcd_drv->displayon(dsim);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

static int s5p_mipi_dsi_runtime_suspend(struct device *dev)
{
	return 0;
}

static int s5p_mipi_dsi_runtime_resume(struct device *dev)
{
	return 0;
}
#else
#define s5p_mipi_dsi_suspend NULL
#define s5p_mipi_dsi_resume NULL
#define s5p_mipi_dsi_runtime_suspend NULL
#define s5p_mipi_dsi_runtime_resume NULL
#endif

static int s5p_mipi_dsi_enable(struct mipi_dsim_device *dsim)
{
	unsigned int int_msk = 0;
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct lcd_platform_data *lcd_pd =
		(struct lcd_platform_data *)dsim->pd->dsim_lcd_config->mipi_ddi_pd;

	dev_info(dsim->dev, "+%s\n", __func__);

	pm_runtime_get_sync(&pdev->dev);
	clk_enable(dsim->clock);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 1);

	if (lcd_pd && lcd_pd->power_on)
		lcd_pd->power_on(NULL, 1);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	if (lcd_pd && lcd_pd->reset)
		lcd_pd->reset(NULL);

	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	/* clear interrupt */
	int_msk = 0xffffffff;
	s5p_mipi_dsi_clear_interrupt(dsim, int_msk);

	/* enable interrupts */
	int_msk = SFR_PL_FIFO_EMPTY | RX_DAT_DONE;
	s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 0);

	dsim->enabled = true;

	usleep_range(18000, 18000);

	dsim->dsim_lcd_drv->displayon(dsim);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

int s5p_mipi_dsi_enable_by_fimd(struct device *dsim_device)
{
	struct platform_device *pdev = to_platform_device(dsim_device);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->enabled == true)
		return 0;

	s5p_mipi_dsi_enable(dsim);
	return 0;
}

static int s5p_mipi_dsi_disable(struct mipi_dsim_device *dsim)
{
	unsigned int int_msk = 0;
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct lcd_platform_data *lcd_pd =
		(struct lcd_platform_data *)dsim->pd->dsim_lcd_config->mipi_ddi_pd;

	dev_info(dsim->dev, "+%s\n", __func__);

	dsim->dsim_lcd_drv->suspend(dsim);
	if (lcd_pd && lcd_pd->power_on)
		lcd_pd->power_on(NULL, 0);

	/* disable interrupts */
	int_msk = SFR_PL_FIFO_EMPTY | RX_DAT_DONE;
	s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 1);

	dsim->enabled = false;
	dsim->state = DSIM_STATE_SUSPEND;
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	pm_runtime_put_sync(&pdev->dev);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 0);

	clk_disable(dsim->clock);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

int s5p_mipi_dsi_disable_by_fimd(struct device *dsim_device)
{
	struct platform_device *pdev = to_platform_device(dsim_device);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->enabled == false)
		return 0;

	s5p_mipi_dsi_disable(dsim);
	return 0;
}

static int s5p_mipi_dsi_probe(struct platform_device *pdev)
{
	int i;
	unsigned int rx_fifo;
	struct resource *res;
	struct mipi_dsim_device *dsim = NULL;
	struct mipi_dsim_config *dsim_config;
	struct s5p_platform_mipi_dsim *dsim_pd;
	struct lcd_platform_data *lcd_pd = NULL;
	int ret = -1;
	unsigned int int_msk;

	if (!dsim)
		dsim = kzalloc(sizeof(struct mipi_dsim_device),
			GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -ENOMEM;
	}

	dsim->pd = to_dsim_plat(&pdev->dev);
	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;

	spin_lock_init(&dsim->slock);

	ret = s5p_mipi_dsi_register_fb(dsim);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fb notifier chain\n");
		kfree(dsim);
		return -EFAULT;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	/* get s5p_platform_mipi_dsim. */
	dsim_pd = (struct s5p_platform_mipi_dsim *)dsim->pd;
	/* get mipi_dsim_config. */
	dsim_config = dsim_pd->dsim_config;
	dsim->dsim_config = dsim_config;

	if (dsim->dsim_config == NULL) {
		dev_err(&pdev->dev, "dsim_config is NULL.\n");
		ret = -EINVAL;
		goto err_dsim_config;
	}

	dsim->clock = clk_get(&pdev->dev, dsim->pd->clk_name);
	if (IS_ERR(dsim->clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source\n");
		goto err_clock_get;
	}
	clk_enable(dsim->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		ret = -EINVAL;
		goto err_platform_get;
	}
	res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	dsim->res = res;
	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/* clear interrupt */
	int_msk = 0xffffffff;
	s5p_mipi_dsi_clear_interrupt(dsim, int_msk);

	/* enable interrupts */
	int_msk = SFR_PL_FIFO_EMPTY | RX_DAT_DONE;
	s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 0);
	/*
	 * it uses frame done interrupt handler
	 * only in case of MIPI Video mode.
	 */
	if (dsim->pd->dsim_config->e_interface == DSIM_VIDEO) {
		dsim->irq = platform_get_irq(pdev, 0);
		if (request_irq(dsim->irq, s5p_mipi_dsi_interrupt_handler,
				IRQF_DISABLED, "mipi-dsi", dsim)) {
			dev_err(&pdev->dev, "request_irq failed.\n");
			ret = -EINVAL;
			goto err_irq;
		}
	}

	dsim->dsim_lcd_drv = dsim->dsim_config->dsim_ddi_pd;

	platform_set_drvdata(pdev, dsim);

	if (dsim->pd->dsim_lcd_config->mipi_ddi_pd)
		lcd_pd = (struct lcd_platform_data *)dsim->pd->dsim_lcd_config->mipi_ddi_pd;
	else
		dev_err(&pdev->dev, "failed to register lcd platform data\n");

	/* Clear RX FIFO to prevent obnormal read opeation */
	for (i = 0; i < DSIM_MAX_RX_FIFO; i++)
		rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 1);

	if (lcd_pd && lcd_pd->power_on)
		lcd_pd->power_on(NULL, 1);

#if defined(BOOT_LCD_INIT)
	dsim->state = DSIM_STATE_HSCLKEN;
	dsim->enabled = true;
	ret = dsim->dsim_lcd_drv->probe(dsim);
	if (ret < 0)
		goto err_lcd_probe;
#else
	if (lcd_pd && lcd_pd->reset)
		lcd_pd->reset(NULL);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	dsim->enabled = true;
	dsim->dsim_lcd_drv->probe(dsim);
	dsim->dsim_lcd_drv->displayon(dsim);
#endif
	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim_config->e_interface == DSIM_COMMAND) ? "CPU" : "RGB");

	mutex_init(&dsim_rd_wr_mutex);
	return 0;

err_lcd_probe:
	if (dsim->pd->dsim_config->e_interface == DSIM_VIDEO)
		free_irq(dsim->irq, dsim);
err_irq:
	release_resource(dsim->res);
	kfree(dsim->res);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
err_platform_get:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);

err_dsim_config:
err_clock_get:
	kfree(dsim);
	pm_runtime_put_sync(&pdev->dev);
	return ret;

}

static int __devexit s5p_mipi_dsi_remove(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->dsim_config->e_interface == DSIM_VIDEO)
		free_irq(dsim->irq, dsim);

	iounmap(dsim->reg_base);

	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	release_resource(dsim->res);
	kfree(dsim->res);

	kfree(dsim);

	return 0;
}

static void s5p_mipi_dsi_shutdown(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->enabled != false) {
		dsim->dsim_lcd_drv->suspend(dsim);
		dsim->enabled = false;
		dsim->state = DSIM_STATE_SUSPEND;
		s5p_mipi_dsi_d_phy_onoff(dsim, 0);
		if (dsim->pd->mipi_power)
			dsim->pd->mipi_power(dsim, 0);
		pm_runtime_put_sync(&pdev->dev);
		clk_disable(dsim->clock);
	}
}

static const struct dev_pm_ops mipi_dsi_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = s5p_mipi_dsi_suspend,
	.resume = s5p_mipi_dsi_resume,
#endif
	.runtime_suspend	= s5p_mipi_dsi_runtime_suspend,
	.runtime_resume		= s5p_mipi_dsi_runtime_resume,
};

static struct platform_driver s5p_mipi_dsi_driver = {
	.probe = s5p_mipi_dsi_probe,
	.remove = __devexit_p(s5p_mipi_dsi_remove),
	.shutdown = s5p_mipi_dsi_shutdown,
	.driver = {
		   .name = "s5p-mipi-dsim",
		   .owner = THIS_MODULE,
		   .pm = NULL,
	},
};

static int s5p_mipi_dsi_register(void)
{
	platform_driver_register(&s5p_mipi_dsi_driver);

	return 0;
}

static void s5p_mipi_dsi_unregister(void)
{
	platform_driver_unregister(&s5p_mipi_dsi_driver);
}
#if defined(CONFIG_FB_EXYNOS_FIMD_MC) || defined(CONFIG_FB_EXYNOS_FIMD_MC_WB)
device_initcall_sync(s5p_mipi_dsi_register);
#else
module_init(s5p_mipi_dsi_register);
#endif
module_exit(s5p_mipi_dsi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSI driver");
MODULE_LICENSE("GPL");
