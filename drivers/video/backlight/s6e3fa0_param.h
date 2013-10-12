#ifndef __S6E3FA0_PARAM_H__
#define __S6E3FA0_PARAM_H__

#define GAMMA_PARAM_SIZE	34
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_CONDITION_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)
#define ELVSS_TABLE_NUM 2

enum {
	GAMMA_5CD,
	GAMMA_6CD,
	GAMMA_7CD,
	GAMMA_8CD,
	GAMMA_9CD,
	GAMMA_10CD,
	GAMMA_11CD,
	GAMMA_12CD,
	GAMMA_13CD,
	GAMMA_14CD,
	GAMMA_15CD,
	GAMMA_16CD,
	GAMMA_17CD,
	GAMMA_19CD,
	GAMMA_20CD,
	GAMMA_21CD,
	GAMMA_22CD,
	GAMMA_24CD,
	GAMMA_25CD,
	GAMMA_27CD,
	GAMMA_29CD,
	GAMMA_30CD,
	GAMMA_32CD,
	GAMMA_34CD,
	GAMMA_37CD,
	GAMMA_39CD,
	GAMMA_41CD,
	GAMMA_44CD,
	GAMMA_47CD,
	GAMMA_50CD,
	GAMMA_53CD,
	GAMMA_56CD,
	GAMMA_60CD,
	GAMMA_64CD,
	GAMMA_68CD,
	GAMMA_72CD,
	GAMMA_77CD,
	GAMMA_82CD,
	GAMMA_87CD,
	GAMMA_93CD,
	GAMMA_98CD,
	GAMMA_105CD,
	GAMMA_111CD,
	GAMMA_119CD,
	GAMMA_126CD,
	GAMMA_134CD,
	GAMMA_143CD,
	GAMMA_152CD,
	GAMMA_162CD,
	GAMMA_172CD,
	GAMMA_183CD,
	GAMMA_195CD,
	GAMMA_207CD,
	GAMMA_220CD,
	GAMMA_234CD,
	GAMMA_249CD,
	GAMMA_265CD,
	GAMMA_282CD,
	GAMMA_300CD,
	GAMMA_316CD,
	GAMMA_333CD,
	GAMMA_350CD,
	GAMMA_HBM,
	GAMMA_MAX
};

static const unsigned char SEQ_READ_ID[] = {
	0x04,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SEQ_ERR_FG[] = {
	0xED,
	0x0C, 0x04
};

static const unsigned char SEQ_GAMMA_CONTROL_SET_300CD[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_ACL_CONDITION[] = {
	0xB5,
	0x03, 0x99, 0x27, 0x35, 0x45, 0x0A,
};

static const unsigned char SEQ_AOR_CONTROL_G[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x06,
};

static const unsigned char SEQ_AOR_CONTROL_H[] = {
	0xB2,
	0x00, 0x0C, 0x00, 0x0C,
};

static const unsigned char SEQ_AOR_CONTROL_I[] = {
	0xB2,
	0x00, 0x08, 0x00, 0x08,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x06,
};

const unsigned char *pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL;

static const unsigned char SEQ_ELVSS_CONDITION_SET[] = {
	0xB6,
	0x88, 0x0A,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03,
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_PENTILE_CONDITION[] = {
	0xC0,
	0x00, 0x02, 0x03, 0x32, 0x03, 0x44, 0x44, 0xC0, 0x00, 0x1C,
	0x20, 0xE8,
};

static const unsigned char SEQ_RE_SETTING_EVT1_1[] = {
	0xC0,
	0x00, 0x02, 0x03, 0x32, 0x03, 0x44, 0x44, 0xC0, 0x00, 0x1C,
	0x20, 0xE8,
};

static const unsigned char SEQ_RE_SETTING_EVT1_2[] = {
	0xE3,
	0xFF, 0xFF, 0xFF, 0xFF,
};

static const unsigned char SEQ_RE_SETTING_EVT1_3[] = {
	0xFE,
	0x00, 0x03,
};

static const unsigned char SEQ_RE_SETTING_EVT1_4[] = {
	0xB0,
	0x2B,
};

static const unsigned char SEQ_RE_SETTING_EVT1_5[] = {
	0xFE,
	0xE4,
};

static const unsigned char SEQ_GLOBAL_PARAM_TEMP_OFFSET[] = {
	0xB0,
	0x0B,
};

static const unsigned char SEQ_TEMP_OFFSET_CONDITION[] = {
	0xB6,
	0x00, 0x06, 0x66, 0x6C, 0x0C,
};

static const unsigned char SEQ_GLOBAL_PARAM_SOURCE_AMP[] = {
	0xB0,
	0x24,
};

static const unsigned char SEQ_SOURCE_AMP_A[] = {
	0xD7,
	0xA5,
};

static const unsigned char SEQ_GLOBAL_PARAM_BIAS_CURRENT[] = {
	0xB0,
	0x1F,
};

static const unsigned char SEQ_BIAS_CURRENT[] = {
	0xD7,
	0x0A,
};

static const unsigned char SEQ_GLOBAL_PARAM_ILVL[] = {
	0xB0,
	0x01,
};

static const unsigned char SEQ_ILVL[] = {
	0xE8,
	0x34,
};

static const unsigned char SEQ_GLOBAL_PARAM_VLIN1[] = {
	0xB0,
	0x02,
};

static const unsigned char SEQ_VLIN1[] = {
	0xB8,
	0x40,
};

static const unsigned char SEQ_GLOBAL_PARAM_TSET[] = {
	0xB0,
	0x05,
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19,
};

static const unsigned char SEQ_GLOBAL_PARAM_HBM[] = {
	0xB0,
	0x10,
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_TE_OFF[] = {
	0x34,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
};

static const unsigned char SEQ_PARTIAL_DISP_OFF[] = {
	0x13,
};

static const unsigned char SEQ_PARTIAL_DISP_ON[] = {
	0x12,
};

static const unsigned char SEQ_LDI_FPS_READ[] = {
	0xD7,
};

static const unsigned char SEQ_LDI_FPS_POS[] = {
	0xB0,
	0x1A,
};

static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C, 0x51,
};

static const unsigned char SEQ_TOUCHKEY_OFF[] = {
	0xFF,
	0x00,
};

static const unsigned char SEQ_TOUCHKEY_ON[] = {
	0xFF,
	0x01,
};

#if !defined(CONFIG_I80_COMMAND_MODE)
static const unsigned char SEQ_DISPCTL[] = {
	0xF2,
	0x02, 0x03, 0xC, 0xA0, 0x01, 0x48,
};
#endif

enum {
	TSET_25_DEGREES,
	TSET_MINUS_0_DEGREES,
	TSET_MINUS_20_DEGREES,
	TSET_STATUS_MAX,
};

static const unsigned char TSET_TABLE[TSET_STATUS_MAX] = {
	0x19,	/* +25 degree */
	0x00,	/* -0 degree */
	0x94,	/* -20 degree */
};

#define LDI_FPS_OFFSET_MAX 33
static int ldi_fps_offset[LDI_FPS_OFFSET_MAX][2] = {
	{57260, -17},
	{57410, -16},
	{57610, -15},
	{57810, -14},
	{57970, -13},
	{58170, -12},
	{58370, -11},
	{58530, -10},
	{58730, -9},
	{58920, -8},
	{59080, -7},
	{59280, -6},
	{59480, -5},
	{59640, -4},
	{59840, -3},
	{60040, -2},
	{60200, -1},
	{60390, 0},
	{60590, 1},
	{60790, 2},
	{60950, 3},
	{61150, 4},
	{61350, 5},
	{61510, 6},
	{61710, 7},
	{61900, 8},
	{62060, 9},
	{62260, 10},
	{62460, 11},
	{62620, 12},
	{62820, 13},
	{63020, 14},
	{63180, 15},
};

static int ldi_vddm_lut[][2] = {
	{0, 13},
	{1, 13},
	{2, 14},
	{3, 15},
	{4, 16},
	{5, 17},
	{6, 18},
	{7, 19},
	{8, 20},
	{9, 21},
	{10, 22},
	{11, 23},
	{12, 24},
	{13, 25},
	{14, 26},
	{15, 27},
	{16, 28},
	{17, 29},
	{18, 30},
	{19, 31},
	{20, 32},
	{21, 33},
	{22, 34},
	{23, 35},
	{24, 36},
	{25, 37},
	{26, 38},
	{27, 39},
	{28, 40},
	{29, 41},
	{30, 42},
	{31, 43},
	{32, 44},
	{33, 45},
	{34, 46},
	{35, 47},
	{36, 48},
	{37, 49},
	{38, 50},
	{39, 51},
	{40, 52},
	{41, 53},
	{42, 54},
	{43, 55},
	{44, 56},
	{45, 57},
	{46, 58},
	{47, 59},
	{48, 60},
	{49, 61},
	{50, 62},
	{51, 63},
	{52, 63},
	{53, 63},
	{54, 63},
	{55, 63},
	{56, 63},
	{57, 63},
	{58, 63},
	{59, 63},
	{60, 63},
	{61, 63},
	{62, 63},
	{63, 63},
	{64, 12},
	{65, 11},
	{66, 10},
	{67, 9},
	{68, 8},
	{69, 7},
	{70, 6},
	{71, 5},
	{72, 4},
	{73, 3},
	{74, 2},
	{75, 1},
	{76, 64},
	{77, 65},
	{78, 66},
	{79, 67},
	{80, 68},
	{81, 69},
	{82, 70},
	{83, 71},
	{84, 72},
	{85, 73},
	{86, 74},
	{87, 75},
	{88, 76},
	{89, 77},
	{90, 78},
	{91, 79},
	{92, 80},
	{93, 81},
	{94, 82},
	{95, 83},
	{96, 84},
	{97, 85},
	{98, 86},
	{99, 87},
	{100, 88},
	{101, 89},
	{102, 90},
	{103, 91},
	{104, 92},
	{105, 93},
	{106, 94},
	{107, 95},
	{108, 96},
	{109, 97},
	{110, 98},
	{111, 99},
	{112, 100},
	{113, 101},
	{114, 102},
	{115, 103},
	{116, 104},
	{117, 105},
	{118, 106},
	{119, 107},
	{120, 108},
	{121, 109},
	{122, 110},
	{123, 111},
	{124, 112},
	{125, 113},
	{126, 114},
	{127, 115},
};

enum {
	ELVSS_STATUS_105,
	ELVSS_STATUS_111,
	ELVSS_STATUS_119,
	ELVSS_STATUS_126,
	ELVSS_STATUS_134,
	ELVSS_STATUS_143,
	ELVSS_STATUS_152,
	ELVSS_STATUS_162,
	ELVSS_STATUS_172,
	ELVSS_STATUS_183,
	ELVSS_STATUS_195,
	ELVSS_STATUS_207,
	ELVSS_STATUS_220,
	ELVSS_STATUS_234,
	ELVSS_STATUS_249,
	ELVSS_STATUS_265,
	ELVSS_STATUS_282,
	ELVSS_STATUS_300,
	ELVSS_STATUS_316,
	ELVSS_STATUS_333,
	ELVSS_STATUS_350,
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned char ELVSS_TABLE_I[ELVSS_STATUS_MAX][ELVSS_TABLE_NUM] = {
	{0x11, 0x15},
	{0x0F, 0x13},
	{0x0E, 0x12},
	{0x0E, 0x12},
	{0x0D, 0x11},
	{0x0C, 0x10},
	{0x0B, 0x0F},
	{0x09, 0x0D},
	{0x08, 0x0C},
	{0x06, 0x0A},
	{0x06, 0x0A},
	{0x06, 0x0A},
	{0x05, 0x09},
	{0x04, 0x08},
	{0x04, 0x08},
	{0x03, 0x07},
	{0x03, 0x08},
	{0x02, 0x07},
	{0x01, 0x06},
	{0x01, 0x06},
	{0x00, 0x05},
	{0x00, 0x00}
};

static const unsigned char ELVSS_TABLE[ELVSS_STATUS_MAX][ELVSS_TABLE_NUM] = {
	{0x0B, 0x0E},
	{0x0A, 0x0D},
	{0x09, 0x0C},
	{0x09, 0x0C},
	{0x08, 0x0B},
	{0x07, 0x0A},
	{0x07, 0x0A},
	{0x06, 0x09},
	{0x05, 0x08},
	{0x06, 0x09},
	{0x05, 0x08},
	{0x05, 0x08},
	{0x04, 0x07},
	{0x04, 0x07},
	{0x03, 0x06},
	{0x03, 0x06},
	{0x02, 0x06},
	{0x02, 0x06},
	{0x01, 0x05},
	{0x01, 0x05},
	{0x00, 0x04},
	{0x00, 0x00}
};

const unsigned char (*pELVSS_TABLE)[ELVSS_TABLE_NUM] = ELVSS_TABLE;

enum {
	ACL_STATUS_0P,
	ACL_STATUS_25P,
	ACL_STATUS_25P_RE_LOW,
	ACL_STATUS_MAX
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_25[] = {
	0x55,
	0x02,
};

static const unsigned char SEQ_ACL_25_RE_LOW[] = {
	0x55,
	0x42,
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_25,
	SEQ_ACL_25_RE_LOW,
};
#endif /* __S6E3FA0_PARAM_H__ */
