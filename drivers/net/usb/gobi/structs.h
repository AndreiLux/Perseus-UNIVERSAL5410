/* structs.h - shared structures
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef QCUSBNET_STRUCTS_H
#define QCUSBNET_STRUCTS_H

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#include <linux/usb/usbnet.h>

#include <linux/fdtable.h>

extern int gobi_debug;

#define GOBI_LOG(lvl, fmt, arg...) \
do { \
	if (lvl <= gobi_debug) \
		printk(KERN_INFO "gobi: %s: " fmt, __func__, ##arg); \
} while (0)

#define GOBI_ERROR(fmt, arg...) GOBI_LOG(0, fmt, ##arg)
#define GOBI_WARN(fmt, arg...)  GOBI_LOG(1, fmt, ##arg)
#define GOBI_DEBUG(fmt, arg...) GOBI_LOG(2, fmt, ##arg)

struct qcusbnet;

struct urbreq {
	struct list_head node;
	struct urb *urb;
};

#define DEFAULT_READ_URB_LENGTH 0x1000

struct qmidev {
	dev_t devnum;
	struct class *devclass;
	struct cdev cdev;
	struct urb *readurb;
	struct urbsetup *readsetup;
	void *readbuf;
	struct urb *inturb;
	void *intbuf;
	struct list_head clients;
	spinlock_t clients_lock;
	atomic_t qmitid;
};

enum {
	DOWN_NO_NDIS_CONNECTION = 0,
	DOWN_CDC_CONNECTION_SPEED = 1,
	DOWN_DRIVER_SUSPENDED = 2,
	DOWN_NET_IFACE_STOPPED = 3,
};

#define MEID_SIZE 14
#define IMEI_SIZE 15

struct qcusbnet {
	struct list_head node;
	struct kref refcount;
	struct usbnet *usbnet;
	struct usb_interface *iface;
	int (*open)(struct net_device *);
	int (*stop)(struct net_device *);
	unsigned long down;
	bool valid;
	bool dying;
	struct qmidev qmi;
	char meid[MEID_SIZE];
	char imei[IMEI_SIZE];

	struct workqueue_struct *workqueue;
	spinlock_t urbs_lock;
	struct list_head urbs;
	struct urb *active;

	struct work_struct startxmit;
	struct work_struct txtimeout;
	struct work_struct complete;

	unsigned int iface_num;
	unsigned int int_in_endp;
	unsigned int bulk_in_endp;
	unsigned int bulk_out_endp;
};

#endif /* !QCUSBNET_STRUCTS_H */
