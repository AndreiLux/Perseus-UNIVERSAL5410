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
	IV_MAX
};

enum {
	IBRIGHTNESS_10NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_40NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_70NT,
	IBRIGHTNESS_80NT,
	IBRIGHTNESS_90NT,
	IBRIGHTNESS_100NT,
	IBRIGHTNESS_110NT,
	IBRIGHTNESS_120NT,
	IBRIGHTNESS_130NT,
	IBRIGHTNESS_140NT,
	IBRIGHTNESS_150NT,
	IBRIGHTNESS_160NT,
	IBRIGHTNESS_170NT,
	IBRIGHTNESS_180NT,
	IBRIGHTNESS_190NT,
	IBRIGHTNESS_200NT,
	IBRIGHTNESS_210NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_230NT,
	IBRIGHTNESS_240NT,
	IBRIGHTNESS_250NT,
	IBRIGHTNESS_260NT,
	IBRIGHTNESS_270NT,
	IBRIGHTNESS_280NT,
	IBRIGHTNESS_290NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6300	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] = {
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,		/* IV_151 */
	203,		/* IV_203 */
	255		/* IV_255 */
};

static const int index_brightness_table[IBRIGHTNESS_MAX] = {
	10,	/* IBRIGHTNESS_10NT */
	20,	/* IBRIGHTNESS_20NT */
	30,	/* IBRIGHTNESS_30NT */
	40,	/* IBRIGHTNESS_40NT */
	50,	/* IBRIGHTNESS_50NT */
	60,	/* IBRIGHTNESS_60NT */
	70,	/* IBRIGHTNESS_70NT */
	80,	/* IBRIGHTNESS_80NT */
	90,	/* IBRIGHTNESS_90NT */
	100,	/* IBRIGHTNESS_100NT */
	110,	/* IBRIGHTNESS_110NT */
	120,	/* IBRIGHTNESS_120NT */
	130,	/* IBRIGHTNESS_130NT */
	140,	/* IBRIGHTNESS_140NT */
	150,	/* IBRIGHTNESS_150NT */
	160,	/* IBRIGHTNESS_160NT */
	170,	/* IBRIGHTNESS_170NT */
	180,	/* IBRIGHTNESS_180NT */
	190,	/* IBRIGHTNESS_190NT */
	200,	/* IBRIGHTNESS_200NT */
	210,	/* IBRIGHTNESS_210NT */
	220,	/* IBRIGHTNESS_220NT */
	230,	/* IBRIGHTNESS_230NT */
	240,	/* IBRIGHTNESS_240NT */
	250,	/* IBRIGHTNESS_250NT */
	260,	/* IBRIGHTNESS_260NT */
	270,	/* IBRIGHTNESS_270NT */
	280,	/* IBRIGHTNESS_280NT */
	290,	/* IBRIGHTNESS_290NT */
	300	/* IBRIGHTNESS_300NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] = {
	0x00, 0x00, 0x00,	/* IV_VT */
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] = {
	{0, 860},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{72, 860}	/* IV_255 */
};

static const int vt_voltage_value[] = {
	0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186
};

static const int brightness_base_table[2][IBRIGHTNESS_MAX] = {
	{
		110,	/* IBRIGHTNESS_10NT */
		110,	/* IBRIGHTNESS_20NT */
		110,	/* IBRIGHTNESS_30NT */
		110,	/* IBRIGHTNESS_40NT */
		110,	/* IBRIGHTNESS_50NT */
		110,	/* IBRIGHTNESS_60NT */
		110,	/* IBRIGHTNESS_70NT */
		110,	/* IBRIGHTNESS_80NT */
		110,	/* IBRIGHTNESS_90NT */
		110,	/* IBRIGHTNESS_100NT */
		170,	/* IBRIGHTNESS_110NT */
		184,	/* IBRIGHTNESS_120NT */
		199,	/* IBRIGHTNESS_130NT */
		213,	/* IBRIGHTNESS_140NT */
		227,	/* IBRIGHTNESS_150NT */
		241,	/* IBRIGHTNESS_160NT */
		256,	/* IBRIGHTNESS_170NT */
		270,	/* IBRIGHTNESS_180NT */
		190,	/* IBRIGHTNESS_190NT */
		200,	/* IBRIGHTNESS_200NT */
		210,	/* IBRIGHTNESS_210NT */
		220,	/* IBRIGHTNESS_220NT */
		230,	/* IBRIGHTNESS_230NT */
		240,	/* IBRIGHTNESS_240NT */
		250,	/* IBRIGHTNESS_250NT */
		260,	/* IBRIGHTNESS_260NT */
		270,	/* IBRIGHTNESS_270NT */
		280,	/* IBRIGHTNESS_280NT */
		290,	/* IBRIGHTNESS_290NT */
		300	/* IBRIGHTNESS_300NT */
	}, {
		110,	/* IBRIGHTNESS_10NT */
		110,	/* IBRIGHTNESS_20NT */
		110,	/* IBRIGHTNESS_30NT */
		110,	/* IBRIGHTNESS_40NT */
		110,	/* IBRIGHTNESS_50NT */
		110,	/* IBRIGHTNESS_60NT */
		110,	/* IBRIGHTNESS_70NT */
		110,	/* IBRIGHTNESS_80NT */
		110,	/* IBRIGHTNESS_90NT */
		110,	/* IBRIGHTNESS_100NT */
		180,	/* IBRIGHTNESS_110NT */
		194,	/* IBRIGHTNESS_120NT */
		208,	/* IBRIGHTNESS_130NT */
		222,	/* IBRIGHTNESS_140NT */
		235,	/* IBRIGHTNESS_150NT */
		249,	/* IBRIGHTNESS_160NT */
		263,	/* IBRIGHTNESS_170NT */
		277,	/* IBRIGHTNESS_180NT */
		190,	/* IBRIGHTNESS_190NT */
		200,	/* IBRIGHTNESS_200NT */
		210,	/* IBRIGHTNESS_210NT */
		220,	/* IBRIGHTNESS_220NT */
		230,	/* IBRIGHTNESS_230NT */
		240,	/* IBRIGHTNESS_240NT */
		250,	/* IBRIGHTNESS_250NT */
		260,	/* IBRIGHTNESS_260NT */
		270,	/* IBRIGHTNESS_270NT */
		280,	/* IBRIGHTNESS_280NT */
		290,	/* IBRIGHTNESS_290NT */
		300	/* IBRIGHTNESS_300NT */
	}
};

static const int *gamma_curve_tables[2][IBRIGHTNESS_MAX] = {
	{
		gamma_curve_2p20_table,	/* IBRIGHTNESS_10NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_20NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_30NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_40NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_50NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_60NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_70NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_90NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_100NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_110NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_120NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_130NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_140NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_150NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_160NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_170NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_180NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_190NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_200NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_210NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_220NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_230NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_240NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_250NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_260NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_270NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_280NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_290NT */
		gamma_curve_2p20_table	/* IBRIGHTNESS_300NT */
	}, {
		gamma_curve_1p90_table,	/* IBRIGHTNESS_10NT */
		gamma_curve_2p00_table,	/* IBRIGHTNESS_20NT */
		gamma_curve_2p10_table,	/* IBRIGHTNESS_30NT */
		gamma_curve_2p10_table,	/* IBRIGHTNESS_40NT */
		gamma_curve_2p10_table,	/* IBRIGHTNESS_50NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_60NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_70NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_90NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_100NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_110NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_120NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_130NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_140NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_150NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_160NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_170NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_180NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_190NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_200NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_210NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_220NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_230NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_240NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_250NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_260NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_270NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_280NT */
		gamma_curve_2p25_table,	/* IBRIGHTNESS_290NT */
		gamma_curve_2p20_table	/* IBRIGHTNESS_300NT */
	}
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[2][IBRIGHTNESS_MAX][3] = {
	{
		{0x09, 0x07, 0x01},	/* IBRIGHTNESS_10NT */
		{0x09, 0x06, 0x65},	/* IBRIGHTNESS_20NT */
		{0x09, 0x05, 0xC1},	/* IBRIGHTNESS_30NT */
		{0x09, 0x05, 0x26},	/* IBRIGHTNESS_40NT */
		{0x09, 0x04, 0x82},	/* IBRIGHTNESS_50NT */
		{0x09, 0x03, 0xD4},	/* IBRIGHTNESS_60NT */
		{0x09, 0x03, 0x23},	/* IBRIGHTNESS_70NT */
		{0x09, 0x02, 0x68},	/* IBRIGHTNESS_80NT */
		{0x09, 0x01, 0xC8},	/* IBRIGHTNESS_90NT */
		{0x09, 0x01, 0x11},	/* IBRIGHTNESS_100NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_110NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_120NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_130NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_140NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_150NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_160NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_170NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_180NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_190NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_200NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_210NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_220NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_230NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_240NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_250NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_260NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_270NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_280NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_290NT */
		{0x01, 0x00, 0x06}	/* IBRIGHTNESS_300NT */
	}, {
		{0x09, 0x06, 0xE5},	/* IBRIGHTNESS_10NT */
		{0x09, 0x06, 0x46},	/* IBRIGHTNESS_20NT */
		{0x09, 0x05, 0xA5},	/* IBRIGHTNESS_30NT */
		{0x09, 0x04, 0xF5},	/* IBRIGHTNESS_40NT */
		{0x09, 0x04, 0x53},	/* IBRIGHTNESS_50NT */
		{0x09, 0x03, 0x95},	/* IBRIGHTNESS_60NT */
		{0x09, 0x02, 0xEC},	/* IBRIGHTNESS_70NT */
		{0x09, 0x02, 0x3C},	/* IBRIGHTNESS_80NT */
		{0x09, 0x01, 0x66},	/* IBRIGHTNESS_90NT */
		{0x09, 0x00, 0xCA},	/* IBRIGHTNESS_100NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_110NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_120NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_130NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_140NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_150NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_160NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_170NT */
		{0x09, 0x03, 0x06},	/* IBRIGHTNESS_180NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_190NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_200NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_210NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_220NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_230NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_240NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_250NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_260NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_270NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_280NT */
		{0x01, 0x00, 0x06},	/* IBRIGHTNESS_290NT */
		{0x01, 0x00, 0x06}	/* IBRIGHTNESS_300NT */
	}
};

static const int offset_gradation[2][IBRIGHTNESS_MAX][IV_MAX] = {	/* V0 ~ V255 */
	{
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_10NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_20NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_30NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_40NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_50NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_60NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_70NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_80NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_90NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_100NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_110NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_120NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_130NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_140NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_150NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_160NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_170NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_180NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_190NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_200NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_210NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_220NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_230NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_240NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_250NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_260NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_270NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_280NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_290NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_300NT */
	}, {
		{0, 0, 10, 9, 8, 6, 3, 0, 0, 0},  /* IBRIGHTNESS_10NT */
		{0, 0, 8, 7, 5, 3, 1, 0, 0, 0},    /* IBRIGHTNESS_20NT */
		{0, 1, 7, 6, 4, 2, 0, 0, 0, 0},    /* IBRIGHTNESS_30NT */
		{0, 1, 5, 4, 2, 1, 0, 0, 0, 0},    /* IBRIGHTNESS_40NT */
		{0, 1, 4, 3, 1, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_50NT */
		{0, 1, 4, 3, 1, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_60NT */
		{0, 1, 4, 2, 1, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_70NT */
		{0, 1, 3, 2, 1, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_80NT */
		{0, 1, 2, 1, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_90NT */
		{0, 1, 1, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_100NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_110NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_120NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_130NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_140NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_150NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_160NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_170NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_180NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_190NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_200NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_210NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_220NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_230NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_240NT */
		{0, 1, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_250NT */
		{0, 0, 0, 0, 1, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_260NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_270NT */
		{0, 0, 0, 0, 1, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_280NT */
		{0, 0, 0, 0, 0, 1, 0, 0, 0, 0},    /* IBRIGHTNESS_290NT */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    /* IBRIGHTNESS_300NT */
	}
};

static const int offset_color[2][IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {	/* V0 ~ V255 */
	{
		{0, 0, 0, 0, 0, 0, -60, 0, 48, -60, 0, 5, -60, 7, -11, -28, 5, -9, -9, 4, -4, -2, 3, 0, 0, 1, 0, -1, -1, 0},
		{0, 0, 0, 0, 0, 0, -60, 0, 23, -60, 3, -5, -19, 5, -10, -13, 4, -5, -5, 3, -3, -1, 0, 0, 0, 0, 0, -1, -1, 0},
		{0, 0, 0, 0, 0, 0, -60, 0, 7, -53, 3, -8, -15, 3, -9, -8, 2, -4, -2, 1, -2, -1, 0, 0, 0, 0, 0, -1, -1, 0},
		{0, 0, 0, 0, 0, 0, -60, 0, 1, -38, 2, -7, -12, 1, -8, -6, 1, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -60, 0, 0, -27, 1, -7, -10, 0, -7, -5, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -60, 0, 0, -19, 0, -7, -8, 0, -5, -4, 0, -3, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -60, 0, 0, -12, 0, -6, -6, 0, -3, -3, 0, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -48, 0, 0, -7, 0, -4, -4, 0, -2, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, 0, -3, 0, -1, -3, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -25, 0, -3, -6, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	}, {
		{0, 0, 0, 0, 0, 0, -13, 0, 5, -7, 10, -5, -8, 6, -8, -11, 4, -9, -6, 3, -8, -3, 1, -2, -3, 0, -2, -1, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 2, -3, -12, 6, -12, -6, 3, -6, -8, 3, -6, -5, 1, -5, -2, 0, -2, 0, 0, -1, -1, -1, 0},
		{0, 0, 0, 0, 0, 0, -2, 0, -4, -16, 2, -19, -10, 1, -9, -8, 1, -5, -3, 0, -4, -2, 0, -2, 0, 0, -1, -1, -1, 0},
		{0, 0, 0, 0, 0, 0, -20, 0, -18, -9, 2, -13, -9, 1, -6, -6, 0, -5, -2, 0, -3, -2, 0, -1, 0, 0, -1, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -50, 0, -27, -8, 1, -11, -9, 0, -6, -4, 0, -3, -1, 0, -2, -1, 0, -1, -1, 0, -1, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -4, 4, -11, -9, 6, -12, -8, 0, -6, -3, 0, -1, -2, 0, -2, -1, 0, -1, -1, 0, -1, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -28, 0, -23, -4, 1, -8, -5, 0, -2, -1, 0, -1, -2, 0, -1, -1, 0, -1, -1, 0, -1, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -21, 0, -18, -3, 0, -6, -3, 0, -1, -3, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0, -1, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -18, 0, -17, -2, 0, -6, -2, 0, 0, -2, 0, 0, -1, 0, -1, -1, 0, -1, -1, 0, -1, 0, -1, 0},
		{0, 0, 0, 0, 0, 0, -26, 0, -11, 0, 0, -4, -2, 0, 0, -1, 0, 0, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -12, 3, -12, -3, 0, -4, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -12, 3, -12, -3, 0, -4, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -12, 3, -12, -3, 0, -4, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -10, 3, -10, -3, 0, -4, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -6, 2, -6, -2, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -6, 2, -6, -2, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -6, 2, -6, -2, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, -6, 2, -6, -2, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	}
};

#endif /* __DYNAMIC_AID_XXXX_H */
