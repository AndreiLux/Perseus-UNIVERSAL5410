/*
 *  linux/arch/arm/lib/delay.S
 *
 *  Copyright (C) 1995, 1996 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/param.h>
		.text

.LC0:		.word	loops_per_jiffy
.LC1:		.word	((HZ<<32)/1000000000)

/*
 * r0  <= 999999	(max (HZ * r0) < 4294967295)
 * lpj <= 0xffffffff	(max 8589934 bogomips @ HZ = 1000)
 * HZ  <= 1000		@1000 HZ
 */

ENTRY(__ndelay)
		ldr	r2, .LC1	@ jiffies_per_nsec << 32
		mul	r0, r2, r0
ENTRY(__const_ndelay)			@ 0 <= r0 <= 0xffffffff
		mov	r2, #-1
		ldr	r1, .LC0	@ loops_per_jiffy
		ldr	r1, [r1]	@ max = 0xffffffff
		umull	r1, r0, r0, r1	@ r0 = ((jpn << 32) * lpj) >> 32
		adds	r1, r1, r2
		adcs	r0, r0, #0
		moveq	pc, lr

/*
 * loops = r0 * HZ * loops_per_jiffy / 1000000000
 *
 * Oh, if only we had a cycle counter...
 */

@ Delay routine
ENTRY(__delay)
		subs	r0, r0, #1
		bhi	__delay
		mov	pc, lr
ENDPROC(__ndelay)
ENDPROC(__const_ndelay)
ENDPROC(__delay)
