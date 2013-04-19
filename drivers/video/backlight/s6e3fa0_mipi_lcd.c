/* linux/drivers/video/backlight/s6e3fa0_mipi_lcd.c
 *
 * Samsung SoC MIPI COMMAND MODE LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <linux/syscalls.h> /* sys_sync */

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>
#include <asm/system_info.h>

#include "s6e3fa0_param.h"

#undef SMART_DIMMING
#undef SMART_DIMMING_DEBUG

#ifdef SMART_DIMMING
#include "smart_dimming_s6e3fa0.h"
#include "aid_s6e3fa0.h"
#endif

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		160

#define POWER_IS_ON(pwr)		((pwr) <= FB_BLANK_NORMAL)

#define MAX_GAMMA			300
#define DEFAULT_GAMMA_LEVEL		GAMMA_160CD
#define DEFAULT_ELVSS_LEVEL		0x04

#define LDI_ID_REG			0x04
#define LDI_ID1_REG			0xDA
#define LDI_ID2_REG			0xDB
#define LDI_ID3_REG			0xDC
#define LDI_ID_LEN			3

#ifdef SMART_DIMMING
#define LDI_MTP_LENGTH		33
#define LDI_MTP_ADDR			0xC8
#endif

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct lcd_platform_data	*lcd_pd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table;
	unsigned int			elvss_ref;
#ifdef SMART_DIMMING
	struct str_smart_dim		smart;
	struct aid_info			*aid;
	unsigned char			aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
#endif
	unsigned int			irq;
	unsigned int			gpio;
	unsigned int			connected;

	struct mipi_dsim_device		*dsim;
};

static const unsigned int candela_table[GAMMA_MAX] = {
	10, 20,  30,  40,  50,  60,  70,  80,  90, 100,
	110, 120, 130, 140, 150, 160, 170, 180,
	190, 200, 210, 220, 230, 240, 250,
	260, 270, 280, 290, MAX_GAMMA-1
};

static struct lcd_info *g_lcd;
static void s6e3fa0_read_id(struct lcd_info *lcd, u8 *buf);

static int s6e3fa0_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret;
	int retry;
	u8 cmd;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	if (len > 2)
		cmd = MIPI_DSI_DCS_LONG_WRITE;
	else if (len == 2)
		cmd = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_DCS_SHORT_WRITE;
	else {
		ret = -EINVAL;
		goto write_err;
	}

	retry = 5;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed - exceed retry count\n", __func__);
		goto write_err;
	}
	ret = s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq, len);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}


static int s6e3fa0_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}
	retry = 5;
read_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed - exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 1);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0 ... 19:
		backlightlevel = GAMMA_10CD;
		break;
	case 20 ... 29:
		backlightlevel = GAMMA_20CD;
		break;
	case 30 ... 39:
		backlightlevel = GAMMA_30CD;
		break;
	case 40 ... 49:
		backlightlevel = GAMMA_40CD;
		break;
	case 50 ... 59:
		backlightlevel = GAMMA_50CD;
		break;
	case 60 ... 69:
		backlightlevel = GAMMA_60CD;
		break;
	case 70 ... 79:
		backlightlevel = GAMMA_70CD;
		break;
	case 80 ... 89:
		backlightlevel = GAMMA_80CD;
		break;
	case 90 ... 99:
		backlightlevel = GAMMA_90CD;
		break;
	case 100 ... 109:
		backlightlevel = GAMMA_100CD;
		break;
	case 110 ... 119:
		backlightlevel = GAMMA_110CD;
		break;
	case 120 ... 129:
		backlightlevel = GAMMA_120CD;
		break;
	case 130 ... 139:
		backlightlevel = GAMMA_130CD;
		break;
	case 140 ... 149:
		backlightlevel = GAMMA_140CD;
		break;
	case 150 ... 159:
		backlightlevel = GAMMA_150CD;
		break;
	case 160 ... 169:
		backlightlevel = GAMMA_160CD;
		break;
	case 170 ... 179:
		backlightlevel = GAMMA_170CD;
		break;
	case 180 ... 189:
		backlightlevel = GAMMA_180CD;
		break;
	case 190 ... 199:
		backlightlevel = GAMMA_190CD;
		break;
	case 200 ... 209:
		backlightlevel = GAMMA_200CD;
		break;
	case 210 ... 219:
		backlightlevel = GAMMA_210CD;
		break;
	case 220 ... 229:
		backlightlevel = GAMMA_220CD;
		break;
	case 230 ... 239:
		backlightlevel = GAMMA_230CD;
		break;
	case 240 ... 249:
		backlightlevel = GAMMA_240CD;
		break;
	case 250 ... 254:
		backlightlevel = GAMMA_250CD;
		break;
	case 255:
		backlightlevel = GAMMA_300CD;
		break;
	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
	}
	return backlightlevel;
}

#ifdef SMART_DIMMING
static int s6e3fa0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][0x01] !=  lcd->aor[lcd->current_bl][0x01])
		goto aid_update;
	else if (lcd->aor[lcd->bl][0x06] !=  lcd->aor[lcd->current_bl][0x01])
		goto aid_update;
	else if (lcd->aor[lcd->bl][0x07] !=  lcd->aor[lcd->current_bl][0x01])
		goto aid_update;
	else
		goto exit;

aid_update:
	s6e3fa0_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	s6e3fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 0;
}
#endif

static int s6e3fa0_gamma_ctl(struct lcd_info *lcd)
{
	s6e3fa0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	s6e3fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

	return 0;
}

static int s6e3fa0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = 0;
	u32 candela = candela_table[lcd->bl];

	switch (candela) {
	case 0 ... 255:
		level = ACL_STATUS_40P;
		break;
	default:
		level = ACL_STATUS_40P;
		break;
	}

	if (lcd->siop_enable)
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3fa0_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_dbg(&lcd->ld->dev, "current_acl = %d\n", lcd->current_acl);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3fa0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0;
	u32 candela = candela_table[lcd->bl];

	switch (candela) {
	case 0 ... 109:
		elvss_level = ELVSS_STATUS_100;
		break;
	case 110 ... 119:
		elvss_level = ELVSS_STATUS_110;
		break;
	case 120 ... 129:
		elvss_level = ELVSS_STATUS_120;
		break;
	case 130 ... 139:
		elvss_level = ELVSS_STATUS_130;
		break;
	case 140 ... 149:
		elvss_level = ELVSS_STATUS_140;
		break;
	case 150 ... 159:
		elvss_level = ELVSS_STATUS_150;
		break;
	case 160 ... 169:
		elvss_level = ELVSS_STATUS_160;
		break;
	case 170 ... 179:
		elvss_level = ELVSS_STATUS_170;
		break;
	case 180 ... 189:
		elvss_level = ELVSS_STATUS_180;
		break;
	case 190 ... 199:
		elvss_level = ELVSS_STATUS_190;
		break;
	case 200 ... 209:
		elvss_level = ELVSS_STATUS_200;
		break;
	case 210 ... 219:
		elvss_level = ELVSS_STATUS_210;
		break;
	case 220 ... 229:
		elvss_level = ELVSS_STATUS_220;
		break;
	case 230 ... 239:
		elvss_level = ELVSS_STATUS_230;
		break;
	case 240 ... 250:
		elvss_level = ELVSS_STATUS_240;
		break;
	case 299:
		elvss_level = ELVSS_STATUS_300;
		break;
	}

	if (force || lcd->current_elvss != lcd->elvss_table[elvss_level][2]) {
		ret = s6e3fa0_write(lcd, lcd->elvss_table[elvss_level], ELVSS_PARAM_SIZE);
		lcd->current_elvss = lcd->elvss_table[elvss_level][2];
	}

	dev_dbg(&lcd->ld->dev, "elvss = %x\n", lcd->elvss_table[elvss_level][2]);

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static int init_elvss_table(struct lcd_info *lcd)
{
	int i, j, ret = 0;

	lcd->elvss_table = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

	if (IS_ERR_OR_NULL(lcd->elvss_table)) {
		pr_err("failed to allocate elvss table\n");
		ret = -ENOMEM;
		goto err_alloc_elvss_table;
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		lcd->elvss_table[i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->elvss_table[i])) {
			pr_err("failed to allocate elvss\n");
			ret = -ENOMEM;
			goto err_alloc_elvss;
		}
		lcd->elvss_table[i][0] = 0xB6;
		lcd->elvss_table[i][1] = 0x28;
		 if ((DEFAULT_ELVSS_LEVEL + ELVSS_CONTROL_TABLE[i][2]) > 0x29)
			lcd->elvss_table[i][2] = 0x29;
		else
			lcd->elvss_table[i][2] = DEFAULT_ELVSS_LEVEL + ELVSS_CONTROL_TABLE[i][2];
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			smtd_dbg("0x%02x, ", lcd->elvss_table[i][j]);
		smtd_dbg("\n");
	}

	return 0;

err_alloc_elvss:
	while (i > 0) {
		kfree(lcd->elvss_table[i-1]);
		i--;
	}
	kfree(lcd->elvss_table);
err_alloc_elvss_table:
	return ret;
}

#ifdef SMART_DIMMING
static int init_gamma_table(struct lcd_info *lcd , const u8 *mtp_data)
{
	int i, j, ret = 0;

	lcd->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lcd->gamma_table)) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->gamma_table[i])) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	if ((lcd->id[2] == 0x00) ||  (lcd->id[2] == 0x01)) {
		lcd->aid = aid_setting_table_VT888;
		for (i = 0; i < ARRAY_SIZE(aid_setting_table_VT888); i++)
			calc_gamma_table(&lcd->smart, lcd->aid, &lcd->gamma_table[i][1], mtp_data, i);
	}

	if (lcd->id[2] == 0x02) {
		lcd->aid = aid_setting_table_VT232;
		for (i = 0; i < ARRAY_SIZE(aid_setting_table_VT232); i++)
			calc_gamma_table(&lcd->smart, lcd->aid, &lcd->gamma_table[i][1], mtp_data, i);
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%d,", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	return 0;

err_alloc_gamma:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table:
	return ret;
}

static int init_aid_dimming_table(struct lcd_info *lcd)
{
	unsigned int i, j, c;
	u16 reverse_seq[] = {
		0, 28, 29, 30, 31, 32,
		33, 25, 26, 27, 22, 23,
		24, 19, 20, 21, 16, 17,
		18, 13, 14, 15, 10, 11,
		12, 7, 8, 9, 4, 5, 6, 1, 2, 3};
	u16 temp[GAMMA_PARAM_SIZE];

	if ((lcd->id[2] == 0x00) || (lcd->id[2] == 0x01)) {
		for (i = 0; i < ARRAY_SIZE(aid_rgb_fix_table_VT888); i++) {
			if (aid_rgb_fix_table_VT888[i].gray == IV_255)
				j = (aid_rgb_fix_table_VT888[i].gray * 3 + aid_rgb_fix_table_VT888[i].rgb*2) + 2;
			else
				j = (aid_rgb_fix_table_VT888[i].gray * 3 + aid_rgb_fix_table_VT888[i].rgb) + 1;
			c = lcd->gamma_table[aid_rgb_fix_table_VT888[i].candela_idx][j] + aid_rgb_fix_table_VT888[i].offset;
			if (c > 0xff)
				lcd->gamma_table[aid_rgb_fix_table_VT888[i].candela_idx][j] = 0xff;
			else
				lcd->gamma_table[aid_rgb_fix_table_VT888[i].candela_idx][j] += aid_rgb_fix_table_VT888[i].offset;
		}
		for (i = 0; i < GAMMA_MAX; i++) {
			memcpy(lcd->aor[i], SEQ_AOR_CONTROL, AID_PARAM_SIZE);
			lcd->aor[i][0x01] = aid_setting_table_VT888[i].aor_cmd[0];
			lcd->aor[i][0x06] = aid_setting_table_VT888[i].aor_cmd[1];
			lcd->aor[i][0x07] = aid_setting_table_VT888[i].aor_cmd[2];
		}
	}

	if (lcd->id[2] == 0x02) {
		for (i = 0; i < ARRAY_SIZE(aid_rgb_fix_table_VT232); i++) {
			if (aid_rgb_fix_table_VT232[i].gray == IV_255)
				j = (aid_rgb_fix_table_VT232[i].gray * 3 + aid_rgb_fix_table_VT232[i].rgb*2) + 2;
			else
				j = (aid_rgb_fix_table_VT232[i].gray * 3 + aid_rgb_fix_table_VT232[i].rgb) + 1;
			c = lcd->gamma_table[aid_rgb_fix_table_VT232[i].candela_idx][j] + aid_rgb_fix_table_VT232[i].offset;
			if (c > 0xff)
				lcd->gamma_table[aid_rgb_fix_table_VT232[i].candela_idx][j] = 0xff;
			else
				lcd->gamma_table[aid_rgb_fix_table_VT232[i].candela_idx][j] += aid_rgb_fix_table_VT232[i].offset;
		}
		for (i = 0; i < GAMMA_MAX; i++) {
			memcpy(lcd->aor[i], SEQ_AOR_CONTROL, AID_PARAM_SIZE);
			lcd->aor[i][0x01] = aid_setting_table_VT232[i].aor_cmd[0];
			lcd->aor[i][0x06] = aid_setting_table_VT232[i].aor_cmd[1];
			lcd->aor[i][0x07] = aid_setting_table_VT232[i].aor_cmd[2];
		}
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%d,", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			temp[j] = lcd->gamma_table[i][reverse_seq[j]];

		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			lcd->gamma_table[i][j] = temp[j];

		for (c = CI_RED; c < CI_MAX ; c++)
			lcd->gamma_table[i][31+c] = lcd->smart.default_gamma[30+c];
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%d,", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}
#endif

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;

	return 0;

#ifdef SMART_DIMMING
	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		s6e3fa0_gamma_ctl(lcd);

		s6e3fa0_aid_parameter_ctl(lcd, force);
		s6e3fa0_set_acl(lcd, force);

		s6e3fa0_set_elvss(lcd, force);

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", brightness, lcd->bl, candela_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);
#endif

	return 0;
}

static int s6e3fa0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	s6e3fa0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(20);

#if !defined(CONFIG_I80_COMMAND_MODE)
	s6e3fa0_write(lcd, SEQ_DISPCTL, ARRAY_SIZE(SEQ_DISPCTL));
#endif
	s6e3fa0_write(lcd, SEQ_GAMMA_CONTROL_SET_300CD, ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET_300CD));
	s6e3fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	s6e3fa0_write(lcd, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	s6e3fa0_write(lcd, SEQ_ELVSS_CONTROL, ARRAY_SIZE(SEQ_ELVSS_CONTROL));
	s6e3fa0_write(lcd, SEQ_ETC_ACL_CONTROL, ARRAY_SIZE(SEQ_ETC_ACL_CONTROL));
	s6e3fa0_write(lcd, SEQ_ETC_POWER_SAVING, ARRAY_SIZE(SEQ_ETC_POWER_SAVING));

	s6e3fa0_write(lcd, SEQ_ETC_PENTILE_SETTING, ARRAY_SIZE(SEQ_ETC_PENTILE_SETTING));
	s6e3fa0_write(lcd, SEQ_ETC_SOURCE_AMP_TYPE_SET_B0, ARRAY_SIZE(SEQ_ETC_SOURCE_AMP_TYPE_SET_B0));
	s6e3fa0_write(lcd, SEQ_ETC_SOURCE_AMP_TYPE_SET_D7, ARRAY_SIZE(SEQ_ETC_SOURCE_AMP_TYPE_SET_D7));
	s6e3fa0_write(lcd, SEQ_ETC_SOURCE_AMP_BC_SET_B0, ARRAY_SIZE(SEQ_ETC_SOURCE_AMP_BC_SET_B0));
	s6e3fa0_write(lcd, SEQ_ETC_SOURCE_AMP_BC_SET_D7, ARRAY_SIZE(SEQ_ETC_SOURCE_AMP_BC_SET_D7));

	return ret;
}

static int s6e3fa0_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e3fa0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int s6e3fa0_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	s6e3fa0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	msleep(35);

	s6e3fa0_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	msleep(135);

	return ret;
}

static int s6e3fa0_power_on(struct lcd_info *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	pd = lcd->lcd_pd;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6e3fa0_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	msleep(120);

	ret = s6e3fa0_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;

	update_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

err:
	return ret;
}

static int s6e3fa0_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->ldi_enable = 0;

	ret = s6e3fa0_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3fa0_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e3fa0_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e3fa0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e3fa0_fhd_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3fa0_power(lcd, power);
}

static int s6e3fa0_fhd_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}


static int s6e3fa0_fhd_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);
	/* dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness); */

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS, brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int s6e3fa0_fhd_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return candela_table[lcd->bl];
}

static int s6e3fa0_fhd_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	/* struct s3cfb_window *win = fb->par;
	struct lcd_info *lcd = lcd_get_data(ld);

	dev_info(&lcd->ld->dev, "%s, fb%d\n", __func__, win->id);*/
	return 0;
}

static struct lcd_ops s6e3fa0_fhd_lcd_ops = {
	.set_power = s6e3fa0_fhd_set_power,
	.get_power = s6e3fa0_fhd_get_power,
	.check_fb  = s6e3fa0_fhd_check_fb,
};

static const struct backlight_ops s6e3fa0_fhd_backlight_ops  = {
	.get_brightness = s6e3fa0_fhd_get_brightness,
	.update_status = s6e3fa0_fhd_set_brightness,
};

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "SMD_AMS568AT01\n");
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[15];

	if (lcd->ldi_enable)
		s6e3fa0_read_id(lcd, lcd->id);

	sprintf(temp, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int i, j;

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->gamma_table[i][j]);
		printk("\n");
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->elvss_table[i][j]);
		printk("\n");
	}

	return strlen(buf);
}
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->siop_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);

static void s6e3fa0_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;

	ret = s6e3fa0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

#ifdef SMART_DIMMING
static int s6e3fa0_read_mtp(struct lcd_info *lcd, u8 *mtp_data)
{
	int ret = 0;
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	msleep(20); /* one frame delay befor reading MTP. */
	ret = s6e3fa0_read(lcd, LDI_MTP_ADDR, mtp_data, LDI_MTP_LENGTH);

	return ret;
}
#endif

static int s6e3fa0_probe(struct mipi_dsim_device *dsim)
{
	int ret = 0, i;
	struct lcd_info *lcd;

#ifdef SMART_DIMMING
	u8 mtp_data[LDI_MTP_LENGTH] = {0,};
#endif

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e3fa0_fhd_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e3fa0_fhd_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_LEVEL;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->ldi_enable = 1;
	lcd->auto_brightness = 0;
	lcd->connected = 1;
	lcd->siop_enable = 0;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	/* dev_set_drvdata(dsim->dev, lcd); */

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e3fa0_read_id(lcd, lcd->id);

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", dev_name(dsim->dev));

#ifdef SMART_DIMMING
	for (i = 0; i < LDI_ID_LEN; i++)
		lcd->smart.panelid[i] = lcd->id[i];

	init_table_info(&lcd->smart);

	ret = s6e3fa0_read_mtp(lcd, mtp_data);
	for (i = 0; i < LDI_MTP_LENGTH ; i++)
		smtd_dbg(" %dth mtp value is %d\n", i, mtp_data[i]);

	if (ret < 1)
		printk(KERN_ERR "[LCD:ERROR] : %s read mtp failed\n", __func__);
	/*return -EPERM;*/

	calc_voltage_table(&lcd->smart, mtp_data);

	ret = init_elvss_table(lcd);
	ret += init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);

	if (ret)
		printk(KERN_ERR "gamma table generation is failed\n");

	update_brightness(lcd, 1);
#endif

	return 0;

out_free_backlight:
	lcd_device_unregister(lcd->ld);
	kfree(lcd);
	return ret;

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}


static int s6e3fa0_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e3fa0_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e3fa0_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e3fa0_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e3fa0_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3fa0_mipi_lcd_driver = {
	.probe		= s6e3fa0_probe,
	.displayon	= s6e3fa0_displayon,
	.suspend	= s6e3fa0_suspend,
	.resume		= s6e3fa0_resume,
};

static int s6e3fa0_fhd_init(void)
{
	return 0 ;
#if 0
	s5p_mipi_dsi_register_lcd_driver(&s6e3fa0_mipi_lcd_driver);
	exynos_mipi_dsi_register_lcd_driver
#endif
}

static void s6e3fa0_fhd_exit(void)
{
	return;
}

module_init(s6e3fa0_fhd_init);
module_exit(s6e3fa0_fhd_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E3FA0 (1080*1920) Panel Driver");
MODULE_LICENSE("GPL");
