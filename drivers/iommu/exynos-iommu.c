/* linux/drivers/iommu/exynos_iommu.c
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

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/export.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>

#define S5P_MMU_CTRL			0x000
#define S5P_MMU_CFG			0x004
#define S5P_MMU_STATUS			0x008
#define S5P_MMU_FLUSH			0x00C
#define S5P_PT_BASE_ADDR		0x014
#define S5P_INT_STATUS			0x018
#define S5P_INT_CLEAR			0x01C
#define S5P_PAGE_FAULT_ADDR		0x024
#define S5P_AW_FAULT_ADDR		0x028
#define S5P_AR_FAULT_ADDR		0x02C
#define S5P_DEFAULT_SLAVE_ADDR		0x030
#define S5P_MMU_VERSION			0x034
#define S5P_PB0_SADDR			0x04C
#define S5P_PB0_EADDR			0x050
#define S5P_PB1_SADDR			0x054
#define S5P_PB1_EADDR			0x058

#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define SECT_SIZE (1 << SECT_ORDER)
#define LPAGE_SIZE (1 << LPAGE_ORDER)
#define SPAGE_SIZE (1 << SPAGE_ORDER)

#define SECT_MASK (~(SECT_SIZE - 1))
#define LPAGE_MASK (~(LPAGE_SIZE - 1))
#define SPAGE_MASK (~(SPAGE_SIZE - 1))

#define lv1ent_fault(sent) (((*(sent) & 3) == 0) || ((*(sent) & 3) == 3))
#define lv1ent_page(sent) ((*(sent) & 3) == 1)
#define lv1ent_section(sent) ((*(sent) & 3) == 2)

#define lv2ent_fault(pent) ((*(pent) & 3) == 0)
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)

#define section_phys(sent) (*(sent) & SECT_MASK)
#define section_offs(iova) ((iova) & 0xFFFFF)
#define lpage_phys(pent) (*(pent) & LPAGE_MASK)
#define lpage_offs(iova) ((iova) & 0xFFFF)
#define spage_phys(pent) (*(pent) & SPAGE_MASK)
#define spage_offs(iova) ((iova) & 0xFFF)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)

#define NUM_LV1ENTRIES 4096
#define NUM_LV2ENTRIES 256

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * sizeof(long))

#define SPAGES_PER_LPAGE (LPAGE_SIZE / SPAGE_SIZE)

#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

#define mk_lv1ent_sect(pa) ((pa) | 2)
#define mk_lv1ent_page(pa) ((pa) | 1)
#define mk_lv2ent_lpage(pa) ((pa) | 1)
#define mk_lv2ent_spage(pa) ((pa) | 2)

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0

#define REG_MMU_CTRL		0x000
#define REG_MMU_CFG		0x004
#define REG_MMU_STATUS		0x008
#define REG_MMU_FLUSH		0x00C
#define REG_MMU_FLUSH_ENTRY	0x010
#define REG_PT_BASE_ADDR	0x014
#define REG_INT_STATUS		0x018
#define REG_INT_CLEAR		0x01C
#define REG_MMU_VERSION		0x034

#define REG_PAGE_FAULT_ADDR	0x024
#define REG_AW_FAULT_ADDR	0x028
#define REG_AR_FAULT_ADDR	0x02C
#define REG_DEFAULT_SLAVE_ADDR	0x030

#define for_each_sysmmu_contnue(owner, data)			\
	list_for_each_entry_continue(data, &sysmmu_list, node)	\
		if (data->owner == owner)

#define for_each_sysmmu(owner, data)			\
	list_for_each_entry(data, &sysmmu_list, node)	\
		if (data->owner == owner)

#define for_each_sysmmu_reverse(owner, data)				\
	list_for_each_entry_continue_reverse(data, &sysmmu_list, node)	\
		if (data->owner == owner)

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

enum EXYNOS_SYSMMU_INTERRUPT_TYPE {
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, /* 7 */
	SYSMMU_FAULT_UNKNOWN,
	SYSMMU_FAULTS_NUM
};

typedef int (*sysmmu_fault_handler_t)(enum EXYNOS_SYSMMU_INTERRUPT_TYPE itype,
			unsigned long pgtable_base, unsigned long fault_addr);

static unsigned short fault_reg_offset[SYSMMU_FAULTS_NUM] = {
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_DEFAULT_SLAVE_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_AW_FAULT_ADDR
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

struct exynos_iommu_domain;

struct sysmmu_drvdata {
	struct list_head node;
	struct device *dev;
	struct device *owner;
	void __iomem *sfrbase;
	struct clk *clk;
	bool active;
	rwlock_t lock;
	struct iommu_domain *domain;
	sysmmu_fault_handler_t fault_handler;
	unsigned long pgtable;
	unsigned long version;
};

static LIST_HEAD(sysmmu_list);

static bool set_sysmmu_active(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU was not active previously
	   and it needs to be initialized */
	if (WARN_ON_ONCE(data->active))
		return false;
	data->active = true;
	return true;
}

static bool set_sysmmu_inactive(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU is needed to be disabled */
	if (WARN_ON_ONCE(!data->active))
		return false;
	data->active = false;
	return true;
}

static bool is_sysmmu_active(struct sysmmu_drvdata *data)
{
	return data->active;
}

static void sysmmu_block(void __iomem *sfrbase)
{
	__raw_writel(CTRL_BLOCK, sfrbase + REG_MMU_CTRL);
}

static void sysmmu_unblock(void __iomem *sfrbase)
{
	__raw_writel(CTRL_ENABLE, sfrbase + REG_MMU_CTRL);
}

static void __sysmmu_tlb_invalidate(void __iomem *sfrbase)
{
	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase,
						unsigned long iova)
{
	__raw_writel((iova & SPAGE_MASK) | 1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

static void __sysmmu_set_ptbase(void __iomem *sfrbase,
				       unsigned long pgd)
{
	__raw_writel(0x1, sfrbase + REG_MMU_CFG); /* 16KB LV1, LRU */
	__raw_writel(pgd, sfrbase + REG_PT_BASE_ADDR);

	__sysmmu_tlb_invalidate(sfrbase);
}

static void __sysmmu_set_prefbuf(void __iomem *sfrbase, unsigned long base,
						unsigned long size, int idx)
{
	__raw_writel(base, sfrbase + S5P_PB0_SADDR + idx * 8);
	__raw_writel(size - 1 + base,  sfrbase + S5P_PB0_EADDR + idx * 8);
}

void exynos_sysmmu_set_prefbuf(struct device *owner,
				unsigned long base0, unsigned long size0,
				unsigned long base1, unsigned long size1)
{
	struct sysmmu_drvdata *data = NULL;
	unsigned long flags;

	BUG_ON((base0 + (size0 - 1)) <= base0);
	BUG_ON((base1 + (size1 - 1)) <= base1);

	for_each_sysmmu(owner, data) {
		if (WARN_ON(data->version != 3))
			continue;

		read_lock_irqsave(&data->lock, flags);
		if (is_sysmmu_active(data)) {
			sysmmu_block(data->sfrbase);

			if (size1 == 0) {
				if (size0 <= SZ_128K) {
					base1 = base0;
					size1 = size0;
				} else {
					size1 = size0 -
						ALIGN(size0 / 2, SZ_64K);
					size0 = size0 - size1;
					base1 = base0 + size0;
				}
			}

			__sysmmu_set_prefbuf(data->sfrbase, base0, size0, 0);
			__sysmmu_set_prefbuf(data->sfrbase, base1, size1, 1);

			sysmmu_unblock(data->sfrbase);
		}
		read_unlock_irqrestore(&data->lock, flags);
	}
}

static void __set_fault_handler(struct sysmmu_drvdata *data,
					sysmmu_fault_handler_t handler)
{
	unsigned long flags;

	write_lock_irqsave(&data->lock, flags);
	data->fault_handler = handler;
	write_unlock_irqrestore(&data->lock, flags);
}

void exynos_sysmmu_set_fault_handler(struct device *owner,
					sysmmu_fault_handler_t handler)
{
	struct sysmmu_drvdata *data;

	for_each_sysmmu(owner, data)
		__set_fault_handler(data, handler);
}

static int default_fault_handler(enum EXYNOS_SYSMMU_INTERRUPT_TYPE itype,
		     unsigned long pgtable_base, unsigned long fault_addr)
{
	unsigned long *ent;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx(Page table base: 0x%lx)\n",
			sysmmu_fault_name[itype], fault_addr, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	BUG();

	return 0;
}

static irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *data = dev_id;
	enum EXYNOS_SYSMMU_INTERRUPT_TYPE itype;
	unsigned long addr;
	int ret = -ENOSYS;

	read_lock(&data->lock);

	WARN_ON(!is_sysmmu_active(data));

	itype = (enum EXYNOS_SYSMMU_INTERRUPT_TYPE)
		__ffs(__raw_readl(data->sfrbase + REG_INT_STATUS));

	if (WARN_ON(!((itype >= 0) && (itype < SYSMMU_FAULT_UNKNOWN)))) {
		itype = SYSMMU_FAULT_UNKNOWN;
		addr = (unsigned long)-1;
	} else {
		addr = __raw_readl(data->sfrbase + fault_reg_offset[itype]);
	}

	if (data->domain)
		ret = report_iommu_fault(data->domain, data->owner, addr,
									itype);
	if ((ret == -ENOSYS) && data->fault_handler) {
		unsigned long base;
		base = __raw_readl(data->sfrbase + REG_PT_BASE_ADDR);
		ret = data->fault_handler(itype, base, addr);
	}

	if (!ret && (itype != SYSMMU_FAULT_UNKNOWN))
		__raw_writel(1 << itype, data->sfrbase + REG_INT_CLEAR);
	else
		dev_dbg(data->dev, "%s is not handled.\n",
						sysmmu_fault_name[itype]);

	sysmmu_unblock(data->sfrbase);

	read_unlock(&data->lock);

	return IRQ_HANDLED;
}

static bool __sysmmu_disable(struct sysmmu_drvdata *data)
{
	unsigned long flags;
	bool disabled = false;

	write_lock_irqsave(&data->lock, flags);

	if (set_sysmmu_inactive(data)) {
		__raw_writel(CTRL_DISABLE, data->sfrbase + REG_MMU_CTRL);
		clk_disable(data->clk);
		disabled = true;
		data->pgtable = 0;
		data->domain = NULL;
		pm_runtime_put(data->dev);
	}

	write_unlock_irqrestore(&data->lock, flags);

	return disabled;
}

static int __exynos_sysmmu_enable(struct device *owner, unsigned long pgtable,
						struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;
	struct sysmmu_drvdata *data;

	BUG_ON(!memblock_is_memory(pgtable));

	/* There are some devices that control more System MMUs than one such
	 * as MFC.
	 */
	for_each_sysmmu(owner, data) {
		ret = pm_runtime_get_sync(data->dev);
		if (ret < 0)
			break;

		write_lock_irqsave(&data->lock, flags);

		if (set_sysmmu_active(data)) {
			clk_enable(data->clk);

			data->pgtable = pgtable;

			__sysmmu_set_ptbase(data->sfrbase, pgtable);

			if (data->version == 0) {
				data->version = readl(
					data->sfrbase + S5P_MMU_VERSION);
				data->version >>= 28;
			}

			if (data->version == 3) {
				__raw_writel((1 << 12) | (2 << 28),
						data->sfrbase + S5P_MMU_CFG);
				__sysmmu_set_prefbuf(data->sfrbase, 0, -1, 0);
				__sysmmu_set_prefbuf(data->sfrbase, 0, -1, 1);
			}

			__raw_writel(CTRL_ENABLE, data->sfrbase + REG_MMU_CTRL);

			data->domain = domain;

			dev_dbg(data->dev, "Enabled.\n");
		} else {
			/* RPM of sysmmu won't be refcounted */
			if (ret > 0)
				pm_runtime_put(data->dev);

			if (WARN_ON(pgtable != data->pgtable)) {
				set_sysmmu_inactive(data);
				ret = -EBUSY;
			}

			dev_dbg(data->dev, "Already enabled.\n");
		}

		write_unlock_irqrestore(&data->lock, flags);

		if (ret < 0)
			break;
	}

	if (ret < 0) {
		for_each_sysmmu_reverse(owner, data) {
			__sysmmu_disable(data);
			dev_dbg(data->dev, "Failed to enable.\n");
		}
	} else {
		ret = 0;
	}

	return ret;
}

int exynos_sysmmu_enable(struct device *owner, unsigned long pgtable)
{
	return __exynos_sysmmu_enable(owner, pgtable, NULL);
}

void exynos_sysmmu_disable(struct device *owner)
{
	struct sysmmu_drvdata *data;

	for_each_sysmmu(owner, data) {
		if (__sysmmu_disable(data))
			dev_dbg(data->dev, "Disabled.\n");
		else
			dev_dbg(data->dev, "Deactivation request ignorred\n");
	}
}

static void sysmmu_tlb_invalidate_entry(struct device *owner,
						unsigned long iova) {
	struct sysmmu_drvdata *data;

	for_each_sysmmu(owner, data) {
		unsigned long flags;

		read_lock_irqsave(&data->lock, flags);

		if (is_sysmmu_active(data)) {
			sysmmu_block(data->sfrbase);
			__sysmmu_tlb_invalidate_entry(data->sfrbase, iova);
			sysmmu_unblock(data->sfrbase);
		} else {
			dev_dbg(data->dev,
				"Disabled. Skipping invalidating TLB.\n");
		}

		read_unlock_irqrestore(&data->lock, flags);
	}
}

void exynos_sysmmu_tlb_invalidate(struct device *owner)
{
	struct sysmmu_drvdata *data;

	for_each_sysmmu(owner, data) {
		unsigned long flags;

		read_lock_irqsave(&data->lock, flags);

		if (is_sysmmu_active(data)) {
			sysmmu_block(data->sfrbase);
			__sysmmu_tlb_invalidate(data->sfrbase);
			sysmmu_unblock(data->sfrbase);
		} else {
			dev_dbg(data->dev,
				"Disabled. Skipping invalidating TLB.\n");
		}

		read_unlock_irqrestore(&data->lock, flags);
	}
}

static int exynos_sysmmu_probe(struct platform_device *pdev)
{
	struct resource *res, *ioarea;
	int ret;
	int irq;
	struct device *dev, *owner;
	void *sfr;
	struct sysmmu_drvdata *data;

	dev = &pdev->dev;
	owner = pdev->dev.archdata.iommu;

	if (owner == NULL) {
		dev_dbg(dev, "Skip probing: No device assigned.\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_dbg(dev, "Not enough memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	data->owner = owner;

	ret = dev_set_drvdata(dev, data);
	if (ret) {
		dev_dbg(dev, "Unabled to initialize driver data\n");
		goto err_init;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(dev, "Unable to find IOMEM region\n");
		goto err_init;
	}

	ioarea = request_mem_region(res->start, resource_size(res),
								dev_name(dev));
	if (ioarea == NULL) {
		dev_dbg(dev, "Unable to register IOMEM region @ 0x%#x\n",
								res->start);
		ret = -ENOMEM;
		goto err_init;
	}

	sfr = ioremap(res->start, resource_size(res));
	if (!sfr) {
		dev_dbg(dev, "Unable to map IOMEM @ PA:0x%#x\n", res->start);
		ret = -ENOENT;
		goto err_ioremap;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_dbg(dev, "Unable to find IRQ resource\n");
		ret = irq;
		goto err_irq;
	}

	ret = request_irq(irq, exynos_sysmmu_irq, 0, dev_name(dev), data);
	if (ret) {
		dev_dbg(dev, "Unabled to register interrupt handler\n");
		goto err_irq;
	}

	data->clk = clk_get(dev, "sysmmu");
	if (IS_ERR(data->clk)) {
		dev_dbg(dev, "No clock descriptor registered\n");
		ret = PTR_ERR(data->clk);
		goto err_clk;
	}

	data->dev = dev;
	data->sfrbase = sfr;
	rwlock_init(&data->lock);
	INIT_LIST_HEAD(&data->node);

	__set_fault_handler(data, &default_fault_handler);
	list_add(&data->node, &sysmmu_list);

	if (dev->parent)
		pm_runtime_enable(dev);

	pdev = to_platform_device(owner);

	if (pdev->id == -1)
		dev_dbg(dev, "Initialized for %s\n", pdev->name);
	else
		dev_dbg(dev, "Initialized for %s.%d\n", pdev->name, pdev->id);
	return 0;
err_clk:
	free_irq(irq, data);
err_irq:
	iounmap(sfr);
err_ioremap:
	release_resource(ioarea);
	kfree(ioarea);
err_init:
	kfree(data);
err_alloc:
	pdev = to_platform_device(owner);
	if (pdev->id == -1)
		dev_err(dev, "Failed to initialize for %s\n", pdev->name);
	else
		dev_err(dev, "Failed to initialize for %s.%d\n", pdev->name,
								pdev->id);
	return ret;
}

static struct platform_driver exynos_sysmmu_driver = {
	.probe		= exynos_sysmmu_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "exynos-sysmmu",
	}
};

/* We does not consider super section mapping (16MB) */
struct iommu_client {
	struct list_head node;
	struct device *dev;
	int refcnt;
};

struct exynos_iommu_domain {
	struct list_head clients; /* list of iommu_client */
	unsigned long *pgtable; /* lv1 page table, 16KB */
	short *lv2entcnt; /* free lv2 entry counter for each section */
	spinlock_t lock; /* lock for this structure */
	spinlock_t pgtablelock; /* lock for modifying page table @ pgtable */
};

static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}

static int exynos_iommu_domain_init(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtable = (unsigned long *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->pgtable)
		goto err_pgtable;

	priv->lv2entcnt = (short *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 1);
	if (!priv->lv2entcnt)
		goto err_counter;

	pgtable_flush(priv->pgtable, priv->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->pgtablelock);
	INIT_LIST_HEAD(&priv->clients);

	domain->priv = priv;
	return 0;

err_counter:
	free_pages((unsigned long)priv->pgtable, 2);
err_pgtable:
	kfree(priv);
	return -ENOMEM;
}

static void exynos_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct list_head *pos, *n;
	unsigned long flags;
	int i;

	WARN_ON(!list_empty(&priv->clients));

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_safe(pos, n, &priv->clients) {
		struct iommu_client *client;

		client = list_entry(pos, struct iommu_client, node);
		exynos_sysmmu_disable(client->dev);
		kfree(client);
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	for (i = 0; i < NUM_LV1ENTRIES; i++)
		if (lv1ent_page(priv->pgtable + i))
			kfree(__va(lv2table_base(priv->pgtable + i)));

	free_pages((unsigned long)priv->pgtable, 2);
	free_pages((unsigned long)priv->lv2entcnt, 1);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int exynos_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	int ret;
	struct exynos_iommu_domain *priv = domain->priv;
	struct iommu_client *client = NULL;
	struct list_head *pos;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each(pos, &priv->clients) {
		struct iommu_client *cur;

		cur = list_entry(pos, struct iommu_client, node);
		if (cur->dev == dev) {
			client = cur;
			break;
		}
	}

	if (client != NULL) {
		dev_dbg(dev, "%s: IOMMU with pgtable 0x%lx already attached\n",
					__func__, __pa(priv->pgtable));
		client->refcnt++;
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (client != NULL)
		return 0;

	client = kmalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	INIT_LIST_HEAD(&client->node);
	client->dev = dev;
	client->refcnt = 1;

	ret = __exynos_sysmmu_enable(dev, __pa(priv->pgtable), domain);
	if (ret) {
		kfree(client);
		return ret;
	}

	spin_lock_irqsave(&priv->lock, flags);
	list_add_tail(&client->node, &priv->clients);
	spin_unlock_irqrestore(&priv->lock, flags);

	dev_dbg(dev, "%s: Attached new IOMMU with pgtable 0x%lx\n", __func__,
						__pa(priv->pgtable));
	return 0;
}

static void exynos_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct iommu_client *client = NULL;
	struct list_head *pos;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each(pos, &priv->clients) {
		struct iommu_client *cur;

		cur = list_entry(pos, struct iommu_client, node);
		if (cur->dev == dev) {
			cur->refcnt--;
			client = cur;
			break;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (WARN_ON(client == NULL))
		return;

	if (client->refcnt > 0) {
		dev_dbg(dev, "%s: Detaching IOMMU with pgtable 0x%lx delayed\n",
					__func__, __pa(priv->pgtable));
		return;
	}

	BUG_ON(client->refcnt != 0);

	list_del(&client->node);
	exynos_sysmmu_disable(client->dev);
	kfree(client);
	dev_dbg(dev, "%s: Detached IOMMU with pgtable 0x%lx\n", __func__,
						__pa(priv->pgtable));
}

static unsigned long *alloc_lv2entry(unsigned long *sent, unsigned long iova,
					short *pgcounter)
{
	if (lv1ent_fault(sent)) {
		unsigned long *pent;

		pent = kzalloc(LV2TABLE_SIZE, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return NULL;

		*sent = mk_lv1ent_page(__pa(pent));
		*pgcounter = NUM_LV2ENTRIES;
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		pgtable_flush(sent, sent + 1);
	}

	return page_entry(sent, iova);
}

static int lv1set_section(unsigned long *sent, phys_addr_t paddr, short *pgcnt)
{
	if (lv1ent_section(sent))
		return -EADDRINUSE;

	if (lv1ent_page(sent)) {
		if (*pgcnt != NUM_LV2ENTRIES)
			return -EADDRINUSE;

		kfree(page_entry(sent, 0));

		*pgcnt = 0;
	}

	*sent = mk_lv1ent_sect(paddr);

	pgtable_flush(sent, sent + 1);

	return 0;
}

static int lv2set_page(unsigned long *pent, phys_addr_t paddr, size_t size,
								short *pgcnt)
{
	if (size == SPAGE_SIZE) {
		if (!lv2ent_fault(pent))
			return -EADDRINUSE;

		*pent = mk_lv2ent_spage(paddr);
		pgtable_flush(pent, pent + 1);
		*pgcnt += 1;
	} else { /* size == LPAGE_SIZE */
		int i;
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (!lv2ent_fault(pent)) {
				memset(pent, 0, sizeof(*pent) * i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
		}
		pgtable_flush(pent, pent + SPAGES_PER_LPAGE);
		*pgcnt += SPAGES_PER_LPAGE;
	}

	return 0;
}

static int exynos_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long *entry;
	unsigned long flags;
	int ret = -ENOMEM;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (size == SECT_SIZE) {
		ret = lv1set_section(entry, paddr,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		unsigned long *pent;

		pent = alloc_lv2entry(entry, iova,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

		if (!pent)
			ret = -ENOMEM;

		ret = lv2set_page(pent, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	}

	if (ret) {
		pr_debug("%s: Failed to map iova 0x%lx/0x%x bytes\n",
							__func__, iova, size);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return ret;
}

static size_t exynos_iommu_unmap(struct iommu_domain *domain,
					       unsigned long iova, size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct iommu_client *client;
	unsigned long flags;
	unsigned long *ent;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	ent = section_entry(priv->pgtable, iova);

	if (lv1ent_section(ent)) {
		BUG_ON(size < SECT_SIZE);

		*ent = 0;
		pgtable_flush(ent, ent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_fault(ent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	ent = page_entry(ent, iova);

	if (unlikely(lv2ent_fault(ent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(ent)) {
		*ent = 0;
		size = SPAGE_SIZE;
		goto done;
	}

	/* lv1ent_large(ent) == true here */
	BUG_ON(size < LPAGE_SIZE);

	memset(ent, 0, sizeof(*ent) * SPAGES_PER_LPAGE);

	size = LPAGE_SIZE;
done:
	list_for_each_entry(client, &priv->clients, node)
		sysmmu_tlb_invalidate_entry(client->dev, iova);

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return size;
}

static phys_addr_t exynos_iommu_iova_to_phys(struct iommu_domain *domain,
					  unsigned long iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long *entry;
	unsigned long flags;
	phys_addr_t phys = -1;

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return phys;
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.pgsize_bitmap = SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE,
};

static int __init exynos_iommu_init(void)
{
	int ret;

	ret = platform_driver_register(&exynos_sysmmu_driver);

	if (ret == 0)
		bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);

	return ret;
}
arch_initcall(exynos_iommu_init);
