#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/system.h>
#include <mach/regs-pmu.h>
#include <linux/gpio.h>
#include "common.h"

static void sec_power_off(void)
{
	int poweroff_try = 0;

	local_irq_disable();

	pr_emerg("%s : cable state=%d wpc status=%d\n",
		__func__, is_cable_attached, is_wpc_cable_attached);

	while (1) {
		/* Check reboot charging */
		if ((is_wpc_cable_attached || is_cable_attached ||
				(poweroff_try >= 5)) &&
				(!is_ovlo_state || !lpcharge)) {

			pr_emerg
			    ("%s: charger connected(%d) or power"
			     "off failed(%d), reboot!\n",
			     __func__, is_cable_attached, poweroff_try);
			/* To enter LP charging */
			writel(0x0, EXYNOS_INFORM2);

			flush_cache_all();
			outer_flush_all();
			exynos5_restart(0, 0);

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1)
				;
		}

		/* wait for power button release */
		if (gpio_get_value(GPIO_nPOWER)) {
			pr_emerg("%s: set PS_HOLD low\n", __func__);

			/* power off code
			 * PS_HOLD Out/High -->
			 * Low PS_HOLD_CONTROL, R/W, 0x1002_330C
			 */
			writel(readl(EXYNOS_PS_HOLD_CONTROL) & 0xFFFFFEFF,
			       EXYNOS_PS_HOLD_CONTROL);

			++poweroff_try;
			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, poweroff_try);
		} else {
		/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton is not released.\n", __func__);
		}
		mdelay(1000);
	}
}

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_DOWNLOAD	1
#define REBOOT_MODE_UPLOAD	2
#define REBOOT_MODE_CHARGING	3
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_FOTA	5
#define REBOOT_MODE_FOTA_BL	6	/* update bootloader */
#define REBOOT_MODE_SECURE	7	/* image secure check fail */

#define REBOOT_SET_PREFIX	0xabc00000
#define REBOOT_SET_DEBUG	0x000d0000
#define REBOOT_SET_SWSEL	0x000e0000
#define REBOOT_SET_SUD		0x000f0000

static void sec_reboot(char str, const char *cmd)
{
	local_irq_disable();

	pr_emerg("%s (%d, %s)\n", __func__, str, cmd ? cmd : "(null)");

	writel(0x12345678, EXYNOS_INFORM2);	/* Don't enter lpm mode */

	if (!cmd) {
		writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE, EXYNOS_INFORM3);
	} else {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA,
			       EXYNOS_INFORM3);
		else if (!strcmp(cmd, "fota_bl"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL,
			       EXYNOS_INFORM3);
		else if (!strcmp(cmd, "recovery"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY,
			       EXYNOS_INFORM3);
		else if (!strcmp(cmd, "download"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD,
			       EXYNOS_INFORM3);
		else if (!strcmp(cmd, "upload"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD,
			       EXYNOS_INFORM3);
		else if (!strcmp(cmd, "secure"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_SECURE,
			       EXYNOS_INFORM3);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value,
			       EXYNOS_INFORM3);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value,
			       EXYNOS_INFORM3);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SUD | value,
			       EXYNOS_INFORM3);
		else if (!strncmp(cmd, "emergency", 9))
			writel(0, EXYNOS_INFORM3);
		else
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE,
			       EXYNOS_INFORM3);
	}

	flush_cache_all();
	outer_flush_all();

	exynos5_restart(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1)
		;
}

static int __init sec_reboot_init(void)
{
	pm_power_off = sec_power_off;
	arm_pm_restart = sec_reboot;
	return 0;
}

subsys_initcall(sec_reboot_init);
