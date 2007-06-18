/*
 * wm8956.h  --  WM8956 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8956_H
#define _WM8956_H

/* WM8956 register space */


#define WM8956_CACHEREGNUM 	56

#define WM8956_LINVOL		0x0
#define WM8956_RINVOL	0x1
#define WM8956_LOUT1		0x2
#define WM8956_ROUT1		0x3
#define WM8956_CLOCK1	0x4
#define WM8956_DACCTL1	0x5
#define WM8956_DACCTL2	0x6
#define WM8956_IFACE1		0x7
#define WM8956_CLOCK2	0x8
#define WM8956_IFACE2		0x9
#define WM8956_LDAC		0xa
#define WM8956_RDAC		0xb

#define WM8956_RESET		0xf
#define WM8956_3D				0x10

#define WM8956_ADDCTL1		0x17
#define WM8956_ADDCTL2		0x18
#define WM8956_POWER1	0x19
#define WM8956_POWER2	0x1a
#define WM8956_ADDCTL3	0x1b
#define WM8956_APOP1		0x1c
#define WM8956_APOP2		0x1d

#define WM8956_LINPATH	0x20
#define WM8956_RINPATH	0x21
#define WM8956_LOUTMIX1	0x22

#define WM8956_ROUTMIX2	0x25
#define WM8956_MONOMIX1	0x26
#define WM8956_MONOMIX2	0x27
#define WM8956_LOUT2		0x28
#define WM8956_ROUT2		0x29
#define WM8956_MONO		0x2a
#define WM8956_INBMIX1	0x2b
#define WM8956_INBMIX2	0x2c
#define WM8956_BYPASS1	0x2d
#define WM8956_BYPASS2	0x2e
#define WM8956_POWER3	0x2f
#define WM8956_ADDCTL4		0x30
#define WM8956_CLASSD1		0x31

#define WM8956_CLASSD3		0x33
#define WM8956_PLLN		0x34
#define WM8956_PLLK1		0x35
#define WM8956_PLLK2		0x36
#define WM8956_PLLK3		0x37


/*
 * WM8956 Clock dividers
 */
#define WM8956_SYSCLKDIV 		0
#define WM8956_DACDIV			1
#define WM8956_OPCLKDIV			2
#define WM8956_DCLKDIV			3
#define WM8956_TOCLKSEL			4
#define WM8956_SYSCLKSEL		5

#define WM8956_SYSCLK_DIV_1		(0 << 1)
#define WM8956_SYSCLK_DIV_2		(2 << 1)

#define WM8956_SYSCLK_MCLK		(0 << 0)
#define WM8956_SYSCLK_PLL		(1 << 0)

#define WM8956_DAC_DIV_1		(0 << 3)
#define WM8956_DAC_DIV_1_5		(1 << 3)
#define WM8956_DAC_DIV_2		(2 << 3)
#define WM8956_DAC_DIV_3		(3 << 3)
#define WM8956_DAC_DIV_4		(4 << 3)
#define WM8956_DAC_DIV_5_5		(5 << 3)
#define WM8956_DAC_DIV_6		(6 << 3)

#define WM8956_DCLK_DIV_1_5		(0 << 6)
#define WM8956_DCLK_DIV_2		(1 << 6)
#define WM8956_DCLK_DIV_3		(2 << 6)
#define WM8956_DCLK_DIV_4		(3 << 6)
#define WM8956_DCLK_DIV_6		(4 << 6)
#define WM8956_DCLK_DIV_8		(5 << 6)
#define WM8956_DCLK_DIV_12		(6 << 6)
#define WM8956_DCLK_DIV_16		(7 << 6)

#define WM8956_TOCLK_F19		(0 << 1)
#define WM8956_TOCLK_F21		(1 << 1)

#define WM8956_OPCLK_DIV_1		(0 << 0)
#define WM8956_OPCLK_DIV_2		(1 << 0)
#define WM8956_OPCLK_DIV_3		(2 << 0)
#define WM8956_OPCLK_DIV_4		(3 << 0)
#define WM8956_OPCLK_DIV_5_5	(4 << 0)
#define WM8956_OPCLK_DIV_6		(5 << 0)

struct wm8956_setup_data {
	unsigned short i2c_address;
};

extern struct snd_soc_codec_dai wm8956_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8956;

#endif
