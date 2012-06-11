/*
 * AESS IP block integration
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/kernel.h>

#include <plat/omap_hwmod.h>

#include "common.h"

/*
 * AESS_AUTO_GATING_ENABLE_OFFSET: offset in bytes of the AESS IP
 *     block's AESS_AUTO_GATING_ENABLE__1 register from the IP block's
 *     base address
 */
#define AESS_AUTO_GATING_ENABLE_OFFSET				0x07c

/* Register bitfields in the AESS_AUTO_GATING_ENABLE__1 register */
#define AESS_AUTO_GATING_ENABLE_SHIFT				0

/**
 * omap_aess_preprogram - enable AESS internal autogating
 * @oh: struct omap_hwmod *
 *
 * Since the AESS will not IdleAck to the PRCM until its internal
 * autogating is enabled, we must enable autogating during the initial
 * AESS hwmod setup.  Returns 0.
 */
int omap_aess_preprogram(struct omap_hwmod *oh)
{
	u32 v;

	/* Set AESS_AUTO_GATING_ENABLE__1.ENABLE to allow idle entry */
	v = 1 << AESS_AUTO_GATING_ENABLE_SHIFT;
	omap_hwmod_write(v, oh, AESS_AUTO_GATING_ENABLE_OFFSET);

	return 0;
}
