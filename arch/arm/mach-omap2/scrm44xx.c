/*
 * OMAP4/5 SCRM module functions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Tero Kristo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>

#include "scrm44xx.h"

void omap4_scrm_write(u32 val, u32 offset)
{
	__raw_writel(val, OMAP44XX_SCRM_REGADDR(offset));
}

void omap5_scrm_write(u32 val, u32 offset)
{
	__raw_writel(val, OMAP54XX_SCRM_REGADDR(offset));
}
