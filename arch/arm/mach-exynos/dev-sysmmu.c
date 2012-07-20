/* linux/arch/arm/mach-exynos/dev-sysmmu.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - System MMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <mach/map.h>
#include <mach/irqs.h>
#include <mach/sysmmu.h>
#include <plat/s5p-clock.h>
#include <asm/dma-iommu.h>
#include <linux/slab.h>

#ifdef CONFIG_ARM_DMA_USE_IOMMU
struct dma_iommu_mapping * s5p_create_iommu_mapping(struct device *client,
				dma_addr_t base, unsigned int size,
				int order, struct dma_iommu_mapping *mapping)
{
	if (!client)
		return NULL;

	if (mapping == NULL) {
		mapping = arm_iommu_create_mapping(&platform_bus_type,
						base, size, order);
		if (!mapping)
			return NULL;
	}

	client->dma_parms = kzalloc(sizeof(*client->dma_parms), GFP_KERNEL);
	dma_set_max_seg_size(client, 0xffffffffu);
	arm_iommu_attach_device(client, mapping);
	return mapping;
}

void s5p_destroy_iommu_mapping(struct device *client)
{
	if (!client) {
		printk(KERN_ERR"Invalid client passed to %s()\n", __func__);
		return;
	}
	/* detach the device from the IOMMU */
	arm_iommu_detach_device(client, client->archdata.mapping);

	/* release the IOMMU mapping */
	arm_iommu_release_mapping(client->archdata.mapping);

	return;
}

struct platform_device *find_sysmmu_dt(struct platform_device *pdev,
					char *name)
{
	struct device_node *dn, *dns;
	struct platform_device *pds;
	const __be32 *parp;

	dn = pdev->dev.of_node;
	parp = of_get_property(dn, name, NULL);
	if (parp==NULL) {
		printk(KERN_ERR "Could not find property SYSMMU\n");
		return NULL;
	}
	dns = of_find_node_by_phandle(be32_to_cpup(parp));
	if (dns==NULL) {
		printk(KERN_ERR "Could not find node\n");
		return NULL;
	}

	pds = of_find_device_by_node(dns);
	if (!pds) {
		printk(KERN_ERR "No platform device found\n");
		return NULL;
	}
	return pds;
}
#endif


