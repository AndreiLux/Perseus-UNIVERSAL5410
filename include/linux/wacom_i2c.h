
#ifndef _LINUX_WACOM_I2C_H
#define _LINUX_WACOM_I2C_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define NAMEBUF 12
#define WACNAME "WAC_I2C_EMR"
#define WACFLASH "WAC_I2C_FLASH"

#ifdef CONFIG_EPEN_WACOM_G9PM
#define WACOM_FW_SIZE 61440
#elif defined(CONFIG_EPEN_WACOM_G9PLL)
#define WACOM_FW_SIZE 65535
#else
#define WACOM_FW_SIZE 32768
#endif

/*Wacom Command*/
#if defined(CONFIG_MACH_T0)
#define COM_COORD_NUM	8
#else
#define COM_COORD_NUM	7
#endif
#define COM_QUERY_NUM	9

#define COM_SAMPLERATE_STOP 0x30
#define COM_SAMPLERATE_40  0x33
#define COM_SAMPLERATE_80  0x32
#define COM_SAMPLERATE_133 0x31
#define COM_SURVEYSCAN     0x2B
#define COM_QUERY          0x2A
#define COM_FLASH          0xff
#define COM_CHECKSUM       0x63

/*I2C address for digitizer and its boot loader*/
#define WACOM_I2C_ADDR 0x56
#if defined(CONFIG_EPEN_WACOM_G9PL) \
	|| defined(CONFIG_EPEN_WACOM_G9PLL)
#define WACOM_I2C_BOOT 0x09
#else
#define WACOM_I2C_BOOT 0x57
#endif

/*Information for input_dev*/
#define EMR 0
#define WACOM_PKGLEN_I2C_EMR 0

/*Enable/disable irq*/
#define ENABLE_IRQ 1
#define DISABLE_IRQ 0

/*Special keys*/
#define EPEN_TOOL_PEN		0x220
#define EPEN_TOOL_RUBBER	0x221
#define EPEN_STYLUS			0x22b
#define EPEN_STYLUS2		0x22c

#define WACOM_DELAY_FOR_RST_RISING 200
/* #define INIT_FIRMWARE_FLASH */

#define WACOM_PDCT_WORK_AROUND
#define WACOM_USE_QUERY_DATA

/*PDCT Signal*/
#define PDCT_NOSIGNAL 1
#define PDCT_DETECT_PEN 0

#define WACOM_PRESSURE_MAX 255

/*Digitizer Type*/
#define EPEN_DTYPE_B660	1
#define EPEN_DTYPE_B713 2
#define EPEN_DTYPE_B746 3
#define EPEN_DTYPE_B804 4
#define EPEN_DTYPE_B878 5

#define EPEN_DTYPE_B887 6
#define EPEN_DTYPE_B911 7

#define WACOM_I2C_MODE_BOOT 1
#define WACOM_I2C_MODE_NORMAL 0

#if defined(CONFIG_MACH_V1)

#define WACOM_DVFS_LOCK_FREQ 800000
#ifdef CONFIG_SEC_TOUCHSCREEN_DVFS_LOCK
#define SEC_BUS_LOCK
#endif

#define WACOM_HAVE_FWE_PIN

#define WACOM_USE_SOFTKEY

#define WACOM_X_INVERT 0
#define WACOM_XY_SWITCH 0

#define BATTERY_SAVING_MODE

#define WACOM_MAX_COORD_X 26266
#define WACOM_MAX_COORD_Y 16416
#define WACOM_MAX_PRESSURE 1023

#define WACOM_PEN_DETECT

/* For Android origin */
/*v1, both origins are same */
#define WACOM_POSX_MAX WACOM_MAX_COORD_X
#define WACOM_POSY_MAX WACOM_MAX_COORD_Y

#define COOR_WORK_AROUND

#define WACOM_DTYPE_B804_HWID 0
#define WACOM_DTYPE_B878_HWID 3

#elif defined(CONFIG_MACH_HA)
#define WACOM_MAX_COORD_X 12576
#define WACOM_MAX_COORD_Y 7074
#define WACOM_MAX_PRESSURE 1023
#define WACOM_USE_PDATA

#define WACOM_USE_SOFTKEY

/* For Android origin */
#define WACOM_POSX_MAX WACOM_MAX_COORD_Y
#define WACOM_POSY_MAX WACOM_MAX_COORD_X

#define COOR_WORK_AROUND

#define WACOM_IMPORT_FW_ALGO
/*#define WACOM_USE_OFFSET_TABLE*/
#define WACOM_USE_AVERAGING
#if 0
#define WACOM_USE_AVE_TRANSITION
#define WACOM_USE_BOX_FILTER
#define WACOM_USE_TILT_OFFSET
#define WACOM_USE_HEIGHT
#endif

#define MAX_ROTATION	4
#define MAX_HAND		2

#define WACOM_PEN_DETECT

/* origin offset */
#define EPEN_B660_ORG_X 456
#define EPEN_B660_ORG_Y 504

#define EPEN_B713_ORG_X 676
#define EPEN_B713_ORG_Y 724

/*Box Filter Parameters*/
#define  X_INC_S1  1500
#define  X_INC_E1  (WACOM_MAX_COORD_X - 1500)
#define  Y_INC_S1  1500
#define  Y_INC_E1  (WACOM_MAX_COORD_Y - 1500)

#define  Y_INC_S2  500
#define  Y_INC_E2  (WACOM_MAX_COORD_Y - 500)
#define  Y_INC_S3  1100
#define  Y_INC_E3  (WACOM_MAX_COORD_Y - 1100)

//#define CONFIG_SEC_TOUCHSCREEN_DVFS_LOCK
#define WACOM_DVFS_LOCK_FREQ 800000
#define BATTERY_SAVING_MODE

/*HWID to distinguish Detect Switch*/
#define WACOM_DETECT_SWITCH_HWID 0xFFFF

/*HWID to distinguish FWE1*/
#define WACOM_FWE1_HWID 0xFFFF

/*HWID to distinguish B911 Digitizer*/
#define WACOM_DTYPE_B911_HWID 1

#endif /*End of Model config*/

#ifndef WACOM_X_INVERT
#define WACOM_X_INVERT 1
#endif
#ifndef WACOM_Y_INVERT
#define WACOM_Y_INVERT 0
#endif
#ifndef WACOM_XY_SWITCH
#define WACOM_XY_SWITCH 1
#endif

#if !defined(WACOM_SLEEP_WITH_PEN_SLP)
#define WACOM_SLEEP_WITH_PEN_LDO_EN
#endif

#ifdef BATTERY_SAVING_MODE
#ifndef WACOM_PEN_DETECT
#define WACOM_PEN_DETECT
#endif
#endif

#ifdef WACOM_USE_PDATA
#undef WACOM_USE_QUERY_DATA
#endif

/*Parameters for wacom own features*/
struct wacom_features {
	int x_max;
	int y_max;
	int pressure_max;
	char comstat;
	u8 data[COM_COORD_NUM];
	unsigned int fw_version;
	int firm_update_status;
};

/*sec_class sysfs*/
extern struct class *sec_class;

static struct wacom_features wacom_feature_EMR = {
	.x_max = WACOM_MAX_COORD_X,
	.y_max = WACOM_MAX_COORD_Y,
	.pressure_max = WACOM_MAX_PRESSURE,
	.comstat = COM_QUERY,
	.data = {0, 0, 0, 0, 0, 0, 0},
	.fw_version = 0x0,
	.firm_update_status = 0,
};

struct wacom_g5_callbacks {
	int (*check_prox)(struct wacom_g5_callbacks *);
};

struct wacom_g5_platform_data {
	char *name;
	int x_invert;
	int y_invert;
	int xy_switch;
	int min_x;
	int max_x;
	int min_y;
	int max_y;
	int max_pressure;
	int min_pressure;
	int gpio_pendct;
#ifdef WACOM_PEN_DETECT
	int gpio_pen_insert;
#endif
	void (*compulsory_flash_mode)(bool);
	int (*init_platform_hw)(void);
	int (*exit_platform_hw)(void);
	int (*suspend_platform_hw)(void);
	int (*resume_platform_hw)(void);
#ifdef CONFIG_HAS_EARLYSUSPEND
	int (*early_suspend_platform_hw)(void);
	int (*late_resume_platform_hw)(void);
#endif
	int (*reset_platform_hw)(void);
	void (*register_cb)(struct wacom_g5_callbacks *);
};

/*Parameters for i2c driver*/
struct wacom_i2c {
	struct i2c_client *client;
	struct i2c_client *client_boot;
	struct completion init_done;
	struct input_dev *input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;
	struct wake_lock wakelock;
	struct device	*dev;
	int irq;
#ifdef WACOM_PDCT_WORK_AROUND
	int irq_pdct;
	bool rdy_pdct;
#endif
	int pen_pdct;
	int gpio;
	int irq_flag;
	int pen_prox;
	int pen_pressed;
	int side_pressed;
	int tool;
	s16 last_x;
	s16 last_y;
#ifdef WACOM_PEN_DETECT
	struct delayed_work pen_insert_dwork;
	bool pen_insert;
	int gpio_pen_insert;
#ifdef CONFIG_MACH_T0
	int invert_pen_insert;
#endif
#endif
	int gpio_fwe;
#ifdef WACOM_IMPORT_FW_ALGO
	bool use_offset_table;
	bool use_aveTransition;
#endif
	bool checksum_result;
	const char name[NAMEBUF];
	struct wacom_features *wac_feature;
	struct wacom_g5_platform_data *wac_pdata;
	struct wacom_g5_callbacks callbacks;
	int (*power)(int on);
	struct delayed_work resume_work;
#ifdef CONFIG_SEC_TOUCHSCREEN_DVFS_LOCK
	unsigned int cpufreq_level;
	bool dvfs_lock_status;
	struct delayed_work dvfs_work;
	struct device *bus_dev;
#endif
#ifdef WACOM_CONNECTION_CHECK
	bool connection_check;
#endif
#ifdef BATTERY_SAVING_MODE
	bool battery_saving_mode;
#endif
	bool power_enable;
	bool boot_mode;
	bool query_status;
};

#endif /* _LINUX_WACOM_I2C_H */
