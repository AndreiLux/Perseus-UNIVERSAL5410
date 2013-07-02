/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#ifdef CONFIG_MEMORY_HOTPLUG
#include <linux/memory.h>
#include <linux/memory_hotplug.h>
#endif

#ifdef CONFIG_ZRAM_FOR_ANDROID
#include <linux/fs.h>
#include <linux/swap.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm_inline.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <asm/atomic.h>

#define MIN_FREESWAP_PAGES 8192 /* 32MB */
#define MIN_RECLAIM_PAGES 512  /* 2MB */
#define MIN_CSWAP_INTERVAL (10*HZ)  /* 10 senconds */
#define RTCC_DAEMON_PROC "rtccd"
#define _KCOMPCACHE_DEBUG 0
#if _KCOMPCACHE_DEBUG
#define lss_dbg(x...) printk("lss: " x)
#else
#define lss_dbg(x...)
#endif

struct soft_reclaim {
	unsigned long nr_total_soft_reclaimed;
	unsigned long nr_total_soft_scanned;
	unsigned long nr_last_soft_reclaimed;
	unsigned long nr_last_soft_scanned;
	int nr_empty_reclaimed;

	atomic_t kcompcached_running;
	atomic_t need_to_reclaim;
	atomic_t lmk_running;
	atomic_t kcompcached_enable;
	atomic_t idle_report;
	struct task_struct *kcompcached;
	struct task_struct *rtcc_daemon;
};

static struct soft_reclaim s_reclaim = {
	.nr_total_soft_reclaimed = 0,
	.nr_total_soft_scanned = 0,
	.nr_last_soft_reclaimed = 0,
	.nr_last_soft_scanned = 0,
	.nr_empty_reclaimed = 0,
	.kcompcached = NULL,
};
extern atomic_t kswapd_thread_on;
static unsigned long prev_jiffy;
int hidden_cgroup_counter = 0;
static uint32_t minimum_freeswap_pages = MIN_FREESWAP_PAGES;
static uint32_t minimun_reclaim_pages = MIN_RECLAIM_PAGES;
static uint32_t minimum_interval_time = MIN_CSWAP_INTERVAL;
#endif /* CONFIG_ZRAM_FOR_ANDROID */

#define ENHANCED_LMK_ROUTINE
#define LMK_COUNT_READ

#ifdef ENHANCED_LMK_ROUTINE
#define LOWMEM_DEATHPENDING_DEPTH 3
#endif

#ifdef LMK_COUNT_READ
static uint32_t lmk_count = 0;
#endif

#ifdef CONFIG_ANDROID_OOM_KILLER
#define MULTIPLE_OOM_KILLER
#define OOM_COUNT_READ
#endif

#ifdef OOM_COUNT_READ
static uint32_t oom_count = 0;
#endif

#ifdef MULTIPLE_OOM_KILLER
#define OOM_DEPTH 5
#endif

static uint32_t lowmem_debug_level = 1;
static int lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;

#ifdef CONFIG_MEMORY_HOTPLUG
static unsigned int offlining = 0;
#endif
#ifdef ENHANCED_LMK_ROUTINE
static struct task_struct *lowmem_deathpending[LOWMEM_DEATHPENDING_DEPTH] = {
	NULL,
};
#else
static struct task_struct *lowmem_deathpending;
#endif
static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			printk(x);			\
	} while (0)

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_nb = {
	.notifier_call	= task_notify_func,
};

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;
#ifdef ENHANCED_LMK_ROUTINE
	int i = 0;

	for (i = 0; i < LOWMEM_DEATHPENDING_DEPTH; i++) {
		if (task == lowmem_deathpending[i]) {
			lowmem_deathpending[i] = NULL;
			break;
		}
	}
#else
	if (task == lowmem_deathpending)
		lowmem_deathpending = NULL;
#endif

	return NOTIFY_OK;
}

#ifdef CONFIG_MEMORY_HOTPLUG
static int lmk_hotplug_callback(struct notifier_block *self,
				unsigned long cmd, void *data)
{
	switch (cmd) {
	/* Don't care LMK cases */
	case MEM_ONLINE:
	case MEM_OFFLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
	case MEM_GOING_ONLINE:
		offlining = 0;
		lowmem_print(4, "lmk in normal mode\n");
		break;
	/* LMK should account for movable zone */
	case MEM_GOING_OFFLINE:
		offlining = 1;
		lowmem_print(4, "lmk in hotplug mode\n");
		break;
	}
	return NOTIFY_DONE;
}
#endif

static int lowmem_shrink(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
#ifdef ENHANCED_LMK_ROUTINE
	struct task_struct *selected[LOWMEM_DEATHPENDING_DEPTH] = {NULL,};
#else
	struct task_struct *selected = NULL;
#endif
	int rem = 0;
	int tasksize;
	int i;
	int min_score_adj = OOM_SCORE_ADJ_MAX + 1;
#ifdef ENHANCED_LMK_ROUTINE
	int selected_tasksize[LOWMEM_DEATHPENDING_DEPTH] = {0,};
	int selected_oom_score_adj[LOWMEM_DEATHPENDING_DEPTH] = {OOM_ADJUST_MAX,};
	int all_selected_oom = 0;
	int max_selected_oom_idx = 0;
#else
	int selected_tasksize = 0;
	int selected_oom_score_adj;
#endif
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM);

#if defined(CONFIG_ZRAM_FOR_ANDROID)
	other_file -= total_swapcache_pages();
#endif

#ifdef CONFIG_MEMORY_HOTPLUG
	struct zone *zone;

	if (offlining) {
		/* Discount all free space in the section being offlined */
		for_each_zone(zone) {
			 if (zone_idx(zone) == ZONE_MOVABLE) {
				other_free -= zone_page_state(zone,
						NR_FREE_PAGES);
				lowmem_print(4, "lowmem_shrink discounted "
					"%lu pages in movable zone\n",
					zone_page_state(zone, NR_FREE_PAGES));
			}
		}
	}
#endif
	/*
	 * If we already have a death outstanding, then
	 * bail out right away; indicating to vmscan
	 * that we have nothing further to offer on
	 * this pass.
	 *
	 */
#ifdef ENHANCED_LMK_ROUTINE
	for (i = 0; i < LOWMEM_DEATHPENDING_DEPTH; i++) {
		if (lowmem_deathpending[i] &&
			time_before_eq(jiffies, lowmem_deathpending_timeout))
			return 0;
	}
#else
	if (lowmem_deathpending &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout))
		return 0;
#endif

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		if (other_free < lowmem_minfree[i] &&
		    other_file < lowmem_minfree[i]) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}
	if (sc->nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %lu, %x, ofree %d %d, ma %d\n",
				sc->nr_to_scan, sc->gfp_mask, other_free,
				other_file, min_score_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (sc->nr_to_scan <= 0 || min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %lu, %x, return %d\n",
			     sc->nr_to_scan, sc->gfp_mask, rem);
		return rem;
	}

#ifdef ENHANCED_LMK_ROUTINE
	for (i = 0; i < LOWMEM_DEATHPENDING_DEPTH; i++)
		selected_oom_score_adj[i] = min_score_adj;
#else
	selected_oom_score_adj = min_score_adj;
#endif

#ifdef CONFIG_ZRAM_FOR_ANDROID
	atomic_set(&s_reclaim.lmk_running, 1);
#endif
	read_lock(&tasklist_lock);
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_score_adj;
#ifdef ENHANCED_LMK_ROUTINE
		int is_exist_oom_task = 0;
#endif

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;

#ifdef ENHANCED_LMK_ROUTINE
		if (all_selected_oom < LOWMEM_DEATHPENDING_DEPTH) {
			for (i = 0; i < LOWMEM_DEATHPENDING_DEPTH; i++) {
				if (!selected[i]) {
					is_exist_oom_task = 1;
					max_selected_oom_idx = i;
					break;
				}
			}
		} else if (selected_oom_score_adj[max_selected_oom_idx] < oom_score_adj ||
			(selected_oom_score_adj[max_selected_oom_idx] == oom_score_adj &&
			selected_tasksize[max_selected_oom_idx] < tasksize)) {
			is_exist_oom_task = 1;
		}

		if (is_exist_oom_task) {
			selected[max_selected_oom_idx] = p;
			selected_tasksize[max_selected_oom_idx] = tasksize;
			selected_oom_score_adj[max_selected_oom_idx] = oom_score_adj;

			if (all_selected_oom < LOWMEM_DEATHPENDING_DEPTH)
				all_selected_oom++;

			if (all_selected_oom == LOWMEM_DEATHPENDING_DEPTH) {
				for (i = 0; i < LOWMEM_DEATHPENDING_DEPTH; i++) {
					if (selected_oom_score_adj[i] < selected_oom_score_adj[max_selected_oom_idx])
						max_selected_oom_idx = i;
					else if (selected_oom_score_adj[i] == selected_oom_score_adj[max_selected_oom_idx] &&
						selected_tasksize[i] < selected_tasksize[max_selected_oom_idx])
						max_selected_oom_idx = i;
				}
			}

			lowmem_print(2, "select %d (%s), adj %d, \
					size %d, to kill\n",
				p->pid, p->comm, oom_score_adj, tasksize);
		}
#else
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_score_adj, tasksize);
#endif
	}
#ifdef ENHANCED_LMK_ROUTINE
	for (i = 0; i < LOWMEM_DEATHPENDING_DEPTH; i++) {
		if (selected[i]) {
			lowmem_print(1, "send sigkill to %d (%s), adj %d,\
				     size %d\n",
				     selected[i]->pid, selected[i]->comm,
				     selected_oom_score_adj[i],
				     selected_tasksize[i]);
			lowmem_deathpending[i] = selected[i];
			lowmem_deathpending_timeout = jiffies + HZ;
			send_sig(SIGKILL, selected[i], 0);
			rem -= selected_tasksize[i];
#ifdef LMK_COUNT_READ
			lmk_count++;
#endif
		}
	}
#else
	if (selected) {
		lowmem_print(1, "send sigkill to %d (%s), adj %d, size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_score_adj, selected_tasksize);
		lowmem_deathpending_timeout = jiffies + HZ;
		send_sig(SIGKILL, selected, 0);
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		rem -= selected_tasksize;
#ifdef LMK_COUNT_READ
		lmk_count++;
#endif
	}
#endif
	lowmem_print(4, "lowmem_shrink %lu, %x, return %d\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	read_unlock(&tasklist_lock);
#ifdef CONFIG_ZRAM_FOR_ANDROID
	atomic_set(&s_reclaim.lmk_running, 0);
#endif
	return rem;
}

/*
 * CONFIG_ANDROID_OOM_KILLER : klaatu@sec
 *
 * The way to select victim by oom-killer provided by
 * linux kernel is totally different from android policy.
 * Hence, it makes more sense that we select the oom victim
 * as android does when LMK is invoked.
 *
*/
#ifdef CONFIG_ANDROID_OOM_KILLER

static void dump_tasks_info(void)
{
	struct task_struct *p;
	struct task_struct *task;

	pr_info("[ pid ]   uid  tgid total_vm      rss cpu oom_adj oom_score_adj name\n");
	for_each_process(p) {
		/* check unkillable tasks */
		if (is_global_init(p))
			continue;
		if (p->flags & PF_KTHREAD)
			continue;

		task = find_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 */
			continue;
		}

		pr_info("[%5d] %5d %5d %8lu %8lu %3u     %3d         %5d %s\n",
			task->pid, task_uid(task), task->tgid,
			task->mm->total_vm, get_mm_rss(task->mm),
			task_cpu(task), task->signal->oom_adj,
			task->signal->oom_score_adj, task->comm);
		task_unlock(task);
	}
}

static int android_oom_handler(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct task_struct *tsk;
#ifdef MULTIPLE_OOM_KILLER
	struct task_struct *selected[OOM_DEPTH] = {NULL,};
#else
	struct task_struct *selected = NULL;
#endif
	int order = (int)val;
	int rem = 0;
	int tasksize;
	int i;
#ifdef MULTIPLE_OOM_KILLER
	int selected_tasksize[OOM_DEPTH] = {0,};
	int selected_oom_score_adj[OOM_DEPTH] = {OOM_ADJUST_MAX,};
	int all_selected_oom = 0;
	int max_selected_oom_idx = 0;
#else
	int selected_tasksize = 0;
	int selected_oom_score_adj;
#endif

	unsigned long *freed = data;

	/* show status */
	pr_warning("%s invoked Android-oom-killer: order=%d, "
		"oom_adj=%d, oom_score_adj=%d\n",
		current->comm, order, current->signal->oom_adj,
		current->signal->oom_score_adj);
	dump_stack();
	show_mem(SHOW_MEM_FILTER_NODES);
	dump_tasks_info();

#ifdef MULTIPLE_OOM_KILLER
	for (i = 0; i < OOM_DEPTH; i++)
		selected_oom_score_adj[i] = lowmem_adj[0];
#else
	selected_oom_score_adj = lowmem_adj[0];
#endif

#ifdef CONFIG_ZRAM_FOR_ANDROID
	atomic_set(&s_reclaim.lmk_running, 1);
#endif

	read_lock(&tasklist_lock);
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_score_adj;
#ifdef MULTIPLE_OOM_KILLER
		int is_exist_oom_task = 0;
#endif

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;

		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;

		lowmem_print(2, "oom: ------ %d (%s), adj %d, size %d\n",
			     p->pid, p->comm, oom_score_adj, tasksize);
#ifdef MULTIPLE_OOM_KILLER
		if (all_selected_oom < OOM_DEPTH) {
			for (i = 0; i < OOM_DEPTH; i++) {
				if (!selected[i]) {
					is_exist_oom_task = 1;
					max_selected_oom_idx = i;
					break;
				}
			}
		} else if (selected_oom_score_adj[max_selected_oom_idx] < oom_score_adj ||
			(selected_oom_score_adj[max_selected_oom_idx] == oom_score_adj &&
			selected_tasksize[max_selected_oom_idx] < tasksize)) {
			is_exist_oom_task = 1;
		}

		if (is_exist_oom_task) {
			selected[max_selected_oom_idx] = p;
			selected_tasksize[max_selected_oom_idx] = tasksize;
			selected_oom_score_adj[max_selected_oom_idx] = oom_score_adj;

			if (all_selected_oom < OOM_DEPTH)
				all_selected_oom++;

			if (all_selected_oom == OOM_DEPTH) {
				for (i = 0; i < OOM_DEPTH; i++) {
					if (selected_oom_score_adj[i] < selected_oom_score_adj[max_selected_oom_idx])
						max_selected_oom_idx = i;
					else if (selected_oom_score_adj[i] == selected_oom_score_adj[max_selected_oom_idx] &&
						selected_tasksize[i] < selected_tasksize[max_selected_oom_idx])
						max_selected_oom_idx = i;
				}
			}

			lowmem_print(2, "oom: max_selected_oom_idx(%d) select %d (%s), adj %d, \
					size %d, to kill\n",
				max_selected_oom_idx, p->pid, p->comm, oom_score_adj, tasksize);
		}
#else
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "oom: select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_score_adj, tasksize);
#endif
	}
#ifdef MULTIPLE_OOM_KILLER
	for (i = 0; i < OOM_DEPTH; i++) {
		if (selected[i]) {
			lowmem_print(1, "oom: send sigkill to %d (%s), adj %d,\
				     size %d\n",
				     selected[i]->pid, selected[i]->comm,
				     selected_oom_score_adj[i],
				     selected_tasksize[i]);
			send_sig(SIGKILL, selected[i], 0);
			rem -= selected_tasksize[i];
			*freed += (unsigned long)selected_tasksize[i];
#ifdef OOM_COUNT_READ
			oom_count++;
#endif

		}
	}
#else
	if (selected) {
		lowmem_print(1, "oom: send sigkill to %d (%s), adj %d, size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_score_adj, selected_tasksize);
		send_sig(SIGKILL, selected, 0);
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		rem -= selected_tasksize;
		*freed += (unsigned long)selected_tasksize;
#ifdef OOM_COUNT_READ
		oom_count++;
#endif
	}
#endif
	read_unlock(&tasklist_lock);

#ifdef CONFIG_ZRAM_FOR_ANDROID
	atomic_set(&s_reclaim.lmk_running, 0);
#endif

	lowmem_print(2, "oom: get memory %lu", *freed);
	return rem;
}

static struct notifier_block android_oom_notifier = {
	.notifier_call = android_oom_handler,
};
#endif /* CONFIG_ANDROID_OOM_KILLER */

#ifdef CONFIG_ZRAM_FOR_ANDROID
void could_cswap(void)
{
	if((hidden_cgroup_counter <= 0) && (atomic_read(&s_reclaim.need_to_reclaim) != 1))
		return;

	if (time_before(jiffies, prev_jiffy + minimum_interval_time))
		return;

	if (atomic_read(&s_reclaim.lmk_running) == 1 || atomic_read(&kswapd_thread_on) == 1) 
		return;

	if (nr_swap_pages < minimum_freeswap_pages)
		return;

	if (unlikely(s_reclaim.kcompcached == NULL))
		return;

	if (likely(atomic_read(&s_reclaim.kcompcached_enable) == 0))
		return;

	if (idle_cpu(task_cpu(s_reclaim.kcompcached)) && this_cpu_loadx(4) == 0) {
		if ((atomic_read(&s_reclaim.idle_report) == 1) && (hidden_cgroup_counter > 0)) {
			if(s_reclaim.rtcc_daemon){
				send_sig(SIGUSR1, s_reclaim.rtcc_daemon, 0);
				hidden_cgroup_counter -- ;
				atomic_set(&s_reclaim.idle_report, 0);
				prev_jiffy = jiffies;
				return;
			}
		}

		if (atomic_read(&s_reclaim.need_to_reclaim) != 1) {
			atomic_set(&s_reclaim.idle_report, 1);
			return;
		}

		if (atomic_read(&s_reclaim.kcompcached_running) == 0) {
			wake_up_process(s_reclaim.kcompcached);
			atomic_set(&s_reclaim.kcompcached_running, 1);
			atomic_set(&s_reclaim.idle_report, 1);
			prev_jiffy = jiffies;
		}
	}
}

inline void enable_soft_reclaim(void)
{
	atomic_set(&s_reclaim.kcompcached_enable, 1);
}

inline void disable_soft_reclaim(void)
{
	atomic_set(&s_reclaim.kcompcached_enable, 0);
}

inline void need_soft_reclaim(void)
{
	atomic_set(&s_reclaim.need_to_reclaim, 1);
}

inline void cancel_soft_reclaim(void)
{
	atomic_set(&s_reclaim.need_to_reclaim, 0);
}

int get_soft_reclaim_status(void)
{
	int kcompcache_running = atomic_read(&s_reclaim.kcompcached_running);
	if(kcompcache_running)
		set_user_nice(s_reclaim.kcompcached, 0);
	return kcompcache_running;
}

static int soft_reclaim(void)
{
	int nid;
	int i;
	unsigned long nr_soft_reclaimed;
	unsigned long nr_soft_scanned;
	unsigned long nr_reclaimed = 0;

	for_each_node_state(nid, N_HIGH_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		for (i = 0; i <= 1; i++) {
			struct zone *zone = pgdat->node_zones + i;
			if (!populated_zone(zone))
				continue;
			if (zone->all_unreclaimable)
				continue;

			nr_soft_scanned = 0;
			nr_soft_reclaimed = mem_cgroup_soft_limit_reclaim(zone,
						0, GFP_KERNEL,
						&nr_soft_scanned);
	
			s_reclaim.nr_last_soft_reclaimed = nr_soft_reclaimed;
			s_reclaim.nr_last_soft_scanned = nr_soft_scanned;
			s_reclaim.nr_total_soft_reclaimed += nr_soft_reclaimed;
			s_reclaim.nr_total_soft_scanned += nr_soft_scanned;
			nr_reclaimed += nr_soft_reclaimed;
		}
	}

	lss_dbg("soft reclaimed %ld pages\n", nr_reclaimed);
	return nr_reclaimed;
}

static int do_compcache(void * nothing)
{
	int ret;
	set_freezable();

	for ( ; ; ) {
		ret = try_to_freeze();
		if (kthread_should_stop())
			break;

		if (soft_reclaim() < minimun_reclaim_pages)
			cancel_soft_reclaim();

		atomic_set(&s_reclaim.kcompcached_running, 0);
		set_current_state(TASK_INTERRUPTIBLE);
		set_user_nice(s_reclaim.kcompcached, 15);
		schedule();
	}

	return 0;
}
static ssize_t rtcc_daemon_store(struct class *class, struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct task_struct *p;
	pid_t pid;
	long val = -1;
	long magic_sign = -1;

	sscanf(buf, "%ld,%ld", &val, &magic_sign);

	if (val < 0 || ((val * val - 1) != magic_sign)) {
		pr_warning("Invalid rtccd pid\n");
		goto out;
	}

	pid = (pid_t)val;
	for_each_process(p) {
		if ((pid == p->pid) && strstr(p->comm, RTCC_DAEMON_PROC)) {
			s_reclaim.rtcc_daemon = p;
			atomic_set(&s_reclaim.idle_report, 1);
			goto out;
		}
	}
	pr_warning("No found rtccd at pid %d!\n", pid);

out:
	return count;
}
static CLASS_ATTR(rtcc_daemon, 0200, NULL, rtcc_daemon_store);
static struct class *kcompcache_class;
#endif /* CONFIG_ZRAM_FOR_ANDROID */


static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	task_free_register(&task_nb);
	register_shrinker(&lowmem_shrinker);
#ifdef CONFIG_ANDROID_OOM_KILLER
	register_oom_notifier(&android_oom_notifier);
#endif
#ifdef CONFIG_MEMORY_HOTPLUG
	hotplug_memory_notifier(lmk_hotplug_callback, 0);
#endif
#ifdef CONFIG_ZRAM_FOR_ANDROID
	kcompcache_class = class_create(THIS_MODULE, "kcompcache");
	if (IS_ERR(kcompcache_class)) {
		pr_err("%s: couldn't create kcompcache sysfs class.\n", __func__);
		goto error_create_kcompcache_class;
	}
	if (class_create_file(kcompcache_class, &class_attr_rtcc_daemon) < 0) {
		pr_err("%s: couldn't create rtcc daemon sysfs file.\n", __func__);
		goto error_create_rtcc_daemon_class_file;
	}
	
	s_reclaim.kcompcached = kthread_run(do_compcache, NULL, "kcompcached");
	if (IS_ERR(s_reclaim.kcompcached)) {
		/* failure at boot is fatal */
		BUG_ON(system_state == SYSTEM_BOOTING);
	}
	set_user_nice(s_reclaim.kcompcached, 0);
	atomic_set(&s_reclaim.need_to_reclaim, 0);
	atomic_set(&s_reclaim.kcompcached_running, 0);
	atomic_set(&s_reclaim.idle_report, 0);
	enable_soft_reclaim();
	prev_jiffy = jiffies;
	return 0;
error_create_rtcc_daemon_class_file:
	class_remove_file(kcompcache_class, &class_attr_rtcc_daemon);
error_create_kcompcache_class:
	class_destroy(kcompcache_class);
#endif /* CONFIG_ZRAM_FOR_ANDROID */
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
	task_free_unregister(&task_nb);

#ifdef CONFIG_ZRAM_FOR_ANDROID
	if (s_reclaim.kcompcached) {
		cancel_soft_reclaim();
		kthread_stop(s_reclaim.kcompcached);
		s_reclaim.kcompcached = NULL;
	}
	if (kcompcache_class) {
		class_remove_file(kcompcache_class, &class_attr_rtcc_daemon);
		class_destroy(kcompcache_class);
	}
#endif /* CONFIG_ZRAM_FOR_ANDROID */
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static int lowmem_oom_adj_to_oom_score_adj(int oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	int oom_adj;
	int oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_int,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
__module_param_call(MODULE_PARAM_PREFIX, adj,
		    &lowmem_adj_array_ops,
		    .arr = &__param_arr_adj,
		    S_IRUGO | S_IWUSR, -1);
__MODULE_PARM_TYPE(adj, "array of int");
#else
module_param_array_named(adj, lowmem_adj, int, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

#ifdef LMK_COUNT_READ
module_param_named(lmkcount, lmk_count, uint, S_IRUGO);
#endif

#ifdef OOM_COUNT_READ
module_param_named(oomcount, oom_count, uint, S_IRUGO);
#endif

#ifdef CONFIG_ZRAM_FOR_ANDROID
module_param_named(min_freeswap, minimum_freeswap_pages, uint, S_IRUSR | S_IWUSR);
module_param_named(min_reclaim, minimun_reclaim_pages, uint, S_IRUSR | S_IWUSR);
module_param_named(min_interval, minimum_interval_time, uint, S_IRUSR | S_IWUSR);
#endif /* CONFIG_ZRAM_FOR_ANDROID */

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

