/* qmidevice.c - gobi QMI device
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

#include "qmidevice.h"
#include "qcusbnet.h"

#include <linux/module.h>
#include <linux/compat.h>

struct readreq {
	struct list_head node;
	void *data;
	u16 tid;
	u16 size;
};

struct notifyreq {
	struct list_head node;
	void (*func)(struct qcusbnet *, u16, void *);
	u16  tid;
	void *data;
};

struct client {
	struct list_head node;
	u16 cid;
	struct list_head reads;
	struct list_head notifies;
	struct list_head urbs;
};

struct urbsetup {
	u8 type;
	u8 code;
	u16 value;
	u16 index;
	u16 len;
};

struct qmihandle {
	u16 cid;
	struct qcusbnet *dev;
};

extern int qcusbnet_debug;
static int qcusbnet2k_fwdelay;

static bool device_valid(struct qcusbnet *dev);
static struct client *client_bycid(struct qcusbnet *dev, u16 cid);
static bool client_addread(struct qcusbnet *dev, u16 cid, u16 tid, void *data, u16 size);
static bool client_delread(struct qcusbnet *dev, u16 cid, u16 tid, void **data, u16 *size);
static bool client_addnotify(struct qcusbnet *dev, u16 cid, u16 tid,
			     void (*hook)(struct qcusbnet *, u16 cid, void *),
			     void *data);
static bool client_notify(struct qcusbnet *dev, u16 cid, u16 tid);
static bool client_addurb(struct qcusbnet *dev, u16 cid, struct urb *urb);
static struct urb *client_delurb(struct qcusbnet *dev, u16 cid);

static int resubmit_int_urb(struct urb *urb);

static int devqmi_open(struct inode *inode, struct file *file);
static long devqmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
static int devqmi_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#endif
static int devqmi_release(struct inode *inode, struct file *file);
static ssize_t devqmi_read(struct file *file, char __user *buf, size_t size,
			   loff_t *pos);
static ssize_t devqmi_write(struct file *file, const char __user *buf,
			    size_t size, loff_t *pos);

static bool qmi_ready(struct qcusbnet *dev, u16 timeout);
static void wds_callback(struct qcusbnet *dev, u16 cid, void *data);
static int setup_wds_callback(struct qcusbnet *dev);
static int qmidms_getmeid(struct qcusbnet *dev);

#define IOCTL_QMI_GET_SERVICE_FILE      (0x8BE0 + 1)
#define IOCTL_QMI_GET_DEVICE_VIDPID     (0x8BE0 + 2)
#define IOCTL_QMI_GET_DEVICE_MEID       (0x8BE0 + 3)
#define IOCTL_QMI_CLOSE                 (0x8BE0 + 4)
#define CDC_GET_ENCAPSULATED_RESPONSE	0x01A1ll
#define CDC_CONNECTION_SPEED_CHANGE	0x08000000002AA1ll

static const struct file_operations devqmi_fops = {
	.owner   = THIS_MODULE,
	.read    = devqmi_read,
	.write   = devqmi_write,
	.unlocked_ioctl   = devqmi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = devqmi_compat_ioctl,
#endif
	.open    = devqmi_open,
	.release = devqmi_release,
};

#ifdef CONFIG_SMP
static inline void assert_locked(struct qcusbnet *dev)
{
	BUG_ON(!spin_is_locked(&dev->qmi.clients_lock));
}
#else
static inline void assert_locked(struct qcusbnet *dev)
{

}
#endif

static bool device_valid(struct qcusbnet *dev)
{
	return dev && dev->valid;
}

void qc_setdown(struct qcusbnet *dev, u8 reason)
{
	set_bit(reason, &dev->down);
	netif_carrier_off(dev->usbnet->net);
}

void qc_cleardown(struct qcusbnet *dev, u8 reason)
{
	clear_bit(reason, &dev->down);
	if (!dev->down)
		netif_carrier_on(dev->usbnet->net);
}

bool qc_isdown(struct qcusbnet *dev, u8 reason)
{
	return test_bit(reason, &dev->down);
}

static int resubmit_int_urb(struct urb *urb)
{
	int status;
	int interval;
	if (!urb || !urb->dev)
		return -EINVAL;
	interval = urb->dev->speed == USB_SPEED_HIGH ? 7 : 3;
	usb_fill_int_urb(urb, urb->dev, urb->pipe, urb->transfer_buffer,
	                 urb->transfer_buffer_length, urb->complete,
	                 urb->context, interval);
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status)
		DBG("status %d", status);
	return status;
}

static void read_callback(struct urb *urb)
{
	struct list_head *node;
	int result;
	u16 cid;
	struct client *client;
	void *data;
	void *copy;
	u16 size;
	struct qcusbnet *dev;
	unsigned long flags;
	u16 tid;

	if (!urb) {
		DBG("bad read URB\n");
		return;
	}

	dev = urb->context;
	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return;
	}

	if (urb->status) {
		DBG("Read status = %d\n", urb->status);
		resubmit_int_urb(dev->qmi.inturb);
		return;
	}

	DBG("Read %d bytes\n", urb->actual_length);

	data = urb->transfer_buffer;
	size = urb->actual_length;

	if (qcusbnet_debug)
		print_hex_dump(KERN_INFO, "gobi-read: ", DUMP_PREFIX_OFFSET,
		               16, 1, data, size, true);

	result = qmux_parse(&cid, data, size);
	if (result < 0) {
		DBG("Read error parsing QMUX %d\n", result);
		resubmit_int_urb(dev->qmi.inturb);
		return;
	}

	if (size < result + 3) {
		DBG("Data buffer too small to parse\n");
		resubmit_int_urb(dev->qmi.inturb);
		return;
	}

	if (cid == QMICTL)
		tid = *(u8 *)(data + result + 1);
	else
		tid = *(u16 *)(data + result + 1);
	spin_lock_irqsave(&dev->qmi.clients_lock, flags);

	list_for_each(node, &dev->qmi.clients) {
		client = list_entry(node, struct client, node);
		if (client->cid == cid || (client->cid | 0xff00) == cid) {
			copy = kmalloc(size, GFP_ATOMIC);
			memcpy(copy, data, size);
			if (!client_addread(dev, client->cid, tid, copy, size)) {
				DBG("Error allocating pReadMemListEntry "
					  "read will be discarded\n");
				kfree(copy);
				spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
				resubmit_int_urb(dev->qmi.inturb);
				return;
			}

			DBG("Creating new readListEntry for client 0x%04X, TID %x\n",
			    cid, tid);

			client_notify(dev, client->cid, tid);

			if (cid >> 8 != 0xff)
				break;
		}
	}

	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
	resubmit_int_urb(dev->qmi.inturb);
}

static void int_callback(struct urb *urb)
{
	int status;
	struct qcusbnet *dev = (struct qcusbnet *)urb->context;

	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return;
	}

	if (urb->status) {
		DBG("Int status = %d\n", urb->status);
		if (urb->status != -EOVERFLOW)
			return;
	} else {
		if ((urb->actual_length == 8) &&
		    (*(u64 *)urb->transfer_buffer == CDC_GET_ENCAPSULATED_RESPONSE)) {
			usb_fill_control_urb(dev->qmi.readurb, dev->usbnet->udev,
					     usb_rcvctrlpipe(dev->usbnet->udev, 0),
					     (unsigned char *)dev->qmi.readsetup,
					     dev->qmi.readbuf,
					     DEFAULT_READ_URB_LENGTH,
					     read_callback, dev);
			status = usb_submit_urb(dev->qmi.readurb, GFP_ATOMIC);
			if (status) {
				DBG("Error submitting Read URB %d\n", status);
				return;
			}
		} else if ((urb->actual_length == 16) &&
			   (*(u64 *)urb->transfer_buffer == CDC_CONNECTION_SPEED_CHANGE)) {
			/* if upstream or downstream is 0, stop traffic.
			 * Otherwise resume it */
			if ((*(u32 *)(urb->transfer_buffer + 8) == 0) ||
			    (*(u32 *)(urb->transfer_buffer + 12) == 0)) {
				qc_setdown(dev, DOWN_CDC_CONNECTION_SPEED);
				DBG("traffic stopping due to CONNECTION_SPEED_CHANGE\n");
			} else {
				qc_cleardown(dev, DOWN_CDC_CONNECTION_SPEED);
				DBG("resuming traffic due to CONNECTION_SPEED_CHANGE\n");
			}
		} else {
			DBG("ignoring invalid interrupt in packet\n");
			if (qcusbnet_debug)
				print_hex_dump(KERN_INFO, "gobi-int: ",
				               DUMP_PREFIX_OFFSET, 16, 1,
				               urb->transfer_buffer,
				               urb->actual_length, true);
		}
	}

	resubmit_int_urb(dev->qmi.inturb);
	return;
}

int qc_startread(struct qcusbnet *dev)
{
	int interval;

	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	dev->qmi.readurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->qmi.readurb) {
		DBG("Error allocating read urb\n");
		return -ENOMEM;
	}

	dev->qmi.inturb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->qmi.inturb) {
		usb_free_urb(dev->qmi.readurb);
		DBG("Error allocating int urb\n");
		return -ENOMEM;
	}

	dev->qmi.readbuf = kmalloc(DEFAULT_READ_URB_LENGTH, GFP_KERNEL);
	if (!dev->qmi.readbuf) {
		usb_free_urb(dev->qmi.readurb);
		usb_free_urb(dev->qmi.inturb);
		DBG("Error allocating read buffer\n");
		return -ENOMEM;
	}

	dev->qmi.intbuf = kmalloc(DEFAULT_READ_URB_LENGTH, GFP_KERNEL);
	if (!dev->qmi.intbuf) {
		usb_free_urb(dev->qmi.readurb);
		usb_free_urb(dev->qmi.inturb);
		kfree(dev->qmi.readbuf);
		DBG("Error allocating int buffer\n");
		return -ENOMEM;
	}

	dev->qmi.readsetup = kmalloc(sizeof(*dev->qmi.readsetup), GFP_KERNEL);
	if (!dev->qmi.readsetup) {
		usb_free_urb(dev->qmi.readurb);
		usb_free_urb(dev->qmi.inturb);
		kfree(dev->qmi.readbuf);
		kfree(dev->qmi.intbuf);
		DBG("Error allocating setup packet buffer\n");
		return -ENOMEM;
	}

	dev->qmi.readsetup->type = 0xA1;
	dev->qmi.readsetup->code = 1;
	dev->qmi.readsetup->value = 0;
	dev->qmi.readsetup->index = 0;
	dev->qmi.readsetup->len = DEFAULT_READ_URB_LENGTH;

	interval = (dev->usbnet->udev->speed == USB_SPEED_HIGH) ? 7 : 3;

	usb_fill_int_urb(dev->qmi.inturb, dev->usbnet->udev,
			 usb_rcvintpipe(dev->usbnet->udev, 0x81),
			 dev->qmi.intbuf, DEFAULT_READ_URB_LENGTH,
			 int_callback, dev, interval);
	return usb_submit_urb(dev->qmi.inturb, GFP_KERNEL);
}

void qc_stopread(struct qcusbnet *dev)
{
	if (dev->qmi.readurb) {
		DBG("Killing read URB\n");
		usb_kill_urb(dev->qmi.readurb);
	}

	if (dev->qmi.inturb) {
		DBG("Killing int URB\n");
		usb_kill_urb(dev->qmi.inturb);
	}

	kfree(dev->qmi.readsetup);
	dev->qmi.readsetup = NULL;
	kfree(dev->qmi.readbuf);
	dev->qmi.readbuf = NULL;
	kfree(dev->qmi.intbuf);
	dev->qmi.intbuf = NULL;

	usb_free_urb(dev->qmi.readurb);
	dev->qmi.readurb = NULL;
	usb_free_urb(dev->qmi.inturb);
	dev->qmi.inturb = NULL;
}

static int read_async(struct qcusbnet *dev, u16 cid, u16 tid,
		      void (*hook)(struct qcusbnet *, u16, void *),
		      void *data)
{
	struct list_head *node;
	struct client *client;
	struct readreq *readreq;

	unsigned long flags;

	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	spin_lock_irqsave(&dev->qmi.clients_lock, flags);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find matching client ID 0x%04X\n", cid);
		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		return -ENXIO;
	}

	list_for_each(node, &client->reads) {
		readreq = list_entry(node, struct readreq, node);
		if (!tid || tid == readreq->tid) {
			spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
			hook(dev, cid, data);
			return 0;
		}
	}

	if (!client_addnotify(dev, cid, tid, hook, data))
		DBG("Unable to register for notification\n");

	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
	return 0;
}

static void upsem(struct qcusbnet *dev, u16 cid, void *data)
{
	DBG("0x%04X\n", cid);
	up((struct semaphore *)data);
}

static int read_sync(struct qcusbnet *dev, void **buf, u16 cid, u16 tid)
{
	struct list_head *node;
	int result;
	struct client *client;
	struct notifyreq *notify;
	struct semaphore sem;
	void *data;
	unsigned long flags;
	u16 size;

	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	spin_lock_irqsave(&dev->qmi.clients_lock, flags);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find matching client ID 0x%04X\n", cid);
		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		return -ENXIO;
	}

	while (!client_delread(dev, cid, tid, &data, &size)) {
		sema_init(&sem, 0);
		if (!client_addnotify(dev, cid, tid, upsem, &sem)) {
			DBG("unable to register for notification\n");
			spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
			return -EFAULT;
		}

		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);

		result = down_interruptible(&sem);
		if (result) {
			DBG("Interrupted %d\n", result);
			spin_lock_irqsave(&dev->qmi.clients_lock, flags);
			list_for_each(node, &client->notifies) {
				notify = list_entry(node, struct notifyreq, node);
				if (notify->data == &sem) {
					list_del(&notify->node);
					kfree(notify);
					break;
				}
			}

			spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
			return -EINTR;
		}

		if (!device_valid(dev)) {
			DBG("Invalid device!\n");
			return -ENXIO;
		}

		spin_lock_irqsave(&dev->qmi.clients_lock, flags);
	}

	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
	*buf = data;
	return size;
}

struct writectx {
	struct semaphore sem;
	struct kref ref;
};

static void writectx_release(struct kref *ref)
{
	struct writectx *ctx = container_of(ref, struct writectx, ref);
	kfree(ctx);
}

static void write_callback(struct urb *urb)
{
	struct writectx *ctx = urb->context;

	DBG("Write status/size %d/%d\n", urb->status, urb->actual_length);
	up(&ctx->sem);
	kref_put(&ctx->ref, writectx_release);
}

static int write_sync(struct qcusbnet *dev, char *buf, int size, u16 cid)
{
	int result;
	struct writectx *ctx;
	struct urb *urb;
	struct urbsetup setup;
	unsigned long flags;

	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		DBG("no memory for write context");
		return -ENOMEM;
	}

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		DBG("URB mem error\n");
		kfree(ctx);
		return -ENOMEM;
	}

	result = qmux_fill(cid, buf, size);
	if (result < 0) {
		usb_free_urb(urb);
		kfree(ctx);
		return result;
	}

	/* CDC Send Encapsulated Request packet */
	setup.type = 0x21;
	setup.code = 0;
	setup.value = 0;
	setup.index = 0;
	setup.len = 0;
	setup.len = size;

	usb_fill_control_urb(urb, dev->usbnet->udev,
			     usb_sndctrlpipe(dev->usbnet->udev, 0),
			     (unsigned char *)&setup, (void *)buf, size,
			     NULL, dev);

	DBG("Actual Write:\n");
	if (qcusbnet_debug)
		print_hex_dump(KERN_INFO,  "gobi-write: ", DUMP_PREFIX_OFFSET,
		               16, 1, buf, size, true);

	sema_init(&ctx->sem, 0);
	kref_init(&ctx->ref);
	kref_get(&ctx->ref);	/* for the urb */

	urb->complete = write_callback;
	urb->context = ctx;

	result = usb_autopm_get_interface(dev->iface);
	if (result < 0) {
		DBG("unable to resume interface: %d\n", result);
		if (result == -EPERM) {
			qc_suspend(dev->iface, PMSG_SUSPEND);
		}
		return result;
	}

	spin_lock_irqsave(&dev->qmi.clients_lock, flags);

	if (!client_addurb(dev, cid, urb)) {
		usb_free_urb(urb);
		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		usb_autopm_put_interface(dev->iface);
		return -EINVAL;
	}

	result = usb_submit_urb(urb, GFP_KERNEL);
	if (result < 0)	{
		DBG("submit URB error %d\n", result);
		if (client_delurb(dev, cid) != urb) {
			DBG("Didn't get write URB back\n");
		}

		usb_free_urb(urb);

		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		usb_autopm_put_interface(dev->iface);
		return result;
	}

	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
	result = down_interruptible(&ctx->sem);
	kref_put(&ctx->ref, writectx_release);
	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	usb_autopm_put_interface(dev->iface);
	spin_lock_irqsave(&dev->qmi.clients_lock, flags);
	if (client_delurb(dev, cid) != urb) {
		DBG("Didn't get write URB back\n");
		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);

	if (!result) {
		if (!urb->status) {
			result = size;
		} else {
			DBG("bad status = %d\n", urb->status);
			result = urb->status;
		}
	} else {
		DBG("Interrupted %d !!!\n", result);
		DBG("Device may be in bad state and need reset !!!\n");
	}

	usb_free_urb(urb);
	return result;
}

static int client_alloc(struct qcusbnet *dev, u8 type)
{
	u16 cid;
	struct client *client;
	int result;
	void *wbuf;
	size_t wbufsize;
	void *rbuf;
	u16 rbufsize;
	unsigned long flags;
	u8 tid;

	if (!device_valid(dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	if (type) {
		tid = atomic_add_return(1, &dev->qmi.qmitid);
		if (!tid)
			atomic_add_return(1, &dev->qmi.qmitid);
		wbuf = qmictl_new_getcid(tid, type, &wbufsize);
		if (!wbuf)
			return -ENOMEM;
		result = write_sync(dev, wbuf, wbufsize, QMICTL);
		kfree(wbuf);

		if (result < 0)
			return result;

		result = read_sync(dev, &rbuf, QMICTL, tid);
		if (result < 0) {
			DBG("bad read data %d\n", result);
			return result;
		}
		rbufsize = result;

		result = qmictl_alloccid_resp(rbuf, rbufsize, &cid);
		kfree(rbuf);

		if (result < 0)
			return result;
	} else {
		cid = 0;
	}

	spin_lock_irqsave(&dev->qmi.clients_lock, flags);
	if (client_bycid(dev, cid)) {
		DBG("Client memory already exists\n");
		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		return -ETOOMANYREFS;
	}

	client = kmalloc(sizeof(*client), GFP_ATOMIC);
	if (!client) {
		DBG("Error allocating read list\n");
		spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		return -ENOMEM;
	}

	list_add_tail(&client->node, &dev->qmi.clients);
	client->cid = cid;
	INIT_LIST_HEAD(&client->reads);
	INIT_LIST_HEAD(&client->notifies);
	INIT_LIST_HEAD(&client->urbs);
	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
	return cid;
}

static void client_free(struct qcusbnet *dev, u16 cid)
{
	struct list_head *node, *tmp;
	int result;
	struct client *client;
	struct urb *urb;
	void *data;
	u16 size;
	void *wbuf;
	size_t wbufsize;
	void *rbuf;
	u16 rbufsize;
	unsigned long flags;
	u8 tid;

	DBG("releasing 0x%04X\n", cid);

	if (cid != QMICTL) {
		tid = atomic_add_return(1, &dev->qmi.qmitid);
		if (!tid)
			tid = atomic_add_return(1, &dev->qmi.qmitid);
		wbuf = qmictl_new_releasecid(tid, cid, &wbufsize);
		if (!wbuf) {
			DBG("memory error\n");
		} else {
			result = write_sync(dev, wbuf, wbufsize, QMICTL);
			kfree(wbuf);

			if (result < 0) {
				DBG("bad write status %d\n", result);
			} else {
				result = read_sync(dev, &rbuf, QMICTL, tid);
				if (result < 0) {
					DBG("bad read status %d\n", result);
				} else {
					rbufsize = result;
					result = qmictl_freecid_resp(rbuf, rbufsize);
					kfree(rbuf);
					if (result < 0)
						DBG("error %d parsing response\n", result);
				}
			}
		}
	}

	spin_lock_irqsave(&dev->qmi.clients_lock, flags);
	list_for_each_safe(node, tmp, &dev->qmi.clients) {
		client = list_entry(node, struct client, node);
		if (client->cid == cid) {
			while (client_notify(dev, cid, 0)) {
				;
			}

			urb = client_delurb(dev, cid);
			while (urb != NULL) {
				usb_kill_urb(urb);
				usb_free_urb(urb);
				urb = client_delurb(dev, cid);
			}

			while (client_delread(dev, cid, 0, &data, &size))
				kfree(data);

			list_del(&client->node);
			kfree(client);
		}
	}
	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
}

struct client *client_bycid(struct qcusbnet *dev, u16 cid)
{
	struct list_head *node;
	struct client *client;

	if (!device_valid(dev))	{
		DBG("Invalid device\n");
		return NULL;
	}

	assert_locked(dev);

	list_for_each(node, &dev->qmi.clients) {
		client = list_entry(node, struct client, node);
		if (client->cid == cid)
			return client;
	}

	DBG("Could not find client mem 0x%04X\n", cid);
	return NULL;
}

static bool client_addread(struct qcusbnet *dev, u16 cid, u16 tid, void *data,
			   u16 size)
{
	struct client *client;
	struct readreq *req;

	assert_locked(dev);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find this client's memory 0x%04X\n", cid);
		return false;
	}

	req = kmalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		DBG("Mem error\n");
		return false;
	}

	req->data = data;
	req->size = size;
	req->tid = tid;

	list_add_tail(&req->node, &client->reads);

	return true;
}

static bool client_delread(struct qcusbnet *dev, u16 cid, u16 tid, void **data,
			   u16 *size)
{
	struct client *client;
	struct readreq *req;
	struct list_head *node;

	assert_locked(dev);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find this client's memory 0x%04X\n", cid);
		return false;
	}

	list_for_each(node, &client->reads) {
		req = list_entry(node, struct readreq, node);
		if (!tid || tid == req->tid) {
			*data = req->data;
			*size = req->size;
			list_del(&req->node);
			kfree(req);
			return true;
		}

		DBG("skipping 0x%04X data TID = %x\n", cid, req->tid);
	}

	DBG("No read memory to pop, Client 0x%04X, TID = %x\n", cid, tid);
	return false;
}

static bool client_addnotify(struct qcusbnet *dev, u16 cid, u16 tid,
			     void (*hook)(struct qcusbnet *, u16, void *),
			     void *data)
{
	struct client *client;
	struct notifyreq *req;

	assert_locked(dev);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find this client's memory 0x%04X\n", cid);
		return false;
	}

	req = kmalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		DBG("Mem error\n");
		return false;
	}

	list_add_tail(&req->node, &client->notifies);
	req->func = hook;
	req->data = data;
	req->tid = tid;

	return true;
}

static bool client_notify(struct qcusbnet *dev, u16 cid, u16 tid)
{
	struct client *client;
	struct notifyreq *delnotify, *notify;
	struct list_head *node;

	assert_locked(dev);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find this client's memory 0x%04X\n", cid);
		return false;
	}

	delnotify = NULL;

	list_for_each(node, &client->notifies) {
		notify = list_entry(node, struct notifyreq, node);
		if (!tid || !notify->tid || tid == notify->tid) {
			delnotify = notify;
			break;
		}

		DBG("skipping data TID = %x\n", notify->tid);
	}

	if (delnotify) {
		list_del(&delnotify->node);
		if (delnotify->func) {
			spin_unlock(&dev->qmi.clients_lock);
			delnotify->func(dev, cid, delnotify->data);
			spin_lock(&dev->qmi.clients_lock);
		}
		kfree(delnotify);
		return true;
	}

	DBG("no one to notify for TID %x\n", tid);
	return false;
}

static bool client_addurb(struct qcusbnet *dev, u16 cid, struct urb *urb)
{
	struct client *client;
	struct urbreq *req;

	assert_locked(dev);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find this client's memory 0x%04X\n", cid);
		return false;
	}

	req = kmalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		DBG("Mem error\n");
		return false;
	}

	req->urb = urb;
	list_add_tail(&req->node, &client->urbs);

	return true;
}

static struct urb *client_delurb(struct qcusbnet *dev, u16 cid)
{
	struct client *client;
	struct urbreq *req;
	struct urb *urb;

	assert_locked(dev);

	client = client_bycid(dev, cid);
	if (!client) {
		DBG("Could not find this client's memory 0x%04X\n", cid);
		return NULL;
	}

	if (list_empty(&client->urbs)) {
		DBG("No URB's to pop\n");
		return NULL;
	}

	req = list_first_entry(&client->urbs, struct urbreq, node);
	list_del(&req->node);
	urb = req->urb;
	kfree(req);
	return urb;
}

static int devqmi_open(struct inode *inode, struct file *file)
{
	struct qmihandle *handle;
	struct qmidev *qmidev = container_of(inode->i_cdev, struct qmidev, cdev);
	struct qcusbnet *dev = container_of(qmidev, struct qcusbnet, qmi);

	/* We need an extra ref on the device per fd, since we stash a ref
	 * inside the handle. If qcusbnet_get() returns NULL, that means the
	 * device has been removed from the list - no new refs for us. */
	struct qcusbnet *ref = qcusbnet_get(dev);

	if (!ref)
		return -ENXIO;

	file->private_data = kmalloc(sizeof(struct qmihandle), GFP_KERNEL);
	if (!file->private_data) {
		DBG("Mem error\n");
		return -ENOMEM;
	}

	handle = (struct qmihandle *)file->private_data;
	handle->cid = (u16)-1;
	handle->dev = ref;

	DBG("%p %04x", handle, handle->cid);

	return 0;
}

static long devqmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int result;
	u32 vidpid;

	struct qmihandle *handle = (struct qmihandle *)file->private_data;

	DBG("%p %04x %08x", handle, handle->cid, cmd);

	if (handle->dev->dying) {
		DBG("Dying device");
		return -ENXIO;
	}

	if (!device_valid(handle->dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	switch (cmd) {
	case IOCTL_QMI_GET_SERVICE_FILE:

		DBG("Setting up QMI for service %lu\n", arg);
		if (!(u8)arg) {
			DBG("Cannot use QMICTL from userspace\n");
			return -EINVAL;
		}

		if (handle->cid != (u16)-1) {
			DBG("Close the current connection before opening a new one\n");
			return -EBADR;
		}

		result = client_alloc(handle->dev, (u8)arg);
		if (result < 0)
			return result;
		handle->cid = result;

		return 0;
		break;

	/* Okay, all aboard the nasty hack express. If we don't have this
	 * ioctl() (and we just rely on userspace to close() the file
	 * descriptors), if userspace has any refs left to this fd (like, say, a
	 * pending read()), then the read might hang around forever. Userspace
	 * needs a way to cause us to kick people off those waitqueues before
	 * closing the fd for good.
	 *
	 * If this driver used workqueues, the correct approach here would
	 * instead be to make the file descriptor select()able, and then just
	 * use select() instead of aio in userspace (thus allowing us to get
	 * away with one thread total and avoiding the recounting mess
	 * altogether).
	 */
	case IOCTL_QMI_CLOSE:
		DBG("Tearing down QMI for service %lu", arg);
		if (handle->cid == (u16)-1) {
			DBG("no qmi cid");
			return -EBADR;
		}

		file->private_data = NULL;
		client_free(handle->dev, handle->cid);
		kfree(handle);
		return 0;
		break;

	case IOCTL_QMI_GET_DEVICE_VIDPID:
		if (!arg) {
			DBG("Bad VIDPID buffer\n");
			return -EINVAL;
		}

		if (!handle->dev->usbnet) {
			DBG("Bad usbnet\n");
			return -ENOMEM;
		}

		if (!handle->dev->usbnet->udev) {
			DBG("Bad udev\n");
			return -ENOMEM;
		}

		vidpid = ((le16_to_cpu(handle->dev->usbnet->udev->descriptor.idVendor) << 16)
			  + le16_to_cpu(handle->dev->usbnet->udev->descriptor.idProduct));

		result = copy_to_user((unsigned int *)arg, &vidpid, 4);
		if (result)
			DBG("Copy to userspace failure\n");

		return result;
		break;

	case IOCTL_QMI_GET_DEVICE_MEID:
		if (!arg) {
			DBG("Bad MEID buffer\n");
			return -EINVAL;
		}

		result = copy_to_user((unsigned int *)arg, &handle->dev->meid[0], 14);
		if (result)
			DBG("copy to userspace failure\n");

		return result;
		break;
	default:
		return -EBADRQC;
	}
}

#ifdef CONFIG_COMPAT
static int devqmi_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return devqmi_ioctl(file, cmd, compat_ptr(arg));
}
#endif

static int devqmi_release(struct inode *inode, struct file *file)
{
	struct qmihandle *handle = (struct qmihandle *)file->private_data;
	if (!handle)
		return 0;
	file->private_data = NULL;
	if (handle->cid != (u16)-1)
		client_free(handle->dev, handle->cid);
	qcusbnet_put(handle->dev);
	kfree(handle);
	return 0;
}

static ssize_t devqmi_read(struct file *file, char __user *buf, size_t size,
			   loff_t *pos)
{
	int result;
	void *data = NULL;
	void *smalldata;
	struct qmihandle *handle = (struct qmihandle *)file->private_data;

	if (!handle) {
		DBG("Bad file data\n");
		return -EBADF;
	}

	if (handle->dev->dying) {
		DBG("Dying device");
		return -ENXIO;
	}

	if (!device_valid(handle->dev)) {
		DBG("Invalid device!\n");
		return -ENXIO;
	}

	if (handle->cid == (u16)-1) {
		DBG("Client ID must be set before reading 0x%04X\n",
		    handle->cid);
		return -EBADR;
	}

	result = read_sync(handle->dev, &data, handle->cid, 0);
	if (result <= 0)
		return result;

	result -= qmux_size;
	smalldata = data + qmux_size;

	if (result > size) {
		DBG("Read data is too large for amount user has requested\n");
		kfree(data);
		return -EOVERFLOW;
	}

	if (copy_to_user(buf, smalldata, result)) {
		DBG("Error copying read data to user\n");
		result = -EFAULT;
	}

	kfree(data);
	return result;
}

static ssize_t devqmi_write(struct file *file, const char __user * buf,
			    size_t size, loff_t *pos)
{
	int status;
	void *wbuf;
	struct qmihandle *handle = (struct qmihandle *)file->private_data;

	if (!handle) {
		DBG("Bad file data\n");
		return -EBADF;
	}

	if (!device_valid(handle->dev)) {
		DBG("Invalid device! Updating f_ops\n");
		file->f_op = file->f_dentry->d_inode->i_fop;
		return -ENXIO;
	}

	if (handle->cid == (u16)-1) {
		DBG("Client ID must be set before writing 0x%04X\n",
			  handle->cid);
		return -EBADR;
	}

	wbuf = kmalloc(size + qmux_size, GFP_KERNEL);
	if (!wbuf)
		return -ENOMEM;
	status = copy_from_user(wbuf + qmux_size, buf, size);
	if (status) {
		DBG("Unable to copy data from userspace %d\n", status);
		kfree(wbuf);
		return status;
	}

	status = write_sync(handle->dev, wbuf, size + qmux_size,
			    handle->cid);

	kfree(wbuf);
	if (status == size + qmux_size)
		return size;
	return status;
}

int qc_register(struct qcusbnet *dev)
{
	int result;
	int qmiidx = 0;
	dev_t devno;
	char *name;

	dev->valid = true;
	dev->dying = false;
	result = client_alloc(dev, QMICTL);
	if (result) {
		dev->valid = false;
		return result;
	}
	atomic_set(&dev->qmi.qmitid, 1);

	result = qc_startread(dev);
	if (result) {
		dev->valid = false;
		return result;
	}

	if (!qmi_ready(dev, 30000)) {
		DBG("Device unresponsive to QMI\n");
		return -ETIMEDOUT;
	}

	result = setup_wds_callback(dev);
	if (result) {
		dev->valid = false;
		return result;
	}

	result = qmidms_getmeid(dev);
	if (result) {
		dev->valid = false;
		return result;
	}

	result = alloc_chrdev_region(&devno, 0, 1, "qcqmi");
	if (result < 0)
		return result;

	cdev_init(&dev->qmi.cdev, &devqmi_fops);
	dev->qmi.cdev.owner = THIS_MODULE;
	dev->qmi.cdev.ops = &devqmi_fops;

	result = cdev_add(&dev->qmi.cdev, devno, 1);
	if (result) {
		DBG("error adding cdev\n");
		return result;
	}

	name = strstr(dev->usbnet->net->name, "usb");
	if (!name) {
		DBG("Bad net name: %s\n", dev->usbnet->net->name);
		return -ENXIO;
	}
	name += strlen("usb");
	qmiidx = simple_strtoul(name, NULL, 10);
	if (qmiidx < 0) {
		DBG("Bad minor number\n");
		return -ENXIO;
	}

	printk(KERN_INFO "creating qcqmi%d\n", qmiidx);
	device_create(dev->qmi.devclass, &dev->iface->dev, devno, NULL, "qcqmi%d", qmiidx);

	dev->qmi.devnum = devno;
	return 0;
}

void qc_deregister(struct qcusbnet *dev)
{
	struct list_head *node, *tmp;
	struct client *client;

	dev->dying = true;
	list_for_each_safe(node, tmp, &dev->qmi.clients) {
		client = list_entry(node, struct client, node);
		DBG("release 0x%04X\n", client->cid);
		client_free(dev, client->cid);
	}

	qc_stopread(dev);
	dev->valid = false;
	if (!IS_ERR(dev->qmi.devclass))
		device_destroy(dev->qmi.devclass, dev->qmi.devnum);
	cdev_del(&dev->qmi.cdev);
	unregister_chrdev_region(dev->qmi.devnum, 1);
}

static bool qmi_ready(struct qcusbnet *dev, u16 timeout)
{
	int result;
	void *wbuf;
	size_t wbufsize;
	void *rbuf;
	u16 rbufsize;
	struct semaphore sem;
	u16 now;
	unsigned long flags;
	u8 tid;

	if (!device_valid(dev)) {
		DBG("Invalid device\n");
		return -EFAULT;
	}


	for (now = 0; now < timeout; now += 100) {
		sema_init(&sem, 0);

		tid = atomic_add_return(1, &dev->qmi.qmitid);
		if (!tid)
			tid = atomic_add_return(1, &dev->qmi.qmitid);
		kfree(wbuf);
		wbuf = qmictl_new_ready(tid, &wbufsize);
		if (!wbuf)
			return -ENOMEM;

		result = read_async(dev, QMICTL, tid, upsem, &sem);
		if (result) {
			kfree(wbuf);
			return false;
		}

		write_sync(dev, wbuf, wbufsize, QMICTL);

		msleep(100);
		if (!down_trylock(&sem)) {
			spin_lock_irqsave(&dev->qmi.clients_lock, flags);
			if (client_delread(dev,	QMICTL,	tid, &rbuf, &rbufsize)) {
				spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
				kfree(rbuf);
				break;
			} else {
				spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
			}
		} else {
			spin_lock_irqsave(&dev->qmi.clients_lock, flags);
			client_notify(dev, QMICTL, tid);
			spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);
		}
	}

	kfree(wbuf);

	if (now >= timeout)
		return false;

	DBG("QMI Ready after %u milliseconds\n", now);

	/* 3580 and newer doesn't need a delay; older needs 5000ms */
	if (qcusbnet2k_fwdelay)
		msleep(qcusbnet2k_fwdelay * 1000);

	return true;
}

static void wds_callback(struct qcusbnet *dev, u16 cid, void *data)
{
	bool ret;
	int result;
	void *rbuf;
	u16 rbufsize;

	struct net_device_stats *stats = &(dev->usbnet->net->stats);

	struct qmiwds_stats dstats = {
		.txok = (u32)-1,
		.rxok = (u32)-1,
		.txerr = (u32)-1,
		.rxerr = (u32)-1,
		.txofl = (u32)-1,
		.rxofl = (u32)-1,
		.txbytesok = (u64)-1,
		.rxbytesok = (u64)-1,
	};
	unsigned long flags;

	if (!device_valid(dev)) {
		DBG("Invalid device\n");
		return;
	}

	spin_lock_irqsave(&dev->qmi.clients_lock, flags);
	ret = client_delread(dev, cid, 0, &rbuf, &rbufsize);
	spin_unlock_irqrestore(&dev->qmi.clients_lock, flags);

	if (!ret) {
		DBG("WDS callback failed to get data\n");
		return;
	}

	dstats.linkstate = !qc_isdown(dev, DOWN_NO_NDIS_CONNECTION);
	dstats.reconfigure = false;

	result = qmiwds_event_resp(rbuf, rbufsize, &dstats);
	if (result < 0) {
		DBG("bad WDS packet\n");
	} else {
		if (dstats.txofl != (u32)-1)
			stats->tx_fifo_errors = dstats.txofl;

		if (dstats.rxofl != (u32)-1)
			stats->rx_fifo_errors = dstats.rxofl;

		if (dstats.txerr != (u32)-1)
			stats->tx_errors = dstats.txerr;

		if (dstats.rxerr != (u32)-1)
			stats->rx_errors = dstats.rxerr;

		if (dstats.txok != (u32)-1)
			stats->tx_packets = dstats.txok + stats->tx_errors;

		if (dstats.rxok != (u32)-1)
			stats->rx_packets = dstats.rxok + stats->rx_errors;

		if (dstats.txbytesok != (u64)-1)
			stats->tx_bytes = dstats.txbytesok;

		if (dstats.rxbytesok != (u64)-1)
			stats->rx_bytes = dstats.rxbytesok;

		if (dstats.reconfigure) {
			DBG("Net device link reset\n");
			qc_setdown(dev, DOWN_NO_NDIS_CONNECTION);
			qc_cleardown(dev, DOWN_NO_NDIS_CONNECTION);
		} else {
			if (dstats.linkstate) {
				DBG("Net device link is connected\n");
				qc_cleardown(dev, DOWN_NO_NDIS_CONNECTION);
			} else {
				DBG("Net device link is disconnected\n");
				qc_setdown(dev, DOWN_NO_NDIS_CONNECTION);
			}
		}
	}

	kfree(rbuf);

	result = read_async(dev, cid, 0, wds_callback, data);
	if (result != 0)
		DBG("unable to setup next async read\n");
}

static int setup_wds_callback(struct qcusbnet *dev)
{
	int result;
	void *buf;
	size_t size;
	u16 cid;

	if (!device_valid(dev)) {
		DBG("Invalid device\n");
		return -EFAULT;
	}

	result = client_alloc(dev, QMIWDS);
	if (result < 0)
		return result;
	cid = result;

	buf = qmiwds_new_seteventreport(1, &size);
	if (!buf)
		return -ENOMEM;

	result = write_sync(dev, buf, size, cid);
	kfree(buf);

	if (result < 0) {
		return result;
	}

	buf = qmiwds_new_getpkgsrvcstatus(2, &size);
	if (buf == NULL)
		return -ENOMEM;

	result = write_sync(dev, buf, size, cid);
	kfree(buf);

	if (result < 0)
		return result;

	result = read_async(dev, cid, 0, wds_callback, NULL);
	if (result) {
		DBG("unable to setup async read\n");
		return result;
	}

	result = usb_control_msg(dev->usbnet->udev,
				 usb_sndctrlpipe(dev->usbnet->udev, 0),
				 0x22, 0x21, 1, 0, NULL, 0, 100);
	if (result < 0) {
		DBG("Bad SetControlLineState status %d\n", result);
		return result;
	}

	return 0;
}

static int qmidms_getmeid(struct qcusbnet *dev)
{
	int result;
	void *wbuf;
	size_t wbufsize;
	void *rbuf;
	u16 rbufsize;
	u16 cid;

	if (!device_valid(dev))	{
		DBG("Invalid device\n");
		return -EFAULT;
	}

	result = client_alloc(dev, QMIDMS);
	if (result < 0)
		return result;
	cid = result;

	wbuf = qmidms_new_getmeid(1, &wbufsize);
	if (!wbuf)
		return -ENOMEM;

	result = write_sync(dev, wbuf, wbufsize, cid);
	kfree(wbuf);

	if (result < 0)
		return result;

	result = read_sync(dev, &rbuf, cid, 1);
	if (result < 0)
		return result;
	rbufsize = result;

	result = qmidms_meid_resp(rbuf, rbufsize, &dev->meid[0], 14);
	kfree(rbuf);

	if (result < 0) {
		DBG("bad get MEID resp\n");
		memset(&dev->meid[0], '0', 14);
	}

	client_free(dev, cid);
	return 0;
}

module_param(qcusbnet2k_fwdelay, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(qcusbnet2k_fwdelay, "Delay for old firmware");
