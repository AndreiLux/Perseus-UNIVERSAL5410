/* linux/arch/arm/plat-samsung/include/plat/regs_ielcd.h
 *
 * Header file for Samsung (IELCD) driver
 *
 * Copyright (c) 2009 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_IELCD_H
#define _REGS_IELCD_H

#define S3C_IELCD_PHY_BASE		0x14440000
#define S3C_IELCD_MAP_SIZE		SZ_32K

/* Register Offset */
#define IELCD_VIDCON0			(0x0000)
#define IELCD_VIDCON1			(0x0004)

#define IELCD_VIDTCON0		(0x0010)
#define IELCD_VIDTCON1		(0x0014)
#define IELCD_VIDTCON2		(0x0018)

#define IELCD_WINCON0			(0x0020)

#define IELCD_VIDOSD0C		(0x0048)
#define IELCD_VIDW00ADD2		(0x0100)

#define IELCD_AUXCON			(0x0278)

/* Value */
#define IELCD_MAGIC_KEY		(0x2ff47)
#define IELCD_WINDOW0_FIXED		(0x40002d)

/* Register bit */
#define IELCD_VIDTCON2_LINEVAL_FOR_5410(_x)		((_x) << 12)
#define IELCD_VIDTCON2_HOZVAL(_x)			((_x) << 0)

#endif
