/*
 * arizona_control.h - WolfonMicro WM51xx kernel-space register control
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: June 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <sound/soc.h>
#include <linux/regmap.h>

#define ARIZONA_MIXER_VOL_MASK             0x00FE
#define ARIZONA_MIXER_VOL_SHIFT                 1
#define ARIZONA_MIXER_VOL_WIDTH                 7

void arizona_control_init(struct snd_soc_codec *pcodec);
void arizona_control_regmap_hook(struct regmap *pmap, unsigned int reg,
		      		unsigned int *val);


