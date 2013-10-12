#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sii8240.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#endif
#include <linux/power_supply.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include "board-universal5420.h"
#include "linux/power_supply.h"

/*MHL control I2C*/
/*#define USE_GPIO_I2C_MHL*/
#define I2C_BUS_ID_MHL	10

/*DDC I2C */
#define DDC_I2C 6

/*MHL LDO*/
#define MHL_LDO1_8 "vcc_1.8v_mhl"
#define MHL_LDO3_3 "vcc_3.3v_mhl"
#define MHL_LDO1_2 "vsil_1.2a"

/*Event of receiving*/
#define PSY_BAT_NAME "battery"
/*Event of sending*/
#define PSY_CHG_NAME "max77693-charger"

static bool mhl_power_on;

#define MHL_DEFAULT_SWING 0x27 /*0x2D*/


static void sii8240_cfg_gpio(void)
{
	pr_info("%s()\n", __func__);
#ifdef USE_GPIO_I2C_MHL
	/* AP_MHL_SDA */
	s3c_gpio_cfgpin(GPIO_MHL_SDA_18V, S3C_GPIO_SFN(0x0));
	s3c_gpio_setpull(GPIO_MHL_SDA_18V, S3C_GPIO_PULL_NONE);

	/* AP_MHL_SCL */
	s3c_gpio_cfgpin(GPIO_MHL_SCL_18V, S3C_GPIO_SFN(0x0));
	s3c_gpio_setpull(GPIO_MHL_SCL_18V, S3C_GPIO_PULL_NONE);

	s5p_gpio_set_drvstr(GPIO_MHL_SCL_18V, S5P_GPIO_DRVSTR_LV4);
	s5p_gpio_set_drvstr(GPIO_MHL_SDA_18V, S5P_GPIO_DRVSTR_LV4);
#endif
	gpio_request(GPIO_MHL_INT, "MHL_INT");
	s5p_register_gpio_interrupt(GPIO_MHL_INT);
	s3c_gpio_setpull(GPIO_MHL_INT, S3C_GPIO_PULL_DOWN);
	irq_set_irq_type(gpio_to_irq(GPIO_MHL_INT), IRQ_TYPE_EDGE_RISING);
	s3c_gpio_cfgpin(GPIO_MHL_INT, S3C_GPIO_SFN(0xF));

	s3c_gpio_cfgpin(GPIO_MHL_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MHL_RST, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_MHL_RST, 0);
}

static void sii8240_power_onoff(bool on)
{
	struct regulator *regulator1_8, *regulator3_3, *regulator1_2;

	regulator1_8 = regulator_get(NULL, MHL_LDO1_8);
	regulator3_3 = regulator_get(NULL, MHL_LDO3_3);
	regulator1_2 = regulator_get(NULL, MHL_LDO1_2);
	if ((IS_ERR(regulator1_8)) || (IS_ERR(regulator3_3))
		|| (IS_ERR(regulator1_2))) {
		pr_err("%s : regulato is not available", __func__);
		return;
	}

	if (mhl_power_on == on) {
		pr_info("sii8240 : sii8240_power_onoff : alread %d\n", on);
		regulator_put(regulator1_2);
		regulator_put(regulator3_3);
		regulator_put(regulator1_8);
		return;
	}

	mhl_power_on = on;
	pr_info("sii8240 : sii8240_power_onoff : %d\n", on);

	if (on) {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		s3c_gpio_setpull(GPIO_HDMI_HPD, S3C_GPIO_PULL_NONE);
		regulator_enable(regulator3_3);
		regulator_enable(regulator1_8);
		regulator_enable(regulator1_2);
		usleep_range(10000, 20000);
	} else {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		s3c_gpio_setpull(GPIO_HDMI_HPD, S3C_GPIO_PULL_DOWN);
		regulator_disable(regulator3_3);
		regulator_disable(regulator1_8);
		regulator_disable(regulator1_2);

		gpio_set_value(GPIO_MHL_RST, 0);
	}
	regulator_put(regulator3_3);
	regulator_put(regulator1_8);
	regulator_put(regulator1_2);

}

static void sii8240_reset(void)
{
	pr_info("%s()\n", __func__);

	s3c_gpio_cfgpin(GPIO_MHL_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MHL_RST, S3C_GPIO_PULL_NONE);

	gpio_set_value(GPIO_MHL_RST, 0);
	usleep_range(10000, 20000);
	gpio_set_value(GPIO_MHL_RST, 1);
	usleep_range(10000, 20000);
}

#ifdef __MHL_NEW_CBUS_MSC_CMD__
static void sii9234_vbus_present(bool on, int value)
{
	struct power_supply *psy = power_supply_get_by_name(PSY_CHG_NAME);
	union power_supply_propval power_value;
	u8 intval;
	pr_info("%s: on(%d), vbus type(%d)\n", __func__, on, value);

	if (!psy) {
		pr_err("%s: fail to get %s psy\n", __func__, PSY_CHG_NAME);
		return;
	}

	power_value.intval = ((POWER_SUPPLY_TYPE_MISC << 4) |
			(on << 2) | (value << 0));

	pr_info("%s: value.intval(0x%x)\n", __func__, power_value.intval);
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &power_value);

	return;
}
#endif

#ifdef CONFIG_SAMSUNG_MHL_UNPOWERED
static int sii9234_get_vbus_status(void)
{
	struct power_supply *psy = power_supply_get_by_name(PSY_BAT_NAME);
	union power_supply_propval value;
	u8 intval;

	if (!psy) {
		pr_err("%s: fail to get %s psy\n", __func__, PSY_BAT_NAME);
		return -1;
	}

	psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	pr_info("%s: value.intval(0x%x)\n", __func__, value.intval);

	return (int)value.intval;
}

static void sii9234_otg_control(bool onoff)
{
	otg_control(onoff);

	gpio_request(GPIO_OTG_EN, "USB_OTG_EN");
	gpio_direction_output(GPIO_OTG_EN, onoff);
	gpio_free(GPIO_OTG_EN);

	pr_info("[MHL] %s: onoff =%d\n", __func__, onoff);

	return;
}
#endif
#ifndef CONFIG_MACH_V1
static void muic_mhl_cb(bool otg_enable, int plim)
{
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	int current_cable_type = POWER_SUPPLY_TYPE_MISC;
	int sub_type = ONLINE_SUB_TYPE_MHL;
	int power_type = ONLINE_POWER_TYPE_UNKNOWN;
	int muic_cable_type = max77803_muic_get_charging_type();

	pr_info("%s: muic cable_type = %d\n",
		__func__, muic_cable_type);
	switch (muic_cable_type) {
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		return;
	default:
		break;
	}
	pr_info("muic_mhl_cb:otg_enable=%d, plim=%d\n", otg_enable, plim);

	if (plim == 0x00) {
		pr_info("TA charger 500mA\n");
		power_type = ONLINE_POWER_TYPE_MHL_500;
	} else if (plim == 0x01) {
		pr_info("TA charger 900mA\n");
		power_type = ONLINE_POWER_TYPE_MHL_900;
	} else if (plim == 0x02) {
		pr_info("TA charger 1500mA\n");
		power_type = ONLINE_POWER_TYPE_MHL_1500;
	} else if (plim == 0x03) {
		pr_info("USB charger\n");
		power_type = ONLINE_POWER_TYPE_USB;
	} else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

	if (otg_enable) {
		psy_do_property("sec-charger", get,
					POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("sec-charger : %d\n", value.intval);
		if (value.intval == POWER_SUPPLY_TYPE_BATTERY ||
				value.intval == POWER_SUPPLY_TYPE_WPC) {
			if (!lpcharge) {
				otg_control(true);
				current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
				power_type = ONLINE_POWER_TYPE_UNKNOWN;
			}
		}
	} else
		otg_control(false);

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == 10) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT
		| sub_type<<ONLINE_TYPE_SUB_SHIFT
		| power_type<<ONLINE_TYPE_PWR_SHIFT;
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE,
		&value);
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	}
}
#endif
static BLOCKING_NOTIFIER_HEAD(acc_notifier);

int acc_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&acc_notifier, nb);
}

int acc_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&acc_notifier, nb);
}

void acc_notify(int event)
{
	blocking_notifier_call_chain(&acc_notifier, event, NULL);
}

static struct sii8240_platform_data sii8240_pdata = {
	.init = sii8240_cfg_gpio,
	.mhl_sel = NULL,
	.hw_onoff = sii8240_power_onoff,
	.hw_reset = sii8240_reset,
	.enable_vbus = NULL,
#if defined(__MHL_NEW_CBUS_MSC_CMD__)
	.vbus_present = sii9234_vbus_present,
#else
	.vbus_present = NULL,
#endif
#ifdef CONFIG_SAMSUNG_MHL_UNPOWERED
	.get_vbus_status = sii9234_get_vbus_status,
	.sii9234_otg_control = sii9234_otg_control,
#endif
#ifdef CONFIG_MACH_V1
	.sii8240_muic_cb = NULL,
#else
	.sii8240_muic_cb = muic_mhl_cb,
#endif
	.reg_notifier   = acc_register_notifier,
	.unreg_notifier = acc_unregister_notifier,

#ifdef CONFIG_EXTCON
	.extcon_name = "max77693-muic",
#endif
	.swing_level = MHL_DEFAULT_SWING,
};

static struct i2c_board_info __initdata i2c_devs_sii8240[] = {
	{
		I2C_BOARD_INFO("sii8240_tmds", 0x72>>1),
		.platform_data = &sii8240_pdata,
	},
	{
		I2C_BOARD_INFO("sii8240_hdmi", 0x92>>1),
		.platform_data = &sii8240_pdata,
	},
	{
		I2C_BOARD_INFO("sii8240_disc", 0x9A>>1),
		.platform_data = &sii8240_pdata,
	},
	{
		I2C_BOARD_INFO("sii8240_tpi", 0xB2>>1),
		.platform_data = &sii8240_pdata,
	},
	{
		I2C_BOARD_INFO("sii8240_cbus", 0xC8>>1),
		.platform_data = &sii8240_pdata,
	},
};

#ifdef USE_GPIO_I2C_MHL
static struct i2c_gpio_platform_data i2c15_platdata = {
	.sda_pin = GPIO_MHL_SDA_18V,
	.scl_pin = GPIO_MHL_SCL_18V,
	.udelay = 5,		/* default is 2 and 250KHz */
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c15 = {
	.name = "i2c-gpio",
	.id = 15,
	.dev.platform_data = &i2c15_platdata,
};

/*SW I2C: sys/bus/i2c/device/i2c.15*/
static struct platform_device *universal5410_mhl_device[] __initdata = {
	&s3c_device_i2c15,
};
#else

/*HW I2C: hs_i2c6 => sys/bus/i2c/device/i2c.10*/
struct exynos5_platform_i2c hs_i2c6_data __initdata = {
	.bus_number = I2C_BUS_ID_MHL,
        .speed_mode = HSI2C_FAST_SPD,
        .fast_speed = 100000,
        .high_speed = 0,
        .cfg_gpio = NULL,
};
static struct platform_device *universal5410_mhl_device[] __initdata = {
	&exynos5_device_hs_i2c6,
};
#endif
int __init exynos5_setup_mhl_i2cport(void)
{
#ifdef USE_GPIO_I2C_MHL
	universal5410_mhl_device[0] = &s3c_device_i2c15;
#else
	exynos5_hs_i2c6_set_platdata(&hs_i2c6_data);
	universal5410_mhl_device[0] = &exynos5_device_hs_i2c6;
#endif
	return I2C_BUS_ID_MHL;
}

void __init exynos5_universal5410_mhl_init(void)
{
	int mhl_i2c_num;

	/*Setting of i2c bus depend on h/w layout*/
	mhl_i2c_num = exynos5_setup_mhl_i2cport();
	platform_add_devices(universal5410_mhl_device,
			ARRAY_SIZE(universal5410_mhl_device));

	/*Register i2c specific device on i2c.num bus*/
	sii8240_pdata.ddc_i2c_num = DDC_I2C;
	i2c_register_board_info(mhl_i2c_num, i2c_devs_sii8240,
		ARRAY_SIZE(i2c_devs_sii8240));

	sii8240_cfg_gpio();
	pr_info("[sii8240] %s - done\n", __func__);
}
