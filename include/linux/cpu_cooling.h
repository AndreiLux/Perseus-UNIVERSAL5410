/*
 *  linux/include/linux/cpu_cooling.h
 *
 *  Copyright (C) 2011	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2011  Amit Daniel <amit.kachhap@linaro.org>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __CPU_COOLING_H__
#define __CPU_COOLING_H__

#include <linux/thermal.h>

#define CPUFREQ_COOLING_TYPE		0
#define CPUHOTPLUG_COOLING_TYPE		1

struct freq_clip_table {
	unsigned int freq_clip_max;
	unsigned int polling_interval;
	unsigned int temp_level;
	const struct cpumask *mask_val;
};

int cputherm_register_notifier(struct notifier_block *nb, unsigned int list);
int cputherm_unregister_notifier(struct notifier_block *nb, unsigned int list);

#ifdef CONFIG_CPU_FREQ
struct thermal_cooling_device *cpufreq_cooling_register(
	struct freq_clip_table *tab_ptr, unsigned int tab_size,
	const struct cpumask *mask_val, enum thermal_trip_type type);

void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev);
#else /*!CONFIG_CPU_FREQ*/
static inline struct thermal_cooling_device *cpufreq_cooling_register(
	struct freq_clip_table *tab_ptr, unsigned int tab_size,
	const struct cpumask *mask_val, enum thermal_trip_type type)
{
	return NULL;
}
static inline void cpufreq_cooling_unregister(
				struct thermal_cooling_device *cdev)
{
	return;
}
#endif	/*CONFIG_CPU_FREQ*/
#ifdef CONFIG_HOTPLUG_CPU
extern struct thermal_cooling_device *cpuhotplug_cooling_register(
	const struct cpumask *mask_val);

extern void cpuhotplug_cooling_unregister(struct thermal_cooling_device *cdev);
#else /*!CONFIG_HOTPLUG_CPU*/
static inline struct thermal_cooling_device *cpuhotplug_cooling_register(
	const struct cpumask *mask_val)
{
	return NULL;
}
static inline void cpuhotplug_cooling_unregister(
				struct thermal_cooling_device *cdev)
{
	return;
}
#endif /*CONFIG_HOTPLUG_CPU*/

#endif /* __CPU_COOLING_H__ */
