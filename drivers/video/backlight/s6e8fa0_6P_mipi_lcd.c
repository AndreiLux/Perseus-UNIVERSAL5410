/* linux/drivers/video/backlight/s6e8fa0_6P_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
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

#include "s6e8fa0_6P_param.h"

#define SMART_DIMMING

#ifdef SMART_DIMMING
#include "smart_dimming_s6e8fa0_6P.h"
#include "aid_s6e8fa0_6P.h"
#endif

#define PSRE_HBM

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		162

#define POWER_IS_ON(pwr)		((pwr) <= FB_BLANK_NORMAL)

#define MAX_GAMMA			300
#define DEFAULT_GAMMA_LEVEL		GAMMA_162CD

#define LDI_ID_REG			0x04
#define LDI_ID1_REG			0xDA
#define LDI_ID2_REG			0xDB
#define LDI_ID3_REG			0xDC
#define LDI_ID_LEN			3

#ifdef SMART_DIMMING
#define LDI_MTP_LENGTH		40
#define LDI_MTP_ADDR			0xC8
#define LDI_ELVSS_LENGTH		17
#define LDI_ELVSS_ADDR		0xB6
#define HBM_READ_LEN			6
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
#ifdef PSRE_HBM
	unsigned char			elvss_cal_origin;
	unsigned char			elvss_cal_hbm;
	unsigned char			hbm_gamma_table[GAMMA_PARAM_SIZE];
	unsigned char			hbm_enable;
#endif
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
	10,	11,	12,	13,	14,	15,	16,	17,
	19,	20,	21,	22,	24,	25,	27,	29,
	30,	32,	34,	37,	39,	41,	44,	47,
	50,	53,	56,	60,	64,	68,	72,	77,
	82,	87,	93,	98,	105,	111,	119,	126,
	134,	143,	152,	162,	172,	183,	195,	207,
	220,	234,	249,	265,	282,	MAX_GAMMA-1
};

static struct lcd_info *g_lcd;
static void s6e8fa0_read_id(struct lcd_info *lcd, u8 *buf);

static int s6e8fa0_write(struct lcd_info *lcd, const u8 *seq, u32 len)
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


static int s6e8fa0_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
	case 0 ... 10:
		backlightlevel = GAMMA_10CD;
		break;
	case 11:
		backlightlevel = GAMMA_11CD;
		break;
	case 12:
		backlightlevel = GAMMA_12CD;
		break;
	case 13:
		backlightlevel = GAMMA_13CD;
		break;
	case 14:
		backlightlevel = GAMMA_14CD;
		break;
	case 15:
		backlightlevel = GAMMA_15CD;
		break;
	case 16:
		backlightlevel = GAMMA_16CD;
		break;
	case 17 ... 18:
		backlightlevel = GAMMA_17CD;
		break;
	case 19:
		backlightlevel = GAMMA_19CD;
		break;
	case 20:
		backlightlevel = GAMMA_20CD;
		break;
	case 21:
		backlightlevel = GAMMA_21CD;
		break;
	case 22 ... 23:
		backlightlevel = GAMMA_22CD;
		break;
	case 24:
		backlightlevel = GAMMA_24CD;
		break;
	case 25 ... 26:
		backlightlevel = GAMMA_25CD;
		break;
	case 27 ... 28:
		backlightlevel = GAMMA_27CD;
		break;
	case 29:
		backlightlevel = GAMMA_29CD;
		break;
	case 30 ... 31:
		backlightlevel = GAMMA_30CD;
		break;
	case 32 ... 33:
		backlightlevel = GAMMA_32CD;
		break;
	case 34 ... 36:
		backlightlevel = GAMMA_34CD;
		break;
	case 37 ... 38:
		backlightlevel = GAMMA_37CD;
		break;
	case 39 ... 40:
		backlightlevel = GAMMA_39CD;
		break;
	case 41 ... 43:
		backlightlevel = GAMMA_41CD;
		break;
	case 44 ... 46:
		backlightlevel = GAMMA_44CD;
		break;
	case 47 ... 49:
		backlightlevel = GAMMA_47CD;
		break;
	case 50 ... 52:
		backlightlevel = GAMMA_50CD;
		break;
	case 53 ... 55:
		backlightlevel = GAMMA_53CD;
		break;
	case 56 ... 59:
		backlightlevel = GAMMA_56CD;
		break;
	case 60 ... 63:
		backlightlevel = GAMMA_60CD;
		break;
	case 64 ... 67:
		backlightlevel = GAMMA_64CD;
		break;
	case 68 ... 71:
		backlightlevel = GAMMA_68CD;
		break;
	case 72 ... 76:
		backlightlevel = GAMMA_72CD;
		break;
	case 77 ... 81:
		backlightlevel = GAMMA_77CD;
		break;
	case 82 ... 86:
		backlightlevel = GAMMA_82CD;
		break;
	case 87 ... 92:
		backlightlevel = GAMMA_87CD;
		break;
	case 93 ... 97:
		backlightlevel = GAMMA_93CD;
		break;
	case 98 ... 104:
		backlightlevel = GAMMA_98CD;
		break;
	case 105 ... 110:
		backlightlevel = GAMMA_105CD;
		break;
	case 111 ... 118:
		backlightlevel = GAMMA_111CD;
		break;
	case 119 ... 125:
		backlightlevel = GAMMA_119CD;
		break;
	case 126 ... 133:
		backlightlevel = GAMMA_126CD;
		break;
	case 134 ... 142:
		backlightlevel = GAMMA_134CD;
		break;
	case 143 ... 151:
		backlightlevel = GAMMA_143CD;
		break;
	case 152 ... 161:
		backlightlevel = GAMMA_152CD;
		break;
	case 162 ... 171:
		backlightlevel = GAMMA_162CD;
		break;
	case 172 ... 182:
		backlightlevel = GAMMA_172CD;
		break;
	case 183 ... 194:
		backlightlevel = GAMMA_183CD;
		break;
	case 195 ... 206:
		backlightlevel = GAMMA_195CD;
		break;
	case 207 ... 219:
		backlightlevel = GAMMA_207CD;
		break;
	case 220 ... 233:
		backlightlevel = GAMMA_220CD;
		break;
	case 234 ... 248:
		backlightlevel = GAMMA_234CD;
		break;
	case 249:
		backlightlevel = GAMMA_249CD;
		break;
	case 250 ... 251:
		backlightlevel = GAMMA_265CD;
		break;
	case 252 ... 253:
		backlightlevel = GAMMA_282CD;
		break;
	case 254 ... 255:
		backlightlevel = GAMMA_300CD;
		break;
	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
	}

	return backlightlevel;
}


#ifdef SMART_DIMMING
static int s6e8fa0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
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
	s6e8fa0_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	s6e8fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 0;
}
#endif

static int s6e8fa0_gamma_ctl(struct lcd_info *lcd)
{
#ifdef PSRE_HBM
	if (lcd->auto_brightness == 6) {
		if (lcd->hbm_enable == 0) {
			s6e8fa0_write(lcd, SEQ_PSRE_MODE_ON, ARRAY_SIZE(SEQ_PSRE_MODE_ON));
			dev_info(lcd->dev, "HBM enable\n");
			lcd->hbm_enable = 1;
		}
		s6e8fa0_write(lcd, lcd->hbm_gamma_table, GAMMA_PARAM_SIZE);
	} else {
		if (lcd->hbm_enable == 1) {
			s6e8fa0_write(lcd, SEQ_PSRE_MODE_OFF, ARRAY_SIZE(SEQ_PSRE_MODE_OFF));
			dev_info(lcd->dev, "HBM disable\n");
			lcd->hbm_enable = 0;
		}
		s6e8fa0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	}
#else
	s6e8fa0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
#endif

	return 0;
}

static int s6e8fa0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = 0;

	if (lcd->auto_brightness >= 6)
		level = ACL_STATUS_40P_RE_MID;
	else
		level = ACL_STATUS_40P;

	if ((lcd->siop_enable) || (lcd->auto_brightness >= 6))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e8fa0_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "current_acl = %d, auto_brightness = %d\n", lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e8fa0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0;
	u32 candela = candela_table[lcd->bl];

	switch (candela) {
	case 0 ... 105:
		elvss_level = ELVSS_STATUS_105;
		break;
	case 106 ... 111:
		elvss_level = ELVSS_STATUS_111;
		break;
	case 112 ... 119:
		elvss_level = ELVSS_STATUS_119;
		break;
	case 120 ... 126:
		elvss_level = ELVSS_STATUS_126;
		break;
	case 127 ... 134:
		elvss_level = ELVSS_STATUS_134;
		break;
	case 135 ... 143:
		elvss_level = ELVSS_STATUS_143;
		break;
	case 144 ... 152:
		elvss_level = ELVSS_STATUS_152;
		break;
	case 153 ... 162:
		elvss_level = ELVSS_STATUS_162;
		break;
	case 163 ... 172:
		elvss_level = ELVSS_STATUS_172;
		break;
	case 173 ... 183:
		elvss_level = ELVSS_STATUS_183;
		break;
	case 184 ... 195:
		elvss_level = ELVSS_STATUS_195;
		break;
	case 196 ... 207:
		elvss_level = ELVSS_STATUS_207;
		break;
	case 208 ... 220:
		elvss_level = ELVSS_STATUS_220;
		break;
	case 221 ... 234:
		elvss_level = ELVSS_STATUS_234;
		break;
	case 235 ... 249:
		elvss_level = ELVSS_STATUS_249;
		break;
	case 250 ... 265:
		elvss_level = ELVSS_STATUS_265;
		break;
	case 266 ... 282:
		elvss_level = ELVSS_STATUS_282;
		break;
	case 283 ... 299:
		elvss_level = ELVSS_STATUS_300;
		break;
	}

	if (force || lcd->current_elvss != lcd->elvss_table[elvss_level][2]) {
#ifdef PSRE_HBM
		if (lcd->auto_brightness == 6)
			lcd->elvss_table[elvss_level][17] = lcd->elvss_cal_hbm;
		else
			lcd->elvss_table[elvss_level][17] = lcd->elvss_cal_origin;
#endif
		ret = s6e8fa0_write(lcd, lcd->elvss_table[elvss_level], ELVSS_PARAM_SIZE);
		lcd->current_elvss = lcd->elvss_table[elvss_level][2];
	}

	/*for (j = 0; j < ELVSS_PARAM_SIZE; j++)
		printk("ELVSS: 0x%02x\n", lcd->elvss_table[elvss_level][j]);*/

	dev_dbg(&lcd->ld->dev, "elvss = %x, %d\n", lcd->elvss_table[elvss_level][2], lcd->elvss_table[elvss_level][17]);

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
		if ((lcd->elvss_ref + ELVSS_CONTROL_TABLE[i][2]) > 0x29)
			lcd->elvss_table[i][2] = 0x29;
		else
			lcd->elvss_table[i][2] = lcd->elvss_ref + ELVSS_CONTROL_TABLE[i][2];
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

	lcd->aid = aid_setting_table_VT232;
	for (i = 0; i < ARRAY_SIZE(aid_setting_table_VT232); i++)
		calc_gamma_table_6P(&lcd->smart, lcd->aid, &lcd->gamma_table[i][1], mtp_data, i);

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

static int init_aid_dimming_table(struct lcd_info *lcd, const u8 *mtp_data)
{
	unsigned int i, j, c;
	u16 reverse_seq[] = {
		0, 28, 29, 30, 31, 32,
		33, 25, 26, 27, 22, 23,
		24, 19, 20, 21, 16, 17,
		18, 13, 14, 15, 10, 11,
		12, 7, 8, 9, 4, 5, 6, 1, 2, 3};
	u16 temp[GAMMA_PARAM_SIZE];

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

#ifdef PSRE_HBM
	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		lcd->hbm_gamma_table[i] = lcd->gamma_table[GAMMA_300CD][i];

	for (i = 0; i < HBM_READ_LEN; i++)
		lcd->hbm_gamma_table[1+i] = mtp_data[33+i];
#endif

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

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		s6e8fa0_gamma_ctl(lcd);

		s6e8fa0_aid_parameter_ctl(lcd, force);

		s6e8fa0_set_acl(lcd, force);

		s6e8fa0_set_elvss(lcd, force);

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d, hbm=%d\n", \
				brightness, lcd->bl, candela_table[lcd->bl], lcd->hbm_enable);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6e8fa0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;
	char lcd_buf[3] = {0,};
	char lcd_count = 0;
	unsigned char evt1_check = 0x20;

	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	/*EVT1 check*/
	if ((lcd->id[2] & evt1_check) != evt1_check) {
		for (lcd_count = 0; lcd_count < 15; lcd_count++) {
			s6e8fa0_read(lcd, SEQ_TEST_KEY_ON_F0[0], lcd_buf, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

			printk(KERN_INFO "EVT 0 lcd_check = %x, lcd_count = %d\n", lcd_buf[1], lcd_count);

			if (lcd_buf[1] == 0x5A)
				break;

			msleep(100);

			s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		}
	}

	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if (lcd->id[2] == 0x03)
		s6e8fa0_write(lcd, SEQ_SCAN_TIMMING_1_FE, ARRAY_SIZE(SEQ_SCAN_TIMMING_1_FE));

	usleep_range(17000, 17000); /*wait 1 frame*/

	s6e8fa0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	s6e8fa0_write(lcd, SEQ_ERROR_FLAG_ON, ARRAY_SIZE(SEQ_ERROR_FLAG_ON));

	msleep(25);

	s6e8fa0_write(lcd, SEQ_LTPS_F2, ARRAY_SIZE(SEQ_LTPS_F2));
	s6e8fa0_write(lcd, SEQ_LTPS_B0, ARRAY_SIZE(SEQ_LTPS_B0));
	s6e8fa0_write(lcd, SEQ_LTPS_CB, ARRAY_SIZE(SEQ_LTPS_CB));
	s6e8fa0_write(lcd, SEQ_LTPS_F7, ARRAY_SIZE(SEQ_LTPS_F7));

	msleep(100);

	if (lcd->id[2] == 0x03) {
		s6e8fa0_write(lcd, SEQ_SCAN_TIMMING_2_FD, ARRAY_SIZE(SEQ_SCAN_TIMMING_2_FD));
		s6e8fa0_write(lcd, SEQ_SCAN_TIMMING_2_C0, ARRAY_SIZE(SEQ_SCAN_TIMMING_2_C0));
		s6e8fa0_write(lcd, SEQ_SCAN_TIMMING_2_F4, ARRAY_SIZE(SEQ_SCAN_TIMMING_2_F4));
	}
	s6e8fa0_write(lcd, SEQ_SCAN_TIMMING_2_F6, ARRAY_SIZE(SEQ_SCAN_TIMMING_2_F6));

	s6e8fa0_gamma_ctl(lcd);
	s6e8fa0_write(lcd, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	s6e8fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	s6e8fa0_write(lcd, SEQ_ELVSS_CONDITION_SET, ARRAY_SIZE(SEQ_ELVSS_CONDITION_SET));
#ifdef PSRE_HBM
	s6e8fa0_write(lcd, SEQ_PSRE_MODE_OFF, ARRAY_SIZE(SEQ_PSRE_MODE_OFF));
	lcd->hbm_enable  = 0;
#endif
	s6e8fa0_write(lcd, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
#ifdef PSRE_HBM
	s6e8fa0_write(lcd, SEQ_PSRE_MODE_SET2, ARRAY_SIZE(SEQ_PSRE_MODE_SET2));
	s6e8fa0_write(lcd, SEQ_PSRE_MODE_SET3, ARRAY_SIZE(SEQ_PSRE_MODE_SET3));
#endif
	usleep_range(17000, 17000); /*wait 1 frame*/

	s6e8fa0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int s6e8fa0_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e8fa0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int s6e8fa0_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	s6e8fa0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	msleep(35);

	s6e8fa0_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	msleep(125);

	return ret;
}

static int s6e8fa0_power_on(struct lcd_info *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	pd = lcd->lcd_pd;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6e8fa0_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	msleep(120);

	ret = s6e8fa0_ldi_enable(lcd);
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

static int s6e8fa0_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->ldi_enable = 0;

	ret = s6e8fa0_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e8fa0_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e8fa0_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e8fa0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e8fa0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e8fa0_power(lcd, power);
}

static int s6e8fa0_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e8fa0_set_brightness(struct backlight_device *bd)
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

static int s6e8fa0_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return candela_table[lcd->bl];
}

static int s6e8fa0_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	/* struct s3cfb_window *win = fb->par;
	struct lcd_info *lcd = lcd_get_data(ld);

	dev_info(&lcd->ld->dev, "%s, fb%d\n", __func__, win->id);*/
	return 0;
}

static struct lcd_ops s6e8fa0_lcd_ops = {
	.set_power = s6e8fa0_set_power,
	.get_power = s6e8fa0_get_power,
	.check_fb  = s6e8fa0_check_fb,
};

static const struct backlight_ops s6e8fa0_backlight_ops  = {
	.get_brightness = s6e8fa0_get_brightness,
	.update_status = s6e8fa0_set_brightness,
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
	char temp[] = "SMD_AMS499QP01\n";

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
		s6e8fa0_read_id(lcd, lcd->id);

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

static void s6e8fa0_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;

	ret = s6e8fa0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

#ifdef SMART_DIMMING
static int s6e8fa0_read_mtp(struct lcd_info *lcd, u8 *mtp_data)
{
	int ret = 0;
	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	msleep(20); /* one frame delay befor reading MTP. */
	ret = s6e8fa0_read(lcd, LDI_MTP_ADDR, mtp_data, LDI_MTP_LENGTH);

	return ret;
}

static int s6e8fa0_read_elvss(struct lcd_info *lcd, u8 *elvss_data)
{
	int ret = 0;
	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e8fa0_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	msleep(20); /* one frame delay befor reading ELVSS. */
	ret = s6e8fa0_read(lcd, LDI_ELVSS_ADDR, elvss_data, LDI_ELVSS_LENGTH);

	return ret;
}
#endif

static int s6e8fa0_probe(struct mipi_dsim_device *dsim)
{
	int ret = 0, i;
	struct lcd_info *lcd;

#ifdef SMART_DIMMING
	u8 mtp_data[LDI_MTP_LENGTH] = {0,};
	u8 elvss_data[LDI_ELVSS_LENGTH] = {0,};
#endif

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e8fa0_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e8fa0_backlight_ops, NULL);
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

	s6e8fa0_read_id(lcd, lcd->id);

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", dev_name(dsim->dev));

#ifdef SMART_DIMMING
	for (i = 0; i < LDI_ID_LEN; i++)
		lcd->smart.panelid[i] = lcd->id[i];

	init_table_info_6P(&lcd->smart);

	ret = s6e8fa0_read_mtp(lcd, mtp_data);
	for (i = 0; i < LDI_MTP_LENGTH ; i++)
		smtd_dbg("%dth mtp value is %d\n", i, mtp_data[i]);

	if (ret < 1)
		printk(KERN_ERR "[LCD:ERROR] : %s read mtp failed\n", __func__);
	/*return -EPERM;*/

	calc_voltage_table_6P(&lcd->smart, mtp_data);

	ret = s6e8fa0_read_elvss(lcd, elvss_data);
	for (i = 0; i < LDI_ELVSS_LENGTH ; i++)
		smtd_dbg("%dth ELVSS_volt_level %x\n", i, elvss_data[i]);

	lcd->elvss_ref = elvss_data[1];
#ifdef PSRE_HBM
	lcd->elvss_cal_origin = elvss_data[16];
	lcd->elvss_cal_hbm = mtp_data[39];
#endif
	ret = init_elvss_table(lcd);
	ret += init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd, mtp_data);

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


static int s6e8fa0_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e8fa0_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e8fa0_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e8fa0_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e8fa0_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e8fa0_6P_mipi_lcd_driver = {
	.probe		= s6e8fa0_probe,
	.displayon	= s6e8fa0_displayon,
	.suspend	= s6e8fa0_suspend,
	.resume		= s6e8fa0_resume,
};

static int s6e8fa0_init(void)
{
	return 0 ;
#if 0
	s5p_mipi_dsi_register_lcd_driver(&s6e8fa0_6P_mipi_lcd_driver);
	exynos_mipi_dsi_register_lcd_driver
#endif
}

static void s6e8fa0_exit(void)
{
	return;
}

module_init(s6e8fa0_init);
module_exit(s6e8fa0_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E8FA0 (1080*1920) Panel Driver");
MODULE_LICENSE("GPL");
