/* qcusbnet.c - gobi network device
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.

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

#include "structs.h"
#include "qmidevice.h"
#include "qmi.h"
#include "qcusbnet.h"

#include <linux/module.h>
#include <linux/ctype.h>

#define DRIVER_VERSION "1.0.110+google+w0"
#define DRIVER_AUTHOR "Qualcomm Innovation Center"
#define DRIVER_DESC "gobi"

static LIST_HEAD(qcusbnet_list);
static DEFINE_MUTEX(qcusbnet_lock);

int gobi_debug;
static struct class *devclass;

static void free_dev(struct kref *ref)
{
	struct qcusbnet *dev = container_of(ref, struct qcusbnet, refcount);
	list_del(&dev->node);
	kfree(dev);
}

static void free_urb_with_skb(struct urb *urb)
{
	struct sk_buff *skb = (struct sk_buff *)urb->context;
	dev_kfree_skb_any(skb);
	usb_free_urb(urb);
}

void qcusbnet_put(struct qcusbnet *dev)
{
	mutex_lock(&qcusbnet_lock);
	kref_put(&dev->refcount, free_dev);
	mutex_unlock(&qcusbnet_lock);
}

struct qcusbnet *qcusbnet_get(struct qcusbnet *key)
{
	/* Given a putative qcusbnet struct, return either the struct itself
	 * (with a ref taken) if the struct is still visible, or NULL if it's
	 * not. This prevents object-visibility races where someone is looking
	 * up an object as the last ref gets dropped; dropping the last ref and
	 * removing the object from the list are atomic with respect to getting
	 * a new ref. */
	struct qcusbnet *entry;
	mutex_lock(&qcusbnet_lock);
	list_for_each_entry(entry, &qcusbnet_list, node) {
		if (entry == key) {
			kref_get(&entry->refcount);
			mutex_unlock(&qcusbnet_lock);
			return entry;
		}
	}
	mutex_unlock(&qcusbnet_lock);
	return NULL;
}

int qc_suspend(struct usb_interface *iface, pm_message_t event)
{
	struct usbnet *usbnet;
	struct qcusbnet *dev;

	BUG_ON(!iface);

	usbnet = usb_get_intfdata(iface);
	BUG_ON(!usbnet || !usbnet->net);

	dev = (struct qcusbnet *)usbnet->data[0];
	BUG_ON(!dev);

	if (!(event.event & PM_EVENT_AUTO)) {
		GOBI_DEBUG("device suspended to power level %d",
		    event.event);
		qc_setdown(dev, DOWN_DRIVER_SUSPENDED);
	} else {
		GOBI_DEBUG("device autosuspend");
	}

	if (event.event & PM_EVENT_SUSPEND) {
		qc_stopread(dev);
		usbnet->udev->reset_resume = 0;
		iface->dev.power.power_state.event = event.event;
	} else {
		usbnet->udev->reset_resume = 1;
	}

	return usbnet_suspend(iface, event);
}

static int qc_resume(struct usb_interface *iface)
{
	struct usbnet *usbnet;
	struct qcusbnet *dev;
	int ret;
	int oldstate;

	BUG_ON(!iface);

	usbnet = usb_get_intfdata(iface);
	BUG_ON(!usbnet || !usbnet->net);

	dev = (struct qcusbnet *)usbnet->data[0];
	BUG_ON(!dev);

	oldstate = iface->dev.power.power_state.event;
	iface->dev.power.power_state.event = PM_EVENT_ON;
	GOBI_DEBUG("resuming from power mode %d", oldstate);

	if (oldstate & PM_EVENT_SUSPEND) {
		qc_cleardown(dev, DOWN_DRIVER_SUSPENDED);

		ret = usbnet_resume(iface);
		if (ret) {
			GOBI_ERROR("usbnet_resume failed: %d", ret);
			return ret;
		}

		ret = qc_startread(dev);
		if (ret) {
			GOBI_ERROR("qc_startread failed: %d", ret);
			return ret;
		}
	} else {
		GOBI_DEBUG("nothing to resume");
		return 0;
	}

	return ret;
}

static int qcnet_bind(struct usbnet *usbnet, struct usb_interface *iface)
{
	int numends;
	int i;
	struct usb_host_endpoint *endpoint = NULL;
	struct usb_host_endpoint *in = NULL;
	struct usb_host_endpoint *out = NULL;
	int dir_in, dir_out, xfer_int;

	if (iface->num_altsetting != 1) {
		GOBI_WARN("invalid num_altsetting %u", iface->num_altsetting);
		return -ENODEV;
	}

	if (iface->cur_altsetting->desc.bInterfaceNumber != 0
	    && iface->cur_altsetting->desc.bInterfaceNumber != 5) {
		GOBI_WARN("invalid interface %d",
			  iface->cur_altsetting->desc.bInterfaceNumber);
		return -ENODEV;
	}

	GOBI_DEBUG("interface number: %d",
		iface->cur_altsetting->desc.bInterfaceNumber);

	numends = iface->cur_altsetting->desc.bNumEndpoints;
	for (i = 0; i < numends; i++) {
		endpoint = iface->cur_altsetting->endpoint + i;

		dir_in   = usb_endpoint_dir_in(&endpoint->desc);
		dir_out  = usb_endpoint_dir_out(&endpoint->desc);
		xfer_int = usb_endpoint_xfer_int(&endpoint->desc);

		GOBI_DEBUG("endpoint %d: addr %u "
			"dir_in %d dir_out %d xfer_int %d",
			i, endpoint->desc.bEndpointAddress,
			dir_in, dir_out, xfer_int);

		if (dir_in && !xfer_int) {
			if (in) {
				GOBI_WARN("multiple in endpoints");
				return -ENODEV;
			}
			GOBI_DEBUG("setting endpoint %d as in", i);
			in = endpoint;
		} else if (dir_out && !xfer_int) {
			if (out) {
				GOBI_WARN("multiple out endpoints");
				return -ENODEV;
			}
			GOBI_DEBUG("setting endpoint %d as out", i);
			out = endpoint;
		} else {
			GOBI_DEBUG("ignoring endpoint %d", i);
		}
	}

	if (!in || !out) {
		GOBI_WARN("missing endpoint(s)");
		if (in)
			GOBI_WARN("found in endpoint: %u",
				in->desc.bEndpointAddress);
		else
			GOBI_WARN("didn't find in endpoint");
		if (out)
			GOBI_WARN("found out endpoint: %u",
				out->desc.bEndpointAddress);
		else
			GOBI_WARN("didn't find out endpoint");
		return -ENODEV;
	}

	if (usb_set_interface(usbnet->udev,
			      iface->cur_altsetting->desc.bInterfaceNumber, 0))	{
		GOBI_WARN("unable to set interface");
		return -EINVAL;
	}

	GOBI_DEBUG("found endpoints: in %u, out %u",
		   in->desc.bEndpointAddress,
		   out->desc.bEndpointAddress);

	usbnet->in = usb_rcvbulkpipe(usbnet->udev,
		in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	usbnet->out = usb_sndbulkpipe(usbnet->udev,
		out->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);

	return 0;
}

static void qcnet_unbind(struct usbnet *usbnet, struct usb_interface *iface)
{
	struct qcusbnet *dev = (struct qcusbnet *)usbnet->data[0];

	dev->dying = true;

	kfree(usbnet->net->netdev_ops);
	usbnet->net->netdev_ops = NULL;
}

static void qcnet_bg_complete(struct work_struct *work)
{
	unsigned long listflags;
	struct qcusbnet *dev = container_of(work, struct qcusbnet, complete);

	BUG_ON(!dev->active);
	free_urb_with_skb(dev->active);
	dev->active = NULL;

	usb_autopm_put_interface(dev->iface);

	spin_lock_irqsave(&dev->urbs_lock, listflags);
	if (!list_empty(&dev->urbs))
		queue_work(dev->workqueue, &dev->startxmit);
	spin_unlock_irqrestore(&dev->urbs_lock, listflags);
}

static void qcnet_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct qcusbnet *dev = *(struct qcusbnet **)skb->cb;

	BUG_ON(urb != dev->active);
	queue_work(dev->workqueue, &dev->complete);
}

static void qcnet_bg_txtimeout(struct work_struct *work)
{
	struct qcusbnet *dev = container_of(work, struct qcusbnet, txtimeout);
	struct list_head *node, *tmp;
	struct urb *urb;
	if (dev->active)
		usb_kill_urb(dev->active);
	list_for_each_safe(node, tmp, &dev->urbs) {
		urb = list_entry(node, struct urb, urb_list);
		list_del(&urb->urb_list);
		free_urb_with_skb(urb);
	}
}

static void qcnet_txtimeout(struct net_device *netdev)
{
	struct usbnet *usbnet = netdev_priv(netdev);
	struct qcusbnet *dev = (struct qcusbnet *)usbnet->data[0];
	queue_work(dev->workqueue, &dev->txtimeout);
}

static void qcnet_bg_startxmit(struct work_struct *work)
{
	unsigned long listflags;
	struct qcusbnet *dev = container_of(work, struct qcusbnet, startxmit);
	struct urb *urb = NULL;
	int status;

	if (dev->dying) {
		GOBI_WARN("dying device");
		/* Fear not; queued urbs will be freed in qcnet_disconnect. */
		return;
	}

	if (dev->active)
		return;

	status = usb_autopm_get_interface(dev->iface);
	if (status < 0) {
		/* Should be ERROR, but it can spin, which floods logs. */
		GOBI_WARN("failed to autoresume interface: %d", status);
		if (status == -EPERM)
			qc_suspend(dev->iface, PMSG_SUSPEND);
		/* We could just drop the packet here, right...? It seems like
		 * if this ever happens, we'll spin, but the old driver did that
		 * as well. */
		queue_work(dev->workqueue, &dev->startxmit);
		return;
	}

	spin_lock_irqsave(&dev->urbs_lock, listflags);
	if (!list_empty(&dev->urbs)) {
		urb = list_first_entry(&dev->urbs, struct urb, urb_list);
		list_del(&urb->urb_list);
	}
	spin_unlock_irqrestore(&dev->urbs_lock, listflags);
	if (urb == NULL) {
		/* If we hit this case, it means that we added our urb to the
		 * list while there was an urb in flight, and that urb
		 * completed, causing our urb to be submitted; in addition, our
		 * urb completed too, all before we got to schedule this work.
		 * Unlikely, but possible. */
		usb_autopm_put_interface(dev->iface);
		return;
	}

	dev->active = urb;
	status = usb_submit_urb(urb, GFP_KERNEL);
	if (status < 0) {
		GOBI_ERROR("failed to submit urb: %d (packet dropped)", status);
		free_urb_with_skb(urb);
		dev->active = NULL;
		usb_autopm_put_interface(dev->iface);
	}
}

static int qcnet_startxmit(struct sk_buff *skb, struct net_device *netdev)
{
	unsigned long listflags;
	struct urb *urb;
	struct usbnet *usbnet = netdev_priv(netdev);
	struct qcusbnet *dev = (struct qcusbnet *)usbnet->data[0];

	if (qc_isdown(dev, DOWN_DRIVER_SUSPENDED)) {
		GOBI_ERROR("device is suspended (packet requeued)");
		return NETDEV_TX_BUSY;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		GOBI_ERROR("failed to allocate urb (packet requeued)");
		return NETDEV_TX_BUSY;
	}

	usb_fill_bulk_urb(urb, dev->usbnet->udev, dev->usbnet->out,
			  skb->data, skb->len, qcnet_complete, skb);
	*(struct qcusbnet **)skb->cb = dev;

	spin_lock_irqsave(&dev->urbs_lock, listflags);
	list_add_tail(&urb->urb_list, &dev->urbs);
	spin_unlock_irqrestore(&dev->urbs_lock, listflags);

	queue_work(dev->workqueue, &dev->startxmit);

	netdev->trans_start = jiffies;

	return NETDEV_TX_OK;
}

static int qcnet_open(struct net_device *netdev)
{
	int status = 0;
	struct qcusbnet *dev;
	struct usbnet *usbnet = netdev_priv(netdev);

	BUG_ON(!usbnet);

	dev = (struct qcusbnet *)usbnet->data[0];
	BUG_ON(!dev);

	qc_cleardown(dev, DOWN_NET_IFACE_STOPPED);
	if (dev->open) {
		status = dev->open(netdev);
		if (status == 0) {
			usb_autopm_put_interface(dev->iface);
		}
	} else {
		GOBI_WARN("no USBNetOpen defined");
	}

	return status;
}

int qcnet_stop(struct net_device *netdev)
{
	struct qcusbnet *dev;
	struct usbnet *usbnet = netdev_priv(netdev);

	BUG_ON(!usbnet);

	dev = (struct qcusbnet *)usbnet->data[0];
	BUG_ON(!dev);

	qc_setdown(dev, DOWN_NET_IFACE_STOPPED);

	if (dev->stop != NULL)
		return dev->stop(netdev);
	return 0;
}

static const struct driver_info qc_netinfo = {
	.description   = "QCUSBNet Ethernet Device",
	.flags         = FLAG_ETHER | FLAG_WWAN,
	.bind          = qcnet_bind,
	.unbind        = qcnet_unbind,
	.data          = 0,
};

#define MKVIDPID(v, p)					\
{							\
	USB_DEVICE(v, p),				\
	.driver_info = (unsigned long)&qc_netinfo,	\
}

static const struct usb_device_id qc_vidpids[] = {
	MKVIDPID(0x05c6, 0x9215),	/* Acer Gobi 2000 */
	MKVIDPID(0x05c6, 0x9265),	/* Asus Gobi 2000 */
	MKVIDPID(0x16d8, 0x8002),	/* CMOTech Gobi 2000 */
	MKVIDPID(0x413c, 0x8186),	/* Dell Gobi 2000 */
	MKVIDPID(0x1410, 0xa010),	/* Entourage Gobi 2000 */
	MKVIDPID(0x1410, 0xa011),	/* Entourage Gobi 2000 */
	MKVIDPID(0x1410, 0xa012),	/* Entourage Gobi 2000 */
	MKVIDPID(0x1410, 0xa013),	/* Entourage Gobi 2000 */
	MKVIDPID(0x03f0, 0x251d),	/* HP Gobi 2000 */
	MKVIDPID(0x05c6, 0x9205),	/* Lenovo Gobi 2000 */
	MKVIDPID(0x05c6, 0x920b),	/* Generic Gobi 2000 */
	MKVIDPID(0x04da, 0x250f),	/* Panasonic Gobi 2000 */
	MKVIDPID(0x05c6, 0x9245),	/* Samsung Gobi 2000 */
	MKVIDPID(0x1199, 0x9001),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9002),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9003),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9004),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9005),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9006),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9007),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9008),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x9009),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x1199, 0x900a),	/* Sierra Wireless Gobi 2000 */
	MKVIDPID(0x05c6, 0x9225),	/* Sony Gobi 2000 */
	MKVIDPID(0x05c6, 0x9235),	/* Top Global Gobi 2000 */
	MKVIDPID(0x05c6, 0x9275),	/* iRex Technologies Gobi 2000 */

	MKVIDPID(0x05c6, 0x920d),	/* Qualcomm Gobi 3000 */
	MKVIDPID(0x0af0, 0x8120),	/* Option Gobi 3000 */
	MKVIDPID(0x1410, 0xa021),	/* Novatel Gobi 3000 */
	MKVIDPID(0x413c, 0x8194),	/* Dell Gobi 3000 */
	MKVIDPID(0x12D1, 0x14F1),	/* Sony Gobi 3000 */
	{ }
};

MODULE_DEVICE_TABLE(usb, qc_vidpids);

static u8 nibble(unsigned char c)
{
	if (likely(isdigit(c)))
		return c - '0';
	c = toupper(c);
	if (likely(isxdigit(c)))
		return 10 + c - 'A';
	return 0;
}

static int discover_endpoints(struct qcusbnet *dev)
{
	int numends;
	int i;
	struct usb_host_endpoint *endpoint = NULL;
	struct usb_host_endpoint *int_in   = NULL;
	struct usb_host_endpoint *bulk_in  = NULL;
	struct usb_host_endpoint *bulk_out = NULL;
	int dir_in, dir_out, xfer_int;

	GOBI_DEBUG("interface number: %d",
		dev->iface->cur_altsetting->desc.bInterfaceNumber);

	numends = dev->iface->cur_altsetting->desc.bNumEndpoints;
	for (i = 0; i < numends; i++) {
		endpoint = dev->iface->cur_altsetting->endpoint + i;

		dir_in = usb_endpoint_dir_in(&endpoint->desc);
		dir_out = usb_endpoint_dir_out(&endpoint->desc);
		xfer_int = usb_endpoint_xfer_int(&endpoint->desc);

		GOBI_DEBUG("endpoint %d: addr %x "
			"dir_in %d dir_out %d xfer_int %d",
			i, endpoint->desc.bEndpointAddress,
			dir_in, dir_out, xfer_int);

		if (dir_in && xfer_int) {
			if (int_in) {
				GOBI_ERROR("multiple int_in endpoints");
				return -ENODEV;
			}
			GOBI_DEBUG("setting endpoint %d as int in", i);
			int_in = endpoint;
		} else if (dir_in && !xfer_int) {
			if (bulk_in) {
				GOBI_ERROR("multiple bulk_in endpoints");
				return -ENODEV;
			}
			GOBI_DEBUG("setting endpoint %d as bulk in", i);
			bulk_in = endpoint;
		} else if (dir_out && !xfer_int) {
			if (bulk_out) {
				GOBI_ERROR("multiple bulk_out endpoints");
				return -ENODEV;
			}
			GOBI_DEBUG("setting endpoint %d as bulk out", i);
			bulk_out = endpoint;
		} else {
			GOBI_DEBUG("ignoring endpoint %d", i);
		}
	}

	if (!int_in || !bulk_in || !bulk_out) {
		GOBI_ERROR("missing endpoint(s)");
		if (int_in)
			GOBI_ERROR("found int_in endpoint: %u",
				int_in->desc.bEndpointAddress);
		else
			GOBI_ERROR("didn't find int_in endpoint");
		if (bulk_in)
			GOBI_ERROR("found bulk_in endpoint: %u",
				bulk_in->desc.bEndpointAddress);
		else
			GOBI_ERROR("didn't find bulk_in endpoint");
		if (bulk_out)
			GOBI_ERROR("found bulk_out endpoint: %u",
				bulk_out->desc.bEndpointAddress);
		else
			GOBI_ERROR("didn't find bulk_out endpoint");
		return -ENODEV;
	}

	dev->iface_num     = dev->iface->cur_altsetting->desc.bInterfaceNumber;
	dev->int_in_endp   = int_in->desc.bEndpointAddress;
	dev->bulk_in_endp  = bulk_in->desc.bEndpointAddress;
	dev->bulk_out_endp = bulk_out->desc.bEndpointAddress;

	GOBI_DEBUG("found endpoints: iface_num %u "
		"int_in %u bulk_in %u bulk_out %u",
		dev->iface_num,
		dev->int_in_endp, dev->bulk_in_endp, dev->bulk_out_endp);

	return 0;
}

int qcnet_probe(struct usb_interface *iface, const struct usb_device_id *vidpids)
{
	int status;
	struct usbnet *usbnet;
	struct qcusbnet *dev;
	struct net_device_ops *netdevops;
	int i;
	int addr_len;
	u8 *addr;
	const char *id;

	status = usbnet_probe(iface, vidpids);
	if (status < 0) {
		GOBI_WARN("usbnet_probe failed: %d", status);
		goto fail;
	}

	usbnet = usb_get_intfdata(iface);

	if (!usbnet) {
		GOBI_ERROR("usbnet is NULL");
		status = -ENXIO;
		goto fail_disconnect;
	}

	if (!usbnet->net) {
		GOBI_ERROR("usbnet->net is NULL");
		status = -ENXIO;
		goto fail_disconnect;
	}

	dev = kmalloc(sizeof(struct qcusbnet), GFP_KERNEL);
	if (!dev) {
		GOBI_ERROR("failed to allocate struct qcusbnet");
		status = -ENOMEM;
		goto fail_disconnect;
	}

	dev->dying = false;
	dev->iface = iface;

	status = discover_endpoints(dev);
	if (status) {
		GOBI_ERROR("discover_endpoints failed: %d", status);
		goto fail_free;
	}

	usbnet->data[0] = (unsigned long)dev;

	dev->usbnet = usbnet;

	netdevops = kmalloc(sizeof(struct net_device_ops), GFP_KERNEL);
	if (!netdevops) {
		GOBI_ERROR("failed to allocate net device ops");
		status = -ENOMEM;
		goto fail_free;
	}
	memcpy(netdevops, usbnet->net->netdev_ops, sizeof(struct net_device_ops));

	/* TODO(ttuttle): Can we just make a static copy of this? */
	dev->open = netdevops->ndo_open;
	netdevops->ndo_open = qcnet_open;
	dev->stop = netdevops->ndo_stop;
	netdevops->ndo_stop = qcnet_stop;
	netdevops->ndo_start_xmit = qcnet_startxmit;
	netdevops->ndo_tx_timeout = qcnet_txtimeout;

	usbnet->net->netdev_ops = netdevops;

	memset(&(dev->usbnet->net->stats), 0, sizeof(struct net_device_stats));

	memset(dev->meid, '0', sizeof(dev->meid));
	memset(dev->imei, '0', sizeof(dev->imei));

	dev->valid = false;
	memset(&dev->qmi, 0, sizeof(dev->qmi));

	dev->qmi.devclass = devclass;

	kref_init(&dev->refcount);
	INIT_LIST_HEAD(&dev->node);
	INIT_LIST_HEAD(&dev->qmi.clients);
	dev->workqueue = alloc_ordered_workqueue("gobi", 0);

	spin_lock_init(&dev->urbs_lock);
	INIT_LIST_HEAD(&dev->urbs);
	dev->active = NULL;
	INIT_WORK(&dev->startxmit, qcnet_bg_startxmit);
	INIT_WORK(&dev->txtimeout, qcnet_bg_txtimeout);
	INIT_WORK(&dev->complete, qcnet_bg_complete);

	spin_lock_init(&dev->qmi.clients_lock);

	dev->down = 0;
	qc_setdown(dev, DOWN_NO_NDIS_CONNECTION);
	qc_setdown(dev, DOWN_NET_IFACE_STOPPED);

	status = qc_register(dev);
	if (status) {
		GOBI_ERROR("qc_register failed: %d", status);
		goto fail_destroy;
	}

	iface->needs_remote_wakeup = 1;

	mutex_lock(&qcusbnet_lock);
	/* Give our initial ref to the list */
	list_add(&dev->node, &qcusbnet_list);
	mutex_unlock(&qcusbnet_lock);

	/* After calling qc_register, either MEID or IMEI is valid */
	id = memchr_inv(dev->meid, '0', sizeof(dev->meid)) ?
			dev->meid : dev->imei;
	addr = usbnet->net->dev_addr;
	addr_len = usbnet->net->addr_len;
	if (addr_len > ETH_ALEN)
		addr_len = ETH_ALEN;

	for (i = 0; i < addr_len; i++)
		addr[i] = (nibble(id[i*2+2]) << 4) + nibble(id[i*2+3]);
	addr[0] &= 0xfe;		/* clear multicast bit */
	addr[0] |= 0x02;		/* set local assignment bit (IEEE802) */

	return 0;

fail_destroy:
	destroy_workqueue(dev->workqueue);
fail_free:
	kfree(dev);
fail_disconnect:
	usbnet_disconnect(iface);
fail:
	return status;
}
EXPORT_SYMBOL_GPL(qcnet_probe);

static void qcnet_disconnect(struct usb_interface *intf)
{
	struct usbnet *usbnet = usb_get_intfdata(intf);
	struct qcusbnet *dev = (struct qcusbnet *)usbnet->data[0];
	struct list_head *node, *tmp;
	struct urb *urb;

	intf->needs_remote_wakeup = 0;
	netif_carrier_off(usbnet->net);
	usbnet_disconnect(intf);

	qc_deregister(dev);

	destroy_workqueue(dev->workqueue);
	list_for_each_safe(node, tmp, &dev->urbs) {
		urb = list_entry(node, struct urb, urb_list);
		list_del(&urb->urb_list);
		free_urb_with_skb(urb);
	}
	qcusbnet_put(dev);
}

static struct usb_driver qcusbnet = {
	.name       = "gobi",
	.id_table   = qc_vidpids,
	.probe      = qcnet_probe,
	.disconnect = qcnet_disconnect,
	.suspend    = qc_suspend,
	.resume     = qc_resume,
	.supports_autosuspend = true,
};

static int __init modinit(void)
{
	devclass = class_create(THIS_MODULE, "QCQMI");
	if (IS_ERR(devclass)) {
		GOBI_ERROR("class_create failed: %ld", PTR_ERR(devclass));
		return -ENOMEM;
	}
	printk(KERN_INFO "%s: %s", DRIVER_DESC, DRIVER_VERSION);
	return usb_register(&qcusbnet);
}
module_init(modinit);

static void __exit modexit(void)
{
	usb_deregister(&qcusbnet);
	class_destroy(devclass);
}
module_exit(modexit);

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("Dual BSD/GPL");

module_param(gobi_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gobi_debug, "Debugging level");
