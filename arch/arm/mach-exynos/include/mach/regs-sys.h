/*
 * Copyright 2010 Ben Dooks <ben-linux@fluff.org>
 *
 * S5PV210 - System registers definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <plat/map-s5p.h>

/* Registers related to power management */
#define S5P_PMREGx(x)			(S5P_VA_PMU + (x))

#define S5P_USBOTG_PHY_CONTROL		S5P_PMREGx(0x0704)
#define S5P_USBOTG_PHY_EN		(1 << 0)

/* compatibility for hsotg driver */
#define S3C64XX_OTHERS			S5P_USBOTG_PHY_CONTROL
#define S3C64XX_OTHERS_USBMASK		S5P_USBOTG_PHY_EN

