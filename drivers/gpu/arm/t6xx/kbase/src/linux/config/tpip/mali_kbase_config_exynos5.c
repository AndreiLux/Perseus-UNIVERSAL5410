/*
 *
 * (C) COPYRIGHT 2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>

#include <mach/map.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/regs-pmu.h>
#include <asm/delay.h>
#include <mach/map.h>
#include <generated/autoconf.h>

#include <linux/timer.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_pm.h>
#include <kbase/src/common/mali_kbase_uku.h>
#include <kbase/src/common/mali_kbase_mem.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/linux/mali_kbase_mem_linux.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/src/linux/mali_kbase_config_linux.h>

#include <kbase/src/linux/mali_linux_dvfs_trace.h>

#if CONFIG_MALI_UNCACHED == 0
#error CONFIG_MALI_UNCACHED should equal 1 for Exynos5 support, your scons commandline should contain 'no_syncsets=1'
#endif

#define MALI_DVFS_DEBUG 0
#define MALI_DVFS_STEP 7
#define MALI_DVFS_KEEP_STAY_CNT 10

#ifdef CONFIG_T6XX_DVFS
#ifdef CONFIG_CPU_FREQ
#define MALI_DVFS_ASV_ENABLE
#endif
#endif

#ifdef MALI_DVFS_ASV_ENABLE
#include <mach/busfreq_exynos5.h>
#define MALI_DVFS_ASV_GROUP_SPECIAL_NUM 10
#define MALI_DVFS_ASV_GROUP_NUM 12
#endif

#define HZ_IN_MHZ            (1000000)
#define MALI_RTPM_DEBUG      0
#define VITHAR_DEFAULT_CLOCK 533000000
#define RUNTIME_PM_DELAY_TIME 10
#define CONFIG_T6XX_HWVER_R0P0 1

struct regulator *kbase_platform_get_regulator(void);
int kbase_platform_regulator_init(void);
int kbase_platform_regulator_disable(void);
int kbase_platform_regulator_enable(void);
int kbase_platform_get_default_voltage(struct device *dev, int *vol);
int kbase_platform_get_voltage(struct device *dev, int *vol);
int kbase_platform_set_voltage(struct device *dev, int vol);
void kbase_platform_dvfs_set_clock(kbase_device *kbdev, int freq);
void kbase_platform_dvfs_set_level(kbase_device *kbdev, int level);
int kbase_platform_dvfs_get_level(int freq);

#ifdef CONFIG_T6XX_DVFS
int kbase_platform_dvfs_init(kbase_device *kbdev);
void kbase_platform_dvfs_term(void);
int kbase_platform_dvfs_get_control_status(void);
int kbase_platform_dvfs_get_utilisation(void);
#ifdef MALI_DVFS_ASV_ENABLE
static int kbase_platform_asv_set(int enable);
static int kbase_platform_dvfs_sprint_avs_table(char *buf);
#endif /* MALI_DVFS_ASV_ENABLE */
#endif /* CONFIG_T6XX_DVFS */

int kbase_platform_cmu_pmu_control(struct kbase_device *kbdev, int control);
void kbase_platform_remove_sysfs_file(struct device *dev);
mali_error kbase_platform_init(struct kbase_device *kbdev);
static int kbase_platform_is_power_on(void);
void kbase_platform_term(struct kbase_device *kbdev);

#ifdef CONFIG_T6XX_DEBUG_SYS
static int kbase_platform_create_sysfs_file(struct device *dev);
#endif /* CONFIG_T6XX_DEBUG_SYS */

static int mali_setup_system_tracing(struct device *dev);
static void mali_cleanup_system_tracing(struct device *dev);

struct exynos_context
{
	/** Indicator if system clock to mail-t604 is active */
	int cmu_pmu_status;
	/** cmd & pmu lock */
	spinlock_t cmu_pmu_lock;
	struct clk *sclk_g3d;
};

static kbase_io_resources io_resources =
{
	.job_irq_number   = EXYNOS5_JOB_IRQ_NUMBER,
	.mmu_irq_number   = EXYNOS5_MMU_IRQ_NUMBER,
	.gpu_irq_number   = EXYNOS5_GPU_IRQ_NUMBER,
	.io_memory_region =
	{
		.start = EXYNOS5_PA_G3D,
		.end   = EXYNOS5_PA_G3D + (4096 * 5) - 1
	}
};

atomic_t mali_memory_pages;

/**
 * Read the CPU clock speed
 */
int get_cpu_clock_speed(u32* cpu_clock)
{
	struct clk * cpu_clk;
	u32 freq=0;
	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return 1;
	freq = clk_get_rate(cpu_clk);
	*cpu_clock = (freq/HZ_IN_MHZ);
	return 0;
}

/**
 * Power Management callback - power ON
 */
static int pm_callback_power_on(kbase_device *kbdev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_resume(kbdev->osdev.dev);
#endif /* CONFIG_PM_RUNTIME */
	return 0;
}

/**
 * Power Management callback - power OFF
 */
static void pm_callback_power_off(kbase_device *kbdev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_schedule_suspend(kbdev->osdev.dev, RUNTIME_PM_DELAY_TIME);
#endif /* CONFIG_PM_RUNTIME */
}

/**
 * Power Management callback - runtime power ON
 */
#ifdef CONFIG_PM_RUNTIME
static int pm_callback_runtime_power_on(kbase_device *kbdev)
{
#if MALI_RTPM_DEBUG
	printk("kbase_device_runtime_resume\n");
#endif /* MALI_RTPM_DEBUG */
	return kbase_platform_cmu_pmu_control(kbdev, 1);
}
#endif /* CONFIG_PM_RUNTIME */

/**
 * Power Management callback - runtime power OFF
 */
#ifdef CONFIG_PM_RUNTIME
static void pm_callback_runtime_power_off(kbase_device *kbdev)
{
#if MALI_RTPM_DEBUG
	printk("kbase_device_runtime_suspend\n");
#endif /* MALI_RTPM_DEBUG */
	kbase_platform_cmu_pmu_control(kbdev, 0);
}
#endif /* CONFIG_PM_RUNTIME */

static kbase_pm_callback_conf pm_callbacks =
{
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
};

/**
 * Exynos5 hardware specific initialization
 */
mali_bool kbase_platform_exynos5_init(kbase_device *kbdev)
{
	if(MALI_ERROR_NONE == kbase_platform_init(kbdev))
	{
#ifdef CONFIG_T6XX_DEBUG_SYS
		if(kbase_platform_create_sysfs_file(kbdev->osdev.dev))
		{
			return MALI_TRUE;
		}
#endif /* CONFIG_T6XX_DEBUG_SYS */
		return MALI_TRUE;
	}

	return MALI_FALSE;
}

/**
 * Exynos5 hardware specific termination
 */
void kbase_platform_exynos5_term(kbase_device *kbdev)
{
#ifdef CONFIG_T6XX_DEBUG_SYS
	kbase_platform_remove_sysfs_file(kbdev->osdev.dev);
#endif /* CONFIG_T6XX_DEBUG_SYS */
	kbase_platform_term(kbdev);
}

kbase_platform_funcs_conf platform_funcs =
{
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

static kbase_attribute config_attributes[] = {
	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
		2048 * 1024 * 1024UL /* 2048MB */
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
		KBASE_MEM_PERF_FAST
	},
	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
		(uintptr_t)&pm_callbacks
	},
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&platform_funcs
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
		533000
	},
	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
		100000
	},
	{
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		500 /* 500ms before cancelling stuck jobs */
	},
	{
		KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
		(uintptr_t)&get_cpu_clock_speed
	},
	{
		KBASE_CONFIG_ATTR_END,
		0
	}
};

kbase_platform_config platform_config =
{
	.attributes   = config_attributes,
	.io_resources = &io_resources,
	.midgard_type = KBASE_MALI_T604
};

static struct clk *clk_g3d = NULL;

/**
 * Initialize GPU clocks
 */
static int kbase_platform_power_clock_init(kbase_device *kbdev)
{
	struct device *dev =  kbdev->osdev.dev;
	int timeout;
	struct exynos_context *platform;

	platform = (struct exynos_context *) kbdev->platform_context;
	if(NULL == platform)
	{
		panic("oops");
	}

	/* Turn on G3D power */
	__raw_writel(0x7, EXYNOS5_G3D_CONFIGURATION);

	/* Wait for G3D power stability for 1ms */
	timeout = 10;
	while((__raw_readl(EXYNOS5_G3D_STATUS) & 0x7) != 0x7) {
		if(timeout == 0) {
			/* need to call panic  */
			panic("failed to turn on g3d power\n");
			goto out;
		}
		timeout--;
		udelay(100);
	}

	/* Turn on G3D clock */
	clk_g3d = clk_get(dev, "g3d");
	if(IS_ERR(clk_g3d)) {
		clk_g3d = NULL;
		OSK_PRINT_ERROR(OSK_BASE_PM, "failed to clk_get [clk_g3d]\n");
		/* chrome linux does not have this clock */
	}
	else
	{
		/* android_v4 support */
		clk_enable(clk_g3d);
		printk("v4 support\n");
	}

#ifdef CONFIG_T6XX_HWVER_R0P0
	platform->sclk_g3d = clk_get(dev, "aclk_400_g3d");
	if(IS_ERR(platform->sclk_g3d)) {
		OSK_PRINT_ERROR(OSK_BASE_PM, "failed to clk_get [sclk_g3d]\n");
		goto out;
	}
#else /* CONFIG_T6XX_HWVER_R0P0 */
	{
		struct clk *mpll = NULL;
		mpll = clk_get(dev, "mout_mpll_user");
		if(IS_ERR(mpll)) {
			OSK_PRINT_ERROR(OSK_BASE_PM, "failed to clk_get [mout_mpll_user]\n");
			goto out;
		}

		platform->sclk_g3d = clk_get(dev, "sclk_g3d");
		if(IS_ERR(platform->sclk_g3d)) {
			OSK_PRINT_ERROR(OSK_BASE_PM, "failed to clk_get [sclk_g3d]\n");
			goto out;
		}

		clk_set_parent(platform->sclk_g3d, mpll);
		if(IS_ERR(platform->sclk_g3d)) {
			OSK_PRINT_ERROR(OSK_BASE_PM, "failed to clk_set_parent\n");
			goto out;
		}

		clk_set_rate(platform->sclk_g3d, VITHAR_DEFAULT_CLOCK);
		if(IS_ERR(platform->sclk_g3d)) {
			OSK_PRINT_ERROR(OSK_BASE_PM, "failed to clk_set_rate [sclk_g3d] = %d\n", VITHAR_DEFAULT_CLOCK);
			goto out;
		}
	}
#endif /*  CONFIG_T6XX_HWVER_R0P0 */
	(void) clk_enable(platform->sclk_g3d);
	return 0;
out:
	return -EPERM;
}

/**
 * Enable GPU clocks
 */
static int kbase_platform_clock_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if(clk_g3d)
	{
		/* android_v4 support */
		(void) clk_enable(clk_g3d);
	}
	else
	{
		/* chrome support */
		(void) clk_enable(platform->sclk_g3d);
	}

	return 0;
}

/**
 * Disable GPU clocks
 */
static int kbase_platform_clock_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if(clk_g3d)
	{
		/* android_v4 support */
		(void)clk_disable(clk_g3d);
	}
	else
	{
		/* chrome support */
		(void)clk_disable(platform->sclk_g3d);
	}
	return 0;
}

/**
 * Report GPU power status
 */
static inline int kbase_platform_is_power_on(void)
{
	return ((__raw_readl(EXYNOS5_G3D_STATUS) & 0x7) == 0x7) ? 1 : 0;
}

/**
 * Enable GPU power
 */
static int kbase_platform_power_on(void)
{
	int timeout;

	/* Turn on G3D  */
	__raw_writel(0x7, EXYNOS5_G3D_CONFIGURATION);

	/* Wait for G3D power stability */
	timeout = 1000;

	while((__raw_readl(EXYNOS5_G3D_STATUS) & 0x7) != 0x7) {
		if(timeout == 0) {
			/* need to call panic  */
			panic("failed to turn on g3d via g3d_configuration\n");
			return -ETIMEDOUT;
		}
		timeout--;
		udelay(10);
	}

	return 0;
}

/**
 * Disable GPU power
 */
static int kbase_platform_power_off(void)
{
	int timeout;

	/* Turn off G3D  */
	__raw_writel(0x0, EXYNOS5_G3D_CONFIGURATION);

	/* Wait for G3D power stability */
	timeout = 1000;

	while(__raw_readl(EXYNOS5_G3D_STATUS) & 0x7) {
		if(timeout == 0) {
			/* need to call panic */
			panic( "failed to turn off g3d via g3d_configuration\n");
			return -ETIMEDOUT;
		}
		timeout--;
		udelay(10);
	}

	return 0;
}

/**
 * Power Management unit control. Enable/disable power and clocks to GPU
 */
int kbase_platform_cmu_pmu_control(struct kbase_device *kbdev, int control)
{
	unsigned long flags;
	struct exynos_context *platform;
	if (!kbdev)
	{
		return -ENODEV;
	}

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
	{
		return -ENODEV;
	}

	spin_lock_irqsave(&platform->cmu_pmu_lock, flags);

	/* off */
	if(control == 0)
	{
		if(platform->cmu_pmu_status == 0)
		{
			spin_unlock_irqrestore(&platform->cmu_pmu_lock, flags);
			return 0;
		}

		if(kbase_platform_power_off())
			panic("failed to turn off g3d power\n");
		if(kbase_platform_clock_off(kbdev))

			panic("failed to turn off sclk_g3d\n");

		platform->cmu_pmu_status = 0;
#if MALI_RTPM_DEBUG
		printk( KERN_ERR "3D cmu_pmu_control - off\n" );
#endif /* MALI_RTPM_DEBUG */
	}
	else
	{
		/* on */
		if(platform->cmu_pmu_status == 1)
		{
			spin_unlock_irqrestore(&platform->cmu_pmu_lock, flags);
			return 0;
		}

		if(kbase_platform_clock_on(kbdev))
			panic("failed to turn on sclk_g3d\n");
		if(kbase_platform_power_on())
			panic("failed to turn on g3d power\n");

		platform->cmu_pmu_status = 1;
#if MALI_RTPM_DEBUG
		printk( KERN_ERR "3D cmu_pmu_control - on\n");
#endif /* MALI_RTPM_DEBUG */
	}

	spin_unlock_irqrestore(&platform->cmu_pmu_lock, flags);

	return 0;
}

#ifdef CONFIG_T6XX_DEBUG_SYS
/** The sysfs file @c clock, fbdev.
 *
 * This is used for obtaining information about the vithar
 * operating clock & framebuffer address,
 */

static ssize_t mali_sysfs_show_clock(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	ssize_t ret = 0;
	unsigned int clkrate;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *) kbdev->platform_context;
	if(!platform)
		return -ENODEV;

	if(!platform->sclk_g3d)
		return -ENODEV;

	clkrate = clk_get_rate(platform->sclk_g3d);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Current sclk_g3d[G3D_BLK] = %dMhz", clkrate/1000000);

	/* To be revised  */
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "\nPossible settings : 533, 450, 400, 350, 266, 160, 100Mhz");
	if (ret < PAGE_SIZE - 1)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	else
	{
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t mali_sysfs_set_clock(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct exynos_context *platform;
	unsigned int tmp = 0, freq = 0 ;

	if (!kbdev) {
		pr_err("%s: no kbdev\n", __func__);
		return -ENODEV;
	}

	platform = (struct exynos_context *) kbdev->platform_context;
	if (platform == NULL) {
		pr_err("%s: no platform\n", __func__);
		return -ENODEV;
	}
	if (!platform->sclk_g3d) {
		pr_info("%s: clkout not 3d\n", __func__);
		return -ENODEV;
	}

	/* TODO(dianders) need to be more careful fiddling voltage+clock */
	if (sysfs_streq("533", buf)) {
		freq = 533;
	} else if (sysfs_streq("450", buf)) {
		freq = 450;
	} else if (sysfs_streq("400", buf)) {
		freq = 400;
	} else if (sysfs_streq("350", buf)) {
		freq = 350;
	} else if (sysfs_streq("266", buf)) {
		freq = 266;
	} else if (sysfs_streq("160", buf)) {
		freq = 160;
	} else if (sysfs_streq("100", buf)) {
		freq = 100;
	} else {
		pr_err("%s: invalid value\n", __func__);
		return -ENOENT;
	}

	pr_info("aclk400 %u\n", (unsigned int)clk_get_rate(platform->sclk_g3d));
	kbase_platform_dvfs_set_level(kbdev,
		kbase_platform_dvfs_get_level(freq));
	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(EXYNOS5_CLKDIV_STAT_TOP0);
	} while (tmp & 0x1000000);

	return count;
}
DEVICE_ATTR(clock, S_IRUGO|S_IWUSR, mali_sysfs_show_clock,
	mali_sysfs_set_clock);

static ssize_t mali_sysfs_show_fbdev(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;
	int i;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	for(i = 0 ; i < num_registered_fb ; i++) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "fb[%d] xres=%d, yres=%d, addr=0x%lx\n", i, registered_fb[i]->var.xres, registered_fb[i]->var.yres, registered_fb[i]->fix.smem_start);
	}

	if (ret < PAGE_SIZE - 1)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	else
	{
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}
DEVICE_ATTR(fbdev, S_IRUGO, mali_sysfs_show_fbdev, NULL);

static ssize_t mali_sysfs_show_memory(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	int ret;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	ret = sprintf(buf, "%lu bytes\n",
		      atomic_read(&mali_memory_pages) * PAGE_SIZE);

	return ret;
}
DEVICE_ATTR(memory, S_IRUGO, mali_sysfs_show_memory, NULL);

typedef enum {
	L1_I_tag_RAM = 0x00,
	L1_I_data_RAM = 0x01,
	L1_I_BTB_RAM = 0x02,
	L1_I_GHB_RAM = 0x03,
	L1_I_TLB_RAM = 0x04,
	L1_I_indirect_predictor_RAM = 0x05,
	L1_D_tag_RAM = 0x08,
	L1_D_data_RAM = 0x09,
	L1_D_load_TLB_array = 0x0A,
	L1_D_store_TLB_array = 0x0B,
	L2_tag_RAM = 0x10,
	L2_data_RAM = 0x11,
	L2_snoop_tag_RAM = 0x12,
	L2_data_ECC_RAM = 0x13,
	L2_dirty_RAM = 0x14,
	L2_TLB_RAM = 0x18
} RAMID_type;

static inline void asm_ramindex_mrc(u32 *DL1Data0, u32 *DL1Data1,
	u32 *DL1Data2, u32 *DL1Data3)
{
	u32 val;

	if(DL1Data0)
	{
		asm volatile("mrc p15, 0, %0, c15, c1, 0" : "=r" (val));
		*DL1Data0 = val;
	}
	if(DL1Data1)
	{
		asm volatile("mrc p15, 0, %0, c15, c1, 1" : "=r" (val));
		*DL1Data1 = val;
	}
	if(DL1Data2)
	{
		asm volatile("mrc p15, 0, %0, c15, c1, 2" : "=r" (val));
		*DL1Data2 = val;
	}
	if(DL1Data3)
	{
		asm volatile("mrc p15, 0, %0, c15, c1, 3" : "=r" (val));
		*DL1Data3 = val;
	}
}

static inline void asm_ramindex_mcr(u32 val)
{
	asm volatile("mcr p15, 0, %0, c15, c4, 0" : : "r" (val));
	asm volatile("dsb");
	asm volatile("isb");
}

static void get_tlb_array(u32 val, u32 *DL1Data0, u32 *DL1Data1,
	u32 *DL1Data2, u32 *DL1Data3)
{
	asm_ramindex_mcr(val);
	asm_ramindex_mrc(DL1Data0, DL1Data1, DL1Data2, DL1Data3);
}

static RAMID_type ramindex = L1_D_load_TLB_array;
static ssize_t mali_sysfs_show_dtlb(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;
	int entries, ways;
	u32 DL1Data0 = 0, DL1Data1 = 0, DL1Data2 = 0, DL1Data3 = 0;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	/* L1-I tag RAM */
	if(ramindex == L1_I_tag_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-I data RAM */
	else if(ramindex == L1_I_data_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-I BTB RAM */
	else if(ramindex == L1_I_BTB_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-I GHB RAM */
	else if(ramindex == L1_I_GHB_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-I TLB RAM */
	else if(ramindex == L1_I_TLB_RAM)
	{
		printk("L1-I TLB RAM\n");
		for(entries = 0 ; entries < 32 ; entries++)
		{
			get_tlb_array((((u8)ramindex) << 24) + entries, &DL1Data0, &DL1Data1, &DL1Data2, NULL);
			printk("entries[%d], DL1Data0=%08x, DL1Data1=%08x DL1Data2=%08x\n", entries, DL1Data0, DL1Data1 & 0xffff, 0x0);
		}
	}
	/* L1-I indirect predictor RAM */
	else if(ramindex == L1_I_indirect_predictor_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-D tag RAM */
	else if(ramindex == L1_D_tag_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-D data RAM */
	else if(ramindex == L1_D_data_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L1-D load TLB array */
	else if(ramindex == L1_D_load_TLB_array)
	{
		printk("L1-D load TLB array\n");
		for(entries = 0 ; entries < 32 ; entries++)
		{
		get_tlb_array((((u8)ramindex) << 24) + entries, &DL1Data0, &DL1Data1, &DL1Data2, &DL1Data3);
		printk("entries[%d], DL1Data0=%08x, DL1Data1=%08x, DL1Data2=%08x, DL1Data3=%08x\n", entries, DL1Data0, DL1Data1, DL1Data2, DL1Data3 & 0x3f);
		}
	}
	/* L1-D store TLB array */
	else if(ramindex == L1_D_store_TLB_array)
	{
		printk("\nL1-D store TLB array\n");
		for(entries = 0 ; entries < 32 ; entries++)
		{
			get_tlb_array((((u8)ramindex) << 24) + entries, &DL1Data0, &DL1Data1, &DL1Data2, &DL1Data3);
			printk("entries[%d], DL1Data0=%08x, DL1Data1=%08x, DL1Data2=%08x, DL1Data3=%08x\n", entries, DL1Data0, DL1Data1, DL1Data2, DL1Data3 & 0x3f);
		}
	}
	/* L2 tag RAM */
	else if(ramindex == L2_tag_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L2 data RAM */
	else if(ramindex == L2_data_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L2 snoop tag RAM */
	else if(ramindex == L2_snoop_tag_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L2 data ECC RAM */
	else if(ramindex == L2_data_ECC_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L2 dirty RAM */
	else if(ramindex == L2_dirty_RAM)
	{
		printk("Not implemented yet\n");
	}
	/* L2 TLB array */
	else if(ramindex == L2_TLB_RAM)
	{
		printk("\nL2 TLB array\n");
		for(ways = 0 ; ways < 4 ; ways++)
		{
			for(entries = 0 ; entries < 512 ; entries++)
			{
				get_tlb_array((ramindex << 24) + (ways << 18) + entries, &DL1Data0, &DL1Data1, &DL1Data2, &DL1Data3);
				printk("ways[%d]:entries[%d], DL1Data0=%08x, DL1Data1=%08x, DL1Data2=%08x, DL1Data3=%08x\n", ways, entries, DL1Data0, DL1Data1, DL1Data2, DL1Data3);
			}
		}
	}

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Succeeded...\n");

	if (ret < PAGE_SIZE - 1)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	else
	{
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
	return ret;
}

static ssize_t mali_sysfs_set_dtlb(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	if (sysfs_streq("L1_I_tag_RAM", buf)) {
		ramindex = L1_I_tag_RAM;
	} else if (sysfs_streq("L1_I_data_RAM", buf)) {
		ramindex = L1_I_data_RAM;
	} else if (sysfs_streq("L1_I_BTB_RAM", buf)) {
		ramindex = L1_I_BTB_RAM;
	} else if (sysfs_streq("L1_I_GHB_RAM", buf)) {
		ramindex = L1_I_GHB_RAM;
	} else if (sysfs_streq("L1_I_TLB_RAM", buf)) {
		ramindex = L1_I_TLB_RAM;
	} else if (sysfs_streq("L1_I_indirect_predictor_RAM", buf)) {
		ramindex = L1_I_indirect_predictor_RAM;
	} else if (sysfs_streq("L1_D_tag_RAM", buf)) {
		ramindex = L1_D_tag_RAM;
	} else if (sysfs_streq("L1_D_data_RAM", buf)) {
		ramindex = L1_D_data_RAM;
	} else if (sysfs_streq("L1_D_load_TLB_array", buf)) {
		ramindex = L1_D_load_TLB_array;
	} else if (sysfs_streq("L1_D_store_TLB_array", buf)) {
		ramindex = L1_D_store_TLB_array;
	} else if (sysfs_streq("L2_tag_RAM", buf)) {
		ramindex = L2_tag_RAM;
	} else if (sysfs_streq("L2_data_RAM", buf)) {
		ramindex = L2_data_RAM;
	} else if (sysfs_streq("L2_snoop_tag_RAM", buf)) {
		ramindex = L2_snoop_tag_RAM;
	} else if (sysfs_streq("L2_data_ECC_RAM", buf)) {
		ramindex = L2_data_ECC_RAM;
	} else if (sysfs_streq("L2_dirty_RAM", buf)) {
		ramindex = L2_dirty_RAM;
	} else if (sysfs_streq("L2_TLB_RAM", buf)) {
		ramindex = L2_TLB_RAM;
	} else {
		printk("Invalid value....\n\n");
		printk("Available options are one of below\n");
		printk("L1_I_tag_RAM, L1_I_data_RAM, L1_I_BTB_RAM\n");
		printk("L1_I_GHB_RAM, L1_I_TLB_RAM, L1_I_indirect_predictor_RAM\n");
		printk("L1_D_tag_RAM, L1_D_data_RAM, L1_D_load_TLB_array, L1_D_store_TLB_array\n");
		printk("L2_tag_RAM, L2_data_RAM, L2_snoop_tag_RAM, L2_data_ECC_RAM\n");
		printk("L2_dirty_RAM, L2_TLB_RAM\n");
	}

	return count;
}
DEVICE_ATTR(dtlb, S_IRUGO|S_IWUSR, mali_sysfs_show_dtlb, mali_sysfs_set_dtlb);

static ssize_t mali_sysfs_show_vol(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;
	int vol;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	kbase_platform_get_voltage(dev, &vol);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Current operating voltage for mali t6xx = %d", vol);

	if (ret < PAGE_SIZE - 1)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	else
	{
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}
DEVICE_ATTR(vol, S_IRUGO|S_IWUSR, mali_sysfs_show_vol, NULL);

static int get_clkout_cmu_top(int *val)
{
	*val = __raw_readl(/*EXYNOS5_CLKOUT_CMU_TOP*/EXYNOS_CLKREG(0x10A00));
	if((*val & 0x1f) == 0xB) /* CLKOUT is ACLK_400 in CLKOUT_CMU_TOP */
		return 1;
	else
		return 0;
}

static void set_clkout_for_3d(void)
{
	int tmp;

	tmp = 0x0;
	tmp |= 0x1000B; /* ACLK_400 selected */
	tmp |= 9 << 8;  /* divided by (9 + 1) */
	__raw_writel(tmp, /*EXYNOS5_CLKOUT_CMU_TOP*/EXYNOS_CLKREG(0x10A00));

#ifdef PMU_XCLKOUT_SET
	exynos5_pmu_xclkout_set(1, XCLKOUT_CMU_TOP);
#else /* PMU_XCLKOUT_SET */
	tmp = 0x0;
	tmp |= 7 << 8; /* CLKOUT_CMU_TOP selected */
	__raw_writel(tmp, /*S5P_PMU_DEBUG*/S5P_PMUREG(0x0A00));
#endif /* PMU_XCLKOUT_SET */
}

static ssize_t mali_sysfs_show_clkout(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;
	int val;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	if(get_clkout_cmu_top(&val))
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "Current CLKOUT is g3d divided by 10, CLKOUT_CMU_TOP=0x%x", val);
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "Current CLKOUT is not g3d, CLKOUT_CMU_TOP=0x%x", val);

	if (ret < PAGE_SIZE - 1)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	else
	{
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t mali_sysfs_set_clkout(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	set_clkout_for_3d();
	pr_info("clkout set to 3d\n");
	return count;
}
DEVICE_ATTR(clkout, S_IRUGO|S_IWUSR, mali_sysfs_show_clkout,
	mali_sysfs_set_clkout);

#ifdef CONFIG_T6XX_DVFS
static ssize_t mali_sysfs_show_dvfs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	if (!kbasep_pm_metrics_isactive(kbdev))
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "G3D DVFS is off\n");
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "G3D DVFS is on\n");

	return ret;
}

static ssize_t mali_sysfs_set_dvfs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	mali_error ret;
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	if (sysfs_streq("off", buf)) {
		if (kbasep_pm_metrics_isactive(kbdev)) {
			kbasep_pm_metrics_term(kbdev);
			kbase_platform_dvfs_set_level(kbdev,
				kbase_platform_dvfs_get_level(VITHAR_DEFAULT_CLOCK / 1000000));
			pr_info("G3D DVFS is disabled\n");
		}
	} else if (sysfs_streq("on", buf)) {
		if (!kbasep_pm_metrics_isactive(kbdev)) {
			ret = kbasep_pm_metrics_init(kbdev);
			if (ret != MALI_ERROR_NONE)
				pr_warning("kbase_pm_metrics_init failed,"
				    " error %u\n", ret);
			else
				pr_info("G3D DVFS is enabled\n");
		}
	} else {
		pr_info("%s: invalid, only [on, off] is accepted\n", buf);
	}

	return count;
}
DEVICE_ATTR(dvfs, S_IRUGO|S_IWUSR, mali_sysfs_show_dvfs, mali_sysfs_set_dvfs);
#endif /* CONFIG_T6XX_DVFS */

#ifdef MALI_DVFS_ASV_ENABLE
static ssize_t mali_sysfs_show_asv(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct kbase_device *kbdev;
	ssize_t ret = 0;
	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	ret = kbase_platform_dvfs_sprint_avs_table(buf);

	return ret;
}

static ssize_t mali_sysfs_set_asv(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (sysfs_streq("off", buf))
		kbase_platform_asv_set(0);
	else if (sysfs_streq("on", buf))
		kbase_platform_asv_set(1);
	else
		printk(KERN_ERR "invalid val -only [on, off] is accepted\n");

	return count;
}
DEVICE_ATTR(asv, S_IRUGO|S_IWUSR, mali_sysfs_show_asv, mali_sysfs_set_asv);
#endif

static int kbase_platform_create_sysfs_file(struct device *dev)
{
	if (device_create_file(dev, &dev_attr_clock))
	{
		dev_err(dev, "Couldn't create sysfs file [clock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_fbdev))
	{
		dev_err(dev, "Couldn't create sysfs file [fbdev]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_memory))
	{
		dev_err(dev, "Couldn't create sysfs file [memory]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dtlb))
	{
		dev_err(dev, "Couldn't create sysfs file [dtlb]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_vol))
	{
		dev_err(dev, "Couldn't create sysfs file [vol]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_clkout))
	{
		dev_err(dev, "Couldn't create sysfs file [clkout]\n");
		goto out;
	}
#ifdef CONFIG_T6XX_DVFS
	if (device_create_file(dev, &dev_attr_dvfs))
	{
		dev_err(dev, "Couldn't create sysfs file [dvfs]\n");
		goto out;
	}
#ifdef MALI_DVFS_ASV_ENABLE
	if (device_create_file(dev, &dev_attr_asv)) {
		dev_err(dev, "Couldn't create sysfs file [asv]\n");
		goto out;
	}
#endif
#endif /* CONFIG_T6XX_DVFS */
	if (!mali_setup_system_tracing(dev))
		goto out;


	return 0;
out:
	return -ENOENT;
}

void kbase_platform_remove_sysfs_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_clock);
	device_remove_file(dev, &dev_attr_fbdev);
	device_remove_file(dev, &dev_attr_dtlb);
	device_remove_file(dev, &dev_attr_vol);
	device_remove_file(dev, &dev_attr_clkout);
#ifdef CONFIG_T6XX_DVFS
	device_remove_file(dev, &dev_attr_dvfs);
#ifdef MALI_DVFS_ASV_ENABLE
	device_remove_file(dev, &dev_attr_asv);
#endif
#endif /* CONFIG_T6XX_DVFS */
	mali_cleanup_system_tracing(dev);
}
#endif /* CONFIG_T6XX_DEBUG_SYS */

#include "osk/include/mali_osk_lock_order.h"

#ifdef CONFIG_PM_RUNTIME
static void kbase_platform_runtime_term(struct kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->osdev.dev);
}
#endif /* CONFIG_PM_RUNTIME */

#ifdef CONFIG_PM_RUNTIME
extern void pm_runtime_init(struct device *dev);

static mali_error kbase_platform_runtime_init(struct kbase_device *kbdev)
{
	pm_runtime_init(kbdev->osdev.dev);
	pm_suspend_ignore_children(kbdev->osdev.dev, true);
	pm_runtime_enable(kbdev->osdev.dev);
	return MALI_ERROR_NONE;
}
#endif /* CONFIG_PM_RUNTIME */

mali_error kbase_platform_init(kbase_device *kbdev)
{
	struct exynos_context *platform;

	platform = osk_malloc(sizeof(struct exynos_context));

	if(NULL == platform)
	{
		return MALI_ERROR_OUT_OF_MEMORY;
	}

	kbdev->platform_context = (void *) platform;

	platform->cmu_pmu_status = 0;
	spin_lock_init(&platform->cmu_pmu_lock);

	if(kbase_platform_power_clock_init(kbdev))
	{
		goto clock_init_fail;
	}

#ifdef CONFIG_REGULATOR
	if(kbase_platform_regulator_init())
	{
		goto regulator_init_fail;
	}
#endif /* CONFIG_REGULATOR */

#ifdef CONFIG_T6XX_DVFS
	kbase_platform_dvfs_init(kbdev);
#endif /* CONFIG_T6XX_DVFS */

	/* Enable power */
	kbase_platform_cmu_pmu_control(kbdev, 1);
	return MALI_ERROR_NONE;

#ifdef CONFIG_REGULATOR
	kbase_platform_regulator_disable();
#endif /* CONFIG_REGULATOR */
regulator_init_fail:
clock_init_fail:
	osk_free(platform);

	return MALI_ERROR_FUNCTION_FAILED;
}

void kbase_platform_term(kbase_device *kbdev)
{
	struct exynos_context *platform;

	platform = (struct exynos_context *) kbdev->platform_context;

#ifdef CONFIG_T6XX_DVFS
	kbase_platform_dvfs_term();
#endif /* CONFIG_T6XX_DVFS */

	/* Disable power */
	kbase_platform_cmu_pmu_control(kbdev, 0);
#ifdef CONFIG_REGULATOR
	kbase_platform_regulator_disable();
#endif /* CONFIG_REGULATOR */
	osk_free(kbdev->platform_context);
	kbdev->platform_context = 0;
	return;
}

#ifdef CONFIG_REGULATOR
static struct regulator *g3d_regulator=NULL;
#ifdef CONFIG_T6XX_HWVER_R0P0
static int mali_gpu_vol = 1250000; /* 1.25V @ 533 MHz */
#else
static int mali_gpu_vol = 1050000; /* 1.05V @ 266 MHz */
#endif /*  CONFIG_T6XX_HWVER_R0P0 */
#endif /* CONFIG_REGULATOR */

#ifdef CONFIG_T6XX_DVFS
/*
 * Weighted moving average support for signed integer data
 * with 7-bits of precision (not currently used; all data
 * are integers).
 */
#define DVFS_AVG_DUMMY_MARKER	(~0)
#define DVFS_AVG_LPF_LEN	4	/* NB: best to be pow2 */
#define DVFS_AVG_EP_MULTIPLIER	(1<<7)	/* 7 fractional bits */

#define DVFS_AVG_RESET(x)	((x) = DVFS_AVG_DUMMY_MARKER)
#define _DVFS_AVG_IN(x)		((x) * DVFS_AVG_EP_MULTIPLIER)
#define _DVFS_LPF_UTIL(x, y, len) \
	((x != DVFS_AVG_DUMMY_MARKER) ? \
	 (((x) * ((len) - 1) + (y)) / (len)) : (y))
#define DVFS_AVG_LPF(x, y) do { \
	x = _DVFS_LPF_UTIL((x), _DVFS_AVG_IN((y)), DVFS_AVG_LPF_LEN); \
} while (0)
#define DVFS_TO_AVG(x)		DIV_ROUND_CLOSEST(x, DVFS_AVG_EP_MULTIPLIER)

#ifdef MALI_DVFS_ASV_ENABLE
enum asv_update_val {
	DVFS_NOT_UPDATE_ASV_TBL = 0,
	DVFS_UPDATE_ASV_TBL = 1,
	DVFS_UPDATE_ASV_DEFAULT_TBL = 2,
};
#endif /* MALI_DVFS_ASV_ENABLE */

struct mali_dvfs_status {
	kbase_device *kbdev;
	int step;
	int utilisation;
	uint nsamples;
	u32 avg_utilisation;
#ifdef MALI_DVFS_ASV_ENABLE
	enum asv_update_val asv_need_update;
	int asv_group;
#endif
};

static struct workqueue_struct *mali_dvfs_wq = 0;
int mali_dvfs_control=0;
osk_spinlock_irq mali_dvfs_spinlock;


static struct mali_dvfs_status mali_dvfs_status_current;
/*
 * Governor parameters.  The governor gets periodic samples of the
 * GPU utilisation (%busy) and maintains a weighted average over the
 * last DVFS_AVG_LPF_LEN values.  When the average is in the range
 * [min_threshold..max_threshold] we maintain the current clock+voltage.
 * If the utilisation drops below min for down_cnt_threshold samples
 * we step down.  If the utilisation exceeds max_threshold for
 * up_cnt_threshold samples we step up.
 *
 * The up/down thresholds are chosen to enable fast step up under
 * load with a longer step down; this optimizes for performance over
 * power consumption.  266MHz is the "sweet spot"; it has the best
 * performance/power ratio.  For this reason it has slightly extended
 * up/down thresholds to make it "sticky".
 */
struct mali_dvfs_info {
	unsigned int voltage;
	unsigned int clock;
	int min_threshold;
	int max_threshold;
	int up_cnt_threshold;
	int down_cnt_threshold;
};

/* TODO(sleffler) round or verify time is a multiple of frequency */
/* convert a time in milliseconds to a dvfs sample count */
#define	DVFS_TIME_TO_CNT(t)	((t) / KBASE_PM_DVFS_FREQUENCY)

/* TODO(sleffler) should be const but for voltage */
static struct mali_dvfs_info mali_dvfs_infotbl[MALI_DVFS_STEP] = {
#if (MALI_DVFS_STEP == 7)
	{ 912500, 100,  0,  60, DVFS_TIME_TO_CNT(750), DVFS_TIME_TO_CNT(2000)},
	{ 925000, 160, 40,  75, DVFS_TIME_TO_CNT(750), DVFS_TIME_TO_CNT(2000)},
	{1025000, 266, 65,  85, DVFS_TIME_TO_CNT(1000), DVFS_TIME_TO_CNT(3000)},
	{1075000, 350, 65,  85, DVFS_TIME_TO_CNT(750), DVFS_TIME_TO_CNT(1500)},
	{1125000, 400, 65,  85, DVFS_TIME_TO_CNT(750), DVFS_TIME_TO_CNT(1500)},
	{1150000, 450, 65,  90, DVFS_TIME_TO_CNT(1000), DVFS_TIME_TO_CNT(1500)},
	{1250000, 533, 75, 100, DVFS_TIME_TO_CNT(750), DVFS_TIME_TO_CNT(1500)}
#else
#error no table
#endif
};

#ifdef MALI_DVFS_ASV_ENABLE
static const unsigned int mali_dvfs_asv_vol_tbl_special
	[MALI_DVFS_ASV_GROUP_SPECIAL_NUM][MALI_DVFS_STEP] = {
	/*  100Mh   160Mh     266Mh   350Mh             400Mh   450Mh   533Mh*/
	{/*Group 1*/
		912500, 925000, 1025000, 1075000, 1100000, 1150000, 1225000,
	},
	{/*Group 2*/
		900000, 900000, 1000000, 1037500, 1087500, 1125000, 1200000,
	},
	{/*Group 3*/
	       912500, 925000, 1025000, 1037500, 1100000, 1150000, 1225000,
	},
	{/*Group 4*/
		900000, 900000, 1000000, 1025000, 1087500, 1125000, 1200000,
	},
	{/*Group 5*/
		912500, 925000, 1000000, 1000000, 1125000, 1150000, 1250000,
	},
	{/*Group 6*/
		900000, 912500, 987500, 987500, 1112500, 1150000, 1237500,
	},
	{/*Group 7*/
		900000, 900000, 975000, 987500, 1100000, 1137500, 1225000,
	},
	{/*Group 8*/
		900000, 900000, 975000, 987500, 1100000, 1137500, 1225000,
	},
	{/*Group 9*/
		887500, 900000, 962500, 975000, 1087500, 1125000, 1212500,
	},
	{/*Group 10*/
		887500, 900000, 962500, 962500, 1087500, 1125000, 1212500,
	},
};

static const unsigned int mali_dvfs_asv_vol_tbl
	[MALI_DVFS_ASV_GROUP_NUM][MALI_DVFS_STEP] = {
       /*  100Mh       160Mh      266Mh        350Mh,  400Mh   450Mh   533Mh*/
	{/*Group 0*/
		925000, 925000, 1025000, 1075000, 1125000, 1150000, 1200000,
	},
	{/*Group 1*/
		900000, 900000, 1000000, 1037500, 1087500, 1137500, 1187500,
	},
	{/*Group 2*/
		900000, 900000, 950000, 1037500, 1075000, 1125000, 1187500,
	},
	{/*Group 3*/
		900000, 900000, 950000, 1037500, 1075000, 1125000, 1187500,
	},
	{/*Group 4*/
		900000, 900000, 937500, 1025000, 1075000, 1112500, 1175000,
	},
	{/*Group 5*/
		900000, 900000, 937500, 1000000, 1050000, 1100000, 1150000,
	},
	{/*Group 6*/
		900000, 900000, 925000, 987500, 1037500, 1087500, 1137500,
	},
	{/*Group 7*/
		900000, 900000, 912500, 987500, 1025000, 1075000, 1125000,
	},
	{/*Group 8*/
		900000, 900000, 912500, 987500, 1012500, 1075000, 1125000,
	},
	{/*Group 9*/
		900000, 900000, 900000, 975000, 1012500, 1050000, 1125000,
	},
	{/*Group 10*/
		875000, 900000, 900000, 962500, 1000000, 1050000, 1112500,
	},
	{/*Group 11*/
		875000, 900000, 900000, 962500, 1000000, 1050000, 1112500,
	},
};

static const unsigned int mali_dvfs_vol_default[MALI_DVFS_STEP] = {
	 925000, 925000, 1025000, 1075000, 1125000, 1150000, 120000
};

static int mali_dvfs_update_asv(int group)
{
	int i;

	/* TODO(Shariq): Replace exynos_lot_id with api call once ASV code is rebased */
	if (exynos_lot_id && group == 0) {
		/* wrong group: Use default table to keep the system running smoothly */
		for (i = 0; i < MALI_DVFS_STEP; i++)
			mali_dvfs_infotbl[i].voltage = mali_dvfs_vol_default[i];
		pr_err("exynos_lot_id has group 0. Using default gpu asv table\n");
		return 1;
	}

	if (group == -1) {
		for (i = 0; i < MALI_DVFS_STEP; i++)
			mali_dvfs_infotbl[i].voltage = mali_dvfs_vol_default[i];
		pr_err("mali_dvfs_update_asv use default table\n");
		return 1;
	}
	if (group > MALI_DVFS_ASV_GROUP_NUM) {
		/* unknown group: Use default table to keep the system running smoothly */
		for (i = 0; i < MALI_DVFS_STEP; i++)
			mali_dvfs_infotbl[i].voltage = mali_dvfs_vol_default[i];
		OSK_PRINT_ERROR(OSK_BASE_PM, "invalid asv group (%d)\n", group);
		return 1;
	}
	for (i = 0; i < MALI_DVFS_STEP; i++) {
		if (exynos_lot_id)
			mali_dvfs_infotbl[i].voltage =
				mali_dvfs_asv_vol_tbl_special[group-1][i];
		 else
			 mali_dvfs_infotbl[i].voltage =
				mali_dvfs_asv_vol_tbl[group][i];
	}

	return 0;
}
#endif /* MALI_DVFS_ASV_ENABLE */

static void mali_dvfs_event_proc(struct work_struct *w)
{
	struct mali_dvfs_status dvfs_status;
	const struct mali_dvfs_info *info;
	int avg_utilisation;

	osk_spinlock_irq_lock(&mali_dvfs_spinlock);
	dvfs_status = mali_dvfs_status_current;
#ifdef MALI_DVFS_ASV_ENABLE
	if (dvfs_status.asv_need_update == DVFS_UPDATE_ASV_DEFAULT_TBL) {
		mali_dvfs_update_asv(-1);
		dvfs_status.asv_need_update = DVFS_NOT_UPDATE_ASV_TBL;
	} else if (dvfs_status.asv_need_update == DVFS_UPDATE_ASV_TBL) {
		if (mali_dvfs_update_asv(exynos_result_of_asv&0xf) == 0)
			dvfs_status.asv_group = (exynos_result_of_asv&0xf);
		dvfs_status.asv_need_update = DVFS_NOT_UPDATE_ASV_TBL;
	}
#endif
	osk_spinlock_irq_unlock(&mali_dvfs_spinlock);

	info = &mali_dvfs_infotbl[dvfs_status.step];

	OSK_ASSERT(dvfs_status.utilisation <= 100);
	dvfs_status.nsamples++;
	DVFS_AVG_LPF(dvfs_status.avg_utilisation, dvfs_status.utilisation);
	OSK_ASSERT(dvfs_status.nsamples > 0);
	avg_utilisation = DVFS_TO_AVG(dvfs_status.avg_utilisation);
#if MALI_DVFS_DEBUG
	pr_debug("%s: step %d utilisation %d avg %d max %d min %d "
	    "nsamples %u up_cnt %d down_cnt %d\n", __func__,
	    dvfs_status.step, dvfs_status.utilisation,
	    avg_utilisation, info->max_threshold, info->min_threshold,
	    dvfs_status.nsamples, allow_up_cnt, allow_down_cnt);
#endif
	trace_mali_dvfs_event(dvfs_status.utilisation, avg_utilisation);

	if (avg_utilisation > info->max_threshold &&
	    dvfs_status.nsamples >= info->up_cnt_threshold) {
		dvfs_status.step++;
		/*
		 * NB: max clock should have max_threshold of 100
		 * so this should never trip.
		 */
		OSK_ASSERT(dvfs_status.step < MALI_DVFS_STEP);
		DVFS_AVG_RESET(dvfs_status.avg_utilisation);
		dvfs_status.nsamples = 0;
	} else if (dvfs_status.step > 0 &&
	    avg_utilisation < info->min_threshold &&
	    dvfs_status.nsamples >= info->down_cnt_threshold) {
		OSK_ASSERT(dvfs_status.step > 0);
		dvfs_status.step--;
		DVFS_AVG_RESET(dvfs_status.avg_utilisation);
		dvfs_status.nsamples = 0;
	}

	if (kbasep_pm_metrics_isactive(dvfs_status.kbdev))
		kbase_platform_dvfs_set_level(dvfs_status.kbdev,
		dvfs_status.step);

	osk_spinlock_irq_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current = dvfs_status;
	osk_spinlock_irq_unlock(&mali_dvfs_spinlock);

}

static DECLARE_WORK(mali_dvfs_work, mali_dvfs_event_proc);

/**
 * Exynos5 alternative dvfs_callback imlpementation.
 * instead of:
 *    action = kbase_pm_get_dvfs_action(kbdev);
 * use this:
 *    kbase_platform_dvfs_event(kbdev);
 */

int kbase_platform_dvfs_get_control_status(void)
{
	return mali_dvfs_control;
}

int kbase_platform_dvfs_init(kbase_device *kbdev)
{
	/*default status
	add here with the right function to get initilization value.
	*/
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

	osk_spinlock_irq_init(&mali_dvfs_spinlock, OSK_LOCK_ORDER_PM_METRICS);

	/*add a error handling here*/
	osk_spinlock_irq_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.kbdev = kbdev;
	mali_dvfs_status_current.step = MALI_DVFS_STEP-1;
	mali_dvfs_status_current.utilisation = 100;
	DVFS_AVG_RESET(mali_dvfs_status_current.avg_utilisation);
#ifdef MALI_DVFS_ASV_ENABLE
	mali_dvfs_status_current.asv_need_update = DVFS_UPDATE_ASV_TBL;
	mali_dvfs_status_current.asv_group = -1;
#endif
	mali_dvfs_control = 1;
	osk_spinlock_irq_unlock(&mali_dvfs_spinlock);

	return MALI_TRUE;
}

void kbase_platform_dvfs_term(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}
#endif /* CONFIG_T6XX_DVFS */


int kbase_platform_dvfs_event(struct kbase_device *kbdev)
{
#ifdef CONFIG_T6XX_DVFS
        int utilisation = kbase_pm_get_dvfs_utilisation(kbdev);

	osk_spinlock_irq_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.utilisation = utilisation;
	osk_spinlock_irq_unlock(&mali_dvfs_spinlock);
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	/*add error handle here*/
	return MALI_TRUE;
#else
	return MALI_FALSE;
#endif
}

int kbase_platform_regulator_init(void)
{
#ifdef CONFIG_REGULATOR
	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if(IS_ERR(g3d_regulator))
	{
		printk("[kbase_platform_regulator_init] failed to get mali t6xx regulator\n");
		return -1;
	}

	if(regulator_enable(g3d_regulator) != 0)
	{
		printk("[kbase_platform_regulator_init] failed to enable mali t6xx regulator\n");
		return -1;
	}

	if(regulator_set_voltage(g3d_regulator, mali_gpu_vol, mali_gpu_vol) != 0)
	{
		printk("[kbase_platform_regulator_init] failed to set mali t6xx operating voltage [%d]\n", mali_gpu_vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	return 0;
}

int kbase_platform_regulator_disable(void)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_regulator_disable] g3d_regulator is not initialized\n");
		return -1;
	}

	if(regulator_disable(g3d_regulator) != 0)
	{
		printk("[kbase_platform_regulator_disable] failed to disable g3d regulator\n");
		return -1;
	}
#endif /* CONFIG_REGULATOR */
	return 0;
}

int kbase_platform_regulator_enable(void)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_regulator_enable] g3d_regulator is not initialized\n");
		return -1;
	}

	if(regulator_enable(g3d_regulator) != 0)
	{
		printk("[kbase_platform_regulator_enable] failed to enable g3d regulator\n");
		return -1;
	}
#endif /* CONFIG_REGULATOR */
	return 0;
}

int kbase_platform_get_default_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
	*vol = mali_gpu_vol;
#else /* CONFIG_REGULATOR */
	*vol = 0;
#endif /* CONFIG_REGULATOR */
	return 0;
}

#ifdef CONFIG_T6XX_DEBUG_SYS
int kbase_platform_get_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_get_voltage] g3d_regulator is not initialized\n");
		return -1;
	}

	*vol = regulator_get_voltage(g3d_regulator);
#else /* CONFIG_REGULATOR */
	*vol = 0;
#endif /* CONFIG_REGULATOR */
	return 0;
}
#endif /* CONFIG_T6XX_DEBUG_SYS */

#if defined CONFIG_T6XX_DEBUG_SYS || defined CONFIG_T6XX_DVFS
int kbase_platform_set_voltage(struct device *dev, int vol)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_set_voltage] g3d_regulator is not initialized\n");
		return -1;
	}

	if(regulator_set_voltage(g3d_regulator, vol, vol) != 0)
	{
		printk("[kbase_platform_set_voltage] failed to set voltage\n");
		return -1;
	}
#endif /* CONFIG_REGULATOR */
	return 0;
}
#endif /* CONFIG_T6XX_DEBUG_SYS */

void kbase_platform_dvfs_set_clock(kbase_device *kbdev, int freq)
{
	static struct clk * mout_gpll = NULL;
	static struct clk * fin_gpll = NULL;
	static struct clk * fout_gpll = NULL;
	static int _freq = -1;
	static unsigned long gpll_rate_prev = 0;
	unsigned long gpll_rate = 0, aclk_400_rate = 0;
	unsigned long tmp = 0;
	struct exynos_context *platform;


	if (!kbdev)
		panic("no kbdev");

	platform = (struct exynos_context *) kbdev->platform_context;
	if (NULL == platform)
		panic("no platform");

	if (platform->sclk_g3d == 0)
		return;

	if (mout_gpll == NULL) {
		mout_gpll = clk_get(kbdev->osdev.dev, "mout_gpll");
		fin_gpll = clk_get(kbdev->osdev.dev, "ext_xtal");
		fout_gpll = clk_get(kbdev->osdev.dev, "fout_gpll");
		if (IS_ERR(mout_gpll) || IS_ERR(fin_gpll) || IS_ERR(fout_gpll))
			panic("clk_get ERROR");
	}

	if (freq == _freq)
		return;

	trace_mali_dvfs_set_clock(freq);

	switch(freq)
	{
		case 533:
			gpll_rate = 533000000;
			aclk_400_rate = 533000000;
			break;
		case 450:
			gpll_rate = 450000000;
			aclk_400_rate = 450000000;
			break;
		case 400:
			gpll_rate = 800000000;
			aclk_400_rate = 400000000;
			break;
		case 350:
			gpll_rate = 1400000000;
			aclk_400_rate = 350000000;
			break;
		case 266:
			gpll_rate = 800000000;
			aclk_400_rate = 267000000;
			break;
		case 160:
			gpll_rate = 800000000;
			aclk_400_rate = 160000000;
			break;
		case 100:
			gpll_rate = 800000000;
			aclk_400_rate = 100000000;
			break;
		default:
			return;
	}

	/* if changed the GPLL rate, set rate for GPLL and wait for lock time */
	if( gpll_rate != gpll_rate_prev) {
		/*for stable clock input.*/
		clk_set_rate(platform->sclk_g3d, 100000000);
		clk_set_parent(mout_gpll, fin_gpll);

		/*change gpll*/
		clk_set_rate( fout_gpll, gpll_rate );

		/*restore parent*/
		clk_set_parent(mout_gpll, fout_gpll);
		gpll_rate_prev = gpll_rate;
	}

	_freq = freq;
	clk_set_rate(platform->sclk_g3d, aclk_400_rate);

	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(/*EXYNOS5_CLKDIV_STAT_TOP0*/EXYNOS_CLKREG(0x10610));
	} while (tmp & 0x1000000);
#ifdef CONFIG_T6XX_DVFS
#if MALI_DVFS_DEBUG
	printk("aclk400 %u[%d]\n", (unsigned int)clk_get_rate(platform->sclk_g3d),mali_dvfs_status_current.utilisation);
	printk("dvfs_set_clock GPLL : %lu, ACLK_400 : %luMhz\n", gpll_rate, aclk_400_rate );
#endif /* MALI_DVFS_DEBUG */
#endif /* CONFIG_T6XX_DVFS */
	return;
}

static void kbase_platform_dvfs_set_vol(unsigned int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return;
	trace_mali_dvfs_set_voltage(vol);

	kbase_platform_set_voltage(NULL, vol);

	_vol = vol;
#if MALI_DVFS_DEBUG
	printk(KERN_DEBUG "dvfs_set_vol %dmV\n", vol);
#endif
	return;
}

int kbase_platform_dvfs_get_level(int freq)
{
	int i;
	for (i = 0; i < MALI_DVFS_STEP; i++) {
		if (mali_dvfs_infotbl[i].clock == freq)
			return i;
	}
	return -1;
}

#if defined CONFIG_T6XX_DVFS || defined CONFIG_T6XX_DEBUG_SYS
void kbase_platform_dvfs_set_level(kbase_device *kbdev, int level)
{
	static int level_prev = -1;

	if (level == level_prev)
		return;
	if (WARN_ON((level >= MALI_DVFS_STEP) || (level < 0)))
		panic("invalid level");

	if (level > level_prev) {
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[level].voltage);
		kbase_platform_dvfs_set_clock(kbdev,
			mali_dvfs_infotbl[level].clock);
	} else{
		kbase_platform_dvfs_set_clock(kbdev,
			mali_dvfs_infotbl[level].clock);
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[level].voltage);
	}
	level_prev = level;
}
#endif

#ifdef MALI_DVFS_ASV_ENABLE
static int kbase_platform_dvfs_sprint_avs_table(char *buf)
{
	int i, cnt = 0;
	if (buf == NULL)
		return 0;

	cnt += sprintf(buf, "asv group:%d exynos_lot_id:%d\n",
		exynos_result_of_asv&0xf, exynos_lot_id);
	for (i = MALI_DVFS_STEP-1; i >= 0; i--) {
		cnt += sprintf(buf+cnt, "%dMhz:%d\n",
			mali_dvfs_infotbl[i].clock, mali_dvfs_infotbl[i].voltage);
	}
	return cnt;
}

static int kbase_platform_asv_set(int enable)
{
	osk_spinlock_irq_lock(&mali_dvfs_spinlock);
	if (enable) {
		mali_dvfs_status_current.asv_need_update = DVFS_UPDATE_ASV_TBL;
		mali_dvfs_status_current.asv_group = -1;
	} else{
		mali_dvfs_status_current.asv_need_update = DVFS_UPDATE_ASV_DEFAULT_TBL;
	}
	osk_spinlock_irq_unlock(&mali_dvfs_spinlock);
	return 0;
}
#endif /* MALI_DVFS_ASV_ENABLE */

/**
 * Exynos5 alternative dvfs_callback imlpementation.
 * instead of:
 *    action = kbase_pm_get_dvfs_action(kbdev);
 * use this:
 *    kbase_platform_dvfs_event(kbdev);
 */

#ifdef CONFIG_T6XX_DVFS
int kbase_pm_get_dvfs_utilisation(kbase_device *kbdev)
{
	unsigned long flags;
	int utilisation=0;
	ktime_t now = ktime_get();
	ktime_t diff;

	OSK_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);

	if (kbdev->pm.metrics.gpu_active)
	{
		diff = ktime_sub(now, kbdev->pm.metrics.time_period_start);
		kbdev->pm.metrics.time_busy += (u32)(ktime_to_ns( diff ) >> KBASE_PM_TIME_SHIFT);
		kbdev->pm.metrics.time_period_start = now;
	}
	else
	{
		diff = ktime_sub(now, kbdev->pm.metrics.time_period_start);
		kbdev->pm.metrics.time_idle += (u32)(ktime_to_ns( diff ) >> KBASE_PM_TIME_SHIFT);
		kbdev->pm.metrics.time_period_start = now;
	}

	if (kbdev->pm.metrics.time_idle + kbdev->pm.metrics.time_busy == 0)
	{
		/* No data - so we return NOP */
		goto out;
	}

	utilisation = (100*kbdev->pm.metrics.time_busy) / (kbdev->pm.metrics.time_idle + kbdev->pm.metrics.time_busy);

out:

	kbdev->pm.metrics.time_idle = 0;
	kbdev->pm.metrics.time_busy = 0;

	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);

	return utilisation;
}
#endif

#ifdef CONFIG_MALI_HWC_TRACE
/*
 * Mali hardware performance counter trace support.  Each counter
 * has a corresponding trace event.  To use, enable events and write
 * 1 to the hwc_enable sysfs file to start polling for counter data
 * at vblank.  An event is dispatched each time a counter's value
 * changes.  Note system event tracing must be enabled to get the
 * events; otherwise the polling will not generate events.  When done
 * turn off polling.  Note also that the GPU_ACTIVE counter causes
 * the GPU to never be idle; this causes the governor to push the
 * clock to max (since it sees 100% utilization).
 *
 * This code is derived from ARM's gator driver.
 */
#include <drm/drm_notifier.h>
#include <kbase/src/linux/mali_linux_hwc_trace.h>

/*
 * Mali hw counters by block.  Note that each hw counter enable bit
 * covers 4 counters.  There are 64 counters / block for 256 total
 * counters.
 */
#define	MALI_HWC_TOTAL	(4*64)			/* total # hw counters */

#define	hwc_off(x)	((x) & 0x3f)		/* offset in block */
#define	hwc_bit(x)	(hwc_off(x) >> 2)	/* block bm enable bit */

#define	BLOCK_OFF(b)	((b) << 6)

/* Job Manager (Block 0) */
#define	BLOCK_JM_OFF	BLOCK_OFF(0)
#define MALI_HW_MESSAGES_SENT		(BLOCK_JM_OFF + 4)
#define MALI_HW_MESSAGES_RECEIVED	(BLOCK_JM_OFF + 5)
/* NB: if GPU_ACTIVE is enabled the GPU will stay active when idle */
#define MALI_HW_GPU_ACTIVE		(BLOCK_JM_OFF + 6)
#define MALI_HW_IRQ_ACTIVE		(BLOCK_JM_OFF + 7)

#define MALI_HW_JS0_JOBS		(BLOCK_JM_OFF + 8*0 + 8)
#define MALI_HW_JS0_TASKS		(BLOCK_JM_OFF + 8*0 + 9)
#define MALI_HW_JS0_ACTIVE		(BLOCK_JM_OFF + 8*0 + 10)
#define MALI_HW_JS0_WAIT_READ		(BLOCK_JM_OFF + 8*0 + 11)
#define MALI_HW_JS0_WAIT_ISSUE		(BLOCK_JM_OFF + 8*0 + 12)
#define MALI_HW_JS0_WAIT_DEPEND		(BLOCK_JM_OFF + 8*0 + 13)
#define MALI_HW_JS0_WAIT_FINISH		(BLOCK_JM_OFF + 8*0 + 14)

#define MALI_HW_JS1_JOBS		(BLOCK_JM_OFF + 8*1 + 8)
#define MALI_HW_JS1_TASKS		(BLOCK_JM_OFF + 8*1 + 9)
#define MALI_HW_JS1_ACTIVE		(BLOCK_JM_OFF + 8*1 + 10)
#define MALI_HW_JS1_WAIT_READ		(BLOCK_JM_OFF + 8*1 + 11)
#define MALI_HW_JS1_WAIT_ISSUE		(BLOCK_JM_OFF + 8*1 + 12)
#define MALI_HW_JS1_WAIT_DEPEND		(BLOCK_JM_OFF + 8*1 + 13)
#define MALI_HW_JS1_WAIT_FINISH		(BLOCK_JM_OFF + 8*1 + 14)

#define MALI_HW_JS2_JOBS		(BLOCK_JM_OFF + 8*2 + 8)
#define MALI_HW_JS2_TASKS		(BLOCK_JM_OFF + 8*2 + 9)
#define MALI_HW_JS2_ACTIVE		(BLOCK_JM_OFF + 8*2 + 10)
#define MALI_HW_JS2_WAIT_READ		(BLOCK_JM_OFF + 8*2 + 11)
#define MALI_HW_JS2_WAIT_ISSUE		(BLOCK_JM_OFF + 8*2 + 12)
#define MALI_HW_JS2_WAIT_DEPEND		(BLOCK_JM_OFF + 8*2 + 13)
#define MALI_HW_JS2_WAIT_FINISH		(BLOCK_JM_OFF + 8*2 + 14)

#define MALI_HW_JS3_JOBS		(BLOCK_JM_OFF + 8*3 + 8)
#define MALI_HW_JS3_TASKS		(BLOCK_JM_OFF + 8*3 + 9)
#define MALI_HW_JS3_ACTIVE		(BLOCK_JM_OFF + 8*3 + 10)
#define MALI_HW_JS3_WAIT_READ		(BLOCK_JM_OFF + 8*3 + 11)
#define MALI_HW_JS3_WAIT_ISSUE		(BLOCK_JM_OFF + 8*3 + 12)
#define MALI_HW_JS3_WAIT_DEPEND		(BLOCK_JM_OFF + 8*3 + 13)
#define MALI_HW_JS3_WAIT_FINISH		(BLOCK_JM_OFF + 8*3 + 14)

#define MALI_HW_JS4_JOBS		(BLOCK_JM_OFF + 8*4 + 8)
#define MALI_HW_JS4_TASKS		(BLOCK_JM_OFF + 8*4 + 9)
#define MALI_HW_JS4_ACTIVE		(BLOCK_JM_OFF + 8*4 + 10)
#define MALI_HW_JS4_WAIT_READ		(BLOCK_JM_OFF + 8*4 + 11)
#define MALI_HW_JS4_WAIT_ISSUE		(BLOCK_JM_OFF + 8*4 + 12)
#define MALI_HW_JS4_WAIT_DEPEND		(BLOCK_JM_OFF + 8*4 + 13)
#define MALI_HW_JS4_WAIT_FINISH		(BLOCK_JM_OFF + 8*4 + 14)

#define MALI_HW_JS5_JOBS		(BLOCK_JM_OFF + 8*5 + 8)
#define MALI_HW_JS5_TASKS		(BLOCK_JM_OFF + 8*5 + 9)
#define MALI_HW_JS5_ACTIVE		(BLOCK_JM_OFF + 8*5 + 10)
#define MALI_HW_JS5_WAIT_READ		(BLOCK_JM_OFF + 8*5 + 11)
#define MALI_HW_JS5_WAIT_ISSUE		(BLOCK_JM_OFF + 8*5 + 12)
#define MALI_HW_JS5_WAIT_DEPEND		(BLOCK_JM_OFF + 8*5 + 13)
#define MALI_HW_JS5_WAIT_FINISH		(BLOCK_JM_OFF + 8*5 + 14)

#define MALI_HW_JS6_JOBS		(BLOCK_JM_OFF + 8*6 + 8)
#define MALI_HW_JS6_TASKS		(BLOCK_JM_OFF + 8*6 + 9)
#define MALI_HW_JS6_ACTIVE		(BLOCK_JM_OFF + 8*6 + 10)
#define MALI_HW_JS6_WAIT_READ		(BLOCK_JM_OFF + 8*6 + 11)
#define MALI_HW_JS6_WAIT_ISSUE		(BLOCK_JM_OFF + 8*6 + 12)
#define MALI_HW_JS6_WAIT_DEPEND		(BLOCK_JM_OFF + 8*6 + 13)
#define MALI_HW_JS6_WAIT_FINISH		(BLOCK_JM_OFF + 8*6 + 14)

#define	MALI_HW_JM_FIRST	MALI_HW_GPU_ACTIVE
#define	MALI_HW_JM_LAST		MALI_HW_JS6_WAIT_FINISH

/* Tiler (Block 1) */
#define	BLOCK_TILER_OFF	BLOCK_OFF(1)
#define MALI_HW_JOBS_PROCESSED		(BLOCK_TILER_OFF + 3)
#define MALI_HW_TRIANGLES		(BLOCK_TILER_OFF + 4)
#define MALI_HW_QUADS			(BLOCK_TILER_OFF + 5)
#define MALI_HW_POLYGONS		(BLOCK_TILER_OFF + 6)
#define MALI_HW_POINTS			(BLOCK_TILER_OFF + 7)
#define MALI_HW_LINES			(BLOCK_TILER_OFF + 8)
#define MALI_HW_VCACHE_HIT		(BLOCK_TILER_OFF + 9)
#define MALI_HW_VCACHE_MISS		(BLOCK_TILER_OFF + 10)
#define MALI_HW_FRONT_FACING		(BLOCK_TILER_OFF + 11)
#define MALI_HW_BACK_FACING		(BLOCK_TILER_OFF + 12)
#define MALI_HW_PRIM_VISIBLE		(BLOCK_TILER_OFF + 13)
#define MALI_HW_PRIM_CULLED		(BLOCK_TILER_OFF + 14)
#define MALI_HW_PRIM_CLIPPED		(BLOCK_TILER_OFF + 15)

#define MALI_HW_COMPRESS_IN		(BLOCK_TILER_OFF + 32)
#define MALI_HW_COMPRESS_OUT		(BLOCK_TILER_OFF + 33)
#define MALI_HW_COMPRESS_FLUSH		(BLOCK_TILER_OFF + 34)
#define MALI_HW_TIMESTAMPS		(BLOCK_TILER_OFF + 35)
#define MALI_HW_PCACHE_HIT		(BLOCK_TILER_OFF + 36)
#define MALI_HW_PCACHE_MISS		(BLOCK_TILER_OFF + 37)
#define MALI_HW_PCACHE_LINE		(BLOCK_TILER_OFF + 38)
#define MALI_HW_PCACHE_STALL		(BLOCK_TILER_OFF + 39)
#define MALI_HW_WRBUF_HIT		(BLOCK_TILER_OFF + 40)
#define MALI_HW_WRBUF_MISS		(BLOCK_TILER_OFF + 41)
#define MALI_HW_WRBUF_LINE		(BLOCK_TILER_OFF + 42)
#define MALI_HW_WRBUF_PARTIAL		(BLOCK_TILER_OFF + 43)
#define MALI_HW_WRBUF_STALL		(BLOCK_TILER_OFF + 44)
#define MALI_HW_ACTIVE			(BLOCK_TILER_OFF + 45)
#define MALI_HW_LOADING_DESC		(BLOCK_TILER_OFF + 46)
#define MALI_HW_INDEX_WAIT		(BLOCK_TILER_OFF + 47)
#define MALI_HW_INDEX_RANGE_WAIT	(BLOCK_TILER_OFF + 48)
#define MALI_HW_VERTEX_WAIT		(BLOCK_TILER_OFF + 49)
#define MALI_HW_PCACHE_WAIT		(BLOCK_TILER_OFF + 50)
#define MALI_HW_WRBUF_WAIT		(BLOCK_TILER_OFF + 51)
#define MALI_HW_BUS_READ		(BLOCK_TILER_OFF + 52)
#define MALI_HW_BUS_WRITE		(BLOCK_TILER_OFF + 53)

#define MALI_HW_TILER_UTLB_STALL	(BLOCK_TILER_OFF + 59)
#define MALI_HW_TILER_UTLB_REPLAY_MISS	(BLOCK_TILER_OFF + 60)
#define MALI_HW_TILER_UTLB_REPLAY_FULL	(BLOCK_TILER_OFF + 61)
#define MALI_HW_TILER_UTLB_NEW_MISS	(BLOCK_TILER_OFF + 62)
#define MALI_HW_TILER_UTLB_HIT		(BLOCK_TILER_OFF + 63)

#define	MALI_HW_TILER_FIRST	MALI_HW_JOBS_PROCESSED
#define	MALI_HW_TILER_LAST	MALI_HW_TILER_UTLB_HIT

/* Shader Core (Block 2) */
#define	BLOCK_SHADER_OFF	BLOCK_OFF(2)
#define MALI_HW_SHADER_CORE_ACTIVE	(BLOCK_SHADER_OFF + 3)
#define MALI_HW_FRAG_ACTIVE		(BLOCK_SHADER_OFF + 4)
#define MALI_HW_FRAG_PRIMATIVES		(BLOCK_SHADER_OFF + 5)
#define MALI_HW_FRAG_PRIMATIVES_DROPPED	(BLOCK_SHADER_OFF + 6)
#define MALI_HW_FRAG_CYCLE_DESC		(BLOCK_SHADER_OFF + 7)
#define MALI_HW_FRAG_CYCLES_PLR		(BLOCK_SHADER_OFF + 8)
#define MALI_HW_FRAG_CYCLES_VERT	(BLOCK_SHADER_OFF + 9)
#define MALI_HW_FRAG_CYCLES_TRISETUP	(BLOCK_SHADER_OFF + 10)
#define MALI_HW_FRAG_CYCLES_RAST	(BLOCK_SHADER_OFF + 11)
#define MALI_HW_FRAG_THREADS		(BLOCK_SHADER_OFF + 12)
#define MALI_HW_FRAG_DUMMY_THREADS	(BLOCK_SHADER_OFF + 13)
#define MALI_HW_FRAG_QUADS_RAST		(BLOCK_SHADER_OFF + 14)
#define MALI_HW_FRAG_QUADS_EZS_TEST	(BLOCK_SHADER_OFF + 15)
#define MALI_HW_FRAG_QUADS_EZS_KILLED	(BLOCK_SHADER_OFF + 16)
#define MALI_HW_FRAG_QUADS_LZS_TEST	(BLOCK_SHADER_OFF + 17)
#define MALI_HW_FRAG_QUADS_LZS_KILLED	(BLOCK_SHADER_OFF + 18)
#define MALI_HW_FRAG_CYCLE_NO_TILE	(BLOCK_SHADER_OFF + 19)
#define MALI_HW_FRAG_NUM_TILES		(BLOCK_SHADER_OFF + 20)
#define MALI_HW_FRAG_TRANS_ELIM		(BLOCK_SHADER_OFF + 21)
#define MALI_HW_COMPUTE_ACTIVE		(BLOCK_SHADER_OFF + 22)
#define MALI_HW_COMPUTE_TASKS		(BLOCK_SHADER_OFF + 23)
#define MALI_HW_COMPUTE_THREADS		(BLOCK_SHADER_OFF + 24)
#define MALI_HW_COMPUTE_CYCLES_DESC	(BLOCK_SHADER_OFF + 25)
#define MALI_HW_TRIPIPE_ACTIVE		(BLOCK_SHADER_OFF + 26)
#define MALI_HW_ARITH_WORDS		(BLOCK_SHADER_OFF + 27)
#define MALI_HW_ARITH_CYCLES_REG	(BLOCK_SHADER_OFF + 28)
#define MALI_HW_ARITH_CYCLES_L0		(BLOCK_SHADER_OFF + 29)
#define MALI_HW_ARITH_FRAG_DEPEND	(BLOCK_SHADER_OFF + 30)
#define MALI_HW_LS_WORDS		(BLOCK_SHADER_OFF + 31)
#define MALI_HW_LS_ISSUES		(BLOCK_SHADER_OFF + 32)
#define MALI_HW_LS_RESTARTS		(BLOCK_SHADER_OFF + 33)
#define MALI_HW_LS_REISSUES_MISS	(BLOCK_SHADER_OFF + 34)
#define MALI_HW_LS_REISSUES_VD		(BLOCK_SHADER_OFF + 35)
#define MALI_HW_LS_REISSUE_ATTRIB_MISS	(BLOCK_SHADER_OFF + 36)
#define MALI_HW_LS_NO_WB		(BLOCK_SHADER_OFF + 37)
#define MALI_HW_TEX_WORDS		(BLOCK_SHADER_OFF + 38)
#define MALI_HW_TEX_BUBBLES		(BLOCK_SHADER_OFF + 39)
#define MALI_HW_TEX_WORDS_L0		(BLOCK_SHADER_OFF + 40)
#define MALI_HW_TEX_WORDS_DESC		(BLOCK_SHADER_OFF + 41)
#define MALI_HW_TEX_THREADS		(BLOCK_SHADER_OFF + 42)
#define MALI_HW_TEX_RECIRC_FMISS	(BLOCK_SHADER_OFF + 43)
#define MALI_HW_TEX_RECIRC_DESC		(BLOCK_SHADER_OFF + 44)
#define MALI_HW_TEX_RECIRC_MULTI	(BLOCK_SHADER_OFF + 45)
#define MALI_HW_TEX_RECIRC_PMISS	(BLOCK_SHADER_OFF + 46)
#define MALI_HW_TEX_RECIRC_CONF		(BLOCK_SHADER_OFF + 47)
#define MALI_HW_LSC_READ_HITS		(BLOCK_SHADER_OFF + 48)
#define MALI_HW_LSC_READ_MISSES		(BLOCK_SHADER_OFF + 49)
#define MALI_HW_LSC_WRITE_HITS		(BLOCK_SHADER_OFF + 50)
#define MALI_HW_LSC_WRITE_MISSES	(BLOCK_SHADER_OFF + 51)
#define MALI_HW_LSC_ATOMIC_HITS		(BLOCK_SHADER_OFF + 52)
#define MALI_HW_LSC_ATOMIC_MISSES	(BLOCK_SHADER_OFF + 53)
#define MALI_HW_LSC_LINE_FETCHES	(BLOCK_SHADER_OFF + 54)
#define MALI_HW_LSC_DIRTY_LINE		(BLOCK_SHADER_OFF + 55)
#define MALI_HW_LSC_SNOOPS		(BLOCK_SHADER_OFF + 56)
#define MALI_HW_AXI_TLB_STALL		(BLOCK_SHADER_OFF + 57)
#define MALI_HW_AXI_TLB_MISS		(BLOCK_SHADER_OFF + 58)
#define MALI_HW_AXI_TLB_TRANSACTION	(BLOCK_SHADER_OFF + 59)
#define MALI_HW_LS_TLB_MISS		(BLOCK_SHADER_OFF + 60)
#define MALI_HW_LS_TLB_HIT		(BLOCK_SHADER_OFF + 61)
#define MALI_HW_AXI_BEATS_READ		(BLOCK_SHADER_OFF + 62)
#define MALI_HW_AXI_BEATS_WRITE		(BLOCK_SHADER_OFF + 63)

#define	MALI_HW_SHADER_FIRST	MALI_HW_SHADER_CORE_ACTIVE
#define	MALI_HW_SHADER_LAST	MALI_HW_AXI_BEATS_WRITE

/* L2 and MMU (Block 3) */
#define	BLOCK_MMU_OFF	BLOCK_OFF(3)
#define MALI_HW_MMU_TABLE_WALK		(BLOCK_MMU_OFF + 4)
#define MALI_HW_MMU_REPLAY_MISS		(BLOCK_MMU_OFF + 5)
#define MALI_HW_MMU_REPLAY_FULL		(BLOCK_MMU_OFF + 6)
#define MALI_HW_MMU_NEW_MISS		(BLOCK_MMU_OFF + 7)
#define MALI_HW_MMU_HIT			(BLOCK_MMU_OFF + 8)

#define MALI_HW_UTLB_STALL		(BLOCK_MMU_OFF + 16)
#define MALI_HW_UTLB_REPLAY_MISS	(BLOCK_MMU_OFF + 17)
#define MALI_HW_UTLB_REPLAY_FULL	(BLOCK_MMU_OFF + 18)
#define MALI_HW_UTLB_NEW_MISS		(BLOCK_MMU_OFF + 19)
#define MALI_HW_UTLB_HIT		(BLOCK_MMU_OFF + 20)

#define MALI_HW_L2_WRITE_BEATS		(BLOCK_MMU_OFF + 30)
#define MALI_HW_L2_READ_BEATS		(BLOCK_MMU_OFF + 31)
#define MALI_HW_L2_ANY_LOOKUP		(BLOCK_MMU_OFF + 32)
#define MALI_HW_L2_READ_LOOKUP		(BLOCK_MMU_OFF + 33)
#define MALI_HW_L2_SREAD_LOOKUP		(BLOCK_MMU_OFF + 34)
#define MALI_HW_L2_READ_REPLAY		(BLOCK_MMU_OFF + 35)
#define MALI_HW_L2_READ_SNOOP		(BLOCK_MMU_OFF + 36)
#define MALI_HW_L2_READ_HIT		(BLOCK_MMU_OFF + 37)
#define MALI_HW_L2_CLEAN_MISS		(BLOCK_MMU_OFF + 38)
#define MALI_HW_L2_WRITE_LOOKUP		(BLOCK_MMU_OFF + 39)
#define MALI_HW_L2_SWRITE_LOOKUP	(BLOCK_MMU_OFF + 40)
#define MALI_HW_L2_WRITE_REPLAY		(BLOCK_MMU_OFF + 41)
#define MALI_HW_L2_WRITE_SNOOP		(BLOCK_MMU_OFF + 42)
#define MALI_HW_L2_WRITE_HIT		(BLOCK_MMU_OFF + 43)
#define MALI_HW_L2_EXT_READ_FULL	(BLOCK_MMU_OFF + 44)
#define MALI_HW_L2_EXT_READ_HALF	(BLOCK_MMU_OFF + 45)
#define MALI_HW_L2_EXT_WRITE_FULL	(BLOCK_MMU_OFF + 46)
#define MALI_HW_L2_EXT_WRITE_HALF	(BLOCK_MMU_OFF + 47)
#define MALI_HW_L2_EXT_READ		(BLOCK_MMU_OFF + 48)
#define MALI_HW_L2_EXT_READ_LINE	(BLOCK_MMU_OFF + 49)
#define MALI_HW_L2_EXT_WRITE		(BLOCK_MMU_OFF + 50)
#define MALI_HW_L2_EXT_WRITE_LINE	(BLOCK_MMU_OFF + 51)
#define MALI_HW_L2_EXT_WRITE_SMALL	(BLOCK_MMU_OFF + 52)
#define MALI_HW_L2_EXT_BARRIER		(BLOCK_MMU_OFF + 53)
#define MALI_HW_L2_EXT_AR_STALL		(BLOCK_MMU_OFF + 54)
#define MALI_HW_L2_EXT_R_BUF_FULL	(BLOCK_MMU_OFF + 55)
#define MALI_HW_L2_EXT_RD_BUF_FULL	(BLOCK_MMU_OFF + 56)
#define MALI_HW_L2_EXT_R_RAW		(BLOCK_MMU_OFF + 57)
#define MALI_HW_L2_EXT_W_STALL		(BLOCK_MMU_OFF + 58)
#define MALI_HW_L2_EXT_W_BUF_FULL	(BLOCK_MMU_OFF + 59)
#define MALI_HW_L2_EXT_R_W_HAZARD	(BLOCK_MMU_OFF + 60)
#define MALI_HW_L2_TAG_HAZARD		(BLOCK_MMU_OFF + 61)
#define MALI_HW_L2_SNOOP_FULL		(BLOCK_MMU_OFF + 62)
#define MALI_HW_L2_REPLAY_FULL		(BLOCK_MMU_OFF + 63)

#define	MALI_HW_MMU_FIRST	MALI_HW_MMU_TABLE_WALK
#define	MALI_HW_MMU_LAST	MALI_HW_L2_REPLAY_FULL

/*
 * The amount of memory required to store a hwc dump is:
 *    # "core groups" (1)
 *  x # blocks (always 8 for midgard arch)
 *  x # counters / block (always 64)
 *  x # bytes / counter (4 for 32-bit counters)
 */
#define	MALI_HWC_DUMP_SIZE	(1*8*64*4)

struct mali_hwcounter_state {
	struct workqueue_struct *wq;		/* collection context */
	struct kbase_context *ctx;		/* kbase device context */
	void *buf;				/* counter data buffer */
	kbase_uk_hwcnt_setup setup;		/* hwcounter setup block */
	bool active;				/* collecting data */
	u32 last_read[MALI_HWC_TOTAL];		/* last counter value read */
};
static struct mali_hwcounter_state mali_hwcs;
static osk_mutex mali_hwcounter_mutex;

/*
 * Support for mapping between hw counters and trace events.
 */
struct mali_hwcounter_trace_map {
	void (*do_trace)(unsigned int val);	/* NB: all the same prototype */
	struct static_key *key;
};
#define	HWC_EVENT_MAP(event)						\
	[MALI_HW_##event] = {						\
		.do_trace = &trace_mali_hwc_##event,			\
		.key = &__tracepoint_mali_hwc_##event.key		\
	}

static struct mali_hwcounter_trace_map mali_hwcounter_map[256] = {
	/* Job Manager */
	HWC_EVENT_MAP(MESSAGES_SENT),
	HWC_EVENT_MAP(MESSAGES_RECEIVED),

	HWC_EVENT_MAP(GPU_ACTIVE),
	HWC_EVENT_MAP(IRQ_ACTIVE),

	HWC_EVENT_MAP(JS0_JOBS),
	HWC_EVENT_MAP(JS0_TASKS),
	HWC_EVENT_MAP(JS0_ACTIVE),
	HWC_EVENT_MAP(JS0_WAIT_READ),
	HWC_EVENT_MAP(JS0_WAIT_ISSUE),
	HWC_EVENT_MAP(JS0_WAIT_DEPEND),
	HWC_EVENT_MAP(JS0_WAIT_FINISH),
	HWC_EVENT_MAP(JS1_JOBS),
	HWC_EVENT_MAP(JS1_TASKS),
	HWC_EVENT_MAP(JS1_ACTIVE),
	HWC_EVENT_MAP(JS1_WAIT_READ),
	HWC_EVENT_MAP(JS1_WAIT_ISSUE),
	HWC_EVENT_MAP(JS1_WAIT_DEPEND),
	HWC_EVENT_MAP(JS1_WAIT_FINISH),
	HWC_EVENT_MAP(JS2_JOBS),
	HWC_EVENT_MAP(JS2_TASKS),
	HWC_EVENT_MAP(JS2_ACTIVE),
	HWC_EVENT_MAP(JS2_WAIT_READ),
	HWC_EVENT_MAP(JS2_WAIT_ISSUE),
	HWC_EVENT_MAP(JS2_WAIT_DEPEND),
	HWC_EVENT_MAP(JS2_WAIT_FINISH),
	HWC_EVENT_MAP(JS3_JOBS),
	HWC_EVENT_MAP(JS3_TASKS),
	HWC_EVENT_MAP(JS3_ACTIVE),
	HWC_EVENT_MAP(JS3_WAIT_READ),
	HWC_EVENT_MAP(JS3_WAIT_ISSUE),
	HWC_EVENT_MAP(JS3_WAIT_DEPEND),
	HWC_EVENT_MAP(JS3_WAIT_FINISH),
	HWC_EVENT_MAP(JS4_JOBS),
	HWC_EVENT_MAP(JS4_TASKS),
	HWC_EVENT_MAP(JS4_ACTIVE),
	HWC_EVENT_MAP(JS4_WAIT_READ),
	HWC_EVENT_MAP(JS4_WAIT_ISSUE),
	HWC_EVENT_MAP(JS4_WAIT_DEPEND),
	HWC_EVENT_MAP(JS4_WAIT_FINISH),
	HWC_EVENT_MAP(JS5_JOBS),
	HWC_EVENT_MAP(JS5_TASKS),
	HWC_EVENT_MAP(JS5_ACTIVE),
	HWC_EVENT_MAP(JS5_WAIT_READ),
	HWC_EVENT_MAP(JS5_WAIT_ISSUE),
	HWC_EVENT_MAP(JS5_WAIT_DEPEND),
	HWC_EVENT_MAP(JS5_WAIT_FINISH),
	HWC_EVENT_MAP(JS6_JOBS),
	HWC_EVENT_MAP(JS6_TASKS),
	HWC_EVENT_MAP(JS6_ACTIVE),
	HWC_EVENT_MAP(JS6_WAIT_READ),
	HWC_EVENT_MAP(JS6_WAIT_ISSUE),
	HWC_EVENT_MAP(JS6_WAIT_DEPEND),
	HWC_EVENT_MAP(JS6_WAIT_FINISH),

	/* Tiler */
	HWC_EVENT_MAP(JOBS_PROCESSED),
	HWC_EVENT_MAP(TRIANGLES),
	HWC_EVENT_MAP(QUADS),
	HWC_EVENT_MAP(POLYGONS),
	HWC_EVENT_MAP(POINTS),
	HWC_EVENT_MAP(LINES),
	HWC_EVENT_MAP(VCACHE_HIT),
	HWC_EVENT_MAP(VCACHE_MISS),
	HWC_EVENT_MAP(FRONT_FACING),
	HWC_EVENT_MAP(BACK_FACING),
	HWC_EVENT_MAP(PRIM_VISIBLE),
	HWC_EVENT_MAP(PRIM_CULLED),
	HWC_EVENT_MAP(PRIM_CLIPPED),

	HWC_EVENT_MAP(COMPRESS_IN),
	HWC_EVENT_MAP(COMPRESS_OUT),
	HWC_EVENT_MAP(COMPRESS_FLUSH),
	HWC_EVENT_MAP(TIMESTAMPS),
	HWC_EVENT_MAP(PCACHE_HIT),
	HWC_EVENT_MAP(PCACHE_MISS),
	HWC_EVENT_MAP(PCACHE_LINE),
	HWC_EVENT_MAP(PCACHE_STALL),
	HWC_EVENT_MAP(WRBUF_HIT),
	HWC_EVENT_MAP(WRBUF_MISS),
	HWC_EVENT_MAP(WRBUF_LINE),
	HWC_EVENT_MAP(WRBUF_PARTIAL),
	HWC_EVENT_MAP(WRBUF_STALL),
	HWC_EVENT_MAP(ACTIVE),
	HWC_EVENT_MAP(LOADING_DESC),
	HWC_EVENT_MAP(INDEX_WAIT),
	HWC_EVENT_MAP(INDEX_RANGE_WAIT),
	HWC_EVENT_MAP(VERTEX_WAIT),
	HWC_EVENT_MAP(PCACHE_WAIT),
	HWC_EVENT_MAP(WRBUF_WAIT),
	HWC_EVENT_MAP(BUS_READ),
	HWC_EVENT_MAP(BUS_WRITE),

	HWC_EVENT_MAP(TILER_UTLB_STALL),
	HWC_EVENT_MAP(TILER_UTLB_REPLAY_MISS),
	HWC_EVENT_MAP(TILER_UTLB_REPLAY_FULL),
	HWC_EVENT_MAP(TILER_UTLB_NEW_MISS),
	HWC_EVENT_MAP(TILER_UTLB_HIT),

	/* Shader */
	HWC_EVENT_MAP(SHADER_CORE_ACTIVE),
	HWC_EVENT_MAP(FRAG_ACTIVE),
	HWC_EVENT_MAP(FRAG_PRIMATIVES),
	HWC_EVENT_MAP(FRAG_PRIMATIVES_DROPPED),
	HWC_EVENT_MAP(FRAG_CYCLE_DESC),
	HWC_EVENT_MAP(FRAG_CYCLES_PLR),
	HWC_EVENT_MAP(FRAG_CYCLES_VERT),
	HWC_EVENT_MAP(FRAG_CYCLES_TRISETUP),
	HWC_EVENT_MAP(FRAG_CYCLES_RAST),
	HWC_EVENT_MAP(FRAG_THREADS),
	HWC_EVENT_MAP(FRAG_DUMMY_THREADS),
	HWC_EVENT_MAP(FRAG_QUADS_RAST),
	HWC_EVENT_MAP(FRAG_QUADS_EZS_TEST),
	HWC_EVENT_MAP(FRAG_QUADS_EZS_KILLED),
	HWC_EVENT_MAP(FRAG_QUADS_LZS_TEST),
	HWC_EVENT_MAP(FRAG_QUADS_LZS_KILLED),
	HWC_EVENT_MAP(FRAG_CYCLE_NO_TILE),
	HWC_EVENT_MAP(FRAG_NUM_TILES),
	HWC_EVENT_MAP(FRAG_TRANS_ELIM),
	HWC_EVENT_MAP(COMPUTE_ACTIVE),
	HWC_EVENT_MAP(COMPUTE_TASKS),
	HWC_EVENT_MAP(COMPUTE_THREADS),
	HWC_EVENT_MAP(COMPUTE_CYCLES_DESC),
	HWC_EVENT_MAP(TRIPIPE_ACTIVE),
	HWC_EVENT_MAP(ARITH_WORDS),
	HWC_EVENT_MAP(ARITH_CYCLES_REG),
	HWC_EVENT_MAP(ARITH_CYCLES_L0),
	HWC_EVENT_MAP(ARITH_FRAG_DEPEND),
	HWC_EVENT_MAP(LS_WORDS),
	HWC_EVENT_MAP(LS_ISSUES),
	HWC_EVENT_MAP(LS_RESTARTS),
	HWC_EVENT_MAP(LS_REISSUES_MISS),
	HWC_EVENT_MAP(LS_REISSUES_VD),
	HWC_EVENT_MAP(LS_REISSUE_ATTRIB_MISS),
	HWC_EVENT_MAP(LS_NO_WB),
	HWC_EVENT_MAP(TEX_WORDS),
	HWC_EVENT_MAP(TEX_BUBBLES),
	HWC_EVENT_MAP(TEX_WORDS_L0),
	HWC_EVENT_MAP(TEX_WORDS_DESC),
	HWC_EVENT_MAP(TEX_THREADS),
	HWC_EVENT_MAP(TEX_RECIRC_FMISS),
	HWC_EVENT_MAP(TEX_RECIRC_DESC),
	HWC_EVENT_MAP(TEX_RECIRC_MULTI),
	HWC_EVENT_MAP(TEX_RECIRC_PMISS),
	HWC_EVENT_MAP(TEX_RECIRC_CONF),
	HWC_EVENT_MAP(LSC_READ_HITS),
	HWC_EVENT_MAP(LSC_READ_MISSES),
	HWC_EVENT_MAP(LSC_WRITE_HITS),
	HWC_EVENT_MAP(LSC_WRITE_MISSES),
	HWC_EVENT_MAP(LSC_ATOMIC_HITS),
	HWC_EVENT_MAP(LSC_ATOMIC_MISSES),
	HWC_EVENT_MAP(LSC_LINE_FETCHES),
	HWC_EVENT_MAP(LSC_DIRTY_LINE),
	HWC_EVENT_MAP(LSC_SNOOPS),
	HWC_EVENT_MAP(AXI_TLB_STALL),
	HWC_EVENT_MAP(AXI_TLB_MISS),
	HWC_EVENT_MAP(AXI_TLB_TRANSACTION),
	HWC_EVENT_MAP(LS_TLB_MISS),
	HWC_EVENT_MAP(LS_TLB_HIT),
	HWC_EVENT_MAP(AXI_BEATS_READ),
	HWC_EVENT_MAP(AXI_BEATS_WRITE),

	/* MMU */
	HWC_EVENT_MAP(MMU_TABLE_WALK),
	HWC_EVENT_MAP(MMU_REPLAY_MISS),
	HWC_EVENT_MAP(MMU_REPLAY_FULL),
	HWC_EVENT_MAP(MMU_NEW_MISS),
	HWC_EVENT_MAP(MMU_HIT),

	HWC_EVENT_MAP(UTLB_STALL),
	HWC_EVENT_MAP(UTLB_REPLAY_MISS),
	HWC_EVENT_MAP(UTLB_REPLAY_FULL),
	HWC_EVENT_MAP(UTLB_NEW_MISS),
	HWC_EVENT_MAP(UTLB_HIT),

	HWC_EVENT_MAP(L2_WRITE_BEATS),
	HWC_EVENT_MAP(L2_READ_BEATS),
	HWC_EVENT_MAP(L2_ANY_LOOKUP),
	HWC_EVENT_MAP(L2_READ_LOOKUP),
	HWC_EVENT_MAP(L2_SREAD_LOOKUP),
	HWC_EVENT_MAP(L2_READ_REPLAY),
	HWC_EVENT_MAP(L2_READ_SNOOP),
	HWC_EVENT_MAP(L2_READ_HIT),
	HWC_EVENT_MAP(L2_CLEAN_MISS),
	HWC_EVENT_MAP(L2_WRITE_LOOKUP),
	HWC_EVENT_MAP(L2_SWRITE_LOOKUP),
	HWC_EVENT_MAP(L2_WRITE_REPLAY),
	HWC_EVENT_MAP(L2_WRITE_SNOOP),
	HWC_EVENT_MAP(L2_WRITE_HIT),
	HWC_EVENT_MAP(L2_EXT_READ_FULL),
	HWC_EVENT_MAP(L2_EXT_READ_HALF),
	HWC_EVENT_MAP(L2_EXT_WRITE_FULL),
	HWC_EVENT_MAP(L2_EXT_WRITE_HALF),
	HWC_EVENT_MAP(L2_EXT_READ),
	HWC_EVENT_MAP(L2_EXT_READ_LINE),
	HWC_EVENT_MAP(L2_EXT_WRITE),
	HWC_EVENT_MAP(L2_EXT_WRITE_LINE),
	HWC_EVENT_MAP(L2_EXT_WRITE_SMALL),
	HWC_EVENT_MAP(L2_EXT_BARRIER),
	HWC_EVENT_MAP(L2_EXT_AR_STALL),
	HWC_EVENT_MAP(L2_EXT_R_BUF_FULL),
	HWC_EVENT_MAP(L2_EXT_RD_BUF_FULL),
	HWC_EVENT_MAP(L2_EXT_R_RAW),
	HWC_EVENT_MAP(L2_EXT_W_STALL),
	HWC_EVENT_MAP(L2_EXT_W_BUF_FULL),
	HWC_EVENT_MAP(L2_EXT_R_W_HAZARD),
	HWC_EVENT_MAP(L2_TAG_HAZARD),
	HWC_EVENT_MAP(L2_SNOOP_FULL),
	HWC_EVENT_MAP(L2_REPLAY_FULL),
};

/*
 * Support for building hw counter enable bitmaps from enabled trace events.
 */
#define	__BLOCK_CHECK_ENABLED(block, bm_name) do {			\
	int ev;								\
	for (ev = block##_FIRST; ev <= block##_LAST; ev++) {		\
		const struct mali_hwcounter_trace_map *map =		\
		    &mali_hwcounter_map[ev];				\
		if (map->key != NULL && static_key_false(map->key)) {	\
			mali_hwcs.setup.bm_name |= 1<<hwc_bit(ev);	\
			ncounters++;					\
		}							\
	}								\
} while (0)
#define	JM_CHECK_ENABLED() __BLOCK_CHECK_ENABLED(MALI_HW_JM, jm_bm)
#define	TILER_CHECK_ENABLED() __BLOCK_CHECK_ENABLED(MALI_HW_TILER, tiler_bm)
#define	SHADER_CHECK_ENABLED() __BLOCK_CHECK_ENABLED(MALI_HW_SHADER, shader_bm)
#define	MMU_CHECK_ENABLED() __BLOCK_CHECK_ENABLED(MALI_HW_MMU, mmu_l2_bm)

/*
 * Support for dispatching trace events based on hw counter data.
 * Note we dispatch an event only when a counter changes value.
 */
static int block_update(const int boff, const int event_num)
{
	const u32 *block = ((const u32 *)((uintptr_t)mali_hwcs.buf + boff));
	u32 value = block[hwc_off(event_num)];
	if (value != mali_hwcs.last_read[event_num]) {
		mali_hwcs.last_read[event_num] = value;
		return true;
	} else
		return false;
}

#define	__BLOCK_DISPATCH_EVENTS(block, bm_name, boff) do {		\
	int ev;								\
	for (ev = block##_FIRST; ev <= block##_LAST; ev++) {		\
		const struct mali_hwcounter_trace_map *map =		\
		    &mali_hwcounter_map[ev];				\
		/* NB: each enable bit covers 4 counters */		\
		if ((mali_hwcs.setup.bm_name & (1<<hwc_bit(ev))) &&	\
		    map->key != NULL && static_key_false(map->key) &&	\
		    block_update(boff, ev))				\
			map->do_trace(mali_hwcs.last_read[ev]);		\
	}								\
} while (0)
#define	JM_DISPATCH_EVENTS() \
	__BLOCK_DISPATCH_EVENTS(MALI_HW_JM, jm_bm, 0x700)
#define	TILER_DISPATCH_EVENTS() \
	__BLOCK_DISPATCH_EVENTS(MALI_HW_TILER, tiler_bm, 0x400)
#define	MMU_DISPATCH_EVENTS() \
	__BLOCK_DISPATCH_EVENTS(MALI_HW_MMU, mmu_l2_bm, 0x500)

static int shader_block_update(const int event_num)
{
	const u32 *block = ((const u32 *)((uintptr_t)mali_hwcs.buf + 0x000));
	u32 value = block[hwc_off(event_num)]
		  + block[hwc_off(event_num) + 0x100]
		  + block[hwc_off(event_num) + 0x200]
		  + block[hwc_off(event_num) + 0x300]
		  ;
	if (value != mali_hwcs.last_read[event_num]) {
		mali_hwcs.last_read[event_num] = value;
		return true;
	} else
		return false;
}

#define	SHADER_DISPATCH_EVENTS() do {					\
	int ev;								\
	for (ev = MALI_HW_SHADER_FIRST; ev <= MALI_HW_SHADER_LAST; ev++) {\
		const struct mali_hwcounter_trace_map *map =		\
		    &mali_hwcounter_map[ev];				\
		/* NB: each enable bit covers 4 counters */		\
		if ((mali_hwcs.setup.shader_bm & (1<<hwc_bit(ev))) &&	\
		    map->key != NULL && static_key_false(map->key) &&	\
		    shader_block_update(ev))				\
			map->do_trace(mali_hwcs.last_read[ev]);		\
	}								\
} while (0)

/*
 * Collect hw counter data and dispatch trace events.
 * This runs from the workqueue so it can block when
 * fetching hw counter data.
 */
static void mali_hwcounter_collect(struct work_struct *w)
{
	mali_error error;

	osk_mutex_lock(&mali_hwcounter_mutex);
	if (mali_hwcs.active) {
		/* NB: dumping the counters can block */
		error = kbase_instr_hwcnt_dump(mali_hwcs.ctx);
		if (error == MALI_ERROR_NONE) {
			/* extract data and generate trace events */
			JM_DISPATCH_EVENTS();
			TILER_DISPATCH_EVENTS();
			SHADER_DISPATCH_EVENTS();
			MMU_DISPATCH_EVENTS();
		} else
			pr_err("%s: failed to dump hw counters\n", __func__);
	}
	osk_mutex_unlock(&mali_hwcounter_mutex);
}
static DECLARE_WORK(mali_hwcounter_work, mali_hwcounter_collect);

/*
 * drm vblank notifier; used to trigger hw counter polling.
 */
static int mali_hwcounter_vblank(struct notifier_block *nb,
	unsigned long unused, void *data)
{
	/* punt to work queue where we can block */
	queue_work_on(0, mali_hwcs.wq, &mali_hwcounter_work);
	return 0;
}
static struct notifier_block mali_vblank_notifier = {
	.notifier_call	= mali_hwcounter_vblank,
};

/*
 * Start hw counter polling.  Construct the counter enable bitmaps
 * based on the enabled trace events, call kbase to turn on counters,
 * and arrange polling at vblank.
 */
static int mali_hwcounter_polling_start(struct kbase_device *kbdev)
{
	int ncounters = 0;

	osk_mutex_lock(&mali_hwcounter_mutex);
	if (mali_hwcs.active) {
		osk_mutex_unlock(&mali_hwcounter_mutex);
		return -EALREADY;
	}

	/* Construct hw counter bitmaps from enabled trace events */
	memset(&mali_hwcs.setup, 0, sizeof(mali_hwcs.setup));

	JM_CHECK_ENABLED();
	TILER_CHECK_ENABLED();
	SHADER_CHECK_ENABLED();
	MMU_CHECK_ENABLED();

	if (ncounters > 0) {
		mali_error error;

		mali_hwcs.ctx = kbase_create_context(kbdev);
		if (mali_hwcs.ctx == NULL) {
			pr_err("%s: cannot create context\n", __func__);
			osk_mutex_unlock(&mali_hwcounter_mutex);
			return -ENOSPC;
		}
		mali_hwcs.buf = kbase_va_alloc(mali_hwcs.ctx,
					       MALI_HWC_DUMP_SIZE);
		mali_hwcs.setup.dump_buffer = (uintptr_t) mali_hwcs.buf;

		error = kbase_instr_hwcnt_enable(mali_hwcs.ctx,
						 &mali_hwcs.setup);
		if (error != MALI_ERROR_NONE) {
			pr_err("%s: cannot enable hw counters\n", __func__);

			kbase_va_free(mali_hwcs.ctx, mali_hwcs.buf);
			mali_hwcs.buf = NULL;

			kbase_destroy_context(mali_hwcs.ctx);
			mali_hwcs.ctx = NULL;

			osk_mutex_unlock(&mali_hwcounter_mutex);
			return -EIO;
		}
		kbase_instr_hwcnt_clear(mali_hwcs.ctx);
		mali_hwcs.active = true;
		/* NB: use 0 to minimize meaningless events */
		memset(mali_hwcs.last_read, 0, sizeof(mali_hwcs.last_read));

		drm_vblank_register_notifier(&mali_vblank_notifier);
	}
	osk_mutex_unlock(&mali_hwcounter_mutex);

	return 0;
}

/*
 * Stop hw counter polling. Disable polling, turn off hw counters,
 * and release our resources.
 */
static void mali_hwcounter_polling_stop(struct kbase_device *kbdev)
{
	osk_mutex_lock(&mali_hwcounter_mutex);
	if (mali_hwcs.active) {
		drm_vblank_unregister_notifier(&mali_vblank_notifier);

		kbase_instr_hwcnt_disable(mali_hwcs.ctx);

		kbase_va_free(mali_hwcs.ctx, mali_hwcs.buf);
		mali_hwcs.buf = NULL;

		kbase_destroy_context(mali_hwcs.ctx);
		mali_hwcs.ctx = NULL;

		mali_hwcs.active = false;
	}
	osk_mutex_unlock(&mali_hwcounter_mutex);
}

static ssize_t mali_sysfs_show_hwc_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", (mali_hwcs.active == true));
}

static ssize_t mali_sysfs_set_hwc_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	int error;
	bool enable;

	if (strtobool(buf, &enable) < 0)
		return -EINVAL;
	if (enable) {
		error = mali_hwcounter_polling_start(kbdev);
		if (error < 0)
			return error;
	} else
		mali_hwcounter_polling_stop(kbdev);
	return count;
}
DEVICE_ATTR(hwc_enable, S_IRUGO|S_IWUSR, mali_sysfs_show_hwc_enable,
	mali_sysfs_set_hwc_enable);

static int mali_setup_system_tracing(struct device *dev)
{
	osk_mutex_init(&mali_hwcounter_mutex, OSK_LOCK_ORDER_FIRST);
	mali_hwcs.wq = create_singlethread_workqueue("mali_hwc");
	mali_hwcs.active = false;

	if (device_create_file(dev, &dev_attr_hwc_enable)) {
		dev_err(dev, "Couldn't create sysfs file [hwc_enable]\n");
		return -ENOENT;
	}
	return 0;
}

static void mali_cleanup_system_tracing(struct device *dev)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	mali_hwcounter_polling_stop(kbdev);
	destroy_workqueue(mali_hwcs.wq);
	device_remove_file(dev, &dev_attr_hwc_enable);
	osk_mutex_term(&mali_hwcounter_mutex);
}
#else /* CONFIG_MALI_HWC_TRACE */
static int mali_setup_system_tracing(struct device *dev)
{
	return 0;
}

static void mali_cleanup_system_tracing(struct device *dev)
{
}
#endif /* CONFIG_MALI_HWC_TRACE */
