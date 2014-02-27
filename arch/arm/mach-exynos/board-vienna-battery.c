/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>

#include "board-universal5410.h"

#include "common.h"

#if defined(CONFIG_S3C_ADC)
#include <plat/adc.h>
#endif

#if defined(CONFIG_STMPE811_ADC)
#include <linux/platform_data/stmpe811-adc.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#if defined(CONFIG_CHARGER_MAX77693)
#include <linux/battery/charger/max77693_charger.h>
#endif

#define SEC_BATTERY_PMIC_NAME ""

static struct s3c_adc_client* adc_client;

bool is_wpc_cable_attached = false;

unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

#ifdef CONFIG_MFD_MAX77693
extern bool is_jig_attached;
#endif

static sec_charging_current_t charging_current_table[] = {
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{2000,	2000,	400,	0},
	{460,	460,	400,	0},
	{460,	460,	400,	0},
	{460,	460,	400,	0},
	{460,	460,	400,	0},
	{2000,	2000,	400,	0},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{2000,	2000,	400,	0},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
};

static bool sec_bat_adc_none_init(struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

#define SMTPE811_CHANNEL_ADC_CHECK_1	6
#define SMTPE811_CHANNEL_VICHG		0

static bool sec_bat_adc_ap_init(struct platform_device *pdev)
{
	adc_client = s3c_adc_register(pdev, NULL, NULL, 0);
	return true;
}
static bool sec_bat_adc_ap_exit(void) {return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
	int data = -1;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_TEMP:
		data = s3c_adc_read(adc_client, 4);
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	}

	return data;
}

static bool sec_bat_adc_ic_init(struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ic_exit(void) {return true; }
static int sec_bat_adc_ic_read(unsigned int channel)
{
	int data = 0;

	if (system_rev >= 0x03)
		return 0;

#if defined(CONFIG_STMPE811_ADC)/* No use from rev 0.3 ~*/
	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_CABLE_CHECK:
		data = stmpe811_read_adc_data(
			SMTPE811_CHANNEL_ADC_CHECK_1);
		break;
	case SEC_BAT_ADC_CHANNEL_FULL_CHECK:
		data = stmpe811_read_adc_data(
			SMTPE811_CHANNEL_VICHG);
		break;
	}
#endif

	return data;
}

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_FUEL_ALERT, S3C_GPIO_INPUT);
	return true;
}

static bool sec_chg_gpio_init(void)
{
	/* For wireless charging */
	/*s3c_gpio_cfgpin(GPIO_WPC_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_WPC_INT, S3C_GPIO_PULL_NONE);*/
	return true;
}

static int sec_bat_is_lpm_check(char *str)
{
	get_option(&str, &lpcharge);
	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("lpcharge=", sec_bat_is_lpm_check);

static bool sec_bat_is_lpm(void)
{
	return lpcharge;
}

int extended_cable_type;

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;
	int ta_int = 0;

	if (system_rev < 0x03) {
		if (system_rev == 0x2) /* charger ic changed */
			ta_int = !gpio_get_value_cansleep(GPIO_TA_INT);
		else
			ta_int = gpio_get_value_cansleep(GPIO_TA_INT);

		pr_info("%s: system_rev = %d, ta_int = %d\n",
			__func__, system_rev, ta_int);

		/* check cable by sec_bat_get_cable_type()
		 * for initial check
		 */
		value.intval = -1;

		if (extended_cable_type || ta_int)
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	} else {
			if (POWER_SUPPLY_TYPE_BATTERY < current_cable_type) {
				value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
			} else {
				psy_do_property("sec-charger", get,
						POWER_SUPPLY_PROP_ONLINE, value);
				if (value.intval == POWER_SUPPLY_TYPE_WPC) {
					value.intval =
						POWER_SUPPLY_TYPE_WPC<<ONLINE_TYPE_MAIN_SHIFT;
					psy_do_property("battery", set,
							POWER_SUPPLY_PROP_ONLINE, value);
				}
			}
	}
}

static bool sec_bat_check_jig_status(void)
{
#if defined(CONFIG_MFD_MAX77693)
	if (is_jig_attached || gpio_get_value_cansleep(GPIO_JIG_ON_18))
		return true;
	else
		return false;
#else/*system_rev < 0x03*/
	if (gpio_get_value_cansleep(GPIO_JIG_ON_18))/*IF_CON_SENSE*/
		return true;
	else
		return false;
#endif
}

static bool sec_bat_switch_to_check(void)
{
	int i;
	pr_debug("%s\n", __func__);

	if (system_rev < 0x03) {
		if (!gpio_get_value_cansleep(GPIO_ACCESSORY_INT)) {
			for (i = 0; i < 5; i++)
				if (POWER_SUPPLY_TYPE_CARDOCK ==
					GET_MAIN_CABLE_TYPE(extended_cable_type))
					/* universal keyboard dock connected */
					return false;
				else
					msleep(200);
		}

		s3c_gpio_cfgpin(GPIO_USB_SEL0, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_USB_SEL0, S3C_GPIO_PULL_NONE);

		s3c_gpio_cfgpin(GPIO_USB_SEL1, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_USB_SEL1, S3C_GPIO_PULL_NONE);

		gpio_set_value(GPIO_USB_SEL0, 1);
		gpio_set_value(GPIO_USB_SEL1, 0);

		mdelay(300);
	}

	return true;
}

static bool sec_bat_switch_to_normal(void)
{
	pr_debug("%s\n", __func__);

	if (system_rev < 0x03) {
		s3c_gpio_cfgpin(GPIO_USB_SEL1, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_USB_SEL1, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_USB_SEL1, 1);
	}

	return true;
}

static bool sec_bat_is_interrupt_cable_check_possible(int extended_cable_type)
{
	return (GET_MAIN_CABLE_TYPE(extended_cable_type) ==
		POWER_SUPPLY_TYPE_CARDOCK) ? false : true;
}

static int sec_bat_check_cable_callback(void)
{
	return current_cable_type;
}

static int sec_bat_get_cable_from_extended_cable_type(
	int input_extended_cable_type)
{
	int cable_main, cable_sub, cable_power;
	int cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
	union power_supply_propval value;
	int charge_current_max = 0, charge_current = 0;

	cable_main = GET_MAIN_CABLE_TYPE(input_extended_cable_type);
	if (cable_main != POWER_SUPPLY_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_MAIN_MASK) |
			(cable_main << ONLINE_TYPE_MAIN_SHIFT);
	cable_sub = GET_SUB_CABLE_TYPE(input_extended_cable_type);
	if (cable_sub != ONLINE_SUB_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_SUB_MASK) |
			(cable_sub << ONLINE_TYPE_SUB_SHIFT);
	cable_power = GET_POWER_CABLE_TYPE(input_extended_cable_type);
	if (cable_power != ONLINE_POWER_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_PWR_MASK) |
			(cable_power << ONLINE_TYPE_PWR_SHIFT);

	switch (cable_main) {
	case POWER_SUPPLY_TYPE_CARDOCK:
		switch (cable_power) {
		case ONLINE_POWER_TYPE_BATTERY:
			cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case ONLINE_POWER_TYPE_TA:
			switch (cable_sub) {
			case ONLINE_SUB_TYPE_MHL:
				cable_type = POWER_SUPPLY_TYPE_USB;
				break;
			case ONLINE_SUB_TYPE_AUDIO:
			case ONLINE_SUB_TYPE_DESK:
			case ONLINE_SUB_TYPE_SMART_NOTG:
			case ONLINE_SUB_TYPE_KBD:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_CARDOCK;
				break;
			}
			break;
		case ONLINE_POWER_TYPE_USB:
			cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		default:
			cable_type = current_cable_type;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_MISC:
		switch (cable_sub) {
		case ONLINE_SUB_TYPE_MHL:
			switch (cable_power) {
			case ONLINE_POWER_TYPE_BATTERY:
				cable_type = POWER_SUPPLY_TYPE_BATTERY;
				break;
			case ONLINE_POWER_TYPE_TA:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				charge_current_max = 700;
				charge_current = 700;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			}
			break;
		default:
			cable_type = cable_main;
			break;
		}
		break;
	default:
		cable_type = cable_main;
		break;
	}

	if (charge_current_max == 0) {
		charge_current_max =
			charging_current_table[cable_type].input_current_limit;
		charge_current =
			charging_current_table[cable_type].
			fast_charging_current;
	}
	value.intval = charge_current_max;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);
	value.intval = charge_current;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
	return cable_type;
}

static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	static bool is_usb = false;
	current_cable_type = cable_type;

	pr_info("%s cable_type %d\n",__func__,cable_type);
	switch (cable_type) {
	case POWER_SUPPLY_TYPE_USB:
		if (system_rev < 0x03) {
			is_usb = true;
			exynos5_usb_set_vbus_state(USB_CABLE_ATTACHED);
		}
		pr_info("%s set vbus applied\n",
				__func__);
		break;
	case POWER_SUPPLY_TYPE_BATTERY:
		if (system_rev < 0x03) {
			if(is_usb == true)
			{
				is_usb = false;
				exynos5_usb_set_vbus_state(USB_CABLE_DETACHED);
			}
		}
		pr_info("%s set vbus cut\n",
			__func__);
		break;
	case POWER_SUPPLY_TYPE_MAINS:
		break;
	default:
		pr_err("%s cable type (%d)\n",
			__func__, cable_type);
		return false;
	}
	return true;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void)
{
	struct power_supply *psy;
	union power_supply_propval value;

	if (system_rev < 0x03)
		return true;

	psy = get_power_supply_by_name(("sec-charger"));
	if (!psy) {
		pr_err("%s: Fail to get psy (%s)\n",
			__func__, "sec_charger");
		value.intval = 1;
	} else {
		int ret;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &(value));
		if (ret < 0) {
			pr_err("%s: Fail to sec-charger get_property (%d=>%d)\n",
				__func__, POWER_SUPPLY_PROP_PRESENT, ret);
			value.intval = 1;
		}
	}

	return value.intval;
}
static bool sec_bat_check_result_callback(void) {return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) {return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }

static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

/* ADC region should be exclusive */
static sec_bat_adc_region_t cable_adc_value_table[] = {
	{0,	0},
	{0,	0},
	{0,	0},
	{700,	2200},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
};

static int polling_time_table[] = {
	10,	/* BASIC */
	30,	/* CHARGING */
	30,	/* DISCHARGING */
	30,	/* NOT_CHARGING */
	300,	/* SLEEP */
};

/* for MAX17050 */
static struct battery_data_t vienna_battery_data[] = {
	/* SDI battery data */
	{
		.Capacity = 0x5208,
		.low_battery_comp_voltage = 3600,
		.low_battery_table = {
			/* range, slope, offset */
			{-5000,	0,	0},	/* dummy for top limit */
			{-1250, 0,	3320},
			{-750, 97,	3451},
			{-100, 96,	3461},
			{0, 0,	3456},
		},
		.temp_adjust_table = {
			/* range, slope, offset */
			{47000, 122,	8950},
			{60000, 200,	51000},
			{100000, 0,	0},	/* dummy for top limit */
		},
		.type_str = "SDI 7000mAh",
	}
};

sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.jig_irq = 0,
	.jig_irq_attr =
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.check_jig_status = sec_bat_check_jig_status,
	.is_interrupt_cable_check_possible =
		sec_bat_is_interrupt_cable_check_possible,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
	.cable_switch_check = sec_bat_switch_to_check,
	.cable_switch_normal = sec_bat_switch_to_normal,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
		},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 6,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_IC,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_NONE,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_IC,	/* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)vienna_battery_data,
	.bat_gpio_ta_nconnected = GPIO_TA_INT,
	.bat_polarity_ta_nconnected = 1,
	.bat_irq_attr =
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE |
		SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = SEC_BATTERY_CABLE_SOURCE_ADC |
		SEC_BATTERY_CABLE_SOURCE_EXTENDED,

	.event_check = false,
	.event_waiting_time = 600,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 3,
	/* Battery check by ADC */
	.check_adc_max = 0,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 2,
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 400,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 500,
	.temp_high_recovery_normal = 420,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 500,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,

	.full_check_type = SEC_BATTERY_FULLCHARGED_FG_CURRENT,
	.full_check_count = 2,/*1*/
	.full_check_adc_1st = 265,/*200*/
	.full_check_adc_2nd = 265,/*200*/
	.chg_gpio_full_check = GPIO_TA_nCHG,
	.chg_polarity_full_check = 0,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 97,
	.full_condition_vcell = 4300,

	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL |
		SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL,
	.recharge_condition_avgvcell = 4150,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 10 * 60 * 60,
	.recharging_total_time = 1.5 * 60 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = GPIO_FUEL_ALERT,
	.fg_irq_attr =
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.fuel_alert_soc = 1,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE,
		/* SEC_FUELGAUGE_CAPACITY_TYPE_SCALE | */
		/* SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC, */
	.capacity_max = 1000,
	.capacity_max_margin = 50,
	.capacity_min = 0,
	.get_fg_current = true,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = 0,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = IRQF_TRIGGER_FALLING,
	.chg_float_voltage = 4360,
	.chg_functions_setting = 0
};

#define SEC_FG_I2C_ID	18

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

#if defined(CONFIG_CHARGER_BQ24260) || defined(CONFIG_CHARGER_SMB347)
#define SEC_CHG_I2C_ID	13

static struct i2c_gpio_platform_data gpio_i2c_data_chg = {
	.sda_pin = (GPIO_CHG_SDA),
	.scl_pin = (GPIO_CHG_SCL),
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device sec_device_chg = {
	.name = "i2c-gpio",
	.id = SEC_CHG_I2C_ID,
	.dev = {
		.platform_data = &gpio_i2c_data_chg,
	},
};
#endif

static struct i2c_gpio_platform_data gpio_i2c_data_fg = {
	.sda_pin = GPIO_FUEL_SDA_18V,
	.scl_pin = GPIO_FUEL_SCL_18V,
};

struct platform_device sec_device_fg = {
	.name = "i2c-gpio",
	.id = SEC_FG_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_fg,
};

#if defined(CONFIG_CHARGER_BQ24260) || defined(CONFIG_CHARGER_SMB347)
static struct i2c_board_info sec_brdinfo_chg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-charger",
				SEC_CHARGER_I2C_SLAVEADDR),
		.platform_data = &sec_battery_pdata,
	},
};
#endif

static struct i2c_board_info sec_brdinfo_fg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
			SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};

static struct platform_device *vienna_battery_devices[] __initdata = {
#if defined(CONFIG_CHARGER_BQ24260) || defined(CONFIG_CHARGER_SMB347)
	&sec_device_chg,
#endif
	&sec_device_fg,
	&sec_device_battery,
};

static void charger_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_TA_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TA_INT, S3C_GPIO_PULL_NONE);

	if (system_rev < 0x03) {
		s3c_gpio_cfgpin(GPIO_TA_nCHG, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TA_nCHG, S3C_GPIO_PULL_NONE);
	}

	sec_battery_pdata.bat_irq = gpio_to_irq(GPIO_TA_INT);
#if 0
	sec_battery_pdata.chg_irq = gpio_to_irq(GPIO_CHG_INT);
#endif
}

void __init exynos5_vienna_battery_init(void)
{
	/* board dependent changes in booting */
	/* From system_rev 0x03, max77693(with mUIC) is used*/

	pr_info("%s (system_rev = 0x%x)\n", __func__, system_rev);
	if (system_rev >= 0x02) {
#if defined(CONFIG_CHARGER_BQ24260)
		gpio_i2c_data_chg.sda_pin = GPIO_CHG_SDA_02;
		gpio_i2c_data_chg.scl_pin = GPIO_CHG_SCL_02;
#endif
		sec_battery_pdata.bat_polarity_ta_nconnected = 0;
	}

#if defined(CONFIG_MFD_MAX77693)/*mUIC*/
	sec_battery_pdata.adc_type[0] = SEC_BATTERY_ADC_TYPE_NONE;	/* CABLE_CHECK */
	sec_battery_pdata.adc_type[1] = SEC_BATTERY_ADC_TYPE_NONE;	/* BAT_CHECK */
	sec_battery_pdata.adc_type[2] = SEC_BATTERY_ADC_TYPE_NONE;	/* TEMP */
	sec_battery_pdata.adc_type[3] = SEC_BATTERY_ADC_TYPE_NONE;	/* TEMP_AMB */
	sec_battery_pdata.adc_type[4] = SEC_BATTERY_ADC_TYPE_AP;	/* FULL_CHECK */
	sec_battery_pdata.is_interrupt_cable_check_possible = 0;
	sec_battery_pdata.bat_gpio_ta_nconnected = 0;
	sec_battery_pdata.bat_irq_attr = IRQF_TRIGGER_FALLING;
	sec_battery_pdata.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE |
		SEC_BATTERY_CABLE_CHECK_PSY;
	sec_battery_pdata.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL |
		SEC_BATTERY_CABLE_SOURCE_EXTENDED;
	sec_battery_pdata.chg_gpio_full_check = 0,
	sec_battery_pdata.chg_polarity_full_check = 1;
	sec_battery_pdata.chg_float_voltage = 4350;
#endif

	charger_gpio_init();

	platform_add_devices(
		vienna_battery_devices,
		ARRAY_SIZE(vienna_battery_devices));

#if defined(CONFIG_CHARGER_BQ24260) || defined(CONFIG_CHARGER_SMB347)
	i2c_register_board_info(SEC_CHG_I2C_ID, sec_brdinfo_chg,
			ARRAY_SIZE(sec_brdinfo_chg));
#endif

	i2c_register_board_info(
		SEC_FG_I2C_ID,
		sec_brdinfo_fg,
		ARRAY_SIZE(sec_brdinfo_fg));
}

#endif
