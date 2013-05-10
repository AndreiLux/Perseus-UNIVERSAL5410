/* linux/drivers/video/samsung/smartdimming.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com

 * Samsung Smart Dimming for OCTA
 *
 * Minwoo Kim, <minwoo7945.kim@samsung.com>
 *
*/

#include "smart_dimming_s6e8fa0_G.h"
#include "s6e8fa0_G_volt_tbl.h"

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#ifdef FINAL_SMART_DIMMING_EXCEL_CHECK
#define smtd_info(format, arg...)	printk(format, ##arg)
#else
#define smtd_info(format, arg...)
#endif

#define VALUE_DIM_1000	1000
#define VT_255_div_1000	860000
#define VREG_OUT_1000		6300
#define VT_255_calc_param	(VT_255_div_1000 / VREG_OUT_1000)

static const u8 range_table_count[IV_TABLE_MAX] = {
	3, 8, 12, 12, 16, 36, 64, 52, 52, 1
};

static const u32 table_radio[IV_TABLE_MAX] = {
	16384, 4138, 2763, 2763, 2080, 918, 516, 636, 636, 0
};

static const u32 dv_value[IV_MAX] = {
	0, 3, 11, 23, 35, 51, 87, 151, 203, 255
};

static const char color_name[3] = {'R', 'G', 'B'};

static const u8 *offset_table[IV_TABLE_MAX] = {
	NULL, /*V3*/
	NULL, /*V11*/
	NULL, /*V23*/
	NULL, /*V23*/
	NULL, /*V35*/
	NULL, /*V51*/
	NULL, /*V87*/
	NULL, /*V151*/
	NULL, /*V203*/
	NULL  /*V255*/
};

static const unsigned char gamma_300cd_00[] = {
	0x01, 0x00, 0x01,
	0x00, 0x01, 0x00,	/*VT255*/
	0x80, 0x80, 0x80,	/*VT203*/
	0x80, 0x80, 0x80,	/*VT151*/
	0x80, 0x80, 0x80,	/*VT87*/
	0x80, 0x80, 0x80,	/*VT51*/
	0x80, 0x80, 0x80,	/*VT35*/
	0x80, 0x80, 0x80,	/*VT23*/
	0x80, 0x80, 0x80,	/*VT11*/
	0x80, 0x80, 0x80,	/*VT3*/
	0x00, 0x00, 0x00	/*VT*/
};

static const unsigned char *gamma_300cd_list[GAMMA_300CD_MAX] = {
	gamma_300cd_00,
};


static u32 calc_vt_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 ret = 0;

	ret = volt_table_vt_G[gamma] >> 16;

	return ret;
}

static u32 calc_v3_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 64, DV :320 */
	int ret = 0;
	u32 v0 = VREG_OUT_1000, v11;
	u32 ratio = 0;

	/*vt = adjust_volt[rgb_index][AD_IVT];*/
	v11 = adjust_volt[rgb_index][AD_IV11];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (v0 << 16) - ((v0-v11)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v11_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 64, DV :320*/
	int ret = 0;
	u32 vt, v23;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v23 = adjust_volt[rgb_index][AD_IV23];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v23)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v23_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 64, DV :319 */
	int ret = 0;
	u32 vt, v35;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v35 = adjust_volt[rgb_index][AD_IV35];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v35)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v35_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 65, DV :319 */
	int ret = 0;
	u32 vt, v51;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v51 = adjust_volt[rgb_index][AD_IV51];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v51)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v51_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 65, DV :319 */
	int ret = 0;
	u32 vt, v87;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v87 = adjust_volt[rgb_index][AD_IV87];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v87)*ratio);
	ret = ret >> 16;

	return ret;

}

static u32 calc_v87_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 64, DV :319 */
	int ret = 0;
	u32 vt, v151;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v151 = adjust_volt[rgb_index][AD_IV151];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v151)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v151_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 64, DV :319 */
	int ret = 0;
	u32 vt, v203;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v203 = adjust_volt[rgb_index][AD_IV203];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v203)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v203_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	/* for CV : 64, DV :319 */
	int ret = 0;
	u32 vt, v255;
	u32 ratio = 0;

	vt = adjust_volt[rgb_index][AD_IVT];
	v255 = adjust_volt[rgb_index][AD_IV255];
	ratio = volt_table_cv_64_dv_320_G[gamma];

	ret = (vt << 16) - ((vt-v255)*ratio);
	ret = ret >> 16;

	return ret;
}

static u32 calc_v255_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 ret = 0;

	ret = volt_table_v255_G[gamma] >> 16;

	return ret;
}

u8 calc_voltage_table_G(struct str_smart_dim *smart, const u8 *mtp)
{
	int c, i, j;
#if defined(MTP_REVERSE)
	int offset1 = 0;
#endif
	int offset = 0;
	s16 t1, t2;
	s16 adjust_mtp[CI_MAX][IV_MAX];
	u8 range_index;
	u8 table_index = 0;
	u32 v1, v2;
	u32 ratio;
	u32(*calc_volt[IV_MAX])(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX]) = {
		calc_vt_volt,
		calc_v3_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	u8 calc_seq[9] = {IV_VT,  IV_203, IV_151, IV_87, IV_51, IV_35, IV_23, IV_11, IV_3};
	u8 ad_seq[9] = {AD_IVT, AD_IV203, AD_IV151, AD_IV87, AD_IV51, AD_IV35, AD_IV23, AD_IV11, AD_IV3};

	memset(adjust_mtp, 0, sizeof(adjust_mtp));

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = c*2;
		if (mtp[offset] & 0x01)
			t1 = mtp[offset + 1] * -1;
		else
			t1 = mtp[offset + 1];
		t2 = (smart->default_gamma[offset]<<8|smart->default_gamma[offset+1]) + t1;
		smart->mtp[c][IV_255] = t1;
		adjust_mtp[c][IV_255] = t2;
		smart->adjust_volt[c][AD_IV255] = calc_volt[IV_255](t2, c, smart->adjust_volt);
		/* for V0 All RGB Voltage Value is Reference Voltage */
		smart->adjust_volt[c][AD_IVT] = VREG_OUT_1000;
	}

	for (i = IV_VT; i < IV_255; i++) {
		for (c = CI_RED; c < CI_MAX; c++) {
			if (mtp[CI_MAX*(10-calc_seq[i])+c] & 0x80)
				t1 = (mtp[CI_MAX*(10-calc_seq[i])+c] & 0x7f) * (-1);
			else
				t1 = (mtp[CI_MAX*(10-calc_seq[i])+c] & 0x7f);
			t2 = smart->default_gamma[CI_MAX*(10-calc_seq[i])+c] + t1;
			smart->mtp[c][calc_seq[i]] = t1;
			adjust_mtp[c][calc_seq[i]] = t2;
			smart->adjust_volt[c][ad_seq[i]] = calc_volt[calc_seq[i]](t2, c, smart->adjust_volt);
		}
	}

	for (i = AD_IVT; i < AD_IVMAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++) {
			if (i == 0)
				smart->ve[table_index].v[c] = VREG_OUT_1000;
			else
				smart->ve[table_index].v[c] = smart->adjust_volt[c][i];
		}
		range_index = 0;

		 for (j = table_index+1; j < table_index+range_table_count[i]; j++) {
			for (c = CI_RED; c < CI_MAX; c++) {
				if (smart->t_info[i].offset_table != NULL)
					ratio = smart->t_info[i].offset_table[range_index] * smart->t_info[i].rv;
				else
					ratio = (range_table_count[i]-(range_index+1)) * smart->t_info[i].rv;

				v1 = smart->adjust_volt[c][i+1] << 15;
				v2 = (smart->adjust_volt[c][i] - smart->adjust_volt[c][i+1])*ratio;
				smart->ve[j].v[c] = ((v1+v2) >> 15);
			}
			range_index++;
		}
		table_index = j;
	}

	for (i = 1; i < 3; i++) {
		for (c = CI_RED; c < CI_MAX; c++)
			smart->ve[i].v[c] = smart->ve[3].v[c]+((smart->ve[0].v[c]-smart->ve[3].v[c])*(3-i)/3);
	}

	smtd_info("++++++++++++++++++++++++++++++ MTP VALUE ++++++++++++++++++++++++++++++\n");
	for (i = IV_VT; i < IV_MAX; i++) {
		smtd_info("V Level : %d - ", i);
		for (c = CI_RED; c < CI_MAX; c++)
			smtd_info("%c : 0x%08x(%04d)", color_name[c], smart->mtp[c][i], smart->mtp[c][i]);
		smtd_info("\n");
	}

	smtd_dbg("\n\n++++++++++++++++++++++++++++++ ADJUST VALUE ++++++++++++++++++++++++++++++\n");
	for (i = IV_VT; i < IV_MAX; i++) {
		smtd_dbg("V Level : %d - ", i);
		for (c = CI_RED; c < CI_MAX; c++)
			smtd_dbg("%c : 0x%08x(%04d)", color_name[c], adjust_mtp[c][i], adjust_mtp[c][i]);
		smtd_dbg("\n");
	}

	smtd_dbg("\n\n++++++++++++++++++++++++++++++ ADJUST VOLTAGE ++++++++++++++++++++++++++++++\n");
	for (i = AD_IVT; i < AD_IVMAX; i++) {
		smtd_dbg("V Level : %d - ", i);
		for (c = CI_RED; c < CI_MAX; c++)
			smtd_dbg("%c : %04dV", color_name[c], smart->adjust_volt[c][i]);
		smtd_dbg("\n");
	}

	smtd_dbg("\n\n++++++++++++++++++++++++++++++++++++++  VOLTAGE TABLE ++++++++++++++++++++++++++++++++++++++\n");
	for (i = 0; i < 256; i++) {
		smtd_dbg("Gray Level : %03d - ", i);
		for (c = CI_RED; c < CI_MAX; c++)
			smtd_dbg("%c : %04dV", color_name[c], smart->ve[i].v[c]);
		smtd_dbg("\n");
	}

	return 0;
}


int init_table_info_G(struct str_smart_dim *smart)
{
	int i;
	int offset = 0;

	for (i = 0; i < IV_TABLE_MAX; i++) {
		smart->t_info[i].count = (u8)range_table_count[i];
		smart->t_info[i].offset_table = offset_table[i];
		smart->t_info[i].rv = table_radio[i];
		offset += range_table_count[i];
	}

	smart->flooktbl = flookup_table_G;
	smart->g300_gra_tbl = gamma_300_gra_table_G;
	smart->gamma_table[G_195] = GAMMA_CONTROL_TABLE_G[G_195];
	smart->gamma_table[G_200] = GAMMA_CONTROL_TABLE_G[G_200];
	smart->gamma_table[G_205] = GAMMA_CONTROL_TABLE_G[G_205];
	smart->gamma_table[G_210] = GAMMA_CONTROL_TABLE_G[G_210];
	smart->gamma_table[G_215] = GAMMA_CONTROL_TABLE_G[G_215];
	smart->gamma_table[G_220] = GAMMA_CONTROL_TABLE_G[G_220];

	smart->default_gamma = gamma_300cd_list[0];

	return 0;
}

static u32 lookup_vtbl_idx(struct str_smart_dim *smart, u32 gamma)
{
	u32 lookup_index;
	u16 table_count, table_index;
	u32 gap, i;
	u32 minimum = smart->g300_gra_tbl[255];
	u32 candidate = 0;
	u32 offset = 0;

	lookup_index = (gamma/VALUE_DIM_1000)+1;
	if (lookup_index > MAX_GRADATION) {
		/*printk(KERN_ERR "ERROR Wrong input value  LOOKUP INDEX : %d\n", lookup_index);*/
		return 0;
	}

	if (smart->flooktbl[lookup_index].count) {
		if (smart->flooktbl[lookup_index-1].count) {
			table_index = smart->flooktbl[lookup_index-1].entry;
			table_count = smart->flooktbl[lookup_index].count + smart->flooktbl[lookup_index-1].count;
		} else {
			table_index = smart->flooktbl[lookup_index].entry;
			table_count = smart->flooktbl[lookup_index].count;
		}
	} else {
		offset += 1;
		while (!(smart->flooktbl[lookup_index+offset].count || smart->flooktbl[lookup_index-offset].count))
			offset++;

		if (smart->flooktbl[lookup_index-offset].count)
			table_index = smart->flooktbl[lookup_index-offset].entry;
		else
			table_index = smart->flooktbl[lookup_index+offset].entry;

		table_count = smart->flooktbl[lookup_index+offset].count + smart->flooktbl[lookup_index-offset].count;
	}

	for (i = 0; i < table_count; i++) {
		if (gamma > smart->g300_gra_tbl[table_index])
			gap = gamma - smart->g300_gra_tbl[table_index];
		else
			gap = smart->g300_gra_tbl[table_index] - gamma;

		if (gap == 0) {
			candidate = table_index;
			break;
		}

		if (gap <= minimum) {
			minimum = gap;
			candidate = table_index;
		}
		table_index++;
	}

	smtd_dbg("gamma: %d, found index: %d, found gamma: %d\n", gamma, candidate, smart->g300_gra_tbl[candidate]);

	return candidate;
}

static u32 calc_vt_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	return 0;
}

static u32 calc_v3_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 v0 = VREG_OUT_1000, v3, v11;
	u32 ret;

	/*v0 = dv[ci][IV_0];*/
	v3 = dv[ci][IV_3];
	v11 = dv[ci][IV_11];

	t1 = (v0 - v3) << 10;
	t2 = (v0 - v11) ? (v0 - v11) : (v0) ? v0 : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v11_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v11, v23;
	u32 ret;

	vt = adjust_volt[ci][AD_IVT];
	v11 = dv[ci][IV_11];
	v23 = dv[ci][IV_23];
	t1 = (vt - v11) << 10;
	t2 = (vt - v23) ? (vt - v23) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v23_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v23, v35;
	u32 ret;

	/*vt = dv[ci][IV_VT];*/
	vt = adjust_volt[ci][AD_IVT];
	v23 = dv[ci][IV_23];
	v35 = dv[ci][IV_35];

	t1 = (vt - v23) << 10;
	t2 = (vt - v35) ? (vt - v35) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v35_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v35, v51;
	u32 ret;

	/*vt = dv[ci][IV_VT];*/
	vt = adjust_volt[ci][AD_IVT];
	v35 = dv[ci][IV_35];
	v51 = dv[ci][IV_51];

	t1 = (vt - v35) << 10;
	t2 = (vt - v51) ? (vt - v51) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v51_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v51, v87;
	u32 ret;

	/*vt = dv[ci][IV_VT];*/
	vt = adjust_volt[ci][AD_IVT];
	v51 = dv[ci][IV_51];
	v87 = dv[ci][IV_87];

	t1 = (vt - v51) << 10;
	t2 = (vt - v87) ? (vt - v87) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}


static u32 calc_v87_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v87, v151;
	u32 ret;

	/*vt = dv[ci][IV_VT];*/
	vt = adjust_volt[ci][AD_IVT];
	v87 = dv[ci][IV_87];
	v151 = dv[ci][IV_151];

	t1 = (vt - v87) << 10;
	t2 = (vt - v151) ? (vt - v151) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v151_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v151, v203;
	u32 ret;

	/*vt = dv[ci][IV_VT];*/
	vt = adjust_volt[ci][AD_IVT];
	v151 = dv[ci][IV_151];
	v203 = dv[ci][IV_203];

	t1 = (vt - v151) << 10;
	t2 = (vt - v203) ? (vt - v203) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v203_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 t1, t2;
	u32 vt, v151, v255;
	u32 ret;

	/*vt = dv[ci][IV_VT];*/
	vt =  adjust_volt[ci][IV_VT];
	v151 = dv[ci][IV_203];
	v255 = dv[ci][IV_255];

	t1 = (vt - v151) << 10;
	t2 = (vt - v255) ? (vt - v255) : (vt) ? vt : 1;
	ret = (320 * (t1/t2)) - (64 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v255_reg(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 ret;
	u32 v255;
	v255 = dv[ci][IV_255];

	ret = (788 * 10000) - (1365 * v255);
	ret = ret / 10000;

	return ret;
}

u32 calc_gamma_table_G(struct str_smart_dim *smart, struct aid_info *aid, u8 result[], const u8 *mtp, u8 array_ct)
{
	u32 i, c, t1;
	u32 temp;
	u32 lidx;
	u32 dv[CI_MAX][IV_MAX];
	s16 gamma[CI_MAX][IV_MAX];
	u16 offset;
	u32(*calc_reg[IV_MAX])(int ci, u32 dv[CI_MAX][IV_MAX], u32 adjust_volt[CI_MAX][AD_IVMAX]) = {
		calc_vt_reg,
		calc_v3_reg,
		calc_v11_reg,
		calc_v23_reg,
		calc_v35_reg,
		calc_v51_reg,
		calc_v87_reg,
		calc_v151_reg,
		calc_v203_reg,
		calc_v255_reg,
	};

	memset(gamma, 0, sizeof(gamma));

	for (i = IV_3 ; i < IV_MAX; i++) {
		temp = (smart->gamma_table[(aid+array_ct)->gamma][dv_value[i]] * (aid+array_ct)->base_cd)/1000;
		lidx = lookup_vtbl_idx(smart, temp) + (aid+array_ct)->offsets[i - 1];

		for (c = CI_RED; c < CI_MAX; c++)
			dv[c][i] = smart->ve[lidx].v[c];
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = c*2;
		if (mtp[offset] & 0x01)
			t1 = mtp[offset + 1] * -1;
		else
			t1 = mtp[offset + 1];

		smart->mtp[c][IV_255] = t1;
	}

	for (i = IV_3; i < IV_MAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++) {
			if (mtp[CI_MAX*(i + 1)+c] & 0x80)
				t1 = (mtp[CI_MAX*(i + 1)+c] & 0x7f) * (-1);
			else
				t1 = (mtp[CI_MAX*(i + 1)+c] & 0x7f);

			smart->mtp[c][IV_255 - i] = t1;
		}
	}

	for (i = IV_3; i < IV_MAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++)
			gamma[c][i] = (s16)calc_reg[i](c, dv, smart->adjust_volt) - smart->mtp[c][i];
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = IV_255*CI_MAX+c*2;
		result[offset+1] = gamma[c][IV_255] & 0xff;
		result[offset] = (u8)((gamma[c][IV_255] >> 8) & 0xff);
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		for (i = IV_VT; i < IV_255; i++)
			result[(CI_MAX*i)+c] = gamma[c][i];
	}

	smtd_dbg("\n\n++++++++++++++++++++++++++++++ FOUND VOLTAGE ++++++++++++++++++++++++++++++\n");
	for (i = IV_VT; i < IV_MAX; i++)  {
		smtd_dbg("V Level : %d - ", i);
		for (c = CI_RED; c < CI_MAX; c++)
			smtd_dbg("%c : %04dV", color_name[c], dv[c][i]);
		smtd_dbg("\n");
	}

	smtd_dbg("\n\n++++++++++++++++++++++++++++++ FOUND REG ++++++++++++++++++++++++++++++\n");
	for (i = IV_VT; i < IV_MAX; i++) {
		smtd_dbg("V Level : %d - ", i);
		for (c = CI_RED; c < CI_MAX; c++)
			smtd_dbg("%c : %3d, 0x%2x", color_name[c], gamma[c][i], gamma[c][i]);
		smtd_dbg("\n");
	}
	return 0;
}
