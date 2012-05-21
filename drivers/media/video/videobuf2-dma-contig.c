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
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>

struct vb2_dc_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	dma_addr_t			dma_addr;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	atomic_t			refcount;

	/* USERPTR related */
	struct vm_area_struct		*vma;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/

static struct sg_table *vb2_dc_pages_to_sgt(struct page **pages,
	unsigned int n_pages, unsigned long offset, unsigned long size)
{
	struct sg_table *sgt;
	unsigned int chunks;
	unsigned int i;
	unsigned int cur_page;
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
		unsigned long chunk_size;
		unsigned int j;

		for (j = cur_page + 1; j < n_pages; ++j)
			if (pages[j] != pages[j - 1] + 1)
				break;

		chunk_size = ((j - cur_page) << PAGE_SHIFT) - offset;
		sg_set_page(s, pages[cur_page], min(size, chunk_size), offset);
		size -= chunk_size;
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

static void vb2_dc_sgt_foreach_page(struct sg_table *sgt,
	void (*cb)(struct page *pg))
{
	struct scatterlist *s;
	unsigned int i;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		struct page *page = sg_page(s);
		unsigned int n_pages = PAGE_ALIGN(s->offset + s->length)
			>> PAGE_SHIFT;
		unsigned int j;

		for (j = 0; j < n_pages; ++j, ++page)
			cb(page);
	}
}

static unsigned long vb2_dc_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
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
	if (!buf)
		return NULL;

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

	dma_free_coherent(buf->dev, buf->size, buf->vaddr, buf->dma_addr);
	kfree(buf);
}

static void *vb2_dc_alloc(void *alloc_ctx, unsigned long size)
{
	struct device *dev = alloc_ctx;
	struct vb2_dc_buf *buf;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->vaddr = dma_alloc_coherent(dev, size, &buf->dma_addr, GFP_KERNEL);
	if (!buf->vaddr) {
		dev_err(dev, "dma_alloc_coherent of size %ld failed\n", size);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->dev = dev;
	buf->size = size;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_dc_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	return buf;
}

static int vb2_dc_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (!buf) {
		printk(KERN_ERR "No buffer to map\n");
		return -EINVAL;
	}

	return vb2_mmap_pfn_range(vma, buf->dma_addr, buf->size,
				  &vb2_common_vm_ops, &buf->handler);
}

/*********************************************/
/*       callbacks for USERPTR buffers       */
/*********************************************/

static inline int vma_is_io(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & (VM_IO | VM_PFNMAP));
}

static struct vm_area_struct *vb2_dc_get_user_vma(
	unsigned long start, unsigned long size)
{
	struct vm_area_struct *vma;

	/* current->mm->mmap_sem is taken by videobuf2 core */
	vma = find_vma(current->mm, start);
	if (!vma) {
		printk(KERN_ERR "no vma for address %lu\n", start);
		return ERR_PTR(-EFAULT);
	}

	if (vma->vm_end - vma->vm_start < size) {
		printk(KERN_ERR "vma at %lu is too small for %lu bytes\n",
			start, size);
		return ERR_PTR(-EFAULT);
	}

	vma = vb2_get_vma(vma);
	if (!vma) {
		printk(KERN_ERR "failed to copy vma\n");
		return ERR_PTR(-ENOMEM);
	}

	return vma;
}

static int vb2_dc_get_user_pages(unsigned long start, struct page **pages,
	int n_pages, struct vm_area_struct *vma, int write)
{
	if (vma->vm_mm == NULL)
		vma->vm_mm = current->mm;

	if (vma_is_io(vma)) {
		unsigned int i;

		for (i = 0; i < n_pages; ++i, start += PAGE_SIZE) {
			unsigned long pfn;
			int ret = follow_pfn(vma, start, &pfn);

			if (ret) {
				printk(KERN_ERR "no page for address %lu\n",
					start);
				return ret;
			}
			pages[i] = pfn_to_page(pfn);
		}
	} else {
		unsigned int n;

		n = get_user_pages(current, current->mm, start & PAGE_MASK,
			n_pages, write, 1, pages, NULL);
		if (n != n_pages) {
			printk(KERN_ERR "got only %d of %d user pages\n",
				n, n_pages);
			while (n)
				put_page(pages[--n]);
			return -EFAULT;
		}
	}

	return 0;
}

static void vb2_dc_put_dirty_page(struct page *page)
{
	set_page_dirty_lock(page);
	put_page(page);
}

static void vb2_dc_put_userptr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	dma_unmap_sg(buf->dev, sgt->sgl, sgt->orig_nents, buf->dma_dir);
	if (!vma_is_io(buf->vma))
		vb2_dc_sgt_foreach_page(sgt, vb2_dc_put_dirty_page);

	vb2_dc_release_sgtable(sgt);
	vb2_put_vma(buf->vma);
	kfree(buf);
}

static void *vb2_dc_get_userptr(void *alloc_ctx, unsigned long vaddr,
	unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	unsigned long start;
	unsigned long end;
	unsigned long offset;
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

	start = vaddr & PAGE_MASK;
	offset = vaddr & ~PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);
	n_pages = (end - start) >> PAGE_SHIFT;

	pages = kmalloc(n_pages * sizeof pages[0], GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		printk(KERN_ERR "failed to allocate pages table\n");
		goto fail_buf;
	}

	buf->vma = vb2_dc_get_user_vma(start, size);
	if (IS_ERR(buf->vma)) {
		printk(KERN_ERR "failed to get VMA\n");
		ret = PTR_ERR(buf->vma);
		goto fail_pages;
	}

	/* extract page list from userspace mapping */
	ret = vb2_dc_get_user_pages(start, pages, n_pages, buf->vma, write);
	if (ret) {
		printk(KERN_ERR "failed to get user pages\n");
		goto fail_vma;
	}

	sgt = vb2_dc_pages_to_sgt(pages, n_pages, offset, size);
	if (IS_ERR(sgt)) {
		printk(KERN_ERR "failed to create scatterlist table\n");
		ret = -ENOMEM;
		goto fail_get_user_pages;
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

	return buf;

fail_map_sg:
	dma_unmap_sg(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);

fail_sgt:
	if (!vma_is_io(buf->vma))
		vb2_dc_sgt_foreach_page(sgt, put_page);
	vb2_dc_release_sgtable(sgt);

fail_get_user_pages:
	if (pages && !vma_is_io(buf->vma))
		while (n_pages)
			put_page(pages[--n_pages]);

fail_vma:
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
		printk(KERN_ERR "contiguous chunk is too small %lu/%lu b\n",
			contig_size, buf->size);
		dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
		return -EFAULT;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;

	return 0;
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

	dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);

	buf->dma_addr = 0;
	buf->dma_sgt = NULL;
}

static void vb2_dc_detach_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
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
