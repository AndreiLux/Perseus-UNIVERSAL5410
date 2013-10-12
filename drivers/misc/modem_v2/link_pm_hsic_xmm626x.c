/* /linux/drivers/misc/modem_if/modem_link_pm_xmm626x.c
 *
 * Copyright (C) 2012 Google, Inc.
 * Copyright (C) 2012 Samsung Electronics.
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

#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>

#include <linux/platform_data/modem.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_hsic_ncm.h"

#define IOCTL_LINK_CONTROL_ENABLE	_IO('o', 0x30)
#define IOCTL_LINK_CONTROL_ACTIVE	_IO('o', 0x31)
#define IOCTL_LINK_GET_HOSTWAKE		_IO('o', 0x32)
#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_LINK_SET_BIAS_CLEAR	_IO('o', 0x34)
#define IOCTL_LINK_GET_PHONEACTIVE	_IO('o', 0x35)

#define HOSTWAKE_TRIGLEVEL 0
#define get_hostwake(p) \
	(gpio_get_value((p)->pdata->gpio_link_hostwake) == HOSTWAKE_TRIGLEVEL)

#define get_hostactive(p) \
	(gpio_get_value((p)->pdata->gpio_link_active))

#define MAX_SUSPEND_BUSY 20

/* /sys/module/modem_link_pm_xmm626x/parameters/...*/
static int l2_delay = 200;
module_param(l2_delay, int, S_IRUGO);
MODULE_PARM_DESC(l2_delay, "HSIC autosuspend delay");

static int hub_delay = 500;
module_param(hub_delay, int, S_IRUGO);
MODULE_PARM_DESC(hub_delay, "Root-hub autosuspend delay");

enum linkpm_status {
	HSIC_LINKPM_L0,
	HSIC_LINKPM_L2,
	HSIC_LINKPM_L3,
	HSIC_LINKPM_L3_WAKE,
	HSIC_LINKPM_L3_L0,
	HSIC_LINKPM_L3_WAIT,
};

enum linkpm_event {
	LINKPM_EVENT_RUNTIME,
};

struct xmm626x_linkpm_data {
	struct miscdevice miscdev;
	struct list_head link;
	struct platform_device *pdev;
	struct modemlink_pm_data *pdata;
	struct usb_device *udev;
	struct usb_device *hdev;
	struct notifier_block usb_nfb;
	struct notifier_block pm_nfb;

	bool link_connected;
	bool l3l0_req; /* hub resume require */
	bool dpm_suspending;
	bool resume_req;
	spinlock_t lock;
	struct workqueue_struct *wq;
	struct delayed_work link_pm_work;
	struct delayed_work link_pm_event;
	unsigned long events;
	int suspend_busy_cnt;
	int resume_cnt;
	struct wake_lock l2_wake;
};

enum known_device_type {
	MIF_UNKNOWN_DEVICE,
	MIF_MAIN_DEVICE,
	MIF_BOOT_DEVICE,
};

struct  link_usb_id {
	int vid;
	int pid;
	enum known_device_type type;
};

/*TODO: get pid, vid from platform data */
static struct link_usb_id xmm626x_ids[] = {
	{0x1519, 0x0443, MIF_MAIN_DEVICE}, /* XMM6360 */
	{0x8087, 0x0716, MIF_BOOT_DEVICE}, /* XMM6360 */
	{0x1519, 0x0020, MIF_MAIN_DEVICE}, /* XMM6262 */
	{0x058b, 0x0041, MIF_BOOT_DEVICE}, /* XMM6262 */
};

struct linkpm_devices {
	struct list_head pmdata;
	spinlock_t lock;
} xmm626x_devices;

/* hooking from generic_suspend and generic_resume */
static int (*_usb_suspend) (struct usb_device *, pm_message_t);
static int (*_usb_resume) (struct usb_device *, pm_message_t);

static int xmm626x_linkpm_known_device(struct xmm626x_linkpm_data *pmdata,
				const struct usb_device_descriptor *desc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(xmm626x_ids); i++) {
		if (xmm626x_ids[i].vid == desc->idVendor &&
					xmm626x_ids[i].pid == desc->idProduct)
			return xmm626x_ids[i].type;
	}
	return MIF_UNKNOWN_DEVICE;
}

static struct xmm626x_linkpm_data *linkdata_from_udev(struct usb_device *udev)
{
	struct xmm626x_linkpm_data *pmdata = NULL;

	spin_lock_bh(&xmm626x_devices.lock);
	list_for_each_entry(pmdata, &xmm626x_devices.pmdata, link) {
		if (pmdata && (udev == pmdata->udev || udev == pmdata->hdev)) {
			spin_unlock_bh(&xmm626x_devices.lock);
			return pmdata;
		}
		mif_debug("udev=%p, %s\n", udev, dev_name(&udev->dev));
	}
	spin_unlock_bh(&xmm626x_devices.lock);
	return NULL;
}

static void usb_cp_crash(struct usb_device *udev, char *msg)
{
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);
	struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_err("Err_msg : %s\n", msg);
	pdata->cp_force_crash_exit();
}

static void set_slavewake(struct modemlink_pm_data *pm_data, int val)
{
	if (!val) {
		gpio_set_value(pm_data->gpio_link_slavewake, 0);
	} else {
		if (gpio_get_value(pm_data->gpio_link_slavewake)) {
			mif_info("warn.. slavewake toggle\n");
			gpio_set_value(pm_data->gpio_link_slavewake, 0);
			msleep(20);
		}
		gpio_set_value(pm_data->gpio_link_slavewake, 1);
	}
	mif_debug("slave wake(%d)\n",
		gpio_get_value(pm_data->gpio_link_slavewake));
}

static void xmm626x_gpio_l3tol0_resume(struct xmm626x_linkpm_data *pmdata)
{
	pmdata->l3l0_req = false;

	if (get_hostactive(pmdata))
		return;

	if (!get_hostwake(pmdata)) {
		set_slavewake(pmdata->pdata, 1);
		msleep(20);
	}

	gpio_set_value(pmdata->pdata->gpio_link_active, 1);
	printk(KERN_DEBUG "mif: host active(%d)\n", get_hostactive(pmdata));
	if (pmdata->pdata->wait_cp_resume)
		pmdata->pdata->wait_cp_resume(pmdata->pdata->port);

	return;
}

static int xmm626x_gpio_l2tol0_resume(struct xmm626x_linkpm_data *pmdata)
{
	int spin = 20;

	/* CP initiated L2->L0 */
	if (get_hostwake(pmdata))
		goto exit;

	/* AP initiated L2->L0 */
	set_slavewake(pmdata->pdata, 1);

	while (spin-- && !get_hostwake(pmdata))
		mdelay(5);

	if (!get_hostwake(pmdata))
		return -ETIMEDOUT;
exit:
	return 0;
}

static int xmm626x_linkpm_usb_resume(struct usb_device *udev, pm_message_t msg)
{
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);
	struct device *dev = &udev->dev;
	int ret = 0;
	int cnt = 10;

	if (!pmdata)
		goto generic_resume;

	if (pmdata->dpm_suspending) {
		mif_info("dpm_suspending, resume_req\n");
		pmdata->resume_req = true;
	}

	/* root hub resume
	   If Host active was low before root hub resume, do L3->L0 sequence */
	if (udev == pmdata->hdev) {
		xmm626x_gpio_l3tol0_resume(pmdata);
		goto generic_resume;
	}

	/* Because HSIC modem skip the hub dpm_resume by quirk, if root hub
	  dpm_suspend was called at runtmie active status, hub resume was not
	  call by port runtime resume. So, it check the L3 status and root hub
	  resume before port resume */
	if (!get_hostactive(pmdata) || pmdata->l3l0_req) {
		mif_err("usb1 root hub resume first\n");
		ret = usb_resume(&pmdata->hdev->dev, PMSG_RESUME);
		if (ret)
			mif_err("hub resume fail\n");
	}

	/* get wake lock */
	wake_lock(&pmdata->l2_wake);
	mif_debug("get wakelock\n");

retry:
	/* Sometimes IMC modem send remote wakeup with gpio,
	   we should check the runtime status and if already resumed*/
	if (dev->power.runtime_status == RPM_ACTIVE) {
		mif_info("aleady resume, skip gpio resume\n");
		goto generic_resume;
	}

	ret = xmm626x_gpio_l2tol0_resume(pmdata);
	if (ret < 0) {
		if (cnt--) {
			mif_err("xmm626x_gpio_l2tol0_resume fail(%d)\n", ret);
			goto retry;
		} else  {
			mif_err("hostwakeup fail, usb_cp_crash\n");
			wake_unlock(&pmdata->l2_wake);
			usb_cp_crash(pmdata->udev, "HostWakeup Fail");
		}
	}
done:
	pmdata->suspend_busy_cnt = MAX_SUSPEND_BUSY;
	pmdata->resume_req = false;

generic_resume:
	return _usb_resume(udev, msg);
}

static int xmm626x_linkpm_usb_suspend(struct usb_device *udev, pm_message_t msg)
{
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);

	if (!pmdata || udev == pmdata->hdev)
		goto generic_suspend;

	if (pmdata->pdata->gpio_link_suspend_req
		&& !gpio_get_value(pmdata->pdata->gpio_link_suspend_req)) {
		mif_info("CP not ready to enter L2\n");
		return -EBUSY;
	}

	if (pmdata->dpm_suspending && pmdata->resume_req) {
		mif_info("resume_req");
		return -EBUSY;
	}

	/* release wake lock */
	wake_lock_timeout(&pmdata->l2_wake, msecs_to_jiffies(50));
	mif_debug("release wakelock timeout\n");

generic_suspend:
	return _usb_suspend(udev, msg);
}

static int xmm626x_linkpm_usb_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct xmm626x_linkpm_data *pmdata =
			container_of(nfb, struct xmm626x_linkpm_data, usb_nfb);
	struct usb_device *udev = arg;
	struct usb_device_driver *udriver =
					to_usb_device_driver(udev->dev.driver);
	const struct usb_device_descriptor *desc = &udev->descriptor;
	unsigned long flags;

	switch (event) {
	case USB_DEVICE_ADD:
		switch (xmm626x_linkpm_known_device(pmdata, desc)) {
		case MIF_MAIN_DEVICE:
			if (pmdata->udev) {
				mif_err("pmdata was assigned for udev=%p\n",
								pmdata->udev);
				return NOTIFY_DONE;
			}
			/* clear previous event and stop event work*/
			pmdata->events = 0;
			cancel_delayed_work_sync(&pmdata->link_pm_event);

			pmdata->udev = udev;
			pmdata->hdev = udev->bus->root_hub;
			mif_info("pmdata=%p, udev=%p, hdev=%p\n", pmdata, udev,
								pmdata->hdev);
			spin_lock_irqsave(&pmdata->lock, flags);
			if (!_usb_resume && udriver->resume) {
				_usb_resume = udriver->resume;
				udriver->resume = xmm626x_linkpm_usb_resume;
			}
			if (!_usb_suspend && udriver->suspend) {
				_usb_suspend = udriver->suspend;
				udriver->suspend = xmm626x_linkpm_usb_suspend;
			}
			spin_unlock_irqrestore(&pmdata->lock, flags);
			mif_info("hook: (%pf, %pf), (%pf, %pf)\n",
					_usb_resume, udriver->resume,
					_usb_suspend,	udriver->suspend);
			pmdata->link_connected = true;
			pmdata->resume_req = false;
			enable_irq(gpio_to_irq(
					pmdata->pdata->gpio_link_hostwake));
			pmdata->suspend_busy_cnt = MAX_SUSPEND_BUSY;
			pmdata->resume_cnt = 0;
			pmdata->dpm_suspending = false;
			set_bit(LINKPM_EVENT_RUNTIME, &pmdata->events);
			queue_delayed_work(pmdata->wq, &pmdata->link_pm_event,
							msecs_to_jiffies(500));
			break;
		case MIF_BOOT_DEVICE:
			mif_info("boot dev connected\n");
			pmdata->link_connected = true;
			break;
		default:
			break;
		}
		break;
	case USB_DEVICE_REMOVE:
		switch (xmm626x_linkpm_known_device(pmdata, desc)) {
		case MIF_MAIN_DEVICE:
			disable_irq(gpio_to_irq(
					pmdata->pdata->gpio_link_hostwake));
			pmdata->link_connected = false;
			cancel_delayed_work_sync(&pmdata->link_pm_work);
			mif_info("unhook: (%pf, %pf), (%pf, %pf)\n",
					_usb_resume, udriver->resume,
					_usb_suspend,	udriver->suspend);
			spin_lock_irqsave(&pmdata->lock, flags);
			if (_usb_resume) {
				udriver->resume = _usb_resume;
				_usb_resume = NULL;
			}
			if (_usb_suspend) {
				udriver->suspend = _usb_suspend;
				_usb_suspend = NULL;
			}
			spin_unlock_irqrestore(&pmdata->lock, flags);
			/* set roothub runtime autosuspend delay to default */
			pm_runtime_set_autosuspend_delay
					(&udev->bus->root_hub->dev, 2000);
			pmdata->hdev = NULL;
			pmdata->udev = NULL;
			break;
		case MIF_BOOT_DEVICE:
			pmdata->link_connected = false;
			mif_info("boot dev disconnected\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

/* For checking to L3 stats(kernel suspend) */
static int xmm626x_linkpm_pm_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct xmm626x_linkpm_data *pmdata =
			container_of(nfb, struct xmm626x_linkpm_data, pm_nfb);

	if (!pmdata || !pmdata->link_connected) {
		mif_info("HSIC not connected, skip\n");
		return NOTIFY_DONE;
	}

	switch (event) {
	case PM_SUSPEND_PREPARE:
		pmdata->dpm_suspending = true;
		cancel_delayed_work_sync(&pmdata->link_pm_work);
		break;
	case PM_POST_SUSPEND:
		pmdata->dpm_suspending = false;
		if (get_hostwake(pmdata) || pmdata->resume_req) {
			wake_lock(&pmdata->l2_wake);
			mif_debug("get wakelock\n");
			queue_delayed_work(pmdata->wq, &pmdata->link_pm_work,
									0);
		}
		break;
	}
	return NOTIFY_DONE;
}

static long link_pm_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct xmm626x_linkpm_data *pmdata =
			(struct xmm626x_linkpm_data *)file->private_data;
	int value;

	mif_info("%x\n", cmd);

	switch (cmd) {
	case IOCTL_LINK_CONTROL_ENABLE:
		if (copy_from_user(&value, (const void __user *)arg,
							sizeof(int)))
			return -EFAULT;
		if (pmdata->pdata->link_ldo_enable)
			pmdata->pdata->link_ldo_enable(!!value);
		if (pmdata->pdata->gpio_link_enable)
			gpio_set_value(pmdata->pdata->gpio_link_enable, value);
		break;
	case IOCTL_LINK_CONTROL_ACTIVE:
		if (copy_from_user(&value, (const void __user *)arg,
							sizeof(int)))
			return -EFAULT;
		gpio_set_value(pmdata->pdata->gpio_link_active, value);
		break;
	case IOCTL_LINK_GET_HOSTWAKE:
		return !gpio_get_value(pmdata->pdata->gpio_link_hostwake);
	case IOCTL_LINK_CONNECTED:
		mif_info("link_connected(%x)\n", pmdata->link_connected);
		return !!pmdata->link_connected;
	default:
		break;
	}
	return 0;
}

static int link_pm_open(struct inode *inode, struct file *file)
{
	struct xmm626x_linkpm_data *pmdata =
			(struct xmm626x_linkpm_data *)file->private_data;
	/*file->private_data = pmdata->misc*/
	file->private_data = (void *)pmdata;
	return 0;
}

static int link_pm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations link_pm_fops = {
	.owner = THIS_MODULE,
	.open = link_pm_open,
	.release = link_pm_release,
	.unlocked_ioctl = link_pm_ioctl,
};

/* CP initiated L2->L0 */
static void link_pm_runtime_work(struct work_struct *work)
{
	int ret;
	struct xmm626x_linkpm_data *pmdata = container_of(work,
				struct xmm626x_linkpm_data, link_pm_work.work);
	struct device *dev = &pmdata->udev->dev;

	mif_debug("rpm_status(%d)\n", dev->power.runtime_status);
	switch (dev->power.runtime_status) {
	case RPM_SUSPENDED:
		ret = pm_runtime_resume(dev);
		if (ret < 0) {
			mif_err("resume error(%d)\n", ret);
			/* force to go runtime idle before retry resume */
			if (dev->power.timer_expires == 0 &&
						!dev->power.request_pending) {
				mif_debug("run time idle\n");
				pm_runtime_idle(dev);
			}
		}
		break;
	case RPM_SUSPENDING:
		/* Checking the usb_runtime_suspend running time.*/
		mif_info("rpm_states=%d", dev->power.runtime_status);
		msleep(20);
		break;
	default:
		break;
	}

	if (dev->power.runtime_status == RPM_ACTIVE) {	/*resume success*/
		pmdata->resume_cnt = 0;
	} else if (pmdata->resume_cnt++ > 30) { /*resume fail over 30 times*/
		mif_info("rpm_status(%d), retry_cnt(%d)\n",
			dev->power.runtime_status, pmdata->resume_cnt);
		usb_cp_crash(pmdata->udev, "Runtim Resume timeout");
		wake_unlock(&pmdata->l2_wake);
	} else {				/*wait for runtime resume done*/
		mif_info("rpm (%d), delayed work\n", dev->power.runtime_status);
		queue_delayed_work(pmdata->wq, &pmdata->link_pm_work,
							msecs_to_jiffies(50));
	}
}

/* link pm event delayed work - runtime start, reconnect, etc.*/
static void link_pm_event_work(struct work_struct *work)
{
	struct xmm626x_linkpm_data *pmdata = container_of(work,
				struct xmm626x_linkpm_data, link_pm_event.work);
	struct usb_device *udev = pmdata->udev;

	mif_info("0x%lx\n", pmdata->events);

	if (test_bit(LINKPM_EVENT_RUNTIME, &pmdata->events)) {
		struct device *dev, *hdev, *roothub;

		mif_info("LINKPM_EVENT_RUNTIME\n");
		dev = &udev->dev;
		roothub = &udev->bus->root_hub->dev;
		hdev = udev->bus->root_hub->dev.parent;
		pm_runtime_set_autosuspend_delay(dev, l2_delay);
		pm_runtime_set_autosuspend_delay(roothub, hub_delay);
		pm_runtime_allow(dev);
		pm_runtime_allow(hdev);/*ehci*/
		clear_bit(LINKPM_EVENT_RUNTIME, &pmdata->events);
	}
}

/* Host wakeup interrupt handler */
static irqreturn_t xmm626x_linkpm_hostwake(int irq, void *data)
{
	bool host_wake, slave_wake;
	struct xmm626x_linkpm_data *pmdata = data;

	if (!pmdata || !pmdata->link_connected)
		return IRQ_HANDLED;

	host_wake = get_hostwake(pmdata);
	slave_wake = !!gpio_get_value(pmdata->pdata->gpio_link_slavewake);
	printk(KERN_DEBUG "mif: host wakeup(%d), slave(%d)\n", host_wake,
								slave_wake);

	if (slave_wake && !host_wake) {
		gpio_set_value(pmdata->pdata->gpio_link_slavewake, 0);
		printk(KERN_DEBUG "mif: slave(%d)\n",
			gpio_get_value(pmdata->pdata->gpio_link_slavewake));
	}

	/* CP request the resume*/
	if (pmdata->dpm_suspending) {
		mif_info("dpm_suspending, will be resume..\n");
		return IRQ_HANDLED;
	}
	if (!slave_wake && host_wake)
		queue_delayed_work(pmdata->wq, &pmdata->link_pm_work, 0);

	return IRQ_HANDLED;
}

static int xmm626x_linkpm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct xmm626x_linkpm_data *pmdata;
	struct modemlink_pm_data *pdata =
			(struct modemlink_pm_data *)pdev->dev.platform_data;
	int irq;

	pmdata = (struct xmm626x_linkpm_data *)
			kzalloc(sizeof(struct xmm626x_linkpm_data), GFP_KERNEL);
	if (!pmdata) {
		mif_err("pmdata alloc fail\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, pmdata);
	pmdata->pdata = pdata;
	pmdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	pmdata->miscdev.name = "link_pm";
	pmdata->miscdev.fops = &link_pm_fops;
	ret = misc_register(&pmdata->miscdev);
	if (ret < 0) {
		mif_err("fail to register pm device(%d)\n", ret);
		goto err_misc_register;
	}

	/* create work queue & init work for runtime pm */
	pmdata->wq = create_singlethread_workqueue("linkpmd");
	if (!pmdata->wq) {
		mif_err("fail to create wq\n");
		goto err_create_wq;
	}
	INIT_DELAYED_WORK(&pmdata->link_pm_work, link_pm_runtime_work);
	INIT_DELAYED_WORK(&pmdata->link_pm_event, link_pm_event_work);
	spin_lock_init(&pmdata->lock);
	spin_lock_bh(&xmm626x_devices.lock);
	list_add(&pmdata->link, &xmm626x_devices.pmdata);
	spin_unlock_bh(&xmm626x_devices.lock);


	irq = gpio_to_irq(pdata->gpio_link_hostwake);
	ret = request_irq(irq, xmm626x_linkpm_hostwake,
		IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"hostwake", (void *)pmdata);
	if (ret) {
		mif_err("fail to request irq(%d)\n", ret);
		goto err_request_irq;
	}
	disable_irq(irq);
	ret = enable_irq_wake(irq);
	if (ret) {
		mif_err("failed to enable_irq_wake:%d\n", ret);
		goto err_set_wake_irq;
	}

	pmdata->usb_nfb.notifier_call = xmm626x_linkpm_usb_notify;
	usb_register_notify(&pmdata->usb_nfb);
	pmdata->pm_nfb.notifier_call = xmm626x_linkpm_pm_notify;
	register_pm_notifier(&pmdata->pm_nfb);

	wake_lock_init(&pmdata->l2_wake, WAKE_LOCK_SUSPEND, "l2_hsic");

	mif_info("success\n");
	return 0;

err_set_wake_irq:
	free_irq(irq, (void *)pmdata);
err_request_irq:
	destroy_workqueue(pmdata->wq);
err_create_wq:
	misc_deregister(&pmdata->miscdev);
err_misc_register:
	kfree(pmdata);
	return ret;
}

static int xmm626x_linkpm_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct xmm626x_linkpm_data *pmdata = dev_get_drvdata(&pdev->dev);

	if (!pmdata)
		return 0;

	usb_unregister_notify(&pmdata->usb_nfb);
	unregister_pm_notifier(&pmdata->pm_nfb);

	spin_lock_bh(&xmm626x_devices.lock);
	list_del(&pmdata->link);
	spin_unlock_bh(&xmm626x_devices.lock);

	kfree(pmdata);
	dev_set_drvdata(&pdev->dev, NULL);
	mif_info("success\n");

	return ret;
}

static int xmm626x_linkpm_resume_noirq(struct device *dev)
{
	struct xmm626x_linkpm_data *pmdata = dev_get_drvdata(dev);

	if (!pmdata)
		return 0;

	mif_debug("resume\n");
	/*disable_irq_wake(gpio_to_irq(pmdata->pdata->gpio_link_hostwake));*/
	enable_irq(gpio_to_irq(pmdata->pdata->gpio_link_hostwake));
	mif_info("enable hostwakeup irq\n");
	return 0;
}

static int xmm626x_linkpm_suspend(struct device *dev)
{
	int ret = 0;
	struct xmm626x_linkpm_data *pmdata = dev_get_drvdata(dev);

	mif_debug("suspend\n");
	if (!pmdata || !pmdata->link_connected) {
		mif_info("HSIC not connected, skip\n");
		return 0;
	}
	pmdata->l3l0_req = true;
	if (get_hostwake(pmdata) || pmdata->resume_req) {
		mif_info("hostwakeup(%d) resume_req(%d), suspend fail\n",
			get_hostwake(pmdata), pmdata->resume_req);
		goto suspend_yet;
	}
	return ret;

suspend_yet:
	return -EBUSY;
}

/*
 * If CP send host wakeup until AP L3 to kernel suspend,
 * it will suspend_noirq fail and resume to L0
 */
static int xmm626x_linkpm_suspend_noirq(struct device *dev)
{
	struct xmm626x_linkpm_data *pmdata = dev_get_drvdata(dev);
	int hostwake;

	if (!pmdata || !pmdata->link_connected) {
		mif_debug("HSIC not connected, skip\n");
		return 0;
	}

	disable_irq(gpio_to_irq(pmdata->pdata->gpio_link_hostwake));
	hostwake = get_hostwake(pmdata);
	mif_debug("check host wakeup level(%d)\n", hostwake);
	if (hostwake || pmdata->resume_req) {
		mif_info("hostwakeup(%d) resume_req(%d), suspend fail\n",
			get_hostwake(pmdata), pmdata->resume_req);
		enable_irq(gpio_to_irq(pmdata->pdata->gpio_link_hostwake));
		return -EAGAIN;
	}
	/*enable_irq_wake(gpio_to_irq(pmdata->pdata->gpio_link_hostwake));*/
	return 0;
}

static const struct dev_pm_ops xmm626x_linkpm_pm_ops = {
	.suspend = xmm626x_linkpm_suspend,
	.resume_noirq = xmm626x_linkpm_resume_noirq,
	.suspend_noirq = xmm626x_linkpm_suspend_noirq,
};

static struct platform_driver xmm626x_linkpm_driver = {
	.probe = xmm626x_linkpm_probe,
	.remove = xmm626x_linkpm_remove,
	.driver = {
		.name = "linkpm-xmm626x",
		.owner = THIS_MODULE,
		.pm = &xmm626x_linkpm_pm_ops,
	},
};

static int __init xmm626x_linkpm_init(void)
{
	int ret;

	INIT_LIST_HEAD(&xmm626x_devices.pmdata);
	spin_lock_init(&xmm626x_devices.lock);
	ret = platform_driver_register(&xmm626x_linkpm_driver);
	if (ret)
		mif_err("xmm626x linkpm register fail: %d\n", ret);
	return ret;
}

static void __exit xmm626x_linkpm_exit(void)
{
	platform_driver_unregister(&xmm626x_linkpm_driver);
}

late_initcall(xmm626x_linkpm_init);
module_exit(xmm626x_linkpm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Interface Runtime PM Driver");
