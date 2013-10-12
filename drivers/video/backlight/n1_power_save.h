#ifndef __V1_POWER_SAVE_H__
#define __V1_POWER_SAVE_H__

enum tcon_mode {
    TCON_MODE_UI = 0,
    TCON_MODE_VIDEO,
    TCON_MODE_VIDEO_WARM,
    TCON_MODE_VIDEO_COLD,
    TCON_MODE_CAMERA,
    TCON_MODE_NAVI,
    TCON_MODE_GALLERY,
    TCON_MODE_VT,
    TCON_MODE_BROWSER,
    TCON_MODE_EBOOK,
    TCON_MODE_EMAIL,
    TCON_MODE_MAX, /* 11 */
};

enum tcon_illu {
    TCON_LEVEL_1 = 0,   /* 0 ~ 150 lux*/
    TCON_LEVEL_2,       /* 150 ~ 40k lux*/
    TCON_LEVEL_3,       /* 40k lux ~ */
    TCON_LEVEL_MAX,
};

enum tcon_auto_br {
    TCON_AUTO_BR_OFF = 0,
    TCON_AUTO_BR_ON, 
    TCON_AUTO_BR_MAX,
};

#define TCON_REG_MAX 30

/* If Limit MN_BL% duty */
#define CONFIG_TCON_SET_MNBL
#ifdef CONFIG_TCON_SET_MNBL
#define MN_BL	10
#define MNBL_INDEX1	8
#define MNBL_INDEX2	9
#endif

struct tcon_reg_info {
	int reg_cnt;
	unsigned short addr[TCON_REG_MAX];
	unsigned char data[TCON_REG_MAX];
};

struct tcon_reg_info TCON_UI = {
	.reg_cnt = 20,

	.addr = {
		0x0DB1, 0x0DB2, 0x0DB3, 0x0DB4, 0x0DB5,
		0x0DB6, 0x0DB7, 0x0DB8, 0x0DB9, 0x0DBA,
		0x0DBB, 0x0DBC, 0x0DBD, 0x0DBE, 0x0DBF,
		0x0DC0, 0x0E39, 0x0E3A, 0x0E3B, 0x0DC5,
		},
	.data = {
		0xB1,   0xFF,   0xF0,   0x1F,   0xF5,
		0xFE,   0x82,   0x46,   0x5A,   0xBF,
		0xC1,   0x04,   0x24,   0x26,   0x01,
		0x0F,   0x41,   0x3F,   0x59,   0xF4,
		},
};

struct tcon_reg_info TCON_POWER_SAVE = {
	.reg_cnt = 20,

	.addr = {
		0x0DB1, 0x0DB2, 0x0DB3, 0x0DB4, 0x0DB5,
		0x0DB6, 0x0DB7, 0x0DB8, 0x0DB9, 0x0DBA,
		0x0DBB, 0x0DBC, 0x0DBD, 0x0DBE, 0x0DBF,
		0x0DC0, 0x0E39, 0x0E3A, 0x0E3B, 0x0DC5,
		},
	.data = {
		0xB1,   0xFF,   0xF0,   0x0E,   0xC9,
		0x4E,   0x92,   0x4A,   0x59,   0xBF,
		0xC1,   0x04,   0x24,   0x26,   0x01,
		0x0F,   0x41,   0x3F,   0x59,   0xF4,
		},
};

struct tcon_reg_info TCON_VIDEO = {
	.reg_cnt = 20,

	.addr = {
		0x0DB1, 0x0DB2, 0x0DB3, 0x0DB4, 0x0DB5,
		0x0DB6, 0x0DB7, 0x0DB8, 0x0DB9, 0x0DBA,
		0x0DBB, 0x0DBC, 0x0DBD, 0x0DBE, 0x0DBF,
		0x0DC0,0x0E39, 0x0E3A, 0x0E3B, 0x0DC5,
		},
	.data = {
		0xB1,   0xFF,   0xF0,   0x08,   0x85,
		0x5E,   0x92,   0x4A,   0x59,   0xBF,
		0xC1,   0x04,   0x24,   0x26,   0x01,
		0x0F,   0x41,   0x3F,   0x59,   0xF4,
		},
};

struct tcon_reg_info TCON_BROWSER = {
	.reg_cnt = 20,

	.addr = {
		0x0DB1, 0x0DB2, 0x0DB3, 0x0DB4, 0x0DB5,
		0x0DB6, 0x0DB7, 0x0DB8, 0x0DB9, 0x0DBA,
		0x0DBB, 0x0DBC, 0x0DBD, 0x0DBE, 0x0DBF,
		0x0DC0,0x0E39, 0x0E3A, 0x0E3B, 0x0DC5,
		},
	.data = {
		0xB1,   0xFF,   0xF0,   0x08,   0x85,
		0x5E,   0x92,   0x4A,   0x59,   0xBF,
		0xC1,   0x04,   0x24,   0x26,   0x01,
		0x0F,   0x41,   0x3F,   0x59,   0xF4,
		},
};

struct tcon_reg_info TCON_OUTDOOR = {
	.reg_cnt = 20,

	.addr = {
		0x0DB1, 0x0DB2, 0x0DB3, 0x0DB4, 0x0DB5,
		0x0DB6, 0x0DB7, 0x0DB8, 0x0DB9, 0x0DBA,
		0x0DBB, 0x0DBC, 0x0DBD, 0x0DBE, 0x0DBF,
		0x0DC0,0x0E39, 0x0E3A, 0x0E3B, 0x0DC5,
		},
	.data = {
		0xB0,   0x79,   0x50,   0x1F,  0xF5,
		0xFE,   0x82,   0x46,   0x5A,  0xBF,
		0xC1,   0x04,   0x24,   0x26,  0x01,
		0x0F,   0x41,   0x3F,   0x59,  0xF4,
		},
};

struct tcon_reg_info TCON_BLACK_IMAGE_BLU_ENABLE = {
	.reg_cnt = 1,
	.addr = {
		0x0DB1,
		},
	.data = {
		0xB0,
		},
};

struct tcon_reg_info *tcon_tune_value[TCON_AUTO_BR_MAX][TCON_LEVEL_MAX][TCON_MODE_MAX] = {
		/*
			UI_APP = 0,
			VIDEO_APP,
			VIDEO_WARM_APP,
			VIDEO_COLD_APP,
			CAMERA_APP,
			NAVI_APP,
			GALLERY_APP,
			VT_APP,
			BROWSER_APP,
			eBOOK_APP,
			EMAIL_APP,
		*/
	/* auto brightness off */
	{

		/* Illumiatation Level 1 */
		{
			&TCON_UI,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_BROWSER,
			&TCON_UI,
			&TCON_UI,
		},
		/* Illumiatation Level 2 */
		{
			&TCON_UI,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_BROWSER,
			&TCON_UI,
			&TCON_UI,
		},
		/* Illumiatation Level 3 */
		{
			&TCON_UI,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_BROWSER,
			&TCON_UI,
			&TCON_UI,
		},
	},
	/* auto brightness on */
	{
		/* Illumiatation Level 1 */
		{
			&TCON_UI,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_UI,
			&TCON_BROWSER,
			&TCON_UI,
			&TCON_UI,
		},
		/* Illumiatation Level 2 */
		{
			&TCON_POWER_SAVE,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_VIDEO,
			&TCON_POWER_SAVE,
			&TCON_POWER_SAVE,
			&TCON_POWER_SAVE,
			&TCON_POWER_SAVE,
			&TCON_BROWSER,
			&TCON_POWER_SAVE,
			&TCON_POWER_SAVE,
		},
		/* Illumiatation Level 3 */
		{
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
			&TCON_OUTDOOR,
		}
	}
};

#endif

