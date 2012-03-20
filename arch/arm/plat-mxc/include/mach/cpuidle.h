/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/cpuidle.h>

extern int imx5_cpuidle_init(void);
extern int imx6q_cpuidle_init(void);

#ifdef CONFIG_CPU_IDLE
extern void imx_cpuidle_devices_uninit(void);
extern void imx_cpuidle_set_driver(struct cpuidle_driver *p);
extern int imx5_cpuidle_init(void);
extern int imx6q_cpuidle_init(void);
#else
static inline void imx_cpuidle_devices_uninit(void) {}
static inline void imx_cpuidle_set_driver(struct cpuidle_driver *p) {}
#endif
