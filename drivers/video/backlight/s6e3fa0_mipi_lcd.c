/* linux/drivers/video/backlight/s6e3fa0_mipi_lcd.c
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
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>
#include <asm/system_info.h>

#include "s6e3fa0_param.h"

#include "dynamic_aid_s6e3fa0.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		143

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)
#define LEVEL_IS_PSRE(level)		(level >= 6)

#define MAX_GAMMA			300
#define DEFAULT_GAMMA_LEVEL		GAMMA_134CD

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			33	/* MTP */
#define LDI_ELVSS_REG			0xB6
#define LDI_HBM_LEN			6	/* HBM V255 */

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			8	/* REG address + 7 para */

#define LDI_DATE_REG			0xC8
#define LDI_DATE_LEN			42

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
	unsigned int			current_psre;
	unsigned int			current_tset;
	unsigned int			elvss_compensation;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table[2];
	struct dynamic_aid_param_t daid;
	unsigned char			aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;

	unsigned char			**tset_table;
	int				temperature;

	unsigned int			coordinate[2];

	struct mipi_dsim_device		*dsim;
};

static const unsigned int candela_table[GAMMA_MAX] = {
	10,	11,	12,	13,	14,	15,	16,	17,
	19,	20,	21,	22,	24,	25,	27,	29,
	30,	32,	34,	37,	39,	41,	44,	47,
	50,	53,	56,	60,	64,	68,	72,	77,
	82,	87,	93,	98,	105,	111,	119,	126,
	134,	143,	152,	162,	172,	183,	195,	207,
	220,	234,	249,	265,	282,	MAX_GAMMA-1,	400,
};

static struct lcd_info *g_lcd;

extern void (*panel_touchkey_on)(void);
extern void (*panel_touchkey_off)(void);

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
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
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
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 1);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void s6e3fa0_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e3fa0_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */
}

static void s6e3fa0_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static int s6e3fa0_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3fa0_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN+6);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	for (i = 0; i < LDI_MTP_LEN ; i++)
		smtd_dbg("%02dth mtp value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6e3fa0_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_ELVSS_REG, buf, ELVSS_PARAM_SIZE);

	return ret;
}

static int s6e3fa0_read_tset(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_TSET_REG, buf, LDI_TSET_LEN);

	return ret;
}

static int s6e3fa0_read_date(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_DATE_REG, buf, LDI_DATE_LEN);

	return ret;
}

static int s6e3fa0_read_hbm(struct lcd_info *lcd, u8 *buf)
{
	int ret;
	unsigned char SEQ_HBM_POSITION[] = {0xB0, 33, 0x00};

	ret = s6e3fa0_write(lcd, SEQ_HBM_POSITION, ARRAY_SIZE(SEQ_HBM_POSITION));

	ret += s6e3fa0_read(lcd, LDI_MTP_REG, buf, LDI_HBM_LEN);

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

#if 0
static int s6e3fa0_set_psre(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force || lcd->current_psre != LEVEL_IS_PSRE(lcd->auto_brightness)) {
		if (LEVEL_IS_PSRE(lcd->auto_brightness))
			ret = s6e3fa0_write(lcd, SEQ_PSRE_MODE_ON, ARRAY_SIZE(SEQ_PSRE_MODE_ON));
		else
			ret = s6e3fa0_write(lcd, SEQ_PSRE_MODE_OFF, ARRAY_SIZE(SEQ_PSRE_MODE_OFF));

		lcd->current_psre = LEVEL_IS_PSRE(lcd->auto_brightness);

		dev_info(&lcd->ld->dev, "psre: %d, auto_brightness: %d\n", lcd->current_psre, lcd->auto_brightness);
	}

	return ret;
}
#endif

static int s6e3fa0_gamma_ctl(struct lcd_info *lcd)
{
	s6e3fa0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int s6e3fa0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][3] !=  lcd->aor[lcd->current_bl][3])
		goto aid_update;
	else if (lcd->aor[lcd->bl][4] !=  lcd->aor[lcd->current_bl][4])
		goto aid_update;
	else if (lcd->aor[lcd->bl][7] !=  lcd->aor[lcd->current_bl][7])
		goto aid_update;
	else
		goto exit;

aid_update:
	s6e3fa0_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	s6e3fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 0;
}

static int s6e3fa0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = 0;

	if (LEVEL_IS_PSRE(lcd->auto_brightness))
		level = ACL_STATUS_40P_RE_LOW;
	else
		level = ACL_STATUS_40P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3fa0_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3fa0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0, elvss;
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
	case 400:
		elvss_level = ELVSS_STATUS_HBM;
		break;
	default:
		elvss_level = ELVSS_STATUS_300;
		break;
	}

	elvss = lcd->elvss_table[lcd->elvss_compensation][elvss_level][2];

	if (force || lcd->elvss_table[lcd->elvss_compensation][lcd->current_elvss][2] != elvss) {
		ret = s6e3fa0_write(lcd, lcd->elvss_table[lcd->elvss_compensation][elvss_level], ELVSS_PARAM_SIZE);
		lcd->current_elvss = elvss_level;

		dev_dbg(&lcd->ld->dev, "elvss: %d, %d, %x\n", lcd->elvss_compensation, lcd->current_elvss,
			lcd->elvss_table[lcd->elvss_compensation][lcd->current_elvss][2]);
	}

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static int s6e3fa0_set_tset(struct lcd_info *lcd, u8 force)
{
	int ret = 0, tset_level = 0;

	switch (lcd->temperature) {
	case 1:
		tset_level = TSET_25_DEGREES;
		break;
	case 0:
	case -19:
		tset_level = TSET_MINUS_0_DEGREES;
		break;
	case -20:
		tset_level = TSET_MINUS_20_DEGREES;
		break;
	}

	if (force || lcd->current_tset != tset_level) {
		ret = s6e3fa0_write(lcd, lcd->tset_table[tset_level], LDI_TSET_LEN);
		lcd->current_tset = tset_level;
		dev_info(&lcd->ld->dev, "tset: %d\n", lcd->current_tset);
	}

	if (!ret) {
		ret = -EPERM;
		goto err;
	}

err:
	return ret;
}

void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.br_base = brightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = offset_color;
}

static void init_mtp_data(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd->daid.mtp;

	for (c = 0, j = 0; c < CI_MAX ; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_MAX - 2; i >= 0; i--) {
		for (c=0; c<CI_MAX ; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (mtp_data[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j];
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c=0; c<CI_MAX ; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n",j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c=0; c<CI_MAX ; c++, j++)
			smtd_dbg("mtp[%d] = %d\n",j, mtp[j]);
}

static int init_gamma_table(struct lcd_info *lcd , const u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(gamma)) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (IS_ERR_OR_NULL(gamma[i])) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lcd->gamma_table)) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->gamma_table[i])) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < GAMMA_MAX - 1; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX ; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX ; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}

		for (v = 0; v < GAMMA_PARAM_SIZE; v++)
			smtd_dbg("%d ", lcd->gamma_table[i][v]);
		smtd_dbg("\n");
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
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
	int i, j;

	for (i = 0; i < GAMMA_MAX -1; i++) {
		memcpy(lcd->aor[i], SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
		lcd->aor[i][7] = aor_cmd[i][0] << 3;
		lcd->aor[i][3] = aor_cmd[i][1];
		lcd->aor[i][4] = aor_cmd[i][2];

		for (j = 0; j < ARRAY_SIZE(SEQ_AOR_CONTROL); j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, j, k, ret;

	for (k = 0; k < 2; k++) {
		lcd->elvss_table[k] = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

		if (IS_ERR_OR_NULL(lcd->elvss_table[k])) {
			pr_err("failed to allocate elvss table\n");
			ret = -ENOMEM;
			goto err_alloc_elvss_table;
		}

		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			lcd->elvss_table[k][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
			if (IS_ERR_OR_NULL(lcd->elvss_table[k][i])) {
				pr_err("failed to allocate elvss\n");
				ret = -ENOMEM;
				goto err_alloc_elvss;
			}

			lcd->elvss_table[k][i][0] = 0xB6;
			lcd->elvss_table[k][i][1] = 0x88;
			lcd->elvss_table[k][i][2] = 0x0A;  /* elvss_data[1]; */
			lcd->elvss_table[k][i][2] += ELVSS_TABLE[i];
		}

		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			for (j = 0; j < ELVSS_PARAM_SIZE; j++)
				smtd_dbg("0x%02x, ", lcd->elvss_table[k][i][j]);
			smtd_dbg("\n");
		}
	}

	/* this (elvss_table[1]) is elvss table to support low temperature */
	for (i = 0; i < ELVSS_STATUS_MAX; i++)
		lcd->elvss_table[1][i][2] = (lcd->elvss_table[1][i][2] > 4) ? (lcd->elvss_table[1][i][2] - 4) : 0;

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with k */
	while (i > 0) {
		kfree(lcd->elvss_table[k][i-1]);
		i--;
	}
	kfree(lcd->elvss_table[k]);
err_alloc_elvss_table:
	return ret;
}

static int init_tset_table(struct lcd_info *lcd, u8 *tset_data)
{
	int i, j, ret;

	lcd->tset_table = kzalloc(TSET_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lcd->tset_table)) {
		pr_err("failed to allocate tset table\n");
		ret = -ENOMEM;
		goto err_alloc_tset_table;
	}

	for (i = 0; i < TSET_STATUS_MAX; i++) {
		lcd->tset_table[i] = kzalloc(LDI_TSET_LEN * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->tset_table[i])) {
			pr_err("failed to allocate tset\n");
			ret = -ENOMEM;
			goto err_alloc_tset;
		}

		lcd->tset_table[i][0] = LDI_TSET_REG;
		for (j = 0; j < 6; j++)
			lcd->tset_table[i][j+1] = tset_data[j];
		lcd->tset_table[i][6] = TSET_TABLE[i];
		lcd->tset_table[i][7] = 0x02;
	}

	for (i = 0; i < TSET_STATUS_MAX; i++) {
		for (j = 0; j < LDI_TSET_LEN; j++)
			smtd_dbg("0x%02x, ", lcd->tset_table[i][j]);
		smtd_dbg("\n");
	}

	return 0;

err_alloc_tset:
	while (i > 0) {
		kfree(lcd->tset_table[i-1]);
		i--;
	}
err_alloc_tset_table:
	return ret;
}

static int init_hbm_parameter(struct lcd_info *lcd, const u8 *mtp_data, const u8 *hbm_data)
{
	int i;

	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		lcd->gamma_table[GAMMA_HBM][i] = lcd->gamma_table[GAMMA_300CD][i];

	/* C8 34~39 -> CA 1~6 */
	lcd->gamma_table[GAMMA_HBM][1] = hbm_data[0];
	lcd->gamma_table[GAMMA_HBM][2] = hbm_data[1];
	lcd->gamma_table[GAMMA_HBM][3] = hbm_data[2];
	lcd->gamma_table[GAMMA_HBM][4] = hbm_data[3];
	lcd->gamma_table[GAMMA_HBM][5] = hbm_data[4];
	lcd->gamma_table[GAMMA_HBM][6] = hbm_data[5];

	lcd->elvss_table[0][ELVSS_STATUS_HBM][2] = 0x00;
	lcd->elvss_table[1][ELVSS_STATUS_HBM][2] = 0x00;

	return 0;

}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == lcd->bd->props.max_brightness))
		lcd->bl = GAMMA_HBM;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		s6e3fa0_gamma_ctl(lcd);

		s6e3fa0_aid_parameter_ctl(lcd, force);

		s6e3fa0_set_acl(lcd, force);

		s6e3fa0_set_elvss(lcd, force);

		/*s6e3fa0_set_psre(lcd, force);*/

		/*s6e3fa0_set_tset(lcd, force);*/

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", \
			brightness, lcd->bl, candela_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

/* This function is only used by touch driver for TSP tunning in RevG panel. */
/* So other type's lcd driver don't have this function */
static void s6e3fa0_ldi_touchkey_on(void)
{
#if 0
	if (g_lcd != NULL)
		s6e3fa0_write(g_lcd, SEQ_TOUCHKEY_ON, ARRAY_SIZE(SEQ_TOUCHKEY_ON));
#endif
}

static void s6e3fa0_ldi_touchkey_off(void)
{
#if 0
	if (g_lcd != NULL)
		s6e3fa0_write(g_lcd, SEQ_TOUCHKEY_OFF, ARRAY_SIZE(SEQ_TOUCHKEY_OFF));
#endif
}

static int s6e3fa0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	s6e3fa0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(20);

#if defined(CONFIG_I80_COMMAND_MODE)
	s6e3fa0_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
#else
	s6e3fa0_write(lcd, SEQ_DISPCTL, ARRAY_SIZE(SEQ_DISPCTL));
#endif

	update_brightness(lcd, 1);
	s6e3fa0_write(lcd, SEQ_ACL_CONTROL, ARRAY_SIZE(SEQ_ACL_CONTROL));

	s6e3fa0_write(lcd, SEQ_ETC_PENTILE_SETTING, ARRAY_SIZE(SEQ_ETC_PENTILE_SETTING));
	s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_SOURCE_AMP, ARRAY_SIZE(SEQ_GLOBAL_PARAM_SOURCE_AMP));
	s6e3fa0_write(lcd, SEQ_ETC_SOURCE_AMP, ARRAY_SIZE(SEQ_ETC_SOURCE_AMP));
	s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_BIAS_CURRENT, ARRAY_SIZE(SEQ_GLOBAL_PARAM_BIAS_CURRENT));
	s6e3fa0_write(lcd, SEQ_ETC_BIAS_CURRENT, ARRAY_SIZE(SEQ_ETC_BIAS_CURRENT));

	msleep(120);

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
	int ret;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6e3fa0_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

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
	int ret;

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

static int s6e3fa0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3fa0_power(lcd, power);
}

static int s6e3fa0_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}


static int s6e3fa0_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	/* dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness); */

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd->bd->props.max_brightness, brightness);
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

static int s6e3fa0_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return candela_table[lcd->bl];
}

static int s6e3fa0_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops s6e3fa0_lcd_ops = {
	.set_power = s6e3fa0_set_power,
	.get_power = s6e3fa0_get_power,
	.check_fb  = s6e3fa0_check_fb,
};

static const struct backlight_ops s6e3fa0_backlight_ops  = {
	.get_brightness = s6e3fa0_get_brightness,
	.update_status = s6e3fa0_set_brightness,
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
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "SMD_AMS568AT01\n";

	strcat(buf, temp);
	return strlen(buf);
}

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
			printk("0x%02x, ", lcd->elvss_table[0][i][j]);
		printk("\n");
	}

	return strlen(buf);
}

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
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 0);
		}
	}
	return size;
}

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
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, rc, temperature = 0, elvss_compensation = 0;

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		switch (value) {
		case 1:
		case 0:
		case -19:
			temperature = value;
			elvss_compensation = 0;
			break;
		case -20:
			temperature = value;
			elvss_compensation = 1;
			break;
		}

		mutex_lock(&lcd->bl_lock);
		lcd->temperature = temperature;
		lcd->elvss_compensation = elvss_compensation;
		mutex_unlock(&lcd->bl_lock);

		if (lcd->ldi_enable)
			update_brightness(lcd, 0);

		dev_info(dev, "%s: %d, %d, %d\n", __func__, value, lcd->temperature, lcd->elvss_compensation);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d, %d\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, manufacture_data[LDI_DATE_LEN] = {0,};

	if (lcd->ldi_enable)
		s6e3fa0_read_date(lcd, manufacture_data);

	year = ((manufacture_data[40] & 0xF0) >> 4) + 2011;
	month = manufacture_data[40] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, manufacture_data[41]);

	return strlen(buf);
}

static ssize_t parameter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	unsigned char temp[50] = {0,};
	int i;

	if (!lcd->ldi_enable)
		return -EINVAL;

	/* ID */
	s6e3fa0_read(lcd, LDI_ID_REG, temp, LDI_ID_LEN);
	pos += sprintf(pos, "ID    [04]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);

#if 0
	/* PSRE */
	s6e3fa0_read(lcd, 0xB5, temp, 3);
	pos += sprintf(pos, "PSRE  [B5]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);
#endif

	/* ACL */
	s6e3fa0_read(lcd, 0x56, temp, ACL_PARAM_SIZE);
	pos += sprintf(pos, "ACL   [56]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);

	/* ELVSS */
	s6e3fa0_read(lcd, LDI_ELVSS_REG, temp, ELVSS_PARAM_SIZE);
	pos += sprintf(pos, "ELVSS [B6]: %02x, %02x, %02x / %d\n",
		temp[0], temp[1], temp[2], lcd->elvss_compensation);

	/* TSET */
	s6e3fa0_read(lcd, LDI_TSET_REG, temp, LDI_TSET_LEN);
	pos += sprintf(pos, "TSET  [B8]: ");
	for (i = 0; i < LDI_TSET_LEN; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "/ %d, %d\n", lcd->temperature, lcd->current_tset);

	/* GAMMA */
	s6e3fa0_read(lcd, 0xCA, temp, GAMMA_PARAM_SIZE);
	pos += sprintf(pos, "GAMMA [CA]: ");
	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	return pos - buf;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(parameter, 0444, parameter_show, NULL);

static int s6e3fa0_probe(struct mipi_dsim_device *dsim)
{
	int ret;
	struct lcd_info *lcd;

	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[ELVSS_PARAM_SIZE] = {0,};
	u8 tset_data[LDI_TSET_LEN] = {0,};
	u8 hbm_data[LDI_HBM_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e3fa0_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e3fa0_backlight_ops, NULL);
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
	lcd->temperature = 1;
	lcd->current_tset = TSET_25_DEGREES;

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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_parameter);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	/* dev_set_drvdata(dsim->dev, lcd); */

	panel_touchkey_on = s6e3fa0_ldi_touchkey_on;
	panel_touchkey_off = s6e3fa0_ldi_touchkey_off;

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e3fa0_read_id(lcd, lcd->id);
	/*s6e3fa0_read_coordinate(lcd);*/
	s6e3fa0_read_mtp(lcd, mtp_data);
	s6e3fa0_read_elvss(lcd, elvss_data);
	s6e3fa0_read_tset(lcd, tset_data);
	s6e3fa0_read_hbm(lcd, hbm_data);

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd, mtp_data);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_tset_table(lcd, tset_data);
	ret += init_hbm_parameter(lcd, mtp_data, hbm_data);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	update_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", __FILE__);

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

static int s6e3fa0_init(void)
{
	return 0 ;
#if 0
	s5p_mipi_dsi_register_lcd_driver(&s6e3fa0_mipi_lcd_driver);
	exynos_mipi_dsi_register_lcd_driver
#endif
}

static void s6e3fa0_exit(void)
{
	return;
}

module_init(s6e3fa0_init);
module_exit(s6e3fa0_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E3FA0 (1080*1920) Panel Driver");
MODULE_LICENSE("GPL");
