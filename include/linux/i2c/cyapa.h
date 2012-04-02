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
#define CYAPA_MISC_NAME  "cyapa"

/* APA trackpad firmware generation */
enum cyapa_gen {
	CYAPA_GEN1 = 0x01,  /* only one finger supported. */
	CYAPA_GEN2 = 0x02,  /* max five fingers supported. */
	CYAPA_GEN3 = 0x03,  /* support MT-protocol B with tracking ID. */
};

/*
 * Data structures for /dev/cyapa device ioctl read/write.
 */
struct cyapa_misc_ioctl_data {
	__u8 *buf;  /* pointer to a buffer for read/write data. */
	__u16 len;  /* valid data length in buf. */
	__u16 flag;  /* additional flag to special ioctl command. */
	__u16 rev;  /* reserved. */
};

struct cyapa_driver_ver {
	__u8 major_ver;
	__u8 minor_ver;
	__u8 revision;
};

struct cyapa_firmware_ver {
	__u8 major_ver;
	__u8 minor_ver;
};

struct cyapa_hardware_ver {
	__u8 major_ver;
	__u8 minor_ver;
};

struct cyapa_protocol_ver {
	__u8 protocol_gen;
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

/*
 * Macro codes for misc device ioctl functions.
 ***********************************************************
 |device type|serial num|direction| data  bytes |
 |-----------|----------|---------|-------------|
 | 8 bit     |  8 bit   |  2 bit  | 8~14 bit    |
 |-----------|----------|---------|-------------|
 ***********************************************************
 */
#define CYAPA_IOC_MAGIC 'C'
#define CYAPA_IOC(nr) _IOC(_IOC_NONE, CYAPA_IOC_MAGIC, nr, 0)
/* bytes value is the location of the data read/written by the ioctl. */
#define CYAPA_IOC_R(nr, bytes) _IOC(IOC_OUT, CYAPA_IOC_MAGIC, nr, bytes)
#define CYAPA_IOC_W(nr, bytes) _IOC(IOC_IN, CYAPA_IOC_MAGIC, nr, bytes)
#define CYAPA_IOC_RW(nr, bytes) _IOC(IOC_INOUT, CYAPA_IOC_MAGIC, nr, bytes)

/*
 * The following ioctl commands are only valid
 * when firmware working in operational mode.
 */
#define CYAPA_GET_PRODUCT_ID  CYAPA_IOC_R(0x00, 16)
#define CYAPA_GET_DRIVER_VER  CYAPA_IOC_R(0x01, 3)
#define CYAPA_GET_FIRMWARE_VER  CYAPA_IOC_R(0x02, 2)
#define CYAPA_GET_HARDWARE_VER  CYAPA_IOC_R(0x03, 2)
#define CYAPA_GET_PROTOCOL_VER  CYAPA_IOC_R(0x04, 1)

#define CYAPA_GET_TRACKPAD_RUN_MODE CYAPA_IOC_R(0x40, 2)
#define CYAYA_SEND_MODE_SWITCH_CMD CYAPA_IOC(0x50)

#endif  /* __LINUX_I2C_CYAPA_H */
