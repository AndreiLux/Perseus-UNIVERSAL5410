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

#include <linux/kernel.h>

#include <linux/addr_overlap.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of_fdt.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include <plat/pm.h>

#include <mach/bitfix-snow.h>

/*
 * Description of what's going on here...
 *
 * We've got 0x80000000 bytes (2G) of memory.
 *
 * We are sometimes getting a bunch of bit flips (memory corruption) across
 * suspend / resume.  That's because the read-only BIOS has a bug that takes
 * SDRAM out of self-refresh for a while before we start clocking memory.
 *
 * When we get corruption, the following bits are always the same for the
 * addresses of the corrupted bits:
 *  -  0x03ff0083
 * Said another way: if bits were corrupted at *addr1, *addr2, ... *addrN
 * then we know that
 *   (addr1 & 0x03ff0083) == (addr2 & 0x03ff0083)
 *   ...
 *   (addr1 & 0x03ff0083) == (addrN & 0x03ff0083)
 *
 * An explanation of the 0x03ff0083:
 *  - The 0x00000003 bit has to do with byte number within a word (there are 4
 *    bytes in a word).  Each byte in a 32-bit word goes to a different chip.
 *  - The 0x00000080 bit has to do with interleaving.  Addresses with the same
 *    value of 0x00000080 are on the same "channel".  Channel 1 is serviced
 *    by a set of 4 chips (1 per byte) and channel 2 by a different 4 chips.
 *  - The 0x03ff0000 makes some sense but isn't completely understood.  We have
 *    15 row bits that are 0x7fff0000.  ...so it lines up pretty nicely.  It's
 *    just weird that we seem to corrupt rows that are far away from each
 *    other.
 *
 * We see failures of a given chip ~2% of the time during suspend/resume.
 * We think that chips fail independent of each other.  So two chips failing
 * at the same time should be 2% * 2% = .04% of the time.
 *
 * When we see failures we tend to see several failures within a single 4K
 * chunk but nearby 4K chunks are fine.
 *
 * Right now we are only going to take advantage of the third fact above
 * (the 0x03ff0000) to recover from corruption.  We'll call all addresses that
 * have the same value for (addr & 0x03ff0000) part of the same "corruption
 * unit" and assert that all corruption happens within a single corruption unit
 * (unless we get a double-corruption which should be extremely rare).
 *
 * There are 0x400 (1024) corruption units.  We'll "allocate" one corruption
 * unit at boot time (2 megs of memory).  At sleep time we'll compute the xor
 * of the other 1023 corruption units and store it in our allocated unit (we'll
 * call this the xor block).
 *
 * We'll then rely on an out-of-band signal (CRC) to identify bad chunks (8K)
 * of memory at resume time.  When we are told that a chunk is bad then we
 * will reconstruct the chunk.  Since all bad chunks are in a single corruption
 * unit we can always recompute successfully (in the case of a single chip
 * failure).
 *
 * Chunks that are XORed together are called an "XOR group".  There are 1024
 * chunks in an XOR group, each from a different corruption unit.
 *
 *
 * SIDE NOTES:
 * - We chose not to take advantage of the fact that corruption is limited
 *   to 1 of the 8 chips because our out-of-band signal (CRC) can't tell
 *   us this information.
 *
 *
 * PUTTING it into physical address terms (assumes chunk size is 8K):
 *
 * Store xor of:
 *    0x00000000 - 0x00001fff
 *    0x00010000 - 0x00011fff
 *    0x00020000 - 0x00021fff
 *    ...
 *    0x03fe0000 - 0x03fe1fff
 * In 0x03ff0000 - 0x03ff1fff
 *
 * Store xor of:
 *    0x00002000 - 0x00003fff
 *    0x00012000 - 0x00013fff
 *    0x00022000 - 0x00023fff
 *    ...
 *    0x03fe2000 - 0x03fe3fff
 * In 0x03ff2000 - 0x03ff3fff
 *
 * ...
 *
 * Store xor of:
 *    0x0000e000 - 0x0000ffff
 *    0x0001e000 - 0x0001ffff
 *    0x0002e000 - 0x0002ffff
 *    ...
 *    0x03fee000 - 0x03feffff
 * In 0x03ffe000 - 0x03ffffff
 *
 * ...
 * ...
 *
 * Store xor of:
 *    0x04000000 - 0x04001fff
 *    0x04010000 - 0x04011fff
 *    0x04020000 - 0x04021fff
 *    ...
 *    0x07fe0000 - 0x07fe1fff
 * In 0x07ff0000 - 0x07ff1fff
 *
 * ...
 * ...
 * ...
 *
 * Store xor of:
 *    0x7c00e000 - 0x7c00ffff
 *    0x7c01e000 - 0x7c01ffff
 *    0x7c02e000 - 0x7c02ffff
 *    ...
 *    0x7ffee000 - 0x7ffeffff
 * In 0x7fffe000 - 0x7fffffff
 *
 */

/*
 * For now it makes sense to just hard code SDRAM base and size since this
 * only affects a single machine.  If we find this code to be useful in some
 * other scenario then we can adjust to use memblock_start_of_DRAM() and
 * memblock_end_of_DRAM().
 *
 * NOTE: I think memblock can give us this.
 */
#define MEM_SIZE			0x80000000UL
#define SDRAM_BASE			0x40000000

/*
 * If (addr1 & 0x03ff0000) == (addr2 & 0x03ff0000) then they are part of the
 * same corruption unit.  When corruption happens all of it is within a single
 * corruption unit.
 */
#define CU_BITS				10
#define CU_COUNT			(1 << CU_BITS)
#define CU_OFFSET			16
#define CU_MASK				((CU_COUNT - 1) << CU_OFFSET)

/*
 * The out-of-band CRC algorithm works on 8K chunks and will notify us about
 * corruption of whole chunks.  So we work with 8K chunks too.
 *
 * One nice thing about 8K chunks is that it works well with our cache, which
 * is 32KB 2-way set-associative.  I believe that with 8K (and maybe even 16K)
 * we can keep the "destination" block in cache as we walk through chunks doing
 * the xor.
 *
 * We can actually work with chunk sizes where CHUNK_BITS can be anything
 * up to (and including) CU_OFFSET without code changes (though
 * comments above would be wrong), so if it makes more sense to adjust the
 * out-of-band CRC we can.
 */
#define CHUNK_BITS		13
#define CHUNK_SIZE		(1 << CHUNK_BITS)
#define CHUNK_MASK		(CHUNK_SIZE - 1)
#define PAGES_PER_CHUNK		(CHUNK_SIZE / PAGE_SIZE)

/*
 * Collections of chunks that are contiguous and part of the same corruption
 * unit are called "superchunks". It is sometimes convenient to work with
 * superchunks when we're doing things like zeroing memory.
 */
#define SUPERCHUNK_BITS		CU_OFFSET
#define SUPERCHUNK_SIZE		(1 << SUPERCHUNK_BITS)
#define PAGES_PER_SUPERCHUNK	(SUPERCHUNK_SIZE / PAGE_SIZE)

#if CHUNK_BITS > SUPERCHUNK_BITS
#error "Chunks must be smaller than superchunks"
#endif

/*
 * Since the important bits for a corruption unit occur in the middle of
 * an address we need to do two different loops to find xor groups.  We'll
 * call this the "upper" loop and the "lower" loop.
 */

#define UPPER_OFFSET	(CU_OFFSET + CU_BITS)
#define UPPER_LOOPS	(MEM_SIZE / (1 << UPPER_OFFSET))
#define LOWER_OFFSET	CHUNK_BITS
#define LOWER_LOOPS	(1 << (CU_OFFSET - LOWER_OFFSET))

/*
 * We'll reserve one of the corruption units for storing xor.
 * Could be anything 0 - CU_COUNT but we should make
 * sure it doesn't collide with other needs on physical memory.
 *
 * TODO: could actually loop over these since it appears that bitfix_reserve()
 * can detect conflicts.
 */
#define XOR_CU_NUM		(CU_COUNT / 2)


/* Functionality is automatically enabled/disabled at boot time based on need */
static bool bitfix_enabled;

/*
 * You can manually enable bitfix via sysfs.  Don't forget to also enable
 * pm_check in /sys/module/pm_check/parameters/pm_check_enabled
 */
module_param(bitfix_enabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bitfix_enabled, "Enable bitfixing across suspend/resume");

/* We'll recover to this chunk */
static u32 __suspend_volatile_bss recover_chunk[CHUNK_SIZE / sizeof(u32)];


/**
 * Get the address of the given superchunk in the xor corruption unit.
 *
 * @superchunk_num: The superchunk to get.
 * @return: The physical base address of the given superchunk in the xor
 *	corruption unit.
 */

static phys_addr_t bitfix_get_xor_superchunk_addr(int superchunk_num)
{
	phys_addr_t base_addr = SDRAM_BASE + (superchunk_num << UPPER_OFFSET);
	phys_addr_t xor_superchunk_addr = base_addr + (XOR_CU_NUM << CU_OFFSET);

	return xor_superchunk_addr;
}

/**
 * Return the corruption unit of the given chunk.
 *
 * @chunk: Address of a chunk.
 * @return: The corruption unit it belongs to.
 *
 */
static u32 bitfix_get_cu(phys_addr_t chunk)
{
	return (chunk & CU_MASK) >> CU_OFFSET;
}

/**
 * Run xor from src onto dest.
 *
 * This is dest[i] ^= src[i] for count bytes.  Chunks must be 32-bit aligned.
 *
 * @dest: Place to xor into.
 * @src: Place to xor from.
 * @count: Number of _bytes_ to process.  Must be an even number of 32-bit
 *	elements.
 */

static void bitfix_xor32(u32 *dest, const u32 *src, size_t count)
{
	size_t i;

	BUG_ON(count & 0x03);

	for (i = 0; i < count / sizeof(u32); i++)
		dest[i] ^= src[i];
}

/**
 * Run xor of each byte in a page with each equivalent byte in the dest cu.
 *
 * We'll xor the given page into the equivalent page in the given corruption
 * unit (the destination page).  That's dest[i] ^= src[i] for PAGE_SIZE bytes.
 *
 * @page_addr: The address of the page to read.
 * @dest_cu: The corruption unit to store into.
 */

static void bitfix_xor_page(phys_addr_t page_addr, u32 dest_cu)
{
	phys_addr_t dest_page_addr = (page_addr & ~CU_MASK) |
		(dest_cu << CU_OFFSET);
	u32 *virt_page = kmap_atomic(phys_to_page(page_addr));
	u32 *virt_dest_page = kmap_atomic(phys_to_page(dest_page_addr));

	BUG_ON(page_addr & ~PAGE_MASK);
	BUG_ON(dest_page_addr == page_addr);

	bitfix_xor32(virt_dest_page, virt_page, PAGE_SIZE);

	kunmap_atomic(virt_dest_page);
	kunmap_atomic(virt_page);
}

/**
 * Print out which bits were fixed in a page.
 *
 * @addr: The address of the page that was recovered.
 * @orig: The old (corrupted) data.
 * @recovered: The new data.
 */

static void bitfix_compare(phys_addr_t addr, const u32 *orig,
			   const u32 *recovered)
{
	int i;

	for (i = 0; i < PAGE_SIZE/4; i++)
		if (orig[i] != recovered[i])
			pr_info("...fixed 0x%08x from 0x%08x to 0x%08x\n",
				addr + (i * 4), orig[i], recovered[i]);
}

/**
 * Process the given page for bitfixing.
 *
 * The bitfix contract looks something like this:
 * - Only pages that have been processed with this function can be recovered.
 * - If a page isn't processed then the should_skip_fn later passed to
 *   bitfix_recover_chunk() should indicate that it was never processed.
 * - You should never process a page that overlaps bitfix reserved memory.
 *   See bitfix_does_overlap_reserved().
 * - AT THE MOMENT: Either all pages in a chunk need to be processed or none
 *   of them should be.  We could change this assumption.  If we did, we'd
 *   want to change where we call should_skip_fn()--we'd want to call it page-
 *   by-page instead of chunk-by-chunk.
 */

void bitfix_process_page(phys_addr_t page_addr)
{
	u32 cu;

	if (!bitfix_enabled)
		return;

	cu = bitfix_get_cu(page_addr);
	BUG_ON(cu == XOR_CU_NUM);

	bitfix_xor_page(page_addr, XOR_CU_NUM);
}

/**
 * Recover a given chunk to recover_chunk, which should already be cleared.
 *
 * @failed_chunk: Address of the start of the chunk that failed (we'll recover
 *	data from this chunk to recover_chunk).
 * @should_skip_fn: This will be called chunk at a time.  If a chunk was never
 *	processed with calls to bitfix_process_page() then the should_skip_fn
 *	_must_ return true.  This means that the skip function must call the
 *	bitfix_does_overlap_reserved() function.
 */
static void _bitfix_recover_chunk(phys_addr_t failed_chunk,
				  bitfix_should_skip_fn_t should_skip_fn)
{
	const u32 failed_cu = bitfix_get_cu(failed_chunk);
	u32 cu;

	BUG_ON(should_skip_fn(failed_chunk, CHUNK_SIZE));

	for (cu = 0; cu < CU_COUNT; cu++) {
		phys_addr_t this_chunk = (failed_chunk & ~CU_MASK) |
			(cu << CU_OFFSET);
		size_t offset;

		/* Don't include the failed corruption unit in our xor */
		if (cu == failed_cu)
			continue;

		/*
		 * Don't include blocks that were skipped (never passed to
		 * bitfix_process_page()).  Except blocks in the xor corruption
		 * unit.
		 *
		 * should_skip_fn() will return true for the xor corruption
		 * unit but we do still need to process those pages now.
		 *
		 * should_skip_fn() will return true for them because it needs
		 * to incorporate bitfix_ does_overlap_reserved() and that will
		 * return true for the xor corruption unit).
		 */
		if ((cu != XOR_CU_NUM) &&
		    should_skip_fn(this_chunk, CHUNK_SIZE))
			continue;

		for (offset = 0; offset < CHUNK_SIZE; offset += PAGE_SIZE) {
			phys_addr_t this_page = this_chunk + offset;
			u32 *virt_page = kmap_atomic(phys_to_page(this_page));

			bitfix_xor32(&recover_chunk[offset / sizeof(u32)],
				     virt_page, PAGE_SIZE);

			kunmap_atomic(virt_page);
		}
	}
}

/**
 * Recover a chunk of memory that failed.
 *
 * We can only recover a chunk whose pages were processed originally with
 * bitfix_process_page().
 *
 * TODO: we might be able to recover from bit errors of two chips at the same
 * time if we pass in a CRC function here too.  Then we can try all combinations
 * of only recovering bit errors from different sets of failing chips and find
 * one that makes CRC pass.
 *
 * @failed_addr: Any address in the chunk that failed.
 * @should_skip_fn: This will be called chunk at a time.  If a chunk was never
 *	processed with calls to bitfix_process_page() then the should_skip_fn
 *	_must_ return true.  This means that the skip function must call the
 *	bitfix_does_overlap_reserved() function.
 */
void bitfix_recover_chunk(phys_addr_t failed_addr,
			  bitfix_should_skip_fn_t should_skip_fn)
{
	const phys_addr_t bad_chunk_addr = failed_addr & ~(CHUNK_MASK);
	int pgnum;

	if (!bitfix_enabled)
		return;

	pr_info("%s: Attempting recovery at %08x\n", __func__, failed_addr);

	/*
	 * We recover to recover_chunk and then copy instead of recovering
	 * directly to the destination chunk.  That could be critical if
	 * the block we're recovering to is used for something important
	 * (like maybe storing the bitfix code?)
	 */
	memset(recover_chunk, 0, CHUNK_SIZE);
	_bitfix_recover_chunk(bad_chunk_addr, should_skip_fn);

	/* Do comparisons to characterize the corruption. */
	for (pgnum = 0; pgnum < PAGES_PER_CHUNK; pgnum++) {
		u32 offset = pgnum * PAGE_SIZE;
		phys_addr_t addr = bad_chunk_addr + offset;
		u32 *virt;
		u32 *recover_page = recover_chunk + offset / sizeof(u32);

		virt = kmap_atomic(phys_to_page(addr));
		bitfix_compare(addr, virt, recover_page);
		memcpy(virt, recover_page, PAGE_SIZE);
		kunmap_atomic(virt);
	}
}

/**
 * Prepare for running bitfix.
 *
 * This will zero out bitfix memory in preparation for calling
 * bitfix_process_page() on pages.  It will also allocate some internal
 * temporary memory that will be freed with bitfix_finish.
 *
 * This should be called each time before suspend.
 *
 * This function must be called before bitfix_does_overlap_reserved().
 */
void bitfix_prepare(void)
{
	int i;

	if (!bitfix_enabled)
		return;

	/*
	 * Chunk size must match.  Set just in case someone was playing around
	 * with sysfs.
	 */
	s3c_pm_check_set_chunksize(CHUNK_SIZE);

	/* Zero out the xor superchunk. */
	for (i = 0; i < UPPER_LOOPS; i++) {
		phys_addr_t base_addr = SDRAM_BASE + (i << UPPER_OFFSET);
		phys_addr_t xor_superchunk_addr = base_addr +
			(XOR_CU_NUM << CU_OFFSET);
		u32 pgnum;

		for (pgnum = 0; pgnum < (PAGES_PER_SUPERCHUNK); pgnum++) {
			phys_addr_t addr = xor_superchunk_addr +
				(pgnum * PAGE_SIZE);
			void *virt = kmap_atomic(phys_to_page(addr));
			memset(virt, 0, PAGE_SIZE);
			kunmap_atomic(virt);
		}
	}
}

/**
 * Free memory/resources allocated by bitfix_prepare().
 *
 * Currently a no-op.
 */

void bitfix_finish(void)
{
}

/**
 * Tell whether the given address range overlaps bitfix private memory.
 *
 * Bitfix will use its private memory when you call bitfix_process_page()
 * and bitfix_recover_chunk().  In order to avoid confusing things we need
 * to exclude this private memory from any CRCs and from passing to
 * bitfix_process_page().
 *
 * @phys: The physical address to check.
 * @len: The length of the chunk we're checking.
 */

bool bitfix_does_overlap_reserved(phys_addr_t phys, unsigned long len)
{
	int i;

	if (!bitfix_enabled)
		return false;

	for (i = 0; i < UPPER_LOOPS; i++) {
		phys_addr_t base_addr = SDRAM_BASE + (i << UPPER_OFFSET);
		phys_addr_t xor_superchunk_addr = base_addr +
			(XOR_CU_NUM << CU_OFFSET);

		if (phys_addrs_overlap(phys, len, xor_superchunk_addr,
				       SUPERCHUNK_SIZE))
			return true;
	}

	return false;
}

/**
 * Scan the device tree looking for devices that need bitfix.
 *
 * We only run on a single version of the readonly firmware that has the
 * issue we're trying to fix.  We can find the readonly firmware in:
 *	firmware/chromeos/readonly-firmware-version
 * We want to match exactly:
 *	Google_Snow.2695.90.0
 */
static int __init init_dt_scan_bitfix(unsigned long node, const char *uname,
				      int depth, void *data)
{
	bool *is_needed = data;
	const char *version;

	/*
	 * Shouldn't be any other nodes called readonly-firmware-version, so
	 * don't bother checking the whole path.
	 */
	version = of_get_flat_dt_prop(node, "readonly-firmware-version", NULL);
	if (version == NULL)
		return 0;

	*is_needed = strcmp(version, "Google_Snow.2695.90.0") == 0;

	return 0;
}

/**
 * Return whether bitfix is needed on this platform.
 */
static bool bitfix_is_needed(void)
{
	bool is_needed = false;

	of_scan_flat_dt(init_dt_scan_bitfix, &is_needed);
	return is_needed;
}

/**
 * Reserve memory for bitfix; call from platform reserve code.
 *
 * This function will auto-detect whether bitfix is needed and will disable
 * all bitfix functionality if it's not.
 */

void __init bitfix_reserve(void)
{
	int i;
	int ret;

	/*
	 * We'll auto-enable if needed.  However we still allocate memory even
	 * if we detect we're not needed.  That allows us to enable this at
	 * runtime for testing.
	 */
	bitfix_enabled = bitfix_is_needed();

	/* We need pm_check enabled */
	if (bitfix_enabled) {
		pr_info("%s: Detected firmware that needs bitfix\n", __func__);
		s3c_pm_check_set_enable(true);
	}

	for (i = 0; i < UPPER_LOOPS; i++) {
		phys_addr_t xor_superchunk_addr =
			bitfix_get_xor_superchunk_addr(i);
		bool was_reserved;

		pr_debug("%s: trying to reserve %08x@%08x\n",
			__func__, SUPERCHUNK_SIZE, xor_superchunk_addr);
		was_reserved = memblock_is_region_reserved(xor_superchunk_addr,
			SUPERCHUNK_SIZE);
		if (was_reserved) {
			pr_err("%s: memory already reserved %08x@%08x\n",
				__func__, SUPERCHUNK_SIZE, xor_superchunk_addr);
			goto error;
		}

		ret = memblock_reserve(xor_superchunk_addr, SUPERCHUNK_SIZE);
		if (ret) {
			pr_err("%s: memblock_reserve fail (%d) %08x@%08x\n",
				__func__, ret, SUPERCHUNK_SIZE,
				xor_superchunk_addr);
			goto error;
		}
	}

	return;
error:
	/*
	 * If we detected that we needed bitfix code and we couldn't init
	 * then that's a serious problem.  Dump stack so it's pretty obvious.
	 */
	WARN_ON(true);

	for (i--; i >= 0; i--) {
		phys_addr_t xor_superchunk_addr =
			bitfix_get_xor_superchunk_addr(i);
		ret = memblock_free(xor_superchunk_addr, SUPERCHUNK_SIZE);
		WARN_ON(ret);
	}
	bitfix_enabled = false;

	__memblock_dump_all();
}
