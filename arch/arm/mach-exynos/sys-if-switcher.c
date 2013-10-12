/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * EXYNOS - support to view information of big.LITTLE switcher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/notifier.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>

#include <asm/bL_switcher.h>

#include <mach/regs-pmu.h>
#ifdef CONFIG_EXYNOS5_CCI
#include <mach/map.h>
#endif


struct bus_type bL_subsys = {
	.name = "b.L",
	.dev_name = "b.L",
};

struct bL_info {
	unsigned long long count;
	long long switch_time;
	long long avg_time;
	long long max_time;
	long long min_time;
};

static struct bL_info bi;

#ifdef CONFIG_EXYNOS5_CCI
#define	is_cci_enabled()	true
#else
#define	is_cci_enabled()	false
#endif

static ssize_t exynos_bL_manual_switch_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
#ifndef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
	unsigned int cpu, cluster;

	if (count < 4 || buf[4]) {
		count = -EINVAL;
		goto out;
	}

	/* format: <cpu#>,<cluster#> */
	if (buf[0] < '0' || buf[0] > '3' ||
	    buf[1] != ',' ||
	    buf[2] < '0' || buf[2] > '1') {
		pr_info("Error format\n");
		count = -EINVAL;
		goto out;
	}

	cpu = buf[0] - '0';
	cluster = buf[2] - '0';

	if (unlikely(is_cci_enabled())) {
		if(cpu_online(cpu))
			bL_switch_request(cpu, cluster);
	} else {
		bL_cluster_switch_request(cluster);
	}

out:
	return count;
#else
	pr_info("Currently, IKS_CPUFREQ is enabled, if you wish to test the\n"
		"manual switch then select the userspace governor or\n"
		"disable IKS_CPUFREQ\n");
	return -EINVAL;
#endif
}

#define read_cpu_stat(cpu)	__raw_readl(EXYNOS_ARM_CORE_STATUS(cpu))
#define core_stat(cpu)		((read_cpu_stat(cpu) & 0x3) ? 1 : 0)
#define read_l2_stat(cluster)	__raw_readl(EXYNOS_COMMON_STATUS(cluster))
#define l2_stat(cluster)	((read_l2_stat(cluster) & 0x3) ? 1 : 0)

static unsigned int cci_stat(unsigned int cluster)
{
	void __iomem *cci_base;
	unsigned int offset, ret;

	if (!is_cci_enabled())
		return -EINVAL;

	cci_base = ioremap(EXYNOS5_PA_CCI, SZ_64K);
	if (!cci_base)
		return -1;

	offset = cluster ? 0x4000 : 0x5000;

	ret = (__raw_readl(cci_base + offset) & 0x3) ? 1 : 0;

	iounmap(cci_base);

	return ret;
}

#define	MAX_LEN	(38)

static ssize_t exynos_bL_switcher_stat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int cluster, cpu;

	len += scnprintf(&buf[len], MAX_LEN, "\t0 1 2 3 L2");
	if (unlikely(is_cci_enabled()))
		len += scnprintf(&buf[len], MAX_LEN, " CCI");
	len += scnprintf(&buf[len], MAX_LEN, "\n");
	for (cluster = 0; cluster < 2; cluster++) {
		len += scnprintf(&buf[len], MAX_LEN, cluster ?
				"[A7]   " : "[A15]  ");
		for_each_present_cpu(cpu) {
			unsigned int core = 0;
			core = cluster ? (cpu + NR_CPUS) : cpu;
			len += scnprintf(&buf[len], MAX_LEN, " %d",
					core_stat(core));
		}
		len += scnprintf(&buf[len], MAX_LEN, "  %d", l2_stat(cluster));
		if (unlikely(is_cci_enabled()))
			len += scnprintf(&buf[len], MAX_LEN, "  %d",
					cci_stat(cluster));
		len += scnprintf(&buf[len], MAX_LEN, "\n");
	}
	len += scnprintf(&buf[len], MAX_LEN, "\n");

	return len;
}

static ssize_t exynos_bL_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int cluster;
	size_t len = 0;

	asm ("mrc\tp15, 0, %0, c0, c0, 5" : "=r"(cluster));
	cluster = (cluster >> 8) & 0xff;

	len += scnprintf(buf, MAX_LEN, "cluster      : %s\n",
			cluster ? "LITTLE" : "big");
	len += scnprintf(&buf[len], MAX_LEN,
			"switch count : %llu\n", bi.count);
	len += scnprintf(&buf[len], MAX_LEN,
			"switch time  : %llu ns\n", bi.switch_time);
	len += scnprintf(&buf[len], MAX_LEN,
			"min time     : %llu ns\n", bi.min_time);
	len += scnprintf(&buf[len], MAX_LEN,
			"max time     : %llu ns\n", bi.max_time);
	len += scnprintf(&buf[len], MAX_LEN,
			"avg time     : %llu ns\n", bi.avg_time);

	return len;
}

static struct kobj_attribute exynos_bL_manual_switch_attr =
	__ATTR(b.L_switcher, 0644, NULL, exynos_bL_manual_switch_store);

static struct kobj_attribute exynos_bL_switcher_stat_attr =
	__ATTR(b.L_core_stat, 0644, exynos_bL_switcher_stat_show, NULL);

static struct kobj_attribute exynos_bL_info_attr =
	__ATTR(b.L_info, 0644, exynos_bL_info_show, NULL);

static struct attribute *exynos_bL_sys_if_attrs[] = {
	&exynos_bL_manual_switch_attr.attr,
	&exynos_bL_switcher_stat_attr.attr,
	&exynos_bL_info_attr.attr,
	NULL,
};

static struct attribute_group exynos_bL_sys_if_group = {
	.attrs = exynos_bL_sys_if_attrs,
};

static const struct attribute_group *exynos_bL_sys_if_groups[] = {
	&exynos_bL_sys_if_group,
	NULL,
};

static long long get_ns(void)
{
	struct timespec	ts;
	getnstimeofday(&ts);
	return timespec_to_ns(&ts);
}

static int
exynos_bL_noti_handler(struct notifier_block *nb, unsigned long cmd, void *ptr)
{
	switch (cmd) {
	case SWITCH_ENTER:
		bi.switch_time = get_ns();
		break;
	case SWITCH_EXIT:
		bi.count++;
		bi.switch_time = get_ns() - bi.switch_time;
		if (unlikely(bi.count == 1 && bi.switch_time)) {
			bi.min_time = bi.switch_time;
			bi.max_time = bi.switch_time;
			bi.avg_time = bi.switch_time;
		} else {
			bi.min_time = min(bi.min_time, bi.switch_time);
			bi.max_time = max(bi.max_time, bi.switch_time);
			bi.avg_time = (bi.avg_time + bi.switch_time) / 2;
		}
		break;
	default:
		return NOTIFY_BAD;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_bL_nb = {
	.notifier_call = exynos_bL_noti_handler,
};

static int __init exynos_bL_sys_if_init(void)
{
	int ret;

	if (subsys_system_register(&bL_subsys, exynos_bL_sys_if_groups)) {
		pr_err("Fail to register bL subsystem\n");
		return -ENOMEM;
	}

	ret = register_bL_swicher_notifier(&exynos_bL_nb);
	if (ret)
		pr_err("Fail to register switcher notifier\n");

	return ret;
}

late_initcall(exynos_bL_sys_if_init);
