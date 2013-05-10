/*
 * LP55XX Common Driver Header
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Derived from leds-lp5521.h, leds-lp5523.h
 */

#ifndef __LINUX_LP55XX_COMMON_H
#define __LINUX_LP55XX_COMMON_H

enum lp55xx_engine_index {
	LP55XX_ENGINE_INVALID,
	LP55XX_ENGINE_1,
	LP55XX_ENGINE_2,
	LP55XX_ENGINE_3,
};

struct lp55xx_led;
struct lp55xx_chip;

/*
 * struct lp55xx_reg
 * @addr : Register address
 * @val  : Register value
 */
struct lp55xx_reg {
	u8 addr;
	u8 val;
};

/*
 * struct lp55xx_device_config
 * @max_channel        : Maximum number of channels
 * @reset              : Chip specific reset command
 * @enable             : Chip specific enable command
 * @post_init_device   : Chip specific initialization code
 * @set_led_current    : Current setting operation
 * @brightness_work_fn : Brightness work function
 * @run_engine         : Run internal engine for pattern
 * @firmware_cb        : Call function when the firmware is loaded
 * @dev_attr_group     : Device specific attributes
 */
struct lp55xx_device_config {
	const int max_channel;

	const struct lp55xx_reg reset;
	const struct lp55xx_reg enable;

	/* define if the device has specific initialization process */
	int (*post_init_device) (struct lp55xx_chip *chip);

	/* current setting function */
	void (*set_led_current) (struct lp55xx_led *led, u8 led_current);

	/* access brightness register */
	void (*brightness_work_fn)(struct work_struct *work);

	/* used for running firmware LED patterns */
	void (*run_engine) (struct lp55xx_chip *chip, bool start);

	/* access program memory when the firmware is loaded */
	void (*firmware_cb)(struct lp55xx_chip *chip);

	/* additional device specific attributes */
	const struct attribute_group *dev_attr_group;
};

/*
 * struct lp55xx_chip
 * @cl         : I2C communication for access registers
 * @pdata      : Platform specific data
 * @lock       : Lock for user-space interface
 * @cfg        : Device specific configuration data
 * @num_leds   : Number of registered LEDs
 * @engine_idx : Selected engine number
 * @fw         : Firmware data for running a LED pattern
 */
struct lp55xx_chip {
	struct i2c_client *cl;
	struct lp55xx_platform_data *pdata;
	struct mutex lock;	/* lock for user-space interface */
	struct lp55xx_device_config *cfg;
	int num_leds;
	enum lp55xx_engine_index engine_idx;
	const struct firmware *fw;
};

/*
 * struct lp55xx_led
 * @chan_nr         : Channel number
 * @cdev            : LED class device
 * @led_current     : Current setting at each led channel
 * @max_current     : Maximun current at each led channel
 * @brightness_work : Workqueue for brightness control
 * @brightness      : Brightness value
 * @chip            : The lp55xx chip data
 */
struct lp55xx_led {
	int chan_nr;
	struct led_classdev cdev;
	u8 led_current;
	u8 max_current;
	struct work_struct brightness_work;
	u8 brightness;
	struct lp55xx_chip *chip;
};

/* register access */
extern int lp55xx_write(struct lp55xx_chip *chip, u8 reg, u8 val);
extern int lp55xx_read(struct lp55xx_chip *chip, u8 reg, u8 *val);
extern int lp55xx_update_bits(struct lp55xx_chip *chip, u8 reg,
			u8 mask, u8 val);

/* common device init/deinit functions */
extern int lp55xx_init_device(struct lp55xx_chip *chip);
extern void lp55xx_deinit_device(struct lp55xx_chip *chip);

/* common LED class device functions */
extern int lp55xx_register_leds(struct lp55xx_led *led,
				struct lp55xx_chip *chip);
extern void lp55xx_unregister_leds(struct lp55xx_led *led,
				struct lp55xx_chip *chip);

/* common device attributes functions */
extern int lp55xx_register_sysfs(struct lp55xx_chip *chip);
extern void lp55xx_unregister_sysfs(struct lp55xx_chip *chip);

#endif /* __LINUX_LP55XX_COMMON_H */
