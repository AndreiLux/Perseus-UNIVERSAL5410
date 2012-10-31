/*
 * Bit error fixing across suspend-to-ram for Snow board.
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MACH_EXYNOS_BITFIX_SNOW_H
#define _MACH_EXYNOS_BITFIX_SNOW_H

/* When recovering we need to know which chunks were skipped. */
typedef bool (bitfix_should_skip_fn_t)(phys_addr_t addr, u32 size);

/* See bitfix-snow.c for descriptions */
#ifdef CONFIG_SNOW_BITFIX
void bitfix_reserve(void);
void bitfix_prepare(void);
void bitfix_finish(void);
bool bitfix_does_overlap_reserved(phys_addr_t addr, unsigned long len);
void bitfix_process_page(phys_addr_t page_addr);
void bitfix_recover_chunk(phys_addr_t failed_addr,
			  bitfix_should_skip_fn_t should_skip_fn);
#else
static inline void bitfix_reserve(void) { ; }
static inline void bitfix_prepare(void) { ; }
static inline void bitfix_finish(void) { ; }
static inline bool bitfix_does_overlap_reserved(phys_addr_t addr,
						unsigned long len)
{
	return false;
}
static inline void bitfix_process_page(phys_addr_t page_addr) { ; }
static inline void bitfix_recover_chunk(phys_addr_t failed_addr,
					bitfix_should_skip_fn_t should_skip_fn)
{
	;
}
#endif

#endif /* _MACH_EXYNOS_BITFIX_SNOW_H */
