/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <linux/init.h>

ENTRY(exynos_smc)
	stmfd	sp!, {lr}
	smc	0
	ldmfd	sp!, {pc}
ENDPROC(exynos_smc)
