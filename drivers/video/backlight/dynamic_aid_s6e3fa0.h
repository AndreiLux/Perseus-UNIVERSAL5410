#ifndef __DYNAMIC_AID_XXXX_H
#define __DYNAMIC_AID_XXXX_H __FILE__

#include "dynamic_aid.h"
#include "dynamic_aid_gamma_curve.h"

enum {
	IV_VT,
	IV_3,
	IV_11,
	IV_23,
	IV_35,
	IV_51,
	IV_87,
	IV_151,
	IV_203,
	IV_255,
	IV_MAX,
};

enum {
	IBRIGHTNESS_5NT,
	IBRIGHTNESS_6NT,
	IBRIGHTNESS_7NT,
	IBRIGHTNESS_8NT,
	IBRIGHTNESS_9NT,
	IBRIGHTNESS_10NT,
	IBRIGHTNESS_11NT,
	IBRIGHTNESS_12NT,
	IBRIGHTNESS_13NT,
	IBRIGHTNESS_14NT,
	IBRIGHTNESS_15NT,
	IBRIGHTNESS_16NT,
	IBRIGHTNESS_17NT,
	IBRIGHTNESS_19NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_21NT,
	IBRIGHTNESS_22NT,
	IBRIGHTNESS_24NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_27NT,
	IBRIGHTNESS_29NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_32NT,
	IBRIGHTNESS_34NT,
	IBRIGHTNESS_37NT,
	IBRIGHTNESS_39NT,
	IBRIGHTNESS_41NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_350NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6300	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] =
{
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255		/* IV_255 */
};

static const int index_brightness_table[IBRIGHTNESS_MAX] =
{
	5,	/* IBRIGHTNESS_5NT */
	6,	/* IBRIGHTNESS_6NT */
	7,	/* IBRIGHTNESS_7NT */
	8,	/* IBRIGHTNESS_8NT */
	9,	/* IBRIGHTNESS_9NT */
	10,	/* IBRIGHTNESS_10NT */
	11,	/* IBRIGHTNESS_11NT */
	12,	/* IBRIGHTNESS_12NT */
	13,	/* IBRIGHTNESS_13NT */
	14,	/* IBRIGHTNESS_14NT */
	15,	/* IBRIGHTNESS_15NT */
	16,	/* IBRIGHTNESS_16NT */
	17,	/* IBRIGHTNESS_17NT */
	19,	/* IBRIGHTNESS_19NT */
	20,	/* IBRIGHTNESS_20NT */
	21,	/* IBRIGHTNESS_21NT */
	22,	/* IBRIGHTNESS_22NT */
	24,	/* IBRIGHTNESS_24NT */
	25,	/* IBRIGHTNESS_25NT */
	27,	/* IBRIGHTNESS_27NT */
	29,	/* IBRIGHTNESS_29NT */
	30,	/* IBRIGHTNESS_30NT */
	32,	/* IBRIGHTNESS_32NT */
	34,	/* IBRIGHTNESS_34NT */
	37,	/* IBRIGHTNESS_37NT */
	39,	/* IBRIGHTNESS_39NT */
	41,	/* IBRIGHTNESS_41NT */
	44,	/* IBRIGHTNESS_44NT */
	47,	/* IBRIGHTNESS_47NT */
	50,	/* IBRIGHTNESS_50NT */
	53,	/* IBRIGHTNESS_53NT */
	56,	/* IBRIGHTNESS_56NT */
	60,	/* IBRIGHTNESS_60NT */
	64,	/* IBRIGHTNESS_64NT */
	68,	/* IBRIGHTNESS_68NT */
	72,	/* IBRIGHTNESS_72NT */
	77,	/* IBRIGHTNESS_77NT */
	82,	/* IBRIGHTNESS_82NT */
	87,	/* IBRIGHTNESS_87NT */
	93,	/* IBRIGHTNESS_93NT */
	98,	/* IBRIGHTNESS_98NT */
	105,	/* IBRIGHTNESS_105NT */
	111,	/* IBRIGHTNESS_111NT */
	119,	/* IBRIGHTNESS_119NT */
	126,	/* IBRIGHTNESS_126NT */
	134,	/* IBRIGHTNESS_134NT */
	143,	/* IBRIGHTNESS_143NT */
	152,	/* IBRIGHTNESS_152NT */
	162,	/* IBRIGHTNESS_162NT */
	172,	/* IBRIGHTNESS_172NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] =
{
	0x00, 0x00, 0x00,		/* IV_VT */
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100,	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] =
{
	{0, 860},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{72, 860},	/* IV_255 */
};

static const int vt_voltage_value[] =
{0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186};

static const int brightness_base_table[IBRIGHTNESS_MAX] =
{
	110,	/* IBRIGHTNESS_5NT */
	110,	/* IBRIGHTNESS_6NT */
	110,	/* IBRIGHTNESS_7NT */
	110,	/* IBRIGHTNESS_8NT */
	110,	/* IBRIGHTNESS_9NT */
	110,	/* IBRIGHTNESS_10NT */
	110,	/* IBRIGHTNESS_11NT */
	110,	/* IBRIGHTNESS_12NT */
	110,	/* IBRIGHTNESS_13NT */
	110,	/* IBRIGHTNESS_14NT */
	110,	/* IBRIGHTNESS_15NT */
	110,	/* IBRIGHTNESS_16NT */
	110,	/* IBRIGHTNESS_17NT */
	110,	/* IBRIGHTNESS_19NT */
	110,	/* IBRIGHTNESS_20NT */
	110,	/* IBRIGHTNESS_21NT */
	110,	/* IBRIGHTNESS_22NT */
	110,	/* IBRIGHTNESS_24NT */
	110,	/* IBRIGHTNESS_25NT */
	110,	/* IBRIGHTNESS_27NT */
	110,	/* IBRIGHTNESS_29NT */
	110,	/* IBRIGHTNESS_30NT */
	110,	/* IBRIGHTNESS_32NT */
	110,	/* IBRIGHTNESS_34NT */
	110,	/* IBRIGHTNESS_37NT */
	110,	/* IBRIGHTNESS_39NT */
	110,	/* IBRIGHTNESS_41NT */
	110,	/* IBRIGHTNESS_44NT */
	110,	/* IBRIGHTNESS_47NT */
	110,	/* IBRIGHTNESS_50NT */
	110,	/* IBRIGHTNESS_53NT */
	110,	/* IBRIGHTNESS_56NT */
	110,	/* IBRIGHTNESS_60NT */
	110,	/* IBRIGHTNESS_64NT */
	110,	/* IBRIGHTNESS_68NT */
	110,	/* IBRIGHTNESS_72NT */
	110,	/* IBRIGHTNESS_77NT */
	110,	/* IBRIGHTNESS_82NT */
	110,	/* IBRIGHTNESS_87NT */
	110,	/* IBRIGHTNESS_93NT */
	110,	/* IBRIGHTNESS_98NT */
	110,	/* IBRIGHTNESS_105NT */
	185,	/* IBRIGHTNESS_111NT */
	199,	/* IBRIGHTNESS_119NT */
	207,	/* IBRIGHTNESS_126NT */
	220,	/* IBRIGHTNESS_134NT */
	228,	/* IBRIGHTNESS_143NT */
	242,	/* IBRIGHTNESS_152NT */
	255,	/* IBRIGHTNESS_162NT */
	268,	/* IBRIGHTNESS_172NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] =
{
	gamma_curve_2p15_table,	/* IBRIGHTNESS_5NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_6NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_7NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_8NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_9NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_10NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_11NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_12NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_13NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_14NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_15NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_16NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_17NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_19NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_24NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_30NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_32NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_34NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_37NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_39NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_41NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_44NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_47NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_50NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_53NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_56NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_60NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_64NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_68NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_72NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_77NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_82NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_87NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_93NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_98NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_111NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_172NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_183NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_195NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_207NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_220NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_234NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_249NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_265NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_282NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_300NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table, /* IBRIGHTNESS_350NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][3] =
{
	{0x48, 0x07, 0x31}, /* IBRIGHTNESS_5NT */
	{0x48, 0x07, 0x22}, /* IBRIGHTNESS_6NT */
	{0x48, 0x07, 0x11}, /* IBRIGHTNESS_7NT */
	{0x48, 0x07, 0x00}, /* IBRIGHTNESS_8NT */
	{0x48, 0x06, 0xEF}, /* IBRIGHTNESS_9NT */
	{0x48, 0x06, 0xDE}, /* IBRIGHTNESS_10NT */
	{0x48, 0x06, 0xCD}, /* IBRIGHTNESS_11NT */
	{0x48, 0x06, 0xBC}, /* IBRIGHTNESS_12NT */
	{0x48, 0x06, 0xAC}, /* IBRIGHTNESS_13NT */
	{0x48, 0x06, 0x9B}, /* IBRIGHTNESS_14NT */
	{0x48, 0x06, 0x8B}, /* IBRIGHTNESS_15NT */
	{0x48, 0x06, 0x7A}, /* IBRIGHTNESS_16NT */
	{0x48, 0x06, 0x69}, /* IBRIGHTNESS_17NT */
	{0x48, 0x06, 0x48}, /* IBRIGHTNESS_19NT */
	{0x48, 0x06, 0x38}, /* IBRIGHTNESS_20NT */
	{0x48, 0x06, 0x26}, /* IBRIGHTNESS_21NT */
	{0x48, 0x06, 0x14}, /* IBRIGHTNESS_22NT */
	{0x48, 0x05, 0xF1}, /* IBRIGHTNESS_24NT */
	{0x48, 0x05, 0xE0}, /* IBRIGHTNESS_25NT */
	{0x48, 0x05, 0xBC}, /* IBRIGHTNESS_27NT */
	{0x48, 0x05, 0x99}, /* IBRIGHTNESS_29NT */
	{0x48, 0x05, 0x88}, /* IBRIGHTNESS_30NT */
	{0x48, 0x05, 0x64}, /* IBRIGHTNESS_32NT */
	{0x48, 0x05, 0x41}, /* IBRIGHTNESS_34NT */
	{0x48, 0x05, 0x0C}, /* IBRIGHTNESS_37NT */
	{0x48, 0x04, 0xE9}, /* IBRIGHTNESS_39NT */
	{0x48, 0x04, 0xC5}, /* IBRIGHTNESS_41NT */
	{0x48, 0x04, 0x8C}, /* IBRIGHTNESS_44NT */
	{0x48, 0x04, 0x54}, /* IBRIGHTNESS_47NT */
	{0x48, 0x04, 0x1C}, /* IBRIGHTNESS_50NT */
	{0x48, 0x03, 0xE5}, /* IBRIGHTNESS_53NT */
	{0x48, 0x03, 0xAE}, /* IBRIGHTNESS_56NT */
	{0x48, 0x03, 0x65}, /* IBRIGHTNESS_60NT */
	{0x48, 0x03, 0x01}, /* IBRIGHTNESS_64NT */
	{0x48, 0x02, 0xB2}, /* IBRIGHTNESS_68NT */
	{0x48, 0x02, 0x76}, /* IBRIGHTNESS_72NT */
	{0x48, 0x02, 0x15}, /* IBRIGHTNESS_77NT */
	{0x48, 0x01, 0xB3}, /* IBRIGHTNESS_82NT */
	{0x48, 0x01, 0x50}, /* IBRIGHTNESS_87NT */
	{0x48, 0x00, 0xE5}, /* IBRIGHTNESS_93NT */
	{0x48, 0x00, 0x77}, /* IBRIGHTNESS_98NT */
	{0x48, 0x00, 0x08}, /* IBRIGHTNESS_105NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_111NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_119NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_126NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_134NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_143NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_152NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_162NT */
	{0x48, 0x03, 0x06}, /* IBRIGHTNESS_172NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_183NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_195NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_207NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_220NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_234NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_249NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_265NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_282NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_300NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_316NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_333NT */
	{0x48, 0x00, 0x06}, /* IBRIGHTNESS_350NT */
};

const unsigned char (*paor_cmd)[3] = aor_cmd;

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{0, 35, 34, 31, 27, 25, 20, 13, 6, 0},  /* IBRIGHTNESS_5NT */
	{0, 32, 31, 28, 25, 22, 18, 12, 6, 0},  /* IBRIGHTNESS_6NT */
	{0, 30, 28, 26, 24, 20, 17, 11, 6, 0},  /* IBRIGHTNESS_7NT */
	{0, 28, 26, 24, 22, 19, 16, 10, 5, 0},  /* IBRIGHTNESS_8NT */
	{0, 26, 24, 22, 19, 18, 15, 9, 5, 0},  /* IBRIGHTNESS_9NT */
	{0, 25, 24, 21, 18, 17, 14, 9, 5, 0},  /* IBRIGHTNESS_10NT */
	{0, 24, 22, 20, 17, 16, 14, 9, 5, 0},  /* IBRIGHTNESS_11NT */
	{0, 23, 21, 19, 17, 15, 13, 9, 5, 0},  /* IBRIGHTNESS_12NT */
	{0, 22, 21, 18, 16, 15, 13, 9, 4, 0},  /* IBRIGHTNESS_13NT */
	{0, 21, 20, 17, 15, 14, 12, 9, 4, 0},  /* IBRIGHTNESS_14NT */
	{0, 21, 20, 17, 15, 13, 12, 8, 4, 0},  /* IBRIGHTNESS_15NT */
	{0, 20, 19, 16, 14, 12, 11, 8, 4, 0},  /* IBRIGHTNESS_16NT */
	{0, 19, 18, 16, 13, 12, 11, 8, 3, 0},  /* IBRIGHTNESS_17NT */
	{0, 18, 17, 14, 13, 11, 10, 8, 3, 0},  /* IBRIGHTNESS_19NT */
	{0, 17, 16, 14, 12, 11, 10, 8, 3, 0},  /* IBRIGHTNESS_20NT */
	{0, 16, 15, 13, 11, 10, 8, 6, 3, 0},  /* IBRIGHTNESS_21NT */
	{0, 16, 15, 13, 11, 9, 8, 6, 3, 0},  /* IBRIGHTNESS_22NT */
	{0, 15, 14, 12, 10, 9, 8, 6, 3, 0},  /* IBRIGHTNESS_24NT */
	{0, 14, 13, 11, 10, 8, 8, 6, 3, 0},  /* IBRIGHTNESS_25NT */
	{0, 13, 12, 11, 9, 8, 8, 5, 3, 0},  /* IBRIGHTNESS_27NT */
	{0, 13, 12, 11, 9, 7, 7, 5, 3, 0},  /* IBRIGHTNESS_29NT */
	{0, 13, 12, 10, 8, 8, 6, 5, 3, 0},  /* IBRIGHTNESS_30NT */
	{0, 12, 11, 10, 8, 7, 6, 5, 3, 0},  /* IBRIGHTNESS_32NT */
	{0, 11, 10, 9, 7, 7, 6, 5, 3, 0},  /* IBRIGHTNESS_34NT */
	{0, 10, 9, 8, 7, 6, 5, 4, 3, 0},  /* IBRIGHTNESS_37NT */
	{0, 10, 9, 8, 6, 6, 5, 4, 3, 0},  /* IBRIGHTNESS_39NT */
	{0, 10, 9, 7, 6, 6, 4, 4, 3, 0},  /* IBRIGHTNESS_41NT */
	{0, 9, 8, 7, 6, 6, 4, 4, 3, 0},  /* IBRIGHTNESS_44NT */
	{0, 8, 7, 6, 5, 5, 3, 3, 2, 0},  /* IBRIGHTNESS_47NT */
	{0, 8, 7, 6, 5, 5, 3, 3, 2, 0},  /* IBRIGHTNESS_50NT */
	{0, 8, 7, 6, 5, 5, 3, 3, 2, 0},  /* IBRIGHTNESS_53NT */
	{0, 7, 6, 5, 4, 4, 2, 2, 2, 0},  /* IBRIGHTNESS_56NT */
	{0, 7, 6, 5, 4, 4, 2, 2, 2, 0},  /* IBRIGHTNESS_60NT */
	{0, 6, 5, 5, 4, 4, 2, 2, 2, 0},  /* IBRIGHTNESS_64NT */
	{0, 6, 5, 4, 3, 4, 2, 2, 2, 0},  /* IBRIGHTNESS_68NT */
	{0, 5, 4, 4, 3, 3, 2, 2, 1, 0},  /* IBRIGHTNESS_72NT */
	{0, 5, 4, 4, 3, 3, 2, 2, 1, 0},  /* IBRIGHTNESS_77NT */
	{0, 5, 4, 3, 3, 3, 2, 2, 1, 0},  /* IBRIGHTNESS_82NT */
	{0, 4, 4, 3, 2, 3, 2, 2, 1, 0},  /* IBRIGHTNESS_87NT */
	{0, 3, 2, 2, 1, 2, 1, 1, 1, 0},  /* IBRIGHTNESS_93NT */
	{0, 4, 3, 2, 1, 2, 1, 1, 1, 0},  /* IBRIGHTNESS_98NT */
	{0, 4, 3, 2, 2, 2, 1, 1, 1, 1},  /* IBRIGHTNESS_105NT */
	{0, 5, 5, 4, 3, 3, 3, 3, 1, 0},  /* IBRIGHTNESS_111NT */
	{0, 5, 5, 4, 3, 3, 3, 3, 1, 0},  /* IBRIGHTNESS_119NT */
	{0, 5, 5, 4, 3, 3, 3, 3, 1, 0},  /* IBRIGHTNESS_126NT */
	{0, 5, 4, 3, 3, 3, 3, 3, 1, 0},  /* IBRIGHTNESS_134NT */
	{0, 5, 4, 3, 3, 3, 3, 3, 1, 0},  /* IBRIGHTNESS_143NT */
	{0, 4, 4, 3, 3, 3, 3, 3, 1, 0},  /* IBRIGHTNESS_152NT */
	{0, 4, 4, 3, 3, 3, 3, 3, 2, 0},  /* IBRIGHTNESS_162NT */
	{0, 3, 3, 2, 2, 2, 2, 2, 1, 0},  /* IBRIGHTNESS_172NT */
	{0, 1, 1, 1, 1, 2, 2, 2, 1, 3},  /* IBRIGHTNESS_183NT */
	{0, 1, 1, 1, 1, 1, 2, 2, 1, 3},  /* IBRIGHTNESS_195NT */
	{0, 0, 1, 1, 1, 2, 2, 2, 1, 3},  /* IBRIGHTNESS_207NT */
	{0, 0, 0, 0, 0, 1, 1, 1, 1, 3},  /* IBRIGHTNESS_220NT */
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 2},  /* IBRIGHTNESS_234NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 2},  /* IBRIGHTNESS_249NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  /* IBRIGHTNESS_316NT */
	{0, 0, -1, -1, -1, -1, 0, 0, 0, 1},  /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* IBRIGHTNESS_350NT */
};

static const struct rgb_t offset_color[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -5}},{{-5, 3, -3}},{{-7, 3, -4}},{{-9, 4, -8}},{{-7, 3, -8}},{{-3, 1, -4}},{{-2, 1, -2}},{{-4, 0, -3}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -5}},{{-5, 3, -3}},{{-7, 3, -4}},{{-9, 4, -8}},{{-6, 3, -6}},{{-3, 1, -4}},{{-2, 1, -2}},{{-3, 0, -3}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -6}},{{-5, 3, -3}},{{-7, 3, -4}},{{-9, 3, -8}},{{-6, 2, -6}},{{-3, 1, -4}},{{-2, 1, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -7}},{{-5, 4, -4}},{{-6, 3, -4}},{{-8, 3, -7}},{{-6, 2, -7}},{{-2, 1, -3}},{{-2, 1, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -8}},{{-5, 4, -4}},{{-6, 3, -4}},{{-8, 2, -7}},{{-6, 1, -7}},{{-2, 1, -3}},{{-1, 1, -1}},{{-1, 0, -1}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -8}},{{-5, 3, -4}},{{-6, 3, -4}},{{-8, 2, -7}},{{-6, 1, -7}},{{-2, 1, -3}},{{-1, 1, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -8}},{{-6, 4, -5}},{{-6, 2, -4}},{{-8, 2, -7}},{{-6, 1, -6}},{{-2, 1, -3}},{{-1, 1, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -8}},{{-6, 4, -5}},{{-6, 2, -4}},{{-7, 2, -6}},{{-5, 1, -5}},{{-2, 1, -2}},{{-1, 1, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 4, -8}},{{-6, 4, -6}},{{-6, 2, -4}},{{-7, 2, -6}},{{-5, 1, -5}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 4, -8}},{{-6, 3, -7}},{{-6, 2, -5}},{{-7, 1, -6}},{{-5, 1, -5}},{{-1, 1, -2}},{{-1, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 4, -8}},{{-6, 3, -8}},{{-6, 1, -5}},{{-7, 1, -6}},{{-5, 1, -5}},{{-1, 1, -2}},{{-1, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-6, 3, -8}},{{-6, 1, -5}},{{-7, 1, -6}},{{-4, 1, -5}},{{-1, 1, -2}},{{-1, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-6, 3, -8}},{{-6, 1, -5}},{{-6, 1, -6}},{{-4, 1, -4}},{{-1, 1, -2}},{{-1, 0, -1}},{{0, 0, 0}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-6, 3, -8}},{{-6, 1, -5}},{{-6, 1, -5}},{{-4, 1, -4}},{{-1, 1, -2}},{{-1, 0, -1}},{{0, 0, 0}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 5, -8}},{{-6, 3, -8}},{{-6, 1, -5}},{{-6, 1, -5}},{{-4, 1, -4}},{{-1, 1, -2}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, 5, -8}},{{-6, 3, -8}},{{-6, 1, -5}},{{-7, 1, -5}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, 5, -8}},{{-6, 2, -8}},{{-6, 1, -4}},{{-7, 1, -5}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, 5, -8}},{{-6, 2, -8}},{{-6, 1, -4}},{{-6, 1, -5}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, 5, -8}},{{-6, 2, -8}},{{-5, 1, -4}},{{-6, 1, -5}},{{-2, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 5, -8}},{{-6, 2, -8}},{{-5, 1, -4}},{{-6, 1, -5}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 5, -8}},{{-6, 2, -8}},{{-5, 1, -4}},{{-5, 1, -4}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-6, 1, -8}},{{-5, 1, -4}},{{-4, 1, -4}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-6, 1, -8}},{{-5, 1, -4}},{{-4, 1, -4}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-6, 1, -7}},{{-4, 1, -4}},{{-4, 1, -4}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 4, -8}},{{-5, 1, -7}},{{-4, 1, -4}},{{-4, 1, -3}},{{-1, 1, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 4, -8}},{{-5, 1, -6}},{{-3, 1, -4}},{{-4, 1, -3}},{{-1, 1, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 4, -8}},{{-5, 1, -6}},{{-4, 1, -3}},{{-4, 1, -3}},{{-1, 1, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 4, -8}},{{-5, 1, -6}},{{-4, 1, -3}},{{-4, 1, -3}},{{-1, 1, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 3, -8}},{{-4, 1, -5}},{{-3, 1, -2}},{{-3, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 3, -8}},{{-4, 1, -5}},{{-3, 1, -3}},{{-3, 1, -2}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 3, -8}},{{-4, 1, -5}},{{-3, 1, -2}},{{-3, 0, -2}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 3, -8}},{{-4, 1, -5}},{{-3, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 2, -8}},{{-3, 1, -4}},{{-3, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 2, -8}},{{-3, 1, -4}},{{-3, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 2, -7}},{{-2, 1, -3}},{{-2, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 2, -6}},{{-2, 1, -3}},{{-2, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 2, -6}},{{-2, 1, -3}},{{-2, 0, -2}},{{-2, 0, -1}},{{0, 1, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -6}},{{-2, 0, -2}},{{-1, 0, -2}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -6}},{{-1, 0, -2}},{{-1, 0, -2}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -6}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -5}},{{0, 0, -1}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 1, -5}},{{0, 0, -1}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 2}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -5}},{{-4, 0, -4}},{{-3, 0, -2}},{{-2, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -6}},{{-4, 0, -4}},{{-2, 0, -2}},{{-2, 0, -3}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -6}},{{-3, 0, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -5}},{{-3, 0, -3}},{{-2, 0, -1}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -5}},{{-3, 0, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -5}},{{-2, 0, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -5}},{{-2, 1, -2}},{{-1, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -5}},{{-2, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_350NT */
};
#endif /* __DYNAMIC_AID_XXXX_H */
