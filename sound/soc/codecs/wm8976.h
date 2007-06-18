/*
 * wm8976.h  --  WM8976 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8976_H
#define _WM8976_H

/* WM8976 register space */

#define WM8976_RESET		0x0
#define WM8976_POWER1		0x1
#define WM8976_POWER2		0x2
#define WM8976_POWER3		0x3
#define WM8976_IFACE		0x4
#define WM8976_COMP			0x5
#define WM8976_CLOCK		0x6
#define WM8976_ADD			0x7
#define WM8976_GPIO			0x8
#define WM8976_JACK1        0x9
#define WM8976_DAC			0xa
#define WM8976_DACVOLL	    0xb
#define WM8976_DACVOLR      0xc
#define WM8976_JACK2        0xd
#define WM8976_ADC			0xe
#define WM8976_ADCVOL		0xf
#define WM8976_EQ1			0x12
#define WM8976_EQ2			0x13
#define WM8976_EQ3			0x14
#define WM8976_EQ4			0x15
#define WM8976_EQ5			0x16
#define WM8976_DACLIM1		0x18
#define WM8976_DACLIM2		0x19
#define WM8976_NOTCH1		0x1b
#define WM8976_NOTCH2		0x1c
#define WM8976_NOTCH3		0x1d
#define WM8976_NOTCH4		0x1e
#define WM8976_ALC1			0x20
#define WM8976_ALC2			0x21
#define WM8976_ALC3			0x22
#define WM8976_NGATE		0x23
#define WM8976_PLLN			0x24
#define WM8976_PLLK1		0x25
#define WM8976_PLLK2		0x26
#define WM8976_PLLK3		0x27
#define WM8976_3D           0x29
#define WM8976_BEEP         0x2b
#define WM8976_INPUT		0x2c
#define WM8976_INPPGA	  	0x2d
#define WM8976_ADCBOOST		0x2f
#define WM8976_OUTPUT		0x31
#define WM8976_MIXL	        0x32
#define WM8976_MIXR         0x33
#define WM8976_HPVOLL		0x34
#define WM8976_HPVOLR       0x35
#define WM8976_SPKVOLL      0x36
#define WM8976_SPKVOLR      0x37
#define WM8976_OUT3MIX		0x38
#define WM8976_MONOMIX      0x39

#define WM8976_CACHEREGNUM 	58

/*
 * WM8976 Clock dividers
 */
#define WM8976_MCLKDIV 		0
#define WM8976_BCLKDIV		1
#define WM8976_OPCLKDIV		2
#define WM8976_DACOSR		3
#define WM8976_ADCOSR		4
#define WM8976_MCLKSEL		5

#define WM8976_MCLK_MCLK		(0 << 8)
#define WM8976_MCLK_PLL			(1 << 8)

#define WM8976_MCLK_DIV_1		(0 << 5)
#define WM8976_MCLK_DIV_1_5		(1 << 5)
#define WM8976_MCLK_DIV_2		(2 << 5)
#define WM8976_MCLK_DIV_3		(3 << 5)
#define WM8976_MCLK_DIV_4		(4 << 5)
#define WM8976_MCLK_DIV_5_5		(5 << 5)
#define WM8976_MCLK_DIV_6		(6 << 5)

#define WM8976_BCLK_DIV_1		(0 << 2)
#define WM8976_BCLK_DIV_2		(1 << 2)
#define WM8976_BCLK_DIV_4		(2 << 2)
#define WM8976_BCLK_DIV_8		(3 << 2)
#define WM8976_BCLK_DIV_16		(4 << 2)
#define WM8976_BCLK_DIV_32		(5 << 2)

#define WM8976_DACOSR_64		(0 << 3)
#define WM8976_DACOSR_128		(1 << 3)

#define WM8976_ADCOSR_64		(0 << 3)
#define WM8976_ADCOSR_128		(1 << 3)

#define WM8976_OPCLK_DIV_1		(0 << 4)
#define WM8976_OPCLK_DIV_2		(1 << 4)
#define WM8976_OPCLK_DIV_3		(2 << 4)
#define WM8976_OPCLK_DIV_4		(3 << 4)

struct wm8976_setup_data {
	unsigned short i2c_address;
};

extern struct snd_soc_codec_dai wm8976_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8976;

#endif
