/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Data structure definition for Exynos IOMMU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/iommu.h>

#include <plat/sysmmu.h>
#include <plat/iovmm.h>

#include <mach/sysmmu.h>

#ifdef CONFIG_EXYNOS_IOVMM

#ifdef CONFIG_SOC_EXYNOS5410
#define IOVA_START 0x80000000
#define IOVM_SIZE (SZ_2G - SZ_4K) /* last 4K is for error values */
#else
#define IOVA_START 0x10000000
#define IOVM_SIZE (SZ_2G + SZ_1G + SZ_256M) /* last 4K is for error values */
#endif

#define IOVM_NUM_PAGES(vmsize) (vmsize / PAGE_SIZE)
#define IOVM_BITMAP_SIZE(vmsize) \
		((IOVM_NUM_PAGES(vmsize) + BITS_PER_BYTE) / BITS_PER_BYTE)

/* We does not consider super section mapping (16MB) */
#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define SECT_SIZE (1 << SECT_ORDER)
#define LPAGE_SIZE (1 << LPAGE_ORDER)
#define SPAGE_SIZE (1 << SPAGE_ORDER)

#define MAX_NUM_PLANE	IOVMM_MAX_NUM_ID

/*
 * Metadata attached to the owner of a group of System MMUs that belong
 * to the same owner device.
 */
struct exynos_iommu_owner {
	struct list_head client; /* entry of exynos_iommu_domain.clients */
	struct device *dev;
	void *vmm_data;         /* IO virtual memory manager's data */
	spinlock_t lock;        /* Lock to preserve consistency of System MMU */
};

struct exynos_vm_region {
	struct list_head node;
	dma_addr_t start;
	size_t size;
};

struct exynos_iovmm {
	struct iommu_domain *domain; /* iommu domain for this iovmm */
	size_t iovm_size[MAX_NUM_PLANE]; /* iovm bitmap size per plane */
	dma_addr_t iova_start[MAX_NUM_PLANE]; /* iovm start address per plane */
	unsigned long *vm_map[MAX_NUM_PLANE]; /* iovm biatmap per plane */
	struct list_head regions_list;	/* list of exynos_vm_region */
	spinlock_t vmlist_lock; /* lock for updating regions_list */
	spinlock_t bitmap_lock; /* lock for manipulating bitmaps */
	struct device *dev; /* peripheral device that has this iovmm */
	size_t allocated_size[MAX_NUM_PLANE];
	int num_areas[MAX_NUM_PLANE];
	int inplanes;
	int onplanes;
};
#endif

static inline struct exynos_iovmm *exynos_get_iovmm(struct device *dev)
{
	return ((struct exynos_iommu_owner *)dev->archdata.iommu)->vmm_data;
}

struct exynos_vm_region *find_iovm_region(struct exynos_iovmm *vmm,
						dma_addr_t iova);

void lv2_dummy_map(struct iommu_domain *domain, unsigned long iova);
