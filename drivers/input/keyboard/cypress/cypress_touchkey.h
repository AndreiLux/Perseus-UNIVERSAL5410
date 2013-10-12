#ifndef _CYPRESS_TOUCHKEY_H
#define _CYPRESS_TOUCHKEY_H

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

#include <linux/i2c/touchkey_i2c.h>

/* Touchkey Register */
#define KEYCODE_REG			0x00

#define TK_BIT_PRESS_EV		0x08
#define TK_BIT_KEYCODE		0x07

#define TK_BIT_AUTOCAL		0x80
#define TK_BIT_GLOVE		0x40
#define TK_BIT_TA_ON		0x10
#define TK_BIT_FW_ID_55		0x20
#define TK_BIT_FW_ID_65		0x04

#define TK_CMD_LED_ON		0x10
#define TK_CMD_LED_OFF		0x20

#define I2C_M_WR 0		/* for i2c */

#define TK_UPDATE_DOWN		1
#define TK_UPDATE_FAIL		-1
#define TK_UPDATE_PASS		0

#ifdef CONFIG_SEC_TSP_FACTORY
#endif

/* Flip cover*/
#define TKEY_FLIP_MODE

#ifdef TKEY_FLIP_MODE
#define TK_BIT_FLIP	0x08
#endif

/* Autocalibration */
#define TK_HAS_AUTOCAL

/* Generalized SMBus access */
#define TK_USE_GENERAL_SMBUS

/* Boot-up Firmware Update */
#define TK_HAS_FIRMWARE_UPDATE
#define TK_UPDATABLE_BD_ID	6

/* for HA */
#define FW_PATH "cypress/cypress_ha_m09.fw"
#define TKEY_MODULE07_HWID 8
#define TKEY_FW_PATH "/sdcard/cypress/fw.bin"

/*#define TK_USE_2KEY_TYPE_M0*/

/* LCD Type check*/
#if defined(CONFIG_HA)
#define TK_USE_LCDTYPE_CHECK
#endif

#if defined(TK_USE_4KEY_TYPE_ATT)\
	|| defined(TK_USE_4KEY_TYPE_NA)
#define TK_USE_4KEY
#elif defined(TK_USE_2KEY_TYPE_M0)\
	|| defined(TK_USE_2KEY_TYPE_U1)
#define TK_USE_2KEY
#endif

#define  TOUCHKEY_FW_UPDATEABLE_HW_REV  11
#define TOUCHKEY_BOOSTER
#ifdef TOUCHKEY_BOOSTER
#include <linux/pm_qos.h>
#define TKEY_BOOSTER_ON_TIME	500
#define TKEY_BOOSTER_OFF_TIME	500
#define TKEY_BOOSTER_CHG_TIME	130

enum BOOST_LEVEL {
	TKEY_BOOSTER_DISABLE = 0,
	TKEY_BOOSTER_LEVEL1,
	TKEY_BOOSTER_LEVEL2,
};

#define TKEY_BOOSTER_CPU_FREQ1 1600000
#define TKEY_BOOSTER_MIF_FREQ1 667000
#define TKEY_BOOSTER_INT_FREQ1 333000

#define TKEY_BOOSTER_CPU_FREQ2 650000
#define TKEY_BOOSTER_MIF_FREQ2 400000
#define TKEY_BOOSTER_INT_FREQ2 111000
#endif
/* #define TK_INFORM_CHARGER	1 */

/* #define TK_USE_OPEN_DWORK */
#ifdef TK_USE_OPEN_DWORK
#define	TK_OPEN_DWORK_TIME	10
#endif
#ifdef CONFIG_GLOVE_TOUCH
#define	TK_GLOVE_DWORK_TIME	300
#endif

#if defined(TK_INFORM_CHARGER)
struct touchkey_callbacks {
	void (*inform_charger)(struct touchkey_callbacks *, bool);
};
#endif

enum {
	FW_NONE = 0,
	FW_BUILT_IN,
	FW_HEADER,
	FW_IN_SDCARD,
	FW_EX_SDCARD,
};

struct fw_update_info {
	u8 fw_path;
	const struct firmware *firm_data;
	u8 *fw_data;
};

/*Parameters for i2c driver*/
struct touchkey_i2c {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct completion init_done;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;
	struct wake_lock fw_wakelock;
	struct device	*dev;
	int irq;
	int md_ver_ic; /*module ver*/
	int fw_ver_ic;
	int firmware_id;
	struct touchkey_platform_data *pdata;
	char *name;
	int (*power)(int on);
	int update_status;
	bool enabled;
#ifdef TOUCHKEY_BOOSTER
	bool tsk_dvfs_lock_status;
	struct delayed_work tsk_work_dvfs_off;
	struct delayed_work tsk_work_dvfs_chg;
	struct mutex tsk_dvfs_lock;
	struct pm_qos_request cpu_qos;
	struct pm_qos_request mif_qos;
	struct pm_qos_request int_qos;
	unsigned char boost_level;
	bool dvfs_signal;
#endif
#ifdef TK_USE_OPEN_DWORK
	struct delayed_work open_work;
#endif
#ifdef CONFIG_GLOVE_TOUCH
	struct delayed_work glove_change_work;
	bool tsk_glove_lock_status;
	bool tsk_glove_mode_status;
	struct mutex tsk_glove_lock;
#endif
#ifdef TK_INFORM_CHARGER
	struct touchkey_callbacks callbacks;
	bool charging_mode;
#endif
#ifdef TKEY_FLIP_MODE
	bool enabled_flip;
#endif
	bool status_update;
	struct work_struct update_work;
	struct fw_update_info update_info;
};

extern struct class *sec_class;
void touchkey_charger_infom(bool en);

extern unsigned int lcdtype;
extern unsigned int system_rev;
#endif /* _CYPRESS_TOUCHKEY_H */
