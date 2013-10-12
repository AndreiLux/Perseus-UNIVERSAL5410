/*
 * arch/arm/common/bL_entry.c -- big.LITTLE kernel re-entry point
 *
 * Created by:  Nicolas Pitre, March 2012
 * Copyright:   (C) 2012  Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/cma.h>

#include <asm/bL_entry.h>
#include <asm/barrier.h>
#include <asm/proc-fns.h>
#include <asm/cacheflush.h>
#include <asm/memblock.h>

extern volatile unsigned long bL_entry_vectors[BL_NR_CLUSTERS][BL_CPUS_PER_CLUSTER];

void bL_set_entry_vector(unsigned cpu, unsigned cluster, void *ptr)
{
	unsigned long val = ptr ? virt_to_phys(ptr) : 0;
	bL_entry_vectors[cluster][cpu] = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&bL_entry_vectors[cluster][cpu], 4);
	outer_clean_range(__pa(&bL_entry_vectors[cluster][cpu]),
			  __pa(&bL_entry_vectors[cluster][cpu + 1]));
}

unsigned long bL_sync_phys;
struct bL_sync_struct *bL_sync;

/*
 * __bL_cpu_going_down: Indicates that the cpu is being torn down
 *    This must be called at the point of committing to teardown of a CPU.
 */
void __bL_cpu_going_down(unsigned int cpu, unsigned int cluster)
{
	writeb_relaxed(CPU_GOING_DOWN, &bL_sync->clusters[cluster].cpus[cpu]);
	dsb();
}

/*
 * __bL_cpu_down: Indicates that cpu teardown is complete and that the
 *    cluster can be torn down without disrupting this CPU.
 *    To avoid deadlocks, this must be called before a CPU is powered down.
 */
void __bL_cpu_down(unsigned int cpu, unsigned int cluster)
{
	dsb();
	writeb_relaxed(CPU_DOWN, &bL_sync->clusters[cluster].cpus[cpu]);
	dsb_sev();
}

/*
 * __bL_outbound_leave_critical: Leave the cluster teardown critical section.
 * @state: the final state of the cluster:
 *     CLUSTER_UP: no destructive teardown was done and the cluster has been
 *         restored to the previous state; or
 *     CLUSTER_DOWN: the cluster has been torn-down, ready for power-off.
 */
void __bL_outbound_leave_critical(unsigned int cluster, int state)
{
	dsb();
	writeb_relaxed(state, &bL_sync->clusters[cluster].cluster);
	dsb_sev();
}

/*
 * __bL_outbound_enter_critical: Enter the cluster teardown critical section.
 *   This function should be called by the last man, after local CPU teardown
 *   is complete.
 */
bool __bL_outbound_enter_critical(unsigned int cpu, unsigned int cluster)
{
	unsigned int i;
	struct bL_cluster_sync_struct *c = &bL_sync->clusters[cluster];

	/* Warn inbound CPUs that the cluster is being torn down: */
	writeb_relaxed(CLUSTER_GOING_DOWN, &c->cluster);

	dsb();

	/* Back out if the inbound cluster is already in the critical region: */
	if (readb_relaxed(&c->inbound) == INBOUND_COMING_UP)
		goto abort;

	/*
	 * Wait for all CPUs to get out of the GOING_DOWN state, so that local
	 * teardown is complete on each CPU before tearing down the cluster.
	 *
	 * If any CPU has been woken up again from the DOWN state, then we
	 * shouldn't be taking the cluster down at all: abort in that case.
	 */
	for (i = 0; i < BL_CPUS_PER_CLUSTER; i++) {
		int cpustate;

		if (i == cpu)
			continue;

		while (1) {
			cpustate = readb_relaxed(&c->cpus[i]);
			if (cpustate != CPU_GOING_DOWN)
				break;

			wfe();
		}

		switch (cpustate) {
			case CPU_DOWN:
				continue;
			default:
				goto abort;
		}
	}

	dsb();

	return true;
abort:
	__bL_outbound_leave_critical(cluster, CLUSTER_UP);
	return false;
}

bool __bL_cluster_state(unsigned int cluster)
{
	return readb_relaxed(&bL_sync->clusters[cluster].cluster);
}

/*
 * bL_running_cluster_num_cpus: Return the cluster number of running cpu
 */
unsigned int bL_running_cluster_num_cpus(unsigned int cpu)
{
	unsigned int cluster = 0;
	unsigned int cpustate;

	cpustate = readb_relaxed(&bL_sync->clusters[cluster].cpus[cpu]);

	if (cpustate == CPU_DOWN)
		cluster = 1;

	pr_debug("cpu %d running cluster : %d\n", cpu, cluster);

	return cluster;
}

void bL_update_cluster_state(unsigned int value, unsigned int cluster)
{
	if (value < CLUSTER_DOWN || value > CLUSTER_GOING_DOWN)
		return;
	writeb_relaxed(value, &bL_sync->clusters[cluster].cluster);
}

void bL_update_cpu_state(unsigned int value, unsigned int cpu,
			 unsigned int cluster)
{
	if (value < CPU_DOWN || value > CPU_GOING_DOWN)
		return;
	writeb_relaxed(value, &bL_sync->clusters[cluster].cpus[cpu]);
}

static struct miscdevice bL_reserve_mem_device = {
	MISC_DYNAMIC_MINOR,
	"b.L_mem",
};

extern unsigned long bL_power_up_setup_phys;

unsigned long bL_vlock_phys;
struct bL_firstman_vlock_struct *bL_vlock;

void * __init bL_reserve_memory(struct device *dev, unsigned long *phys)
{
	struct page *page;
	void *virt;

	*phys = cma_alloc(dev, "bl_mem", SZ_4K, 0);
	page = phys_to_page(*phys);
	virt = vmap(&page, 1, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

	return virt;
}

#define INVALID_OWNER	1
#define INVALID_OFFSET	2
int check_bL_vlock_phys(void)
{
	int i;
	for (i = 0; i < BL_NR_CLUSTERS; i++) {
		int j;
		if (bL_vlock->clusters[i].voting_owner != 0)
			return -INVALID_OWNER;
		for_each_online_cpu(j) {
			if (bL_vlock->clusters[i].voting_offset[j] != 0)
				return -INVALID_OFFSET;
		}
	}
	return 0;
}

int __init bL_cluster_sync_init(const struct bL_power_ops *ops)
{
	unsigned int i, mpidr, this_cluster;
	int err;

	err = misc_register(&bL_reserve_mem_device);
	if (err) {
		pr_crit("Fail to reserve the memory for switcher\n");
		return -ENOMEM;
	}

	bL_sync = bL_reserve_memory(bL_reserve_mem_device.this_device,
				    &bL_sync_phys);
	BUG_ON(bL_sync_phys == 0);
	if (!bL_sync) {
		pr_err("big.LITTLE synchronisation buffer mapping failed\nm");
		return -ENOMEM;
	}

	bL_vlock = bL_reserve_memory(bL_reserve_mem_device.this_device,
				     &bL_vlock_phys);
	BUG_ON(bL_vlock_phys == 0);

	if (!bL_vlock) {
		pr_err("big.LITTLE voting lock buffer mapping failed\n");
		return -ENOMEM;
	}

	/*
	 * Set initial CPU and cluster states.
	 * Only one cluster is assumed to be active at this point.
	 */
	asm ("mrc\tp15, 0, %0, c0, c0, 5" : "=r" (mpidr));
	this_cluster = (mpidr >> 8) & 0xf;
	memset(bL_sync, 0, sizeof *bL_sync);
	for_each_online_cpu(i)
		bL_sync->clusters[this_cluster].cpus[i] = CPU_UP;
	bL_sync->clusters[this_cluster].cluster = CLUSTER_UP;

	if (ops->power_up_setup) {
		bL_power_up_setup_phys =
			virt_to_phys(ops->power_up_setup);
		__cpuc_flush_dcache_area((void *)&bL_power_up_setup_phys,
						sizeof bL_power_up_setup_phys);
		outer_clean_range(__pa(&bL_power_up_setup_phys),
					 __pa(&bL_power_up_setup_phys + 1));
	}

	__cpuc_flush_dcache_area((void *)&bL_sync_phys,
					sizeof bL_sync_phys);
	outer_clean_range(__pa(&bL_sync_phys), __pa(&bL_sync_phys + 1));

	/*
	 * For electing firstman, voting lock structure is initialized.
	 * The values of voting onwer & offset is clear.
	 */
	memset(bL_vlock, 0, sizeof *bL_vlock);
	for (i = 0; i < BL_NR_CLUSTERS; i++) {
		int j;
		bL_vlock->clusters[i].voting_owner = 0;
		for_each_online_cpu(j)
			bL_vlock->clusters[i].voting_offset[j] = 0;
	}

	/* Re-check values of bL_vlock */
	err = check_bL_vlock_phys();
	if (err) {
		pr_crit("\n\nInvalid value (%d) of bL_vlock\n\n", err);
		BUG();
	}

	__cpuc_flush_dcache_area((void *)&bL_vlock_phys, sizeof bL_vlock_phys);
	outer_clean_range(__pa(&bL_vlock_phys), __pa(&bL_vlock_phys +1));

	return 0;
}
