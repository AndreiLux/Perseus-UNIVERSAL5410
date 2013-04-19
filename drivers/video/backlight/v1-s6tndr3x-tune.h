

#ifndef __V1_S6TNDR3X_TUNE_H__
#define __V1_S6TNDR3X_TUNE_H__


#include <linux/s6tndr3x_tcon.h>

/* outdoor */
struct tcon_reg_info tcon_tune_mode_outdoor[] = {
	TCON_TUNE_ADDR(BLE, 0),
	TCON_TUNE_ADDR(SVE, 0),
	TCON_TUNE_ADDR(FXSV, 108),
	TCON_TUNE_ADDR(END, 255),
};

/* internet */
struct tcon_reg_info tcon_tune_mode_browser[] = {
	TCON_TUNE_ADDR(RWT, 4),
	TCON_TUNE_ADDR(GWT, 4),
	TCON_TUNE_ADDR(BWT, 2),
	TCON_TUNE_ADDR(YWT, 6),
	TCON_TUNE_ADDR(WWT, 10),
	TCON_TUNE_ADDR(THH1, 2),
	TCON_TUNE_ADDR(THH2, 9),
	TCON_TUNE_ADDR(THL1, 4),
	TCON_TUNE_ADDR(THL2, 9),
	TCON_TUNE_ADDR(BLE, 1),
	TCON_TUNE_ADDR(SVE, 1),
	TCON_TUNE_ADDR(END, 255),
};

/* video */
struct tcon_reg_info tcon_tune_mode_video[] = {
	TCON_TUNE_ADDR(RWT, 4),
	TCON_TUNE_ADDR(GWT, 4),
	TCON_TUNE_ADDR(BWT, 2),
	TCON_TUNE_ADDR(YWT, 6),
	TCON_TUNE_ADDR(WWT, 10),
	TCON_TUNE_ADDR(THH1, 15),
	TCON_TUNE_ADDR(THH2, 12),
	TCON_TUNE_ADDR(THL1, 15),
	TCON_TUNE_ADDR(THL2, 12),
	TCON_TUNE_ADDR(BLE, 1),
	TCON_TUNE_ADDR(SVE, 1),
	TCON_TUNE_ADDR(END, 255),
};


/* basic */
struct tcon_reg_info tcon_tune_mode_ui[] = {
	TCON_TUNE_ADDR(RWT, 15),
	TCON_TUNE_ADDR(GWT, 15),
	TCON_TUNE_ADDR(BWT, 10),
	TCON_TUNE_ADDR(YWT, 15),
	TCON_TUNE_ADDR(WWT, 15),
	TCON_TUNE_ADDR(THH1, 1),
	TCON_TUNE_ADDR(THH2, 9),
	TCON_TUNE_ADDR(THL1, 0),
	TCON_TUNE_ADDR(THL2, 9),
	TCON_TUNE_ADDR(BLE, 1),
	TCON_TUNE_ADDR(SVE, 1),
	TCON_TUNE_ADDR(END, 255),
};


/* saving */
struct tcon_reg_info tcon_tune_mode_saving[] = {
	TCON_TUNE_ADDR(RWT, 7),
	TCON_TUNE_ADDR(GWT, 6),
	TCON_TUNE_ADDR(BWT, 4),
	TCON_TUNE_ADDR(WWT, 10),
	TCON_TUNE_ADDR(THH1, 2),
	TCON_TUNE_ADDR(THH2, 9),
	TCON_TUNE_ADDR(THL1, 4),
	TCON_TUNE_ADDR(THL2, 9),
	TCON_TUNE_ADDR(BLE, 1),
	TCON_TUNE_ADDR(SVE, 1),
	TCON_TUNE_ADDR(END, 255),
};


struct tcon_reg_info *v1_tune_value[TCON_AUTO_BR_MAX][TCON_LEVEL_MAX][TCON_MODE_MAX] = {
    /* auto brightness off */
    {
        /* Illumiatation Level 1 */
        {
            tcon_tune_mode_ui,
            tcon_tune_mode_video,
            tcon_tune_mode_video,
            tcon_tune_mode_video,
            NULL,
            NULL,
            NULL,
            NULL,
            tcon_tune_mode_browser,
            NULL,
        },
        /* Illumiatation Level 2 */
        {
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
        },
        /* Illumiatation Level 3 */
        {
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
        },
    },
    /* auto brightness on */
    {
        /* Illumiatation Level 1 */
        {
            tcon_tune_mode_ui,
            tcon_tune_mode_video,
            tcon_tune_mode_video,
            tcon_tune_mode_video,
            NULL,
            NULL,
            NULL,
            NULL,
            tcon_tune_mode_browser,
            NULL,
        },
        /* Illumiatation Level 2 */
        {
            tcon_tune_mode_saving,
            tcon_tune_mode_video,
            tcon_tune_mode_video,
            tcon_tune_mode_video,
            NULL,
            NULL,
            NULL,
            NULL,
            tcon_tune_mode_browser,
            NULL,    
        },
        /* Illumiatation Level 3 */
        {
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
            tcon_tune_mode_outdoor,
        }
    }
};

#endif

