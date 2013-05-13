#ifndef _LINUX_CYPRESS_TOUCHKEY_I2C_H
#define _LINUX_CYPRESS_TOUCHKEY_I2C_H

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

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

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

/* Firmware Version */
#define TK_FIRMWARE_VER_45  0x09
#define TK_FIRMWARE_VER_55  0x0c
#define TK_FIRMWARE_VER_65  0x10
#define TK_MODULE_VER    0x09
#define TK_MODULE_20045		45
#define TK_MODULE_20055		55
#define TK_MODULE_20065		65

/* Flip cover*/
#define TKEY_FLIP_MODE

#ifdef TKEY_FLIP_MODE
#define TK_BIT_FLIP	0x08
#endif

/* LDO Regulator */
#define	TK_REGULATOR_NAME	"vtouch_1.8v"

/* LDO Regulator */
#define	TK_LED_REGULATOR_NAME	"vtouch_3.3v"

/* LED LDO Type*/
#define LED_LDO_WITH_REGULATOR

/* Autocalibration */
#define TK_HAS_AUTOCAL

/* Generalized SMBus access */
#define TK_USE_GENERAL_SMBUS

/* Boot-up Firmware Update */
#define TK_HAS_FIRMWARE_UPDATE

/*#define TK_USE_2KEY_TYPE_M0*/

/* LCD Type check*/
#if !defined(CONFIG_MACH_HA)
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
#define TOUCH_BOOSTER_OFF_TIME	300
#define TOUCH_BOOSTER_CHG_TIME	200
#endif
#define TK_INFORM_CHARGER	1

#define TK_USE_OPEN_DWORK
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

struct touchkey_platform_data {
	int gpio_sda;
	int gpio_scl;
	int gpio_int;
	void (*init_platform_hw)(void);
	int (*suspend) (void);
	int (*resume) (void);
	int (*power_on) (bool);
	int (*led_power_on) (bool);
	int (*reset_platform_hw)(void);
	void (*register_cb)(void *);
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
	struct device	*dev;
	int irq;
	int module_ver;
	int firmware_ver;
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
	struct pm_qos_request tsk_cpu_qos;
	struct pm_qos_request tsk_mif_qos;
	struct pm_qos_request tsk_int_qos;
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

};

extern struct class *sec_class;
void touchkey_charger_infom(bool en);

extern unsigned int lcdtype;
#endif /* _LINUX_CYPRESS_TOUCHKEY_I2C_H */
