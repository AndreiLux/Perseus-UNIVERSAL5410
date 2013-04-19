#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
//#include <plat/udc-hs.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max8649.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/gpio.h>


#include <linux/power_supply.h>

#include <linux/notifier.h>
#include <linux/sii8240.h>
#include "board-universal5410.h"

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#ifdef CONFIG_JACK_MON
#include <linux/jack.h>
#endif

#ifdef CONFIG_MACH_SLP_NAPLES
#include <mach/naples-tsp.h>
#endif
#include <mach/usb3-drd.h>

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};
#endif
#ifdef CONFIG_MACH_JA
#include <linux/i2c/touchkey_i2c.h>
#endif

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

/* charger cable state */
bool is_cable_attached;
bool is_jig_attached;

#ifdef CONFIG_MFD_MAX77693
extern int g_usbvbus;
#endif
static ssize_t switch_show_vbus(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int i;
	struct regulator *regulator;

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	if (regulator_is_enabled(regulator))
		i = sprintf(buf, "VBUS is enabled\n");
	else
		i = sprintf(buf, "VBUS is disabled\n");
	regulator_put(regulator);

	return i;
}

static ssize_t switch_store_vbus(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int disable, ret, usb_mode;
	struct regulator *regulator;
	/* struct s3c_udc *udc = platform_get_drvdata(&s3c_device_usbgadget); */

	if (!strncmp(buf, "0", 1))
		disable = 0;
	else if (!strncmp(buf, "1", 1))
		disable = 1;
	else {
		pr_warn("%s: Wrong command\n", __func__);
		return count;
	}

	pr_info("%s: disable=%d\n", __func__, disable);
	usb_mode =
	    disable ? USB_CABLE_DETACHED_WITHOUT_NOTI : USB_CABLE_ATTACHED;
	/* ret = udc->change_usb_mode(usb_mode); */
	ret = -1;
	if (ret < 0)
		pr_err("%s: fail to change mode!!!\n", __func__);

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return count;
	}

	if (disable) {
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	} else {
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	}
	regulator_put(regulator);

	return count;
}

DEVICE_ATTR(disable_vbus, 0664, switch_show_vbus,
	    switch_store_vbus);

static int __init sec_switch_init(void)
{
	int ret = 0;
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev)) {
		pr_err("%s:%s= Failed to create device(switch)!\n",
				__FILE__, __func__);
		return -ENODEV;
	}

	ret = device_create_file(switch_dev, &dev_attr_disable_vbus);
	if (ret)
		pr_err("%s:%s= Failed to create device file(disable_vbus)!\n",
				__FILE__, __func__);

	return ret;
};

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
#ifdef CONFIG_MFD_MAX77693
int max77693_muic_charger_cb(enum cable_type_muic cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	static enum cable_type_muic previous_cable_type = CABLE_TYPE_NONE_MUIC;

	pr_info("[BATT] CB enabled %d\n", cable_type);

	/* others setting */
	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
	case CABLE_TYPE_OTG_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
	case CABLE_TYPE_MHL_MUIC:
		is_cable_attached = false;
		break;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_MHL_VB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_TA_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
	case CABLE_TYPE_AUDIODOCK_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
		is_cable_attached = true;
		break;
	default:
		pr_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}

#if defined(CONFIG_MACH_SLP_NAPLES) || defined(CONFIG_MACH_MIDAS) \
		|| defined(CONFIG_MACH_GC1) || defined(CONFIG_MACH_T0)
	tsp_charger_infom(is_cable_attached);
#endif
#if defined(CONFIG_MACH_JA)
	touchkey_charger_infom(is_cable_attached);
#endif

	/*  charger setting */
	if (previous_cable_type == cable_type) {
		pr_info("%s: SKIP cable setting\n", __func__);
		goto skip;
	}
	previous_cable_type = cable_type;

	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE_MHL_VB_MUIC:
		if (lpcharge)
			current_cable_type = POWER_SUPPLY_TYPE_USB;
		else
			goto skip;
		break;
	case CABLE_TYPE_MHL_MUIC:
		if (lpcharge) {
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		} else {
			goto skip;
		}
		break;
	case CABLE_TYPE_OTG_MUIC:
		goto skip;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_AUDIODOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		goto skip;
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

skip:
#ifdef CONFIG_JACK_MON
	jack_event_handler("charger", is_cable_attached);
#endif

	return 0;
}

int max77693_get_jig_state(void)
{
	return is_jig_attached;
}
EXPORT_SYMBOL(max77693_get_jig_state);

void max77693_set_jig_state(int jig_state)
{
	is_jig_attached = jig_state;
}

static void max77693_check_id_state(int state)
{
	pr_info("%s: id state = %d\n", __func__, state);
#if defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH0)
	exynos_drd_switch_id_event(&exynos5_device_usb3_drd0, state);
#else
	exynos_drd_switch_id_event(&exynos5_device_usb3_drd1, state);
#endif
}

static void max77693_set_vbus_state(int state)
{
	pr_info("%s: vbus state = %d\n", __func__, state);
#if defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH0)
	exynos_drd_switch_vbus_event(&exynos5_device_usb3_drd0, state);
#else
	exynos_drd_switch_vbus_event(&exynos5_device_usb3_drd1, state);
#endif
}

/* usb cable call back function */
void max77693_muic_usb_cb(u8 usb_mode)
{
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;
#endif
	if (usb_mode == USB_CABLE_ATTACHED) {
#ifdef CONFIG_MFD_MAX77693
		g_usbvbus = USB_CABLE_ATTACHED;
#endif
		max77693_set_vbus_state(USB_CABLE_ATTACHED);
		pr_info("%s - USB_CABLE_ATTACHED\n", __func__);
	} else if (usb_mode == USB_CABLE_DETACHED) {
#ifdef CONFIG_MFD_MAX77693
		g_usbvbus = USB_CABLE_DETACHED;
#endif	
		max77693_set_vbus_state(USB_CABLE_DETACHED);
		pr_info("%s - USB_CABLE_DETACHED\n", __func__);
	} else if (usb_mode == USB_OTGHOST_ATTACHED) {
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->booster(1);
		host_noti_pdata->ndev.mode = NOTIFY_HOST_MODE;
		if (host_noti_pdata->usbhostd_start)
			host_noti_pdata->usbhostd_start();
#endif

#if defined(CONFIG_MACH_J_CHN_CTC)
		/* defense code for otg mis-detecing issue */
		msleep(40);
#endif

		max77693_check_id_state(0);
		pr_info("%s - USB_OTGHOST_ATTACHED\n", __func__);
	} else if (usb_mode == USB_OTGHOST_DETACHED) {
		max77693_check_id_state(1);
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->ndev.mode = NOTIFY_NONE_MODE;
		if (host_noti_pdata->usbhostd_stop)
			host_noti_pdata->usbhostd_stop();
		host_noti_pdata->booster(0);
#endif
		pr_info("%s - USB_OTGHOST_DETACHED\n", __func__);
	} else if (usb_mode == USB_POWERED_HOST_ATTACHED) {
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->powered_booster(1);
		start_usbhostd_wakelock();
#endif
		max77693_check_id_state(0);
		pr_info("%s - USB_POWERED_HOST_ATTACHED\n", __func__);
	} else if (usb_mode == USB_POWERED_HOST_DETACHED) {
		max77693_check_id_state(1);
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->powered_booster(0);
		stop_usbhostd_wakelock();
#endif
		pr_info("%s - USB_POWERED_HOST_DETACHED\n", __func__);
	}
}

#if defined(CONFIG_MUIC_MAX77693_SUPPORT_MHL_CABLE_DETECTION)
void max77693_muic_mhl_cb(int attached)
{
	if (attached == MAX77693_MUIC_ATTACHED) {
		pr_info("MHL Attached !!\n");
		acc_notify(MHL_ATTACHED);
	} else {
		pr_info("MHL Detached !!\n");
		acc_notify(MHL_DETTACHED);
	}
}
#endif

#if defined(CONFIG_MUIC_MAX77693_SUPPORT_MHL_CABLE_DETECTION)
bool max77693_muic_is_mhl_attached(void)
{
	return true;
}
#endif

void max77693_muic_dock_cb(int type)
{
#ifdef CONFIG_JACK_MON
	jack_event_handler("cradle", type);
#endif
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

void max77693_muic_init_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
#endif
}

int max77693_muic_cfg_uart_gpio(void)
{
	return UART_PATH_AP;
}

void max77693_muic_jig_uart_cb(int path)
{
	switch (path) {
	case UART_PATH_AP:
		break;
	case UART_PATH_CP:
		break;
#ifdef CONFIG_LTE_VIA_SWITCH
	case UART_PATH_LTE:
		break;
#endif
	default:
		pr_info("func %s: invalid value!!\n", __func__);
	}

}

#if defined(CONFIG_MUIC_DET_JACK)
void max77693_muic_earjack_cb(int attached)
{

}
void max77693_muic_earjackkey_cb(int pressed, unsigned int code)
{

}
#endif

#ifdef CONFIG_USB_HOST_NOTIFY
int max77693_muic_host_notify_cb(int enable)
{
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;

	struct host_notify_dev *ndev = &host_noti_pdata->ndev;

	if (!ndev) {
		pr_err("%s: ndev is null.\n", __func__);
		return -1;
	}

	ndev->booster = enable ? NOTIFY_POWER_ON : NOTIFY_POWER_OFF;
	return ndev->mode;
}
#endif

int max77693_muic_set_safeout(int path)
{
	struct regulator *regulator;

	if (path == CP_USB_MODE) {
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		/* AP_USB_MODE || AUDIO_MODE */
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}

struct max77693_muic_data max77693_muic = {
	.usb_cb = max77693_muic_usb_cb,
	.charger_cb = max77693_muic_charger_cb,
#if defined(CONFIG_MUIC_MAX77693_SUPPORT_MHL_CABLE_DETECTION)
	.mhl_cb = max77693_muic_mhl_cb,
	.is_mhl_attached = max77693_muic_is_mhl_attached,
#endif
	.set_safeout = max77693_muic_set_safeout,
	.init_cb = max77693_muic_init_cb,
	.dock_cb = max77693_muic_dock_cb,
	.cfg_uart_gpio = max77693_muic_cfg_uart_gpio,
	.jig_uart_cb = max77693_muic_jig_uart_cb,
#if defined(CONFIG_MUIC_DET_JACK)
	.earjack_cb = max77693_muic_earjack_cb,
	.earjackkey_cb = max77693_muic_earjackkey_cb,
#endif
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify_cb = max77693_muic_host_notify_cb,
#else
	.host_notify_cb = NULL,
#endif
	/* .gpio_usb_sel = GPIO_USB_SEL, */
	.gpio_usb_sel = -1,
	.jig_state = max77693_set_jig_state,
	.check_id_state = max77693_check_id_state,
};
#endif

device_initcall(sec_switch_init);
