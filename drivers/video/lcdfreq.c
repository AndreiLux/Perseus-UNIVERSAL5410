/* linux/driver/video/samsung/lcdfreq.c
 *
 * EXYNOS4 - support LCD PixelClock change at runtime
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/reboot.h>
#include <linux/suspend.h>
#include <linux/cpufreq.h>
#include <linux/sysfs.h>
#include <linux/gcd.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>

#include <plat/clock.h>
#include <plat/clock-clksrc.h>
#include <plat/regs-fb.h>
#include <plat/regs-ielcd.h>

#include <mach/map.h>

#define FIMD_REG_BASE			S5P_PA_FIMD1
#define FIMD_MAP_SIZE			SZ_1K

#define IELCD_REG_BASE		S3C_IELCD_PHY_BASE
#define IELCD_MAP_SIZE		SZ_1K

#define VSTATUS_IS_ACTIVE(reg)	(reg == VIDCON1_VSTATUS_ACTIVE)
#define VSTATUS_IS_FRONT(reg)		(reg == VIDCON1_VSTATUS_FRONTPORCH)

#define reg_mask(shift, size)		((0xffffffff >> (32 - size)) << shift)
#define POWER_IS_ON(pwr)		((pwr) <= FB_BLANK_UNBLANK)

enum lcdfreq_level {
	NORMAL,
	LIMIT,
	LEVEL_MAX,
};

struct lcdfreq_t {
	enum lcdfreq_level level;
	u32 hz;
	u32 vclk;
	u32 cmu_div;
	u32 pixclock;
};

struct lcdfreq_info {
	struct lcdfreq_t	table[LEVEL_MAX];
	enum lcdfreq_level	level;
	atomic_t		usage;
	struct mutex		lock;
	spinlock_t		slock;

	struct device		*dev;
	struct clksrc_clk	*clksrc;

	unsigned int		enable;
	struct notifier_block	pm_noti;
	struct notifier_block	reboot_noti;

	struct delayed_work	work;

	void __iomem		*ielcd_reg;

	int			blank;

	struct notifier_block	fb_notif;

	struct clk		*bus_clk;
	struct clk		*axi_disp1;
	struct clk		*lcd_clk;
	struct clk		*mdnie_bus_clk;
	struct clk		*mdnie_clk;
};

static inline struct lcdfreq_info *dev_get_lcdfreq(struct device *dev)
{
	struct fb_info *fb = dev_get_drvdata(dev);

	return (struct lcdfreq_info *)(fb->fix.reserved[1] << 16 | fb->fix.reserved[0]);
}

static unsigned int get_vstatus(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	u32 reg;

	reg = readl(info->ielcd_reg + IELCD_VIDCON1);
	reg &= VIDCON1_VSTATUS_MASK;

	return reg;
}

static struct clksrc_clk *get_clksrc(struct device *dev)
{
	struct clk *clk;
	struct clksrc_clk *clksrc;

	clk = clk_get(dev, "sclk_fimd");

	clksrc = container_of(clk, struct clksrc_clk, clk);

	return clksrc;
}

static void reset_div(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc = info->clksrc;
	u32 reg;

	reg = __raw_readl(clksrc->reg_div.reg);
	reg &= ~(0xff);
	reg |= info->table[info->level].cmu_div;

	writel(reg, clksrc->reg_div.reg);
}

static int get_div(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc = info->clksrc;

	u32 reg = __raw_readl(clksrc->reg_div.reg);
	u32 mask = reg_mask(clksrc->reg_div.shift, clksrc->reg_div.size);

	reg &= mask;
	reg >>= clksrc->reg_div.shift;

	return reg;
}

static int set_div(struct device *dev, u32 div)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc = info->clksrc;
	u32 mask = reg_mask(clksrc->reg_div.shift, clksrc->reg_div.size);

	unsigned long flags;
	u32 reg, count = 1000000;

	do {
		spin_lock_irqsave(&info->slock, flags);
		reg = __raw_readl(clksrc->reg_div.reg);

		if ((reg & mask) == (div & mask)) {
			spin_unlock_irqrestore(&info->slock, flags);
			return -EINVAL;
		}

		reg &= ~(0xff);
		reg |= div;

		if (VSTATUS_IS_ACTIVE(get_vstatus(dev))) {
			if (VSTATUS_IS_FRONT(get_vstatus(dev))) {
				writel(reg, clksrc->reg_div.reg);
				spin_unlock_irqrestore(&info->slock, flags);
				dev_info(dev, "%x, %d\n", __raw_readl(clksrc->reg_div.reg), 1000000-count);
				return 0;
			}
		}
		spin_unlock_irqrestore(&info->slock, flags);
		count--;
	} while (count && info->enable);

	dev_err(dev, "%s skip, div=%d\n", __func__, div);

	return -EINVAL;
}

static unsigned char get_fimd_divider(void)
{
	void __iomem *fimd_reg;
	unsigned int reg;

	fimd_reg = ioremap(FIMD_REG_BASE, FIMD_MAP_SIZE);

	reg = readl(fimd_reg + VIDCON0);
	reg &= VIDCON0_CLKVAL_F_MASK;
	reg >>= VIDCON0_CLKVAL_F_SHIFT;

	iounmap(fimd_reg);

	return reg;
}

static int get_divider(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc;
	struct clk *clk;
	u32 rate, reg, fimd_div, i;

	info->clksrc = clksrc = get_clksrc(dev->parent);
	clk = clk_get_parent(&clksrc->clk);
	rate = clk_get_rate(clk);

	for (i = 0; i < LEVEL_MAX; i++)
		info->table[i].cmu_div = DIV_ROUND_CLOSEST(rate, info->table[i].vclk);

	if (info->table[LIMIT].cmu_div > (1 << clksrc->reg_div.size))
		fimd_div = gcd(info->table[NORMAL].cmu_div, info->table[LIMIT].cmu_div);
	else
		fimd_div = 1;

	dev_info(dev, "%s rate is %d, fimd div=%d\n", clk->name, rate, fimd_div);

	reg = get_fimd_divider() + 1;

	if ((!fimd_div) || (fimd_div > 256) || (fimd_div != reg)) {
		dev_info(dev, "%s skip, fimd div=%d, reg=%d\n", __func__, fimd_div, reg);
		goto err;
	}

	for (i = 0; i < LEVEL_MAX; i++) {
		info->table[i].cmu_div /= fimd_div;
		if (info->table[i].cmu_div > (1 << clksrc->reg_div.size)) {
			dev_info(dev, "%s skip, cmu div=%d\n", __func__, info->table[i].cmu_div);
			goto err;
		}
		dev_info(dev, "%dhz div is %d\n", info->table[i].hz, info->table[i].cmu_div);
		info->table[i].cmu_div--;
	}

	reg = get_div(dev);
	if (info->table[NORMAL].cmu_div != reg) {
		dev_info(dev, "%s skip, cmu div=%d, reg=%d\n", __func__, info->table[NORMAL].cmu_div, reg);
		goto err;
	}

	for (i = 0; i < LEVEL_MAX; i++) {
		reg = info->table[i].cmu_div;
		info->table[i].cmu_div = (reg << clksrc->reg_div.size) | reg;
	}

	return 0;

err:
	return -EINVAL;
}

static int set_lcdfreq_div(struct device *dev, enum lcdfreq_level level)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	u32 ret;

	mutex_lock(&info->lock);

	if (!info->enable) {
		dev_err(dev, "%s reject. enable flag is %d\n", __func__, info->enable);
		ret = -EINVAL;
		goto exit;
	}

	ret = set_div(dev, info->table[level].cmu_div);

	if (ret) {
		dev_err(dev, "skip to change lcd freq\n");
		goto exit;
	}

	info->level = level;

exit:
	mutex_unlock(&info->lock);

	return ret;
}

static int lcdfreq_lock(struct device *dev, int value)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	int ret;

	if (!atomic_read(&info->usage))
		ret = set_lcdfreq_div(dev, value);
	else {
		dev_err(dev, "lcd freq is already limit state\n");
		return -EINVAL;
	}

	if (!ret) {
		mutex_lock(&info->lock);
		atomic_inc(&info->usage);
		mutex_unlock(&info->lock);
		schedule_delayed_work(&info->work, 0);
	}

	return ret;
}

static int lcdfreq_lock_free(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	int ret;

	if (atomic_read(&info->usage))
		ret = set_lcdfreq_div(dev, NORMAL);
	else {
		dev_err(dev, "lcd freq is already normal state\n");
		return -EINVAL;
	}

	if (!ret) {
		mutex_lock(&info->lock);
		atomic_dec(&info->usage);
		mutex_unlock(&info->lock);
		cancel_delayed_work(&info->work);
	}

	return ret;
}

static ssize_t level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcdfreq_info *info = dev_get_lcdfreq(dev);

	if (!info->enable) {
		dev_err(info->dev, "%s reject. enable flag is %d\n", __func__, info->enable);
		return -EINVAL;
	}

	return sprintf(buf, "%dhz, div=%d\n", info->table[info->level].hz, get_div(info->dev));
}

static ssize_t level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct lcdfreq_info *info = dev_get_lcdfreq(dev);
	unsigned int value;
	int ret;

	if (!info->enable) {
		dev_err(info->dev, "%s reject. enable flag is %d\n", __func__, info->enable);
		return -EINVAL;
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(info->dev, "%s :: value=%d\n", __func__, value);

	if (value >= LEVEL_MAX)
		return -EINVAL;

	if (value)
		ret = lcdfreq_lock(info->dev, value);
	else
		ret = lcdfreq_lock_free(info->dev);

	if (ret) {
		dev_err(info->dev, "%s skip\n", __func__);
		return -EINVAL;
	}
	return count;
}

static ssize_t usage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcdfreq_info *info = dev_get_lcdfreq(dev);

	return sprintf(buf, "%d\n", atomic_read(&info->usage));
}

static DEVICE_ATTR(level, S_IRUGO|S_IWUSR, level_show, level_store);
static DEVICE_ATTR(usage, S_IRUGO, usage_show, NULL);

static struct attribute *lcdfreq_attributes[] = {
	&dev_attr_level.attr,
	&dev_attr_usage.attr,
	NULL,
};

static struct attribute_group lcdfreq_attr_group = {
	.name = "lcdfreq",
	.attrs = lcdfreq_attributes,
};

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct lcdfreq_info *info;
	struct fb_event *evdata = data;
	int fb_blank;
	unsigned long flags;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	info = container_of(self, struct lcdfreq_info, fb_notif);

	fb_blank = *(int *)evdata->data;

	if (POWER_IS_ON(fb_blank) && !POWER_IS_ON(info->blank)) {
		dev_info(info->dev, "%s: %d, %d\n", __func__, info->blank, fb_blank);

		spin_lock_irqsave(&info->slock, flags);
		pm_runtime_get_sync(info->dev);
		spin_unlock_irqrestore(&info->slock, flags);

		mutex_lock(&info->lock);
		info->enable = true;
		mutex_unlock(&info->lock);
	} else if (!POWER_IS_ON(fb_blank) && POWER_IS_ON(info->blank)) {
		dev_info(info->dev, "%s: %d, %d\n", __func__, info->blank, fb_blank);

		mutex_lock(&info->lock);
		info->enable = false;
		info->level = NORMAL;
		reset_div(info->dev);
		atomic_set(&info->usage, 0);
		mutex_unlock(&info->lock);

		spin_lock_irqsave(&info->slock, flags);
		pm_runtime_put_sync(info->dev);
		spin_unlock_irqrestore(&info->slock, flags);
	}

	info->blank = fb_blank;

	return 0;
}

#if 0
static int lcdfreq_pm_notifier_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct lcdfreq_info *info =
		container_of(this, struct lcdfreq_info, pm_noti);

	dev_info(info->dev, "%s :: event=%ld\n", __func__, event);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&info->lock);
		info->enable = false;
		info->level = NORMAL;
		reset_div(info->dev);
		atomic_set(&info->usage, 0);
		mutex_unlock(&info->lock);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&info->lock);
		info->enable = true;
		mutex_unlock(&info->lock);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}
#endif

static int lcdfreq_reboot_notify(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct lcdfreq_info *info =
		container_of(this, struct lcdfreq_info, reboot_noti);

	mutex_lock(&info->lock);
	info->enable = false;
	info->level = NORMAL;
	reset_div(info->dev);
	atomic_set(&info->usage, 0);
	mutex_unlock(&info->lock);

	dev_info(info->dev, "%s\n", __func__);

	return NOTIFY_DONE;
}

static void lcdfreq_status_work(struct work_struct *work)
{
	struct lcdfreq_info *info =
		container_of(work, struct lcdfreq_info, work.work);

	u32 hz = info->table[info->level].hz;

	cancel_delayed_work(&info->work);

	dev_info(info->dev, "hz=%d, usage=%d\n", hz, atomic_read(&info->usage));

	schedule_delayed_work(&info->work, HZ*120);
}

static struct fb_videomode *get_videmode(struct list_head *list)
{
	struct fb_modelist *modelist;
	struct list_head *pos;
	struct fb_videomode *m = NULL;

	if (!list->prev || !list->next || list_empty(list))
		goto exit;

	list_for_each(pos, list) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;
	}

exit:
	return m;
}

static int clk_init(struct device *dev)
{
	int ret = 0;
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	info->bus_clk = clk_get(dev->parent, "lcd");
	if (IS_ERR(info->bus_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		ret = PTR_ERR(info->bus_clk);
		goto exit;
	}

	info->axi_disp1 = clk_get(dev->parent, "axi_disp1");
	if (IS_ERR(info->axi_disp1)) {
		dev_err(dev, "failed to get axi bus clock\n");
		ret = PTR_ERR(info->axi_disp1);
		goto err_bus_clk;
	}

	info->lcd_clk = clk_get(dev->parent, "sclk_fimd");
	if (IS_ERR(info->lcd_clk)) {
		dev_err(dev, "failed to get lcd clock\n");
		ret = PTR_ERR(info->lcd_clk);
		goto err_axi_clk;
	}

#ifdef CONFIG_FB_S5P_MDNIE
	info->mdnie_bus_clk = clk_get(NULL, "mdnie1");
	if (IS_ERR(info->mdnie_bus_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		ret = PTR_ERR(info->mdnie_bus_clk);
		goto err_sclk_fimd;
	}

	info->mdnie_clk = clk_get(NULL, "sclk_mdnie");
	if (IS_ERR(info->mdnie_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		ret = PTR_ERR(info->mdnie_clk);
		goto err_mdnie1;
	}
#endif
	return 0;

err_mdnie1:
	clk_disable(info->mdnie_bus_clk);
	clk_put(info->mdnie_bus_clk);
err_sclk_fimd:
	clk_disable(info->lcd_clk);
	clk_put(info->lcd_clk);
err_axi_clk:
	clk_disable(info->axi_disp1);
	clk_put(info->axi_disp1);
err_bus_clk:
	clk_disable(info->bus_clk);
	clk_put(info->bus_clk);
exit:
	return ret;
}

static int lcdfreq_probe(struct platform_device *pdev)
{
	struct fb_info *fb = registered_fb[0];
	struct fb_videomode *m;
	struct lcdfreq_info *info = NULL;
	unsigned int vclk, freq_limit;
	int ret = 0;
	unsigned int *limit = NULL;

	m = get_videmode(&fb->modelist);
	if (!m) {
		pr_err("fail to get_videmode\n");
		goto err_1;
	}

	info = kzalloc(sizeof(struct lcdfreq_info), GFP_KERNEL);
	if (!info) {
		pr_err("fail to allocate for lcdfreq\n");
		ret = -ENOMEM;
		goto err_1;
	}

	limit = pdev->dev.platform_data;
	freq_limit = *limit;
	if (!freq_limit) {
		pr_err("skip to get platform data for lcdfreq\n");
		ret = -EINVAL;
		goto err_2;
	}

	platform_set_drvdata(pdev, info);

	fb->fix.reserved[0] = (u32)info;
	fb->fix.reserved[1] = (u32)info >> 16;

	info->dev = &pdev->dev;
	info->level = NORMAL;
	info->blank = FB_BLANK_UNBLANK;

	vclk = (m->left_margin + m->right_margin + m->hsync_len + m->xres) *
		(m->upper_margin + m->lower_margin + m->vsync_len + m->yres);

	info->table[NORMAL].level = NORMAL;
	info->table[NORMAL].vclk = vclk * m->refresh;
	info->table[NORMAL].pixclock = m->pixclock;
	info->table[NORMAL].hz = m->refresh;

	info->table[LIMIT].level = LIMIT;
	info->table[LIMIT].vclk = vclk * freq_limit;
	info->table[LIMIT].pixclock = KHZ2PICOS((vclk * freq_limit)/1000);
	info->table[LIMIT].hz = freq_limit;

	ret = get_divider(info->dev);
	if (ret < 0) {
		pr_err("%s skip: get_divider", __func__);
		fb->fix.reserved[0] = 0;
		fb->fix.reserved[1] = 0;
		goto err_1;
	}

	atomic_set(&info->usage, 0);
	mutex_init(&info->lock);
	spin_lock_init(&info->slock);

	INIT_DELAYED_WORK_DEFERRABLE(&info->work, lcdfreq_status_work);

	ret = sysfs_create_group(&fb->dev->kobj, &lcdfreq_attr_group);
	if (ret < 0) {
		pr_err("%s skip: sysfs_create_group\n", __func__);
		goto err_2;
	}

	/* info->pm_noti.notifier_call = lcdfreq_pm_notifier_event; */
	info->reboot_noti.notifier_call = lcdfreq_reboot_notify;
	info->fb_notif.notifier_call = fb_notifier_callback;

	register_reboot_notifier(&info->reboot_noti);
	fb_register_client(&info->fb_notif);

	info->ielcd_reg = ioremap(IELCD_REG_BASE, IELCD_MAP_SIZE);
	info->enable = true;

	ret = clk_init(&pdev->dev);
	if (ret) {
		pr_err("%s skip: clk_init\n", __func__);
		goto err_3;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	dev_info(info->dev, "%s is done\n", __func__);

	return 0;

err_3:
	iounmap(info->ielcd_reg);
	sysfs_remove_group(&fb->dev->kobj, &lcdfreq_attr_group);
err_2:
	kfree(info);
err_1:
	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int lcdfreq_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lcdfreq_info *info = platform_get_drvdata(pdev);

	clk_disable(info->lcd_clk);
	clk_disable(info->axi_disp1);
	clk_disable(info->bus_clk);

#ifdef CONFIG_FB_S5P_MDNIE
	clk_disable(info->mdnie_bus_clk);
	clk_disable(info->mdnie_clk);
#endif
	return 0;
}

static int lcdfreq_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lcdfreq_info *info = platform_get_drvdata(pdev);

	clk_enable(info->bus_clk);
	clk_enable(info->axi_disp1);
	clk_enable(info->lcd_clk);

#ifdef CONFIG_FB_S5P_MDNIE
	clk_enable(info->mdnie_bus_clk);
	clk_enable(info->mdnie_clk);
#endif

	return 0;
}
#endif

static const struct dev_pm_ops lcdfreq_pm_ops = {
	SET_RUNTIME_PM_OPS(lcdfreq_runtime_suspend, lcdfreq_runtime_resume, NULL)
};


static struct platform_driver lcdfreq_driver = {
	.probe		= lcdfreq_probe,
	.driver		= {
		.name	= "lcdfreq",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
		.pm	= &lcdfreq_pm_ops,
#endif
	},
};

static int __init lcdfreq_init(void)
{
	return platform_driver_register(&lcdfreq_driver);
}
late_initcall(lcdfreq_init);

static void __exit lcdfreq_exit(void)
{
	platform_driver_unregister(&lcdfreq_driver);
}
module_exit(lcdfreq_exit);

