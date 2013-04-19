/* linux/drivers/video/backlight/er63311_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>

#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>

#include "eR63311_gamma.h"

struct backlight_device *bd;
struct mipi_dsim_device *dsim_base;

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 120

u8 rx_id[3] = {0x00, 0x00, 0x00};

static void er63311_fhd_read_id(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
			(unsigned int)SEQ_MCAP_OFF, ARRAY_SIZE(SEQ_MCAP_OFF));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			(unsigned int)MIPI_DCS_NOP, 0);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			(unsigned int)MIPI_DCS_NOP, 0);

	if (s5p_mipi_dsi_rd_data(dsim, MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
		0xC1, 3, rx_id, 1) == -1)
		printk(KERN_ERR "Display module: Read ID failed!\n");
	else
		printk(KERN_INFO "Display module ID1: %#X\n", rx_id[0]);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
			(unsigned int)SEQ_MCAP_ON, ARRAY_SIZE(SEQ_MCAP_ON));
}

static int er63311_fhd_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int update_brightness(int brightness)
{
	unsigned	int backlightlevel = 0;
#if defined(CONFIG_LCD_MIPI_ER63311_LDI)
	backlightlevel = brightness;

	if (backlightlevel > 255)
		backlightlevel = 255;

	SEQ_BRIGHTNESS_SET[1] = backlightlevel;
#else
	backlightlevel = 16 * brightness;

	if (backlightlevel > 4095)
		backlightlevel = 4095;

	SEQ_BRIGHTNESS_SET[1] = backlightlevel >> 8;
	SEQ_BRIGHTNESS_SET[2] = backlightlevel & 0x00ff;
#endif
	s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_BRIGHTNESS_SET,
			ARRAY_SIZE(SEQ_BRIGHTNESS_SET));

	return 1;
}

static int er63311_fhd_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 1;
}

static const struct backlight_ops er63311_fhd_backlight_ops = {
	.get_brightness = er63311_fhd_get_brightness,
	.update_status = er63311_fhd_set_brightness,
};

static int er63311_fhd_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &er63311_fhd_backlight_ops, NULL);

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_BRIGHTNESS_SET,
				ARRAY_SIZE(SEQ_BRIGHTNESS_SET));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_CABC_SET,
				ARRAY_SIZE(SEQ_CABC_SET));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_BACKLIGHT_CTRL,
				ARRAY_SIZE(SEQ_BACKLIGHT_CTRL));

	if (rx_id[0] == 0x84)
		s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_SET_ADDR_MODE,
				ARRAY_SIZE(SEQ_SET_ADDR_MODE));

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_DISP_ON, ARRAY_SIZE(SEQ_DISP_ON));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(200);

}

static void init_backlight(void)
{
	gpio_request(EXYNOS5410_GPJ0(4), "LED_DRIVER_EN");
	gpio_direction_output(EXYNOS5410_GPJ0(4), 1);
	usleep_range(10, 20);
	gpio_direction_output(EXYNOS5410_GPJ0(4), 0);
	usleep_range(10, 20);
	gpio_direction_output(EXYNOS5410_GPJ0(4), 1);
	usleep_range(10, 20);
	gpio_direction_output(EXYNOS5410_GPJ0(4), 0);
	usleep_range(10, 20);
	gpio_direction_output(EXYNOS5410_GPJ0(4), 1);

	gpio_free(EXYNOS5410_GPJ0(4));
}

static int er63311_fhd_displayon(struct mipi_dsim_device *dsim)
{
	er63311_fhd_read_id(dsim);
	init_lcd(dsim);
	init_backlight();
	return 1;
}

static int er63311_fhd_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int er63311_fhd_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver er63311_mipi_lcd_driver = {
	.probe		= er63311_fhd_probe,
	.displayon	= er63311_fhd_displayon,
	.suspend	= er63311_fhd_suspend,
	.resume		= er63311_fhd_resume,
};
