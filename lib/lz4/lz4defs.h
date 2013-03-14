/*
 * LZ4 Decompressor for Linux kernel
 *
 * Copyright (C) 2013, LG Electronics, Kyungsik Lee <kyungsik.lee@lge.com>
 *
 * Based on LZ4 implementation by Yann Collet.
 *
 * LZ4 - Fast LZ compression algorithm
 * Copyright (C) 2011-2012, Yann Collet.
 * BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You can contact the author at :
 *  - LZ4 homepage : http://fastcompression.blogspot.com/p/lz4.html
 *  - LZ4 source repository : http://code.google.com/p/lz4/
 */

/*
 * Detects 64 bits mode
 */
#if (defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) \
	|| defined(__ppc64__) || defined(__LP64__))
#define LZ4_ARCH64 1
#else
#define LZ4_ARCH64 0
#endif

/*
 * Compiler Options
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L /* C99 */
/* "restrict" is a known keyword */
#else
#define restrict	/* Disable restrict */
#endif

/*
 * Architecture-specific macros
 */
#define BYTE	u8
typedef struct _U16_S { u16 v; } U16_S;
typedef struct _U32_S { u32 v; } U32_S;
typedef struct _U64_S { u64 v; } U64_S;
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)

#define A16(x) (((U16_S *)(x))->v)
#define A32(x) (((U32_S *)(x))->v)
#define A64(x) (((U64_S *)(x))->v)

#define PUT4(s, d) (A32(d) = A32(s))
#define PUT8(s, d) (A64(d) = A64(s))
#define LZ4_WRITE_LITTLEENDIAN_16(p, v)	\
	do {	\
		A16(p) = v; \
		p += 2; \
	} while (0)
#else /* CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS */

#define A64(x) get_unaligned((u64 *)&(((U16_S *)(x))->v))
#define A32(x) get_unaligned((u32 *)&(((U16_S *)(x))->v))
#define A16(x) get_unaligned((u16 *)&(((U16_S *)(x))->v))

#define PUT4(s, d) \
	put_unaligned(get_unaligned((const u32 *) s), (u32 *) d)
#define PUT8(s, d) \
	put_unaligned(get_unaligned((const u64 *) s), (u64 *) d)
#endif

#define LZ4_WRITE_LITTLEENDIAN_16(p, v)	\
	do {	\
		put_unaligned(v, (u16 *)(p)); \
		p += 2; \
	} while (0)

#define COPYLENGTH 8
#define ML_BITS  4
#define ML_MASK  ((1U << ML_BITS) - 1)
#define RUN_BITS (8 - ML_BITS)
#define RUN_MASK ((1U << RUN_BITS) - 1)
#define MEMORY_USAGE	14
#define MINMATCH	4
#define SKIPSTRENGTH	6
#define LASTLITERALS	5
#define MFLIMIT		(COPYLENGTH + MINMATCH)
#define MINLENGTH	(MFLIMIT + 1)
#define MAXD_LOG	16
#define MAXD		(1 << MAXD_LOG)
#define MAXD_MASK	(u32)(MAXD - 1)
#define MAX_DISTANCE	(MAXD - 1)
#define HASH_LOG	(MAXD_LOG - 1)
#define HASHTABLESIZE	(1 << HASH_LOG)
#define MAX_NB_ATTEMPTS	256
#define OPTIMAL_ML	(int)((ML_MASK-1)+MINMATCH)
#define LZ4_64KLIMIT	((1<<16) + (MFLIMIT - 1))
#define HASHLOG64K	((MEMORY_USAGE - 2) + 1)
#define HASH64KTABLESIZE	(1U << HASHLOG64K)
#define LZ4_HASH_VALUE(p)	(((A32(p)) * 2654435761U) >> \
				((MINMATCH * 8) - (MEMORY_USAGE-2)))
#define LZ4_HASH64K_VALUE(p)	(((A32(p)) * 2654435761U) >> \
				((MINMATCH * 8) - HASHLOG64K))
#define HASH_VALUE(p)		(((A32(p)) * 2654435761U) >> \
				((MINMATCH * 8) - HASH_LOG))

#if LZ4_ARCH64/* 64-bit */
#define STEPSIZE 8

#define LZ4_COPYSTEP(s, d)	\
	do {	\
		PUT8(s, d);	\
		d += 8;	\
		s += 8;	\
	} while (0)

#define LZ4_COPYPACKET(s, d)	LZ4_COPYSTEP(s, d)

#define LZ4_SECURECOPY(s, d, e)	\
	do {				\
		if (d < e) {		\
			LZ4_WILDCOPY(s, d, e);	\
		}	\
	} while (0)
#define HTYPE u32

#ifdef __BIG_ENDIAN
#define LZ4_NBCOMMONBYTES(val) (__builtin_clzll(val) >> 3)
#else
#define LZ4_NBCOMMONBYTES(val) (__builtin_ctzll(val) >> 3)
#endif

#else	/* 32-bit */
#define STEPSIZE 4

#define LZ4_COPYSTEP(s, d)	\
	do {	\
		PUT4(s, d);	\
		d += 4;	\
		s += 4;	\
	} while (0)

#define LZ4_COPYPACKET(s, d)	\
	do {			\
		LZ4_COPYSTEP(s, d);	\
		LZ4_COPYSTEP(s, d);	\
	} while (0)

#define LZ4_SECURECOPY	LZ4_WILDCOPY
#define HTYPE const u8*

#ifdef __BIG_ENDIAN
#define LZ4_NBCOMMONBYTES(val) (__builtin_clz(val) >> 3)
#else
#define LZ4_NBCOMMONBYTES(val) (__builtin_ctz(val) >> 3)
#endif

#endif

#define LZ4_READ_LITTLEENDIAN_16(d, s, p) \
	(d = s - get_unaligned_le16(p))
#define LZ4_WILDCOPY(s, d, e)	\
	do {				\
		LZ4_COPYPACKET(s, d);	\
	} while (d < e)

#define LZ4_BLINDCOPY(s, d, l)	\
	do {	\
		u8 *e = (d) + l;	\
		LZ4_WILDCOPY(s, d, e);	\
		d = e;	\
	} while (0)
