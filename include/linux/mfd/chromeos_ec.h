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
 * and MKBP (matrix keyboard protocol) message-based protocol definitions.
 */

#ifndef __LINUX_MFD_CHROMEOS_EC_H
#define __LINUX_MFD_CHROMEOS_EC_H

enum {
	/* Mask to convert a command byte into a command */
	MKBP_MSG_TRAILER_BYTES	= 1,
	MKBP_MSG_PROTO_BYTES	= MKBP_MSG_TRAILER_BYTES,
};

/* The EC command codes */

enum message_cmd_t {
	/* EC control/status messages */
	MKBP_CMDC_PROTO_VER =	0x00,	/* Read protocol version */
	MKBP_CMDC_NOP =		0x01,	/* No operation / ping */
	MKBP_CMDC_ID =		0x02,	/* Read EC ID */

	/* Functional messages */
	MKBP_CMDC_KEY_STATE =	0x20,	/* Read key state */
};


struct chromeos_ec_device {
	struct device *dev;
	struct i2c_client *client;
	int irq;
	struct blocking_notifier_head event_notifier;
	int (*send_command)(struct chromeos_ec_device *ec,
			char cmd, uint8_t *buf, int buf_len);
};

#endif /* __LINUX_MFD_CHROMEOS_EC_H */
