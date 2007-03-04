/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IMX_SSI_H
#define _IMX_SSI_H

/* pxa2xx DAI SSP ID's */
#define IMX_DAI_SSI1			0
#define IMX_DAI_SSI2			1

/* SSI clock sources */
#define IMX_SSP_SYS_CLK		0

/* SSI audio dividers */
#define IMX_SSI_DIV_2			0
#define IMX_SSI_DIV_PSR			1
#define IMX_SSI_DIV_PM			2

/* SSI Div 2 */
#define IMX_SSI_DIV_2_OFF		~SSI_STCCR_DIV2
#define IMX_SSI_DIV_2_ON		SSI_STCCR_DIV2

extern struct snd_soc_cpu_dai imx_ssi_pcm_dai[2];

#endif
