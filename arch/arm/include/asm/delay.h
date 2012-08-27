/*
 * Copyright (C) 1995-2004 Russell King
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */
#ifndef __ASM_ARM_DELAY_H
#define __ASM_ARM_DELAY_H

#include <asm/param.h>	/* HZ */

extern void __delay(int loops);

/*
 * This function intentionally does not exist; if you see references to
 * it, it means that you're calling ndelay() with an out of range value.
 *
 * With currently imposed limits, this means that we support a max delay
 * of 999999ns. Further limits: HZ<=1000 and bogomips<=8589934
 */
extern void __bad_ndelay(void);

extern void __ndelay(unsigned long usecs);
extern void __const_ndelay(unsigned long);

#define MAX_NDELAY			999999
#define NS_TO_JIFFY_LSHIFT_32(n)	((((u64)(n)*HZ)<<32)/1000000000)

#define ndelay(n)							\
	(__builtin_constant_p(n) ?					\
	  ((n) > MAX_NDELAY ? __bad_ndelay() :				\
			__const_ndelay(NS_TO_JIFFY_LSHIFT_32(n))) :	\
	  __ndelay(n))

#define MAX_UDELAY_NS			999

#define udelay(n) (\
	(__builtin_constant_p(n) && (n) <= MAX_UDELAY_NS) ? ndelay((n)*1000) :\
	({unsigned long __us; \
		for (__us = (n); __us > MAX_UDELAY_NS; __us -= MAX_UDELAY_NS) \
			ndelay(1000*MAX_UDELAY_NS); \
		ndelay(__us*1000); \
		}))

#endif /* defined(_ARM_DELAY_H) */

