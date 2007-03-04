/*
 * tlv320.h  --  TLV 320 ALSA Soc Audio driver
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright 2006 Atlab srl.
 *
 * Authors: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *          Nicola Perrino <nicola.perrino@atlab.it>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _TLV320_H
#define _TLV320_H

#define TLV320AIC24K


/* TLV320 register space */
#define CODEC_NOOP	0x00
#define CODEC_REG1	0x01
#define CODEC_REG2	0x02
#define CODEC_REG3A	0x03
#define CODEC_REG3B	0x04
#define CODEC_REG3C	0x05
#define CODEC_REG3D	0x06
#define CODEC_REG4A	0x07
#define CODEC_REG4B	0x08
#define CODEC_REG5A	0x09
#define CODEC_REG5B	0x0a
#define CODEC_REG5C	0x0b
#define CODEC_REG5D	0x0c
#define CODEC_REG6A	0x0d
#define CODEC_REG6B	0x0e


//	Control Register 1
#define REG1_CONTINUOUS			0x40
#define REG1_IIR_EN			0x20
#define REG1_MIC_BIAS_235		0x08
#define REG1_ANALOG_LOOP_BACK		0x04
#define REG1_DIGITAL_LOOP_BACK		0x02
#define REG1_DAC16			0x01

//	Control Register 2
#define REG2_TURBO_EN			0x80
#define REG2_FIR_BYPASS			0x40
#define REG2_GPIO			0x02
#define REG2_GPIO_1			0x06

//	Control Register 3A
#define REG3_PWDN_ALL			0x30
#define REG3_PWDN_ADC			0x10
#define REG3_PWDN_DAC			0x20
#define REG3_SW_RESET			0x08
#define REG3_SAMPLING_FACTOR1		0x01
#define REG3_SAMPLING_FACTOR2		0x02

//	Control Register 3B
#define REG3_8KBP_EN			0x60
#define REG3_MUTE_OUTP1			0x42
#define REG3_MUTE_OUTP2			0x48
#define REG3_MUTE_OUTP3			0x44

//	Control Register 4
#define REG4_FSDIV_M			0x85	//M=5
#define REG4_FSDIV_NP			0x08	//N=1, P=8
//#define REG4_FSDIV_NP			0x01	//N=1, P=8
#define REG4_FSDIV_NP1			0x02	//N=16, P=2

//	Control Register 5
#define REG5A_ADC_GAIN			0x02	//3dB
#define REG5A_ADC_MUTE			0x0f	//Mute
#define REG5B_DAC_GAIN			0x42	//-3dB
#define REG5B_DAC_MUTE			0x4f	//Mute
#define REG5C_SIDETONE_MUTE		0xBF

//	Control Register 6
#define REG6A_AIC24A_CH1_IN		0x08	//INP1 to ADC
#define REG6B_AIC24A_CH1_OUT		0x82	//OUTP2 to DAC
#define REG6A_AIC24A_CH2_IN		0x02	//INP2 to ADC
#define REG6B_AIC24A_CH2_OUT		0x81	//OUTP3 to DAC

/* clock inputs */
#define TLV320_MCLK		0
#define TLV320_PCMCLK		1


struct tlv320_setup_data {
	unsigned short i2c_address;
};

/* DAI ifmodes */
/* mode 1 IFMODE = 00 */
#define TLV320_DAI_MODE1_VOICE	0
#define TLV320_DAI_MODE1_HIFI	1
/* mode 2 IFMODE = 01 */
#define TLV320_DAI_MODE2_VOICE	2
/* mode 3 IFMODE = 10 */
#define TLV320_DAI_MODE3_HIFI	3
/* mode 4 IFMODE = 11 */
#define TLV320_DAI_MODE4_HIFI	4

extern struct snd_soc_codec_dai tlv320_dai[5];
extern struct snd_soc_codec_device soc_codec_dev_tlv320;

#endif
