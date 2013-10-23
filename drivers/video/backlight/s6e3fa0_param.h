#ifndef __S6E3FA0_PARAM_H__
#define __S6E3FA0_PARAM_H__

#define GAMMA_PARAM_SIZE	34
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_CONDITION_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)

enum {
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

static const unsigned char SEQ_GAMMA_CONTROL_SET_300CD[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x06, 0x06, 0x06, 0x48, 0x18, 0x3F, 0xFF,
	0xFF,
};

static const unsigned char SEQ_ELVSS_CONDITION_SET[] = {
	0xB6,
	0x88, 0x0A,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03, 0x00
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_ACL_CONTROL[] = {
	0xB5,
	0x03, 0x98, 0x26, 0x36, 0x45,
};

static const unsigned char SEQ_ETC_PENTILE_SETTING[] = {
	0xC0,
	0x00, 0x02, 0x03, 0x32, 0xD8, 0x44, 0x44, 0xC0, 0x00, 0x48,
	0x20, 0xD8,
};

static const unsigned char SEQ_GLOBAL_PARAM_SOURCE_AMP[] = {
	0xB0,
	0x24,
};

static const unsigned char SEQ_ETC_SOURCE_AMP[] = {
	0xD7,
	0xA5,
};

static const unsigned char SEQ_GLOBAL_PARAM_BIAS_CURRENT[] = {
	0xB0,
	0x1F,
};

static const unsigned char SEQ_ETC_BIAS_CURRENT[] = {
	0xD7,
	0x0A,
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00,  0x00
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00,  0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
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
	0x80,	/* -0 degree */
	0x94,	/* -20 degree */
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
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned char ELVSS_TABLE[ELVSS_STATUS_MAX] = {
	0x0F,
	0x0C,
	0x0B,
	0x0A,
	0x09,
	0x08,
	0x07,
	0x06,
	0x05,
	0x10,
	0x09,
	0x08,
	0x06,
	0x05,
	0x04,
	0x03,
	0x02,
	0x00,
	0x00,
};

enum {
	ACL_STATUS_0P,
	ACL_STATUS_40P,
	ACL_STATUS_40P_RE_LOW,
	ACL_STATUS_MAX
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55, 0x00,
	0x00
};

static const unsigned char SEQ_ACL_40[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char SEQ_ACL_40_RE_LOW[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_40,
	SEQ_ACL_40_RE_LOW,
};
#endif /* __S6E3FA0_PARAM_H__ */
