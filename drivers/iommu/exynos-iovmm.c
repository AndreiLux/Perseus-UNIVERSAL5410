/* linux/arch/arm/plat-s5p/s5p_iovmm.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_EXYNOS_IOMMU_DEBUG
#define DEBUG
#endif

#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/genalloc.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <plat/iovmm.h>

struct s5p_vm_region {
	struct list_head node;
	dma_addr_t start;
	size_t size;
};

struct s5p_iovmm {
	struct list_head node;		/* element of s5p_iovmm_list */
	struct iommu_domain *domain;
	struct device *dev;
	struct gen_pool *vmm_pool;
	struct list_head regions_list;	/* list of s5p_vm_region */
	atomic_t activations;
	spinlock_t lock;
};

static DEFINE_RWLOCK(iovmm_list_lock);
static LIST_HEAD(s5p_iovmm_list);

static struct s5p_iovmm *find_iovmm(struct device *dev)
{
	struct list_head *pos;
	struct s5p_iovmm *vmm = NULL;

	read_lock(&iovmm_list_lock);
	list_for_each(pos, &s5p_iovmm_list) {
		vmm = list_entry(pos, struct s5p_iovmm, node);
		if (vmm->dev == dev)
			break;
	}
	read_unlock(&iovmm_list_lock);
	return vmm;
}

static struct s5p_vm_region *find_region(struct s5p_iovmm *vmm, dma_addr_t iova)
{
	struct list_head *pos;
	struct s5p_vm_region *region;

	list_for_each(pos, &vmm->regions_list) {
		region = list_entry(pos, struct s5p_vm_region, node);
		if (region->start == iova)
			return region;
	}
	return NULL;
}

int iovmm_setup(struct device *dev)
{
	struct s5p_iovmm *vmm;
	int ret;

	vmm = kzalloc(sizeof(*vmm), GFP_KERNEL);
	if (!vmm) {
		ret = -ENOMEM;
		goto err_setup_alloc;
	}

	vmm->vmm_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!vmm->vmm_pool) {
		ret = -ENOMEM;
		goto err_setup_genalloc;
	}

	/* 1GB addr space from 0x80000000 */
	ret = gen_pool_add(vmm->vmm_pool, 0x80000000, 0x40000000, -1);
	if (ret)
		goto err_setup_domain;

	vmm->domain = iommu_domain_alloc(&platform_bus_type);
	if (!vmm->domain) {
		ret = -ENOMEM;
		goto err_setup_domain;
	}

	vmm->dev = dev;

	spin_lock_init(&vmm->lock);

	INIT_LIST_HEAD(&vmm->node);
	INIT_LIST_HEAD(&vmm->regions_list);
	atomic_set(&vmm->activations, 0);

	write_lock(&iovmm_list_lock);
	list_add(&vmm->node, &s5p_iovmm_list);
	write_unlock(&iovmm_list_lock);

	dev_dbg(dev, "IOVMM: Created 1GB IOVMM from 0x80000000.\n");

	return 0;
err_setup_domain:
	gen_pool_destroy(vmm->vmm_pool);
err_setup_genalloc:
	kfree(vmm);
err_setup_alloc:
	dev_dbg(dev, "IOVMM: Failed to create IOVMM (%d)\n", ret);
	return ret;
}

void iovmm_cleanup(struct device *dev)
{
	struct s5p_iovmm *vmm;

	vmm = find_iovmm(dev);

	WARN_ON(!vmm);
	if (vmm) {
		struct list_head *pos, *tmp;

		while (atomic_dec_return(&vmm->activations) > 0)
			iommu_detach_device(vmm->domain, dev);

		iommu_domain_free(vmm->domain);

		list_for_each_safe(pos, tmp, &vmm->regions_list) {
			struct s5p_vm_region *region;

			region = list_entry(pos, struct s5p_vm_region, node);

			/* No need to unmap the region because
			 * iommu_domain_free() frees the page table */
			gen_pool_free(vmm->vmm_pool, region->start,
								region->size);

			kfree(list_entry(pos, struct s5p_vm_region, node));
		}

		gen_pool_destroy(vmm->vmm_pool);

		write_lock(&iovmm_list_lock);
		list_del(&vmm->node);
		write_unlock(&iovmm_list_lock);

		kfree(vmm);

		dev_dbg(dev, "IOVMM: Removed IOVMM\n");
	}
}

int iovmm_activate(struct device *dev)
{
	struct s5p_iovmm *vmm;
	int ret = 0;

	vmm = find_iovmm(dev);
	if (WARN_ON(!vmm))
		return -EINVAL;

	ret = iommu_attach_device(vmm->domain, vmm->dev);
	if (!ret)
		atomic_inc(&vmm->activations);

	return ret;
}

void iovmm_deactivate(struct device *dev)
{
	struct s5p_iovmm *vmm;

	vmm = find_iovmm(dev);
	if (WARN_ON(!vmm))
		return;

	iommu_detach_device(vmm->domain, vmm->dev);

	atomic_add_unless(&vmm->activations, -1, 0);
}

dma_addr_t iovmm_map(struct device *dev, struct scatterlist *sg, off_t offset,
								size_t size)
{
	off_t start_off;
	dma_addr_t addr, start = 0;
	size_t mapped_size = 0;
	struct s5p_vm_region *region;
	struct s5p_iovmm *vmm;
	int order;
	unsigned long flags;
#ifdef CONFIG_EXYNOS_IOVMM_ALIGN64K
	size_t iova_size = 0;
#endif

	BUG_ON(!sg);

	vmm = find_iovmm(dev);
	if (WARN_ON(!vmm))
		goto err_map_nomem;

	for (; sg_dma_len(sg) < offset; sg = sg_next(sg))
		offset -= sg_dma_len(sg);

	spin_lock_irqsave(&vmm->lock, flags);

	start_off = offset_in_page(sg_phys(sg) + offset);
	size = PAGE_ALIGN(size + start_off);

	order = __fls(min_t(size_t, size, SZ_1M));
#ifdef CONFIG_EXYNOS_IOVMM_ALIGN64K
	iova_size = ALIGN(size, SZ_64K);
	start = (dma_addr_t)gen_pool_alloc_aligned(vmm->vmm_pool, iova_size,
									order);
#else
	start = (dma_addr_t)gen_pool_alloc_aligned(vmm->vmm_pool, size, order);
#endif
	if (!start)
		goto err_map_nomem_lock;

	addr = start;
	do {
		phys_addr_t phys;
		size_t len;

		phys = sg_phys(sg);
		len = sg_dma_len(sg);

		if (offset > 0) {
			len -= offset;
			phys += offset;
			offset = 0;
		}

		if (offset_in_page(phys)) {
			len += offset_in_page(phys);
			phys = round_down(phys, PAGE_SIZE);
		}

		len = PAGE_ALIGN(len);

		if (len > (size - mapped_size))
			len = size - mapped_size;

		if (iommu_map(vmm->domain, addr, phys, len, 0))
			break;

		addr += len;
		mapped_size += len;
	} while ((sg = sg_next(sg)) && (mapped_size < size));

	BUG_ON(mapped_size > size);

	if (mapped_size < size)
		goto err_map_map;

#ifdef CONFIG_EXYNOS_IOVMM_ALIGN64K
	if (iova_size != size) {
		addr = start + size;
		size = iova_size;

		for (; addr < start + size; addr += PAGE_SIZE) {
			if (iommu_map(vmm->domain, addr,
				page_to_phys(ZERO_PAGE(0)), PAGE_SIZE, 0))
				goto err_map_map;

			mapped_size += PAGE_SIZE;
		}
	}
#endif

	region = kmalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		goto err_map_map;

	region->start = start + start_off;
	region->size = size;
	INIT_LIST_HEAD(&region->node);

	list_add(&region->node, &vmm->regions_list);

	spin_unlock_irqrestore(&vmm->lock, flags);

	dev_dbg(dev, "IOVMM: Allocated VM region @ %#x/%#X bytes.\n",
					region->start, region->size);
	return region->start;
err_map_map:
	iommu_unmap(vmm->domain, start, mapped_size);
	gen_pool_free(vmm->vmm_pool, start, size);
err_map_nomem_lock:
	spin_unlock_irqrestore(&vmm->lock, flags);
	kfree(region);
err_map_nomem:
	dev_dbg(dev, "IOVMM: Failed to allocated VM region for %#x bytes.\n",
									size);
	return (dma_addr_t)0;
}

void iovmm_unmap(struct device *dev, dma_addr_t iova)
{
	struct s5p_vm_region *region;
	struct s5p_iovmm *vmm;
	unsigned long flags;
	size_t unmapped_size;

	vmm = find_iovmm(dev);

	if (WARN_ON(!vmm))
		return;

	spin_lock_irqsave(&vmm->lock, flags);

	region = find_region(vmm, iova);
	if (WARN_ON(!region))
		goto err_region_not_found;

	region->start = round_down(region->start, PAGE_SIZE);

	gen_pool_free(vmm->vmm_pool, region->start, region->size);
	list_del(&region->node);

	unmapped_size = iommu_unmap(vmm->domain, region->start, region->size);

	if (unmapped_size != region->size) {
		dev_err(dev, "IOVMM: Unmapped %#x bytes from %#x/%#x bytes.\n",
				unmapped_size, region->start, region->size);
	} else {
		dev_dbg(dev, "IOVMM: Unmapped %#x bytes from %#x.\n",
				region->start, region->size);
	}

	kfree(region);
err_region_not_found:
	spin_unlock_irqrestore(&vmm->lock, flags);
}
