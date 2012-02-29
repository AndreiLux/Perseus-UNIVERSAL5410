/*
 * videobuf2-dma-contig.c - DMA contig memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>

struct vb2_dc_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	dma_addr_t			dma_addr;
	struct sg_table			*dma_sgt;
	enum dma_data_direction		dma_dir;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	atomic_t			refcount;
	struct dma_buf			*dma_buf;
	struct sg_table			*sgt_base;

	/* USERPTR related */
	struct vm_area_struct		*vma;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/

static struct sg_table *vb2_dc_pages_to_sgt(struct page **pages,
	unsigned long n_pages, size_t offset, size_t offset2)
{
	struct sg_table *sgt;
	int i, j; /* loop counters */
	int cur_page, chunks;
	int ret;
	struct scatterlist *s;

	sgt = kzalloc(sizeof *sgt, GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	/* compute number of chunks */
	chunks = 1;
	for (i = 1; i < n_pages; ++i)
		if (pages[i] != pages[i - 1] + 1)
			++chunks;

	ret = sg_alloc_table(sgt, chunks, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(-ENOMEM);
	}

	/* merging chunks and putting them into the scatterlist */
	cur_page = 0;
	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		size_t size = PAGE_SIZE;

		for (j = cur_page + 1; j < n_pages; ++j) {
			if (pages[j] != pages[j - 1] + 1)
				break;
			size += PAGE_SIZE;
		}

		/* cut offset if chunk starts at the first page */
		if (cur_page == 0)
			size -= offset;
		/* cut offset2 if chunk ends at the last page */
		if (j == n_pages)
			size -= offset2;

		sg_set_page(s, pages[cur_page], size, offset);
		offset = 0;
		cur_page = j;
	}

	return sgt;
}

static void vb2_dc_release_sgtable(struct sg_table *sgt)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static void vb2_dc_put_sgtable(struct sg_table *sgt, int dirty)
{
	struct scatterlist *s;
	int i, j;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		struct page *page = sg_page(s);
		int n_pages = PAGE_ALIGN(s->offset + s->length) >> PAGE_SHIFT;

		for (j = 0; j < n_pages; ++j, ++page) {
			if (dirty)
				set_page_dirty_lock(page);
			put_page(page);
		}
	}

	vb2_dc_release_sgtable(sgt);
}

static unsigned long vb2_dc_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	int i;
	unsigned long size = 0;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected = sg_dma_address(s) + sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static void *vb2_dc_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void *vb2_dc_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return buf->vaddr;
}

static unsigned int vb2_dc_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static void vb2_dc_prepare(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sg_for_device(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);
}

static void vb2_dc_finish(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sg_for_cpu(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);
}

/*********************************************/
/*        callbacks for MMAP buffers         */
/*********************************************/

static void vb2_dc_put(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (!atomic_dec_and_test(&buf->refcount))
		return;

	if (buf->dma_buf)
		dma_buf_put(buf->dma_buf);
	vb2_dc_release_sgtable(buf->sgt_base);
	dma_free_coherent(buf->dev, buf->size, buf->vaddr,
		buf->dma_addr);
	kfree(buf);
}

static void *vb2_dc_alloc(void *alloc_ctx, unsigned long size)
{
	struct device *dev = alloc_ctx;
	struct vb2_dc_buf *buf;
	int ret;
	int n_pages;
	struct page **pages = NULL;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = dev;
	buf->size = size;
	buf->vaddr = dma_alloc_coherent(buf->dev, buf->size, &buf->dma_addr,
		GFP_KERNEL);

	ret = -ENOMEM;
	if (!buf->vaddr) {
		dev_err(dev, "dma_alloc_coherent of size %ld failed\n",
			size);
		goto fail_buf;
	}

	WARN_ON((unsigned long)buf->vaddr & ~PAGE_MASK);
	WARN_ON(buf->dma_addr & ~PAGE_MASK);

	n_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;

	pages = kmalloc(n_pages * sizeof pages[0], GFP_KERNEL);
	if (!pages) {
		printk(KERN_ERR "failed to alloc page table\n");
		goto fail_dma;
	}

	ret = dma_get_pages(dev, buf->vaddr, buf->dma_addr, pages, n_pages);
	if (ret < 0) {
		printk(KERN_ERR "failed to get buffer pages from DMA API\n");
		goto fail_pages;
	}
	if (ret != n_pages) {
		ret = -EFAULT;
		printk(KERN_ERR "failed to get all pages from DMA API\n");
		goto fail_pages;
	}

	buf->sgt_base = vb2_dc_pages_to_sgt(pages, n_pages, 0, 0);
	if (IS_ERR(buf->sgt_base)) {
		ret = PTR_ERR(buf->sgt_base);
		printk(KERN_ERR "failed to prepare sg table\n");
		goto fail_pages;
	}

	/* pages are no longer needed */
	kfree(pages);

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_dc_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	return buf;

fail_pages:
	kfree(pages);

fail_dma:
	dma_free_coherent(buf->dev, buf->size, buf->vaddr, buf->dma_addr);

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}

static int vb2_dc_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dc_buf *buf = buf_priv;
	int ret;

	/*
	 * dma_mmap_* uses vm_pgoff as in-buffer offset, but we want to
	 * map whole buffer
	 */
	vma->vm_pgoff = 0;

	ret = dma_mmap_writecombine(buf->dev, vma, buf->vaddr,
		buf->dma_addr, buf->size);

	if (ret) {
		printk(KERN_ERR "Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vma->vm_flags		|= VM_DONTEXPAND | VM_RESERVED;
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	printk(KERN_DEBUG "%s: mapped dma addr 0x%08lx at 0x%08lx, size %ld\n",
		__func__, (unsigned long)buf->dma_addr, vma->vm_start,
		buf->size);

	return 0;
}

/*********************************************/
/*         DMABUF ops for exporters          */
/*********************************************/

struct vb2_dc_attachment {
	struct sg_table sgt;
	enum dma_data_direction dir;
};

static int vb2_dc_dmabuf_ops_attach(struct dma_buf *dbuf, struct device *dev,
	struct dma_buf_attachment *dbuf_attach)
{
	/* nothing to be done */
	return 0;
}

static void vb2_dc_dmabuf_ops_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *db_attach)
{
	struct vb2_dc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->nents, attach->dir);
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *vb2_dc_dmabuf_ops_map(
	struct dma_buf_attachment *db_attach, enum dma_data_direction dir)
{
	struct dma_buf *dbuf = db_attach->dmabuf;
	struct vb2_dc_buf *buf = dbuf->priv;
	struct vb2_dc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;
	struct scatterlist *rd, *wr;
	int i, ret;

	/* return previously mapped sg table */
	if (attach)
		return &attach->sgt;

	attach = kzalloc(sizeof *attach, GFP_KERNEL);
	if (!attach)
		return ERR_PTR(-ENOMEM);

	sgt = &attach->sgt;
	attach->dir = dir;

	/* copying the buf->base_sgt to attachment */
	ret = sg_alloc_table(sgt, buf->sgt_base->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return ERR_PTR(-ENOMEM);
	}

	rd = buf->sgt_base->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	/* mapping new sglist to the client */
	ret = dma_map_sg(db_attach->dev, sgt->sgl, sgt->orig_nents, dir);
	if (ret <= 0) {
		printk(KERN_ERR "failed to map scatterlist\n");
		sg_free_table(sgt);
		kfree(attach);
		return ERR_PTR(-EIO);
	}

	db_attach->priv = attach;

	return sgt;
}

static void vb2_dc_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
	struct sg_table *sgt)
{
	/* nothing to be done here */
}

static void vb2_dc_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* drop reference obtained in vb2_dc_get_dmabuf */
	vb2_dc_put(dbuf->priv);
}

static struct dma_buf_ops vb2_dc_dmabuf_ops = {
	.attach = vb2_dc_dmabuf_ops_attach,
	.detach = vb2_dc_dmabuf_ops_detach,
	.map_dma_buf = vb2_dc_dmabuf_ops_map,
	.unmap_dma_buf = vb2_dc_dmabuf_ops_unmap,
	.release = vb2_dc_dmabuf_ops_release,
};

static struct dma_buf *vb2_dc_get_dmabuf(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct dma_buf *dbuf;

	if (buf->dma_buf)
		return buf->dma_buf;

	/* dmabuf keeps reference to vb2 buffer */
	atomic_inc(&buf->refcount);
	dbuf = dma_buf_export(buf, &vb2_dc_dmabuf_ops, buf->size, 0);
	if (IS_ERR(dbuf)) {
		atomic_dec(&buf->refcount);
		return NULL;
	}

	buf->dma_buf = dbuf;

	return dbuf;
}

/*********************************************/
/*       callbacks for USERPTR buffers       */
/*********************************************/

static inline int vma_is_io(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & (VM_IO | VM_PFNMAP));
}

static int vb2_dc_get_pages(unsigned long start, struct page **pages,
	int n_pages, struct vm_area_struct **copy_vma, int write)
{
	struct vm_area_struct *vma;
	int n = 0; /* number of get pages */
	int ret = -EFAULT;

	/* entering critical section for mm access */
	down_read(&current->mm->mmap_sem);

	vma = find_vma(current->mm, start);
	if (!vma) {
		printk(KERN_ERR "no vma for address %lu\n", start);
		goto cleanup;
	}

	if (vma_is_io(vma)) {
		unsigned long pfn;

		if (vma->vm_end - start < n_pages * PAGE_SIZE) {
			printk(KERN_ERR "vma is too small\n");
			goto cleanup;
		}

		for (n = 0; n < n_pages; ++n, start += PAGE_SIZE) {
			ret = follow_pfn(vma, start, &pfn);
			if (ret) {
				printk(KERN_ERR "no page for address %lu\n",
					start);
				goto cleanup;
			}
			pages[n] = pfn_to_page(pfn);
			get_page(pages[n]);
		}
	} else {
		n = get_user_pages(current, current->mm, start & PAGE_MASK,
			n_pages, write, 1, pages, NULL);
		if (n != n_pages) {
			printk(KERN_ERR "got only %d of %d user pages\n",
				n, n_pages);
			goto cleanup;
		}
	}

	*copy_vma = vb2_get_vma(vma);
	if (!*copy_vma) {
		printk(KERN_ERR "failed to copy vma\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	/* leaving critical section for mm access */
	up_read(&current->mm->mmap_sem);

	return 0;

cleanup:
	up_read(&current->mm->mmap_sem);

	/* putting user pages if used, can be done wothout the lock */
	while (n)
		put_page(pages[--n]);

	return ret;
}

static void vb2_dc_put_userptr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	dma_unmap_sg(buf->dev, sgt->sgl, sgt->orig_nents, buf->dma_dir);
	vb2_dc_put_sgtable(sgt, !vma_is_io(buf->vma));
	vb2_put_vma(buf->vma);
	kfree(buf);
}

static void *vb2_dc_get_userptr(void *alloc_ctx, unsigned long vaddr,
	unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	unsigned long start, end, offset, offset2;
	struct page **pages;
	int n_pages;
	int ret = 0;
	struct sg_table *sgt;
	unsigned long contig_size;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = alloc_ctx;
	buf->dma_dir = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	start = (unsigned long)vaddr & PAGE_MASK;
	offset = (unsigned long)vaddr & ~PAGE_MASK;
	end = PAGE_ALIGN((unsigned long)vaddr + size);
	offset2 = end - (unsigned long)vaddr - size;
	n_pages = (end - start) >> PAGE_SHIFT;

	pages = kmalloc(n_pages * sizeof pages[0], GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		printk(KERN_ERR "failed to allocate pages table\n");
		goto fail_buf;
	}

	/* extract page list from userspace mapping */
	ret = vb2_dc_get_pages(start, pages, n_pages, &buf->vma, write);
	if (ret) {
		printk(KERN_ERR "failed to get user pages\n");
		goto fail_pages;
	}

	sgt = vb2_dc_pages_to_sgt(pages, n_pages, offset, offset2);
	if (!sgt) {
		printk(KERN_ERR "failed to create scatterlist table\n");
		ret = -ENOMEM;
		goto fail_get_pages;
	}

	/* pages are no longer needed */
	kfree(pages);
	pages = NULL;

	sgt->nents = dma_map_sg(buf->dev, sgt->sgl, sgt->orig_nents,
		buf->dma_dir);
	if (sgt->nents <= 0) {
		printk(KERN_ERR "failed to map scatterlist\n");
		ret = -EIO;
		goto fail_sgt;
	}

	contig_size = vb2_dc_get_contiguous_size(sgt);
	if (contig_size < size) {
		printk(KERN_ERR "contiguous mapping is too small %lu/%lu\n",
			contig_size, size);
		ret = -EFAULT;
		goto fail_map_sg;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->size = size;
	buf->dma_sgt = sgt;

	atomic_inc(&buf->refcount);

	return buf;

fail_map_sg:
	dma_unmap_sg(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);

fail_sgt:
	vb2_dc_put_sgtable(sgt, 0);

fail_get_pages:
	while (pages && n_pages)
		put_page(pages[--n_pages]);
	vb2_put_vma(buf->vma);

fail_pages:
	kfree(pages); /* kfree is NULL-proof */

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}

/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/

static int vb2_dc_map_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;
	struct sg_table *sgt;
	unsigned long contig_size;
	int ret = 0;

	if (WARN_ON(!buf->db_attach)) {
		printk(KERN_ERR "trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		printk(KERN_ERR "dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
	if (IS_ERR_OR_NULL(sgt)) {
		printk(KERN_ERR "Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/* checking if dmabuf is big enough to store contiguous chunk */
	contig_size = vb2_dc_get_contiguous_size(sgt);
	if (contig_size < buf->size) {
		printk(KERN_ERR "contiguous chunk of dmabuf is too small\n");
		ret = -EFAULT;
		goto fail_map;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;

	return 0;

fail_map:
	dma_buf_unmap_attachment(buf->db_attach, sgt);

	return ret;
}

static void vb2_dc_unmap_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (WARN_ON(!buf->db_attach)) {
		printk(KERN_ERR "trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		printk(KERN_ERR "dmabuf buffer is already unpinned\n");
		return;
	}

	dma_buf_unmap_attachment(buf->db_attach, sgt);

	buf->dma_addr = 0;
	buf->dma_sgt = NULL;
}

static void vb2_dc_detach_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	if (buf->dma_addr)
		vb2_dc_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static void *vb2_dc_attach_dmabuf(void *alloc_ctx, struct dma_buf *dbuf,
	unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	struct dma_buf_attachment *dba;

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = alloc_ctx;
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		printk(KERN_ERR "failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	buf->dma_dir = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf->size = size;
	buf->db_attach = dba;

	return buf;
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vb2_mem_ops vb2_dma_contig_memops = {
	.alloc		= vb2_dc_alloc,
	.put		= vb2_dc_put,
	.get_dmabuf	= vb2_dc_get_dmabuf,
	.cookie		= vb2_dc_cookie,
	.vaddr		= vb2_dc_vaddr,
	.mmap		= vb2_dc_mmap,
	.get_userptr	= vb2_dc_get_userptr,
	.put_userptr	= vb2_dc_put_userptr,
	.prepare	= vb2_dc_prepare,
	.finish		= vb2_dc_finish,
	.map_dmabuf	= vb2_dc_map_dmabuf,
	.unmap_dmabuf	= vb2_dc_unmap_dmabuf,
	.attach_dmabuf	= vb2_dc_attach_dmabuf,
	.detach_dmabuf	= vb2_dc_detach_dmabuf,
	.num_users	= vb2_dc_num_users,
};
EXPORT_SYMBOL_GPL(vb2_dma_contig_memops);

void *vb2_dma_contig_init_ctx(struct device *dev)
{
	return dev;
}
EXPORT_SYMBOL_GPL(vb2_dma_contig_init_ctx);

void vb2_dma_contig_cleanup_ctx(void *alloc_ctx)
{
}
EXPORT_SYMBOL_GPL(vb2_dma_contig_cleanup_ctx);

MODULE_DESCRIPTION("DMA-contig memory handling routines for videobuf2");
MODULE_AUTHOR("Pawel Osciak <pawel@osciak.com>");
MODULE_LICENSE("GPL");
