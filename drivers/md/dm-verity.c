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
#include <linux/bio.h>
#include <linux/blkdev.h>
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
#include <asm/atomic.h>
#include <asm/page.h>

/* #define CONFIG_DM_DEBUG 1 */
#define CONFIG_DM_VERITY_TRACE 1
#include <linux/device-mapper.h>
#include <linux/dm-bht.h>

#include "dm-verity.h"

#define DM_MSG_PREFIX "verity"

/* Supports up to 512-bit digests */
#define VERITY_MAX_DIGEST_SIZE 64

/* TODO(wad) make both of these report the error line/file to a
 *           verity_bug function.
 */
#define VERITY_BUG(msg...) BUG()
#define VERITY_BUG_ON(cond, msg...) BUG_ON(cond)

/* Helper for printing sector_t */
#define ULL(x) ((unsigned long long)(x))

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

/* MUST be true: SECTOR_SHIFT <= VERITY_BLOCK_SHIFT <= PAGE_SHIFT */
#define VERITY_BLOCK_SIZE 4096
#define VERITY_BLOCK_SHIFT 12

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

/* Provide a lightweight means of specifying the global default for
 * error behavior: eio, reboot, or none
 * Legacy support for 0 = eio, 1 = reboot/panic, 2 = none, 3 = notify.
 * This is matched to the enum in dm-verity.h.
 */
static const char *allowed_error_behaviors[] = { "eio", "panic", "none",
						 "notify", NULL };
static char *error_behavior = "eio";
module_param(error_behavior, charp, 0644);
MODULE_PARM_DESC(error_behavior, "Behavior on error "
				 "(eio, panic, none, notify)");

/* Controls whether verity_get_device will wait forever for a device. */
static int dev_wait;
module_param(dev_wait, bool, 0444);
MODULE_PARM_DESC(dev_wait, "Wait forever for a backing device");

/* Used for tracking pending bios as well as for exporting information via
 * STATUSTYPE_INFO.
 */
struct verity_stats {
	unsigned int io_queue;
	unsigned int verify_queue;
	unsigned int average_requeues;
	unsigned int total_requeues;
	unsigned long long total_requests;
};

/* per-requested-bio private data */
enum verity_io_flags {
	VERITY_IOFLAGS_CLONED = 0x1,	/* original bio has been cloned */
};

struct dm_verity_io {
	struct dm_target *target;
	struct bio *bio;
	struct delayed_work work;
	unsigned int flags;

	int error;
	atomic_t pending;

	sector_t sector;  /* converted to target sector */
	u64 block;  /* aligned block index */
	u64 count;  /* aligned count in blocks */
};

struct verity_config {
	struct dm_dev *dev;
	sector_t start;
	sector_t size;

	struct dm_dev *hash_dev;
	sector_t hash_start;

	struct dm_bht bht;

	/* Pool required for io contexts */
	mempool_t *io_pool;
	/* Pool and bios required for making sure that backing device reads are
	 * in PAGE_SIZE increments.
	 */
	struct bio_set *bs;

	char hash_alg[CRYPTO_MAX_ALG_NAME];

	int error_behavior;

	struct verity_stats stats;
};

static struct kmem_cache *_verity_io_pool;
static struct workqueue_struct *kveritydq, *kverityd_ioq;

static void kverityd_verify(struct work_struct *work);
static void kverityd_io(struct work_struct *work);
static void kverityd_io_bht_populate(struct dm_verity_io *io);
static void kverityd_io_bht_populate_end(struct bio *, int error);

static BLOCKING_NOTIFIER_HEAD(verity_error_notifier);

/*-----------------------------------------------
 * Statistic tracking functions
 *-----------------------------------------------*/

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
 * Exported interfaces
 *-----------------------------------------------*/

int dm_verity_register_error_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&verity_error_notifier, nb);
}
EXPORT_SYMBOL_GPL(dm_verity_register_error_notifier);

int dm_verity_unregister_error_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&verity_error_notifier, nb);
}
EXPORT_SYMBOL_GPL(dm_verity_unregister_error_notifier);

/*-----------------------------------------------
 * Allocation and utility functions
 *-----------------------------------------------*/

static void kverityd_src_io_read_end(struct bio *clone, int error);

/* Shared destructor for all internal bios */
static void dm_verity_bio_destructor(struct bio *bio)
{
	struct dm_verity_io *io = bio->bi_private;
	struct verity_config *vc = io->target->private;
	bio_free(bio, vc->bs);
}

struct bio *verity_alloc_bioset(struct verity_config *vc, gfp_t gfp_mask,
				int nr_iovecs)
{
	return bio_alloc_bioset(gfp_mask, nr_iovecs, vc->bs);
}

static struct dm_verity_io *verity_io_alloc(struct dm_target *ti,
					    struct bio *bio, sector_t sector)
{
	struct verity_config *vc = ti->private;
	struct dm_verity_io *io;

	ALLOCTRACE("dm_verity_io for sector %llu", ULL(sector));
	io = mempool_alloc(vc->io_pool, GFP_NOIO);
	if (unlikely(!io))
		return NULL;
	io->flags = 0;
	io->target = ti;
	io->bio = bio;
	io->sector = sector;
	io->error = 0;

	/* Adjust the sector by the virtual starting sector */
	io->block = (to_bytes(sector)) >> VERITY_BLOCK_SHIFT;
	io->count = bio->bi_size >> VERITY_BLOCK_SHIFT;

	DMDEBUG("io_alloc for %llu blocks starting at %llu",
		ULL(io->count), ULL(io->block));

	atomic_set(&io->pending, 0);

	return io;
}

static struct bio *verity_bio_clone(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	struct bio *bio = io->bio;
	struct bio *clone = verity_alloc_bioset(vc, GFP_NOIO, bio->bi_max_vecs);

	if (!clone)
		return NULL;

	__bio_clone(clone, bio);
	clone->bi_private = io;
	clone->bi_end_io  = kverityd_src_io_read_end;
	clone->bi_bdev    = vc->dev->bdev;
	clone->bi_sector  = vc->start + io->sector;
	clone->bi_destructor = dm_verity_bio_destructor;

	return clone;
}

/* If the request is not successful, this handler takes action.
 * TODO make this call a registered handler.
 */
static void verity_error(struct verity_config *vc, struct dm_verity_io *io,
			 int error)
{
	const char *message;
	int error_behavior = DM_VERITY_ERROR_BEHAVIOR_PANIC;
	dev_t devt = 0;
	u64 block = ~0;
	int transient = 1;
	struct dm_verity_error_state error_state;

	if (vc) {
		devt = vc->dev->bdev->bd_dev;
		error_behavior = vc->error_behavior;
	}

	if (io) {
		io->error = -EIO;
		block = io->block;
	}

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
		/* Image is bad. */
		transient = 0;
		break;
	case -EPERM:
		message = "hash tree population failure";
		/* Should be dm-bht specific errors */
		transient = 0;
		break;
	case -EINVAL:
		message = "unexpected missing/invalid data";
		/* The device was configured incorrectly - fallback. */
		transient = 0;
		break;
	default:
		/* Other errors can be passed through as IO errors */
		message = "unknown or I/O error";
		return;
	}

	DMERR_LIMIT("verification failure occurred: %s", message);

	if (error_behavior == DM_VERITY_ERROR_BEHAVIOR_NOTIFY) {
		error_state.code = error;
		error_state.transient = transient;
		error_state.block = block;
		error_state.message = message;
		error_state.dev_start = vc->start;
		error_state.dev_len = vc->size;
		error_state.dev = vc->dev->bdev;
		error_state.hash_dev_start = vc->hash_start;
		error_state.hash_dev_len = dm_bht_sectors(&vc->bht);
		error_state.hash_dev = vc->hash_dev->bdev;

		/* Set default fallthrough behavior. */
		error_state.behavior = DM_VERITY_ERROR_BEHAVIOR_PANIC;
		error_behavior = DM_VERITY_ERROR_BEHAVIOR_PANIC;

		if (!blocking_notifier_call_chain(
		    &verity_error_notifier, transient, &error_state)) {
			error_behavior = error_state.behavior;
		}
	}

	switch (error_behavior) {
	case DM_VERITY_ERROR_BEHAVIOR_EIO:
		break;
	case DM_VERITY_ERROR_BEHAVIOR_NONE:
		if (error != -EIO && io)
			io->error = 0;
		break;
	default:
		goto do_panic;
	}
	return;

do_panic:
	panic("dm-verity failure: "
	      "device:%u:%u error:%d block:%llu message:%s",
	      MAJOR(devt), MINOR(devt), error, ULL(block), message);
}

/**
 * verity_parse_error_behavior - parse a behavior charp to the enum
 * @behavior:	NUL-terminated char array
 *
 * Checks if the behavior is valid either as text or as an index digit
 * and returns the proper enum value or -1 on error.
 */
static int verity_parse_error_behavior(const char *behavior)
{
	const char **allowed = allowed_error_behaviors;
	char index = '0';

	for (; *allowed; allowed++, index++)
		if (!strcmp(*allowed, behavior) || behavior[0] == index)
			break;

	if (!*allowed)
		return -1;

	/* Convert to the integer index matching the enum. */
	return allowed - allowed_error_behaviors;
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
	if (!dm_get_device(ti, devt_buf, dm_table_get_mode(ti->table), dm_dev))
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
		if (!dm_get_device(ti, devname,
				   dm_table_get_mode(ti->table), dm_dev))
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

static void verity_return_bio_to_caller(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;

	if (io->error)
		verity_error(vc, io, io->error);

	bio_endio(io->bio, io->error);
	mempool_free(io, vc->io_pool);
}

/* Check for any missing bht hashes. */
static bool verity_is_bht_populated(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	u64 block;

	for (block = io->block; block < io->block + io->count; ++block)
		if (!dm_bht_is_populated(&vc->bht, block))
			return false;

	return true;
}

/* verity_dec_pending manages the lifetime of all dm_verity_io structs.
 * Non-bug error handling is centralized through this interface and
 * all passage from workqueue to workqueue.
 */
static void verity_dec_pending(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	VERITY_BUG_ON(!io, "NULL argument");

	DMDEBUG("dec pending %p: %d--", io, atomic_read(&io->pending));

	if (!atomic_dec_and_test(&io->pending))
		goto done;

	if (unlikely(io->error))
		goto io_error;

	/* I/Os that were pending may now be ready */
	if (verity_is_bht_populated(io)) {
		verity_stats_io_queue_dec(vc);
		verity_stats_verify_queue_inc(vc);
		INIT_DELAYED_WORK(&io->work, kverityd_verify);
		queue_delayed_work(kveritydq, &io->work, 0);
		REQTRACE("Block %llu+ is being queued for verify (io:%p)",
			 ULL(io->block), io);
	} else {
		INIT_DELAYED_WORK(&io->work, kverityd_io);
		queue_delayed_work(kverityd_ioq, &io->work, HZ/10);
		verity_stats_total_requeues_inc(vc);
		REQTRACE("Block %llu+ is being requeued for io (io:%p)",
			 ULL(io->block), io);
	}

done:
	return;

io_error:
	verity_return_bio_to_caller(io);
}

/* Walks the data set and computes the hash of the data read from the
 * untrusted source device.  The computed hash is then passed to dm-bht
 * for verification.
 */
static int verity_verify(struct verity_config *vc,
			 struct bio *bio)
{
	unsigned int idx;
	u64 block;
	int r;

	VERITY_BUG_ON(bio == NULL);

	block = to_bytes(bio->bi_sector) >> VERITY_BLOCK_SHIFT;

	for (idx = bio->bi_idx; idx < bio->bi_vcnt; idx++) {
		struct bio_vec *bv = bio_iovec_idx(bio, idx);

		VERITY_BUG_ON(bv->bv_offset % VERITY_BLOCK_SIZE);
		VERITY_BUG_ON(bv->bv_len % VERITY_BLOCK_SIZE);

		DMDEBUG("Updating hash for block %llu", ULL(block));

		/* TODO(msb) handle case where multiple blocks fit in a page */
		r = dm_bht_verify_block(&vc->bht, block,
					bv->bv_page, bv->bv_offset);
		/* dm_bht functions aren't expected to return errno friendly
		 * values.  They are converted here for uniformity.
		 */
		if (r > 0) {
			DMERR("Pending data for block %llu seen at verify",
			      ULL(block));
			r = -EBUSY;
			goto bad_state;
		}
		if (r < 0) {
			DMERR_LIMIT("Block hash does not match!");
			r = -EACCES;
			goto bad_match;
		}
		REQTRACE("Block %llu verified", ULL(block));

		block++;
		/* After completing a block, allow a reschedule.
		 * TODO(wad) determine if this is truly needed.
		 */
		cond_resched();
	}

	return 0;

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

	io->error = verity_verify(vc, io->bio);

	/* Free up the bio and tag with the return value */
	verity_stats_verify_queue_dec(vc);
	verity_return_bio_to_caller(io);
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
		DMERR("Failed to read for sector %llu (%u)",
		      ULL(io->bio->bi_sector), io->bio->bi_size);
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
	VERITY_BUG_ON(count != to_sector(VERITY_BLOCK_SIZE));

	vc = io->target->private;

	/* The I/O context is nested inside the entry so that we don't need one
	 * io context per page read.
	 */
	entry->io_context = ctx;

	/* We should only get page size requests at present. */
	verity_inc_pending(io);
	bio = verity_alloc_bioset(vc, GFP_NOIO, 1);
	if (unlikely(!bio)) {
		DMCRIT("Out of memory at bio_alloc_bioset");
		dm_bht_read_completed(entry, -ENOMEM);
		return -ENOMEM;
	}
	bio->bi_private = (void *) entry;
	bio->bi_idx = 0;
	bio->bi_size = VERITY_BLOCK_SIZE;
	bio->bi_sector = vc->hash_start + start;
	bio->bi_bdev = vc->hash_dev->bdev;
	bio->bi_end_io = kverityd_io_bht_populate_end;
	bio->bi_rw = REQ_META;
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

/* Submits an io request for each missing block of block hashes.
 * The last one to return will then enqueue this on the io workqueue.
 */
static void kverityd_io_bht_populate(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	u64 block;

	REQTRACE("populating %llu starting at block %llu (io:%p)",
		 ULL(io->count), ULL(io->block), io);
	for (block = io->block; block < io->block + io->count; ++block) {
		int populated;

		DMDEBUG("Calling dm_bht_populate for %ull (io:%p)",
			ULL(block), io);
		populated = dm_bht_populate(&vc->bht, io, block);
		if (populated < 0) {
			DMCRIT("dm_bht_populate error: block %llu (io:%p): %d",
			       ULL(block), io, populated);
			/* TODO(wad) support propagating transient errors
			 *           cleanly.
			 */
			/* verity_dec_pending will handle the error case. */
			io->error = -EPERM;
			break;
		}
	}
	REQTRACE("Block %llu+ initiated %d requests (io: %p)",
		 ULL(io->block), atomic_read(&io->pending) - 1, io);
}

/* Asynchronously called upon the completion of I/O issued
 * from kverityd_src_io_read. verity_dec_pending() acts as
 * the scheduler/flow manager.
 */
static void kverityd_src_io_read_end(struct bio *clone, int error)
{
	struct dm_verity_io *io = clone->bi_private;

	DMDEBUG("I/O completed");
	if (unlikely(!bio_flagged(clone, BIO_UPTODATE) && !error))
		error = -EIO;

	if (unlikely(error)) {
		DMERR("Error occurred: %d (%llu, %u)",
			error, ULL(clone->bi_sector), clone->bi_size);
		io->error = error;
	}

	/* Release the clone which just avoids the block layer from
	 * leaving offsets, etc in unexpected states.
	 */
	bio_put(clone);

	verity_dec_pending(io);
	DMDEBUG("all data has been loaded from the data device");
}

/* If not yet underway, an I/O request will be issued to the vc->dev
 * device for the data needed. It is cloned to avoid unexpected changes
 * to the original bio struct.
 */
static void kverityd_src_io_read(struct dm_verity_io *io)
{
	struct verity_config *vc = io->target->private;
	struct bio *clone;

	VERITY_BUG_ON(!io);

	/* If clone is non-NULL, then the read is already issued. Could also
	 * check BIO_UPTODATE, but it doesn't seem needed.
	 */
	if (io->flags & VERITY_IOFLAGS_CLONED) {
		DMDEBUG("io_read called with existing bio. bailing: %p", io);
		return;
	}
	io->flags |= VERITY_IOFLAGS_CLONED;

	DMDEBUG("kverity_io_read started");

	/* Clone the bio. The block layer may modify the bvec array. */
	DMDEBUG("Creating clone of the request");
	ALLOCTRACE("clone for io %p, sector %llu",
		   io, ULL(vc->start + io->sector));
	clone = verity_bio_clone(io);
	if (unlikely(!clone)) {
		io->error = -ENOMEM;
		return;
	}

	verity_inc_pending(io);

	/* Submit to the block device */
	DMDEBUG("Submitting bio");
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
	VERITY_BUG_ON(!io->bio);

	/* Issue requests asynchronously. */
	verity_inc_pending(io);
	kverityd_src_io_read(io);
	kverityd_io_bht_populate(io);
	verity_dec_pending(io);
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
		VERITY_BUG_ON(bio->bi_sector % to_sector(VERITY_BLOCK_SIZE));
		VERITY_BUG_ON(bio->bi_size % VERITY_BLOCK_SIZE);

		/* Queue up the request to be verified */
		io = verity_io_alloc(ti, bio, bio->bi_sector - ti->begin);
		if (!io) {
			DMERR_LIMIT("Failed to allocate and init IO data");
			return DM_MAPIO_REQUEUE;
		}
		verity_stats_io_queue_inc(vc);
		INIT_DELAYED_WORK(&io->work, kverityd_io);
		queue_delayed_work(kverityd_ioq, &io->work, 0);
	}

	return DM_MAPIO_SUBMITTED;
}

static void splitarg(char *arg, char **key, char **val) {
	*key = strsep(&arg, "=");
	*val = strsep(&arg, "");
}

/*
 * Non-block interfaces and device-mapper specific code
 */

/**
 * verity_ctr - Construct a verified mapping
 * @ti:   Target being created
 * @argc: Number of elements in argv
 * @argv: Vector of key-value pairs (see below).
 *
 * Accepts the following keys:
 * @payload:        hashed device
 * @hashtree:       device hashtree is stored on
 * @hashstart:      start address of hashes (default 0)
 * @alg:            hash algorithm
 * @root_hexdigest: toplevel hash of the tree
 * @error_behavior: what to do when verification fails [optional]
 *
 * E.g.,
 * payload=/dev/sda2 hashtree=/dev/sda3 alg=sha256
 * root_hexdigest=f08aa4a3695290c569eb1b0ac032ae1040150afb527abbeb0a3da33d82fb2c6e
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
	struct verity_config *vc = NULL;
	int ret = 0;
	sector_t blocks;
	const char *payload = NULL;
	const char *hashtree = NULL;
	unsigned long hashstart = 0;
	const char *alg = NULL;
	const char *root_hexdigest = NULL;
	const char *dev_error_behavior = error_behavior;
	int i;

	if (argc >= 6 && !strchr(argv[3], '=')) {
		/* Transitional hack - support the old positional-argument format.
		 * Detect it because it requires specifying an unused arg
		 * (depth) which does not contain an '='. */
		unsigned long long tmpull;
		if (strcmp(argv[3], "0")) {
			ti->error = "Non-zero depth supplied";
			return -EINVAL;
		}
		if (sscanf(argv[2], "%llu", &tmpull) != 1) {
			ti->error = "Invalid hash_start supplied";
			return -EINVAL;
		}
		payload = argv[0];
		hashtree = argv[1];
		hashstart = tmpull;
		alg = argv[4];
		root_hexdigest = argv[5];
		if (argc > 6)
			dev_error_behavior = argv[6];
	} else {
		for (i = 0; i < argc; ++i) {
			char *key, *val;
			DMWARN("Argument %d: '%s'", i, argv[i]);
			splitarg(argv[i], &key, &val);
			if (!key) {
				DMWARN("Bad argument %d: missing key?", i);
				break;
			}
			if (!val) {
				DMWARN("Bad argument %d='%s': missing value", i, key);
				break;
			}
			if (!strcmp(key, "alg")) {
				alg = val;
			} else if (!strcmp(key, "payload")) {
				payload = val;
			} else if (!strcmp(key, "hashtree")) {
				hashtree = val;
			} else if (!strcmp(key, "root_hexdigest")) {
				root_hexdigest = val;
			} else if (!strcmp(key, "hashstart")) {
				if (strict_strtoul(val, 10, &hashstart)) {
					ti->error = "Invalid hashstart";
					return -EINVAL;
				}
			} else if (!strcmp(key, "error_behavior")) {
				dev_error_behavior = val;
			}
		}
	}

#define NEEDARG(n) \
	if (!(n)) { \
		ti->error = "Missing argument: " #n; \
		return -EINVAL; \
	}

	NEEDARG(alg);
	NEEDARG(payload);
	NEEDARG(hashtree);
	NEEDARG(root_hexdigest);

#undef NEEDARG

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

	/* Calculate the blocks from the given device size */
	vc->size = ti->len;
	blocks = to_bytes(vc->size) >> VERITY_BLOCK_SHIFT;
	if (dm_bht_create(&vc->bht, blocks, alg)) {
		DMERR("failed to create required bht");
		goto bad_bht;
	}
	if (dm_bht_set_root_hexdigest(&vc->bht, root_hexdigest)) {
		DMERR("root hexdigest error");
		goto bad_root_hexdigest;
	}
	dm_bht_set_read_cb(&vc->bht, kverityd_bht_read_callback);

	/* payload: device to verify */
	vc->start = 0;  /* TODO: should this support a starting offset? */
	/* We only ever grab the device in read-only mode. */
	ret = verity_get_device(ti, payload, vc->start, ti->len, &vc->dev);
	if (ret) {
		DMERR("Failed to acquire device '%s': %d", payload, ret);
		ti->error = "Device lookup failed";
		goto bad_verity_dev;
	}

	if ((to_bytes(vc->start) % VERITY_BLOCK_SIZE) ||
	    (to_bytes(vc->size) % VERITY_BLOCK_SIZE)) {
		ti->error = "Device must be VERITY_BLOCK_SIZE divisble/aligned";
		goto bad_hash_start;
	}

	vc->hash_start = (sector_t)hashstart;

	/* hashtree: device with hashes.
	 * Note, payload == hashtree is okay as long as the size of
	 *       ti->len passed to device mapper does not include
	 *       the hashes.
	 */
	if (verity_get_device(ti, hashtree, vc->hash_start,
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
	if (snprintf(vc->hash_alg, CRYPTO_MAX_ALG_NAME, "%s", alg) >=
	    CRYPTO_MAX_ALG_NAME) {
		ti->error = "Hash algorithm name is too long";
		goto bad_hash;
	}

	/* override with optional device-specific error behavior */
	vc->error_behavior = verity_parse_error_behavior(dev_error_behavior);
	if (vc->error_behavior == -1) {
		ti->error = "Bad error_behavior supplied";
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

	/* Allocate the bioset used for request padding */
	/* TODO(wad) allocate a separate bioset for the first verify maybe */
	ALLOCTRACE("bioset for I/O reqs");
	vc->bs = bioset_create(MIN_BIOS, 0);
	if (!vc->bs) {
		ti->error = "Cannot allocate verity bioset";
		goto bad_bs;
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

bad_bs:
	mempool_destroy(vc->io_pool);
bad_slab_pool:
bad_err_behavior:
bad_hash:
	dm_put_device(ti, vc->hash_dev);
bad_hash_dev:
bad_hash_start:
	dm_put_device(ti, vc->dev);
bad_bht:
bad_root_hexdigest:
bad_verity_dev:
	kfree(vc);   /* hash is not secret so no need to zero */
	return -EINVAL;
}

static void verity_dtr(struct dm_target *ti)
{
	struct verity_config *vc = (struct verity_config *) ti->private;

	DMDEBUG("Destroying bs");
	bioset_free(vc->bs);
	DMDEBUG("Destroying io_pool");
	mempool_destroy(vc->io_pool);

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
	limits->logical_block_size = VERITY_BLOCK_SIZE;
	limits->physical_block_size = VERITY_BLOCK_SIZE;
	blk_limits_io_min(limits, VERITY_BLOCK_SIZE);
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

#define VERITY_WQ_FLAGS (WQ_CPU_INTENSIVE|WQ_HIGHPRI)

static int __init dm_verity_init(void)
{
	int r = -ENOMEM;

	_verity_io_pool = KMEM_CACHE(dm_verity_io, 0);
	if (!_verity_io_pool) {
		DMERR("failed to allocate pool dm_verity_io");
		goto bad_io_pool;
	}

	kverityd_ioq = alloc_workqueue("kverityd_io", VERITY_WQ_FLAGS, 1);
	if (!kverityd_ioq) {
		DMERR("failed to create workqueue kverityd_ioq");
		goto bad_io_queue;
	}

	kveritydq = alloc_workqueue("kverityd", VERITY_WQ_FLAGS, 1);
	if (!kveritydq) {
		DMERR("failed to create workqueue kveritydq");
		goto bad_verify_queue;
	}

	r = dm_register_target(&verity_target);
	if (r < 0) {
		DMERR("register failed %d", r);
		goto register_failed;
	}

	DMINFO("version %u.%u.%u loaded", verity_target.version[0],
	       verity_target.version[1], verity_target.version[2]);

	return r;

register_failed:
	destroy_workqueue(kveritydq);
bad_verify_queue:
	destroy_workqueue(kverityd_ioq);
bad_io_queue:
	kmem_cache_destroy(_verity_io_pool);
bad_io_pool:
	return r;
}

static void __exit dm_verity_exit(void)
{
	destroy_workqueue(kveritydq);
	destroy_workqueue(kverityd_ioq);

	dm_unregister_target(&verity_target);
	kmem_cache_destroy(_verity_io_pool);
}

module_init(dm_verity_init);
module_exit(dm_verity_exit);

MODULE_AUTHOR("The Chromium OS Authors <chromium-os-dev@chromium.org>");
MODULE_DESCRIPTION(DM_NAME " target for transparent disk integrity checking");
MODULE_LICENSE("GPL");
