/*
 * AD1939 I2S codec
 *
 */

#ifndef _AD1939_H_
#define _AD1939_H_

#define AD1939_PLLCTL0	0x00
#define AD1939_PLLCTL1	0x01
#define AD1939_DACCTL0	0x02
#define AD1939_DACCTL1	0x03
#define AD1939_DACCTL2	0x04
#define AD1939_DACMUTE	0x05
#define AD1939_VOL1L	0x06
#define AD1939_VOL1R	0x07
#define AD1939_VOL2L	0x08
#define AD1939_VOL2R	0x09
#define AD1939_VOL3L	0x0A
#define AD1939_VOL3R	0x0B
#define AD1939_VOL4L	0x0C
#define AD1939_VOL4R	0x0D
#define AD1939_ADCCTL0	0x0E
#define AD1939_ADCCTL1	0x0F
#define AD1939_ADCCTL2	0x10

#define AD1939_REGCOUNT	0x11

/*
 * AD1939 setup data
 */

/* TDM modes. Have a look at the manual to understand what these do. */
#define AD1939_TDM_MODE_TDM		1
#define AD1939_TDM_MODE_AUX		2
#define AD1939_TDM_MODE_DUALLINE	3

/* Master PLL clock source, select one */
#define AD1939_PLL_SRC_MCLK		0	/* external clock */
#define AD1939_PLL_SRC_DACLRCK		1	/* get from DAC LRCLK */
#define AD1939_PLL_SRC_ADCLRCK		2	/* get from ADC LRCLK */

/* clock sources for ADC, DAC. Refer to the manual for more information
 * (for 192000kHz modes, internal PLL _MUST_ be used). Select one for ADC
 * and DAC.
 */
#define AD1939_CLKSRC_DAC_PLL		0	/* DAC clocked by int. PLL */
#define AD1939_CLKSRC_DAC_MCLK		(1<<0)	/* DAC clocked by ext. MCK */
#define AD1939_CLKSRC_ADC_PLL		0	/* ADC clocked by int. PLL */
#define AD1939_CLKSRC_ADC_MCLK		(1<<1)	/* ADC clocked by ext. MCK */

/* I2S Bitclock sources for DAC and ADC I2S interfaces.
 * OR it to ad1939_setup_data.dac_adc_clksrc. Select one for ADC and DAC.
 */
#define AD1939_BCLKSRC_DAC_EXT		0	/* DAC I2SCLK from DBCLK pin */
#define AD1939_BCLKSRC_DAC_PLL		(1<<6)	/* DAC I2SCLK from int. PLL */
#define AD1939_BCLKSRC_ADC_EXT		0	/* DAC I2SCLK from DBCLK pin */
#define AD1939_BCLKSRC_ADC_PLL		(1<<7)	/* DAC I2SCLK from int. PLL */

struct ad1939_setup_data {
	unsigned char i2c_address;
	unsigned char tdm_mode;		/* one of AD1939_TDM_MODE_* */
	unsigned char pll_src;		/* one of AD1939_PLL_SRC_* */
	unsigned char dac_adc_clksrc;	/* AD1939_{B,}CLKSRC_* or'ed together */
};

extern struct snd_soc_codec_device soc_codec_dev_ad1939;
extern struct snd_soc_codec_dai ad1939_dai;

#endif
