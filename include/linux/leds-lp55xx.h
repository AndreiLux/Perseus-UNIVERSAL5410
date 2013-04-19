/*
 * LP55XX Platform Data Header
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Derived from leds-lp5521.h, leds-lp5523.h
 */

#ifndef __LINUX_LP55XX_H
#define __LINUX_LP55XX_H

#define LP55XX_CLOCK_AUTO	0
#define LP55XX_CLOCK_INT	1
#define LP55XX_CLOCK_EXT	2

/* Bits in LP5521 CONFIG register */
#define LP5521_PWM_HF			0x40	/* PWM: 0 = 256Hz, 1 = 558Hz */
#define LP5521_PWRSAVE_EN		0x20	/* 1 = Power save mode */
#define LP5521_CP_MODE_OFF		0	/* Charge pump (CP) off */
#define LP5521_CP_MODE_BYPASS		8	/* CP forced to bypass mode */
#define LP5521_CP_MODE_1X5		0x10	/* CP forced to 1.5x mode */
#define LP5521_CP_MODE_AUTO		0x18	/* Automatic mode selection */
#define LP5521_R_TO_BATT		4	/* R out: 0 = CP, 1 = Vbat */
#define LP5521_CLK_SRC_EXT		0	/* Ext-clk source (CLK_32K) */
#define LP5521_CLK_INT			1	/* Internal clock */
#define LP5521_CLK_AUTO			2	/* Automatic clock selection */

/* Bits in LP5562 CONFIG register */
#define LP5562_PWM_HF			LP5521_PWM_HF
#define LP5562_PWRSAVE_EN		LP5521_PWRSAVE_EN
#define LP5562_CLK_SRC_EXT		LP5521_CLK_SRC_EXT
#define LP5562_CLK_INT			LP5521_CLK_INT
#define LP5562_CLK_AUTO			LP5521_CLK_AUTO

struct lp55xx_led_config {
	const char	*name;
	u8		chan_nr;
	u8		led_current; /* mA x10, 0 if led is not connected */
	u8		max_current;
};

struct lp55xx_predef_pattern {
	u8 *r;
	u8 *g;
	u8 *b;
	u8 size_r;
	u8 size_g;
	u8 size_b;
};

struct lp55xx_platform_data {
	/* Common Platform Data */
	struct lp55xx_led_config *led_config;
	u8	num_channels;
	u8	clock_mode;
	int	(*setup_resources)(void);
	void	(*release_resources)(void);
	void	(*enable)(bool state);
	const char *label;
	struct lp55xx_predef_pattern *patterns;
	unsigned int num_patterns;

	/* LP5521, LP5562 Specific Data */
	u8	update_config;
};

extern void lp5562_run_pattern(int val);
extern void lp5562_blink(int rgb, int on, int off);

#endif /* __LINUX_LP55XX_H */
