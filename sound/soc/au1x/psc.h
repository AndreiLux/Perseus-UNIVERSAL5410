/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _AU1X_PCM_H
#define _AU1X_PCM_H

#define PSC0_BASE	0xb1a00000
#define PSC1_BASE	0xb1b00000
#define PSC2_BASE	0xb0a00000
#define PSC3_BASE	0xb0b00000

#define PSC_SEL		0x0
#define PSC_CTL		0x4

#define PSC_I2SCFG	0x08
#define PSC_I2SMASK	0x0C
#define PSC_I2SPCR	0x10
#define PSC_I2SSTAT	0x14
#define PSC_I2SEVNT	0x18
#define PSC_I2SRXTX	0x1C
#define PSC_I2SUDF	0x20

#define PSC_AC97CFG	0x08
#define PSC_AC97MASK	0x0C
#define PSC_AC97PCR	0x10
#define PSC_AC97STAT	0x14
#define PSC_AC97EVNT	0x18
#define PSC_AC97RXTX	0x1C
#define PSC_AC97CDC	0x20
#define PSC_AC97RST	0x24
#define PSC_AC97GPO	0x28
#define PSC_AC97GPI	0x2C

#ifdef CONFIG_SOC_AU1200
#define PSC_COUNT 2
#elif defined(CONFIG_SOC_AU1550)
#define PSC_COUNT 4
#endif

extern struct snd_soc_cpu_dai au1xpsc_ac97_dai[PSC_COUNT];
extern struct snd_soc_cpu_dai au1xpsc_i2s_dai[PSC_COUNT];
extern struct snd_soc_platform au1xpsc_soc_platform;
extern struct snd_ac97_bus_ops soc_ac97_ops;

#endif
