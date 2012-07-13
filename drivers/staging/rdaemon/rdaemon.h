/*
 * drivers/staging/rdaemon/rdaemon.h
 *
 * Copyright (C) 2012 Texas Instruments
 * Author: Guillaume Aubertin <g-aubertin@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef RDAEMON_H
#define RDAEMON_H

/* message type */
enum rdaemon_msg_type {
	RDAEMON_PING,
};

/* message frame */
struct rdaemon_msg_frame {
	int msg_type;
	u32 data;
};

#endif /* RDAEMON_H */
