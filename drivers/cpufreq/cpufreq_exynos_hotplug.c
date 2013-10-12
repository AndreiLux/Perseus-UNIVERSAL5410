#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG

static bool lcd_is_on;

#define HOTPLUG_OUT_LOAD		10
#define HOTPLUG_OUT_CNT			5

#define HOTPLUG_TRANS			250000
#define HOTPLUG_TRANS_CPUS		1

#define UP_THRESHOLD_FB_BLANK		(90)

/*
 * Increase this value if cpu load is less than base load of hotplug
 * out condition.
 */
static int hotplug_out_cnt;

struct cpumask out_cpus = CPU_MASK_NONE;
static struct cpumask to_be_out_cpus;
static struct work_struct qos_change;
bool hotplug_out;

DEFINE_MUTEX(hotplug_mutex);

void __do_hotplug(void)
{
	unsigned int cpu, ret;

	pr_debug("%s: %s\n", __func__, hotplug_out ? "OUT" : "IN");

	if (hotplug_out) {
		for_each_cpu(cpu, &to_be_out_cpus) {
			if (cpu == 0)
				continue;

			ret = cpu_down(cpu);
			if (ret) {
				pr_debug("%s: CPU%d down fail: %d\n",
					__func__, cpu, ret);
				continue;
			} else {
				cpumask_set_cpu(cpu, &out_cpus);
			}
		}
		cpumask_clear(&to_be_out_cpus);
	} else {
		for_each_cpu(cpu, &out_cpus) {
			if (cpu == 0)
				continue;

			ret = cpu_up(cpu);
			if (ret) {
				pr_debug("%s: CPU%d up fail: %d\n",
					__func__, cpu, ret);
				continue;
			} else {
				cpumask_clear_cpu(cpu, &out_cpus);
			}
		}
	}
	pr_debug("%s: exit\n", __func__);
}

static void change_cpu_qos(struct work_struct *work)
{
	/*
	 * LCD blank CPU qos is set by exynos-ikcs-cpufreq
	 * This line of code release max limit when LCD is
	 * turned on.
	 */
#ifdef CONFIG_ARM_EXYNOS_IKS_CLUSTER
	if (pm_qos_request_active(&max_cpu_qos_blank))
		pm_qos_remove_request(&max_cpu_qos_blank);
#endif
	return;
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		dbs_tuners_ins.up_threshold_l = UP_THRESHOLD_FB_BLANK;
		lcd_is_on = false;
		break;
	case FB_BLANK_UNBLANK:
		/*
		 * To prevent lock inversion problem when aquringconsole_lock()
		 * Do not call pm_qos request on cpufreq in fb notifier callback.
		 */
		schedule_work_on(0, &qos_change);
		dbs_tuners_ins.up_threshold_l = MICRO_FREQUENCY_UP_THRESHOLD_L;
		lcd_is_on = true;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};
#endif
