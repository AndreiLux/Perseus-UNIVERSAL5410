 /*
 * Copyright (C) 2010 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *
 * Device-Mapper block hash tree interface.
 * See Documentation/device-mapper/dm-bht.txt for details.
 *
 * This file is released under the GPL.
 */

#include <asm/atomic.h>
#include <asm/page.h>
#include <linux/bitops.h>  /* for fls() */
#include <linux/bug.h>
#include <linux/cpumask.h>  /* nr_cpu_ids */
/* #define CONFIG_DM_DEBUG 1 */
#include <linux/device-mapper.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/dm-bht.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>  /* k*alloc */
#include <linux/string.h>  /* memset */

#define DM_MSG_PREFIX "dm bht"

/* For sector formatting. */
#if defined(_LP64) || defined(__LP64__) || __BITS_PER_LONG == 64
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX "ll"
#endif
#define PRIu64 __PRIS_PREFIX "u"


/*-----------------------------------------------
 * Utilities
 *-----------------------------------------------*/

static u8 from_hex(u8 ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	return -1;
}

/**
 * dm_bht_bin_to_hex - converts a binary stream to human-readable hex
 * @binary:	a byte array of length @binary_len
 * @hex:	a byte array of length @binary_len * 2 + 1
 */
static void dm_bht_bin_to_hex(u8 *binary, u8 *hex, unsigned int binary_len)
{
	while (binary_len-- > 0) {
		sprintf((char *__restrict__)hex, "%02hhx", (int)*binary);
		hex += 2;
		binary++;
	}
}

/**
 * dm_bht_hex_to_bin - converts a hex stream to binary
 * @binary:	a byte array of length @binary_len
 * @hex:	a byte array of length @binary_len * 2 + 1
 */
static void dm_bht_hex_to_bin(u8 *binary, const u8 *hex,
			      unsigned int binary_len)
{
	while (binary_len-- > 0) {
		*binary = from_hex(*(hex++));
		*binary *= 16;
		*binary += from_hex(*(hex++));
		binary++;
	}
}

static void dm_bht_log_mismatch(struct dm_bht *bht, u8 *given, u8 *computed)
{
	u8 given_hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
	u8 computed_hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
	dm_bht_bin_to_hex(given, given_hex, bht->digest_size);
	dm_bht_bin_to_hex(computed, computed_hex, bht->digest_size);
	DMERR_LIMIT("%s != %s", given_hex, computed_hex);
}

/* Used for turning verifiers into computers */
typedef int (*dm_bht_compare_cb)(struct dm_bht *, u8 *, u8 *);

/**
 * dm_bht_compute_and_compare: hashes a page of data; compares it to a hash
 */
static int dm_bht_compute_and_compare(struct dm_bht *bht,
				      struct hash_desc *hash_desc,
				      struct page *page,
				      u8 *expected_digest,
				      dm_bht_compare_cb compare_cb)
{
	struct scatterlist sg;
	u8 digest[DM_BHT_MAX_DIGEST_SIZE];
	int result;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, PAGE_SIZE, 0);
	/* Note, this is synchronous. */
	if (crypto_hash_init(hash_desc)) {
		DMCRIT("failed to reinitialize crypto hash (proc:%d)",
			smp_processor_id());
		return -EINVAL;
	}
	if (crypto_hash_digest(hash_desc, &sg, PAGE_SIZE, digest)) {
		DMCRIT("crypto_hash_digest failed");
		return -EINVAL;
	}

	result = compare_cb(bht, expected_digest, digest);
#ifdef CONFIG_DM_DEBUG
	if (result)
		dm_bht_log_mismatch(bht, expected_digest, digest);
#endif
	return result;
}

static __always_inline struct dm_bht_level *dm_bht_get_level(struct dm_bht *bht,
							     unsigned int depth)
{
	return &bht->levels[depth];
}

static __always_inline unsigned int dm_bht_get_level_shift(struct dm_bht *bht,
						  unsigned int depth)
{
	return (bht->depth - depth) * bht->node_count_shift;
}

/* For the given depth, this is the entry index.  At depth+1 it is the node
 * index for depth.
 */
static __always_inline unsigned int dm_bht_index_at_level(struct dm_bht *bht,
							  unsigned int depth,
							  unsigned int leaf)
{
	return leaf >> dm_bht_get_level_shift(bht, depth);
}

static __always_inline u8 *dm_bht_node(struct dm_bht *bht,
				       struct dm_bht_entry *entry,
				       unsigned int node_index)
{
	return &entry->nodes[node_index * bht->digest_size];
}


/*-----------------------------------------------
 * Implementation functions
 *-----------------------------------------------*/

static int dm_bht_initialize_entries(struct dm_bht *bht);

static int dm_bht_read_callback_stub(void *ctx, sector_t start, u8 *dst,
				     sector_t count,
				     struct dm_bht_entry *entry);
static int dm_bht_write_callback_stub(void *ctx, sector_t start,
				      u8 *dst, sector_t count,
				      struct dm_bht_entry *entry);

/**
 * dm_bht_create - prepares @bht for us
 * @bht:	pointer to a dm_bht_create()d bht
 * @depth:	tree depth without the root; including block hashes
 * @block_count:the number of block hashes / tree leaves
 * @alg_name:	crypto hash algorithm name
 *
 * Returns 0 on success.
 *
 * Callers can offset into devices by storing the data in the io callbacks.
 * TODO(wad) bust up into smaller helpers
 */
int dm_bht_create(struct dm_bht *bht, unsigned int depth,
		  unsigned int block_count, const char *alg_name)
{
	int status = 0;
	int cpu = 0;

	/* Allocate enough crypto contexts to be able to perform verifies
	 * on all available CPUs.
	 */
	bht->hash_desc = (struct hash_desc *)
		kcalloc(nr_cpu_ids, sizeof(struct hash_desc), GFP_KERNEL);
	if (!bht->hash_desc) {
		DMERR("failed to allocate crypto hash contexts");
		return -ENOMEM;
	}

	/* Setup the hash first. Its length determines much of the bht layout */
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu) {
		bht->hash_desc[cpu].tfm = crypto_alloc_hash(alg_name, 0, 0);
		if (IS_ERR(bht->hash_desc[cpu].tfm)) {
			DMERR("failed to allocate crypto hash '%s'", alg_name);
			status = -ENOMEM;
			bht->hash_desc[cpu].tfm = NULL;
			goto bad_hash_alg;
		}
	}
	bht->digest_size = crypto_hash_digestsize(bht->hash_desc[0].tfm);
	/* We expect to be able to pack >=2 hashes into a page */
	if (PAGE_SIZE / bht->digest_size < 2) {
		DMERR("too few hashes fit in a page");
		status = -EINVAL;
		goto bad_digest_len;
	}

	if (bht->digest_size > DM_BHT_MAX_DIGEST_SIZE) {
		DMERR("DM_BHT_MAX_DIGEST_SIZE too small for chosen digest");
		status = -EINVAL;
		goto bad_digest_len;
	}

	bht->root_digest = kzalloc(bht->digest_size, GFP_KERNEL);
	if (!bht->root_digest) {
		DMERR("failed to allocate memory for root digest");
		status = -ENOMEM;
		goto bad_root_digest_alloc;
	}
	/* We use the same defines for root state but just:
	 * UNALLOCATED, REQUESTED, and VERIFIED since the workflow is
	 * different.
	 */
	atomic_set(&bht->root_state, DM_BHT_ENTRY_UNALLOCATED);

	/* Configure the tree */
	bht->block_count = block_count;
	DMDEBUG("Setting block_count %u", block_count);
	if (block_count == 0) {
		DMERR("block_count must be non-zero");
		status = -EINVAL;
		goto bad_block_count;
	}

	bht->depth = depth;  /* assignment below */
	DMDEBUG("Setting depth %u", depth);
	if (!depth || depth > UINT_MAX / sizeof(struct dm_bht_level)) {
		DMERR("bht depth is invalid: %u", depth);
		status = -EINVAL;
		goto bad_depth;
	}

	/* Allocate levels. Each level of the tree may have an arbitrary number
	 * of dm_bht_entry structs.  Each entry contains node_count nodes.
	 * Each node in the tree is a cryptographic digest of either node_count
	 * nodes on the subsequent level or of a specific block on disk.
	 */
	bht->levels = (struct dm_bht_level *)
			kcalloc(depth, sizeof(struct dm_bht_level), GFP_KERNEL);
	if (!bht->levels) {
		DMERR("failed to allocate tree levels");
		status = -ENOMEM;
		goto bad_level_alloc;
	}

	/* Each dm_bht_entry->nodes is one page.  The node code tracks
	 * how many nodes fit into one entry where a node is a single
	 * hash (message digest).
	 */
	bht->node_count_shift = fls(PAGE_SIZE / bht->digest_size) - 1;
	/* Round down to the nearest power of two.  This makes indexing
	 * into the tree much less painful.
	 */
	bht->node_count = 1 << bht->node_count_shift;

	/* TODO(wad) if node_count < DM_BHT_MAX_NODE_COUNT, then retry with
	 * node_count_shift-1.
	 */
	if (bht->node_count > DM_BHT_MAX_NODE_COUNT) {
		DMERR("node_count maximum node bitmap size");
		status = -EINVAL;
		goto bad_node_count;
	}

	/* This is unlikely to happen, but with 64k pages, who knows. */
	if (bht->node_count > UINT_MAX / bht->digest_size) {
		DMERR("node_count * hash_len exceeds UINT_MAX!");
		status = -EINVAL;
		goto bad_node_count;
	}
	/* Ensure that we can safely shift by this value. */
	if (depth * bht->node_count_shift >= sizeof(unsigned int) * 8) {
		DMERR("specified depth and node_count_shift is too large");
		status = -EINVAL;
		goto bad_node_count;
	}

	/* Setup callback stubs */
	bht->read_cb = &dm_bht_read_callback_stub;
	bht->write_cb = &dm_bht_write_callback_stub;

	status = dm_bht_initialize_entries(bht);
	if (status)
		goto bad_entries_alloc;

	bht->verify_mode = DM_BHT_REVERIFY_LEAVES;
	bht->entry_readahead = 0;
	return 0;

bad_entries_alloc:
	while (bht->depth-- > 0)
		kfree(bht->levels[bht->depth].entries);
bad_node_count:
	kfree(bht->levels);
bad_level_alloc:
bad_block_count:
bad_depth:
	kfree(bht->root_digest);
bad_root_digest_alloc:
bad_digest_len:
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
		if (bht->hash_desc[cpu].tfm)
			crypto_free_hash(bht->hash_desc[cpu].tfm);
bad_hash_alg:
	kfree(bht->hash_desc);
	return status;
}
EXPORT_SYMBOL(dm_bht_create);

static int dm_bht_initialize_entries(struct dm_bht *bht)
{
	/* The last_index represents the index into the last
	 * block digest that will be stored in the tree.  By walking the
	 * tree with that index, it is possible to compute the total number
	 * of entries needed at each level in the tree.
	 *
	 * Since each entry will contain up to |node_count| nodes of the tree,
	 * it is possible that the last index may not be at the end of a given
	 * entry->nodes.  In that case, it is assumed the value is padded.
	 *
	 * Note, we treat both the tree root (1 hash) and the tree leaves
	 * independently from the bht data structures.  Logically, the root is
	 * depth=-1 and the block layer level is depth=bht->depth
	 */
	unsigned int last_index = ALIGN(bht->block_count, bht->node_count) - 1;
	unsigned int total_entries = 0;
	struct dm_bht_level *level = NULL;
	unsigned int depth;

	/* check that the largest level->count can't result in an int overflow
	 * on allocation or sector calculation.
	 */
	if (((last_index >> bht->node_count_shift) + 1) >
	    UINT_MAX / max((unsigned int)sizeof(struct dm_bht_entry),
			   (unsigned int)to_sector(PAGE_SIZE))) {
		DMCRIT("required entries %u is too large",
		       last_index + 1);
		return -EINVAL;
	}

	/* Track the current sector location for each level so we don't have to
	 * compute it during traversals.
	 */
	bht->sectors = 0;
	for (depth = 0; depth < bht->depth; ++depth) {
		level = dm_bht_get_level(bht, depth);
		level->count = dm_bht_index_at_level(bht, depth,
						     last_index) + 1;
		DMDEBUG("depth: %u entries: %u", depth, level->count);
		/* TODO(wad) consider the case where the data stored for each
		 * level is done with contiguous pages (instead of using
		 * entry->nodes) and the level just contains two bitmaps:
		 * (a) which pages have been loaded from disk
		 * (b) which specific nodes have been verified.
		 */
		level->entries = kcalloc(level->count,
					 sizeof(struct dm_bht_entry),
					 GFP_KERNEL);
		if (!level->entries) {
			DMERR("failed to allocate entries for depth %u",
			      bht->depth);
			/* let the caller clean up the mess */
			return -ENOMEM;
		}
		total_entries += level->count;
		level->sector = bht->sectors;
		/* number of sectors per entry * entries at this level */
		bht->sectors += level->count * to_sector(PAGE_SIZE);
		/* not ideal, but since unsigned overflow behavior is defined */
		if (bht->sectors < level->sector) {
			DMCRIT("level sector calculation overflowed");
			return -EINVAL;
		}
	}

	/* Go ahead and reserve enough space for everything.  We really don't
	 * want memory allocation failures.  Once we start freeing verified
	 * entries, then we can reduce this reservation.
	 */
	bht->entry_pool = mempool_create_page_pool(total_entries, 0);
	if (!bht->entry_pool) {
		DMERR("failed to allocate mempool");
		return -ENOMEM;
	}

	return 0;
}

static int dm_bht_read_callback_stub(void *ctx, sector_t start, u8 *dst,
				     sector_t count, struct dm_bht_entry *entry)
{
	DMCRIT("dm_bht_read_callback_stub called!");
	dm_bht_read_completed(entry, -EIO);
	return -EIO;
}

static int dm_bht_write_callback_stub(void *ctx, sector_t start,
				      u8 *dst, sector_t count,
				      struct dm_bht_entry *entry)
{
	DMCRIT("dm_bht_write_callback_stub called!");
	dm_bht_write_completed(entry, -EIO);
	return -EIO;
}

/**
 * dm_bht_read_completed
 * @entry:	pointer to the entry that's been loaded
 * @status:	I/O status. Non-zero is failure.
 * MUST always be called after a read_cb completes.
 */
void dm_bht_read_completed(struct dm_bht_entry *entry, int status)
{
	int state;
	if (status) {
		/* TODO(wad) add retry support */
		DMCRIT("an I/O error occurred while reading entry");
		atomic_set(&entry->state, DM_BHT_ENTRY_ERROR_IO);
		/* entry->nodes will be freed later */
		return;
	}
	state = atomic_cmpxchg(&entry->state,
				   DM_BHT_ENTRY_PENDING,
				   DM_BHT_ENTRY_READY);
	if (state != DM_BHT_ENTRY_PENDING) {
		DMCRIT("state changed on entry out from under io");
		BUG();
	}
}
EXPORT_SYMBOL(dm_bht_read_completed);

/**
 * dm_bht_write_completed
 * @entry:	pointer to the entry that's been loaded
 * @status:	I/O status. Non-zero is failure.
 * Should be called after a write_cb completes. Currently only catches
 * errors which more writers don't care about.
 */
void dm_bht_write_completed(struct dm_bht_entry *entry, int status)
{
	if (status) {
		DMCRIT("an I/O error occurred while writing entry");
		atomic_set(&entry->state, DM_BHT_ENTRY_ERROR_IO);
		/* entry->nodes will be freed later */
		return;
	}
}
EXPORT_SYMBOL(dm_bht_write_completed);


/* dm_bht_maybe_read_entries
 * Attempts to atomically acquire each entry, allocated any needed
 * memory, and issues I/O callbacks to load the hashes from disk.
 * Returns 0 if all entries are loaded and verified.  On error, the
 * return value is negative. When positive, it is the state values
 * ORd.
 */
static int dm_bht_maybe_read_entries(struct dm_bht *bht, void *ctx,
				     unsigned int depth, unsigned int index,
				     unsigned int count, bool until_exist)
{
	struct dm_bht_level *level;
	struct dm_bht_entry *entry, *last_entry;
	sector_t current_sector;
	int state = 0;
	int status = 0;
	struct page *node_page = NULL;
	BUG_ON(depth >= bht->depth);

	level = &bht->levels[depth];
	if (count > level->count - index) {
		DMERR("dm_bht_maybe_read_entries(d=%u,ei=%u,count=%u): "
		      "index+count exceeds available entries %u",
			depth, index, count, level->count);
		return -EINVAL;
	}
	/* XXX: hardcoding PAGE_SIZE means that a perfectly valid image
	 *      on one system may not work on a different kernel.
	 * TODO(wad) abstract PAGE_SIZE with a bht->entry_size or
	 *           at least a define and ensure bht->entry_size is
	 *           sector aligned at least.
	 */
	current_sector = level->sector + to_sector(index * PAGE_SIZE);
	for (entry = &level->entries[index], last_entry = entry + count;
	     entry < last_entry;
	     ++entry, current_sector += to_sector(PAGE_SIZE)) {
		/* If the entry's state is UNALLOCATED, then we'll claim it
		 * for allocation and loading.
		 */
		state = atomic_cmpxchg(&entry->state,
				       DM_BHT_ENTRY_UNALLOCATED,
				       DM_BHT_ENTRY_PENDING);
		DMDEBUG("dm_bht_maybe_read_entries(d=%u,ei=%u,count=%u): "
			"ei=%lu, state=%d",
			depth, index, count,
			(unsigned long)(entry - level->entries), state);
		if (state <= DM_BHT_ENTRY_ERROR) {
			DMCRIT("entry %u is in an error state", index);
			return state;
		}

		/* Currently, the verified state is unused. */
		if (state == DM_BHT_ENTRY_VERIFIED) {
			if (until_exist)
				return 0;
			/* Makes 0 == verified. Is that ideal? */
			continue;
		}

		if (state != DM_BHT_ENTRY_UNALLOCATED) {
			/* PENDING, READY, ... */
			if (until_exist)
				return state;
			status |= state;
			continue;
		}
		/* Current entry is claimed for allocation and loading */
		node_page = (struct page *) mempool_alloc(bht->entry_pool,
							  GFP_NOIO);
		if (!node_page) {
			DMCRIT("failed to allocate memory for "
			       "entry->nodes from pool");
			return -ENOMEM;
		}
		/* dm-bht guarantees page-aligned memory for callbacks. */
		entry->nodes = page_address(node_page);
		/* Let the caller know that not all the data is yet available */
		status |= DM_BHT_ENTRY_REQUESTED;
		/* Issue the read callback */
		/* TODO(wad) error check callback here too */
		DMDEBUG("dm_bht_maybe_read_entries(d=%u,ei=%u,count=%u): "
			"reading %lu",
			depth, index, count,
			(unsigned long)(entry - level->entries));
		bht->read_cb(ctx,   /* external context */
			     current_sector,  /* starting sector */
			     entry->nodes,  /* destination */
			     to_sector(PAGE_SIZE),
			     entry);  /* io context */

	}
	/* Should only be 0 if all entries were verified and not just ready */
	return status;
}

static int dm_bht_compare_hash(struct dm_bht *bht, u8 *known, u8 *computed)
{
	return memcmp(known, computed, bht->digest_size);
}

static int dm_bht_update_hash(struct dm_bht *bht, u8 *known, u8 *computed)
{
#ifdef CONFIG_DM_DEBUG
	u8 hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
#endif
	memcpy(known, computed, bht->digest_size);
#ifdef CONFIG_DM_DEBUG
	dm_bht_bin_to_hex(computed, hex, bht->digest_size);
	DMDEBUG("updating with hash: %s", hex);
#endif
	return 0;
}

/* digest length and bht must be checked already */
static int dm_bht_check_block(struct dm_bht *bht, unsigned int block_index,
			      u8 *digest, int *entry_state)
{
	int depth;
	unsigned int index;
	struct dm_bht_entry *entry;

	/* The leaves contain the block hashes */
	depth = bht->depth - 1;

	/* Index into the bottom level.  Each entry in this level contains
	 * nodes whose hashes are the direct hashes of one block of data on
	 * disk.
	 */
	index = block_index >> bht->node_count_shift;
	entry = &bht->levels[depth].entries[index];

	*entry_state = atomic_read(&entry->state);
	if (*entry_state <= DM_BHT_ENTRY_ERROR) {
		DMCRIT("leaf entry for block %u is invalid",
		       block_index);
		return *entry_state;
	}
	if (*entry_state <= DM_BHT_ENTRY_PENDING) {
		DMERR("leaf data not yet loaded for block %u",
		      block_index);
		return 1;
	}

	/* Index into the entry data */
	index = (block_index % bht->node_count) * bht->digest_size;
	if (memcmp(&entry->nodes[index], digest, bht->digest_size)) {
		DMCRIT("digest mismatch for block %u", block_index);
		dm_bht_log_mismatch(bht, &entry->nodes[index], digest);
		return DM_BHT_ENTRY_ERROR_MISMATCH;
	}
	/* TODO(wad) update bht->block_bitmap here or in the caller */
	return 0;
}

/* Walk all entries at level 0 to compute the root digest.
 * 0 on success.
 */
static int dm_bht_compute_root(struct dm_bht *bht, u8 *digest)
{
	struct dm_bht_entry *entry;
	unsigned int count;
	struct scatterlist sg;  /* feeds digest() */
	struct hash_desc *hash_desc;
	hash_desc = &bht->hash_desc[smp_processor_id()];
	entry = bht->levels[0].entries;

	if (crypto_hash_init(hash_desc)) {
		DMCRIT("failed to reinitialize crypto hash (proc:%d)",
			smp_processor_id());
		return -EINVAL;
	}

	/* Point the scatterlist to the entries, then compute the digest */
	for (count = 0; count < bht->levels[0].count; ++count, ++entry) {
		if (atomic_read(&entry->state) <= DM_BHT_ENTRY_PENDING) {
			DMCRIT("data not ready to compute root: %u",
			       count);
			return 1;
		}
		sg_init_table(&sg, 1);
		sg_set_page(&sg, virt_to_page(entry->nodes), PAGE_SIZE, 0);
		if (crypto_hash_update(hash_desc, &sg, PAGE_SIZE)) {
			DMCRIT("Failed to update crypto hash");
			return -EINVAL;
		}
	}

	if (crypto_hash_final(hash_desc, digest)) {
		DMCRIT("Failed to compute final digest");
		return -EINVAL;
	}

	return 0;
}

static int dm_bht_verify_root(struct dm_bht *bht,
			      dm_bht_compare_cb compare_cb)
{
	int status = 0;
	u8 digest[DM_BHT_MAX_DIGEST_SIZE];
	if (atomic_read(&bht->root_state) == DM_BHT_ENTRY_VERIFIED)
		return 0;
	status = dm_bht_compute_root(bht, digest);
	if (status) {
		DMCRIT("Failed to compute root digest for verification");
		return status;
	}
	DMDEBUG("root computed");
	status = compare_cb(bht, bht->root_digest, digest);
	if (status) {
		DMCRIT("invalid root digest: %d", status);
		dm_bht_log_mismatch(bht, bht->root_digest, digest);
		return DM_BHT_ENTRY_ERROR_MISMATCH;
	}
	/* Could do a cmpxchg, but this should be safe. */
	atomic_set(&bht->root_state, DM_BHT_ENTRY_VERIFIED);
	return 0;
}

/* dm_bht_verify_path
 * Verifies the path to block_index from depth=0. Returns 0 on ok.
 */
static int dm_bht_verify_path(struct dm_bht *bht, unsigned int block_index,
			      dm_bht_compare_cb compare_cb)
{
	unsigned int depth;
	struct dm_bht_level *level;
	struct dm_bht_entry *entry;
	struct dm_bht_entry *parent = NULL;
	u8 *node;
	unsigned int node_index;
	unsigned int entry_index;
	unsigned int parent_index = 0;  /* for logging */
	struct hash_desc *hash_desc;
	int state;
	int ret = 0;

	hash_desc = &bht->hash_desc[smp_processor_id()];
	for (depth = 0; depth < bht->depth; ++depth) {
		level = dm_bht_get_level(bht, depth);
		entry_index = dm_bht_index_at_level(bht, depth, block_index);
		DMDEBUG("verify_path for bi=%u on d=%d ei=%u (max=%u)",
			block_index, depth, entry_index, level->count);
		BUG_ON(entry_index >= level->count);
		entry = &level->entries[entry_index];

		/* Catch any existing errors */
		state = atomic_read(&entry->state);
		if (state <= DM_BHT_ENTRY_ERROR) {
			DMCRIT("entry(d=%u,idx=%u) is in an error state: %d",
			       depth, entry_index, state);
			DMCRIT("verification is not possible");
			ret = 1;
			break;
		} else if (state <= DM_BHT_ENTRY_PENDING) {
			DMERR("entry not ready for verify: d=%u,e=%u",
			      depth, entry_index);
			ret = 1;
			break;
		}

		/* We go through the error-checking for depth=0 here so we
		 * don't have to duplicate it above.  After this point, parent
		 * will always be set.
		 */
		if (!parent) {
			parent_index = entry_index;
			parent = entry;
			continue;
		}


		/* Because we are one level down, the node_index into the
		 * the parent's node list modulo the number of nodes.
		 */
		node_index = entry_index % bht->node_count;

		/* If the nodes in entry have already been checked against the
		 * parent, then we're done.
		 */
		if (state == DM_BHT_ENTRY_VERIFIED) {
			DMDEBUG("verify_path node %u is verified in parent %u",
				node_index, parent_index);
			/* If this entry has been checked, move along. */
			parent_index = entry_index;
			parent = entry;
			continue;
		}

		DMDEBUG("verify_path node %u is not verified in parent %u",
			node_index, parent_index);
		/* We need to check that this entry matches the expected
		 * hash in the parent->nodes.
		 */
		node = dm_bht_node(bht, parent, node_index);
		if (dm_bht_compute_and_compare(bht, hash_desc,
					       virt_to_page(entry->nodes),
					       node, compare_cb)) {
			DMERR("failed to verify entry's hash against parent "
			      "(d=%u,ei=%u,p=%u,pnode=%u)",
			      depth, entry_index, parent_index, node_index);
			ret = DM_BHT_ENTRY_ERROR_MISMATCH;
			break;
		}

		/* Instead of keeping a bitmap of which children have been
		 * checked, this data is kept in the child state. If full
		 * reverifies have been set, then no intermediate entry/node is
		 * ever marked as verified.
		 */
		if (bht->verify_mode != DM_BHT_FULL_REVERIFY)
			atomic_cmpxchg(&entry->state,
				       DM_BHT_ENTRY_READY,
				       DM_BHT_ENTRY_VERIFIED);
		parent_index = entry_index;
		parent = entry;
	}
	return ret;
}

/**
 * dm_bht_store_block - sets a given block's hash in the tree
 * @bht:	pointer to a dm_bht_create()d bht
 * @block_index:numeric index of the block in the tree
 * @digest:	array of u8s containing the digest of length @bht->digest_size
 *
 * Returns 0 on success, >0 when data is pending, and <0 when a IO or other
 * error has occurred.
 *
 * If the containing entry in the tree is unallocated, it will allocate memory
 * and mark the entry as ready.  All other block entries will be 0s.  This
 * function is not safe for simultaneous use when verifying data and should not
 * be used if the @bht is being accessed by any other functions in any other
 * threads/processes.
 *
 * It is expected that virt_to_page will work on |block_data|.
 */
int dm_bht_store_block(struct dm_bht *bht, unsigned int block_index,
		       u8 *block_data)
{
	int depth;
	unsigned int index;
	unsigned int node_index;
	struct dm_bht_entry *entry;
	struct dm_bht_level *level;
	int state;
	struct hash_desc *hash_desc;
	struct page *node_page = NULL;

	/* Look at the last level of nodes above the leaves (data blocks) */
	depth = bht->depth - 1;

	/* Index into the level */
	level = dm_bht_get_level(bht, depth);
	index = dm_bht_index_at_level(bht, depth, block_index);
	/* Grab the node index into the current entry by getting the
	 * index at the leaf-level.
	 */
	node_index = dm_bht_index_at_level(bht, depth + 1, block_index) %
		     bht->node_count;
	entry = &level->entries[index];

	DMDEBUG("Storing block %u in d=%d,ei=%u,ni=%u,s=%d",
		block_index, depth, index, node_index,
		atomic_read(&entry->state));

	state = atomic_cmpxchg(&entry->state,
			       DM_BHT_ENTRY_UNALLOCATED,
			       DM_BHT_ENTRY_PENDING);
	/* !!! Note. It is up to the users of the update interface to
	 *     ensure the entry data is fully populated prior to use.
	 *     The number of updated entries is NOT tracked.
	 */
	if (state == DM_BHT_ENTRY_UNALLOCATED) {
		node_page = (struct page *) mempool_alloc(bht->entry_pool,
							  GFP_KERNEL);
		if (!node_page) {
			atomic_set(&entry->state, DM_BHT_ENTRY_ERROR);
			return -ENOMEM;
		}
		entry->nodes = page_address(node_page);
		/* TODO(wad) could expose this to the caller to that they
		 * can transition from unallocated to ready manually.
		 */
		atomic_set(&entry->state, DM_BHT_ENTRY_READY);
	} else if (state <= DM_BHT_ENTRY_ERROR) {
		DMCRIT("leaf entry for block %u is invalid",
		      block_index);
		return state;
	} else if (state == DM_BHT_ENTRY_PENDING) {
		DMERR("leaf data is pending for block %u", block_index);
		return 1;
	}

	hash_desc = &bht->hash_desc[smp_processor_id()];
	dm_bht_compute_and_compare(bht, hash_desc, virt_to_page(block_data),
				   dm_bht_node(bht, entry, node_index),
				   &dm_bht_update_hash);
	return 0;
}
EXPORT_SYMBOL(dm_bht_store_block);

/**
 * dm_bht_zeroread_callback - read callback which always returns 0s
 * @ctx:	ignored
 * @start:	ignored
 * @data:	buffer to write 0s to
 * @count:	number of sectors worth of data to write
 * @complete_ctx: opaque context for @completed
 * @completed: callback to confirm end of data read
 *
 * Always returns 0.
 *
 * Meant for use by dm_compute() callers.  It allows dm_populate to
 * be used to pre-fill a tree with zeroed out entry nodes.
 */
int dm_bht_zeroread_callback(void *ctx, sector_t start, u8 *dst,
			     sector_t count, struct dm_bht_entry *entry)
{
	memset(dst, 0, to_bytes(count));
	dm_bht_read_completed(entry, 0);
	return 0;
}
EXPORT_SYMBOL(dm_bht_zeroread_callback);

/**
 * dm_bht_compute - computes and updates all non-block-level hashes in a tree
 * @bht:	pointer to a dm_bht_create()d bht
 * @read_cb_ctx:opaque read_cb context for all I/O on this call
 *
 * Returns 0 on success, >0 when data is pending, and <0 when a IO or other
 * error has occurred.
 *
 * Walks the tree and computes the hashes at each level from the
 * hashes below. This can only be called once per tree creation
 * since it will mark entries verified. Expects dm_bht_populate() to
 * correctly populate the tree from the read_callback_stub.
 *
 * This function should not be used when verifying the same tree and
 * should not be used with multiple simultaneous operators on @bht.
 */
int dm_bht_compute(struct dm_bht *bht, void *read_cb_ctx)
{
	unsigned int block;
	int updated = 0;
	/* Start at the last block and walk backwards. */
	for (block = bht->block_count - 1; block != 0;
	     block -= bht->node_count) {
		DMDEBUG("Updating levels for block %u", block);
		updated = dm_bht_populate(bht, read_cb_ctx, block);
		if (updated < 0) {
			DMERR("Failed to pre-zero entries");
			return updated;
		}
		updated = dm_bht_verify_path(bht,
					     block,
					     dm_bht_update_hash);
		if (updated) {
			DMERR("Failed to update levels for block %u",
			      block);
			return updated;
		}
		if (block < bht->node_count)
			break;
	}
	/* Don't forget the root digest! */
	DMDEBUG("Calling verify_root with update_hash");
	updated = dm_bht_verify_root(bht, dm_bht_update_hash);
	return updated;
}
EXPORT_SYMBOL(dm_bht_compute);

/**
 * dm_bht_sync - writes the tree in memory to disk
 * @bht:	pointer to a dm_bht_create()d bht
 * @write_ctx:	callback context for writes issued
 *
 * Since all entry nodes are PAGE_SIZE, the data will be pre-aligned and
 * padded.
 */
int dm_bht_sync(struct dm_bht *bht, void *write_cb_ctx)
{
	unsigned int depth;
	int ret = 0;
	int state;
	sector_t sector;
	struct dm_bht_level *level;
	struct dm_bht_entry *entry;
	struct dm_bht_entry *entry_end;

	for (depth = 0; depth < bht->depth; ++depth) {
		level = dm_bht_get_level(bht, depth);
		entry_end = level->entries + level->count;
		sector = level->sector;
		for (entry = level->entries; entry < entry_end; ++entry) {
			state = atomic_read(&entry->state);
			if (state <= DM_BHT_ENTRY_PENDING) {
				DMERR("At depth %d, entry %lu is not ready",
				      depth,
				      (unsigned long)(entry - level->entries));
				return state;
			}
			ret = bht->write_cb(write_cb_ctx,
					    sector,
					    entry->nodes,
					    to_sector(PAGE_SIZE),
					    entry);
			if (ret) {
				DMCRIT("an error occurred writing entry %lu",
				      (unsigned long)(entry - level->entries));
				return ret;
			}
			sector += to_sector(PAGE_SIZE);
		}
	}

	return 0;
}
EXPORT_SYMBOL(dm_bht_sync);

/**
 * dm_bht_populate - reads entries from disk needed to verify a given block
 * @bht:	pointer to a dm_bht_create()d bht
 * @read_cb_ctx:context used for all read_cb calls on this request
 * @block_index:specific block data is expected from
 *
 * Callers may wish to call dm_bht_populate(0) immediately after initialization
 * to start loading in all the level[0] entries.
 */
int dm_bht_populate(struct dm_bht *bht, void *read_cb_ctx,
		    unsigned int block_index)
{
	unsigned int depth;
	struct dm_bht_level *level;
	int populated = 0;  /* return value */
	unsigned int entry_index = 0;
	int read_status = 0;
	int root_state = 0;

	if (block_index >= bht->block_count) {
		DMERR("Request to populate for invalid block: %u",
		      block_index);
		return -EIO;
	}
	DMDEBUG("dm_bht_populate(%u)", block_index);

	/* Load in all of level 0 if the root is unverified */
	root_state = atomic_read(&bht->root_state);
	/* TODO(wad) create a separate io object for the root request which
	 * can continue on and be verified and stop making every request
	 * check.
	 */
	if (root_state != DM_BHT_ENTRY_VERIFIED) {
		DMDEBUG("root data is not yet loaded");
		/* If positive, it means some are pending. */
		populated = dm_bht_maybe_read_entries(bht, read_cb_ctx, 0, 0,
						      bht->levels[0].count,
						      true);
		if (populated < 0) {
			DMCRIT("an error occurred while reading level[0]");
			/* TODO(wad) define std error codes */
			return populated;
		}
	}

	for (depth = 1; depth < bht->depth; ++depth) {
		level = dm_bht_get_level(bht, depth);
		entry_index = dm_bht_index_at_level(bht, depth,
						    block_index);
		DMDEBUG("populate for bi=%u on d=%d ei=%u (max=%u)",
			block_index, depth, entry_index, level->count);

		/* Except for the root node case, we should only ever need
		 * to load one entry along the path.
		 */
		read_status = dm_bht_maybe_read_entries(bht, read_cb_ctx,
							depth, entry_index,
							1, false);
		if (unlikely(read_status < 0)) {
			DMCRIT("failure occurred reading entry %u depth %u",
			       entry_index, depth);
			return read_status;
		}
		/* Accrue return code flags */
		populated |= read_status;

		/* Attempt to pull in up to entry_readahead extra entries on
		 * this I/O call iff we're doing the read right now.  This
		 * helps optimize sequential access to the mapped drive.
		 */
		if (bht->entry_readahead &&
		    (read_status & DM_BHT_ENTRY_REQUESTED)) {
			unsigned int readahead_count;
			entry_index++;
			readahead_count = min(bht->entry_readahead,
					      level->count - entry_index);
			/* The result is completely ignored since this call is
			 * critical for the current request.
			 */
			if (readahead_count)
				dm_bht_maybe_read_entries(bht, read_cb_ctx,
							  depth, entry_index,
							  readahead_count,
							  true);
		}
	}

	/* All nodes are ready. The hash for the block_index can be verified */
	return populated;
}
EXPORT_SYMBOL(dm_bht_populate);


/**
 * dm_bht_verify_block - checks that all nodes in the path for @block are valid
 * @bht:	pointer to a dm_bht_create()d bht
 * @block_index:specific block data is expected from
 * @digest:	computed digest for the given block to be checked
 * @digest_len:	length of @digest
 *
 * Returns 0 on success, 1 on missing data, and a negative error
 * code on verification failure. All supporting functions called
 * should return similarly.
 */
int dm_bht_verify_block(struct dm_bht *bht, unsigned int block_index,
				u8 *digest, unsigned int digest_len)
{
	int unverified = 0;
	int entry_state = 0;

	/* TODO(wad) do we really need this? */
	if (digest_len != bht->digest_size) {
		DMERR("invalid digest_len passed to verify_block");
		return -EINVAL;
	}

	/* Make sure that the root has been verified */
	if (atomic_read(&bht->root_state) != DM_BHT_ENTRY_VERIFIED) {
		unverified = dm_bht_verify_root(bht, dm_bht_compare_hash);
		if (unverified) {
			DMERR_LIMIT("Failed to verify root: %d", unverified);
			return unverified;
		}
	}

	/* Now check that the digest supplied matches the leaf hash */
	unverified = dm_bht_check_block(bht, block_index, digest, &entry_state);
	if (unverified) {
		DMERR_LIMIT("Block check failed for %u: %d", block_index,
				unverified);
		return unverified;
	}

	/* If the entry which contains the block hash is marked verified, then
	 * it means that its hash has been check with the parent.  In addition,
	 * since that is only possible via verify_path, it transitively means
	 * it is verified to the root of the tree. If the depth is 1, then it
	 * means the entry was verified during root verification.
	 */
	if (entry_state == DM_BHT_ENTRY_VERIFIED || bht->depth == 1)
		return unverified;

	/* Now check levels in between */
	unverified = dm_bht_verify_path(bht,
					block_index,
					dm_bht_compare_hash);
	if (unverified)
		DMERR_LIMIT("Failed to verify intermediary nodes for block: %u (%d)",
		      block_index, unverified);
	return unverified;
}
EXPORT_SYMBOL(dm_bht_verify_block);

/**
 * dm_bht_destroy - cleans up all memory used by @bht
 * @bht:	pointer to a dm_bht_create()d bht
 *
 * Returns 0 on success. Does not free @bht itself.
 */
int dm_bht_destroy(struct dm_bht *bht)
{
	unsigned int depth;
	int cpu = 0;

	kfree(bht->root_digest);

	depth = bht->depth;
	while (depth-- != 0) {
		struct dm_bht_entry *entry = bht->levels[depth].entries;
		struct dm_bht_entry *entry_end = entry +
						 bht->levels[depth].count;
		int state = 0;
		for (; entry < entry_end; ++entry) {
			state = atomic_read(&entry->state);
			switch (state) {
			/* At present, no other states free memory,
			 * but that will change.
			 */
			case DM_BHT_ENTRY_UNALLOCATED:
				/* Allocated with improper state */
				BUG_ON(entry->nodes);
				continue;
			default:
				BUG_ON(!entry->nodes);
				mempool_free(entry->nodes, bht->entry_pool);
				break;
			}
		}
		kfree(bht->levels[depth].entries);
		bht->levels[depth].entries = NULL;
	}
	mempool_destroy(bht->entry_pool);
	kfree(bht->levels);
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
		if (bht->hash_desc[cpu].tfm)
			crypto_free_hash(bht->hash_desc[cpu].tfm);
	kfree(bht->hash_desc);
	return 0;
}
EXPORT_SYMBOL(dm_bht_destroy);

/*-----------------------------------------------
 * Accessors
 *-----------------------------------------------*/

/**
 * dm_bht_sectors - return the sectors required on disk
 * @bht:	pointer to a dm_bht_create()d bht
 */
sector_t dm_bht_sectors(const struct dm_bht *bht)
{
	return bht->sectors;
}
EXPORT_SYMBOL(dm_bht_sectors);

/**
 * dm_bht_set_read_cb - set read callback
 * @bht:	pointer to a dm_bht_create()d bht
 * @read_cb:	callback function used for all read requests by @bht
 */
void dm_bht_set_read_cb(struct dm_bht *bht, dm_bht_callback read_cb)
{
	bht->read_cb = read_cb;
}
EXPORT_SYMBOL(dm_bht_set_read_cb);

/**
 * dm_bht_set_write_cb - set write callback
 * @bht:	pointer to a dm_bht_create()d bht
 * @write_cb:	callback function used for all write requests by @bht
 */
void dm_bht_set_write_cb(struct dm_bht *bht, dm_bht_callback write_cb)
{
	bht->write_cb = write_cb;
}
EXPORT_SYMBOL(dm_bht_set_write_cb);

/**
 * dm_bht_set_verify_mode - set verify mode
 * @bht:	pointer to a dm_bht_create()d bht
 * @verify_mode:	indicate verification behavior
 */
void dm_bht_set_verify_mode(struct dm_bht *bht, int verify_mode)
{
	bht->verify_mode = verify_mode;
}
EXPORT_SYMBOL(dm_bht_set_verify_mode);

/**
 * dm_bht_set_entry_readahead - set verify mode
 * @bht:	pointer to a dm_bht_create()d bht
 * @readahead_count:	number of entries to readahead from a given level
 */
void dm_bht_set_entry_readahead(struct dm_bht *bht,
				unsigned int readahead_count)
{
	bht->entry_readahead = readahead_count;
}
EXPORT_SYMBOL(dm_bht_set_entry_readahead);

/**
 * dm_bht_set_root_hexdigest - sets an unverified root digest hash from hex
 * @bht:	pointer to a dm_bht_create()d bht
 * @hexdigest:	array of u8s containing the new digest in binary
 * Returns non-zero on error.  hexdigest should be NUL terminated.
 */
int dm_bht_set_root_hexdigest(struct dm_bht *bht, const u8 *hexdigest)
{
	if (!bht->root_digest) {
		DMCRIT("No allocation for root digest. Call dm_bht_create");
		return -1;
	}
	/* Make sure we have at least the bytes expected */
	if (strnlen((char *)hexdigest, bht->digest_size * 2) !=
	    bht->digest_size * 2) {
		DMERR("root digest length does not match hash algorithm");
		return -1;
	}
	dm_bht_hex_to_bin(bht->root_digest, hexdigest, bht->digest_size);
#ifdef CONFIG_DM_DEBUG
	DMINFO("Set root digest to %s. Parsed as -> ", hexdigest);
	dm_bht_log_mismatch(bht, bht->root_digest, bht->root_digest);
#endif
	/* Mark the root as unallocated to ensure that it transitions to
	 * requested just in case the levels aren't loaded at this point.
	 */
	atomic_set(&bht->root_state, DM_BHT_ENTRY_UNALLOCATED);
	return 0;
}
EXPORT_SYMBOL(dm_bht_set_root_hexdigest);

/**
 * dm_bht_root_hexdigest - returns root digest in hex
 * @bht:	pointer to a dm_bht_create()d bht
 * @hexdigest:	u8 array of size @available
 * @available:	must be bht->digest_size * 2 + 1
 */
int dm_bht_root_hexdigest(struct dm_bht *bht, u8 *hexdigest, int available)
{
	if (available < 0 ||
	    ((unsigned int) available) < bht->digest_size * 2 + 1) {
		DMERR("hexdigest has too few bytes available");
		return -EINVAL;
	}
	if (!bht->root_digest) {
		DMERR("no root digest exists to export");
		if (available > 0)
			*hexdigest = 0;
		return -1;
	}
	dm_bht_bin_to_hex(bht->root_digest, hexdigest, bht->digest_size);
	return 0;
}
EXPORT_SYMBOL(dm_bht_root_hexdigest);

