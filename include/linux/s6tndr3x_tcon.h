/*
 * s6tndr3x_tcon.h
 * Samsung Mobile S6TNDR3X TCON Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __S6TNDR3X_TCON_H__
#define __S6TNDR3X_TCON_H__ __FILE__

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define S6TNDR3X_I2C_SLAVEADDR 0x10

#define TCON_CONNECT_I2C_GPIO

#define SBOW_REG_PAGE   0x05
#define SBOW_REG_ADDR   0x76
#define SBOW_OFFSET     0x0
#define SBOW_REG_MASK   0xff

#define RWT_REG_PAGE    0x05
#define RWT_REG_ADDR    0x2D
#define RWT_OFFSET      0x0
#define RWT_REG_MASK    0x0f

#define GWT_REG_PAGE    0x05
#define GWT_REG_ADDR    0x2D
#define GWT_OFFSET      0x04
#define GWT_REG_MASK    0xf0

#define THH1_REG_PAGE   0x05
#define THH1_REG_ADDR   0x30
#define THH1_OFFSET     0x0
#define THH1_REG_MASK   0x0f

#define THH2_REG_PAGE   0x05
#define THH2_REG_ADDR   0x30
#define THH2_OFFSET     0x04
#define THH2_REG_MASK   0xf0

#define GMIN_REG_PAGE   0x05
#define GMIN_REG_ADDR   0x26
#define GMIN_OFFSET     0x0
#define GMIN_REG_MASK   0xff

#define BLE_REG_PAGE    0x05
#define BLE_REG_ADDR    0x2A
#define BLE_OFFSET      0x00
#define BLE_REG_MASK    (0x01 << BLE_OFFSET)

#define SVE_REG_PAGE    0x05
#define SVE_REG_ADDR    0x2A
#define SVE_OFFSET      0x01
#define SVE_REG_MASK    (0x01 << SVE_OFFSET)

#define DCE_REG_PAGE    0x05
#define DCE_REG_ADDR    0x2A
#define DCE_OFFET       0x04
#define DCE_REG_MASK    (0x01 << DCE_OFFET)

#define LDE_REG_PAGE    0x05
#define LDE_REG_ADDR    0x2A
#define LDE_OFFSET      0x05
#define LDE_REG_MASK    (0x01 << LDE_OFFSET)

#define BDE_REG_PAGE    0x05
#define BDE_REG_ADDR    0x2A
#define BDE_OFFSET      0x06
#define BDE_REG_MASK    (0x01 << BDE_OFFSET)

#define MNBL_REG_PAGE   0x05
#define MNBL_REG_ADDR   0x32
#define MNBL_OFFSET     0x00
#define MNBL_REG_MASK   0x00

#define BWT_REG_PAGE    0x05
#define BWT_REG_ADDR    0x2E
#define BWT_OFFSET      0x0
#define BWT_REG_MASK    0x0f

#define WWT_REG_PAGE    0x05
#define WWT_REG_ADDR    0x2E
#define WWT_OFFSET      0x04
#define WWT_REG_MASK    0xf0

#define YWT_REG_PAGE    0x05
#define YWT_REG_ADDR    0x2F
#define YWT_OFFSET      0
#define YWT_REG_MASK    0x0f

#define THL1_REG_PAGE   0x05
#define THL1_REG_ADDR   0x31
#define THL1_OFFSET     0x00
#define THL1_REG_MASK   0x0f

#define THL2_REG_PAGE   0x05
#define THL2_REG_ADDR   0x31
#define THL2_OFFSET     0x04
#define THL2_REG_MASK   0xf0

#define WSLB_REG_PAGE   0x05
#define WSLB_REG_ADDR   0x24
#define WSLB_OFFSET     0x0
#define WSLB_REG_MASK   0x0f

#define WR_REG_PAGE     0x05
#define WR_REG_ADDR     0x24
#define WR_OFFSET       0x04
#define WR_REG_MASK     0xf0

#define FXBL_REG_PAGE   0x05
#define FXBL_REG_ADDR   0x2B
#define FXBL_OFFSET     0x0
#define FXBL_REG_MASK   0xff

#define FXSV_REG_PAGE   0x05
#define FXSV_REG_ADDR   0x2C
#define FXSV_OFFSET    0x0
#define FXSV_REG_MASK   0xff

#define N_REG_PAGE      0x05
#define N_REG_ADDR      0x25
#define N_OFFSET        0x0
#define N_REG_MASK      0x0f

#define END_REG_PAGE    0xff
#define END_REG_ADDR    0xff
#define END_OFFSET      0x0
#define END_REG_MASK    0xff


struct tcon_val {
	unsigned char page;
	unsigned char addr;
	unsigned char value;
};


struct tcon_reg_info {
	unsigned char page;
	unsigned char addr;
	unsigned char mask;
	unsigned char value;
};


struct s6tndr3x_priv_data {
    unsigned int tcon_ready;
    unsigned int decay_cnt;
    unsigned char *decay_level;
    unsigned int max_br;
    struct tcon_val **decay_tune_val;
};


#define MAX_LOOKTBL_CNT     11
#define BL_LOOKUP_RES       10
#define MAX_DECAY_TUNE_REG  20


struct s6tndr3x_data {
	struct i2c_client *client;
	struct device   *dev;

	unsigned int mode;
	unsigned int auto_br;
	unsigned int lux;
    unsigned int curr_decay;
    struct mutex	lock;

    struct s6tndr3x_priv_data *pdata;

    unsigned char bl_lookup_tbl[MAX_LOOKTBL_CNT];    
};


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
    TCON_MODE_MAX,
};


enum tcon_illu {
    TCON_LEVEL_1 = 0,   /* 0 ~ 1.5k lux*/
    TCON_LEVEL_2,       /* 1k ~ 30k lux*/
    TCON_LEVEL_3,       /* 30k lux */
    TCON_LEVEL_MAX,
};


enum tcon_auto_br {
    TCON_AUTO_BR_OFF = 0,
    TCON_AUTO_BR_ON, 
    TCON_AUTO_BR_MAX,
};


#define TCON_DEVICE_RW_ATTR(_name) \
	static DEVICE_ATTR(_name, 660, show_##_name, store_##_name)

#define TCON_SET_VALUE(REG, val) (val << REG##_OFFSET)

#define TCON_TUNE_ADDR(REG, val) {\
	.page = REG##_REG_PAGE,\
	.addr = REG##_REG_ADDR,\
	.mask = REG##_REG_MASK,\
	.value = TCON_SET_VALUE(REG, val)\
}

#define CONFIG_TCON_TUNING 1

//int s6tndr3x_tune_value(u8 force);
int s6tndr3x_tune(u8 ready);
int s6tndr3x_tune_decay(unsigned char ratio);

#endif /* __S6TNDR3X_TCON_H__ */
