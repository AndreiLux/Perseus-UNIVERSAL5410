/* linux/drivers/video/mdnie.c
 *
 * Register interface file for Samsung mDNIe driver
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/mdnie.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/gpio.h>

#include <plat/fb.h>
#include <plat/regs-mdnie.h>
#include <plat/regs-fb-v4.h>
#include <plat/regs-fb.h>

#include <mach/map.h>
#include <plat/gpio-cfg.h>
#include "mdnie.h"
#if defined(CONFIG_LCD_MIPI_S6E8FA0) || defined(CONFIG_LCD_MIPI_S6E3FA0)
#include "mdnie_table_j.h"
#elif defined(CONFIG_LCD_LSL122BC01)
#include "mdnie_table_v1.h"
#endif
#include "mdnie_color_tone_5410.h"

#if defined(CONFIG_TDMB)
#include "mdnie_dmb.h"
#endif

#define FIMD_REG_BASE			S5P_PA_FIMD1
#ifdef CONFIG_FB_EXYNOS_FIMD_V8
#define FIMD_MAP_SIZE			SZ_256K
#else
#define FIMD_MAP_SIZE			SZ_32K
#endif

static void __iomem *fimd_reg;
static struct resource *s3c_mdnie_mem;
static void __iomem *s3c_mdnie_base;

#define s3c_mdnie_read(addr)		readl(s3c_mdnie_base + addr*4)
#define s3c_mdnie_write(addr, val)	writel(val, s3c_mdnie_base + addr*4)

#define MDNIE_SYSFS_PREFIX		"/sdcard/mdnie/"
#define PANEL_COORDINATE_PATH	"/sys/class/lcd/panel/color_coordinate"

#if defined(CONFIG_TDMB)
#define SCENARIO_IS_DMB(scenario)	(scenario == DMB_NORMAL_MODE)
#else
#define SCENARIO_IS_DMB(scenario)	NULL
#endif

#define SCENARIO_IS_VIDEO(scenario)			(scenario == VIDEO_MODE)
#define SCENARIO_IS_VALID(scenario)			(SCENARIO_IS_DMB(scenario) || scenario < SCENARIO_MAX)

#define ACCESSIBILITY_IS_VALID(accessibility)	(accessibility && (accessibility < ACCESSIBILITY_MAX))

#define ADDRESS_IS_SCR_WHITE(address)		(address >= S3C_MDNIE_rWHITE_R && address <= S3C_MDNIE_rWHITE_B)
#define ADDRESS_IS_SCR_RGB(address)			(address >= S3C_MDNIE_rRED_R && address <= S3C_MDNIE_rGREEN_B)

#define SCR_RGB_MASK(value)				(value % S3C_MDNIE_rRED_R)

static struct class *mdnie_class;
struct mdnie_info *g_mdnie;

static int mdnie_write(unsigned int addr, unsigned int val)
{
	s3c_mdnie_write(addr, val);

	return 0;
}

static int mdnie_mask(void)
{
	s3c_mdnie_write(S3C_MDNIE_rRFF, 1);

	return 0;
}

static int mdnie_unmask(void)
{
	s3c_mdnie_write(S3C_MDNIE_rRFF, 0);

	return 0;
}

int s3c_mdnie_hw_init(void)
{
	s3c_mdnie_mem = request_mem_region(S3C_MDNIE_PHY_BASE, S3C_MDNIE_MAP_SIZE, "mdnie");
	if (IS_ERR_OR_NULL(s3c_mdnie_mem)) {
		pr_err("%s: fail to request_mem_region\n", __func__);
		return -ENOENT;
	}

	s3c_mdnie_base = ioremap(S3C_MDNIE_PHY_BASE, S3C_MDNIE_MAP_SIZE);
	if (IS_ERR_OR_NULL(s3c_mdnie_base)) {
		pr_err("%s: fail to ioremap\n", __func__);
		return -ENOENT;
	}

	fimd_reg = ioremap(FIMD_REG_BASE, FIMD_MAP_SIZE);
	if (fimd_reg == NULL) {
		pr_err("%s: fail to ioremap - fimd\n", __func__);
		return -ENOENT;
	}
	if (g_mdnie)
		g_mdnie->enable = TRUE;

	return 0;
}

void mdnie_s3cfb_suspend(void)
{
	if (g_mdnie)
		g_mdnie->enable = FALSE;

}

void mdnie_s3cfb_resume(void)
{
	if (g_mdnie)
		g_mdnie->enable = TRUE;
}

static void get_lcd_size(unsigned int *xres, unsigned int *yres)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;

	cfg = readl(base_reg + VIDTCON2);
	*xres = ((cfg & VIDTCON2_HOZVAL_MASK) >> VIDTCON2_HOZVAL_SHIFT) + 1;
	*yres = ((cfg & VIDTCON2_LINEVAL_MASK) >> VIDTCON2_LINEVAL_SHIFT) + 1;
	*xres |= (cfg & VIDTCON2_HOZVAL_E_MASK) ? (1 << 11) : 0;	/* 11 is MSB */
	*yres |= (cfg & VIDTCON2_LINEVAL_E_MASK) ? (1 << 11) : 0;	/* 11 is MSB */
}

int s3c_mdnie_set_size(void)
{
	unsigned int cfg, xres, yres;

	get_lcd_size(&xres, &yres);

	/* Bank0 Select */
	s3c_mdnie_write(S3C_MDNIE_rR0, 0);

	/* Input Data Unmask */
	cfg = s3c_mdnie_read(S3C_MDNIE_rR1);
	cfg &= ~S3C_MDNIE_INPUT_DATA_ENABLE;
	cfg &= ~S3C_MDNIE_INPUT_HSYNC;
	s3c_mdnie_write(S3C_MDNIE_rR1, cfg);

	/* LCD width */
	cfg = s3c_mdnie_read(S3C_MDNIE_rR3);
	cfg &= S3C_MDNIE_SIZE_MASK;
	cfg |= S3C_MDNIE_HSIZE(xres);
	s3c_mdnie_write(S3C_MDNIE_rR3, xres);

	/* LCD height */
	cfg = s3c_mdnie_read(S3C_MDNIE_rR4);
	cfg &= S3C_MDNIE_SIZE_MASK;
	cfg |= S3C_MDNIE_VSIZE(xres);
	s3c_mdnie_write(S3C_MDNIE_rR4, yres);

	/* unmask all */
	mdnie_unmask();

	return 0;
}

static int mdnie_send_sequence(struct mdnie_info *mdnie, const unsigned short *seq)
{
	int ret = 0, i = 0;
	const unsigned short *wbuf = NULL;

	if (IS_ERR_OR_NULL(seq)) {
		dev_err(mdnie->dev, "mdnie sequence is null\n");
		return -EPERM;
	}

	if (IS_ERR_OR_NULL(s3c_mdnie_base)) {
		dev_err(mdnie->dev, "mdnie base is null\n");
		return -EPERM;
	}

	mutex_lock(&mdnie->dev_lock);

	wbuf = seq;

	mdnie_mask();

	while (wbuf[i] != END_SEQ) {
		ret += mdnie_write(wbuf[i], wbuf[i+1]);
		i += 2;
	}

	mdnie_unmask();

	mutex_unlock(&mdnie->dev_lock);

	return ret;
}

static struct mdnie_tuning_info *mdnie_request_table(struct mdnie_info *mdnie)
{
	struct mdnie_tuning_info *table = NULL;

	mutex_lock(&mdnie->lock);

	/* it will be removed next year */
	if (mdnie->negative == NEGATIVE_ON) {
		table = &negative_table[mdnie->cabc];
		goto exit;
	}

	if (ACCESSIBILITY_IS_VALID(mdnie->accessibility)) {
		table = &accessibility_table[mdnie->cabc][mdnie->accessibility];
		goto exit;
	} else if (SCENARIO_IS_DMB(mdnie->scenario)) {
#if defined(CONFIG_TDMB)
		table = &tune_dmb[mdnie->mode];
#endif
		goto exit;
	} else if (mdnie->scenario < SCENARIO_MAX) {
		table = &tuning_table[mdnie->cabc][mdnie->mode][mdnie->scenario];
		goto exit;
	}

exit:
	mutex_unlock(&mdnie->lock);

	return table;
}

static void mdnie_update_sequence(struct mdnie_info *mdnie, struct mdnie_tuning_info *table)
{
	unsigned short *wbuf = NULL;
	int ret;

	if (unlikely(mdnie->tuning)) {
		ret = mdnie_request_firmware(mdnie->path, &wbuf, table->name);
		if (ret < 0 && IS_ERR_OR_NULL(wbuf))
			goto exit;
		mdnie_send_sequence(mdnie, wbuf);
		kfree(wbuf);
	} else
		mdnie_send_sequence(mdnie, table->sequence);

exit:
	return;
}

void mdnie_update(struct mdnie_info *mdnie, u8 force)
{
	struct mdnie_tuning_info *table = NULL;

	if (!mdnie->enable && !force) {
		dev_err(mdnie->dev, "mdnie state is off\n");
		return;
	}

	table = mdnie_request_table(mdnie);
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->sequence)) {
		mdnie_update_sequence(mdnie, table);
		dev_info(mdnie->dev, "%s\n", table->name);
	}

	return;
}

#if !defined(CONFIG_S5P_MDNIE_PWM)
static void update_color_position(struct mdnie_info *mdnie, u16 idx)
{
	u8 cabc, mode, scenario, i;
	unsigned short *wbuf;

	dev_info(mdnie->dev, "%s: idx=%d\n", __func__, idx);

	mutex_lock(&mdnie->lock);

	for (cabc = 0; cabc < CABC_MAX; cabc++) {
		for (mode = 0; mode < MODE_MAX; mode++) {
			for (scenario = 0; scenario < SCENARIO_MAX; scenario++) {
				wbuf = tuning_table[cabc][mode][scenario].sequence;
				if (IS_ERR_OR_NULL(wbuf))
					continue;
				i = 0;
				while (wbuf[i] != END_SEQ) {
					if (ADDRESS_IS_SCR_WHITE(wbuf[i]))
						break;
					i += 2;
				}
				if ((wbuf[i] == END_SEQ) || IS_ERR_OR_NULL(&wbuf[i+5]))
					continue;
				if ((wbuf[i+1] == 0xff) && (wbuf[i+3] == 0xff) && (wbuf[i+5] == 0xff)) {
					wbuf[i+1] = tune_scr_setting[idx][0];
					wbuf[i+3] = tune_scr_setting[idx][1];
					wbuf[i+5] = tune_scr_setting[idx][2];
				}
			}
		}
	}

	mutex_unlock(&mdnie->lock);
}

static int get_panel_coordinate(struct mdnie_info *mdnie, int *result)
{
	int ret = 0;
	char *fp = NULL;
	unsigned int coordinate[2] = {0,};

	ret = mdnie_open_file(PANEL_COORDINATE_PATH, &fp);
	if (IS_ERR_OR_NULL(fp) || ret <= 0) {
		dev_info(mdnie->dev, "%s: open skip: %s, %d\n", __func__, PANEL_COORDINATE_PATH, ret);
		ret = -EINVAL;
		goto skip_color_correction;
	}

	ret = sscanf(fp, "%d, %d", &coordinate[0], &coordinate[1]);
	if (!(coordinate[0] + coordinate[1]) || ret != 2) {
		dev_info(mdnie->dev, "%s: %d, %d\n", __func__, coordinate[0], coordinate[1]);
		ret = -EINVAL;
		goto skip_color_correction;
	}

	ret = mdnie_calibration(coordinate[0], coordinate[1], result);
	dev_info(mdnie->dev, "%s: %d, %d, idx=%d\n", __func__, coordinate[0], coordinate[1], ret - 1);

skip_color_correction:
	mdnie->color_correction = 1;
	if (!IS_ERR_OR_NULL(fp))
		kfree(fp);

	return ret;
}
#endif

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->mode);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret, result[5] = {0,};

	ret = kstrtoul(buf, 0, (unsigned long *)&value);
	if (ret)
		return -EINVAL;

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= MODE_MAX) {
		value = STANDARD;
		return -EINVAL;
	}

	mutex_lock(&mdnie->lock);
	mdnie->mode = value;
	mutex_unlock(&mdnie->lock);

#if !defined(CONFIG_S5P_MDNIE_PWM)
	if (!mdnie->color_correction) {
		ret = get_panel_coordinate(mdnie, result);
		if (ret > 0)
			update_color_position(mdnie, ret - 1);
	}
#endif

	mdnie_update(mdnie, 0);

#if defined(CONFIG_S5P_MDNIE_PWM)
	if ((mdnie->support_pwm) && (mdnie->enable))
		backlight_update_status(mdnie->bd);
#endif

	return count;
}


static ssize_t scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->scenario);
}

static ssize_t scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (!SCENARIO_IS_VALID(value))
		value = UI_MODE;

#if defined(CONFIG_S5P_MDNIE_PWM)
	if (value >= SCENARIO_MAX)
		value = UI_MODE;
#endif

	mutex_lock(&mdnie->lock);
	mdnie->scenario = value;
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie, 0);
#if defined(CONFIG_S5P_MDNIE_PWM)
	if ((mdnie->support_pwm) && (mdnie->enable))
		backlight_update_status(mdnie->bd);
#endif

	return count;
}


#if defined(CONFIG_S5P_MDNIE_PWM)

static ssize_t cabc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->cabc);
}

static ssize_t cabc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= CABC_MAX)
		value = CABC_OFF;

	if (!mdnie->support_pwm)
		return count;

	mdnie->cabc = (value) ? CABC_ON : CABC_OFF;

	mdnie_update(mdnie, 0);

	if ((mdnie->support_pwm) && (mdnie->enable))
		backlight_update_status(mdnie->bd);

	return count;
}
#endif

static ssize_t tuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	struct mdnie_tuning_info *table;
	int ret, i;
	unsigned short *wbuf;

	pos += sprintf(pos, "++ %s: %s\n", __func__, mdnie->path);

	if (!mdnie->tuning) {
		pos += sprintf(pos, "tunning mode is off\n");
		goto exit;
	}

	if (strncmp(mdnie->path, MDNIE_SYSFS_PREFIX, sizeof(MDNIE_SYSFS_PREFIX) - 1)) {
		pos += sprintf(pos, "file path is invalid, %s\n", mdnie->path);
		goto exit;
	}

	table = mdnie_request_table(mdnie);
	if (!IS_ERR_OR_NULL(table)) {
		ret = mdnie_request_firmware(mdnie->path, &wbuf, table->name);
		i = 0;
		while (wbuf[i] != END_SEQ) {
			pos += sprintf(pos, "0x%04x, 0x%04x\n", wbuf[i], wbuf[i+1]);
			i += 2;
		}
		if (!IS_ERR_OR_NULL(wbuf))
			kfree(wbuf);
		pos += sprintf(pos, "%s\n", table->name);
	}

exit:
	pos += sprintf(pos, "-- %s\n", __func__);

	return pos - buf;
}

static ssize_t tuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int ret;

	if (sysfs_streq(buf, "0") || sysfs_streq(buf, "1")) {
		ret = kstrtoul(buf, 0, (unsigned long *)&mdnie->tuning);
		if (!mdnie->tuning)
			memset(mdnie->path, 0, sizeof(mdnie->path));
		dev_info(dev, "%s :: %s\n", __func__, mdnie->tuning ? "enable" : "disable");
	} else {
		if (!mdnie->tuning)
			return count;

		if (count > (sizeof(mdnie->path) - sizeof(MDNIE_SYSFS_PREFIX))) {
			dev_err(dev, "file name %s is too long\n", mdnie->path);
			return -ENOMEM;
		}

		memset(mdnie->path, 0, sizeof(mdnie->path));
		snprintf(mdnie->path, sizeof(MDNIE_SYSFS_PREFIX) + count-1, "%s%s", MDNIE_SYSFS_PREFIX, buf);
		dev_info(dev, "%s :: %s\n", __func__, mdnie->path);

		mdnie_update(mdnie, 0);
	}

	return count;
}

static ssize_t negative_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->negative);
}

static ssize_t negative_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (ret < 0)
		return ret;
	else {
		if (mdnie->negative == value)
			return count;

		if (value >= NEGATIVE_MAX)
			value = NEGATIVE_OFF;

		value = (value) ? NEGATIVE_ON : NEGATIVE_OFF;

		mutex_lock(&mdnie->lock);
		mdnie->negative = value;
		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie, 0);
	}
	return count;
}

static ssize_t accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	unsigned short *wbuf;
	int i = 0;

	pos += sprintf(pos, "%d, ", mdnie->accessibility);
	if (mdnie->accessibility == COLOR_BLIND) {
		if (!IS_ERR_OR_NULL(accessibility_table[mdnie->cabc][COLOR_BLIND].sequence)) {
			wbuf = accessibility_table[mdnie->cabc][COLOR_BLIND].sequence;
			while (wbuf[i] != END_SEQ) {
				if (ADDRESS_IS_SCR_RGB(wbuf[i]))
					pos += sprintf(pos, "0x%04x, ", wbuf[i+1]);
				i += 2;
			}
		}
	}
	pos += sprintf(pos, "\n");

	return pos - buf;
}

static ssize_t accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value, s[9], cabc, i, len = 0;
	int ret;
	unsigned short *wbuf;
	char str[100] = {0,};

	ret = sscanf(buf, "%d %x %x %x %x %x %x %x %x %x",
		&value, &s[0], &s[1], &s[2], &s[3],
		&s[4], &s[5], &s[6], &s[7], &s[8]);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (ret < 0)
		return ret;
	else {
		if (value >= ACCESSIBILITY_MAX)
			value = ACCESSIBILITY_OFF;

		mutex_lock(&mdnie->lock);
		mdnie->accessibility = value;
		if (value == COLOR_BLIND) {
			if (ret != 10) {
				mutex_unlock(&mdnie->lock);
				return -EINVAL;
			}

			for (cabc = 0; cabc < CABC_MAX; cabc++) {
				wbuf = accessibility_table[cabc][COLOR_BLIND].sequence;
				if (IS_ERR_OR_NULL(wbuf))
					continue;
				i = 0;
				while (wbuf[i] != END_SEQ) {
					if (ADDRESS_IS_SCR_RGB(wbuf[i]))
						wbuf[i+1] = s[SCR_RGB_MASK(wbuf[i])];
					i += 2;
				}
			}

			i = 0;
			len = sprintf(str + len, "%s :: ", __func__);
			while (len < sizeof(str) && i < ARRAY_SIZE(s)) {
				len += sprintf(str + len, "0x%04x, ", s[i]);
				i++;
			}
			dev_info(dev, "%s\n", str);
		}
		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie, 0);
	}

	return count;
}

#if !defined(CONFIG_S5P_MDNIE_PWM)
static ssize_t color_correct_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	int i, idx, result[5] = {0,};

	if (!mdnie->color_correction)
		return -EINVAL;

	idx = get_panel_coordinate(mdnie, result);

	for (i = 1; i < ARRAY_SIZE(result); i++)
		pos += sprintf(pos, "F%d= %d, ", i, result[i]);
	pos += sprintf(pos, "TUNE_%d\n", idx);

	return pos - buf;
}
#endif

static ssize_t bypass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->bypass);
}

static ssize_t bypass_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct mdnie_tuning_info *table = NULL;
	unsigned int value;
	int ret;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (ret < 0)
		return ret;
	else {
		if (value >= BYPASS_MAX)
			value = BYPASS_OFF;

		value = (value) ? BYPASS_ON : BYPASS_OFF;

		mutex_lock(&mdnie->lock);
		mdnie->bypass = value;
		mutex_unlock(&mdnie->lock);

		table = &bypass_table[value];
		if (!IS_ERR_OR_NULL(table)) {
			mdnie_update_sequence(mdnie, table);
			dev_info(mdnie->dev, "%s\n", table->name);
		}
	}

	return count;
}

static struct device_attribute mdnie_attributes[] = {
	__ATTR(mode, 0664, mode_show, mode_store),
	__ATTR(scenario, 0664, scenario_show, scenario_store),
#if defined(CONFIG_S5P_MDNIE_PWM)
	__ATTR(cabc, 0664, cabc_show, cabc_store),
#endif
	__ATTR(tuning, 0664, tuning_show, tuning_store),
	__ATTR(negative, 0664, negative_show, negative_store),
	__ATTR(accessibility, 0664, accessibility_show, accessibility_store),
#if !defined(CONFIG_S5P_MDNIE_PWM)
	__ATTR(color_correct, 0444, color_correct_show, NULL),
#endif
	__ATTR(bypass, 0664, bypass_show, bypass_store),
	__ATTR_NULL,
};

#if 0
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	struct fb_event *evdata = data;
	int fb_blank;

	/* If we aren't interested in this event, skip it immediately ... */
	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	mdnie = container_of(self, struct mdnie_info, fb_notif);
	if (!mdnie)
		return 0;

	fb_blank = *(int *)evdata->data;

	dev_info(mdnie->dev, "fb_blank: %d\n", fb_blank);

	if (fb_blank == FB_BLANK_UNBLANK) {
		pm_runtime_get_sync(mdnie->dev);

		mutex_lock(&mdnie->lock);
		mdnie->enable = TRUE;
		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie);
	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		mutex_lock(&mdnie->lock);
		mdnie->enable = FALSE;
		mutex_unlock(&mdnie->lock);

		pm_runtime_put_sync(mdnie->dev);
	}

	return 0;
}

static int mdnie_register_fb(struct mdnie_info *mdnie)
{
	memset(&mdnie->fb_notif, 0, sizeof(mdnie->fb_notif));
	mdnie->fb_notif.notifier_call = fb_notifier_callback;
	return fb_register_client(&mdnie->fb_notif);
}
#endif

#if defined (CONFIG_S5P_MDNIE_PWM)

static int mdnie_runtime_suspend(struct device *dev)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	clk_disable(mdnie->pwm_clk);
/*
	clk_disable(mdnie->bus_clk);
	clk_disable(mdnie->clk);
*/

	return 0;
}

static int mdnie_runtime_resume(struct device *dev)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	clk_enable(mdnie->pwm_clk);
/*
	clk_enable(mdnie->bus_clk);
	clk_enable(mdnie->clk);
*/
	return 0;
}

static int mdnie_suspend(struct device *dev)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	clk_disable(mdnie->pwm_clk);

	return 0;
}

static int mdnie_resume(struct device *dev)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	clk_enable(mdnie->pwm_clk);

	return 0;
}

static const struct dev_pm_ops mdnie_pm_ops = {
/*
	SET_SYSTEM_SLEEP_PM_OPS(mdnie_suspend, mdnie_resume)
	SET_RUNTIME_PM_OPS(mdnie_runtime_suspend, mdnie_runtime_resume, NULL)
*/
};
#endif

#if defined (CONFIG_S5P_MDNIE_PWM)

static int mdnie_get_br(struct backlight_device *bd)
{
	struct mdnie_info *mdnie = bl_get_data(bd);

	return bd->props.brightness;
}


static int mdnie_set_pwm(struct mdnie_info *mdnie, unsigned int br)
{
	int i;
	int ret;
	unsigned short value[MDNIE_PWM_REG_CNT];

	for (i = 0; i < MDNIE_PWM_REG_CNT; i += 2) {
		value[i] = mdnie_pwm[i];
		if (mdnie_pwm[i] == 0x00f9)
			value[i+1] = mdnie->br_table[br];
		else
			value[i+1] = mdnie_pwm[i+1];
	}

	ret = mdnie_send_sequence(mdnie, value);
	if (ret)
		dev_err(mdnie->dev, "failed to write mdnie reg value for pwm\n");

	return ret;
}


static int mdnie_set_cabc_pwm(struct mdnie_info *mdnie, unsigned int br)
{
	int i;
	int ret;
	unsigned short offset;
	unsigned short value[MDNIE_PWM_CABC_REG_CNT];

	for (i = 0; i < MDNIE_PWM_CABC_REG_CNT; i += 2) {
		value[i] = mdnie_pwm_cabc[i];
		if ((mdnie_pwm_cabc[i] >= 0x00bb) && (mdnie_pwm_cabc[i] <= 0x00c3)) {
			offset = mdnie_pwm_cabc[i] - 0x00bb;
			value[i+1] = ((dft_cabc_plut[offset] * mdnie->br_table[br]) / 0x7ff);
		} else {
			value[i+1] = mdnie_pwm_cabc[i+1];
		}
	}
	ret = mdnie_send_sequence(mdnie, value);
	if (ret)
		dev_err(mdnie->dev, "failed to write mdnie reg value for pwm\n");

	return ret;
}



static int mdnie_set_br(struct backlight_device *bd)
{
	int i;
	int ret = 0;
	unsigned int br;
	struct mdnie_info *mdnie;

	mdnie = bl_get_data(bd);
	br = bd->props.brightness;

	if (!mdnie->enable) {
		dev_info(mdnie->dev, "suspended br : %d\n", br);
		return 0;
	}
	if (bd->props.max_brightness < br) {
		dev_err(mdnie->dev, "ERR : invalid brightness\n");
		return -EINVAL;
	}

	if ((mdnie->cabc) && (mdnie->scenario != CAMERA_MODE))
		ret = mdnie_set_cabc_pwm(mdnie, br);
	else
		ret = mdnie_set_pwm(mdnie, br);

	return ret;
}

static const struct backlight_ops mdnie_backlight_ops = {
	.get_brightness = mdnie_get_br,
	.update_status = mdnie_set_br,
};

#endif


static int mdnie_probe(struct platform_device *pdev)
{
    int ret = 0;
	struct mdnie_info *mdnie;
#if defined (CONFIG_S5P_MDNIE_PWM)
	struct backlight_properties props;
	struct platform_mdnie_data *mdnie_data;
#endif

	mdnie_class = class_create(THIS_MODULE, dev_name(&pdev->dev));
	if (IS_ERR_OR_NULL(mdnie_class)) {
		pr_err("failed to create mdnie class\n");
		ret = -EINVAL;
		goto error0;
	}

	mdnie_class->dev_attrs = mdnie_attributes;

	mdnie = kzalloc(sizeof(struct mdnie_info), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie\n");
		ret = -ENOMEM;
		goto error1;
	}

	mdnie->dev = device_create(mdnie_class, &pdev->dev, 0, &mdnie, "mdnie");
	if (IS_ERR_OR_NULL(mdnie->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto error2;
	}

	mdnie->scenario = UI_MODE;
	mdnie->mode = STANDARD;
	mdnie->enable = FALSE;
	mdnie->tuning = FALSE;
	mdnie->negative = NEGATIVE_OFF;
	mdnie->accessibility = ACCESSIBILITY_OFF;
	mdnie->cabc = CABC_OFF;
	mdnie->bypass = BYPASS_OFF;

	mutex_init(&mdnie->lock);
	mutex_init(&mdnie->dev_lock);

#if defined (CONFIG_S5P_MDNIE_PWM)
	mdnie_data = pdev->dev.platform_data;

	if (mdnie_data->support_pwm) {
		mdnie->support_pwm = mdnie_data->support_pwm;
		mdnie->br_table = mdnie_data->br_table;
request_gpio:
		ret = gpio_request(mdnie_data->pwm_out_no, "mdnie_pwm");
		if (ret) {
			dev_info(mdnie->dev, "failed to request gpio for mdniw_pwm\n");
			gpio_free(mdnie_data->pwm_out_no);
			goto request_gpio;
		}
		s3c_gpio_cfgpin(mdnie_data->pwm_out_no, S3C_GPIO_SFN(mdnie_data->pwm_out_func));

		mdnie->pwm_clk = clk_get(mdnie->dev, "sclk_mdnie_pwm");
		if (IS_ERR(mdnie->pwm_clk)) {
			dev_info(mdnie->dev, "failed to get clk for mdnie_pwm\n");
			return PTR_ERR(mdnie->pwm_clk);
		}
		clk_enable(mdnie->pwm_clk);

		memset(&props, 0, sizeof(struct backlight_properties));
		props.max_brightness = 255;
		props.type = BACKLIGHT_RAW;
		mdnie->bd = backlight_device_register("panel", mdnie->dev, mdnie,
			&mdnie_backlight_ops, &props);
		if (IS_ERR(mdnie->bd)) {
			dev_err(mdnie->dev, "failed to register mdnie backlight driver\n");
			goto error2;
		}
		mdnie->bd->props.brightness = mdnie_data->dft_bl;
	}
#endif
	platform_set_drvdata(pdev, mdnie);
	dev_set_drvdata(mdnie->dev, mdnie);

#if 0
	pm_runtime_enable(mdnie->dev);
	pm_runtime_get_sync(mdnie->dev);

	mdnie_register_fb(mdnie);

	mdnie_update(mdnie);
#endif

	g_mdnie = mdnie;

	dev_info(mdnie->dev, "registered successfully\n");

	return 0;

error2:
	kfree(mdnie);
error1:
	class_destroy(mdnie_class);
error0:
	return ret;
}

static int mdnie_remove(struct platform_device *pdev)
{
	struct mdnie_info *mdnie = dev_get_drvdata(&pdev->dev);

	class_destroy(mdnie_class);
	kfree(mdnie);

#if 0
	pm_runtime_disable(mdnie->dev);
#endif

	return 0;
}

static struct platform_driver mdnie_driver = {
	.driver		= {
		.name	= "mdnie",
		.owner	= THIS_MODULE,
#if defined (CONFIG_S5P_MDNIE_PWM)
		.pm	= &mdnie_pm_ops,
#endif
	},
	.probe		= mdnie_probe,
	.remove		= mdnie_remove,
};

static int __init mdnie_init(void)
{
	return platform_driver_register(&mdnie_driver);
}
late_initcall(mdnie_init);

static void __exit mdnie_exit(void)
{
	platform_driver_unregister(&mdnie_driver);
}
module_exit(mdnie_exit);

MODULE_DESCRIPTION("mDNIe Driver");
MODULE_LICENSE("GPL");
