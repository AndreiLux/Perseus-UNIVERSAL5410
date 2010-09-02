/*
 * Originally based on dm-crypt.c,
 * Copyright (C) 2003 Christophe Saout <christophe@saout.de>
 * Copyright (C) 2004 Clemens Fruhwirth <clemens@endorphin.org>
 * Copyright (C) 2006-2008 Red Hat, Inc. All rights reserved.
 * Copyright (C) 2010 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *                    All Rights Reserved.
 *
 * This file is released under the GPL.
 *
 * Implements a verifying transparent block device.
 * See Documentation/device-mapper/dm-verity.txt
 */
#include <linux/async.h>
#include <linux/backing-dev.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mempool.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>
#include <linux/scatterlist.h>
#include <linux/chromeos_platform.h>

#include <asm/atomic.h>
#include <asm/page.h>
#include <asm/unaligned.h>

#include <crypto/hash.h>
#include <crypto/sha.h>

/* #define CONFIG_DM_DEBUG 1 */
#define CONFIG_DM_VERITY_TRACE 1
#include <linux/device-mapper.h>
#include <linux/dm-bht.h>

#define DM_MSG_PREFIX "verity"
#define MESG_STR(x) x, sizeof(x)

/* Supports up to 512-bit digests */
#define VERITY_MAX_DIGEST_SIZE 64

/* TODO(wad) make both of these report the error line/file to a
 *           verity_bug function.
 */
#define VERITY_BUG(msg...) BUG()
#define VERITY_BUG_ON(cond, msg...) BUG_ON(cond)

/* Helper for printing sector_t */
#define ULL(x) ((unsigned long long)(x))

/* Requires the pool to have the given # of pages available. They are only
 * used for padding non-block aligned requests so each request should need
 * at most 2 additional pages. This means our maximum queue without suffering
 * from memory contention could be 32 requests.
 */
#define MIN_POOL_PAGES 16
/* IOS represent min of dm_verity_ios in a pool, but we also use it to
 * preallocate biosets (MIN_IOS * 2):
 * 1. We need to clone the entire bioset, including bio_vecs, before passing
 *    them to the underlying block layer since it may alter the values.
 * 2. We need to pad out biosets that are not block aligned.
 * 3. We need to be able to create biosets while loading in hashes.
 * This will need more tweaking for specific workload expectations.
 */
#define MIN_IOS 32
/* During io_bht_read, we will spawn _many_ bios for a single I/O early on, but
 * once the tree is populated, we will only need MIN_IOS at most to be able to
 * pad out the request. We will also need space for the padding biovecs which
 * is at most 2, less than one page per side.
 */
#define MIN_BIOS (MIN_IOS * 2)

/* We use a block size == page_size in sectors */
#define VERITY_BLOCK_SIZE ((PAGE_SIZE) >> (SECTOR_SHIFT))

/* Support additional tracing of requests */
#ifdef CONFIG_DM_VERITY_TRACE
#define VERITY_TRACE(param, fmt, args...) { \
	if (param) \
		DMINFO(fmt, ## args); \
}
static int request_trace;
module_param(request_trace, bool, 0644);
MODULE_PARM_DESC(request_trace, "Enable request tracing to DMINFO");

static int alloc_trace;
module_param(alloc_trace, bool, 0644);
MODULE_PARM_DESC(alloc_trace, "Enable allocation tracing to DMINFO");
#else
#define VERITY_TRACE(...)
#endif

#define REQTRACE(fmt, args...) VERITY_TRACE(request_trace, "req: " fmt, ## args)
#define ALLOCTRACE(fmt, args...) \
	VERITY_TRACE(alloc_trace, "alloc: " fmt, ## args)

/* MIN_BIOS * 2 is a safe upper bound.  An upper bound is desirable, especially
 * with larger dm-bhts because multiple requests will be issued to bootstrap
 * initial dm-bht root verification.
 */
static int max_bios = MIN_BIOS * 2;
module_param(max_bios, int, 0644);
MODULE_PARM_DESC(max_bios, "Max number of allocated BIOs");

/* Provide a lightweight means of specifying the global default for
 * error behavior: eio, reboot, or none
 * Legacy support for 0 = eio, 1 = reboot/panic, and 2 = none.
 */
enum verity_error_behavior {
	DM_VERITY_ERROR_BEHAVIOR_EIO = 0, DM_VERITY_ERROR_BEHAVIOR_PANIC,
	DM_VERITY_ERROR_BEHAVIOR_NONE };
static const char *allowed_error_behaviors[] = { "eio", "panic", "none", NULL };
#define DM_VERITY_MAX_ERROR_BEHAVIOR sizeof("panic")
static char *error_behavior = "eio";
module_param(error_behavior, charp, 0644);
MODULE_PARM_DESC(error_behavior, "Behavior on error (eio, reboot, none)");

/* Control whether the reads are optimized for sequential access by pre-reading
 * entries in the bht.
 */
static int bht_readahead;
module_param(bht_readahead, int, 0644);
MODULE_PARM_DESC(bht_readahead, "Number of entries to readahead in the bht");

/* Controls whether verity_get_device will wait forever for a device. */
static int dev_wait;
module_param(dev_wait, bool, 0444);
MODULE_PARM_DESC(dev_wait, "Wait forever for a backing device")

/* Used for tracking pending bios as well as for exporting information via
 * STATUSTYPE_INFO.
 */
struct verity_stats {
	unsigned int pending_bio;
	unsigned int io_queue;
	unsigned int verify_queue;
	unsigned int average_requeues;
	unsigned int total_requeues;
	unsigned long long total_requests;
};

/* Holds the context of a given verify operation. */
struct verify_context {
	struct bio *bio;  /* block_size padded bio or aligned original bio */
	unsigned int offset;  /* into current bio_vec */
	unsigned int idx;  /* of current bio_vec */
	unsigned int needed;  /* for next hash */
	sector_t block;  /*  current block */
};

/* per-requested-bio private data */
enum next_queue_type { DM_VERITY_NONE, DM_VERITY_IO, DM_VERITY_VERIFY };
struct dm_verity_io {
	struct dm_target *target;
	struct bio *base_bio;
	struct delayed_work work;
	unsigned int next_queue;

	struct verify_context ctx;
	int error;
	atomic_t pending;

	sector_t sector;  /* unaligned, converted to target sector */
	sector_t block;  /* aligned block index */
	sector_t count;  /* aligned count in blocks */

	/* Tracks the number of bv_pages that were allocated
	 * from the local pool during request padding.
	 */
	unsigned int leading_pages;
	unsigned int trailing_pages;
};

struct verity_config {
	struct dm_dev *dev;
	sector_t start;

	struct dm_dev *hash_dev;
	sector_t hash_start;

	struct dm_bht bht;

	/* Pool required for io contexts */
	mempool_t *io_pool;
	/* Pool and bios required for making sure that backing device reads are
	 * in PAGE_SIZE increments.
	 */
	mempool_t *page_pool;
	struct bio_set *bs;

	/* Single threaded I/O submitter */
	struct workqueue_struct *io_queue;
	/* Multithreaded verifier queue */
	struct workqueue_struct *verify_queue;

	char hash_alg[CRYPTO_MAX_ALG_NAME];
	struct hash_desc *hash;  /* one per cpu */

	char error_behavior[DM_VERITY_MAX_ERROR_BEHAVIOR];

	struct verity_stats stats;
};

static struct kmem_cache *_verity_io_pool;

static void kverityd_queue_verify(struct dm_verity_io *io);
static void kverityd_queue_io(struct dm_verity_io *io, bool delayed);
static void kverityd_io_bht_populate(struct dm_verity_io *io);
static void kverityd_io_bht_populate_end(struct bio *, int error);


/*-----------------------------------------------
 * Statistic tracking functions
 *-----------------------------------------------*/

void verity_stats_pending_bio_inc(struct verity_config *vc)
{
	vc->stats.pending_bio++;
}

void verity_stats_pending_bio_dec(struct verity_config *vc)
{
	vc->stats.pending_bio--;
}

void verity_stats_io_queue_inc(struct verity_config *vc)
{
	vc->stats.io_queue++;
}

void verity_stats_verify_queue_inc(struct verity_config *vc)
{
	vc->stats.verify_queue++;
}

void verity_stats_io_queue_dec(struct verity_config *vc)
{
	vc->stats.io_queue--;
}

void verity_stats_verify_queue_dec(struct verity_config *vc)
{
	vc->stats.verify_queue--;
}

void verity_stats_total_requeues_inc(struct verity_config *vc)
{
	if (vc->stats.total_requeues >= INT_MAX - 1) {
		DMINFO("stats.total_requeues is wrapping");
		vc->stats.total_requeues = 0;
	}
	vc->stats.total_requeues++;
}

void verity_stats_total_requests_inc(struct verity_config *vc)
{
	vc->stats.total_requests++;
}

void verity_stats_average_requeues(struct verity_config *vc, int requeues)
{
	/* TODO(wad) */
}

/*-----------------------------------------------
 * Allocation and utility functions
 *-----------------------------------------------*/

static void verity_align_request(struct verity_config *vc, sector_t *start,
				 unsigned int *size);
static void kverityd_src_io_read_end(struct bio *clone, int error);

/* Shared destructor for all internal bios */
static void dm_verity_bio_destructor(struct bio *bio)
{
	struct dm_verity_io *io = bio->bi_private;
	struct verity_config *vc = io->target->private;
	verity_stats_pending_bio_dec(io->target->private);
	bio_free(bio, vc->bs);
}

/* This function may block if the number of outstanding requests is too high. */
struct bio *verity_alloc_bioset(struct verity_config *vc, gfp_t gfp_mask,
				int nr_iovecs)
{
	/* Don't flood the I/O or over allocate from the pools */
	verity_stats_pending_bio_inc(vc);
	while (vc->stats.pending_bio > (unsigned int) max_bios) {
		DMINFO("too many outstanding BIOs (%u). sleeping.",
		       vc->stats.pending_bio - 1);
		/* The request may be for dev->bdev, but all additional requests
		 * come through the hash_dev and are the issue for clean up.
		 */
		msleep_interruptible(10);
	}
	return bio_alloc_bioset(gfp_mask, nr_iovecs, vc->bs);
}

static struct dm_verity_io *verity_io_alloc(struct dm_target *ti,
					    struct bio *bio, sector_t sector)
{
	struct verity_config *vc = ti->private;
	struct dm_verity_io *io;
	unsigned int aligned_size = bio->bi_size;
	sector_t aligned_sector = sector;

	ALLOCTRACE("dm_verity_io for sector %llu", ULL(sector));
	io = mempool_alloc(vc->io_pool, GFP_NOIO);
	if (unlikely(!io))
		return NULL;
	/* By default, assume io requests will require a hash */
	io->next_queue = DM_VERITY_IO;
	io->target = ti;
	io->base_bio = bio;
	io->sector = sector;
	io->error = 0;
	io->leading_pages = 0;
	io->trailing_pages = 0;
	io->ctx.bio = NULL;

	verity_align_request(vc, &aligned_sector, &aligned_size);
	/* Adjust the sector by the virtual starting sector */
	io->block = (to_bytes(aligned_sector)) >> PAGE_SHIFT;
	io->count = aligned_size >> PAGE_SHIFT;

	DMDEBUG("io_alloc for %llu blocks starting at %llu",
		ULL(io->count), ULL(io->block));

	atomic_set(&io->pending, 0);

	return io;
}

/* ctx->bio should be prepopulated */
static int verity_verify_context_init(struct verity_config *vc,
				      struct verify_context *ctx)
{
	if (unlikely(ctx->bio == NULL)) {
		DMCRIT("padded bio was not supplied to kverityd");
		return -EINVAL;
	}
	ctx->offset = 0;
	ctx->needed = PAGE_SIZE;
	ctx->idx = ctx->bio ? ctx->bio->bi_idx : 0;
	/* The sector has already be translated and adjusted to the
	 * appropriate start for reading.
	 */
	ctx->block = to_bytes(ctx->bio->bi_sector) >> PAGE_SHIFT;
	/* Setup the new hash context too */
	if (crypto_hash_init(&vc->hash[smp_processor_id()])) {
		DMCRIT("Failed to initialize the crypto hash");
		return -EFAULT;
	}
	return 0;
}

static void clone_init(struct dm_verity_io *io, struct bio *clone,
		       unsigned short vcnt, unsigned int size, sector_t start)
{
	struct verity_config *vc = io->target->private;

	clone->bi_private = io;
	clone->bi_end_io  = kverityd_src_io_read_end;
	clone->bi_bdev    = vc->dev->bdev;
	clone->bi_rw      = io->base_bio->bi_rw;
	clone->bi_destructor = dm_verity_bio_destructor;
	clone->bi_idx = 0;
	clone->bi_vcnt = vcnt;
	clone->bi_size = size;
	clone->bi_sector = start;
}

static void verity_align_request(struct verity_config *vc, sector_t *start,
				 unsigned int *size)
{
	sector_t base_sector;
	VERITY_BUG_ON(!start || !size || !vc, "NULL arguments");

	base_sector = *start;
	/* Mask off to the starting sector for a block */
	*start = base_sector & (~(to_sector(PAGE_SIZE) - 1));
	/* Add any extra bytes from the lead */
	*size += to_bytes(base_sector - *start);
	/* Now round up the size to the full block size */
	*size = PAGE_ALIGN(*size);
}

/* Populates a bio_vec array starting with the pointer provided allocating
 * pages from the given page pool until bytes reaches 0.
 * The next position in the bio_vec array is returned on success. On
 * failure, a NULL is returned.
 * It is assumed that the bio_vec array is properly sized.
 */
static struct bio_vec *populate_bio_vec(struct bio_vec *bvec,
					mempool_t *page_pool,
					unsigned int bytes,
					unsigned int *pages_added) {
	gfp_t gfp_mask = GFP_NOIO | __GFP_HIGHMEM;
	if (!bvec || !page_pool || !pages_added)
		return NULL;

	while (bytes > 0) {
		DMDEBUG("bytes == %u", bytes);
		bvec->bv_offset = 0;
		bvec->bv_len = min(bytes, (unsigned int)PAGE_SIZE);
		ALLOCTRACE("page for bio_vec %p", bvec);
		bvec->bv_page = mempool_alloc(page_pool, gfp_mask);
		if (unlikely(bvec->bv_page == NULL)) {
			DMERR("Out of pages in the page_pool!");
			return NULL;
		}
		bytes -= bvec->bv_len;
		++*pages_added;
		++bvec;
	}
	return bvec;
}

static int verity_hash_bv(struct verity_config *vc,
			  struct verify_context *ctx)
{
	struct bio_vec *bv = bio_iovec_idx(ctx->bio, ctx->idx);
	struct scatterlist sg;
	unsigned int size = bv->bv_len - (bv->bv_offset + ctx->offset);

	/* Catch unexpected undersized bvs */
	if (bv->bv_len < bv->bv_offset + ctx->offset) {
		ctx->offset = 0;
		ctx->idx++;
		return 0;
	}

	/* Only update the hash with the amount that's needed for this
	 * block (as tracked in the ctx->sector).
	 */
	size = min(size, ctx->needed);
	DMDEBUG("Updating hash for block %llu vector idx %u: "
		"size:%u offset:%u+%u len:%u",
		ULL(ctx->block), ctx->idx, size, bv->bv_offset, ctx->offset,
		bv->bv_len);

	/* Updates the current digest hash context for the on going block */
	sg_init_table(&sg, 1);
	sg_set_page(&sg, bv->bv_page, size, bv->bv_offset + ctx->offset);

	/* Use one hash_desc+tfm per cpu so that we can make use of all
	 * available cores when verifying.  Only one context is handled per
	 * processor, however.
	 */
	if (crypto_hash_update(&vc->hash[smp_processor_id()], &sg, size)) {
		DMCRIT("Failed to update crypto hash");
		return -EFAULT;
	}
	ctx->needed -= size;
	ctx->offset += size;

	if (ctx->offset + bv->bv_offset >= bv->bv_len) {
		ctx->offset = 0;
		/* Bump the bio_segment counter */
		ctx->idx++;
	}

	return 0;
}

static void verity_free_buffer_pages(struct verity_config *vc,
				     struct dm_verity_io *io, struct bio *clone)
{
	unsigned int original_vecs = clone->bi_vcnt;
	struct bio_vec *bv;
	VERITY_BUG_ON(!vc || !io || !clone, "NULL arguments");

	/* No work to do. */
	if (!io->leading_pages && !io->trailing_pages)
		return;

	/* Determine which pages are ours with one page per vector. */
	original_vecs -= io->leading_pages + io->trailing_pages;

	/* Handle any leading pages */
	bv = bio_iovec_idx(clone, 0);
	while (io->leading_pages--) {
		if (unlikely(bv->bv_page == NULL)) {
			VERITY_BUG("missing leading bv_page in padding");
			bv++;
			continue;
		}
		mempool_free(bv->bv_page, vc->page_pool);
		bv->bv_page = NULL;
		bv++;
	}
	bv += original_vecs;
	/* TODO(wad) This is probably off-by-one */
	while (io->trailing_pages--) {
		if (unlikely(bv->bv_page == NULL)) {
			VERITY_BUG("missing leading bv_page in padding");
			bv++;
			continue;
		}
		mempool_free(bv->bv_page, vc->page_pool);
		bv->bv_page = NULL;
		bv++;
	}
}

static void kverityd_verify_cleanup(struct dm_verity_io *io, int error)
{
	struct verity_config *vc = io->target->private;
	/* Clean up the pages used for padding, if any. */
	verity_free_buffer_pages(vc, io, io->ctx.bio);

	/* Release padded bio, if one was used. */
	if (io->ctx.bio != io->base_bio) {
		bio_put(io->ctx.bio);
		io->ctx.bio = NULL;
	}
	/* Propagate the verification error. */
	io->error = error;
}

/* If the request is not successful, this handler takes action.
 * TODO make this call a registered handler.
 */
static void verity_error(struct verity_config *vc, struct dm_verity_io *io,
			 int error) {
	const char *message;
	dev_t devt = 0;
	if (vc)
		devt = vc->dev->bdev->bd_dev;
	if (io)
		io->error = -EIO;

	switch (error) {
	case -ENOMEM:
		message = "out of memory";
		break;
	case -EBUSY:
		message = "pending data seen during verify";
		break;
	case -EFAULT:
		message = "crypto operation failure";
		break;
	case -EACCES:
		message = "integrity failure";
		break;
	case -EPERM:
		message = "hash tree population failure";
		break;
	case -EINVAL:
		message = "unexpected missing/invalid data";
		break;
	default:
		/* Other errors can be passed through as IO errors */
		return;
	}

	DMERR_LIMIT("verification failure occurred: %s", message);
	if (!strcmp(vc->error_behavior,
		    allowed_error_behaviors[DM_VERITY_ERROR_BEHAVIOR_EIO]))
		return;

	if (!strcmp(vc->error_behavior,
		    allowed_error_behaviors[DM_VERITY_ERROR_BEHAVIOR_NONE])) {
		if (error != -EIO && io)
			io->error = 0;
		return;
	}

	/*  else DM_VERITY_ERROR_BEHAVOR_PANIC */
	/* TODO(wad) If panic_timoeut is zero, a reboot does not occur.
	 * extern int panic_timeout = 1;
	 */

	/* Flag to firmware that this filesystem failed and needs recovery */
	chromeos_set_need_recovery();

	panic("dm-verity failure: "
		"device:%u:%u error%d block:%llu message:%s",
		MAJOR(devt), MINOR(devt), error, io->block, message);
}

/**
 * verity_error_behavior_prepare - check and clean up error_behaviors
 * @behavior:	char array of size DM_VERITY_BMAX_ERROR_BEHAVIOR
 *
 * Checks if the behavior is valid and rewrites it if it was a
 * legacy mode (digit).
 * Returns true if valid, false if not.
 */
static bool verity_error_behavior_prepare(char *behavior)
{
	const char **allowed = allowed_error_behaviors;
	char legacy = '0';
	bool ok = false;
	if (!behavior)
		return false;
	do {
		if (!strcmp(*allowed, behavior)) {
			ok = true;
			break;
		}
		/* Support legacy 0, 1, 2  by rewriting it. */
		if (behavior[0] == legacy++) {
			strcpy(behavior, *allowed);
			ok = true;
			break;
		}
	} while (*(++allowed));
	return ok;
}


/**
 * match_dev_by_uuid - callback for finding a partition using its uuid
 * @dev:	device passed in by the caller
 * @data:	opaque pointer to a uuid packed by part_pack_uuid().
 *
 * Returns 1 if the device matches, and 0 otherwise.
 */
static int match_dev_by_uuid(struct device *dev, void *data)
{
	u8 *uuid = data;
	struct hd_struct *part = dev_to_part(dev);

	if (!part->info)
		goto no_match;

	if (memcmp(uuid, part->info->uuid, sizeof(part->info->uuid)))
			goto no_match;

	return 1;
no_match:
	return 0;
}

/**
 * dm_get_device_by_uuid: claim a device using its UUID
 * @ti:			current dm_target
 * @uuid_string:	36 byte UUID hex encoded
 * 			(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
 * @dev_start:		offset in sectors passed to dm_get_device
 * @dev_len:		length in sectors passed to dm_get_device
 * @dm_dev:		dm_dev to populate
 *
 * Wraps dm_get_device allowing it to use a unique partition id to
 * find a given partition on any drive. This code is based on
 * printk_all_partitions in that it walks all of the register block devices.
 *
 * N.B., uuid_string is not checked for safety just strlen().
 */
static int dm_get_device_by_uuid(struct dm_target *ti, const char *uuid_str,
			     sector_t dev_start, sector_t dev_len,
			     struct dm_dev **dm_dev)
{
	struct device *dev = NULL;
	dev_t devt = 0;
	char devt_buf[BDEVT_SIZE];
	u8 uuid[16];
	size_t uuid_length = strlen(uuid_str);

	if (uuid_length < 36)
		goto bad_uuid;
	/* Pack the requested UUID in the expected format. */
	part_pack_uuid(uuid_str, uuid);

	dev = class_find_device(&block_class, NULL, uuid, &match_dev_by_uuid);
	if (!dev)
		goto found_nothing;

	devt = dev->devt;
	put_device(dev);

	/* The caller may specify +/-%u after the UUID if they want a partition
	 * before or after the one identified.
	 */
	if (uuid_length > 36) {
		unsigned int part_offset;
		char sign;
		unsigned minor = MINOR(devt);
		if (sscanf(uuid_str + 36, "%c%u", &sign, &part_offset) == 2) {
			if (sign == '+') {
				minor += part_offset;
			} else if (sign == '-') {
				minor -= part_offset;
			} else {
				DMWARN("Trailing characters after UUID: %s\n",
					uuid_str);
			}
			devt = MKDEV(MAJOR(devt), minor);
		}
	}

	/* Construct the dev name to pass to dm_get_device.  dm_get_device
	 * doesn't support being passed a dev_t.
	 */
	snprintf(devt_buf, sizeof(devt_buf), "%u:%u", MAJOR(devt), MINOR(devt));

	/* TODO(wad) to make this generic we could also pass in the mode. */
	if (dm_get_device(ti, devt_buf, dm_table_get_mode(ti->table), dm_dev) == 0)
		return 0;

	ti->error = "Failed to acquire device";
	DMDEBUG("Failed to acquire discovered device %s", devt_buf);
	return -1;
bad_uuid:
	ti->error = "Bad UUID";
	DMDEBUG("Supplied value '%s' is an invalid UUID", uuid_str);
	return -1;
found_nothing:
	DMDEBUG("No matching partition for GUID: %s", uuid_str);
	ti->error = "No matching GUID";
	return -1;
}

static int verity_get_device(struct dm_target *ti, const char *devname,
			     sector_t dev_start, sector_t dev_len,
			     struct dm_dev **dm_dev)
{
	do {
		/* Try the normal path first since if everything is ready, it
		 * will be the fastest.
		 */
		if (!dm_get_device(ti, devname, dm_table_get_mode(ti->table), dm_dev))
			return 0;

		/* Try the device by partition UUID */
		if (!dm_get_device_by_uuid(ti, devname, dev_start, dev_len,
		                           dm_dev))
			return 0;

		/* No need to be too aggressive since this is a slow path. */
		msleep(500);
	} while (dev_wait && (driver_probe_done() != 0 || *dm_dev == NULL));
	async_synchronize_full();
	return -1;
}


/*-----------------------------------------------------------------
 * Reverse flow of requests into the device.
 *
 * (Start at the bottom with verity_map and work your way upward).
 *-----------------------------------------------------------------*/

static void verity_inc_pending(struct dm_verity_io *io);

/* verity_dec_pending manages the lifetime of all dm_verity_io structs.
 * Non-bug error handling is centralized through this interface and
 * all passage from workqueue to workqueue.
 */
static void verity_dec_pending(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	struct bio *base_bio = io->base_bio;
	int error = io->error;
	VERITY_BUG_ON(!io, "NULL argument");

	DMDEBUG("dec pending %p: %d--", io, atomic_read(&io->pending));

	if (!atomic_dec_and_test(&io->pending))
		goto more_work_to_do;

	if (unlikely(error)) {
		/* We treat bad I/O as a compromise so that we go
		 * to recovery mode. VERITY_BUG will just reboot on
		 * e.g., OOM errors.
		 */
		verity_error(vc, io, error);
		/* let the handler change the error. */
		error = io->error;
		goto return_to_user;
	}

	if (io->next_queue == DM_VERITY_IO) {
		kverityd_queue_io(io, true);
		DMDEBUG("io %p requeued for io");
		goto more_work_to_do;
	}

	if (io->next_queue == DM_VERITY_VERIFY) {
		verity_stats_io_queue_dec(vc);
		verity_stats_verify_queue_inc(vc);
		kverityd_queue_verify(io);
		DMDEBUG("io %p enqueued for verify");
		goto more_work_to_do;
	}

return_to_user:
	/* else next_queue == DM_VERITY_NONE */
	mempool_free(io, vc->io_pool);

	/* Return back to the caller */
	bio_endio(base_bio, error);

more_work_to_do:
	return;
}

/* Walks the, potentially padded, data set and computes the hash of the
 * data read from the untrusted source device.  The computed hash is
 * then passed to dm-bht for verification.
 */
static int verity_verify(struct verity_config *vc,
			 struct verify_context *ctx)
{
	u8 digest[VERITY_MAX_DIGEST_SIZE];
	int r;
	unsigned int block = 0;
	unsigned int digest_size =
		crypto_hash_digestsize(vc->hash[smp_processor_id()].tfm);

	while (ctx->idx < ctx->bio->bi_vcnt) {
		r = verity_hash_bv(vc, ctx);
		if (r < 0)
			goto bad_hash;

		/* Continue until all the data expected is processed */
		if (ctx->needed)
			continue;

		/* Calculate the final block digest to check in the tree */
		if (crypto_hash_final(&vc->hash[smp_processor_id()], digest)) {
			DMCRIT("Failed to compute final digest");
			r = -EFAULT;
			goto bad_state;
		}
		block = (unsigned int)(ctx->block);
		r = dm_bht_verify_block(&vc->bht, block, digest, digest_size);
		/* dm_bht functions aren't expected to return errno friendly
		 * values.  They are converted here for uniformity.
		 */
		if (r > 0) {
			DMERR("Pending data for block %llu seen at verify",
			      ULL(ctx->block));
			r = -EBUSY;
			goto bad_state;
		}
		if (r < 0) {
			DMERR_LIMIT("Block hash does not match!");
			r = -EACCES;
			goto bad_match;
		}
		REQTRACE("Block %llu verified", ULL(ctx->block));

		/* Reset state for the next block */
		if (crypto_hash_init(&vc->hash[smp_processor_id()])) {
			DMCRIT("Failed to initialize the crypto hash");
			r = -ENOMEM;
			goto no_mem;
		}
		ctx->needed = PAGE_SIZE;
		ctx->block++;
		/* After completing a block, allow a reschedule.
		 * TODO(wad) determine if this is truly needed.
		 */
		cond_resched();
	}

	return 0;

bad_hash:
no_mem:
bad_state:
bad_match:
	return r;
}

/* Services the verify workqueue */
static void kverityd_verify(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work,
						  work);
	struct dm_verity_io *io = container_of(dwork, struct dm_verity_io,
					       work);
	struct verity_config *vc = io->target->private;
	int r = 0;

	verity_inc_pending(io);

	/* Default to ctx.bio as the padded bio clone.
	 * The original bio is never touched until release.
	 */
	r = verity_verify_context_init(vc, &io->ctx);

	/* Only call verify if context initialization succeeded */
	if (!r)
		r = verity_verify(vc, &io->ctx);
	/* Free up the padded bio and tag with the return value */
	kverityd_verify_cleanup(io, r);
	verity_stats_verify_queue_dec(vc);

	verity_dec_pending(io);
}

/* After all I/O is completed successfully for a request, it is queued on the
 * verify workqueue to ensure its integrity prior to returning it to the
 * caller.  There may be multiple workqueue threads - one per logical
 * processor.
 */
static void kverityd_queue_verify(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	/* verify work should never be requeued. */
	io->next_queue = DM_VERITY_NONE;
	REQTRACE("Block %llu+ is being queued for verify (io:%p)",
		 ULL(io->block), io);
	INIT_DELAYED_WORK(&io->work, kverityd_verify);
	/* No delay needed. But if we move over jobs with pending io, then
	 * we could probably delay them here.
	 */
	queue_delayed_work(vc->verify_queue, &io->work, 0);
}

/* Asynchronously called upon the completion of dm-bht I/O.  The status
 * of the operation is passed back to dm-bht and the next steps are
 * decided by verity_dec_pending.
 */
static void kverityd_io_bht_populate_end(struct bio *bio, int error)
{
	struct dm_bht_entry *entry = (struct dm_bht_entry *) bio->bi_private;
	struct dm_verity_io *io = (struct dm_verity_io *) entry->io_context;

	DMDEBUG("kverityd_io_bht_populate_end (io:%p, entry:%p)", io, entry);
	/* Tell the tree to atomically update now that we've populated
	 * the given entry.
	 */
	dm_bht_read_completed(entry, error);

	/* Clean up for reuse when reading data to be checked */
	bio->bi_vcnt = 0;
	bio->bi_io_vec->bv_offset = 0;
	bio->bi_io_vec->bv_len = 0;
	bio->bi_io_vec->bv_page = NULL;
	/* Restore the private data to I/O so the destructor can be shared. */
	bio->bi_private = (void *) io;
	bio_put(bio);

	/* We bail but assume the tree has been marked bad. */
	if (unlikely(error)) {
		DMERR("Failed to read for block %llu (%u)",
		      ULL(io->base_bio->bi_sector), io->base_bio->bi_size);
		io->error = error;
		/* Pass through the error to verity_dec_pending below */
	}
	/* When pending = 0, it will transition to reading real data */
	verity_dec_pending(io);
}

/* Called by dm-bht (via dm_bht_populate), this function provides
 * the message digests to dm-bht that are stored on disk.
 */
static int kverityd_bht_read_callback(void *ctx, sector_t start, u8 *dst,
				      sector_t count,
				      struct dm_bht_entry *entry)
{
	struct dm_verity_io *io = ctx;  /* I/O for this batch */
	struct verity_config *vc;
	struct bio *bio;
	/* Explicitly catches these so we can use a custom bug route */
	VERITY_BUG_ON(!io || !dst || !io->target || !io->target->private);
	VERITY_BUG_ON(!entry);
	VERITY_BUG_ON(count != to_sector(PAGE_SIZE));

	vc = io->target->private;

	/* The I/O context is nested inside the entry so that we don't need one
	 * io context per page read.
	 */
	entry->io_context = ctx;

	/* We should only get page size requests at present. */
	verity_inc_pending(io);
	bio = verity_alloc_bioset(vc, GFP_NOIO, 1);
	if (unlikely(!io->ctx.bio)) {
		DMCRIT("Out of memory at bio_alloc_bioset");
		dm_bht_read_completed(entry, -ENOMEM);
		return -ENOMEM;
	}
	bio->bi_private = (void *) entry;
	bio->bi_idx = 0;
	bio->bi_size = PAGE_SIZE;
	bio->bi_sector = vc->hash_start + start;
	bio->bi_bdev = vc->hash_dev->bdev;
	bio->bi_end_io = kverityd_io_bht_populate_end;
	/* Instead of using NOIDLE, we unplug on intervals */
	bio->bi_rw = (1 << BIO_RW_META);
	/* Only need to free the bio since the page is managed by bht */
	bio->bi_destructor = dm_verity_bio_destructor;
	bio->bi_vcnt = 1;
	bio->bi_io_vec->bv_offset = 0;
	bio->bi_io_vec->bv_len = to_bytes(count);
	/* dst is guaranteed to be a page_pool allocation */
	bio->bi_io_vec->bv_page = virt_to_page(dst);
	/* Track that this I/O is in use.  There should be no risk of the io
	 * being removed prior since this is called synchronously.
	 */
	DMDEBUG("Submitting bht io %p (entry:%p)", io, entry);
	generic_make_request(bio);
	return 0;
}

/* Performs the work of loading in any missing bht hashes. */
static void kverityd_io_bht_populate(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	struct request_queue *r_queue = bdev_get_queue(vc->hash_dev->bdev);
	int populated = 0;
	int io_status = 0;
	sector_t count;

	verity_inc_pending(io);
	/* Submits an io request for each missing block of block hashes.
	 * The last one to return will then enqueue this on the
	 * io workqueue.
	 */
	REQTRACE("populating %llu starting at block %llu (io:%p)",
		 ULL(io->count), ULL(io->block), io);
	for (count = 0; count < io->count; ++count) {
		unsigned int block = (unsigned int)(io->block + count);
		/* Check for truncation. */
		VERITY_BUG_ON((sector_t)(block) < io->block);
		/* populate for each block */
		DMDEBUG("Calling dm_bht_populate for %u (io:%p)", block, io);
		populated = dm_bht_populate(&vc->bht, io, block);
		if (populated < 0) {
			DMCRIT("dm_bht_populate error for block %u (io:%p): %d",
			       block, io, populated);
			/* verity_dec_pending will handle the error case. */
			io->error = -EPERM;
			break;
		}
		/* Accrue view of all I/O state for the full request */
		io_status |= populated;

		/* If this io has outstanding requests, unplug the io queue */
		if (populated & DM_BHT_ENTRY_REQUESTED) {
			blk_unplug(r_queue);
			/* If the bht is reading ahead, a cond_resched may be
			 * needed to break contention.  msleep_interruptible(1)
			 * is an alternative option.
			 */
			if (bht_readahead)
				cond_resched();
		}
	}
	REQTRACE("Block %llu+ initiated %d requests (io: %p)",
		 ULL(io->block), atomic_read(&io->pending) - 1, io);

	/* TODO(wad) clean up the return values from maybe_read_entries */
	/* If we return verified explicitly, then later we could do IO_REMAP
	 * instead of resending to the verify queue.
	 */
	io->next_queue = DM_VERITY_VERIFY;
	if (io_status == 0 || io_status == DM_BHT_ENTRY_READY) {
		/* The whole request is ready. Make sure we can transition. */
		DMDEBUG("io ready to be bumped %p", io);
	}

	if (io_status & DM_BHT_ENTRY_REQUESTED) {
		/* If no data is pending another I/O request, this io
		 * will get bounced on the next queue when the last async call
		 * returns.
		 */
		DMDEBUG("io has outstanding requests %p");
	}

	/* Some I/O is pending outside of this request. */
	if (io_status & DM_BHT_ENTRY_PENDING) {
		/* PENDING will cause a BHT requeue as delayed work */
		/* TODO(wad) add a callback to dm-bht for pending_cb. Then for
		 * each entry, the io could be delayed heavily until the end
		 * read cb requeue them. (entries could be added to the
		 * stored I/O context but races could be a challenge.
		 */
		DMDEBUG("io is pending %p");
		io->next_queue = DM_VERITY_IO;
	}

	/* If I/O requests were issues on behalf of populate, then the last
	 * request will result in a requeue.  If all data was pending from
	 * other requests, this will be requeued now.
	 */
	verity_dec_pending(io);
}

/* Asynchronously called upon the completion of I/O issued
 * from kverityd_src_io_read. verity_dec_pending() acts as
 * the scheduler/flow manager.
 */
static void kverityd_src_io_read_end(struct bio *clone, int error)
{
	struct dm_verity_io *io = clone->bi_private;
	struct verity_config *vc = io->target->private;
	/* struct verity_config *vc = io->target->private; */

	DMDEBUG("Padded I/O completed");
	if (unlikely(!bio_flagged(clone, BIO_UPTODATE) && !error))
		error = -EIO;

	if (unlikely(error)) {
		DMERR("Error occurred: %d (%llu, %u)",
			error, ULL(clone->bi_sector), clone->bi_size);
		io->error = error;
		/* verity_dec_pending will pick up the error, but it won't
		 * free the leading/trailing pages for us.
		 */
		verity_free_buffer_pages(vc, io, io->ctx.bio);
		/* drop the padded bio since we'll never use it now. */
		bio_put(io->ctx.bio);
	}

	/* Release the clone which just avoids the block layer from
	 * leaving offsets, etc in unexpected states.
	 */
	bio_put(clone);

	verity_dec_pending(io);
	DMDEBUG("all data has been loaded from the data device");
}

/* If not yet underway, an I/O request will be issued to the vc->dev
 * device for the data needed.  It is padded to the minimum block
 * size, aligned to that size, and cloned to avoid unexpected changes
 * to the original bio struct.
 */
static void kverityd_src_io_read(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	struct bio *base_bio = io->base_bio;
	sector_t bio_start = vc->start + io->sector;
	unsigned int leading_bytes = 0;
	unsigned int trailing_bytes = 0;
	unsigned int bio_size = base_bio->bi_size;
	unsigned int vecs_needed = bio_segments(base_bio);
	struct bio_vec *cur_bvec = NULL;
	struct bio *clone = NULL;

	VERITY_BUG_ON(!io);

	/* If bio is non-NULL, then the data is already read. Could also check
	 * BIO_UPTODATE, but it doesn't seem needed.
	 */
	if (io->ctx.bio) {
		DMDEBUG("io_read called with existing bio. bailing: %p", io);
		return;
	}
	DMDEBUG("kverity_io_read started");

	verity_inc_pending(io);
	/* We duplicate the bio here for two reasons:
	 * 1. The block layer may modify the bvec array
	 * 2. We may need to pad to BLOCK_SIZE
	 * First, we have to determine if we need more segments than are
	 * currently in use.
	 */
	verity_align_request(vc, &bio_start, &bio_size);
	/* Number of bytes alignment added to the start */
	leading_bytes = to_bytes((vc->start + io->sector) - bio_start);
	/* ... to the end of the original bio. */
	trailing_bytes = (bio_size - leading_bytes) - base_bio->bi_size;

	/* Additions are page aligned so page sized vectors can be padded in */
	vecs_needed += PAGE_ALIGN(leading_bytes) >> PAGE_SHIFT;
	vecs_needed += PAGE_ALIGN(trailing_bytes) >> PAGE_SHIFT;

	DMDEBUG("allocating bioset for padding (%u)", vecs_needed);
	/* Allocate the bioset for the duplicate plus padding */
	if (vecs_needed == bio_segments(base_bio)) {
		/* No padding is needed so we can just verify using the
		 * original bio.
		 */
		DMDEBUG("No padding needed!");
		io->ctx.bio = base_bio;
	} else {
		ALLOCTRACE("bioset for io %p, sector %llu",
			   io, ULL(bio_start));
		io->ctx.bio = verity_alloc_bioset(vc, GFP_NOIO, vecs_needed);
		if (unlikely(io->ctx.bio == NULL)) {
			DMCRIT("Failed to allocate padded bioset");
			io->error = -ENOMEM;
			verity_dec_pending(io);
			return;
		}

		DMDEBUG("clone init");
		clone_init(io, io->ctx.bio, vecs_needed, bio_size, bio_start);
		/* Now we need to copy over the iovecs, but we need to make
		 * sure to offset if we realigned the request.
		 */
		cur_bvec = io->ctx.bio->bi_io_vec;

		DMDEBUG("Populating padded bioset (%u %u)",
			leading_bytes, trailing_bytes);
		DMDEBUG("leading_bytes == %u", leading_bytes);
		cur_bvec = populate_bio_vec(cur_bvec, vc->page_pool,
					    leading_bytes, &io->leading_pages);
		if (unlikely(cur_bvec == NULL)) {
				io->error = -ENOMEM;
				verity_free_buffer_pages(vc, io, io->ctx.bio);
				bio_put(io->ctx.bio);
				verity_dec_pending(io);
				return;
		}
		/* Now we should be aligned to copy the bio_vecs in place */
		DMDEBUG("copying original bvecs");
		memcpy(cur_bvec, bio_iovec(base_bio),
		       sizeof(struct bio_vec) * bio_segments(base_bio));
		cur_bvec += bio_segments(base_bio);
		/* Handle trailing vecs */
		DMDEBUG("trailing_bytes == %u", trailing_bytes);
		cur_bvec = populate_bio_vec(cur_bvec, vc->page_pool,
					    trailing_bytes,
					    &io->trailing_pages);
		if (unlikely(cur_bvec == NULL)) {
			io->error = -ENOMEM;
			verity_free_buffer_pages(vc, io, io->ctx.bio);
			bio_put(io->ctx.bio);
			verity_dec_pending(io);
			return;
		}
	}

	/* Now clone the padded request */
	DMDEBUG("Creating clone of the padded request");
	ALLOCTRACE("clone for io %p, sector %llu",
		   io, ULL(bio_start));
	clone = verity_alloc_bioset(vc, GFP_NOIO, bio_segments(io->ctx.bio));
	if (unlikely(!clone)) {
		io->error = -ENOMEM;
		/* Clean up */
		verity_free_buffer_pages(vc, io, io->ctx.bio);
		if (io->ctx.bio != base_bio)
			bio_put(io->ctx.bio);
		verity_dec_pending(io);
		return;
	}

	clone_init(io, clone, bio_segments(io->ctx.bio), io->ctx.bio->bi_size,
		   bio_start);
	DMDEBUG("Populating clone of the padded request");
	memcpy(clone->bi_io_vec, bio_iovec(io->ctx.bio),
	       sizeof(struct bio_vec) * clone->bi_vcnt);

	/* Submit to the block device */
	DMDEBUG("Submitting padded bio (%u became %u)",
		bio_sectors(base_bio), bio_sectors(clone));
	/* XXX: check queue_max_hw_sectors(bdev_get_queue(clone->bi_bdev)); */
	generic_make_request(clone);
}

/* kverityd_io services the I/O workqueue. For each pass through
 * the I/O workqueue, a call to populate both the origin drive
 * data and the hash tree data is made.
 */
static void kverityd_io(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work,
						  work);
	struct dm_verity_io *io = container_of(dwork, struct dm_verity_io,
					       work);
	VERITY_BUG_ON(!io->base_bio);

	if (bio_data_dir(io->base_bio) == WRITE) {
		/* TODO(wad) do something smarter when writes are seen */
		printk(KERN_WARNING
		       "Unexpected write bio received in kverityd_io");
		io->error = -EIO;
		return;
	}

	/* Issue requests asynchronously. */
	verity_inc_pending(io);
	kverityd_src_io_read(io);  /* never updates next_queue */
	kverityd_io_bht_populate(io);   /* manage next_queue eligibility */
	verity_dec_pending(io);
}

/* All incoming requests are queued on the I/O workqueue at least once to
 * acquire both the data from the real device (vc->dev) and any data from the
 * hash tree device (vc->hash_dev) needed to verify the integrity of the data.
 * There may be multiple I/O workqueues - one per logical processor.
 */
static void kverityd_queue_io(struct dm_verity_io *io, bool delayed)
{
	struct verity_config *vc = io->target->private;
	unsigned long delay = 0;  /* jiffies */
	/* Send all requests through one call to dm_bht_populate on the
	 * queue to ensure that all blocks are accounted for before
	 * proceeding on to verification.
	 */
	INIT_DELAYED_WORK(&io->work, kverityd_io);
	/* If this context is dependent on work from another context, we just
	 * requeue with a delay.  Later we could bounce this work to the verify
	 * queue and have it wait there. TODO(wad)
	 */
	if (delayed) {
		delay = HZ / 10;
		REQTRACE("block %llu+ is being delayed %lu jiffies (io:%p)",
			 ULL(io->block), delay, io);
	}
	queue_delayed_work(vc->io_queue, &io->work, delay);
}

/* Paired with verity_dec_pending, the pending value in the io dictate the
 * lifetime of a request and when it is ready to be processed on the
 * workqueues.
 */
static void verity_inc_pending(struct dm_verity_io *io)
{
	atomic_inc(&io->pending);
}

/* Block-level requests start here. */
static int verity_map(struct dm_target *ti, struct bio *bio,
		      union map_info *map_context) {
	struct dm_verity_io *io;
	struct verity_config *vc;
	struct request_queue *r_queue;

	if (unlikely(!ti)) {
		DMERR("dm_target was NULL");
		return -EIO;
	}

	vc = ti->private;
	r_queue = bdev_get_queue(vc->dev->bdev);

	/* Barriers are passed through. Since the device is read-only,
	 * barrier use seems unlikely but being data-free shouldn't be blocked
	 * here.
	 */
	if (unlikely(bio_empty_barrier(bio))) {
		bio->bi_bdev = vc->dev->bdev;
		return DM_MAPIO_REMAPPED;
	}

	/* Trace incoming bios */
	REQTRACE("Got a %s for %llu, %u bytes)",
		(bio_rw(bio) == WRITE ? "WRITE" :
		(bio_rw(bio) == READ ? "READ" : "READA")),
		ULL(bio->bi_sector), bio->bi_size);

	verity_stats_total_requests_inc(vc);

	if (bio_data_dir(bio) == WRITE) {
		/* If we silently drop writes, then the VFS layer will cache
		 * the write and persist it in memory. While it doesn't change
		 * the underlying storage, it still may be contrary to the
		 * behavior expected by a verified, read-only device.
		 */
		DMWARN_LIMIT("write request received. rejecting with -EIO.");
		verity_error(vc, NULL, -EIO);
		/* bio_endio(bio, -EIO); */
		return -EIO;
	} else {
		/* Queue up the request to be verified */
		io = verity_io_alloc(ti, bio, bio->bi_sector - ti->begin);
		if (!io) {
			DMERR_LIMIT("Failed to allocate and init IO data");
			return DM_MAPIO_REQUEUE;
		}
		verity_stats_io_queue_inc(vc);
		kverityd_queue_io(io, false);
	}

	return DM_MAPIO_SUBMITTED;
}

/*
 * Non-block interfaces and device-mapper specific code
 */

/*
 * Construct an verified mapping:
 *  <device_to_verify> <device_with_hash_data>
 *  <page_aligned_offset_to_hash_data>
 *  <tree_depth> <hash_alg> <hash-of-bundle-hashes> <errbehavior: optional>
 * E.g.,
 *   /dev/sda2 /dev/sda3 0 2 sha256
 *   f08aa4a3695290c569eb1b0ac032ae1040150afb527abbeb0a3da33d82fb2c6e
 *
 * TODO(wad):
 * - Add stats: num_requeues, num_ios, etc with proc ibnterface
 * - Boot time addition
 * - Track block verification to free block_hashes if memory use is a concern
 * Testing needed:
 * - Regular slub_debug tracing (on checkins)
 * - Improper block hash padding
 * - Improper bundle padding
 * - Improper hash layout
 * - Missing padding at end of device
 * - Improperly sized underlying devices
 * - Out of memory conditions (make sure this isn't too flaky under high load!)
 * - Incorrect superhash
 * - Incorrect block hashes
 * - Incorrect bundle hashes
 * - Boot-up read speed; sustained read speeds
 */
static int verity_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct verity_config *vc;
	int cpu = 0;
	int ret = 0;
	int depth;
	unsigned long long tmpull = 0;
	sector_t blocks;

	/* Support expanding after the root hash for optional args */
	if (argc < 6) {
		ti->error = "Not enough arguments supplied";
		return -EINVAL;
	}

	/* The device mapper device should be setup read-only */
	if ((dm_table_get_mode(ti->table) & ~FMODE_READ) != 0) {
		ti->error = "Must be created readonly.";
		return -EINVAL;
	}

	ALLOCTRACE("verity_config");
	vc = kzalloc(sizeof(*vc), GFP_KERNEL);
	if (!vc) {
		/* TODO(wad) if this is called from the setup helper, then we
		 * catch these errors and do a CrOS specific thing. if not, we
		 * need to have this call the error handler.
		 */
		return -EINVAL;
	}


	/* arg3: blocks in a bundle */
	if (sscanf(argv[3], "%u", &depth) != 1 ||
	    depth <= 0) {
		ti->error =
			"Zero or negative depth supplied";
		goto bad_depth;
	}
	/* dm-bht block size is HARD CODED to PAGE_SIZE right now. */
	/* Calculate the blocks from the given device size */
	blocks = to_bytes(ti->len) >> PAGE_SHIFT;
	if (dm_bht_create(&vc->bht, (unsigned int)depth, blocks, argv[4])) {
		DMERR("failed to create required bht");
		goto bad_bht;
	}
	if (dm_bht_set_root_hexdigest(&vc->bht, argv[5])) {
		DMERR("root hexdigest error");
		goto bad_root_hexdigest;
	}
	dm_bht_set_read_cb(&vc->bht, kverityd_bht_read_callback);
	dm_bht_set_entry_readahead(&vc->bht, bht_readahead);

	/* arg0: device to verify */
	vc->start = 0;  /* TODO: should this support a starting offset? */
	/* We only ever grab the device in read-only mode. */
	ret = verity_get_device(ti, argv[0], vc->start, ti->len, &vc->dev);
	if (ret) {
		DMERR("Failed to acquire device '%s': %d", argv[0], ret);
		ti->error = "Device lookup failed";
		goto bad_verity_dev;
	}

	if (PAGE_ALIGN(vc->start) != vc->start ||
	    PAGE_ALIGN(to_bytes(ti->len)) != to_bytes(ti->len)) {
		ti->error = "Device must be PAGE_SIZE divisble/aligned";
		goto bad_hash_start;
	}

	/* arg2: offset to hash on the hash device */
	if (sscanf(argv[2], "%llu", &tmpull) != 1) {
		ti->error =
			"Invalid hash_start supplied";
		goto bad_hash_start;
	}
	vc->hash_start = (sector_t)(tmpull);

	/* arg1: device with hashes.
	 * Note, arg1 == arg0 is okay as long as the size of
	 *       ti->len passed to device mapper does not include
	 *       the hashes.
	 */
	if (verity_get_device(ti, argv[1], vc->hash_start,
			      dm_bht_sectors(&vc->bht), &vc->hash_dev)) {
		ti->error = "Hash device lookup failed";
		goto bad_hash_dev;
	}

	/* We leave the validity on the hash device open until the
	 * next arg.  Then we go ahead and try to read in all the bundle
	 * hashes which live after the block hashes.  If it fails, then
	 * the hash offset was wrong.
	 */


	/* arg4: cryptographic digest algorithm */
	if (snprintf(vc->hash_alg, CRYPTO_MAX_ALG_NAME, "%s", argv[4]) >=
	    CRYPTO_MAX_ALG_NAME) {
		ti->error = "Hash algorithm name is too long";
		goto bad_hash;
	}
	/* Allocate enough crypto contexts to be able to perform verifies
	 * on all available CPUs.
	 */
	ALLOCTRACE("hash_desc array");
	vc->hash = (struct hash_desc *)
		      kcalloc(nr_cpu_ids, sizeof(struct hash_desc), GFP_KERNEL);
	if (!vc->hash) {
		DMERR("failed to allocate crypto hash contexts");
		return -ENOMEM;
	}

	/* Setup the hash first. Its length determines much of the bht layout */
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu) {
		ALLOCTRACE("hash_tfm (per-cpu)");
		vc->hash[cpu].tfm = crypto_alloc_hash(vc->hash_alg, 0, 0);
		if (IS_ERR(vc->hash[cpu].tfm)) {
			DMERR("failed to allocate crypto hash '%s'",
			      vc->hash_alg);
			vc->hash[cpu].tfm = NULL;
			goto bad_hash_alg;
		}
	}

	/* arg6: override with optional device-specific error behavior */
	if (argc >= 7) {
		strncpy(vc->error_behavior, argv[7],
			sizeof(vc->error_behavior));
	} else {
		/* Inherit the current global default. */
		strncpy(vc->error_behavior, error_behavior,
			sizeof(vc->error_behavior));
	}
	if (!verity_error_behavior_prepare(vc->error_behavior)) {
		ti->error =
			"Bad error_behavior supplied";
		goto bad_err_behavior;
	}


	/* TODO: Maybe issues a request on the io queue for block 0? */

	/* Argument processing is done, setup operational data */
	/* Pool for dm_verity_io objects */
	ALLOCTRACE("slab pool for io objects");
	vc->io_pool = mempool_create_slab_pool(MIN_IOS, _verity_io_pool);
	if (!vc->io_pool) {
		ti->error = "Cannot allocate verity io mempool";
		goto bad_slab_pool;
	}

	/* Used to allocate pages for padding requests to PAGE_SIZE */
	ALLOCTRACE("page pool for request padding");
	vc->page_pool = mempool_create_page_pool(MIN_POOL_PAGES, 0);
	if (!vc->page_pool) {
		ti->error = "Cannot allocate page mempool";
		goto bad_page_pool;
	}

	/* Allocate the bioset used for request padding */
	/* TODO(wad) allocate a separate bioset for the first verify maybe */
	ALLOCTRACE("bioset for I/O reqs");
	vc->bs = bioset_create(MIN_BIOS, 0);
	if (!vc->bs) {
		ti->error = "Cannot allocate verity bioset";
		goto bad_bs;
	}

	/* Only one thread for the workqueue to keep the memory allocation
	 * sane.  Requests will be submitted asynchronously. blk_unplug() will
	 * be called at the end of each dm_populate call so that the async
	 * requests are batched per workqueue job.
	 */
	vc->io_queue = create_workqueue("kverityd_io");
	if (!vc->io_queue) {
		ti->error = "Couldn't create kverityd io queue";
		goto bad_io_queue;
	}

	vc->verify_queue = create_workqueue("kverityd");
	if (!vc->verify_queue) {
		ti->error = "Couldn't create kverityd queue";
		goto bad_verify_queue;
	}

	ti->num_flush_requests = 1;
	ti->private = vc;

	/* TODO(wad) add device and hash device names */
	{
		char hashdev[BDEVNAME_SIZE], vdev[BDEVNAME_SIZE];
		bdevname(vc->hash_dev->bdev, hashdev);
		bdevname(vc->dev->bdev, vdev);
		DMINFO("dev:%s hash:%s [sectors:%llu blocks:%llu]", vdev,
		       hashdev, ULL(dm_bht_sectors(&vc->bht)), ULL(blocks));
	}
	return 0;

bad_verify_queue:
	destroy_workqueue(vc->io_queue);
bad_io_queue:
	bioset_free(vc->bs);
bad_bs:
	mempool_destroy(vc->page_pool);
bad_page_pool:
	mempool_destroy(vc->io_pool);
bad_slab_pool:
bad_err_behavior:
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
		if (vc->hash[cpu].tfm)
			crypto_free_hash(vc->hash[cpu].tfm);
bad_hash_alg:
bad_hash:
	kfree(vc->hash);
	dm_put_device(ti, vc->hash_dev);
bad_hash_dev:
bad_hash_start:
	dm_put_device(ti, vc->dev);
bad_depth:
bad_bht:
bad_root_hexdigest:
bad_verity_dev:
	kfree(vc);   /* hash is not secret so no need to zero */
	return -EINVAL;
}

static void verity_dtr(struct dm_target *ti)
{
	struct verity_config *vc = (struct verity_config *) ti->private;
	int cpu;

	DMDEBUG("Destroying io_queue");
	destroy_workqueue(vc->io_queue);
	DMDEBUG("Destroying verify_queue");
	destroy_workqueue(vc->verify_queue);

	DMDEBUG("Destroying bs");
	bioset_free(vc->bs);
	DMDEBUG("Destroying page_pool");
	mempool_destroy(vc->page_pool);
	DMDEBUG("Destroying io_pool");
	mempool_destroy(vc->io_pool);
	DMDEBUG("Destroying crypto hash");
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
		if (vc->hash[cpu].tfm)
			crypto_free_hash(vc->hash[cpu].tfm);
	kfree(vc->hash);

	DMDEBUG("Destroying block hash tree");
	dm_bht_destroy(&vc->bht);

	DMDEBUG("Putting hash_dev");
	dm_put_device(ti, vc->hash_dev);

	DMDEBUG("Putting dev");
	dm_put_device(ti, vc->dev);
	DMDEBUG("Destroying config");
	kfree(vc);
}

static int verity_status(struct dm_target *ti, status_type_t type,
			char *result, unsigned int maxlen) {
	struct verity_config *vc = (struct verity_config *) ti->private;
	unsigned int sz = 0;
	char hashdev[BDEVNAME_SIZE], vdev[BDEVNAME_SIZE];
	u8 hexdigest[VERITY_MAX_DIGEST_SIZE * 2 + 1] = { 0 };

	dm_bht_root_hexdigest(&vc->bht, hexdigest, sizeof(hexdigest));

	switch (type) {
	case STATUSTYPE_INFO:
		DMEMIT("%u %u %u %u %llu",
		       vc->stats.io_queue,
		       vc->stats.verify_queue,
		       vc->stats.average_requeues,
		       vc->stats.total_requeues,
		       vc->stats.total_requests);
		break;

	case STATUSTYPE_TABLE:
		bdevname(vc->hash_dev->bdev, hashdev);
		bdevname(vc->dev->bdev, vdev);
		DMEMIT("/dev/%s /dev/%s %llu %u %s %s",
			vdev,
			hashdev,
			ULL(vc->hash_start),
			vc->bht.depth,
			vc->hash_alg,
			hexdigest);
		break;
	}
	return 0;
}

static int verity_merge(struct dm_target *ti, struct bvec_merge_data *bvm,
		       struct bio_vec *biovec, int max_size)
{
	struct verity_config *vc = ti->private;
	struct request_queue *q = bdev_get_queue(vc->dev->bdev);

	if (!q->merge_bvec_fn)
		return max_size;

	bvm->bi_bdev = vc->dev->bdev;
	bvm->bi_sector = vc->start + bvm->bi_sector - ti->begin;

	/* Optionally, this could just return 0 to stick to single pages. */
	return min(max_size, q->merge_bvec_fn(q, bvm, biovec));
}

static int verity_iterate_devices(struct dm_target *ti,
				 iterate_devices_callout_fn fn, void *data)
{
	struct verity_config *vc = ti->private;

	return fn(ti, vc->dev, vc->start, ti->len, data);
}

static void verity_io_hints(struct dm_target *ti,
			    struct queue_limits *limits)
{
	/*
	 * Set this to the vc->dev value:
	 blk_limits_io_min(limits, chunk_size); */
	blk_limits_io_opt(limits, PAGE_SIZE);
}

static struct target_type verity_target = {
	.name   = "verity",
	.version = {0, 1, 0},
	.module = THIS_MODULE,
	.ctr    = verity_ctr,
	.dtr    = verity_dtr,
	.map    = verity_map,
	.merge  = verity_merge,
	.status = verity_status,
	.iterate_devices = verity_iterate_devices,
	.io_hints = verity_io_hints,
};

static int __init dm_verity_init(void)
{
	int r;

	_verity_io_pool = KMEM_CACHE(dm_verity_io, 0);
	if (!_verity_io_pool)
		return -ENOMEM;

	r = dm_register_target(&verity_target);
	if (r < 0) {
		DMERR("register failed %d", r);
		kmem_cache_destroy(_verity_io_pool);
		/* TODO(wad): add optional recovery bail here. */
	} else {
		DMINFO("dm-verity registered");
		/* TODO(wad): Add root setup to initcalls workqueue here */
	}
	return r;
}

static void __exit dm_verity_exit(void)
{
	dm_unregister_target(&verity_target);
	kmem_cache_destroy(_verity_io_pool);
}

module_init(dm_verity_init);
module_exit(dm_verity_exit);

MODULE_AUTHOR("The Chromium OS Authors <chromium-os-dev@chromium.org>");
MODULE_DESCRIPTION(DM_NAME " target for transparent disk integrity checking");
MODULE_LICENSE("GPL");
