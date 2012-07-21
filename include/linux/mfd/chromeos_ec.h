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

struct chromeos_ec_msg {
	u8 cmd;
	uint8_t *out_buf;
	int out_len;
	uint8_t *in_buf;
	int in_len;
};

struct chromeos_ec_device {
	struct device *dev;
	struct i2c_client *client;
	int irq;
	bool wake_enabled;
	struct blocking_notifier_head event_notifier;
	int (*command_send)(struct chromeos_ec_device *ec,
			char cmd, void *out_buf, int out_len);
	int (*command_recv)(struct chromeos_ec_device *ec,
			char cmd, void *in_buf, int in_len);
	int (*command_xfer)(struct chromeos_ec_device *ec,
			struct chromeos_ec_msg *msg);
	int (*command_raw)(struct chromeos_ec_device *ec,
			struct i2c_msg *msgs, int num);
};

#endif /* __LINUX_MFD_CHROMEOS_EC_H */
