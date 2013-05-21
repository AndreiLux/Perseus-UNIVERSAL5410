#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/asm-offsets.h>
#include <asm/assembler.h>
#include <asm/unwind.h>

/*
 * Inner cache maintenance opeations for the user's area
 * Copied from arch/arm/mm/cache-v7.S
 */


ENTRY(fimg2d_flush_cache_range)
	mrc	p15, 0, r3, c0, c0, 1		@ read ctr
	lsr	r3, r3, #16
	and	r3, r3, #0xf		@ cache line size encoding
	mov	r2, #4			@ bytes per word
	mov	r2, r2, lsl r3		@ actual cache line size
	add	r1, r0, r1
	sub	r3, r2, #1
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
	mcr	p15, 0, r0, c7, c14, 1
	add	r0, r0, r2
2:
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
bad_area:
	stmfd	sp!, {r0-r3, lr}
	bl fimg2d_fixup_user_fault
	cmp	r0, #0
	ldmfd	sp!, {r0-r3, lr}
	beq	1b
	add	r0, r0, #4096
	bic	r0, r0, #0x03f
	bic	r0, r0, #0xfc0
	b	2b
ENDPROC(fimg2d_flush_cache_range)
.pushsection __ex_table, "a"
	.long	1b, bad_area
.popsection

