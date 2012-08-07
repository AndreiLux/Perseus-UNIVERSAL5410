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
#if MALI_USE_UMP == 1
#include <ump/ump_common.h>
#endif /*MALI_USE_UMP == 1*/

#define	CREATE_TRACE_POINTS
#include <trace/events/mali.h>

#if MALI_UNCACHED == 0
#error MALI_UNCACHED should equal 1 for Exynos5 support, your scons commandline should contain 'no_syncsets=1'
#endif

#define MALI_DVFS_DEBUG 0
#define MALI_DVFS_STEP 7
#define MALI_DVFS_KEEP_STAY_CNT 10

#define HZ_IN_MHZ            (1000000)
#define MALI_RTPM_DEBUG      0
#define VITHAR_DEFAULT_CLOCK 533000000
#define RUNTIME_PM_DELAY_TIME 10
#define CONFIG_T6XX_HWVER_R0P0 1

struct regulator *kbase_platform_get_regulator(void);
int kbase_platform_regulator_init(void);
int kbase_platform_regulator_disable(void);
int kbase_platform_regulator_enable(struct device *dev);
int kbase_platform_get_default_voltage(struct device *dev, int *vol);
int kbase_platform_get_voltage(struct device *dev, int *vol);
int kbase_platform_set_voltage(struct device *dev, int vol);
void kbase_platform_dvfs_set_clock(kbase_device *kbdev, int freq);

#ifdef CONFIG_T6XX_DVFS
int kbase_platform_dvfs_init(kbase_device *kbdev);
void kbase_platform_dvfs_term(void);
int kbase_platform_dvfs_get_control_status(void);
#endif /* CONFIG_T6XX_DVFS */
int kbase_platform_cmu_pmu_control(struct kbase_device *kbdev, int control);
void kbase_platform_remove_sysfs_file(struct device *dev);
mali_error kbase_platform_init(struct kbase_device *kbdev);
static int kbase_platform_is_power_on(void);
void kbase_platform_term(struct kbase_device *kbdev);

#ifdef CONFIG_T6XX_DEBUG_SYS
static int kbase_platform_create_sysfs_file(struct device *dev);
#endif /* CONFIG_T6XX_DEBUG_SYS */

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
#if MALI_USE_UMP == 1
	{
		KBASE_CONFIG_ATTR_UMP_DEVICE,
		UMP_DEVICE_Z_SHIFT
	},
#endif /* MALI_USE_UMP == 1 */
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
		kbase_platform_set_voltage( dev, 1250000 );
		kbase_platform_dvfs_set_clock(kbdev, 533);
	} else if (sysfs_streq("450", buf)) {
		kbase_platform_set_voltage( dev, 1150000 );
		kbase_platform_dvfs_set_clock(kbdev, 450);
	} else if (sysfs_streq("400", buf)) {
		kbase_platform_set_voltage( dev, 1100000 );
		kbase_platform_dvfs_set_clock(kbdev, 400);
	} else if (sysfs_streq("350", buf)) {
		kbase_platform_set_voltage(dev, 1075000);
		kbase_platform_dvfs_set_clock(kbdev, 350);
	} else if (sysfs_streq("266", buf)) {
		kbase_platform_set_voltage( dev, 937500);
		kbase_platform_dvfs_set_clock(kbdev, 266);
	} else if (sysfs_streq("160", buf)) {
		kbase_platform_set_voltage( dev, 937500 );
		kbase_platform_dvfs_set_clock(kbdev, 160);
	} else if (sysfs_streq("100", buf)) {
		kbase_platform_set_voltage( dev, 937500 );
		kbase_platform_dvfs_set_clock(kbdev, 100);
	} else {
		pr_err("%s: invalid value\n", __func__);
		return -ENOENT;
	}

	pr_info("aclk400 %u\n", (unsigned int)clk_get_rate(platform->sclk_g3d));

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
	int vol;
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	if (sysfs_streq("off", buf)) {
		if (kbasep_pm_metrics_isactive(kbdev)) {
			kbasep_pm_metrics_term(kbdev);

			kbase_platform_get_default_voltage(dev, &vol);
			if (vol != 0)
				kbase_platform_set_voltage(dev, vol);
			kbase_platform_dvfs_set_clock(kbdev,
			    VITHAR_DEFAULT_CLOCK / 1000000);
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
#endif /* CONFIG_T6XX_DVFS */
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
#endif /* CONFIG_T6XX_DVFS */
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
typedef struct _mali_dvfs_info{
	unsigned int voltage;
	unsigned int clock;
	int min_threshold;
	int max_threshold;
}mali_dvfs_info;

typedef struct _mali_dvfs_status_type{
	kbase_device *kbdev;
	int step;
	int utilisation;
	int above_threshold;
	int below_threshold;
	uint noutilcnt;
}mali_dvfs_status;

static struct workqueue_struct *mali_dvfs_wq = 0;
int mali_dvfs_control=0;
osk_spinlock mali_dvfs_spinlock;


/*dvfs status*/
static mali_dvfs_status mali_dvfs_status_current;
static mali_dvfs_info mali_dvfs_infotbl[MALI_DVFS_STEP] = {
#if (MALI_DVFS_STEP == 7)
	{912500, 100, 0, 60},
	{925000, 160, 40, 65},
	{1025000, 266, 50, 70},
	{1075000, 350, 55, 75},
	{1125000, 400, 65, 80},
	{1150000, 450, 70, 89},
	{1250000, 533, 85, 100}

#else
#error no table
#endif
};

static void kbase_platform_dvfs_set_vol(unsigned int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return;

	trace_mali_dvfs_set_voltage(vol);

	switch(vol)
	{
		case 1250000:
		case 1150000:
		case 1100000:
		case 1000000:
		case 937500:
		case 812500:
		case 750000:
			kbase_platform_set_voltage(NULL, vol);
			break;
		default:
			return;
	}

	_vol = vol;

#if MALI_DVFS_DEBUG
	printk("dvfs_set_vol %dmV\n", vol);
#endif /*  MALI_DVFS_DEBUG */
	return;
}

void kbase_platform_dvfs_set_level(int level)
{
	static int level_prev=-1;

	if (level == level_prev)
		return;

	if (WARN_ON(level >= MALI_DVFS_STEP))
		panic("invalid level");

	if (level > level_prev) {
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[level].voltage);
		kbase_platform_dvfs_set_clock(mali_dvfs_status_current.kbdev, mali_dvfs_infotbl[level].clock);
	}else{
		kbase_platform_dvfs_set_clock(mali_dvfs_status_current.kbdev, mali_dvfs_infotbl[level].clock);
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[level].voltage);
	}
	level_prev = level;
}

static void mali_dvfs_event_proc(struct work_struct *w)
{
	mali_dvfs_status dvfs_status;

	/* TODO(Shariq) set to proper default values */
	int allow_up_cnt = 1000 / KBASE_PM_DVFS_FREQUENCY;
	int allow_down_cnt = 4000 / KBASE_PM_DVFS_FREQUENCY;

	osk_spinlock_lock(&mali_dvfs_spinlock);
	dvfs_status = mali_dvfs_status_current;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

	 /*If no input is keeping for longtime, first step will be @266MHZ. */
	 if (dvfs_status.noutilcnt > 3 && dvfs_status.utilisation > 0)
		dvfs_status.step = MALI_DVFS_STEP - 5;
	else if (dvfs_status.utilisation == 100) {
		allow_up_cnt = 500 / KBASE_PM_DVFS_FREQUENCY;
	} else if (mali_dvfs_infotbl[dvfs_status.step].clock < 266) {
		allow_up_cnt = 1000 / KBASE_PM_DVFS_FREQUENCY;
		allow_down_cnt = 2500 / KBASE_PM_DVFS_FREQUENCY;
	} else if (mali_dvfs_infotbl[dvfs_status.step].clock == 266) {
		allow_up_cnt = 2500 / KBASE_PM_DVFS_FREQUENCY;
	} else {
		allow_up_cnt = 1500 / KBASE_PM_DVFS_FREQUENCY;
		allow_down_cnt =  1000 / KBASE_PM_DVFS_FREQUENCY;
	}

	OSK_ASSERT(dvfs_status.utilisation <= 100);
	if (dvfs_status.utilisation > mali_dvfs_infotbl[dvfs_status.step].max_threshold)
	{
		dvfs_status.above_threshold++;
		dvfs_status.below_threshold = 0;
		if (dvfs_status.above_threshold > allow_up_cnt) {
			dvfs_status.step++;
			dvfs_status.above_threshold = 0;
			OSK_ASSERT(dvfs_status.step < MALI_DVFS_STEP);
		}
	} else if ((dvfs_status.step > 0) && (dvfs_status.utilisation <
			mali_dvfs_infotbl[dvfs_status.step].min_threshold)) {
		dvfs_status.below_threshold++;
		dvfs_status.above_threshold = 0;
		if (dvfs_status.below_threshold > allow_down_cnt)
		{
			OSK_ASSERT(dvfs_status.step > 0);
			dvfs_status.step--;
			dvfs_status.below_threshold = 0;
		}
	}else{
		dvfs_status.above_threshold = 0;
		dvfs_status.below_threshold = 0;
	}

	if (kbasep_pm_metrics_isactive(dvfs_status.kbdev))
		kbase_platform_dvfs_set_level(dvfs_status.step);

	if (dvfs_status.utilisation == 0) {
		dvfs_status.noutilcnt++;
	} else {
		dvfs_status.noutilcnt=0;
	}

#if MALI_DVFS_DEBUG
	 printk(KERN_DEBUG "[mali_dvfs] utilisation: %d step: %d[%d,%d] cnt: %d/%d vsync %d\n",
	       dvfs_status.utilisation, dvfs_status.step,
	       mali_dvfs_infotbl[dvfs_status.step].min_threshold,
	       mali_dvfs_infotbl[dvfs_status.step].max_threshold,
	       dvfs_status.above_threshold, dvfs_status.below_threshold,
	       dvfs_status.kbdev->pm.metrics.vsync_hit);

#endif /* MALI_DVFS_DEBUG */

	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current=dvfs_status;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

}

static DECLARE_WORK(mali_dvfs_work, mali_dvfs_event_proc);

int kbase_platform_dvfs_event(struct kbase_device *kbdev)
{
#ifdef CONFIG_T6XX_DVFS
	int utilisation = kbase_pm_get_dvfs_utilisation(kbdev);

	trace_mali_dvfs_event(utilisation);

	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.utilisation = utilisation;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	/*add error handle here*/
	return MALI_TRUE;
#else
	return MALI_FALSE;
#endif
}

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

	osk_spinlock_init(&mali_dvfs_spinlock,OSK_LOCK_ORDER_PM_METRICS);

	/*add a error handling here*/
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.kbdev = kbdev;
	mali_dvfs_status_current.utilisation = 100;
	mali_dvfs_status_current.step = MALI_DVFS_STEP-1;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

	return MALI_TRUE;
}

void kbase_platform_dvfs_term(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}
#endif /* CONFIG_T6XX_DVFS */

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

int kbase_platform_regulator_enable(struct device *dev)
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
	int utilisation=0;
	osk_ticks now = osk_time_now();

	OSK_ASSERT(kbdev != NULL);

	osk_spinlock_irq_lock(&kbdev->pm.metrics.lock);

	if (kbdev->pm.metrics.gpu_active)
	{
		kbdev->pm.metrics.time_busy += osk_time_elapsed(kbdev->pm.metrics.time_period_start, now);
		kbdev->pm.metrics.time_period_start = now;
	}
	else
	{
		kbdev->pm.metrics.time_idle += osk_time_elapsed(kbdev->pm.metrics.time_period_start, now);
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

	osk_spinlock_irq_unlock(&kbdev->pm.metrics.lock);

	return utilisation;
}
#endif
