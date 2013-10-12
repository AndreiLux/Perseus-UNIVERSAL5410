#ifndef _GPIO_KEYS_H
#define _GPIO_KEYS_H

struct device;

#ifdef CONFIG_MACH_UNIVERSAL5420
#define KEY_BOOSTER
#endif

#ifdef KEY_BOOSTER
#include <linux/pm_qos.h>
#define KEY_BOOSTER_ON_TIME	500
#define KEY_BOOSTER_OFF_TIME	500
#define KEY_BOOSTER_CHG_TIME	130

#define KEY_BOOSTER_CPU_FREQ1 650000
#define KEY_BOOSTER_MIF_FREQ1 400000
#define KEY_BOOSTER_INT_FREQ1 111000

#endif

struct gpio_keys_button {
	/* Configuration parameters */
	unsigned int code;	/* input event code (KEY_*, SW_*) */
	int gpio;		/* -1 if this key does not support gpio */
	int active_low;
	const char *desc;
	unsigned int type;	/* input event type (EV_KEY, EV_SW, EV_ABS) */
	int wakeup;		/* configure the button as a wake-up source */
	int debounce_interval;	/* debounce ticks interval in msecs */
	bool can_disable;
	int value;		/* axis value for EV_ABS */
	unsigned int irq;	/* Irq number in case of interrupt keys */
	void (*isr_hook)(unsigned int code, int value);	/*key callback function */
};

struct gpio_keys_platform_data {
	struct gpio_keys_button *buttons;
	int nbuttons;
	unsigned int poll_interval;	/* polling interval in msecs -
					   for polling driver only */
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	const char *name;		/* input device name */

#ifdef CONFIG_SENSORS_HALL
	int gpio_flip_cover;
#endif
};

extern struct class *sec_class;
#endif
