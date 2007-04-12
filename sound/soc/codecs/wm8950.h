/*
 * wm8950.h  --  WM8950 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8950_H
#define _WM8950_H

/* WM8950 register space */

#define WM8950_RESET		0x0
#define WM8950_POWER1		0x1
#define WM8950_POWER2		0x2
#define WM8950_POWER3		0x3
#define WM8950_IFACE		0x4
#define WM8950_COMP			0x5
#define WM8950_CLOCK		0x6
#define WM8950_ADD			0x7
#define WM8950_GPIO			0x8
#define WM8950_DAC			0xa
#define WM8950_ADC			0xe
#define WM8950_ADCVOL		0xf
#define WM8950_EQ1			0x12
#define WM8950_EQ2			0x13
#define WM8950_EQ3			0x14
#define WM8950_EQ4			0x15
#define WM8950_EQ5			0x16
#define WM8950_NOTCH1		0x1b
#define WM8950_NOTCH2		0x1c
#define WM8950_NOTCH3		0x1d
#define WM8950_NOTCH4		0x1e
#define WM8950_ALC1			0x20
#define WM8950_ALC2			0x21
#define WM8950_ALC3			0x22
#define WM8950_NGATE		0x23
#define WM8950_PLLN			0x24
#define WM8950_PLLK1		0x25
#define WM8950_PLLK2		0x26
#define WM8950_PLLK3		0x27
#define WM8950_INPUT		0x2c
#define WM8950_INPPGA		0x2d
#define WM8950_ADCBOOST		0x2f
#define WM8950_OUTPUT		0x31

#define WM8950_CACHEREGNUM 	57

/* Clock divider Id's */
#define WM8950_OPCLKDIV		0
#define WM8950_MCLKDIV		1
#define WM8950_ADCCLK		2
#define WM8950_BCLKDIV		3


/* ADC clock dividers */
#define WM8950_ADCCLK_F2	(1 << 3)
#define WM8950_ADCCLK_F4	(0 << 3)

/* PLL Out dividers */
#define WM8950_OPCLKDIV_1	(0 << 4)
#define WM8950_OPCLKDIV_2	(1 << 4)
#define WM8950_OPCLKDIV_3	(2 << 4)
#define WM8950_OPCLKDIV_4	(3 << 4)

/* BCLK clock dividers */
#define WM8950_BCLKDIV_1	(0 << 2)
#define WM8950_BCLKDIV_2	(1 << 2)
#define WM8950_BCLKDIV_4	(2 << 2)
#define WM8950_BCLKDIV_8	(3 << 2)
#define WM8950_BCLKDIV_16	(4 << 2)
#define WM8950_BCLKDIV_32	(5 << 2)

/* MCLK clock dividers */
#define WM8950_MCLKDIV_1	(0 << 5)
#define WM8950_MCLKDIV_1_5	(1 << 5)
#define WM8950_MCLKDIV_2	(2 << 5)
#define WM8950_MCLKDIV_3	(3 << 5)
#define WM8950_MCLKDIV_4	(4 << 5)
#define WM8950_MCLKDIV_6	(5 << 5)
#define WM8950_MCLKDIV_8	(6 << 5)
#define WM8950_MCLKDIV_12	(7 << 5)


struct wm8950_setup_data {
	unsigned short i2c_address;
};

extern struct snd_soc_codec_dai wm8950_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8950;

#endif
