/* /linux/drivers/misc/modem_if/sipc5_io_device.c
 *
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/device.h>

#include <linux/platform_data/modem.h>
#ifdef CONFIG_LINK_DEVICE_C2C
#include <linux/platform_data/c2c.h>
#endif
#include "modem_prj.h"
#include "modem_utils.h"
#define DM_LOGGING_CH		28

enum iod_debug_flag_bit {
	IOD_DEBUG_IPC_LOOPBACK,
};

static unsigned long dbg_flags = 0;
module_param(dbg_flags, ulong, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(dbg_flags, "sipc iodevice debug flags\n");

static int fd_waketime = (6 * HZ);
module_param(fd_waketime, int, S_IRUGO);
MODULE_PARM_DESC(fd_waketime, "fd wake lock timeout");

static ssize_t show_waketime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int msec;
	char *p = buf;

	msec = jiffies_to_msecs(fd_waketime);

	p += sprintf(buf, "raw waketime : %ums\n", msec);

	return p - buf;
}

static ssize_t store_waketime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long msec;
	int ret;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	ret = kstrtoul(buf, 10, &msec);
	if (ret)
		return count;

	iod->waketime = msecs_to_jiffies(msec);
	fd_waketime = msecs_to_jiffies(msec);

	return count;
}

static struct device_attribute attr_waketime =
	__ATTR(waketime, S_IRUGO | S_IWUSR, show_waketime, store_waketime);

static ssize_t show_loopback(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	unsigned char *ip = (unsigned char *)&msd->loopback_ipaddr;
	char *p = buf;

	p += sprintf(buf, "%u.%u.%u.%u en(%d)\n", ip[0], ip[1], ip[2], ip[3],
		test_bit(IOD_DEBUG_IPC_LOOPBACK, &dbg_flags));

	return p - buf;
}

static ssize_t store_loopback(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	struct io_device *iod = to_io_device(miscdev);

	msd->loopback_ipaddr = ipv4str_to_be32(buf, count);
	if (msd->loopback_ipaddr)
		set_bit(IOD_DEBUG_IPC_LOOPBACK, &dbg_flags);
	else
		clear_bit(IOD_DEBUG_IPC_LOOPBACK, &dbg_flags);

	mif_info("loopback(%s), en(%d)\n", buf,
				test_bit(IOD_DEBUG_IPC_LOOPBACK, &dbg_flags));
	if (msd->loopback_start)
		msd->loopback_start(iod, msd);

	return count;
}

static struct device_attribute attr_loopback =
	__ATTR(loopback, S_IRUGO | S_IWUSR, show_loopback, store_loopback);

static void iodev_showtxlink(struct io_device *iod, void *args)
{
	char **p = (char **)args;
	struct link_device *ld = get_current_link(iod);

	if (iod->io_typ == IODEV_NET && IS_CONNECTED(iod, ld))
		*p += sprintf(*p, "%s: %s\n", iod->name, ld->name);
}

static ssize_t show_txlink(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	char *p = buf;

	iodevs_for_each(msd, iodev_showtxlink, &p);

	return p - buf;
}

static ssize_t store_txlink(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* don't change without gpio dynamic switching */
	return -EINVAL;
}

static struct device_attribute attr_txlink =
	__ATTR(txlink, S_IRUGO | S_IWUSR, show_txlink, store_txlink);

static ssize_t show_dm_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = to_io_device(miscdev);
	struct link_device *ld = get_current_link(iod);
	char *p = buf;

	p += sprintf(buf, "%d\n", ld->dm_log_enable);
	mif_info("dm logging enable:%d\n", ld->dm_log_enable);

	return p - buf;
}

static ssize_t store_dm_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = to_io_device(miscdev);
	struct link_device *ld = get_current_link(iod);

	ld->dm_log_enable = !!buf[0];
	ld->enable_dm(ld, ld->dm_log_enable);
	mif_info("dm logging enable:%d\n", ld->dm_log_enable);

	return count;
}

static struct device_attribute attr_dm_state =
	__ATTR(dm_state, S_IRUGO | S_IWUSR, show_dm_state, store_dm_state);


#if 0
static int rx_loopback(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod;
	struct link_device *ld = get_current_link(iod);
	struct sipc5_frame_data frm;
	unsigned headroom;
	unsigned tailroom = 0;
	int ret;

	headroom = tx_build_link_header(&frm, iod, ld, skb->len);

	if (ld->aligned)
		tailroom = sipc5_calc_padding_size(headroom + skb->len);

	/* We need not to expand skb in here. dev_alloc_skb (in rx_alloc_skb)
	 * already alloc 32bytes padding in headroom. 32bytes are enough.
	 */

	/* store IPC link header to start of skb
	 * this is skb_push not skb_put. different with misc_write.
	 */
	memcpy(skb_push(skb, headroom), frm.hdr, headroom);

	/* store padding */
	if (tailroom)
		skb_put(skb, tailroom);

	/* forward */
	ret = ld->send(ld, iod, skb);
	if (ret < 0)
		mif_err("%s->%s: ld->send fail: %d\n", iod->name,
				ld->name, ret);
	return ret;
}
#endif

static int rx_multi_pdp(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod; /* same with real_iod */
	struct net_device *ndev;
	struct iphdr *iphdr;
	struct ethhdr *ehdr;
	int ret;
	const char source[ETH_ALEN] = SOURCE_MAC_ADDR;

	ndev = iod->ndev;
	if (!ndev) {
		mif_info("%s: ERR! no iod->ndev\n", iod->name);
		return -ENODEV;
	}
#ifdef CONFIG_LINK_ETHERNET
	skb->protocol = eth_type_trans(skb, ndev);
#else
	skb->dev = ndev;
	/* check the version of IP */
	iphdr = (struct iphdr *)skb->data;
	if (iphdr->version == IP6VERSION)
		skb->protocol = htons(ETH_P_IPV6);
	else
		skb->protocol = htons(ETH_P_IP);

	if (iod->use_handover) {
		skb_push(skb, sizeof(struct ethhdr));
		ehdr = (void *)skb->data;
		memcpy(ehdr->h_dest, ndev->dev_addr, ETH_ALEN);
		memcpy(ehdr->h_source, source, ETH_ALEN);
		ehdr->h_proto = skb->protocol;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb_reset_mac_header(skb);

		skb_pull(skb, sizeof(struct ethhdr));
	}
#endif
	ndev->stats.rx_packets++;
	ndev->stats.rx_bytes += skb->len;

	if (in_interrupt())
		ret = netif_rx(skb);
	else
		ret = netif_rx_ni(skb);

	if (ret != NET_RX_SUCCESS)
		mif_info("%s: ERR! netif_rx fail (err %d)\n", iod->name, ret);

	return ret;
}

#define MAX_RXDATA_SIZE		0x0E00	/* 4 * 1024 - 512 */
static unsigned sipc5_get_packet_len(struct sipc5_link_hdr *hdr)
{
	return (hdr->cfg & SIPC5_EXT_FIELD_EXIST)
				? *((u32 *)&hdr->len) : hdr->len;
}

static unsigned sipc5_get_pad_len(struct sipc5_link_hdr *hdr, unsigned len)
{
	return (hdr->cfg & SIPC5_PADDING_EXIST)
					? sipc5_calc_padding_size(len) : 0;
}

/*TODO: process multi-formated IPC packets */
static int sipc5_hdr_unframe(struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = skbpriv(skb)->ld;
	struct header_data *hdr = &fragdata(iod, ld)->h_post;
	struct sipc5_link_hdr *sipc_hdr;
	unsigned len;
	struct rfs_hdr rfs_head;

	if (iod->format == IPC_BOOT)
		return 0;

	if (!hdr->frag_len) {
		/* First frame - remove SIPC5 header */
		if (unlikely(
			!sipc5_start_valid((struct sipc5_link_hdr *)skb->data)
			)) {
			mif_err("SIPC5 wrong CFG(0x%02x) %s\n",	skb->data[0],
								iod->name);
			return -EBADMSG;
		}
		sipc_hdr = (struct sipc5_link_hdr *)skb->data;
		hdr->len = sipc5_get_packet_len(sipc_hdr);
		len = sipc5_get_hdr_len(sipc_hdr); /* Header len*/
		memcpy(hdr->hdr, skb->data, len);
		skb_pull_inline(skb, len); /* remove sipc5 hdr */
		hdr->frag_len = len;

		if (iod->format == IPC_RFS) { /* Add the old RFS header */
			memset(&rfs_head, 0x00, sizeof(struct rfs_hdr));
			/* packet length - sipc5 hdr + rfs length*/
			rfs_head.len = hdr->len - len + sizeof(u32);
			memcpy(skb_push(skb, sizeof(u32)),
						&rfs_head.len, sizeof(u32));
			hdr->len += sizeof(u32);
		}
	}
	return 0;
}


static int sipc5_hdr_data_done(struct io_device *iod, struct sk_buff *skb,
								unsigned size)
{
	struct link_device *ld = skbpriv(skb)->ld;
	struct header_data *hdr = &fragdata(iod, ld)->h_post;
	struct sipc5_link_hdr *sipc_hdr;
	int multiframe = 0;

	if (iod->format == IPC_BOOT)
		return 0;

	hdr->frag_len += size;
	if (hdr->len - hdr->frag_len) {
		if (iod->format != IPC_RFS) {
			mif_info("frag_len=%u, len=%u\n", hdr->frag_len,
								hdr->len);
			multiframe = 1;
		}
	} else {
		mif_debug("done frag=%u\n", hdr->frag_len);
		hdr->frag_len = 0;
	}

	if (hdr->len < hdr->frag_len)
		panic("HSIC - fail sip5.0 framming\n");

	sipc_hdr = (struct sipc5_link_hdr *)hdr->hdr;
	if (sipc_hdr->cfg & SIPC5_CTL_FIELD_EXIST
						&& (sipc_hdr->ext.ctl & 0x80)) {
		multiframe = 1;
		mif_info("multi-frame\n");
	}

	return multiframe;
}

static void skb_queue_move(struct sk_buff_head *dst, struct sk_buff_head *src)
{
	/*TODO: need spinlock protection? */
	while (src->qlen)
		skb_queue_tail(dst, skb_dequeue(src));
}

static int rx_skb_enqueue_iod(struct link_device *ld,
			struct sipc5_link_hdr *sipc , struct sk_buff *skb)
{
	struct io_device *iod; /* real iod */
	int id;

	iod = link_get_iod_with_channel(ld, sipc->ch);
	if (unlikely(!iod)) {
		mif_err("packet channel error(%d)\n", sipc->ch);
		return -EINVAL;
	}

	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	/* network device ?? */
	if (iod->io_typ == IODEV_NET)
		return rx_multi_pdp(skb);

	if (sipc->cfg & SIPC5_CTL_FIELD_EXIST) {
		mif_err("SIPC5_CTL_FILED not support\n");
		return -EINVAL;
/*
		id = sipc->ext.ctl & 0x7F;
		if (unlikely(id >= 128)) {
			mif_err("multi-packet boundary over = %d\n", id);
			return -EINVAL;
		}
		skb_queue_tail(&iod->sk_multi_q[id], skb);
		if (!(sipc->ext.ctl & 0x80)) {
			skb_queue_move(&iod->sk_rx_q, &iod->sk_multi_q[id]);
			wake_up(&iod->wq);
		}
*/
	} else {
		skb_queue_tail(&iod->sk_rx_q, skb);
		wake_up(&iod->wq);
	}

	return 0;
}

static int rx_sipc5_pick_each_skb(struct io_device *iod, struct link_device *ld,
							struct sk_buff *skb_in)
{
	int ret = 0;
	struct sk_buff *skb;
	unsigned len, temp, pad_len;
	struct header_data *hdr = &fragdata(iod, ld)->h_data;
	struct sipc5_link_hdr *sipc_hdr;

	if (!hdr->frag_len)
		goto next_frame;

	/* Procss fragment packet */
	sipc_hdr = (struct sipc5_link_hdr *)hdr->hdr;
	len = sipc5_get_hdr_len(sipc_hdr);	/* Header length */
	if (hdr->frag_len < len) {
		/* Continue SIPC5 Header fragmentation */
		temp = len - hdr->frag_len;
		if (unlikely(skb_in->len < temp)) {
			mif_err("skb_in too small len=%d\n", skb_in->len);
			ret = -EINVAL;
			goto exit;
		}
		memcpy(hdr->hdr + hdr->frag_len, skb_in->data, temp);
		skb_pull_inline(skb_in, temp);
		hdr->frag_len += temp;
		hdr->len = sipc5_get_packet_len(sipc_hdr);

		/* TODO: SIPC5 raw packet was send to network stack, we should
		 * make a IP packet into a skb */

		skb = alloc_skb(len, GFP_ATOMIC);
		if (unlikely(!skb)) {
			mif_err("fragHeader skb alloc fail\n");
			ret = -ENOMEM;
			goto exit;
		}
		memcpy(skb_put(skb, len), hdr->hdr, len);
		/* Send SIPC5 Header skb to next stage*/
		ret =  rx_skb_enqueue_iod(ld, sipc_hdr, skb);
		if (unlikely(ret)) {
			dev_kfree_skb_any(skb);
			mif_err("skb enqueue fail\n");
			goto exit;
		}
	}
	pad_len = sipc5_get_pad_len(sipc_hdr, hdr->len); /* padding len */
	len = hdr->len - hdr->frag_len;		/* rest fragment length*/
	if (len <= pad_len) {			/* trim pad, fragment done */
		skb_pull_inline(skb_in, len);
		memset(hdr, 0x00, sizeof(struct header_data));
		goto next_frame;
	}

	skb = skb_clone(skb_in, GFP_ATOMIC);
	if (unlikely(!skb)) {
		mif_err("rx skb clone fail\n");
		ret = -ENOMEM;
		goto exit;
	}
	/*pr_skb("frag", skb);*/
	mif_ipc_log(MIF_IPC_FLAG, iod->msd, skb->data, skb->len);
	if (skb->len < len) {			/* continous fragment packet */
		hdr->frag_len += skb->len;
		skb_pull_inline(skb_in, skb->len);
	} else {				/* last fragment packet */
		skb_trim(skb, len);
		if (skb_in->len < len + pad_len) { /* ignore rest padding */
			hdr->frag_len += skb->len;
			skb_pull_inline(skb_in, skb_in->len);
		} else {
			skb_pull_inline(skb_in, len + pad_len);
			hdr->frag_len = 0;		/* fragment done */
			mif_debug("frag done\n");
		}
	}
	goto send_next_stage;

next_frame:
	skb = skb_clone(skb_in, GFP_ATOMIC);
	if (unlikely(!skb)) {
		mif_err("rx skb clone fail\n");
		ret = -ENOMEM;
		goto exit;
	}
	/*pr_skb("new", skb);*/
	mif_ipc_log(MIF_IPC_AP2RL, iod->msd, skb->data, skb->len);
	sipc_hdr = (struct sipc5_link_hdr *)skb->data;
	if (unlikely(!sipc5_start_valid(sipc_hdr))) {
		mif_com_log(iod->msd, "SIPC5 wrong CFG(0x%02x) %s\n",
			skb->data[0], ld->name);
		mif_err("SIPC5 wrong CFG(0x%02x) %s\n", skb->data[0], ld->name);
		dev_kfree_skb_any(skb);
		ret = -EBADMSG;
		goto exit;
	}
	len = sipc5_get_packet_len(sipc_hdr); /* Packet length */
	pad_len = sipc5_get_pad_len(sipc_hdr, len);
	if (skb->len < len + pad_len) {
		/* data fragment - fragment skb or large RFS packets.
		 * Save the SIPC5 header to iod's fragdata buffer and send
		 * data continuosly, then if frame was complete, clear the
		 * fragdata buffer */
		memset(hdr, 0x00, sizeof(struct header_data));
		memcpy(hdr->hdr, skb->data, sipc5_get_hdr_len(sipc_hdr));
		hdr->len = len;
		hdr->frag_len = skb->len;

		skb_trim(skb,  min(skb->len, len)); /* ignore padding data */
		skb_pull_inline(skb_in, skb_in->len);
	} else {
		skb_trim(skb, len);
		skb_pull_inline(skb_in, len + pad_len);
	}

send_next_stage:
	/* send skb to next stage */
	ret =  rx_skb_enqueue_iod(ld, sipc_hdr, skb);
	if (unlikely(ret)) {
		dev_kfree_skb_any(skb);
		mif_err("skb enqueue fail\n");
		goto exit;
	}

	if (skb_in->len) {
		sipc_hdr = (struct sipc5_link_hdr *)skb_in->data;
		len = sipc5_get_hdr_len(sipc_hdr);	/* Header length */
		if (unlikely(skb_in->len < len)) {
			memcpy(hdr->hdr, skb_in->data, skb_in->len);
			hdr->frag_len = skb_in->len;
			/* exit and conitune next rx packet */
		} else {
			goto next_frame;
		}
	}
	mif_debug("end of packets\n");
exit:
	return ret;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_skb_from_link_dev(struct io_device *iod,
		struct link_device *ld, struct sk_buff *skb_in)
{
	struct sk_buff *skb;
	int err;

	if (!skb_in) {
		mif_info("%s: ERR! !data\n", ld->name);
		return -EINVAL;
	}

	if (skb_in->len <= 0) {
		mif_info("%s: ERR! len %d <= 0\n", ld->name, skb_in->len);
		return -EINVAL;
	}

	switch (iod->format) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
	case IPC_MULTI_RAW:
		if (iod->waketime)
			wake_lock_timeout(&iod->wakelock, iod->waketime);

		err = rx_sipc5_pick_each_skb(iod, ld, skb_in);
		if (err < 0) {
			mif_com_log(iod->msd,
				"%s: ERR! rx_frame_from_link fail (err %d)\n",
				ld->name, err);
			mif_info("%s: ERR! rx_frame_from_link fail (err %d)\n",
				ld->name, err);
		}

		return err;

	case IPC_RAW_NCM:
		if (fd_waketime)
			wake_lock_timeout(&iod->wakelock, fd_waketime);

		skbpriv(skb_in)->ld = ld;
		skbpriv(skb_in)->iod = iod;
		skbpriv(skb_in)->real_iod = iod;
		return rx_multi_pdp(skb_in);

	case IPC_CMD:
	case IPC_BOOT:
	case IPC_RAMDUMP:
			/* save packet to sk_buff */
		skb = skb_clone(skb_in, GFP_ATOMIC);
		if (unlikely(!skb)) {
			mif_err("boot skb clone fail\n");
			return -ENOMEM;
		}
		mif_debug("boot len : %d\n", skb_in->len);
		skbpriv(skb)->ld = ld;
		skb_queue_tail(&iod->sk_rx_q, skb);
		wake_up(&iod->wq);

		return skb_in->len;

	default:
		mif_info("%s: ERR! unknown format %d\n", ld->name, iod->format);
		return -EINVAL;
	}
}

/* inform the IO device that the modem is now online or offline or
 * crashing or whatever...
 */
static void io_dev_modem_state_changed(struct io_device *iod,
			enum modem_state state)
{
	mif_info("%s: %s state changed (state %d)\n",
		iod->name, iod->mc->name, state);

	iod->mc->phone_state = state;

	if (state == STATE_CRASH_RESET || state == STATE_CRASH_EXIT ||
	    state == STATE_NV_REBUILDING) {
		wake_lock_timeout(&iod->wakelock, msecs_to_jiffies(2000));
		wake_up(&iod->wq);
	}
}

/**
 * io_dev_sim_state_changed
 * @iod:	IPC's io_device
 * @sim_online: SIM is online?
 */
static void io_dev_sim_state_changed(struct io_device *iod, bool sim_online)
{
	if (atomic_read(&iod->opened) == 0) {
		mif_info("%s: ERR! not opened\n", iod->name);
	} else if (iod->mc->sim_state.online == sim_online) {
		mif_info("%s: SIM state not changed\n", iod->name);
	} else {
		iod->mc->sim_state.online = sim_online;
		iod->mc->sim_state.changed = true;
		mif_info("%s: SIM state changed {online %d, changed %d}\n",
			iod->name, iod->mc->sim_state.online,
			iod->mc->sim_state.changed);
		wake_up(&iod->wq);
	}
}

static void iodev_dump_status(struct io_device *iod, void *args)
{
	if (iod->format == IPC_RAW && iod->io_typ == IODEV_NET) {
		struct link_device *ld = get_current_link(iod);
		mif_com_log(iod->mc->msd, "%s: %s\n", iod->name, ld->name);
	}
}

static int misc_open(struct inode *inode, struct file *filp)
{
	struct io_device *iod = to_io_device(filp->private_data);
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;
	int ret;
	filp->private_data = (void *)iod;

	atomic_inc(&iod->opened);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->init_comm) {
			ret = ld->init_comm(ld, iod);
			if (ret < 0) {
				mif_info("%s: init_comm fail(%d)\n",
					ld->name, ret);
				return ret;
			}
		}
	}

	mif_err("%s (opened %d)\n", iod->name, atomic_read(&iod->opened));

	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	atomic_dec(&iod->opened);
	skb_queue_purge(&iod->sk_rx_q);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->terminate_comm)
			ld->terminate_comm(ld, iod);
	}

	mif_err("%s (opened %d)\n", iod->name, atomic_read(&iod->opened));

	return 0;
}

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;

	if (skb_queue_empty(&iod->sk_rx_q))
		poll_wait(filp, &iod->wq, wait);

	if (!skb_queue_empty(&iod->sk_rx_q) &&
	    iod->mc->phone_state != STATE_OFFLINE) {
		return POLLIN | POLLRDNORM;
	} else if ((iod->mc->phone_state == STATE_CRASH_RESET) ||
		   (iod->mc->phone_state == STATE_CRASH_EXIT) ||
		   (iod->mc->phone_state == STATE_NV_REBUILDING) ||
		   (iod->mc->sim_state.changed)) {
		if (iod->format == IPC_RAW || iod->id == 0x1C) {
			msleep(20);
			return 0;
		}
		return POLLHUP;
	} else {
		return 0;
	}
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int p_state;
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	char cpinfo_buf[530] = "CP Crash ";
	unsigned long size;
	int ret;

	switch (cmd) {
	case IOCTL_MODEM_ON:
		mif_info("%s: IOCTL_MODEM_ON\n", iod->name);
		return iod->mc->ops.modem_on(iod->mc);

	case IOCTL_MODEM_OFF:
		mif_info("%s: IOCTL_MODEM_OFF\n", iod->name);
		return iod->mc->ops.modem_off(iod->mc);

	case IOCTL_MODEM_RESET:
		mif_info("%s: IOCTL_MODEM_RESET\n", iod->name);
		return iod->mc->ops.modem_reset(iod->mc);

	case IOCTL_MODEM_BOOT_ON:
		mif_info("%s: IOCTL_MODEM_BOOT_ON\n", iod->name);
		return iod->mc->ops.modem_boot_on(iod->mc);

	case IOCTL_MODEM_BOOT_OFF:
		mif_info("%s: IOCTL_MODEM_BOOT_OFF\n", iod->name);
		return iod->mc->ops.modem_boot_off(iod->mc);

	case IOCTL_MODEM_BOOT_DONE:
		mif_err("%s: IOCTL_MODEM_BOOT_DONE\n", iod->name);
		if (iod->mc->ops.modem_boot_done)
			return iod->mc->ops.modem_boot_done(iod->mc);
		else
			return 0;

	case IOCTL_MODEM_STATUS:
		mif_debug("%s: IOCTL_MODEM_STATUS\n", iod->name);

		p_state = iod->mc->phone_state;
		if ((p_state == STATE_CRASH_RESET) ||
		    (p_state == STATE_CRASH_EXIT)) {
			mif_info("%s: IOCTL_MODEM_STATUS (state %d)\n",
				iod->name, p_state);
		} else if (iod->mc->sim_state.changed) {
			int s_state = iod->mc->sim_state.online ?
					STATE_SIM_ATTACH : STATE_SIM_DETACH;
			iod->mc->sim_state.changed = false;
			return s_state;
		} else if (p_state == STATE_NV_REBUILDING) {
			mif_info("%s: IOCTL_MODEM_STATUS (state %d)\n",
				iod->name, p_state);
			iod->mc->phone_state = STATE_ONLINE;
		}
		return p_state;

	case IOCTL_MODEM_PROTOCOL_SUSPEND:
		mif_debug("%s: IOCTL_MODEM_PROTOCOL_SUSPEND\n",
			iod->name);

		if (iod->format != IPC_MULTI_RAW)
			return -EINVAL;

		iodevs_for_each(iod->msd, iodev_netif_stop, 0);
		return 0;

	case IOCTL_MODEM_PROTOCOL_RESUME:
		mif_info("%s: IOCTL_MODEM_PROTOCOL_RESUME\n",
			iod->name);

		if (iod->format != IPC_MULTI_RAW)
			return -EINVAL;

		iodevs_for_each(iod->msd, iodev_netif_wake, 0);
		return 0;

	case IOCTL_MODEM_DUMP_START:
		mif_info("%s: IOCTL_MODEM_DUMP_START\n", iod->name);
		return ld->dump_start(ld, iod);

	case IOCTL_MODEM_DUMP_UPDATE:
		mif_debug("%s: IOCTL_MODEM_DUMP_UPDATE\n", iod->name);
		return ld->dump_update(ld, iod, arg);

	case IOCTL_MODEM_FORCE_CRASH_EXIT:
		mif_info("%s: IOCTL_MODEM_FORCE_CRASH_EXIT\n", iod->name);
		if (iod->mc->ops.modem_force_crash_exit)
			return iod->mc->ops.modem_force_crash_exit(iod->mc);
		return -EINVAL;

	case IOCTL_MODEM_CP_UPLOAD:
		mif_info("%s: IOCTL_MODEM_CP_UPLOAD\n", iod->name);
		if (copy_from_user(cpinfo_buf + strlen(cpinfo_buf),
			(void __user *)arg, MAX_CPINFO_SIZE) != 0)
			return -EFAULT;
		panic(cpinfo_buf);
		return 0;

	case IOCTL_MODEM_DUMP_RESET:
		mif_info("%s: IOCTL_MODEM_DUMP_RESET\n", iod->name);
		return iod->mc->ops.modem_dump_reset(iod->mc);

	case IOCTL_MIF_LOG_DUMP:
		iodevs_for_each(iod->msd, iodev_dump_status, 0);
		size = MAX_MIF_BUFF_SIZE;
		ret = copy_to_user((void __user *)arg, &size,
			sizeof(unsigned long));
		if (ret < 0)
			return -EFAULT;

		mif_dump_log(iod->mc->msd, iod);
		return 0;

	case IOCTL_MIF_DPRAM_DUMP:
#ifdef CONFIG_LINK_DEVICE_DPRAM
		if (iod->mc->mdm_data->link_types & LINKTYPE(LINKDEV_DPRAM)) {
			size = iod->mc->mdm_data->dpram_ctl->dp_size;
			ret = copy_to_user((void __user *)arg, &size,
				sizeof(unsigned long));
			if (ret < 0)
				return -EFAULT;
			mif_dump_dpram(iod);
			return 0;
		}
#endif
		return -EINVAL;

	default:
		 /* If you need to handle the ioctl for specific link device,
		  * then assign the link ioctl handler to ld->ioctl
		  * It will be call for specific link ioctl */
		if (ld->ioctl)
			return ld->ioctl(ld, iod, cmd, arg);

		mif_info("%s: ERR! cmd 0x%X not defined.\n", iod->name, cmd);
		return -EINVAL;
	}
	return 0;
}

static unsigned get_fmt_multiframe_id(void)
{
	/*TODO: get unique fmt multiframe id*/
	return 0;
}

/* return copied count size
 */
static unsigned sipc5_hdr_build_frame(struct sipc5_link_hdr *hdr,
	struct io_device *iod, struct link_device *ld, unsigned count,
							unsigned *mfrm)
{
	unsigned hdr_len, len = count;
	unsigned frm_id = (mfrm) ? *mfrm : 0; /*vnet_xmit mfrm not required*/
	unsigned rfs_length_hdr = (iod->format == IPC_RFS) ? sizeof(u32) : 0;

	if (iod->ipc_version == NO_SIPC_VER || iod->format == IPC_BOOT)
		return 0;

	hdr->cfg = SIPC5_START_MASK;
	hdr->ch = iod->id;

	if ((ld->fmt_multiframe && iod->format == IPC_FMT && len > 2048)
								|| frm_id) {
		len = min_t(unsigned, len, 2048);
		hdr_len = SIPC5_HDR_CTRL_LEN;
		hdr->cfg |= (SIPC5_EXT_FIELD_EXIST | SIPC5_CTL_FIELD_EXIST);
		hdr->len = hdr_len + len;
		if (mfrm && !frm_id) {/* first of multiframe */
			*mfrm = get_fmt_multiframe_id();
			frm_id = *mfrm;
		}
		hdr->ext.ctl = frm_id & 0x7F;
		if (len != count) /* not last frame */
			hdr->ext.ctl |= 0x80;
/*	} else if (iod->format == IPC_RFS) {
		hdr->cfg |= SIPC5_EXT_FIELD_EXIST;
		hdr_len = SIPC5_HDR_EXT_LEN;
		*((u32 *)&hdr->len) = (u32)(len + hdr_len - rfs_length_hdr); */
	} else if (len > 0xFFFF - SIPC5_HDR_LEN_MAX) {
		hdr->cfg |= SIPC5_EXT_FIELD_EXIST;
		hdr_len = SIPC5_HDR_EXT_LEN;
		*((u32 *)&hdr->len) = (u32)(len + hdr_len - rfs_length_hdr);
	} else {
		hdr_len = SIPC5_HDR_LEN;
		hdr->len = (u16)(len + hdr_len - rfs_length_hdr);
	}
	if (ld->aligned)
		hdr->cfg |= SIPC5_PADDING_EXIST;
	return hdr_len;
}

static void check_ipc_loopback(struct io_device *iod, struct sk_buff *skb)
{

	struct sipc_fmt_hdr *ipc;
	struct sk_buff *skb_bk;
	struct link_device *ld = skbpriv(skb)->ld;
	int ret;


	if (!skb || !test_bit(IOD_DEBUG_IPC_LOOPBACK, &dbg_flags)
						|| iod->format != IPC_FMT)
		return;

	ipc = (struct sipc_fmt_hdr *)(skb->data + sipc5_get_hdr_len(
					(struct sipc5_link_hdr *)skb->data));
	if (ipc->main_cmd == 0x90) { /* loop-back */
		skb_bk = skb_clone(skb, GFP_KERNEL);
		if (!skb_bk) {
			mif_err("SKB clone fail\n");
			return;
		}
		ret = ld->send(ld, iod,	skb_bk);
		if (ret < 0) {
			mif_err("ld->send fail (%s, err %d)\n", iod->name, ret);
		}
	}
	return;
}

/* It send continous transation with sperate skb, BOOTDATA size should divided
 * with 512B(USB packet size) and remove the zero length packet for mark not
 * end of packet.
 */
#define MAX_BOOTDATA_SIZE	0xE00	/* EBL package format*/
static size_t _boot_write(struct io_device *iod, const char __user *buf,
								size_t count)
{
	int rest_len = count, frame_len = 0;
	char *cur = (char *)buf;
	struct sk_buff *skb = NULL;
	struct link_device *ld = get_current_link(iod);
	int ret;

	while (rest_len) {
		frame_len = min(rest_len, MAX_BOOTDATA_SIZE);
		skb = alloc_skb(frame_len, GFP_KERNEL);
		if (!skb) {
			mif_err("fail alloc skb (%d)\n", __LINE__);
			return -ENOMEM;
		}
		if (copy_from_user(
				skb_put(skb, frame_len), cur, frame_len) != 0) {
			dev_kfree_skb_any(skb);
			return -EFAULT;
		}
		rest_len -= frame_len;
		cur += frame_len;

		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;

		/* non-zerolength packet*/
		if (rest_len)
			skbpriv(skb)->nzlp = true;

		mif_debug("rest=%d, frame=%d, nzlp=%d\n", rest_len, frame_len,
							skbpriv(skb)->nzlp);
		ret = ld->send(ld, iod, skb);
		if (ret < 0) {
			dev_kfree_skb_any(skb);
			return ret;
		}
	}
	return count;
}

#define ALIGN_BYTE 4
#define SIPC_ALIGN(x) ALIGN(x, ALIGN_BYTE)
#define SIPC_PADLEN(x) (ALIGN(x, ALIGN_BYTE) - x)
static ssize_t misc_write(struct file *filp, const char __user *data,
			size_t count, loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct sk_buff *skb;
	int ret;
	struct sipc5_link_hdr hdr;
	unsigned len, headroom = 0, copied = 0, fmt_mfrm_id = 0, tx_size;

	if (iod->format <= IPC_RFS && iod->id == 0)
		return -EINVAL;

fmt_multiframe:
	headroom = sipc5_hdr_build_frame(&hdr, iod, ld, count - copied,
								&fmt_mfrm_id);
	len = headroom ? sipc5_get_packet_len(&hdr) - headroom : count;
	skb = alloc_skb(SIPC_ALIGN(len + headroom), GFP_KERNEL);
	if (!skb) {
		if (len > MAX_BOOTDATA_SIZE && iod->format == IPC_BOOT) {
			mif_info("large alloc fail\n");
			return _boot_write(iod, data, count);
		}
		mif_err("alloc_skb fail (%s, size:%d)\n", iod->name,
							SIPC_ALIGN(len));
		return -ENOMEM;
	}

	if (headroom) {					/*Copy SIPC header*/
		if (iod->format == IPC_RFS) {
			copied += sizeof(u32);
			if (len + sizeof(u32) != *((u32 *)data))
				mif_err("WARN!! rfs len(%u), write len(%u)\n",
					len, *((u32 *)data));
			mif_debug("rfs: count:%u, hdr:%u, pkt:%u, write:%u",
				count, headroom, len, *((u32 *)data));
		}
		memcpy(skb_put(skb, headroom), &hdr, headroom);
	}
	if (copy_from_user(skb_put(skb, len), data + copied, len) != 0) {
		if (skb)
			dev_kfree_skb_any(skb);
		return -EFAULT;
	}
	copied += len;
	if (ld->aligned) {				/*Add 4byte pad*/
		skb_set_tail_pointer(skb, SIPC_ALIGN(skb->len));
		skb->len = SIPC_ALIGN(skb->len);
	}

	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	tx_size = skb->len;
	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		mif_err("ld->send fail (%s, err %d)\n", iod->name, ret);
		return ret;
	}

	if (ld->fmt_multiframe && iod->format == IPC_FMT && copied < count) {
		mif_info("continoue fmt multiframe(%u/%u)\n", copied, count);
		goto fmt_multiframe;
	}

	if (ret != tx_size)
		mif_info("wrong tx size (%s, count:%d copied:%d ret:%d)\n",
			iod->name, count, tx_size, ret);

	return count;
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
			loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *skb;
	unsigned copied = 0, len;
	int status;

continue_multiframe:
	skb = skb_dequeue(rxq);
	if (!skb) {
		mif_info("%s: ERR! no data in rxq\n", iod->name);
		goto exit;
	}

	check_ipc_loopback(iod, skb);

	/* unframe the SIPC5 header */
	status = sipc5_hdr_unframe(iod, skb);
	if (unlikely(status < 0)) {
		dev_kfree_skb_any(skb);
		return status;
	}

	len = min(skb->len, count - copied);
	if (copy_to_user(buf + copied, skb->data, len)) {
		mif_info("%s: ERR! copy_to_user fail\n", iod->name);
		skb_queue_head(rxq, skb);
		return -EFAULT;
	}

	skb_pull_inline(skb, len);
	status = sipc5_hdr_data_done(iod, skb, len);
	copied += len;

	mif_debug("%s: data:%d count:%d copied:%d qlen:%d\n",
				iod->name, len, count, copied, rxq->qlen);

	if (skb->len)
		skb_queue_head(rxq, skb);
	else
		dev_kfree_skb_any(skb);

	if (status == 1 && count - copied) {
		mif_info("multi_frame\n");
		goto continue_multiframe;
	}
exit:
	return copied;
}

#ifdef CONFIG_LINK_DEVICE_C2C
static int misc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int r = 0;
	unsigned long size = 0;
	unsigned long pfn = 0;
	unsigned long offset = 0;
	struct io_device *iod = (struct io_device *)filp->private_data;

	if (!vma)
		return -EFAULT;

	size = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;
	if (offset + size > (C2C_CP_RGN_SIZE + C2C_SH_RGN_SIZE)) {
		mif_info("ERR: offset + size > C2C_CP_RGN_SIZE\n");
		return -EINVAL;
	}

	/* Set the noncacheable property to the region */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED | VM_IO;

	pfn = __phys_to_pfn(C2C_CP_RGN_ADDR + offset);
	r = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
	if (r) {
		mif_info("ERR: Failed in remap_pfn_range()!!!\n");
		return -EAGAIN;
	}

	mif_info("%s: VA = 0x%08lx, offset = 0x%lx, size = %lu\n",
		iod->name, vma->vm_start, offset, size);

	return 0;
}
#endif

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
	.write = misc_write,
	.read = misc_read,
#ifdef CONFIG_LINK_DEVICE_C2C
	.mmap = misc_mmap,
#endif
};

static int vnet_open(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);

	mif_err("%s\n", vnet->iod->name);

	netif_start_queue(ndev);
	atomic_inc(&vnet->iod->opened);
	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);

	mif_err("%s\n", vnet->iod->name);

	atomic_dec(&vnet->iod->opened);
	netif_stop_queue(ndev);
	skb_queue_purge(&vnet->iod->sk_rx_q);
	return 0;
}

static int vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	struct io_device *iod = vnet->iod;
	struct link_device *ld = get_current_link(iod);
	struct sk_buff *skb_new;
	int ret;
	unsigned headroom = 0, tailroom = 0;
	unsigned long tx_bytes = skb->len;
/*	struct iphdr *ip_header = NULL;*/
	struct sipc5_link_hdr hdr;

	/* When use `handover' with Network Bridge,
	 * user -> bridge device(rmnet0) -> real rmnet(xxxx_rmnet0) -> here.
	 * bridge device is ethernet device unlike xxxx_rmnet(net device).
	 * We remove the an ethernet header of skb before using skb->len,
	 * because bridge device added an ethernet header to skb.
	 */
	if (iod->use_handover) {
		if (iod->id >= PS_DATA_CH_0 && iod->id <= PS_DATA_CH_LAST)
			skb_pull(skb, sizeof(struct ethhdr));
	}

	headroom = sipc5_hdr_build_frame(&hdr, iod, ld, skb->len, NULL);
	if (!headroom) {
		skb_new = skb;
		goto sipc_done;
	}
#if 0
	/* ip loop-back */
	ip_header = (struct iphdr *)skb->data;
	if (iod->msd->loopback_ipaddr &&
		ip_header->daddr == iod->msd->loopback_ipaddr) {
		swap(ip_header->saddr, ip_header->daddr);
		frm.ch_id = DATA_LOOPBACK_CHANNEL;
		frm.hdr[SIPC5_CH_ID_OFFSET] = DATA_LOOPBACK_CHANNEL;
	}
#endif
	tailroom = (ld->aligned) ? SIPC_PADLEN(skb->len) : 0;
	/* We assume that skb */
	if (skb_headroom(skb) < headroom && skb_tailroom(skb) < tailroom) {
		mif_debug("%s: skb_copy_expand needed\n", iod->name);
		skb_new = skb_copy_expand(skb, headroom, SIPC_PADLEN(skb->len),
								GFP_ATOMIC);
		/* skb_copy_expand success or not, free old skb from caller */
		dev_kfree_skb_any(skb);
		if (!skb_new) {
			mif_info("%s: ERR! skb_copy_expand fail\n", iod->name);
			return NETDEV_TX_BUSY;
		}
	} else {
		skb_new = skb;
	}
	memcpy(skb_push(skb_new, headroom), &hdr, headroom);

	if (ld->aligned) {				/*Add 4byte pad*/
		skb_set_tail_pointer(skb, SIPC_ALIGN(skb->len));
		skb->len = SIPC_ALIGN(skb->len);
	}

sipc_done:
	skbpriv(skb_new)->iod = iod;
	skbpriv(skb_new)->ld = ld;

	ret = ld->send(ld, iod, skb_new);
	if (ret < 0) {
		netif_stop_queue(ndev);
		mif_info("%s: ERR! ld->send fail (err %d)\n", iod->name, ret);
		return NETDEV_TX_BUSY;
	}

	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += tx_bytes;

	return NETDEV_TX_OK;
}

static struct net_device_ops vnet_ops = {
	.ndo_open = vnet_open,
	.ndo_stop = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
};

/* ehter ops for CDC-NCM */
#ifdef CONFIG_LINK_ETHERNET
static struct net_device_ops vnet_ether_ops = {
	.ndo_open = vnet_open,
	.ndo_stop = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};
#endif
static void vnet_setup(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->addr_len = 0;
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

static void vnet_setup_ether(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_ETHER;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST | IFF_SLAVE;
	ndev->addr_len = ETH_ALEN;
	random_ether_addr(ndev->dev_addr);
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

int sipc5_init_io_device(struct io_device *iod)
{
	int ret = 0;
	struct vnet *vnet;

	/* Get modem state from modem control device */
	iod->modem_state_changed = io_dev_modem_state_changed;

	iod->sim_state_changed = io_dev_sim_state_changed;

	/* Get data from link device */
	mif_debug("%s: SIPC version = %d\n", iod->name, iod->ipc_version);
	iod->recv_skb = io_dev_recv_skb_from_link_dev;

	/* Register misc or net device */
	switch (iod->io_typ) {
	case IODEV_MISC:
		init_waitqueue_head(&iod->wq);
		skb_queue_head_init(&iod->sk_rx_q);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register failed\n", iod->name);

		/*DM logging for XMM6360*/
		if (iod->id == DM_LOGGING_CH) {
			ret = device_create_file(iod->miscdev.this_device,
							&attr_dm_state);
			if (ret)
				mif_err("failed to create `dm_state' : %s\n",
					iod->name);
			else
				mif_err("dm_state : %s, sucess\n",
					iod->name);
		}
		break;

	case IODEV_NET:
		skb_queue_head_init(&iod->sk_rx_q);
#ifdef CONFIG_LINK_ETHERNET
		iod->ndev = alloc_etherdev(0);
		if (!iod->ndev) {
			mif_err("failed to alloc netdev\n");
			return -ENOMEM;
		}
		iod->ndev->netdev_ops = &vnet_ether_ops;
		iod->ndev->watchdog_timeo = 5 * HZ;
#else
		if (iod->use_handover)
			iod->ndev = alloc_netdev(0, iod->name,
						vnet_setup_ether);
		else
			iod->ndev = alloc_netdev(0, iod->name, vnet_setup);

		if (!iod->ndev) {
			mif_info("%s: ERR! alloc_netdev fail\n", iod->name);
			return -ENOMEM;
		}
#endif
		/*register_netdev parsing % */
		strcpy(iod->ndev->name, "rmnet%d");

		ret = register_netdev(iod->ndev);
		if (ret) {
			mif_info("%s: ERR! register_netdev fail\n", iod->name);
			free_netdev(iod->ndev);
		}

		mif_debug("iod 0x%p\n", iod);
		vnet = netdev_priv(iod->ndev);
		mif_debug("vnet 0x%p\n", vnet);
		vnet->iod = iod;

		break;

	case IODEV_DUMMY:
		skb_queue_head_init(&iod->sk_rx_q);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register fail\n", iod->name);
		ret = device_create_file(iod->miscdev.this_device,
					&attr_waketime);
		if (ret)
			mif_info("%s: ERR! device_create_file fail\n",
				iod->name);
		ret = device_create_file(iod->miscdev.this_device,
				&attr_loopback);
		if (ret)
			mif_err("failed to create `loopback file' : %s\n",
					iod->name);
		ret = device_create_file(iod->miscdev.this_device,
				&attr_txlink);
		if (ret)
			mif_err("failed to create `txlink file' : %s\n",
					iod->name);
		break;

	default:
		mif_info("%s: ERR! wrong io_type %d\n", iod->name, iod->io_typ);
		return -EINVAL;
	}

	return ret;
}

