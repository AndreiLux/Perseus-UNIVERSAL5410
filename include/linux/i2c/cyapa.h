/*
 * Cypress APA I2C Touchpad Driver
 *
 * Author: Dudley Du <dudl@cypress.com>
 *
 * Copyright (C) 2011 Cypress Semiconductor, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_I2C_CYAPA_H
#define __LINUX_I2C_CYAPA_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define CYAPA_I2C_NAME  "cyapa"

/* APA trackpad firmware generation */
enum cyapa_gen {
	CYAPA_GEN1 = 0x01,  /* only one finger supported. */
	CYAPA_GEN2 = 0x02,  /* max five fingers supported. */
	CYAPA_GEN3 = 0x03,  /* support MT-protocol B with tracking ID. */
};

struct cyapa_trackpad_run_mode {
	__u8 run_mode;
	__u8 bootloader_state;
	/*
	 * rev_cmd is only use it when sending run mode switch command.
	 * in other situation, this field should be reserved and set to 0.
	 */
	__u8 rev_cmd;
};

#define CYAPA_OPERATIONAL_MODE 0x00
#define CYAPA_BOOTLOADER_MODE  0x01
#define CYAPA_BOOTLOADER_IDLE_STATE  0x00
#define CYAPA_BOOTLOADER_ACTIVE_STATE 0x01
#define CYAPA_BOOTLOADER_INVALID_STATE 0xff

/* trackpad run mode switch command. */
enum cyapa_bl_cmd {
	CYAPA_CMD_APP_TO_IDLE = 0x10,
	CYAPA_CMD_IDLE_TO_ACTIVE = 0x20,
	CYAPA_CMD_ACTIVE_TO_IDLE = 0x30,
	CYAPA_CMD_IDLE_TO_APP = 0x40,
};

#endif  /* __LINUX_I2C_CYAPA_H */
