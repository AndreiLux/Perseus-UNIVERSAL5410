#ifndef __MDNIE_TABLE_H__
#define __MDNIE_TABLE_H__

#include "mdnie.h"

static unsigned char power_lut[2][9] = {
	{0x42, 0x47, 0x3E, 0x52, 0x42, 0x3F, 0x3A, 0x37, 0x3F},
	{0x38, 0x3d, 0x34, 0x48, 0x38, 0x35, 0x30, 0x2d, 0x35},
};

static unsigned short tune_ebook[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_ebook_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_email[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_email_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_camera[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_camera[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_vt[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_vt[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_vt[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_vt[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_browser[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_ui_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_video_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_gallery_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_vt_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_dynamic_browser_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_ui_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_video_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_gallery_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_vt_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_standard_browser_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_ui_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_video_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_gallery_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_vt_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_movie_browser_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_ui_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_video_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_gallery_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_vt_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_auto_browser_cabc[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	0x0009, 0x0000,	/*MCM off*/
	0x000a, 0x0000,	/*UC off*/
	0x00ff, 0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};


#if defined(CONFIG_FB_MDNIE_PWM)
struct mdnie_tuning_info tuning_table[CABC_MAX][MODE_MAX][SCENARIO_MAX] = {
	{
		{
			{"dynamic_ui",		tune_dynamic_ui},
			{"dynamic_video",	tune_dynamic_video},
			{"dynamic_video",	tune_dynamic_video},
			{"dynamic_video",	tune_dynamic_video},
			{"camera",		tune_camera},
			{"dynamic_ui",		tune_dynamic_ui},
			{"dynamic_gallery",	tune_dynamic_gallery},
			{"dynamic_vt",		tune_dynamic_vt},
			{"dynamic_browser",	tune_dynamic_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"standard_ui",		tune_standard_ui},
			{"standard_video",	tune_standard_video},
			{"standard_video",	tune_standard_video},
			{"standard_video",	tune_standard_video},
			{"camera",		tune_camera},
			{"standard_ui",		tune_standard_ui},
			{"standard_gallery",	tune_standard_gallery},
			{"standard_vt",		tune_standard_vt},
			{"standard_browser",	tune_standard_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"movie_ui",		tune_movie_ui},
			{"movie_video",		tune_movie_video},
			{"movie_video",		tune_movie_video},
			{"movie_video",		tune_movie_video},
			{"camera",		tune_camera},
			{"movie_ui",		tune_movie_ui},
			{"movie_gallery",	tune_movie_gallery},
			{"movie_vt",		tune_movie_vt},
			{"movie_browser",	tune_movie_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"auto_ui",		tune_auto_ui},
			{"auto_video",		tune_auto_video},
			{"auto_video",		tune_auto_video},
			{"auto_video",		tune_auto_video},
			{"auto_camera",		tune_auto_camera},
			{"auto_ui",		tune_auto_ui},
			{"auto_gallery",	tune_auto_gallery},
			{"auto_vt",		tune_auto_vt},
			{"auto_browser",	tune_auto_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		},
	}, {
		{
			{"dynamic_ui_cabc",		tune_dynamic_ui_cabc},
			{"dynamic_video_cabc",		tune_dynamic_video_cabc},
			{"dynamic_video_cabc",		tune_dynamic_video_cabc},
			{"dynamic_video_cabc",		tune_dynamic_video_cabc},
			{"camera",			tune_camera,},
			{"dynamic_ui_cabc",		tune_dynamic_ui_cabc},
			{"dynamic_gallery_cabc",	tune_dynamic_gallery_cabc},
			{"dynamic_vt_cabc",		tune_dynamic_vt_cabc},
			{"dynamic_browser_cabc",	tune_dynamic_browser_cabc},
			{"ebook_cabc",			tune_ebook_cabc},
			{"email_cabc",			tune_email_cabc}
		}, {
			{"standard_ui_cabc",		tune_standard_ui_cabc},
			{"standard_video_cabc",		tune_standard_video_cabc},
			{"standard_video_cabc",		tune_standard_video_cabc},
			{"standard_video_cabc",		tune_standard_video_cabc},
			{"camera",			tune_camera},
			{"standard_ui_cabc",		tune_standard_ui_cabc},
			{"standard_gallery_cabc",	tune_standard_gallery_cabc},
			{"standard_vt_cabc",		tune_standard_vt_cabc},
			{"standard_browser_cabc",	tune_standard_browser_cabc},
			{"ebook_cabc",			tune_ebook_cabc},
			{"email_cabc",			tune_email_cabc}
		}, {
			{"movie_ui_cabc",		tune_movie_ui_cabc},
			{"movie_video_cabc",		tune_movie_video_cabc},
			{"movie_video_cabc",		tune_movie_video_cabc},
			{"movie_video_cabc",		tune_movie_video_cabc},
			{"camera",			tune_camera},
			{"movie_ui_cabc",		tune_movie_ui_cabc},
			{"movie_gallery_cabc",		tune_movie_gallery_cabc},
			{"movie_vt_cabc",		tune_movie_vt_cabc},
			{"movie_browser_cabc",		tune_movie_browser_cabc},
			{"ebook_cabc",			tune_ebook_cabc},
			{"email_cabc",			tune_email_cabc}
		}, {
			{"auto_ui",			tune_auto_ui_cabc},
			{"auto_video",			tune_auto_video_cabc},
			{"auto_video",			tune_auto_video_cabc},
			{"auto_video",			tune_auto_video_cabc},
			{"auto_camera",			tune_auto_camera},
			{"auto_ui",			tune_auto_ui_cabc},
			{"auto_gallery",		tune_auto_gallery_cabc},
			{"auto_vt",			tune_auto_vt_cabc},
			{"auto_browser",		tune_auto_browser_cabc},
			{"ebook_cabc",			tune_ebook_cabc},
			{"email_cabc",			tune_email_cabc}
		},
	},
};
#else
struct mdnie_tuning_info tuning_table[CABC_MAX][MODE_MAX][SCENARIO_MAX] = {
	{
		{
			{"dynamic_ui",		tune_dynamic_ui},
			{"dynamic_video",	tune_dynamic_video},
			{"dynamic_video",	tune_dynamic_video},
			{"dynamic_video",	tune_dynamic_video},
			{"camera",		tune_camera},
			{"dynamic_ui",		tune_dynamic_ui},
			{"dynamic_gallery",	tune_dynamic_gallery},
			{"dynamic_vt",		tune_dynamic_vt},
			{"dynamic_browser",	tune_dynamic_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"standard_ui",		tune_standard_ui},
			{"standard_video",	tune_standard_video},
			{"standard_video",	tune_standard_video},
			{"standard_video",	tune_standard_video},
			{"camera",		tune_camera},
			{"standard_ui",		tune_standard_ui},
			{"standard_gallery",	tune_standard_gallery},
			{"standard_vt",		tune_standard_vt},
			{"standard_browser",	tune_standard_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"natural_ui",		tune_natural_ui},
			{"natural_video",	tune_natural_video},
			{"natural_video",	tune_natural_video},
			{"natural_video",	tune_natural_video},
			{"camera",		tune_camera},
			{"natural_ui",		tune_natural_ui},
			{"natural_gallery",	tune_natural_gallery},
			{"natural_vt",		tune_natural_vt},
			{"natural_browser",	tune_natural_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"movie_ui",		tune_movie_ui},
			{"movie_video",		tune_movie_video},
			{"movie_video",		tune_movie_video},
			{"movie_video",		tune_movie_video},
			{"camera",		tune_camera},
			{"movie_ui",		tune_movie_ui},
			{"movie_gallery",	tune_movie_gallery},
			{"movie_vt",		tune_movie_vt},
			{"movie_browser",	tune_movie_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		}, {
			{"auto_ui",		tune_auto_ui},
			{"auto_video",		tune_auto_video},
			{"auto_video",		tune_auto_video},
			{"auto_video",		tune_auto_video},
			{"auto_camera",		tune_auto_camera},
			{"auto_ui",		tune_auto_ui},
			{"auto_gallery",	tune_auto_gallery},
			{"auto_vt",		tune_auto_vt},
			{"auto_browser",	tune_auto_browser},
			{"ebook",		tune_ebook},
			{"email",		tune_email}
		},
	}
};
#endif


#endif /* __MDNIE_TABLE_H__ */
