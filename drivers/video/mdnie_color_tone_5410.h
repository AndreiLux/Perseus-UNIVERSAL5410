#ifndef __MDNIE_COLOR_TONE_H__
#define __MDNIE_COLOR_TONE_H__

#include "mdnie.h"

static const unsigned short tune_scr_setting[9][3] = {
	{0xff, 0xf7, 0xf8},
	{0xff, 0xf9, 0xfe},
	{0xfa, 0xf8, 0xff},
	{0xff, 0xfc, 0xf9},
	{0xff, 0xff, 0xff},
	{0xf8, 0xfa, 0xff},
	{0xfc, 0xff, 0xf8},
	{0xfb, 0xff, 0xfb},
	{0xf9, 0xff, 0xff},
};

#if defined(CONFIG_FB_MDNIE_PWM)
/* this is dummy table, it should be updated with cabc value */
static unsigned short tune_negative[] = {
	/*start JA negative*/
	0x0000, 0x0000, /*BANK 0*/
	0x0008, 0x0200, /*SCR2 CC1 | CS2 DE1 | 0*/
	0x0009, 0x0000, /*MCM off*/
	0x000a, 0x0000, /*UC off*/
	0x0000, 0x0001, /*BANK 1*/
	0x0071, 0x00ff, /*SCR RrCr*/
	0x0072, 0xff00, /*SCR RgCg*/
	0x0073, 0xff00, /*SCR RbCb*/
	0x0074, 0xff00, /*SCR GrMr*/
	0x0075, 0x00ff, /*SCR GgMg*/
	0x0076, 0xff00, /*SCR GbMb*/
	0x0077, 0xff00, /*SCR BrYr*/
	0x0078, 0xff00, /*SCR BgYg*/
	0x0079, 0x00ff, /*SCR BbYb*/
	0x007a, 0xff00, /*SCR KrWr*/
	0x007b, 0xff00, /*SCR KgWg*/
	0x007c, 0xff00, /*SCR KbWb*/
	0x00ff, 0x0000, /*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

/* this is dummy table, it should be updated with cabc value */
static unsigned short tune_negative_cabc[] = {
	/*start JA negative*/
	0x0000, 0x0000, /*BANK 0*/
	0x0008, 0x0200, /*SCR2 CC1 | CS2 DE1 | 0*/
	0x0009, 0x0000, /*MCM off*/
	0x000a, 0x0000, /*UC off*/
	0x0000, 0x0001, /*BANK 1*/
	0x0071, 0x00ff, /*SCR RrCr*/
	0x0072, 0xff00, /*SCR RgCg*/
	0x0073, 0xff00, /*SCR RbCb*/
	0x0074, 0xff00, /*SCR GrMr*/
	0x0075, 0x00ff, /*SCR GgMg*/
	0x0076, 0xff00, /*SCR GbMb*/
	0x0077, 0xff00, /*SCR BrYr*/
	0x0078, 0xff00, /*SCR BgYg*/
	0x0079, 0x00ff, /*SCR BbYb*/
	0x007a, 0xff00, /*SCR KrWr*/
	0x007b, 0xff00, /*SCR KgWg*/
	0x007c, 0xff00, /*SCR KbWb*/
	0x00ff, 0x0000, /*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_color_blind[] = {
	/*start Adonis LCD Color Blind cabcoff*/
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0200,	/*SCR2 CC1 | CS2 DE1 | 0*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0071, 0xff00,	/*SCR RrCr*/
	0x0072, 0x00ff,	/*SCR RgCg*/
	0x0073, 0x00ff,	/*SCR RbCb*/
	0x0074, 0x00ff,	/*SCR GrMr*/
	0x0075, 0xff00,	/*SCR GgMg*/
	0x0076, 0x00ff,	/*SCR GbMb*/
	0x0077, 0x00ff,	/*SCR BrYr*/
	0x0078, 0x00ff,	/*SCR BgYg*/
	0x0079, 0xff00,	/*SCR BbYb*/
	0x007a, 0x00ff,	/*SCR KrWr*/
	0x007b, 0x00ff,	/*SCR KgWg*/
	0x007c, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_color_blind_cabc[] = {
	/*start Adonis LCD Color Blind cabcon*/
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0a00,	/*ABC8 CP4 SCR2 CC1 | CS2 DE1 | 0*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0071, 0xff00,	/*SCR RrCr*/
	0x0072, 0x00ff,	/*SCR RgCg*/
	0x0073, 0x00ff,	/*SCR RbCb*/
	0x0074, 0x00ff,	/*SCR GrMr*/
	0x0075, 0xff00,	/*SCR GgMg*/
	0x0076, 0x00ff,	/*SCR GbMb*/
	0x0077, 0x00ff,	/*SCR BrYr*/
	0x0078, 0x00ff,	/*SCR BgYg*/
	0x0079, 0xff00,	/*SCR BbYb*/
	0x007a, 0x00ff,	/*SCR KrWr*/
	0x007b, 0x00ff,	/*SCR KgWg*/
	0x007c, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};
#else
static unsigned short tune_negative[] = {
	/*start JA negative*/
	0x0000, 0x0000, /*BANK 0*/
	0x0008, 0x0200, /*SCR2 CC1 | CS2 DE1 | 0*/
	0x0009, 0x0000, /*MCM off*/
	0x000a, 0x0000, /*UC off*/
	0x0000, 0x0001, /*BANK 1*/
	0x0071, 0x00ff, /*SCR RrCr*/
	0x0072, 0xff00, /*SCR RgCg*/
	0x0073, 0xff00, /*SCR RbCb*/
	0x0074, 0xff00, /*SCR GrMr*/
	0x0075, 0x00ff, /*SCR GgMg*/
	0x0076, 0xff00, /*SCR GbMb*/
	0x0077, 0xff00, /*SCR BrYr*/
	0x0078, 0xff00, /*SCR BgYg*/
	0x0079, 0x00ff, /*SCR BbYb*/
	0x007a, 0xff00, /*SCR KrWr*/
	0x007b, 0xff00, /*SCR KgWg*/
	0x007c, 0xff00, /*SCR KbWb*/
	0x00ff, 0x0000, /*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_color_blind[] = {
	/*start Adonis AMOLED Color Blind*/
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0200,	/*SCR2 CC1 | CS2 DE1 | 0*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0071, 0xff00,	/*SCR RrCr*/
	0x0072, 0x00ff,	/*SCR RgCg*/
	0x0073, 0x00ff,	/*SCR RbCb*/
	0x0074, 0x00ff,	/*SCR GrMr*/
	0x0075, 0xff00,	/*SCR GgMg*/
	0x0076, 0x00ff,	/*SCR GbMb*/
	0x0077, 0x00ff,	/*SCR BrYr*/
	0x0078, 0x00ff,	/*SCR BgYg*/
	0x0079, 0xff00,	/*SCR BbYb*/
	0x007a, 0x00ff,	/*SCR KrWr*/
	0x007b, 0x00ff,	/*SCR KgWg*/
	0x007c, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};
#endif

static unsigned short tune_bypass_off[] = {
	0x0000, 0x0000, /*BANK 0*/
	0x0008, 0x0000,
	0x0009, 0x0000,
	0x000a, 0x0000,
	0x00ff, 0x0000, /*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_bypass_on[] = {
	0x0000, 0x0000, /*BANK 0*/
	0x0008, 0x2F7F,
	0x0009, 0x0001,
	0x000a, 0x0001,
	0x00ff, 0x0000, /*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

struct mdnie_tuning_info negative_table[CABC_MAX] = {
	{"negative",		tune_negative},
#if defined(CONFIG_FB_MDNIE_PWM)
	{"negative_cabc",	tune_negative_cabc},
#endif
};

struct mdnie_tuning_info accessibility_table[CABC_MAX][ACCESSIBILITY_MAX] = {
	{
		{NULL,			NULL},
		{"negative",		tune_negative},
		{"color_blind",		tune_color_blind},
	},
#if defined(CONFIG_FB_MDNIE_PWM)
	{
		{NULL,			NULL},
		{"negative_cabc",	tune_negative_cabc},
		{"color_blind_cabc",	tune_color_blind_cabc},
	}
#endif
};

struct mdnie_tuning_info bypass_table[BYPASS_MAX] = {
	{"bypass_off",		tune_bypass_off},
	{"bypass_on",		tune_bypass_on},
};

#endif /* __MDNIE_COLOR_TONE_H__ */
