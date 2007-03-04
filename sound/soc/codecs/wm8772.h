/*
 * wm8772.h  --  audio driver for WM8772
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _WM8772_H
#define _WM8772_H

/* WM8772 register space */

#define WM8772_LDAC1VOL   0x00
#define WM8772_RDAC1VOL   0x01
#define WM8772_DACCH      0x02
#define WM8772_IFACE      0x03
#define WM8772_LDAC2VOL   0x04
#define WM8772_RDAC2VOL   0x05
#define WM8772_LDAC3VOL   0x06
#define WM8772_RDAC3VOL   0x07
#define WM8772_MDACVOL    0x08
#define WM8772_DACCTRL    0x09
#define WM8772_DACRATE    0x0a
#define WM8772_ADCRATE    0x0b
#define WM8772_ADCCTRL    0x0c
#define WM8772_RESET	  0x1f

#define WM8772_CACHE_REGNUM 	10

#define WM8772_DACCLK	0
#define WM8772_ADCCLK	1

#define WM8753_DAI_DAC		0
#define WM8753_DAI_ADC		1

extern struct snd_soc_codec_dai wm8772_dai[2];
extern struct snd_soc_codec_device soc_codec_dev_wm8772;

#endif
