/* linux/arch/arm/plat-s3c/pm-check.c
 *  originally in linux/arch/arm/plat-s3c24xx/pm.c
 *
 * Copyright (c) 2004-2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C Power Mangament - suspend/resume memory corruptiuon check.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>

#include <linux/addr_overlap.h>
#include <linux/suspend.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/highmem.h>
#include <linux/ioport.h>
#include <asm/memory.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <plat/pm.h>

#include <mach/bitfix-snow.h>

#if CONFIG_SAMSUNG_PM_CHECK_CHUNKSIZE < 1
#error CONFIG_SAMSUNG_PM_CHECK_CHUNKSIZE must be a positive non-zero value
#endif

/* suspend checking code...
 *
 * this next area does a set of crc checks over all the installed
 * memory, so the system can verify if the resume was ok.
 *
 * CONFIG_SAMSUNG_PM_CHECK_CHUNKSIZE defines the block-size for the CRC,
 * increasing it will mean that the area corrupted will be less easy to spot,
 * and reducing the size will cause the CRC save area to grow
*/

/*
 * Possible values for pm_check_checksum_type:
 * - crc: Best (but slowest).  Catches the most problems.
 * - sum: Simple 32-bit sum.
 * - xor: Simple 32-bit xor.  Bad at catching problems but gives you more
 *        information about the problems you caught (you can tell if all
 *        errors were in the same byte, etc).
 */
#define CHECKSUM_TYPE_CRC	"crc"
#define CHECKSUM_TYPE_SUM	"sum"
#define CHECKSUM_TYPE_XOR	"xor"

static char *pm_check_checksum_type = CHECKSUM_TYPE_SUM;
static bool pm_check_enabled;
static bool pm_check_should_panic = 1;
static bool pm_check_print_skips;
static bool pm_check_print_timings;
static int pm_check_chunksize = CONFIG_SAMSUNG_PM_CHECK_CHUNKSIZE * 1024;

static u32 crc_size;	/* size needed for the crc block */
static u32 *crcs;	/* allocated over suspend/resume */

static phys_addr_t stack_base_phys;
static phys_addr_t crcs_phys;

static u32 (*checksum_func)(u32 val, unsigned char const *ptr, size_t len);

/* number of errors found this resume */
static __suspend_volatile_bss u32 crc_err_cnt;


typedef u32 *(run_fn_t)(struct resource *ptr, u32 *arg);

module_param(pm_check_checksum_type, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_check_checksum_type, "Checksum type (crc, xor, or sum)");
module_param(pm_check_enabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_check_enabled,
		"Enable memory validation on suspend/resume");
module_param(pm_check_should_panic, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_check_should_panic, "Panic on CRC errors");
module_param(pm_check_print_skips, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_check_print_skips, "Print skipped regions");
module_param(pm_check_print_timings, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_check_print_timings,
		"Print time to compute checks (causes runtime warnings)");
module_param(pm_check_chunksize, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_check_chunksize,
		"Size of blocks for CRCs, shold be 1K multiples");

static inline void s3c_pm_printskip(char *desc, unsigned long addr)
{
	if (!pm_check_print_skips)
		return;
	S3C_PMDBG("s3c_pm_check: skipping %08lx, has %s in\n", addr, desc);
}

static bool s3c_pm_should_skip_page(phys_addr_t addr)
{
	if (phys_addrs_overlap(addr, PAGE_SIZE, stack_base_phys, THREAD_SIZE)) {
		s3c_pm_printskip("stack", addr);
		return true;
	}
	if (phys_addrs_overlap(addr, PAGE_SIZE, crcs_phys, crc_size)) {
		s3c_pm_printskip("crc block", addr);
		return true;
	}
	if (pm_does_overlap_suspend_volatile(addr, PAGE_SIZE)) {
		s3c_pm_printskip("volatile suspend data", addr);
		return true;
	}
	if (bitfix_does_overlap_reserved(addr)) {
		s3c_pm_printskip("bitfix", addr);
		return true;
	}

	return false;
}

/**
 * s3c_pm_xor_mem() - XOR all the words in the memory range passed
 * @val: Initial value to start the XOR from
 * @ptr: Pointer to the start of the memory range
 * @len: Length (in bytes) of the memory range
 *
 */

static u32 s3c_pm_xor_mem(u32 val, unsigned char const *ptr, size_t len)
{
	const u32 *word_ptr = (const u32 *)ptr;
	int i;

	len >>= 2; /* get words, not bytes */
	for (i = 0; i < len; i++, word_ptr++)
		val ^= *word_ptr;
	return val;
}

/**
 * s3c_pm_sum_mem() - Sum all the words in the memory range passed
 * @val: Initial value to start the sum from
 * @ptr: Pointer to the start of the memory range
 * @len: Length (in bytes) of the memory range
 *
 */
static u32 s3c_pm_sum_mem(u32 val, unsigned char const *ptr, size_t len)
{
	const u32 *word_ptr = (const u32 *)ptr;
	int i;

	len >>= 2; /* get words, not bytes */
	for (i = 0; i < len; i++, word_ptr++)
		val += *word_ptr;
	return val;
}

/**
 * s3c_pm_process_mem() - Process one memory chunk for checksum
 * @val: Starting point for checksum function
 * @addr: Pointer to the physical memory range.  This must be page-aligned.
 * @len: Length (in bytes) of the memory range
 * @for_save: If true we're processing memory for saving data.
 *
 * Loop through the chunk in PAGE_SIZE blocks, ensuring each
 * page is mapped.  Use either crc32 or xor checksum and return
 * back.
 */
static inline u32 s3c_pm_process_mem(u32 val, u32 addr, size_t len,
				     bool for_save)
{
	size_t processed = 0;

	if (unlikely(addr & (PAGE_SIZE - 1))) {
		printk(KERN_ERR "s3c_pm_check: Unaligned address: 0x%x\n",
			addr);
		return val;
	}
	for (processed = 0; processed < len; processed += PAGE_SIZE) {
		int pfn = __phys_to_pfn(addr + processed);
		unsigned long left = len - processed;
		void *virt;

		if (left > PAGE_SIZE)
			left = PAGE_SIZE;
		if (s3c_pm_should_skip_page(addr + processed))
			continue;
		virt = kmap_atomic(pfn_to_page(pfn));
		if (!virt) {
			printk(KERN_ERR "s3c_pm_check: Could not map "
				"page for pfn %d with addr 0x%x\n",
				pfn, addr + len - processed);
			return val;
		}
		val = checksum_func(val, virt, left);
		kunmap_atomic(virt);

		if (for_save)
			bitfix_process_page(addr + processed);
	}
	return val;
}

/* s3c_pm_run_res
 *
 * go through the given resource list, and look for system ram
*/

static void s3c_pm_run_res(struct resource *ptr, run_fn_t fn, u32 *arg)
{
	while (ptr != NULL) {
		if (ptr->child != NULL)
			s3c_pm_run_res(ptr->child, fn, arg);

		if ((ptr->flags & IORESOURCE_MEM) &&
		    strcmp(ptr->name, "System RAM") == 0) {
			S3C_PMDBG("s3c_pm_check: Found system RAM at "
				  "%08lx..%08lx\n",
				  (unsigned long)ptr->start,
				  (unsigned long)ptr->end);
			arg = (fn)(ptr, arg);
		}

		ptr = ptr->sibling;
	}
}

static void s3c_pm_run_sysram(run_fn_t fn, u32 *arg)
{
	s3c_pm_run_res(&iomem_resource, fn, arg);
}

static u32 *s3c_pm_countram(struct resource *res, u32 *val)
{
	u32 size = (u32)resource_size(res);

	size = DIV_ROUND_UP(size, pm_check_chunksize);

	S3C_PMDBG("s3c_pm_check: Area %08lx..%08lx, %d blocks\n",
		  (unsigned long)res->start, (unsigned long)res->end, size);

	*val += size * sizeof(u32);
	return val;
}

/* s3c_pm_prepare_check
 *
 * prepare the necessary information for creating the CRCs. This
 * must be done before the final save, as it will require memory
 * allocating, and thus touching bits of the kernel we do not
 * know about.
*/
void s3c_pm_check_prepare(void)
{
	ktime_t start, stop, delta;

	if (!pm_check_enabled)
		return;

	if (strcasecmp(pm_check_checksum_type, CHECKSUM_TYPE_CRC) == 0) {
		checksum_func = crc32_le;
	} else if (strcasecmp(pm_check_checksum_type, CHECKSUM_TYPE_XOR) == 0) {
		checksum_func = s3c_pm_xor_mem;
	} else {
		if (strcasecmp(pm_check_checksum_type, CHECKSUM_TYPE_SUM) != 0)
			pr_warn("pm_check: Unknown checksum '%s'; using '%s'\n",
				pm_check_checksum_type, CHECKSUM_TYPE_SUM);
		checksum_func = s3c_pm_sum_mem;
	}

	crc_size = 0;
	/* Timing code generates warnings at this point in suspend */
	if (pm_check_print_timings)
		start = ktime_get();
	bitfix_prepare();
	s3c_pm_run_sysram(s3c_pm_countram, &crc_size);
	if (pm_check_print_timings) {
		stop = ktime_get();
		delta = ktime_sub(stop, start);
		S3C_PMDBG("s3c_pm_check: Suspend prescan took %lld usecs\n",
			(unsigned long long)ktime_to_ns(delta) >> 10);
	}

	S3C_PMDBG("s3c_pm_check: Chunk size: %d, %u bytes for checks needed\n",
		pm_check_chunksize, crc_size);

	crcs = kmalloc(crc_size+4, GFP_KERNEL);
	if (crcs == NULL)
		printk(KERN_ERR "Cannot allocated CRC save area\n");
}

/* externs from sleep.S */
extern u32 *sleep_save_sp;

/**
 * Zero memory that the sleep code may have used but doesn't care about.
 *
 * The sleep code uses some temporary memory that it doesn't need anymore
 * after resume.  We zero it out here so we don't have to skip a whole chunk.
 * This keeps CRCs consistent.
 *
 * Alternatively we could change sleep code to zero this memory, but we run
 * into some issues if we ever get a failed suspend (the normal resume code
 * doesn't run).
 */
static void zero_temp_memory(void)
{
	sleep_save_sp = 0;
}

static u32 *s3c_pm_makecheck(struct resource *res, u32 *val)
{
	unsigned long addr, left;

	for (addr = res->start; addr <= res->end;
	     addr += pm_check_chunksize, val++) {
		left = res->end - addr + 1;
		if (left > pm_check_chunksize)
			left = pm_check_chunksize;

		*val = s3c_pm_process_mem(~0, addr, left, true);
	}
	return val;
}

/* s3c_pm_check_store
 *
 * compute the CRC values for the memory blocks before the final
 * sleep.
*/

void s3c_pm_check_store(void)
{
	ktime_t start, stop, delta;

	if (crcs == NULL)
		return;

	if (pm_check_print_timings)
		start = ktime_get();

	zero_temp_memory();
	stack_base_phys = virt_to_phys(current_thread_info());
	crcs_phys = virt_to_phys(crcs);

	s3c_pm_run_sysram(s3c_pm_makecheck, crcs);
	if (pm_check_print_timings) {
		stop = ktime_get();
		delta = ktime_sub(stop, start);
		S3C_PMDBG("s3c_pm_check: Suspend memory scan took %lld usecs\n",
			(unsigned long long)ktime_to_ns(delta) >> 10);
	}
}

/**
 * s3c_pm_runcheck() - helper to check a resource on restore.
 * @res: The resource to check
 * @vak: Pointer to list of CRC32 values to check.
 *
 * Called from the s3c_pm_check_restore() via s3c_pm_run_sysram(), this
 * function runs the given memory resource checking it against the stored
 * CRC to ensure that memory is restored. The function tries to skip as
 * many of the areas used during the suspend process.
 */
static u32 *s3c_pm_runcheck(struct resource *res, u32 *val)
{
	unsigned long addr;
	unsigned long left;
	u32 calc;

	for (addr = res->start; addr <= res->end;
	     addr += pm_check_chunksize, val++) {
		left = res->end - addr + 1;

		if (left > pm_check_chunksize)
			left = pm_check_chunksize;
		/* calculate and check the checksum */

		calc = s3c_pm_process_mem(~0, addr, left, false);
		if (calc != *val) {
			S3C_PMDBG("s3c_pm_check: Restore CRC error at %08lx "
				"(%08x vs %08x)\n",
				addr, calc, *val);

			bitfix_recover_chunk(addr, s3c_pm_should_skip_page);
			calc = s3c_pm_process_mem(~0, addr, left, false);
			if (calc != *val) {
				crc_err_cnt++;
				S3C_PMDBG("s3c_pm_check: ERROR: "
					  "recovery failed!\n");
			} else {
				S3C_PMDBG("s3c_pm_check: Recovered\n");
			}
		}
	}

	return val;
}

/**
 * s3c_pm_check_restore() - memory check called on resume
 *
 * check the CRCs after the restore event and free the memory used
 * to hold them
*/
void s3c_pm_check_restore(void)
{
	ktime_t start, stop, delta;

	if (crcs == NULL)
		return;

	crc_err_cnt = 0;
	if (pm_check_print_timings)
		start = ktime_get();
	zero_temp_memory();
	s3c_pm_run_sysram(s3c_pm_runcheck, crcs);
	if (pm_check_print_timings) {
		stop = ktime_get();
		delta = ktime_sub(stop, start);
		S3C_PMDBG("s3c_pm_check: Resume memory scan took %lld usecs\n",
			(unsigned long long)ktime_to_ns(delta) >> 10);
	}
	if (crc_err_cnt) {
		S3C_PMDBG("s3c_pm_check: %d CRC errors\n", crc_err_cnt);
		if (pm_check_should_panic)
			panic("%d CRC errors\n", crc_err_cnt);
	}
}

/**
 * s3c_pm_check_cleanup() - free memory resources
 *
 * Free the resources that where allocated by the suspend
 * memory check code. We do this separately from the
 * s3c_pm_check_restore() function as we cannot call any
 * functions that might sleep during that resume.
 */
void s3c_pm_check_cleanup(void)
{
	kfree(crcs);
	crcs = NULL;
	bitfix_finish();
}

/**
 * s3c_pm_check_enable() - enable suspend/resume memory checks
 * @enabled: True to enable, false to disable
 *
 */
void s3c_pm_check_set_enable(bool enabled)
{
	pm_check_enabled = enabled;
}

/**
 * s3c_pm_check_set_chunksize() - set chunksize
 * @chunksize: The value to set the chunksize to.
 */
void s3c_pm_check_set_chunksize(int chunksize)
{
	pm_check_chunksize = chunksize;
}
