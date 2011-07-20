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
#include <linux/mm_types.h>
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
 * dm_bht_compute_hash: hashes a page of data
 */
static int dm_bht_compute_hash(struct dm_bht *bht, struct page *pg,
			       unsigned int offset, u8 *digest)
{
	struct hash_desc *hash_desc = &bht->hash_desc[smp_processor_id()];
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, pg, PAGE_SIZE, offset);
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

	return 0;
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

static inline struct dm_bht_entry *dm_bht_get_entry(struct dm_bht *bht,
						    unsigned int depth,
						    unsigned int block_index)
{
	unsigned int index = dm_bht_index_at_level(bht, depth, block_index);
	struct dm_bht_level *level = dm_bht_get_level(bht, depth);

	BUG_ON(index >= level->count);

	return &level->entries[index];
}

static inline u8 *dm_bht_get_node(struct dm_bht *bht,
				  struct dm_bht_entry *entry,
				  unsigned int depth,
				  unsigned int block_index)
{
	unsigned int index = dm_bht_index_at_level(bht, depth, block_index);

	return dm_bht_node(bht, entry, index % bht->node_count);
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
int dm_bht_create(struct dm_bht *bht, unsigned int block_count,
		  const char *alg_name)
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

	bht->root_digest = (u8 *) kzalloc(bht->digest_size, GFP_KERNEL);
	if (!bht->root_digest) {
		DMERR("failed to allocate memory for root digest");
		status = -ENOMEM;
		goto bad_root_digest_alloc;
	}

	/* Configure the tree */
	bht->block_count = block_count;
	DMDEBUG("Setting block_count %u", block_count);
	if (block_count == 0) {
		DMERR("block_count must be non-zero");
		status = -EINVAL;
		goto bad_block_count;
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

	/* This is unlikely to happen, but with 64k pages, who knows. */
	if (bht->node_count > UINT_MAX / bht->digest_size) {
		DMERR("node_count * hash_len exceeds UINT_MAX!");
		status = -EINVAL;
		goto bad_node_count;
	}

	bht->depth = DIV_ROUND_UP(fls(block_count - 1), bht->node_count_shift);
	DMDEBUG("Setting depth to %u.", bht->depth);

	/* Ensure that we can safely shift by this value. */
	if (bht->depth * bht->node_count_shift >= sizeof(unsigned int) * 8) {
		DMERR("specified depth and node_count_shift is too large");
		status = -EINVAL;
		goto bad_node_count;
	}

	/* Allocate levels. Each level of the tree may have an arbitrary number
	 * of dm_bht_entry structs.  Each entry contains node_count nodes.
	 * Each node in the tree is a cryptographic digest of either node_count
	 * nodes on the subsequent level or of a specific block on disk.
	 */
	bht->levels = (struct dm_bht_level *)
			kcalloc(bht->depth,
				sizeof(struct dm_bht_level), GFP_KERNEL);
	if (!bht->levels) {
		DMERR("failed to allocate tree levels");
		status = -ENOMEM;
		goto bad_level_alloc;
	}

	/* Setup callback stubs */
	bht->read_cb = &dm_bht_read_callback_stub;
	bht->write_cb = &dm_bht_write_callback_stub;

	status = dm_bht_initialize_entries(bht);
	if (status)
		goto bad_entries_alloc;

	/* We compute depth such that there is only be 1 block at level 0. */
	BUG_ON(bht->levels[0].count != 1);

	return 0;

bad_entries_alloc:
	while (bht->depth-- > 0)
		kfree(bht->levels[bht->depth].entries);
	kfree(bht->levels);
bad_node_count:
bad_level_alloc:
bad_block_count:
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
		level->entries = (struct dm_bht_entry *)
				 kcalloc(level->count,
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
	if (status) {
		/* TODO(wad) add retry support */
		DMCRIT("an I/O error occurred while reading entry");
		atomic_set(&entry->state, DM_BHT_ENTRY_ERROR_IO);
		/* entry->nodes will be freed later */
		return;
	}
	BUG_ON(atomic_read(&entry->state) != DM_BHT_ENTRY_PENDING);
	atomic_set(&entry->state, DM_BHT_ENTRY_READY);
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

/* dm_bht_verify_path
 * Verifies the path. Returns 0 on ok.
 */
static int dm_bht_verify_path(struct dm_bht *bht, unsigned int block_index,
			      struct page *pg, unsigned int offset)
{
	unsigned int depth = bht->depth;
	u8 digest[DM_BHT_MAX_DIGEST_SIZE];
	struct dm_bht_entry *entry;
	u8 *node;
	int state;

	do {
		/* Need to check that the hash of the current block is accurate
		 * in its parent.
		 */
		entry = dm_bht_get_entry(bht, depth - 1, block_index);
		state = atomic_read(&entry->state);
		/* This call is only safe if all nodes along the path
		 * are already populated (i.e. READY) via dm_bht_populate.
		 */
		BUG_ON(state < DM_BHT_ENTRY_READY);
		node = dm_bht_get_node(bht, entry, depth, block_index);

		if (dm_bht_compute_hash(bht, pg, offset, digest) ||
		    memcmp(digest, node, bht->digest_size))
			goto mismatch;

		/* Keep the containing block of hashes to be verified in the
		 * next pass.
		 */
		pg = virt_to_page(entry->nodes);
		offset = 0;
	} while (--depth > 0 && state != DM_BHT_ENTRY_VERIFIED);

	if (depth == 0 && state != DM_BHT_ENTRY_VERIFIED) {
		if (dm_bht_compute_hash(bht, pg, offset, digest) ||
		    memcmp(digest, bht->root_digest, bht->digest_size))
			goto mismatch;
		atomic_set(&entry->state, DM_BHT_ENTRY_VERIFIED);
	}

	/* Mark path to leaf as verified. */
	for (depth++; depth < bht->depth; depth++) {
		entry = dm_bht_get_entry(bht, depth, block_index);
		/* At this point, entry can only be in VERIFIED or READY state.
		 * So it is safe to use atomic_set instead of atomic_cmpxchg.
		 */
		atomic_set(&entry->state, DM_BHT_ENTRY_VERIFIED);
	}

	DMDEBUG("verify_path: node %u is verified to root", block_index);
	return 0;

mismatch:
	DMERR_LIMIT("verify_path: failed to verify hash (d=%u,bi=%u)",
		    depth, block_index);
	dm_bht_log_mismatch(bht, node, digest);
	return DM_BHT_ENTRY_ERROR_MISMATCH;
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
		memset(entry->nodes, 0, PAGE_SIZE);
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

	dm_bht_compute_hash(bht, virt_to_page(block_data), 0,
			    dm_bht_node(bht, entry, node_index));
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
	int depth, r = 0;

	for (depth = bht->depth - 2; depth >= 0; depth--) {
		struct dm_bht_level *level = dm_bht_get_level(bht, depth);
		struct dm_bht_level *child_level = level + 1;
		struct dm_bht_entry *entry = level->entries;
		struct dm_bht_entry *child = child_level->entries;
		unsigned int i, j;

		for (i = 0; i < level->count; i++, entry++) {
			unsigned int count = bht->node_count;
			struct page *pg;

			pg = (struct page *) mempool_alloc(bht->entry_pool,
							   GFP_NOIO);
			if (!pg) {
				DMCRIT("an error occurred while reading entry");
				goto out;
			}

			entry->nodes = page_address(pg);
			memset(entry->nodes, 0, PAGE_SIZE);
			atomic_set(&entry->state, DM_BHT_ENTRY_READY);

			if (i == (level->count - 1))
				count = child_level->count % bht->node_count;
			if (count == 0)
				count = bht->node_count;
			for (j = 0; j < count; j++, child++) {
				struct page *pg = virt_to_page(child->nodes);
				u8 *digest = dm_bht_node(bht, entry, j);

				r = dm_bht_compute_hash(bht, pg, 0, digest);
				if (r) {
					DMERR("Failed to update (d=%u,i=%u)",
					      depth, i);
					goto out;
				}
			}
		}
	}
	r = dm_bht_compute_hash(bht,
				virt_to_page(bht->levels[0].entries->nodes),
				0, bht->root_digest);
	if (r)
		DMERR("Failed to update root hash");

out:
	return r;
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
 * dm_bht_is_populated - check that entries from disk needed to verify a given
 *                       block are all ready
 * @bht:	pointer to a dm_bht_create()d bht
 * @block_index:specific block data is expected from
 *
 * Callers may wish to call dm_bht_is_populated() when checking an io
 * for which entries were already pending.
 */
bool dm_bht_is_populated(struct dm_bht *bht, unsigned int block_index)
{
	unsigned int depth;

	/* TODO(msb) convert depth to int and avoid ugly cast */
	for (depth = bht->depth - 1; (int)depth >= 0; depth--) {
		struct dm_bht_entry *entry = dm_bht_get_entry(bht, depth,
							      block_index);
		if (atomic_read(&entry->state) < DM_BHT_ENTRY_READY)
			return false;
	}

	return true;
}
EXPORT_SYMBOL(dm_bht_is_populated);

/**
 * dm_bht_populate - reads entries from disk needed to verify a given block
 * @bht:	pointer to a dm_bht_create()d bht
 * @ctx:        context used for all read_cb calls on this request
 * @block_index:specific block data is expected from
 *
 * Returns negative value on error. Returns 0 on success.
 */
int dm_bht_populate(struct dm_bht *bht, void *ctx,
		    unsigned int block_index)
{
	unsigned int depth;
	int state = 0;

	BUG_ON(block_index >= bht->block_count);

	DMDEBUG("dm_bht_populate(%u)", block_index);

	for (depth = 0; depth < bht->depth; ++depth) {
		struct dm_bht_level *level;
		struct dm_bht_entry *entry;
		unsigned int index;
		struct page *pg;

		entry = dm_bht_get_entry(bht, depth, block_index);
		state = atomic_cmpxchg(&entry->state,
				       DM_BHT_ENTRY_UNALLOCATED,
				       DM_BHT_ENTRY_PENDING);

		if (state <= DM_BHT_ENTRY_ERROR)
			goto error_state;

		if (state != DM_BHT_ENTRY_UNALLOCATED)
			continue;

		/* Current entry is claimed for allocation and loading */
		pg = (struct page *) mempool_alloc(bht->entry_pool, GFP_NOIO);
		if (!pg)
			goto nomem;

		/* dm-bht guarantees page-aligned memory for callbacks. */
		entry->nodes = page_address(pg);

		/* TODO(wad) error check callback here too */

		level = &bht->levels[depth];
		index = dm_bht_index_at_level(bht, depth, block_index);
		bht->read_cb(ctx, level->sector + to_sector(index * PAGE_SIZE),
			     entry->nodes, to_sector(PAGE_SIZE), entry);
	}

	return 0;

error_state:
	DMCRIT("block %u at depth %u is in an error state", block_index, depth);
	return state;

nomem:
	DMCRIT("failed to allocate memory for entry->nodes from pool");
	return -ENOMEM;
}
EXPORT_SYMBOL(dm_bht_populate);


/**
 * dm_bht_verify_block - checks that all nodes in the path for @block are valid
 * @bht:	pointer to a dm_bht_create()d bht
 * @block_index:specific block data is expected from
 * @block:	virtual address of the block data in memory
 *              (must be aligned to block size)
 *
 * Returns 0 on success, 1 on missing data, and a negative error
 * code on verification failure. All supporting functions called
 * should return similarly.
 */
int dm_bht_verify_block(struct dm_bht *bht, unsigned int block_index,
			struct page *pg, unsigned int offset)
{
	BUG_ON(offset != 0);

	return  dm_bht_verify_path(bht, block_index, pg, offset);
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
				mempool_free(virt_to_page(entry->nodes),
					     bht->entry_pool);
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
