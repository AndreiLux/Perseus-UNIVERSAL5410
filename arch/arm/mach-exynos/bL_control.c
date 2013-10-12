/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              htt://www.samsung.com
 *
 * bL platform support
 *
 * Based on arch/arm/mach-vexpress/kingfisher.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/syscore_ops.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <asm/bL_switcher.h>
#include <asm/bL_entry.h>
#include <asm/uaccess.h>

#include <plat/cpu.h>
#include <plat/cci.h>
#include <plat/s5p-clock.h>

#include <mach/regs-pmu.h>
#include <mach/smc.h>
#include <mach/pmu.h>
#include <mach/debug.h>
#include <mach/cpufreq.h>

static bool cluster_power_ctrl_en = true;
static int __init cluster_power_ctrl(char *str)
{
	cluster_power_ctrl_en = false;
	return 1;
}
__setup("cluster_power=", cluster_power_ctrl);

struct sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj,
			struct attribute *attr, char *buf);
	ssize_t (*store)(struct kobject *a, struct attribute *b,
			 const char *c, size_t count);
};

#define PA_SLEP_PLUGIN		(0x02020000)

#ifdef CONFIG_EXYNOS5_CCI

#define REG_SWITCHING_ADDR	(S5P_VA_SYSRAM_NS + 0x18)

#define GIC_ENABLE_STATUS	0x1

#define MAX_CORE_COUNT 4
static int core_count[2];
static int online_core_count;
static struct hrtimer cluster_power_down_timer;

/*
 * We can't use regular spinlocks. In the switcher case, it is possible
 * for an outbound CPU to call power_down() after its inbound counterpart
 * is already live using the same logical CPU number which trips lockdep
 * debugging.
 */
static arch_spinlock_t exynos_lock = __ARCH_SPIN_LOCK_UNLOCKED;
static int kfs_use_count[BL_CPUS_PER_CLUSTER][BL_NR_CLUSTERS];
static unsigned int cluster_hotplug_cpu[BL_CPUS_PER_CLUSTER];
static struct clk *apll;
static struct clk *fout_apll;

static void add_core_count(unsigned int cluster)
{
	BUG_ON(core_count[cluster] >= online_core_count);
	core_count[cluster]++;
}

static unsigned int dec_core_count(unsigned int cluster)
{
	unsigned int count;

	BUG_ON(core_count[cluster] == 0);
	count = --core_count[cluster];

	return count;
}

static void exynos_core_power_control(unsigned int cpu,
				      unsigned int cluster,
				      int enable)
{
	int value = 0;

	if (soc_is_exynos5410()) {
		if (samsung_rev() >= EXYNOS5410_REV_1_0)
			cluster = !cluster;
		if (cluster == 0)
			cpu += 4;
	} else {
		if (cluster == 1)
			cpu += 4;
	}

	if (enable)
		value = EXYNOS_CORE_LOCAL_PWR_EN;


	__raw_writel(value, EXYNOS_ARM_CORE_CONFIGURATION(cpu));
}

#ifdef CONFIG_EXYNOS5_CLUSTER_POWER_CONTROL
static int exynos_change_apll_parent(int cluster_enable)
{
	int ret = 0;

	if (IS_ERR(apll))
		return -ENODEV;

	if (IS_ERR(fout_apll))
		return -ENODEV;

	if (cluster_enable) {
		clk_enable(fout_apll);
		ret = clk_set_parent(apll, &clk_fout_apll);
	} else {
		ret = clk_set_parent(apll, &clk_fin_apll);
		clk_disable(fout_apll);
	}
	return ret;
}
#endif

static int exynos_cluster_power_control(unsigned int cluster, int enable)
{
#ifdef CONFIG_EXYNOS5_CLUSTER_POWER_CONTROL
	int value = 0;
	int ret = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(10);

#ifdef CONFIG_EXYNOS54XX_DEBUG
	if ((soc_is_exynos5420() || soc_is_exynos5410()) && FLAG_T32_EN)
		return 0;
#endif
	if (!cluster_power_ctrl_en)
		return 0;

	if (soc_is_exynos5410()) {
		if (samsung_rev() < EXYNOS5410_REV_1_0)
			cluster = !cluster;
	}

	if (enable) {
		value = EXYNOS_CORE_LOCAL_PWR_EN;
		if (cluster == 0)
			ret = exynos_change_apll_parent(1);
	} else {
		if (cluster == 0)
			ret = exynos_change_apll_parent(0);
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		reset_lpj_for_cluster(cluster);
#endif
	}

	if (ret)
		return ret;

	if ((__raw_readl(EXYNOS_COMMON_STATUS(cluster)) & 0x3) == value)
			return 0;

	__raw_writel(value, EXYNOS_COMMON_CONFIGURATION(cluster));

	/* wait till cluster power control is applied */
	do {
		if ((__raw_readl(EXYNOS_COMMON_STATUS(cluster)) &
		__raw_readl(EXYNOS_L2_STATUS(cluster)) & 0x3) == value)
			return 0;
	} while (time_before(jiffies, timeout));

	return -EDEADLK;
#else
	return 0;
#endif
}

static int exynos_cluster_power_status(unsigned int cluster)
{
	int status = 0;
	int i;

	if (soc_is_exynos5410()) {
		if (samsung_rev() >= EXYNOS5410_REV_1_0)
			cluster = !cluster;

		cluster = (cluster == 0) ? 4 : 0;
	} else {
		cluster = (cluster == 1) ? 4 : 0;
	}

	for (i = 0; i < NR_CPUS; i++)
		status |= !!(__raw_readl(EXYNOS_ARM_CORE_STATUS(cluster + i)) &
			0x3) << i;

	return status;
}

static int read_mpidr(void)
{
	unsigned int id;
	asm volatile ("mrc\tp15, 0, %0, c0, c0, 5" : "=r" (id));
	return id;
}

enum hrtimer_restart exynos_cluster_power_down(struct hrtimer *timer)
{
	ktime_t period;
	int cluster_to_powerdown;
	enum hrtimer_restart ret;

	arch_spin_lock(&exynos_lock);
	if (core_count[0] == 0) {
		cluster_to_powerdown = 0;
	} else if (core_count[1] == 0) {
		cluster_to_powerdown = 1;
	} else {
		ret = HRTIMER_NORESTART;
		goto end;
	}

	if (exynos_cluster_power_status(cluster_to_powerdown)) {
		period = ktime_set(0, 10000000);
		hrtimer_forward_now(timer, period);
		ret = HRTIMER_RESTART;
		goto end;
	} else {
		disable_cci_snoops(cluster_to_powerdown);
		exynos_cluster_power_control(cluster_to_powerdown, 0);
		ret = HRTIMER_NORESTART;
	}
end:
	arch_spin_unlock(&exynos_lock);
	return ret;
}

/*
 * bL_power_up - make given CPU in given cluster runable
 *
 * @cpu: CPU number within given cluster
 * @cluster: cluster number for the CPU
 *
 * The identified CPU is brought out of reset.  If the cluster was powered
 * down then it is brought up as well, taking care not to let the other CPUs
 * in the cluster run, and ensuring appropriate cluster setup.
 * Caller must ensure the appropriate entry vector is initialized prior to
 * calling this.
 */
static void bL_power_up(unsigned int cpu, unsigned int cluster)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1);

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);

	/*
	 * Since this is called with IRQs enabled, and no arch_spin_lock_irq
	 * variant exists, we need to disable IRQs manually here.
	 */
	local_irq_disable();
	arch_spin_lock(&exynos_lock);

	kfs_use_count[cpu][cluster]++;
	if (kfs_use_count[cpu][cluster] == 1) {
#ifdef CONFIG_ARM_TRUSTZONE
		/*
		 * Before turning inbound cpu on, save secure contexts
		 * of outbound
		 */
		exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_SWITCH, 0);
#endif

		add_core_count(cluster);

		/*
		 * If outbound core wake up the first man of inbound cluster,
		 * outbound core must meet conditions which are power-on of
		 * COMMON BLOCK and enabled cci snoop about inbound cluster.
		 *
		 * Also non-first cores do not need to perform
		 * exynos_cluster_power_control() and enable_cci_snoops().
		 */
		if (unlikely(core_count[cluster] == 1)) {
			exynos_cluster_power_control(cluster, 1);
			enable_cci_snoops(cluster);
		}
		set_boot_flag(cpu, SWITCH);
		exynos_core_power_control(cpu, cluster, 1);
	} else if (kfs_use_count[cpu][cluster] != 2) {
		/*
		 * The only possible values are:
		 * 0 = CPU down
		 * 1 = CPU (still) up
		 * 2 = CPU requested to be up before it had a chance
		 *     to actually make itself down.
		 * Any other value is a bug.
		 */
		BUG();
	}

	arch_spin_unlock(&exynos_lock);
	local_irq_enable();

	do {
		if (read_gic_flag(cpu) == GIC_ENABLE_STATUS)
			break;
	} while (time_before(jiffies, timeout));
	clear_gic_flag(cpu);
}

/*
 * Helper to determine whether the specified cluster should still be
 * shut down.  By polling this before shutting a cluster down, we can
 * reduce the probability of wasted cache flushing etc.
 */
static bool powerdown_needed(unsigned int cluster)
{
	bool need = false;

	if (core_count[cluster] == 0)
		need = true;

	return need;
}

/*
 * bL_power_down - power down given CPU in given cluster
 *
 * @cpu: CPU number within given cluster
 * @cluster: cluster number for the CPU
 *
 * The identified CPU is powered down.  If this is the last CPU still alive
 * in the cluster then the necessary steps to power down the cluster are
 * performed as well and a non zero value is returned in that case.
 *
 * It is assumed that the reset will be effective at the next WFI instruction
 * performed by the target CPU.
 */
static void bL_power_down(unsigned int cpu, unsigned int cluster)
{
	int op_type;
	bool last_man = false, skip_wfi = false;

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);

	__bL_cpu_going_down(cpu, cluster);

	arch_spin_lock(&exynos_lock);
	kfs_use_count[cpu][cluster]--;
	if (kfs_use_count[cpu][cluster] == 0) {
		exynos_core_power_control(cpu, cluster, 0);
		dec_core_count(cluster);
		if (core_count[cluster] == 0) {
			if (__bL_cluster_state(cluster) == CLUSTER_UP)
				last_man = true;
		}
	} else if (kfs_use_count[cpu][cluster] == 1) {
		/*
		 * A power_up request went ahead of us.
		 * Even if we do not want to shut this CPU down,
		 * the caller expects a certain state as if the WFI
		 * was aborted.  So let's continue with cache cleaning.
		 */
		skip_wfi = true;
	} else
		BUG();

	if (last_man && __bL_outbound_enter_critical(cpu, cluster)) {
		arch_spin_unlock(&exynos_lock);

		if (!powerdown_needed(cluster)) {
			__bL_outbound_leave_critical(cluster, CLUSTER_UP);
			goto non_last_man;
		}
		__bL_outbound_leave_critical(cluster, CLUSTER_DOWN);
		op_type = OP_TYPE_CLUSTER;
	} else {
		arch_spin_unlock(&exynos_lock);

		non_last_man:
		op_type = OP_TYPE_CORE;
	}

	cpu_proc_fin();
	__bL_cpu_down(cpu, cluster);

#ifdef CONFIG_ARM_TRUSTZONE
	/* Now we are prepared for power-down, do it: */
	if (!skip_wfi) {
		exynos_smc(SMC_CMD_SHUTDOWN, op_type, SMC_POWERSTATE_SWITCH, 0);
		/* should never get here */
		BUG();
	}
#endif

}

static void bL_inbound_setup(unsigned int cpu, unsigned int cluster)
{
	clear_boot_flag(cpu, SWITCH);
	/*
	 * Controling cluster power is last man of inbound cluster only.
	 * In other words, non-lastman dose not need checks the count
	 * of core every swich.
	 */

	if (unlikely(core_count[cluster] == 0))
		hrtimer_start(&cluster_power_down_timer,
			ktime_set(0, 1000000), HRTIMER_MODE_REL);
}

extern void bL_power_up_setup(void);

static const struct bL_power_ops bL_control_power_ops = {
	.power_up		= bL_power_up,
	.power_down		= bL_power_down,
	.power_up_setup		= bL_power_up_setup,
	.inbound_setup		= bL_inbound_setup,
};

#if defined(CONFIG_PM)
static void bL_resume(void)
{
	/*
	 * REG_SWITCHING_ADDR is in iRAM and this area is reset after
	 * suspend/resume. Thus without setting bl_entry_point,
	 * core switching is not working properly.
	 */
	__raw_writel(virt_to_phys(bl_entry_point), REG_SWITCHING_ADDR);
}
#else
#define bL_resume NULL
#endif

static struct syscore_ops exynos_bL_syscore_ops = {
	.resume		= bL_resume,
};

static int __cpuinit bL_hotplug_cpu_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
		/*
		 * When carry out hotplug-in, if cluster power state is down
		 * must turn COMMON BLOCK on and enable cci snoop before cpu
		 * brings up.
		 */
		arch_spin_lock(&exynos_lock);
		if (core_count[cluster_hotplug_cpu[cpu]] == 0) {
			exynos_cluster_power_control(
				cluster_hotplug_cpu[cpu], 1);
			enable_cci_snoops(cluster_hotplug_cpu[cpu]);
		}
		arch_spin_unlock(&exynos_lock);
		break;
	case CPU_ONLINE:
		arch_spin_lock(&exynos_lock);
		BUG_ON(online_core_count >= MAX_CORE_COUNT);
		online_core_count++;
		/*
		 * When some cpu hotplug in, the instance for synchroniztion
		 * must know the information about cpu & cluster power state
		 */
		if (core_count[cluster_hotplug_cpu[cpu]] == 0)
			bL_update_cluster_state(CLUSTER_UP, cluster_hotplug_cpu[cpu]);
		/* Increase the count of cpu which can perform a switch */
		add_core_count(cluster_hotplug_cpu[cpu]);
		kfs_use_count[cpu][cluster_hotplug_cpu[cpu]] = 1;
		 /* The state of hotpluged in cpu changes to CPU_UP */
		bL_update_cpu_state(CPU_UP, cpu, cluster_hotplug_cpu[cpu]);
		arch_spin_unlock(&exynos_lock);
		break;
	case CPU_DEAD:
		arch_spin_lock(&exynos_lock);
		BUG_ON(online_core_count == 0);
		online_core_count--;

		/* Save the cluster number of cpu that carry out hotplug-out */
		cluster_hotplug_cpu[cpu] = bL_running_cluster_num_cpus(cpu);

		/* Decrease the count of cpu which can perform a switch */
		dec_core_count(cluster_hotplug_cpu[cpu]);
		kfs_use_count[cpu][cluster_hotplug_cpu[cpu]] = 0;
		/* The state of hotpluged out cpu changes to CPU_DOWN */
		__bL_cpu_down(cpu, cluster_hotplug_cpu[cpu]);
		arch_spin_unlock(&exynos_lock);
		break;
	case CPU_POST_DEAD:
		/*
		 * If core count is 0, the cluster power state changes
		 * to CLUSTER_DOWN.
		 */
		arch_spin_lock(&exynos_lock);
		if (core_count[cluster_hotplug_cpu[cpu]] == 0) {
			bL_update_cluster_state(CLUSTER_DOWN, cluster_hotplug_cpu[cpu]);
		/*
		 * I don't know why perform the cluster power down here
		 * So if it need the feature, we must think to interwork
		 * with bL_switcher core driver.
		 */
		}
		arch_spin_unlock(&exynos_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata bL_hotplug_cpu_notifier = {
	.notifier_call = bL_hotplug_cpu_callback,
};

static int __init bL_control_init(void)
{
	unsigned int cpu, cluster_id = (read_mpidr() >> 8) & 0xf;

	/* All future entries into the kernel goes through our entry vectors. */
	__raw_writel(virt_to_phys(bl_entry_point), REG_SWITCHING_ADDR);

	core_count[cluster_id] = MAX_CORE_COUNT;
	online_core_count = MAX_CORE_COUNT;

	hrtimer_init(&cluster_power_down_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	cluster_power_down_timer.function = exynos_cluster_power_down;

	hrtimer_start(&cluster_power_down_timer,
			ktime_set(0, 10000000), HRTIMER_MODE_REL);

	register_syscore_ops(&exynos_bL_syscore_ops);

	/*
	 * Initialize CPU usage counts, assuming that only one cluster is
	 * activated at this point.
	 */
	for_each_online_cpu(cpu)
		kfs_use_count[cpu][cluster_id] = 1;

	/*
	 * Reaction of cpu hotplug in & out
	 */
	register_cpu_notifier(&bL_hotplug_cpu_notifier);

	/* Getting APLL */
	apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(apll))
		pr_err("Fail to get APLL, so this problem is not resolved,"
			"function of cluster power down is nothing.\n");

	/* Getting FOUT APLL */
	fout_apll = clk_get(NULL, "fout_apll");
	if (IS_ERR(fout_apll))
		pr_err("Fail to get FOUT APLL, so this problem is not resolved,"
			"function of cluster power down is nothing.\n");

	clk_enable(fout_apll);

	return bL_switcher_init(&bL_control_power_ops);
}

device_initcall(bL_control_init);

#else	/* !CONFIG_EXYNOS5_CCI */

#define GIC_ENABLE_STATUS	0x1

static int core_count[BL_NR_CLUSTERS];
/*
 * We can't use regular spinlocks. In the switcher case, it is possible
 * for an outbound CPU to call power_down() after its inbound counterpart
 * is already live using the same logical CPU number which trips lockdep
 * debugging.
 */
static arch_spinlock_t exynos_lock = __ARCH_SPIN_LOCK_UNLOCKED;
static struct clk *apll;
static struct clk *fout_apll;

static int add_core_count(unsigned int cluster)
{
	BUG_ON(core_count[cluster] >= num_online_cpus());
	return ++core_count[cluster];
}

static unsigned int dec_core_count(unsigned int cluster)
{
	BUG_ON(core_count[cluster] == 0);
	return --core_count[cluster];
}

static void exynos_core_power_control(unsigned int cpu,
				      unsigned int cluster,
				      int enable)
{
	int value = 0;

	if (soc_is_exynos5410()) {
		if (samsung_rev() >= EXYNOS5410_REV_1_0)
			cluster = !cluster;
		if (cluster == 0)
			cpu += 4;
	} else {
		if (cluster == 1)
			cpu += 4;
	}

	if (enable)
		value = EXYNOS_CORE_LOCAL_PWR_EN;

	__raw_writel(value, EXYNOS_ARM_CORE_CONFIGURATION(cpu));
}

#ifdef CONFIG_EXYNOS5_CLUSTER_POWER_CONTROL
static int exynos_change_apll_parent(int cluster_enable)
{
	int ret = 0;

	if (IS_ERR(apll))
		return -ENODEV;

	if (IS_ERR(fout_apll))
		return -ENODEV;

	if (cluster_enable) {
		clk_enable(fout_apll);
		ret = clk_set_parent(apll, &clk_fout_apll);
	} else {
		ret = clk_set_parent(apll, &clk_fin_apll);
		clk_disable(fout_apll);
	}

	return ret;
}

static int exynos_cluster_power_control(unsigned int cluster, int enable)
{
	int value = 0;
	int ret = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(10);

#ifdef CONFIG_EXYNOS54XX_DEBUG
	if ((soc_is_exynos5420() || soc_is_exynos5410()) && FLAG_T32_EN)
		return 0;
#endif
	if (!cluster_power_ctrl_en)
		return 0;

	if (soc_is_exynos5410()) {
		if (samsung_rev() < EXYNOS5410_REV_1_0)
			cluster = !cluster;
	}

	if (enable)
		value = EXYNOS_CORE_LOCAL_PWR_EN;

	if ((__raw_readl(EXYNOS_COMMON_STATUS(cluster)) & 0x3) == value)
		return 0;

	if (enable) {
		if (cluster == 0)
			ret = exynos_change_apll_parent(1);
	} else {
		if (cluster == 0)
			ret = exynos_change_apll_parent(0);

		reset_lpj_for_cluster(cluster);
	}

	if (ret)
		return ret;

	__raw_writel(value, EXYNOS_COMMON_CONFIGURATION(cluster));

	/* wait till cluster power control is applied */
	do {
		if ((__raw_readl(EXYNOS_COMMON_STATUS(cluster)) &
		__raw_readl(EXYNOS_L2_STATUS(cluster)) & 0x3) == value)
			return 0;
	} while (time_before(jiffies, timeout));

	return -EDEADLK;
}
#else
static inline int exynos_cluster_power_control(unsigned int cluster, int enable)
{
	return 0;
}
#endif

static int exynos_cluster_power_status(unsigned int cluster)
{
	int status = 0;
	int i;

	if (soc_is_exynos5410()) {
		if (samsung_rev() >= EXYNOS5410_REV_1_0)
			cluster = !cluster;

		cluster = (cluster == 0) ? 4 : 0;
	} else {
		cluster = (cluster == 1) ? 4 : 0;
	}

	for (i = 0; i < NR_CPUS; i++)
		status |= !!(__raw_readl(EXYNOS_ARM_CORE_STATUS(cluster + i)) &
			0x3) << i;

	return status;
}

static int read_mpidr(void)
{
	unsigned int id;
	asm volatile("mrc\tp15, 0, %0, c0, c0, 5" : "=r" (id));
	return id;
}

void bL_debug_info(void)
{
	/*
	 * If abnormal state occurs, prints the power state of 8 cores
	 * @PMU configuration register value
	 * @PMU status register value
	 */
	unsigned int core_num = 0, cores = 0;
	unsigned int config_val = 0, status_val = 0;
	void __iomem *info_addr = (S5P_VA_SYSRAM_NS + 0x28);

	for (cores = 0; cores < 8; cores++) {
		if (cores == 0) {
			pr_info("EAGLE configuration & status register\n");
		} else if (cores == 4) {
			core_num = 0;
			pr_info("KFC configuration & status register\n");
		}
		config_val = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cores));
		status_val = __raw_readl(EXYNOS_ARM_CORE_STATUS(cores));
		pr_info("\t\t%s\tcpu%d\tconfiguration : %#x, status %#x",
			cores > 3 ? "KFC" : "EAGLE", core_num, config_val, status_val);
		core_num++;
	}

	core_num = 0;

	/*
	 * This informations are cpu power state(C2, HOTPLUG, SWITCH) and
	 * GIC ENABLE STATE of group register 0
	 */
	for (cores = 0; cores < 8 ; cores++) {
		if (cores == 0) {
			pr_info("Information of CPU_STATE\n");
		} else if (cores == 4) {
			core_num = 0;
			pr_info("Information of GIC group 0\n");
		}
		pr_info("\t\tcore%d\tvalue : %#x\n", core_num,
			__raw_readl(info_addr + (cores * 4)));
		core_num++;
	}
}

/*
 * bL_power_up - make given CPU in given cluster runable
 *
 * @cpu: CPU number within given cluster
 * @cluster: cluster number for the CPU
 *
 * The identified CPU is brought out of reset.  If the cluster was powered
 * down then it is brought up as well, taking care not to let the other CPUs
 * in the cluster run, and ensuring appropriate cluster setup.
 * Caller must ensure the appropriate entry vector is initialized prior to
 * calling this.
 */
static void bL_power_up(unsigned int cpu, unsigned int cluster)
{
	/*
	 * If this code is performed when cpu state is interrupt disabled,
	 * while() statement is infinite and it doesn't excape. So Add the
	 * variable using loops_per_jiffy.
	 */
	unsigned long timeout = loops_per_jiffy * msecs_to_jiffies(10);
	unsigned long cnt = 0;

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);

#ifdef CONFIG_ARM_TRUSTZONE
	/*
	 * Before turning inbound cpu on, save secure contexts
	 * of outbound
	 */
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_SWITCH, 0);
#endif

	/*
	 * Since this is called with IRQs enabled, and no arch_spin_lock_irq
	 * variant exists, we need to disable IRQs manually here.
	 */
	arch_spin_lock(&exynos_lock);

	/*
	 * If outbound core wake up the first man of inbound cluster,
	 * outbound core must meet conditions which is power-on of
	 * COMMON BLOCK.
	 *
	 * Also non-first cores do not need to perform
	 * exynos_cluster_power_control().
	 */
	if (add_core_count(cluster) == 1)
		exynos_cluster_power_control(cluster, 1);

	arch_spin_unlock(&exynos_lock);

	set_boot_flag(cpu, SWITCH);
	exynos_core_power_control(cpu, cluster, 1);

#ifdef CONFIG_ARM_TRUSTZONE
	if (soc_is_exynos5420() && (samsung_rev() == EXYNOS5420_REV_0)
							&& cpu == 2) {
		unsigned int value;
		while (1) {
			exynos_smc_readsfr(PA_SLEP_PLUGIN + 0x4, &value);
			if (!value) {
				exynos_smc_readsfr(PA_SLEP_PLUGIN, &value);
				exynos_smc(SMC_CMD_REG,
					SMC_REG_ID_SFR_W(PA_SLEP_PLUGIN + 0x4),
					value,
					0);
				sev();
				break;
			}
		}
	}
#endif

	while (1) {
		if (read_gic_flag(cpu) == GIC_ENABLE_STATUS) {
			clear_gic_flag(cpu);
			break;
		}

		if (++cnt > timeout) {
			bL_debug_info();
			panic("LINE:%d of %s\n", __LINE__, __func__);
		}
	}
}

/*
 * bL_power_down - power down given CPU in given cluster
 *
 * @cpu: CPU number within given cluster
 * @cluster: cluster number for the CPU
 *
 * The identified CPU is powered down.  If this is the last CPU still alive
 * in the cluster then the necessary steps to power down the cluster are
 * performed as well.
 *
 * It is assumed that the reset will be effective at the next WFI instruction
 * performed by the target CPU.
 */
static void bL_power_down(unsigned int cpu, unsigned int cluster)
{
	int op_type;
	bool last_man = false;
	/*
	 * If this code is performed when cpu state is interrupt disabled,
	 * while() statement is infinite and it doesn't excape. So Add the
	 * variable using loops_per_jiffy.
	 */
	unsigned long timeout = loops_per_jiffy * msecs_to_jiffies(10);
	unsigned long cnt = 0;

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);

	exynos_core_power_control(cpu, cluster, 0);

	arch_spin_lock(&exynos_lock);

	if (dec_core_count(cluster) == 0)
		last_man = true;
	arch_spin_unlock(&exynos_lock);

	if (last_man) {
		op_type = OP_TYPE_CLUSTER;

		while (1) {
			/* Check for L1 flush of non last cores */
			if (!(exynos_cluster_power_status(cluster) ^ (1 << cpu)))
				break;
			if (++cnt > timeout) {
				bL_debug_info();
				panic("LINE:%d of %s\n", __LINE__, __func__);
			}
		}
	} else {
		op_type = OP_TYPE_CORE;
	}

	cpu_proc_fin();

#ifdef CONFIG_ARM_TRUSTZONE
	/* Now we are prepared for power-down, do it: */
	exynos_smc(SMC_CMD_SHUTDOWN, op_type, SMC_POWERSTATE_SWITCH, 0);
#endif

	/* should never get here */
	BUG();
}

extern void change_all_power_base_to(unsigned int cluster);

static void bL_inbound_setup(unsigned int cpu, unsigned int cluster)
{
	unsigned long timeout;
	unsigned long cnt = 0;

	clear_boot_flag(cpu, SWITCH);

	/*
	 * Controling cluster power is last man of inbound cluster only.
	 * In other words, non-lastman dose not need checks the count
	 * of core every swich.
	 */

	timeout = loops_per_jiffy * msecs_to_jiffies(10);
	if (cpu == 0) {
		change_all_power_base_to(!cluster);

		/* Wait for power off of OB cores */
		while (1) {
			if (!exynos_cluster_power_status(cluster))
				break;
			if (++cnt > timeout) {
				bL_debug_info();
				panic("LINE:%d of %s\n", __LINE__, __func__);
			}
		}

		exynos_cluster_power_control(cluster, 0);
	}
}

extern void bL_power_up_setup(void);

static const struct bL_power_ops bL_control_power_ops = {
	.power_up		= bL_power_up,
	.power_down		= bL_power_down,
	.power_up_setup		= bL_power_up_setup,
	.inbound_setup		= bL_inbound_setup,
};

#if defined(CONFIG_PM)
#define REG_SWITCHING_ADDR	(S5P_VA_SYSRAM_NS + 0x18)

static void bL_resume(void)
{
	/*
	 * REG_SWITCHING_ADDR is in iRAM and this area is reset after
	 * suspend/resume. Thus without setting bl_entry_point,
	 * core switching is not working properly.
	 */
	__raw_writel(virt_to_phys(bl_entry_point), REG_SWITCHING_ADDR);
}
#else
#define bL_resume NULL
#endif

static struct syscore_ops exynos_bL_syscore_ops = {
	.resume		= bL_resume,
};

static int __cpuinit bL_hotplug_cpu_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int cluster = (read_mpidr() >> 8) & 0xf;

	switch (action) {
	case CPU_ONLINE:
		arch_spin_lock(&exynos_lock);
		add_core_count(cluster);
		arch_spin_unlock(&exynos_lock);
		break;
	case CPU_DEAD:
		arch_spin_lock(&exynos_lock);
		dec_core_count(cluster);
		arch_spin_unlock(&exynos_lock);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata bL_hotplug_cpu_notifier = {
	.notifier_call = bL_hotplug_cpu_callback,
};


void exynos_l2_common_power_on(bool on)
{
	unsigned int cluster = (read_mpidr() >> 8) & 0xf;
	unsigned int ob_cluster = cluster ^ 1;

	if (on) {
		if ((__raw_readl(EXYNOS_COMMON_STATUS(ob_cluster)) & 0x3) == 0)
			__raw_writel(EXYNOS_L2_COMMON_PWR_EN,
					EXYNOS_COMMON_CONFIGURATION(ob_cluster));
	} else {
		if ((__raw_readl(EXYNOS_COMMON_STATUS(ob_cluster)) & 0x3) != 0)
			__raw_writel(0, EXYNOS_COMMON_CONFIGURATION(ob_cluster));
	}
}

static ssize_t show_bL_cluster(struct kobject *kobj,
			       struct attribute *attr, char *buf)
{
	size_t len = 0;
	unsigned int i;

	len += sprintf(&buf[len], "Dynamic cluster power control is %s\n",
			cluster_power_ctrl_en ? "enabled" : "disabled");

	for (i = 0; i < CLUSTER_NUM; i++) {
		len += sprintf(&buf[len], "Cluster %d L2 common : ", i);
		len += sprintf(&buf[len], "%s\n",
				(__raw_readl(EXYNOS_COMMON_STATUS(i)) & 0x3) ? "on" : "off");
	}

	return len;
}

static ssize_t store_bL_cluster(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	input = !!input;

	if (input == cluster_power_ctrl_en)
		goto done;

	if (input)
		exynos_l2_common_power_on(false);
	else
		exynos_l2_common_power_on(true);

	cluster_power_ctrl_en = input;

done:
	return count;
}

static struct sysfs_attr bL_cluster =
__ATTR(bL_cluster, S_IRUGO | S_IWUSR, show_bL_cluster, store_bL_cluster);

static int __init bL_control_init(void)
{
	unsigned int cluster_id = (read_mpidr() >> 8) & 0xf;

	/* All future entries into the kernel goes through our entry vectors. */
	__raw_writel(virt_to_phys(bl_entry_point), REG_SWITCHING_ADDR);

	core_count[cluster_id] = NR_CPUS;

	register_syscore_ops(&exynos_bL_syscore_ops);

	if (sysfs_create_file(power_kobj, &bL_cluster.attr))
		pr_err("%s: failed to crate /sys/power/bL_cluster\n", __func__);

	/*
	 * Reaction of cpu hotplug in & out
	 */
	register_cpu_notifier(&bL_hotplug_cpu_notifier);

	/* Getting APLL */
	apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(apll)) {
		pr_err("Fail to get APLL, so this problem is not resolved,");
		pr_err("function of cluster power down is nothing.\n");
	}

	/* Getting FOUT APLL */
	fout_apll = clk_get(NULL, "fout_apll");
	if (IS_ERR(fout_apll))
		pr_err("Fail to get FOUT APLL, so this problem is not resolved,"
			"function of cluster power down is nothing.\n");

	clk_enable(fout_apll);

	return bL_switcher_init(&bL_control_power_ops);
}

late_initcall(bL_control_init);
#endif
