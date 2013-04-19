/*
 * LP5562 LED driver
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/leds-lp55xx.h>
#include <linux/slab.h>
#include <linux/err.h>

#include "leds-lp55xx-common.h"

#define LP5562_PROGRAM_LENGTH		32
#define LP5562_MAX_LEDS			4

/* Registers */
#define LP5562_REG_ENABLE		0x00
#define LP5562_REG_OP_MODE		0x01
#define LP5562_REG_R_PWM		0x04
#define LP5562_REG_G_PWM		0x03
#define LP5562_REG_B_PWM		0x02
#define LP5562_REG_W_PWM		0x0E
#define LP5562_REG_R_CURRENT		0x07
#define LP5562_REG_G_CURRENT		0x06
#define LP5562_REG_B_CURRENT		0x05
#define LP5562_REG_W_CURRENT		0x0F
#define LP5562_REG_CONFIG		0x08
#define LP5562_REG_RESET		0x0D
#define LP5562_REG_PROG_MEM_ENG1	0x10
#define LP5562_REG_PROG_MEM_ENG2	0x30
#define LP5562_REG_PROG_MEM_ENG3	0x50
#define LP5562_REG_ENG_SEL		0x70

/* Bits in ENABLE register */
#define LP5562_MASTER_ENABLE		0x40	/* Chip master enable */
#define LP5562_LOGARITHMIC_PWM		0x80	/* Logarithmic PWM adjustment */
#define LP5562_EXEC_RUN			0x2A
#define LP5562_ENABLE_DEFAULT	\
	(LP5562_MASTER_ENABLE | LP5562_LOGARITHMIC_PWM)
#define LP5562_ENABLE_RUN_PROGRAM	\
	(LP5562_ENABLE_DEFAULT | LP5562_EXEC_RUN)

/* Reset register value */
#define LP5562_RESET			0xFF

/* CONFIG register */
#define LP5562_DEFAULT_CFG	\
	(LP5562_PWM_HF | LP5562_PWRSAVE_EN | LP5562_CLK_INT)

/* Program Commands */
#define CMD_SET_PWM			0x40
#define CMD_WAIT_LSB			0x00
#define LP5562_CMD_DISABLE		0x00
#define LP5562_CMD_LOAD			0x15
#define LP5562_CMD_RUN			0x2A
#define LP5562_CMD_DIRECT		0x3F

#define MAX_BLINK_TIME			60000	/* 60 sec */
#define LP5562_ENG_SEL_PWM		0
#define LP5562_ENG_SEL_RGB		0x1B	/* R:ENG1, G:ENG2, B:ENG3 */
#define LP5562_PATTERN_OFF		0

#define SEC_LED_SPECIFIC
/*#define LED_DEEP_DEBUG*/

#ifdef SEC_LED_SPECIFIC
extern struct class *sec_class;
static struct device *led_dev;
/*path : /sys/class/sec/led/led_pattern*/
/*path : /sys/class/sec/led/led_blink*/
/*path : /sys/class/sec/led/led_brightness*/
/*path : /sys/class/leds/led_r/brightness*/
/*path : /sys/class/leds/led_g/brightness*/
/*path : /sys/class/leds/led_b/brightness*/
#endif

struct lp55xx_chip *g_chip;

static u8 LED_DYNAMIC_CURRENT = 0x28;
static u8 LED_LOWPOWER_MODE = 0x0;

enum lp5562_wait_type {
	LP5562_CYCLE_INVALID,
	LP5562_CYCLE_50ms,
	LP5562_CYCLE_100ms,
	LP5562_CYCLE_200ms,
	LP5562_CYCLE_500ms,
	LP5562_CYCLE_700ms,
	LP5562_CYCLE_920ms,
	LP5562_CYCLE_982ms,
	LP5562_CYCLE_MAX,
};

struct lp5562_wait_param {
	unsigned cycle;
	unsigned limit;
	u8 cmd;
};

struct lp5562_pattern_data {
	u8 r[LP5562_PROGRAM_LENGTH];
	u8 g[LP5562_PROGRAM_LENGTH];
	u8 b[LP5562_PROGRAM_LENGTH];
	unsigned pc_r;
	unsigned pc_g;
	unsigned pc_b;
};

static const struct lp5562_wait_param lp5562_wait_cycle[LP5562_CYCLE_MAX] = {
	[LP5562_CYCLE_50ms] = {
		.cycle = 50,
		.limit = 3000,
		.cmd   = 0x43,
	},
	[LP5562_CYCLE_100ms] = {
		.cycle = 100,
		.limit = 6000,
		.cmd   = 0x46,
	},
	[LP5562_CYCLE_200ms] = {
		.cycle = 200,
		.limit = 10000,
		.cmd   = 0x4d,
	},
	[LP5562_CYCLE_500ms] = {
		.cycle = 500,
		.limit = 30000,
		.cmd   = 0x60,
	},
	[LP5562_CYCLE_700ms] = {
		.cycle = 700,
		.limit = 40000,
		.cmd   = 0x6d,
	},
	[LP5562_CYCLE_920ms] = {
		.cycle = 920,
		.limit = 50000,
		.cmd   = 0x7b,
	},
	[LP5562_CYCLE_982ms] = {
		.cycle = 982,
		.limit = 60000,
		.cmd   = 0x7f,
	},
};

static inline void lp5562_wait_opmode_done(void)
{
	/* operation mode change needs to be longer than 153 us */
	usleep_range(200, 300);
}

static inline void lp5562_wait_enable_done(void)
{
	/* it takes more 488 us to update ENABLE register */
	usleep_range(500, 600);
}

static void lp5562_set_led_current(struct lp55xx_led *led, u8 led_current)
{
	u8 addr[] = {
		LP5562_REG_R_CURRENT,
		LP5562_REG_G_CURRENT,
		LP5562_REG_B_CURRENT,
		LP5562_REG_W_CURRENT,
	};

	led->led_current = led_current;
	lp55xx_write(led->chip, addr[led->chan_nr], led_current);
}

static int lp5562_post_init_device(struct lp55xx_chip *chip)
{
	int ret;
	u8 update_cfg = chip->pdata->update_config ? : LP5562_DEFAULT_CFG;

	/* Set all PWMs to direct control mode */
	ret = lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
	if (ret)
		return ret;

	lp5562_wait_opmode_done();

	ret = lp55xx_write(chip, LP5562_REG_CONFIG, update_cfg);
	if (ret)
		return ret;

	/* Initialize all channels PWM to zero -> leds off */
	lp55xx_write(chip, LP5562_REG_R_PWM, 0);
	lp55xx_write(chip, LP5562_REG_G_PWM, 0);
	lp55xx_write(chip, LP5562_REG_B_PWM, 0);
	lp55xx_write(chip, LP5562_REG_W_PWM, 0);

	/* Set engines are set to run state when OP_MODE enables engines */
	ret = lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_RUN_PROGRAM);
	if (ret)
		return ret;

	lp5562_wait_enable_done();

	/* Set LED map as register PWM by default */
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);

	return 0;
}

static void lp5562_led_brightness_work(struct work_struct *work)
{
	struct lp55xx_led *led = container_of(work, struct lp55xx_led,
					      brightness_work);
	struct lp55xx_chip *chip = led->chip;
	u8 addr[] = {
		LP5562_REG_R_PWM,
		LP5562_REG_G_PWM,
		LP5562_REG_B_PWM,
		LP5562_REG_W_PWM,
	};

	mutex_lock(&chip->lock);
	lp55xx_write(chip, addr[led->chan_nr], led->brightness);
	mutex_unlock(&chip->lock);
}

static void lp5562_stop_engine(struct lp55xx_chip *chip)
{
	lp55xx_write(chip, LP5562_REG_OP_MODE, 0);
	lp5562_wait_opmode_done();
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);
}

static enum lp5562_wait_type lp5562_lookup_cycle(unsigned int ms)
{
	int i;

	for (i = LP5562_CYCLE_50ms; i < LP5562_CYCLE_MAX; i++) {
		if (ms > lp5562_wait_cycle[i-1].limit &&
		    ms <= lp5562_wait_cycle[i].limit)
			return i;
	}

	return LP5562_CYCLE_INVALID;
}

static void lp5562_set_wait_cmd(struct lp5562_pattern_data *ptn,
				unsigned int ms, u8 jump)
{
	enum lp5562_wait_type type = lp5562_lookup_cycle(ms);
	unsigned int loop = ms / lp5562_wait_cycle[type].cycle;
	u8 cmd_msb = lp5562_wait_cycle[type].cmd;
	u8 msb;
	u8 lsb;
	u16 branch;

	WARN_ON(!cmd_msb);
	WARN_ON(loop > 64);

	/* wait command */
	ptn->r[ptn->pc_r++] = cmd_msb;
	ptn->r[ptn->pc_r++] = CMD_WAIT_LSB;
	ptn->g[ptn->pc_g++] = cmd_msb;
	ptn->g[ptn->pc_g++] = CMD_WAIT_LSB;
	ptn->b[ptn->pc_b++] = cmd_msb;
	ptn->b[ptn->pc_b++] = CMD_WAIT_LSB;

	/* branch command : if wait time is bigger than cycle msec,
			branch is used for command looping */
	if (loop > 1) {
		branch = (5 << 13) | ((loop - 1) << 7) | jump;
		msb = (branch >> 8) & 0xFF;
		lsb = branch & 0xFF;

		ptn->r[ptn->pc_r++] = msb;
		ptn->r[ptn->pc_r++] = lsb;
		ptn->g[ptn->pc_g++] = msb;
		ptn->g[ptn->pc_g++] = lsb;
		ptn->b[ptn->pc_b++] = msb;
		ptn->b[ptn->pc_b++] = lsb;
	}
}

static void lp5562_set_pwm_cmd(struct lp5562_pattern_data *ptn,
			unsigned int color)
{
	u8 r = (color >> 16) & 0xFF;
	u8 g = (color >> 8) & 0xFF;
	u8 b = color & 0xFF;

	ptn->r[ptn->pc_r++] = CMD_SET_PWM;
	ptn->r[ptn->pc_r++] = r;
	ptn->g[ptn->pc_g++] = CMD_SET_PWM;
	ptn->g[ptn->pc_g++] = g;
	ptn->b[ptn->pc_b++] = CMD_SET_PWM;
	ptn->b[ptn->pc_b++] = b;
}

static void lp5562_clear_program_memory(struct lp55xx_chip *chip)
{
	int i;
	u8 rgb_mem[] = {
		LP5562_REG_PROG_MEM_ENG1,
		LP5562_REG_PROG_MEM_ENG2,
		LP5562_REG_PROG_MEM_ENG3,
	};

	for (i = 0; i < ARRAY_SIZE(rgb_mem); i++) {
		lp55xx_write(chip, rgb_mem[i], 0);
		lp55xx_write(chip, rgb_mem[i] + 1, 0);
	}
}

static void lp5562_write_program_memory(struct lp55xx_chip *chip,
					u8 base, u8 *rgb, int size)
{
	int i;

	if (!rgb || size <= 0)
		return;

	for (i = 0; i < size; i++)
		lp55xx_write(chip, base + i, *(rgb + i));

	lp55xx_write(chip, base + i, 0);
	lp55xx_write(chip, base + i + 1, 0);
}

static inline bool _is_pc_overflow(struct lp5562_pattern_data *ptn)
{
	return (ptn->pc_r >= LP5562_PROGRAM_LENGTH ||
		ptn->pc_g >= LP5562_PROGRAM_LENGTH ||
		ptn->pc_b >= LP5562_PROGRAM_LENGTH);
}

static void lp5562_run_led_pattern(struct lp55xx_chip *chip,
				struct lp5562_pattern_data *ptn)
{
	WARN_ON(_is_pc_overflow(ptn));

	/* Set LED map as RGB */
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_RGB);

	/* OP mode : LOAD */
	lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_LOAD);
	lp5562_wait_opmode_done();

	/* copy pattern commands into the program memory */
	lp5562_clear_program_memory(chip);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG1,
				ptn->r, ptn->pc_r);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG2,
				ptn->g, ptn->pc_g);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG3,
				ptn->b, ptn->pc_b);

	/* OP mode : RUN */
	lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_RUN);
	lp5562_wait_opmode_done();
	lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_RUN_PROGRAM);
}

static ssize_t lp5562_show_blink(struct device *dev,
				struct device_attribute *attr,
				char *unused)
{
	return 0;
}

/* LED blink with the internal program engine */
static ssize_t lp5562_store_blink(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	unsigned int rgb = 0;
	unsigned int on = 0;
	unsigned int off = 0;
	struct lp5562_pattern_data ptn = { };
	u8 jump_pc = 0;

	sscanf(buf, "0x%06x %d %d", &rgb, &on, &off);

	lp5562_stop_engine(chip);

	on = min_t(unsigned int, on, MAX_BLINK_TIME);
	off = min_t(unsigned int, off, MAX_BLINK_TIME);

	if (!rgb || !on || !off)
		return len;

	mutex_lock(&chip->lock);

	/* make on-time pattern */
	lp5562_set_pwm_cmd(&ptn, rgb);
	lp5562_set_wait_cmd(&ptn, on, jump_pc);
	jump_pc = ptn.pc_r / 2; /* 16bit size program counter */

	/* make off-time pattern */
	lp5562_set_pwm_cmd(&ptn, 0);
	lp5562_set_wait_cmd(&ptn, off, jump_pc);

	/* run the pattern */
	lp5562_run_led_pattern(chip, &ptn);

	printk(KERN_DEBUG "led_blink is called, Color:0x%X Brightness:%i\n",
			rgb, LED_DYNAMIC_CURRENT);

	mutex_unlock(&chip->lock);

	return len;
}

void lp5562_blink(int rgb, int on, int off)
{
	struct lp55xx_chip *chip = g_chip;
	struct lp5562_pattern_data ptn = { };
	u8 jump_pc = 0;

	lp5562_stop_engine(chip);

	on = min_t(unsigned int, on, MAX_BLINK_TIME);
	off = min_t(unsigned int, off, MAX_BLINK_TIME);

	if (!rgb || !on || !off)
		return;

	mutex_lock(&chip->lock);

	/* make on-time pattern */
	lp5562_set_pwm_cmd(&ptn, rgb);
	lp5562_set_wait_cmd(&ptn, on, jump_pc);
	jump_pc = ptn.pc_r / 2; /* 16bit size program counter */

	/* make off-time pattern */
	lp5562_set_pwm_cmd(&ptn, 0);
	lp5562_set_wait_cmd(&ptn, off, jump_pc);

	/* run the pattern */
	lp5562_run_led_pattern(chip, &ptn);

	printk(KERN_DEBUG "led_blink is called, Color:0x%X Brightness:%i\n",
			rgb, LED_DYNAMIC_CURRENT);

	mutex_unlock(&chip->lock);
}
EXPORT_SYMBOL(lp5562_blink);

static int lp5562_lookup_pattern(struct lp55xx_chip *chip, u8 offset,
				struct lp5562_pattern_data *ptn)
{
	struct lp55xx_predef_pattern *predef;
	
	if(offset < 1 || offset > chip->pdata->num_patterns)
		return -EINVAL;

	predef = chip->pdata->patterns + (offset - 1);

	/* copy predefined pattern to internal data */
	memcpy(ptn->r, predef->r, predef->size_r);
	memcpy(ptn->g, predef->g, predef->size_g);
	memcpy(ptn->b, predef->b, predef->size_b);
	ptn->pc_r = predef->size_r;
	ptn->pc_g = predef->size_g;
	ptn->pc_b = predef->size_b;

	return 0;
}

static int lp5562_run_predef_led_pattern(struct lp55xx_chip *chip, int mode)
{
	int num_patterns = chip->pdata->num_patterns;
	int ret = -EINVAL;

	/* invalid pattern data */
	if (mode > num_patterns || !(chip->pdata->patterns))
		return ret;

	if (mode == LP5562_PATTERN_OFF) {
		lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_DEFAULT);
		lp5562_wait_enable_done();
		lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DISABLE);
		lp5562_wait_opmode_done();
		lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);
		lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
		lp5562_wait_opmode_done();
	} else {
		struct lp5562_pattern_data ptn = { };

		ret = lp5562_lookup_pattern(chip, mode, &ptn);
		if (ret)
			return ret;

		if (LED_LOWPOWER_MODE == 1)
			LED_DYNAMIC_CURRENT = 0x5;
		else
			LED_DYNAMIC_CURRENT = 0x28;

		chip->pdata->led_config[0].led_current = LED_DYNAMIC_CURRENT;
		chip->pdata->led_config[1].led_current = LED_DYNAMIC_CURRENT;
		chip->pdata->led_config[2].led_current = LED_DYNAMIC_CURRENT;

		lp55xx_write(chip, LP5562_REG_R_CURRENT, LED_DYNAMIC_CURRENT);
		lp55xx_write(chip, LP5562_REG_G_CURRENT, LED_DYNAMIC_CURRENT);
		lp55xx_write(chip, LP5562_REG_B_CURRENT, LED_DYNAMIC_CURRENT);

		lp5562_run_led_pattern(chip, &ptn);
	}

	return 0;
}

static ssize_t lp5562_show_pattern(struct device *dev,
				struct device_attribute *attr,
				char *unused)
{
	return 0;
}

static ssize_t lp5562_store_pattern(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&chip->lock);
	lp5562_run_predef_led_pattern(chip, val);
	printk(KERN_DEBUG "led pattern : %lu is activated(Type:%d)\n",
				val, LED_LOWPOWER_MODE);
	mutex_unlock(&chip->lock);

	return len;
}

void lp5562_run_pattern(int val)
{
	if(!g_chip)
		return;
	
	mutex_lock(&g_chip->lock);
	lp5562_run_predef_led_pattern(g_chip, val);
	printk(KERN_DEBUG "led pattern : %d is activated(Type:%d)\n",
				val, LED_LOWPOWER_MODE);
	mutex_unlock(&g_chip->lock);
}
EXPORT_SYMBOL(lp5562_run_pattern);

static DEVICE_ATTR(led_blink, 0664,
		lp5562_show_blink, lp5562_store_blink);
static DEVICE_ATTR(led_pattern, 0664,
		lp5562_show_pattern, lp5562_store_pattern);

#ifdef SEC_LED_SPECIFIC

static ssize_t store_led_r(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char buff[10] = {0,};
	int cnt, ret;
	u8 brightness;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou8(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(&chip->cl->dev, "fail to get brightness.\n");
		goto out;
	}

	brightness = min((u8)40,brightness);
	brightness = brightness * 255 / 40;

	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5562_REG_R_CURRENT, 0x78);
	lp55xx_write(chip, LP5562_REG_R_PWM, brightness);
	mutex_unlock(&chip->lock);

out:
	return count;
}

static ssize_t store_led_g(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char buff[10] = {0,};
	int cnt, ret;
	u8 brightness;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou8(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(&chip->cl->dev, "fail to get brightness.\n");
		goto out;
	}

	brightness = min((u8)40,brightness);
	brightness = brightness * 255 / 40;

	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5562_REG_G_CURRENT, 0x78);
	lp55xx_write(chip, LP5562_REG_G_PWM, brightness);
	mutex_unlock(&chip->lock);
out:
	return count;
}

static ssize_t store_led_b(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char buff[10] = {0,};
	int cnt, ret;
	u8 brightness;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou8(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(&chip->cl->dev, "fail to get brightness.\n");
		goto out;
	}

	brightness = min((u8)40,brightness);
	brightness = brightness * 255 / 40;

	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5562_REG_B_CURRENT, 0x78);
	lp55xx_write(chip, LP5562_REG_B_PWM, brightness);
	mutex_unlock(&chip->lock);

out:
	return count;

}

static ssize_t lp5562_store_led_lowpower(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	u8 led_lowpower;
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;

	retval = kstrtou8(buf, 0, &led_lowpower);
	if (retval != 0) {
		dev_err(&chip->cl->dev, "fail to get led_lowpower.\n");
		return count;
	}

	LED_LOWPOWER_MODE = led_lowpower;

	printk(KERN_DEBUG "led_lowpower mode set to %i\n", led_lowpower);

	return count;
}

static ssize_t lp5562_store_led_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	u8 brightness;
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;

	retval = kstrtou8(buf, 0, &brightness);
	if (retval != 0) {
		dev_err(&chip->cl->dev, "fail to get led_brightness.\n");
		return count;
	}
/*
	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;
*/
	LED_DYNAMIC_CURRENT = brightness;

	printk(KERN_DEBUG "led brightness set to %i\n", brightness);

	return count;
}

static ssize_t lp5562_store_led_br_lev(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	unsigned long brightness_lev;
	struct i2c_client *client;
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	/*client = b_client;*/

	retval = kstrtoul(buf, 16, &brightness_lev);
	if (retval != 0) {
		dev_err(&chip->cl->dev, "fail to get led_br_lev.\n");
		return count;
	}

	/*leds_set_imax(client, brightness_lev);*/

	return count;
}

#endif

#ifdef SEC_LED_SPECIFIC
/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(led_r, 0664, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0664, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0664, NULL, store_led_b);
/* led_pattern node permission is 664 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_br_lev, 0664, NULL, \
					lp5562_store_led_br_lev);
static DEVICE_ATTR(led_brightness, 0664, NULL, \
					lp5562_store_led_brightness);
static DEVICE_ATTR(led_lowpower, 0664, NULL, \
					lp5562_store_led_lowpower);
#endif

static struct attribute *lp5562_attributes[] = {
#ifndef SEC_LED_SPECIFIC
asdadsasdasd
	&dev_attr_led_blink.attr,
	&dev_attr_led_pattern.attr,
#endif
	NULL,
};

static const struct attribute_group lp5562_group = {
	.attrs = lp5562_attributes,
};

#ifdef SEC_LED_SPECIFIC
static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_br_lev.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif

/* Chip specific configurations */
static struct lp55xx_device_config lp5562_cfg = {
	.max_channel  = LP5562_MAX_LEDS,
	.reset = {
		.addr = LP5562_REG_RESET,
		.val  = LP5562_RESET,
	},
	.enable = {
		.addr = LP5562_REG_ENABLE,
		.val  = LP5562_ENABLE_DEFAULT,
	},
	.post_init_device   = lp5562_post_init_device,
	.brightness_work_fn = lp5562_led_brightness_work,
	.set_led_current    = lp5562_set_led_current,
	.dev_attr_group     = &lp5562_group,
};

static int __devinit lp5562_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct lp55xx_chip *chip;
	struct lp55xx_led *led;
	struct lp55xx_platform_data *pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	led = devm_kzalloc(&client->dev,
			sizeof(*led) * pdata->num_channels, GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	chip->cl = client;
	chip->pdata = pdata;
	chip->cfg = &lp5562_cfg;

	mutex_init(&chip->lock);

	i2c_set_clientdata(client, led);

	ret = lp55xx_init_device(chip);
	if (ret)
		goto err_init;

	dev_info(&client->dev, "%s programmable led chip found\n", id->name);

	ret = lp55xx_register_leds(led, chip);
	if (ret)
		goto err_register_leds;

	ret = lp55xx_register_sysfs(chip);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto err_register_sysfs;
	}

#ifdef SEC_LED_SPECIFIC
	led_dev = device_create(sec_class, NULL, 0, led, "led");
	if (IS_ERR(led_dev)) {
		dev_err(&client->dev,
			"Failed to create device for samsung specific led\n");
		ret = -ENODEV;
		goto err_register_sysfs;
	}
	ret = sysfs_create_group(&led_dev->kobj, &sec_led_attr_group);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs group for samsung specific led\n");
		goto err_register_sysfs;
	}
#endif

	g_chip = chip;

	return 0;

err_register_sysfs:
	lp55xx_unregister_leds(led, chip);
err_register_leds:
	lp55xx_deinit_device(chip);
err_init:
	return ret;
}

static int __devexit lp5562_remove(struct i2c_client *client)
{
	struct lp55xx_led *led = i2c_get_clientdata(client);
	struct lp55xx_chip *chip = led->chip;
#ifdef SEC_LED_SPECIFIC
	sysfs_remove_group(&led_dev->kobj, &sec_led_attr_group);
#endif
	lp5562_stop_engine(chip);

	lp55xx_unregister_sysfs(chip);
	lp55xx_unregister_leds(led, chip);
	lp55xx_deinit_device(chip);

	return 0;
}

static const struct i2c_device_id lp5562_id[] = {
	{ "lp5562", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp5562_id);

static struct i2c_driver lp5562_driver = {
	.driver = {
		.name	= "lp5562",
	},
	.probe		= lp5562_probe,
	.remove		= __devexit_p(lp5562_remove),
	.id_table	= lp5562_id,
};

module_i2c_driver(lp5562_driver);

MODULE_DESCRIPTION("Texas Instruments LP5562 LED Driver");
MODULE_VERSION("A1");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
