/*
 *  Copyright (C) 2012 Google, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * ChromeOS EC multi function device
 */

#ifndef __LINUX_MFD_CHROMEOS_EC_H
#define __LINUX_MFD_CHROMEOS_EC_H

struct i2c_msg;

#include <linux/mfd/chromeos_ec_commands.h>

/*
 * Command interface between EC and AP, for LPC, I2C and SPI interfaces.
 */
enum {
	EC_MSG_TX_HEADER_BYTES	= 3,
	EC_MSG_TX_TRAILER_BYTES	= 1,
	EC_MSG_TX_PROTO_BYTES	= EC_MSG_TX_HEADER_BYTES +
					EC_MSG_TX_TRAILER_BYTES,
	EC_MSG_RX_PROTO_BYTES	= 3,

	/* Max length of messages */
	EC_MSG_BYTES		= EC_HOST_PARAM_SIZE + EC_MSG_TX_PROTO_BYTES,

};

struct chromeos_ec_msg {
	u8 version;		/* Command version number */
	u8 cmd;
	uint8_t *out_buf;
	int out_len;
	uint8_t *in_buf;
	int in_len;
};

struct chromeos_ec_device {
	const char *name;		/* Name of this EC interface */
	void *priv;			/* Private data */
	int irq;
	/*
	 * These two buffers will always be dword-aligned and include enough
	 * space for up to 7 word-alignment bytes also, so we can ensure that
	 * the body of the message is always dword-aligned (64-bit).
	 *
	 * We use this alignment to keep ARM and x86 happy. Probably word
	 * alignment would be OK, there might be a small performance advantage
	 * to using dword.
	 */
	uint8_t *din;
	uint8_t *dout;
	int din_size;		/* Size of din buffer */
	int dout_size;		/* Size of dout buffer */
	int (*command_send)(struct chromeos_ec_device *ec,
			char cmd, void *out_buf, int out_len);
	int (*command_recv)(struct chromeos_ec_device *ec,
			char cmd, void *in_buf, int in_len);
	int (*command_xfer)(struct chromeos_ec_device *ec,
			struct chromeos_ec_msg *msg);
	int (*command_i2c)(struct chromeos_ec_device *ec,
			struct i2c_msg *msgs, int num);

	/* @return name of EC device (e.g. 'chromeos-ec') */
	const char *(*get_name)(struct chromeos_ec_device *ec_dev);

	/* @return name of physical comms layer (e.g. 'i2c-4') */
	const char *(*get_phys_name)(struct chromeos_ec_device *ec_dev);

	/* @return pointer to parent device (e.g. i2c or spi device) */
	struct device *(*get_parent)(struct chromeos_ec_device *ec_dev);

	/* These are --private-- fields - do not assign */
	struct device *dev;
	struct mutex dev_lock;		/* Only one access to device at time */
	bool wake_enabled;
	struct blocking_notifier_head event_notifier;

};

/**
 * Handle a suspend operation for the ChromeOS EC device
 *
 * This can be called by drivers to handle a suspend event.
 *
 * @param ec_dev	Device to suspend
 * @return 0 if ok, -ve on error
 */
int cros_ec_suspend(struct chromeos_ec_device *ec_dev);

/**
 * Handle a resume operation for the ChromeOS EC device
 *
 * This can be called by drivers to handle a resume event.
 *
 * @param ec_dev		Device to resume
 * @return 0 if ok, -ve on error
 */
int cros_ec_resume(struct chromeos_ec_device *ec_dev);

/**
 * Prepare an outgoing message in the output buffer
 *
 * This is intended to be used by all ChromeOS EC drivers, but at present
 * only SPI uses it. Once LPC uses the same protocol it can start using it.
 * I2C could use it now, with a refactor of the existing code.
 *
 * @param ec_dev	Device to register
 * @param msg		Message to write
 */
int cros_ec_prepare_tx(struct chromeos_ec_device *ec_dev,
		       struct chromeos_ec_msg *msg);

/**
 * Remove a ChromeOS EC
 *
 * Call this to deregister a ChromeOS EC. After this you should call
 * cros_ec_free().
 *
 * @param ec_dev		Device to register
 * @return 0 if ok, -ve on error
 */
int __devinit cros_ec_remove(struct chromeos_ec_device *ec_dev);

/**
 * Register a new ChromeOS EC, using the provided information
 *
 * Before calling this, call cros_ec_alloc() to get a pointer to a new device
 * and then fill in all the fields up to the --private-- marker.
 *
 * @param ec_dev		Device to register
 * @return 0 if ok, -ve on error
 */
int __devinit cros_ec_register(struct chromeos_ec_device *ec_dev);

/**
 * Allocate a new ChromeOS EC
 *
 * @param name		Name of EC (typically the interface it connects on)
 * @return pointer to created device, or NULL on failure
 */
struct chromeos_ec_device *__devinit cros_ec_alloc(const char *name);

/**
 * Free a ChromeOS EC, which just does the opposite of cros_ec_alloc().
 *
 * @param ec_dev		Device to free (call cros_ec_remove() first)
 */
void __devexit cros_ec_free(struct chromeos_ec_device *ec_dev);

#endif /* __LINUX_MFD_CHROMEOS_EC_H */
