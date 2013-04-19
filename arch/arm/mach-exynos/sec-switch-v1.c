#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/gpio.h>
#include <linux/stat.h>
#include <linux/power_supply.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>

//#include "sec-common.h"
//#include "board-roma.h"

extern struct class *sec_class;
struct device *sec_switch_dev;

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

static unsigned int usb_sel = 2;
static unsigned int uart_sel = 3;

/* charger cable state */
bool is_cable_attached;

static bool usb_connected;

static int if_muic_info;
static int switch_sel;
static int if_pmic_rev;

/* func : get_if_pmic_inifo
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 8 bits (xxxxzzzz)
 * first 4bits(zzzz) mean path infomation.
 * next 4bits(xxxx) mean if pmic version info
 */
static int get_if_pmic_inifo(char *str)
{
	get_option(&str, &if_muic_info);
	switch_sel = if_muic_info & 0x0f;
	if_pmic_rev = (if_muic_info & 0xf0) >> 4;
	pr_info("%s %s: switch_sel: %x if_pmic_rev:%x\n",
		__FILE__, __func__, switch_sel, if_pmic_rev);
	return if_muic_info;
}
__setup("pmic_info=", get_if_pmic_inifo);

int get_switch_sel_v1(void)
{
	return switch_sel;
}

static ssize_t show_usb_sel(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	const char *mode;

	if (usb_sel == 2) {
		/* AP */
		mode = "PDA";
	} else if (usb_sel == 1) {
		/* CP */
		mode = "MODEM";
	} else if (usb_sel == 0) {
		/* ADC */
		mode = "ADC";
	} else {
		mode = "NA";
	}

	pr_info("%s: %s\n", __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t store_usb_sel(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	pr_info("%s: %s\n", __func__, buf);

	if (!strncasecmp(buf, "PDA", 3)) {
		/* USB_SEL1 : 1 for AP, 0 for CP,ADC */
		gpio_set_value(GPIO_USB_SEL0, 1);
		gpio_set_value(GPIO_USB_SEL1, 1);		
		usb_sel = 2;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		/* USB_SEL2 : 1 for CP, 0 for ADC */
		gpio_set_value(GPIO_USB_SEL0, 0);
		gpio_set_value(GPIO_USB_SEL1, 1);
		usb_sel = 1;
	} else if (!strncasecmp(buf, "ADC", 3)) {
		/* USB_SEL2 : 1 for CP, 0 for ADC */
		gpio_set_value(GPIO_USB_SEL0, 1);
		gpio_set_value(GPIO_USB_SEL1, 0);
		usb_sel = 0;
	} else {
		pr_err("%s: wrong usb_sel value(%s)!!\n", __func__, buf);
		return -EINVAL;
	}

	return count;
}

static ssize_t show_uart_sel(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	const char *mode;


	if (uart_sel == 3) {
		/* AP */
		mode = "AP";
	} else if (uart_sel == 1) {
		/* CP */
		mode = "CP";
	} else if (uart_sel == 2) {
		/* DOCK */
		mode = "DOCK";
	} else {
		/* NA */
		mode = "NA";
	}

	pr_info("%s: %s\n", __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t store_uart_sel(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	pr_info("%s: %s\n", __func__, buf);

	if (!strncasecmp(buf, "AP", 2)) {
		gpio_set_value(GPIO_UART_SEL1, 1);
		gpio_set_value(GPIO_UART_SEL2, 1);
		uart_sel = 3;
	} else if (!strncasecmp(buf, "CP", 2)) {
		gpio_set_value(GPIO_UART_SEL1, 0);
		gpio_set_value(GPIO_UART_SEL2, 1);
		uart_sel = 1;
	} else if (!strncasecmp(buf, "DOCK", 4)) {
		gpio_set_value(GPIO_UART_SEL1, 1);
		gpio_set_value(GPIO_UART_SEL2, 0);
		uart_sel = 2;
	} else {
		pr_err("%s: wrong uart_sel value(%s)!!\n", __func__, buf);
		return -EINVAL;
	}

	return count;
}

static ssize_t show_usb_state(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	const char *state;

	if (usb_connected)
		state = "USB_STATE_CONFIGURED";
	else
		state = "USB_STATE_NOTCONFIGURED";

	pr_info("%s: %s\n", __func__, state);

	return sprintf(buf, "%s\n", state);
}

static DEVICE_ATTR(usb_sel, 0664, show_usb_sel, store_usb_sel);
static DEVICE_ATTR(uart_sel, 0664, show_uart_sel, store_uart_sel);
static DEVICE_ATTR(usb_state, S_IRUGO, show_usb_state, NULL);

static struct attribute *sec_switch_attributes[] = {
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_state.attr,
	NULL
};

static const struct attribute_group sec_switch_group = {
	.attrs = sec_switch_attributes,
};

static bool ap_uart_sel;
int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

module_param_named(ap, ap_uart_sel, bool, S_IRUGO | S_IWUSR);

static int __init sec_switch_init(void)
{
	int ret;

	int switch_sel = get_switch_sel_v1();
	int uart_path = switch_sel & 0x02;
	int usb_path = switch_sel & 0x01;

	gpio_request(GPIO_UART_SEL1, "GPIO_UART_SEL1");
	gpio_export(GPIO_UART_SEL1, 1);

	gpio_request(GPIO_UART_SEL2, "GPIO_UART_SEL2");
	gpio_export(GPIO_UART_SEL2, 1);

	gpio_request(GPIO_USB_SEL0, "GPIO_USB_SEL0");
	gpio_export(GPIO_USB_SEL0, 1);
	gpio_request(GPIO_USB_SEL1, "GPIO_USB_SEL1");
	gpio_export(GPIO_USB_SEL1, 1);


	BUG_ON(!sec_class);
	sec_switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	BUG_ON(!sec_switch_dev);
	gpio_export_link(sec_switch_dev, "GPIO_UART_SEL", GPIO_UART_SEL1);
	gpio_export_link(sec_switch_dev, "GPIO_USB_SEL", GPIO_USB_SEL0);

	/* create sysfs group */
	ret = sysfs_create_group(&sec_switch_dev->kobj, &sec_switch_group);
	if (ret) {
		pr_err("failed to create px switch attribute group\n");
		return ret;
	}


	gpio_set_value(GPIO_UART_SEL1, uart_path);
	gpio_set_value(GPIO_UART_SEL2, 1);
	uart_sel = uart_path | 1;

	if (usb_path == 1) { /* init usb_sel for PDA */
		gpio_set_value(GPIO_USB_SEL0, 1);
		gpio_set_value(GPIO_USB_SEL1, 1);		
		usb_sel = 2;
		pr_info("%s : set usb path to PDA\n", __func__);
	}
	else { /* init usb_sel for MODEM */
		gpio_set_value(GPIO_USB_SEL0, 0);
		gpio_set_value(GPIO_USB_SEL1, 1);
		usb_sel = 1;
		pr_info("%s : set usb path to MODEM\n", __func__);
	}


	is_cable_attached = false;

	return 0;
}


 device_initcall(sec_switch_init);
