#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include "board-universal5410.h"
#include <linux/regulator/consumer.h>
#include <asm/system_info.h>

#ifdef CONFIG_SENSORS_CORE
#include <linux/sensor/sensors_core.h>
#endif
#ifdef CONFIG_SENSORS_AK8975C
#include <linux/sensor/ak8975.h>
#endif
#ifdef CONFIG_SENSORS_LSM330DLC
#include <linux/sensor/lsm330dlc_accel.h>
#include <linux/sensor/lsm330dlc_gyro.h>
#endif
#ifdef CONFIG_SENSORS_LPS331
#include <linux/sensor/lps331ap.h>
#endif
#ifdef CONFIG_SENSORS_CM36651
#include <linux/sensor/cm36651.h>
#endif
#ifdef CONFIG_SENSORS_SSP
#include <linux/ssp_platformdata.h>
#ifdef CONFIG_SENSORS_SSP_C12SD
#include <linux/sensor/guva_c12sd.h>
#include <plat/adc.h>
#endif
#endif

/* H/W revision */
#define HW_REV_09	9
#if defined(CONFIG_MACH_JA_KOR_SKT)
#define HW_REV_11	11
#endif
#define HW_REV_12	12

/* POSITION VALUES */
/* K330 */
#define K330_TOP_UPPER_LEFT		3
#define K330_TOP_UPPER_RIGHT		0
#define K330_TOP_LOWER_RIGHT		1
#define K330_TOP_LOWER_LEFT		2
#define K330_BOTTOM_UPPER_LEFT		5
#define K330_BOTTOM_UPPER_RIGHT		4
#define K330_BOTTOM_LOWER_RIGHT		7
#define K330_BOTTOM_LOWER_LEFT		6

/* AK8963C */
#define AK8963C_TOP_UPPER_LEFT		2
#define AK8963C_TOP_UPPER_RIGHT		3
#define AK8963C_TOP_LOWER_RIGHT		0
#define AK8963C_TOP_LOWER_LEFT		1
#define AK8963C_BOTTOM_UPPER_LEFT	6
#define AK8963C_BOTTOM_UPPER_RIGHT	5
#define AK8963C_BOTTOM_LOWER_RIGHT	4
#define AK8963C_BOTTOM_LOWER_LEFT	7

#ifdef CONFIG_SENSORS_SSP
static int wakeup_mcu(void);
static int check_mcu_busy(void);
static int check_mcu_ready(void);
static int set_mcu_reset(int on);
static int check_ap_rev(void);
static int ssp_read_chg(void);
static void ssp_get_positions(int *acc, int *mag);
static void gesture_power_on_off(bool onoff);
#ifdef CONFIG_SENSORS_SSP_C12SD
static int uv_adc_get(void);
static void uv_power_on(bool onoff);
static bool uv_adc_ap_init(struct platform_device *pdev);
static void uv_adc_ap_exit(struct platform_device *pdev);
struct s3c_adc_client *adc_client;
struct regulator *uv_vcc_1_8v;
#endif

#ifdef CONFIG_SENSORS_SSP_SHTC1
/* Add model config here */
#define CP_THM_CHANNEL_NUM		3

/* {adc, temp*10}, -20 to +70 */
static struct cp_thm_adc_table temp_table_cp[] = {
	{240, 700}, {247, 690}, {254, 680}, {261, 670}, {268, 660},
	{275, 650}, {283, 640}, {291, 630}, {299, 620}, {307, 610},
	{315, 600}, {327, 590}, {339, 580}, {351, 570}, {363, 560},
	{375, 550}, {387, 540}, {399, 530}, {411, 520}, {423, 510},
	{435, 500}, {449, 490}, {463, 480}, {477, 470}, {491, 460},
	{505, 450}, {527, 440}, {549, 430}, {571, 420}, {593, 410},
	{615, 400}, {635, 390}, {655, 380}, {675, 370}, {695, 360},
	{715, 350}, {736, 340}, {757, 330}, {778, 320}, {799, 310},
	{820, 300}, {842, 290}, {864, 280}, {886, 270}, {908, 260},
	{953, 250}, {953, 240}, {976, 230}, {999, 220}, {1022, 210},
	{1045, 200}, {1067, 190}, {1089, 180}, {1111, 170}, {1133, 160},
	{1155, 150}, {1178, 140}, {1201, 130}, {1224, 120}, {1247, 110},
	{1270, 100}, {1291, 90}, {1291, 80}, {1333, 70}, {1354, 60},
	{1375, 50}, {1384, 40}, {1393, 30}, {1402, 20}, {1411, 10},
	{1420, 0},
	{1432, -10}, {1444, -20}, {1456, -30}, {1468, -40}, {1480, -50},
	{1490, -60}, {1500, -70}, {1510, -80}, {1520, -90}, {1530, -100},
	{1544, -110}, {1558, -120}, {1572, -130}, {1586, -140},{1600, -150}, 
	{1615, -160}, {1630, -170}, {1645, -180}, {1660, -190}, {1675, -200},
 };
#endif

static struct ssp_platform_data ssp_pdata = {
	.wakeup_mcu = wakeup_mcu,
	.check_mcu_busy = check_mcu_busy,
	.check_mcu_ready = check_mcu_ready,
	.set_mcu_reset = set_mcu_reset,
	.check_ap_rev = check_ap_rev,
	.read_chg = ssp_read_chg,
	.get_positions = ssp_get_positions,
	.gesture_power_on_off = gesture_power_on_off,
#ifdef CONFIG_SENSORS_SSP_SHTC1
	.cp_thm_adc_channel = CP_THM_CHANNEL_NUM,
	.cp_thm_adc_arr_size = ARRAY_SIZE(temp_table_cp),
	.cp_thm_adc_table = temp_table_cp,
#endif
};

#ifdef CONFIG_SENSORS_SSP_C12SD
static struct uv_platform_data uv_pdata = {
	.get_adc_value = uv_adc_get,
	.power_on = uv_power_on,
	.adc_ap_init = uv_adc_ap_init,
	.adc_ap_exit = uv_adc_ap_exit,
};
#endif
#endif

#ifdef CONFIG_SENSORS_LSM330DLC
static int stm_get_position(void);

static struct accel_platform_data accel_pdata = {
	.accel_get_position = stm_get_position,
	.axis_adjust = true,
};

static struct gyro_platform_data gyro_pdata = {
	.gyro_get_position = stm_get_position,
	.axis_adjust = true,
};
#endif

#if defined(CONFIG_MACH_V1)
static struct i2c_gpio_platform_data i2c23_platdata = {
	.sda_pin = GPIO_AP_MCU_SDA_18V,
	.scl_pin = GPIO_AP_MCU_SCL_18V,
	.udelay = 2,		/* 250KHz */
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c23 = {
	.name = "i2c-gpio",
	.id = 23,
	.dev.platform_data = &i2c23_platdata,
};

static struct i2c_board_info i2c_devs23_emul[] __initdata = {
	{
		I2C_BOARD_INFO("ssp", 0x18),
		.platform_data = &ssp_pdata,
		.irq = GPIO_MCU_AP_INT,
	},
};
#endif

static struct i2c_board_info i2c_devs7[] __initdata = {
#if defined(CONFIG_SENSORS_LSM330DLC)
	{
		I2C_BOARD_INFO("lsm330dlc_accel", (0x32 >> 1)),
		.platform_data = &accel_pdata,
	},
	{
		I2C_BOARD_INFO("lsm330dlc_gyro", (0xD6 >> 1)),
		.platform_data = &gyro_pdata,
	},
#elif defined(CONFIG_SENSORS_SSP)
	{
		I2C_BOARD_INFO("ssp", 0x18),
		.platform_data = &ssp_pdata,
		.irq = GPIO_MCU_AP_INT,
	},
#endif
};

#ifdef CONFIG_SENSORS_SSP
static int initialize_ssp_gpio(void)
{
	int err = 0;

#if defined(CONFIG_UNIVERSAL5410_LTE_REV00)\
	&& defined(CONFIG_USE_GPIO_AP_MCU_RXTXD)
	err = gpio_request(GPIO_AP_MCU_RXD, "AP_MCU_RXD");
	if (err)
		pr_err("%s, failed to request AP_MCU_RXD for SSP\n",
			__func__);
	s3c_gpio_cfgpin(GPIO_AP_MCU_RXD, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_AP_MCU_RXD, S3C_GPIO_PULL_NONE);

	err = gpio_request(GPIO_AP_MCU_TXD, "AP_MCU_TXD");
	if (err)
		pr_err("%s, failed to request AP_MCU_TXD for SSP\n",
			__func__);
	s3c_gpio_cfgpin(GPIO_AP_MCU_TXD, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_AP_MCU_TXD, S3C_GPIO_PULL_NONE);
#endif

	err = gpio_request(GPIO_AP_MCU_INT, "AP_MCU_INT_PIN");
	if (err)
		pr_err("%s, failed to request AP_MCU_INT for SSP\n", __func__);

	s3c_gpio_cfgpin(GPIO_AP_MCU_INT, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_AP_MCU_INT, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_AP_MCU_INT, 1);

	err = gpio_request(GPIO_MCU_AP_INT_2, "MCU_AP_INT_PIN2");
	if (err)
		pr_err("%s, failed to request MCU_AP_INT2 for SSP\n", __func__);
	s3c_gpio_cfgpin(GPIO_MCU_AP_INT_2, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_MCU_AP_INT_2, S3C_GPIO_PULL_NONE);

	err = gpio_request(GPIO_MCU_NRST, "AP_MCU_RESET");
	if (err)
		pr_err("%s, failed to request AP_MCU_RESET for SSP\n",
			__func__);
	s3c_gpio_cfgpin(GPIO_MCU_NRST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MCU_NRST, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_MCU_NRST, 1);

#if defined(CONFIG_MACH_UNIVERSAL5410)
	err = gpio_request(GPIO_MCU_CHG, "GPIO_MCU_CHG");
	if (err)
		pr_err("%s, failed to request GPIO_MCU_CHG for SSP\n",
			__func__);
	s3c_gpio_cfgpin(GPIO_MCU_CHG, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_MCU_CHG, S3C_GPIO_PULL_NONE);
#endif

	err = gpio_request(GPIO_MCU_AP_INT, "MCU_AP_INT_PIN");
	if (err)
		pr_err("%s, failed to request MCU_AP_INT for SSP\n", __func__);
	s3c_gpio_setpull(GPIO_MCU_AP_INT, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_MCU_AP_INT);

	return err;
}

static int wakeup_mcu(void)
{
	gpio_set_value(GPIO_AP_MCU_INT, 0);
	udelay(1);
	gpio_set_value(GPIO_AP_MCU_INT, 1);

	return 0;
}

static int set_mcu_reset(int on)
{
	if (on == 0)
		gpio_set_value(GPIO_MCU_NRST, 0);
	else
		gpio_set_value(GPIO_MCU_NRST, 1);

	return 0;
}

static int check_mcu_busy(void)
{
	return gpio_get_value(GPIO_MCU_AP_INT);
}
static int check_mcu_ready(void)
{
	return gpio_get_value(GPIO_MCU_AP_INT_2);
}

static int check_ap_rev(void)
{
	return system_rev;
}

static int ssp_read_chg()
{
#if defined(CONFIG_MACH_UNIVERSAL5410)
	return gpio_get_value(GPIO_MCU_CHG);
#else
	return 0;
#endif
}

/* MCU
 * 0,     1,      2,      3,      4,     5,      6,      7
 * PXPYPZ, PYNXPZ, NXNYPZ, NYPXPZ, NXPYNZ, PYPXNZ, PXNYNZ, NYNXNZ
 * *******************************************************
 * Sensors
 * top/upper-left => top/upper-right ... ... =>	bottom/lower-left
 * 1. K330
 * NYPXPZ, PXPYPZ, PYNXPZ, NXNYPZ, PYPXNZ, NXPYNZ, NYNXNZ, PXNYNZ
 * 2. AK8963C
 * NXNYPZ, NYPXPZ, PXPYPZ, PYNXPZ, PXNYNZ, PYPXNZ, NXPYNZ, NYNXNZ
 * 3. LSM330DLC
 * NXNYPZ, NYPXPZ, PXPYPZ, PYNXPZ, PXNYNZ, PYPXNZ, NXPYNZ, NYNXNZ
*/
static void ssp_get_positions(int *acc, int *mag)
{
#if defined(CONFIG_MACH_UNIVERSAL5410)
#if defined(CONFIG_MACH_JA_KOR_SKT)
	*acc = K330_TOP_UPPER_LEFT;
	*mag = AK8963C_TOP_LOWER_RIGHT;
#elif defined(CONFIG_MACH_V1)
	*acc = K330_BOTTOM_LOWER_LEFT;
	*mag = AK8963C_BOTTOM_LOWER_LEFT;
#elif defined(CONFIG_MACH_J_CHN_CTC) || defined(CONFIG_MACH_J_CHN_CU)
	*acc = K330_TOP_UPPER_LEFT;
	if (system_rev >= HW_REV_12)
		*mag = AK8963C_TOP_LOWER_RIGHT;
	else if (system_rev >= HW_REV_09)
		*mag = AK8963C_BOTTOM_LOWER_RIGHT;
	else
		*mag = AK8963C_BOTTOM_UPPER_LEFT;
#else
	*acc = K330_TOP_UPPER_LEFT;
	*mag = AK8963C_TOP_LOWER_RIGHT;
#endif
#else
	*acc = K330_TOP_UPPER_LEFT;
	*mag = AK8963C_TOP_UPPER_LEFT;
#endif
	pr_info("%s, position acc : %d, mag = %d\n", __func__, *acc, *mag);
}

static void gesture_power_on_off(bool onoff)
{
	struct regulator *vgesture_vcc_1_8v;
	int err = 0;

	vgesture_vcc_1_8v = regulator_get(NULL, "vgesture_1.8v");
	if (IS_ERR(vgesture_vcc_1_8v)) {
		pr_err("%s, regulator_get vgesture_1.8v failed(%ld)\n",
			__func__, IS_ERR(vgesture_vcc_1_8v));
		return;
	}

	err = regulator_enable(vgesture_vcc_1_8v);
	if (err < 0)
		pr_err("%s, vgesture_vcc_1_8v on fail(%d)\n",
			__func__, err);

	if (system_rev <= 1)
		onoff = 0;

	pr_info("%s, onoff = %d, system_rev = %d\n",
        __func__, onoff, system_rev);
	if (onoff) {
		if (!regulator_is_enabled(vgesture_vcc_1_8v)) {
			err = regulator_enable(vgesture_vcc_1_8v);
			if (err < 0)
				pr_err("%s, vgesture_vcc_1_8v on fail(%d)\n",
					__func__, err);
		}
	} else {
		if (regulator_is_enabled(vgesture_vcc_1_8v)) {
			err = regulator_disable(vgesture_vcc_1_8v);
			if (err < 0)
				pr_err("%s, vgesture_vcc_1_8v off fail(%d)\n",
					__func__, err);
		}
	}
	pr_info("%s, Gesture power %s \n",
        __func__, onoff ? "ON" : "OFF");
}

#ifdef CONFIG_SENSORS_SSP_C12SD
/* UV SENSOR */
static bool uv_adc_ap_init(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	adc_client = s3c_adc_register(pdev, NULL, NULL, 0);
	if (IS_ERR(adc_client)) {
		pr_err("%s, fail to register uv_adc(%ld)\n",
			__func__, IS_ERR(adc_client));
		return false;
	}
	uv_vcc_1_8v = regulator_get(NULL, UV_VCC_1_8_V_NAME);
	if (IS_ERR(uv_vcc_1_8v)) {
		pr_err("%s,%s failed(%ld)\n",
			__func__, UV_VCC_1_8_V_NAME,
			IS_ERR(uv_vcc_1_8v));
		return false;
	}
	return true;
}

static void uv_adc_ap_exit(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	s3c_adc_release(adc_client);
	if (regulator_is_enabled(uv_vcc_1_8v))
		regulator_force_disable(uv_vcc_1_8v);
	regulator_put(uv_vcc_1_8v);
}

static int uv_adc_get(void)
{
	return s3c_adc_read(adc_client, 2);
}

static void uv_power_on(bool onoff)
{
	int err = 0;

	if (onoff) {
		if (!regulator_is_enabled(uv_vcc_1_8v)) {
			err = regulator_enable(uv_vcc_1_8v);
			if (err < 0)
				pr_err("%s, uv_vcc on fail(%d)\n",
					__func__, err);
		}
	} else {
		if (regulator_is_enabled(uv_vcc_1_8v)) {
			regulator_disable(uv_vcc_1_8v);
			if (err < 0)
				pr_err("%s, uv_vcc off fail(%d)\n",
					__func__, err);
		}
	}
	pr_info("%s, poweron = %d\n", __func__, onoff);
}
#endif
#endif

#ifdef CONFIG_SENSORS_LSM330DLC
/*
 * {{-1,  0, 0}, {  0, -1, 0}, { 0, 0, 1} }, 0 top/upper-left
 * {{ 0, -1, 0}, {  1,  0, 0}, { 0, 0, 1} }, 1 top/upper-right
 * {{ 1,  0, 0}, {  0,  1, 0}, { 0,  0,  1} }, 2 top/lower-right
 * {{ 0,  1, 0}, {-1,  0, 0}, { 0,  0,  1} }, 3 top/lower-left
 * {{ 1,  0, 0}, {  0, -1, 0}, { 0,  0, -1} },  4 bottom/upper-left
 * {{ 0,  1, 0}, {  1,  0, 0}, { 0,  0, -1} },  5 bottom/upper-right
 * {{-1,  0, 0}, {  0,  1, 0}, { 0,  0, -1} },  6 bottom/lower-right
 * {{ 0, -1, 0}, {-1,  0, 0}, { 0,  0, -1} },  7 bottom/lower-left
*/
static int stm_get_position(void)
{
	int position = 2;

/* Add model config and position here. */

	return position;
}

static int accel_gpio_init(void)
{
	int ret = gpio_request(GPIO_ACC_INT, "accelerometer_irq");

	pr_info("%s\n", __func__);

	if (ret) {
		pr_err("%s, Failed to request gpio accelerometer_irq(%d)\n",
		       __func__, ret);
		return ret;
	}

	/* Accelerometer sensor interrupt pin initialization */
	s3c_gpio_cfgpin(GPIO_ACC_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_ACC_INT, 2);
	s3c_gpio_setpull(GPIO_ACC_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_ACC_INT, S5P_GPIO_DRVSTR_LV1);
	i2c_devs7[0].irq = gpio_to_irq(GPIO_ACC_INT);

	return ret;
}

static int gyro_gpio_init(void)
{
	int ret = gpio_request(GPIO_GYRO_INT, "lsm330dlc_gyro_irq");

	pr_info("%s\n", __func__);

	if (ret) {
		pr_err("%s, Failed to request gpio lsm330dlc_gyro_irq(%d)\n",
		       __func__, ret);
		return ret;
	}

	ret = gpio_request(GPIO_GYRO_DE, "lsm330dlc_gyro_data_enable");

	if (ret) {
		pr_err
		("%s, Failed to request gpio lsm330dlc_gyro_data_enable(%d)\n",
		     __func__, ret);
		return ret;
	}

	/* Gyro sensor interrupt pin initialization */
	s3c_gpio_cfgpin(GPIO_GYRO_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_GYRO_INT, 2);
	s3c_gpio_setpull(GPIO_GYRO_INT, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(GPIO_GYRO_INT, S5P_GPIO_DRVSTR_LV1);
	i2c_devs7[1].irq = -1;	/* polling */

	/* Gyro sensor data enable pin initialization */
	s3c_gpio_cfgpin(GPIO_GYRO_DE, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_GYRO_DE, 0);
	s3c_gpio_setpull(GPIO_GYRO_DE, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(GPIO_GYRO_DE, S5P_GPIO_DRVSTR_LV1);

	return ret;
}
#endif

#ifdef CONFIG_SENSORS_CM36651
static struct i2c_gpio_platform_data i2c9_platdata = {
	.sda_pin = GPIO_RGB_SDA_18V,
	.scl_pin = GPIO_RGB_SCL_18V,
	.udelay = 2,		/* 250KHz */
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c9 = {
	.name = "i2c-gpio",
	.id = 9,
	.dev.platform_data = &i2c9_platdata,
};

/* Depends window, threshold is needed to be set */
static u8 cm36651_get_threshold(void)
{
	u8 threshold = 15;

	/* Add model config and threshold here. */

	return threshold;
}

static struct cm36651_platform_data cm36651_pdata = {
	.cm36651_get_threshold = cm36651_get_threshold,
	.irq = GPIO_RGB_INT,
};

static struct i2c_board_info i2c_devs9_emul[] __initdata = {
	{
		I2C_BOARD_INFO("cm36651", (0x30 >> 1)),
		.platform_data = &cm36651_pdata,
	},
};
#endif

#ifdef CONFIG_SENSORS_AK8975C
static struct i2c_gpio_platform_data i2c10_platdata = {
	.sda_pin = GPIO_MSENSE_SDA_18V,
	.scl_pin = GPIO_MSENSE_SCL_18V,
	.udelay = 2,		/* 250KHz */
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c10 = {
	.name = "i2c-gpio",
	.id = 10,
	.dev.platform_data = &i2c10_platdata,
};

static struct akm8975_platform_data akm8975_pdata = {
	.gpio_data_ready_int = GPIO_MSENSE_INT,
};

static struct i2c_board_info i2c_devs10_emul[] __initdata = {
	{
		I2C_BOARD_INFO("ak8975", 0x0C),
		.platform_data = &akm8975_pdata,
	},
};

static int ak8975c_gpio_init(void)
{
	int ret = gpio_request(GPIO_MSENSE_INT, "gpio_akm_int");

	pr_info("%s\n", __func__);

	if (ret) {
		pr_err("%s, Failed to request gpio akm_int.(%d)\n",
			__func__, ret);
		return ret;
	}

	s5p_register_gpio_interrupt(GPIO_MSENSE_INT);
	s3c_gpio_setpull(GPIO_MSENSE_INT, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_MSENSE_INT, S3C_GPIO_SFN(0xF));
	i2c_devs10_emul[0].irq = gpio_to_irq(GPIO_MSENSE_INT);
	return ret;
}
#endif

#ifdef CONFIG_SENSORS_LPS331
static struct i2c_gpio_platform_data i2c11_platdata = {
	.sda_pin = GPIO_BSENSE_SDA_18V,
	.scl_pin = GPIO_BSENSE_SCL_18V,
	.udelay = 2,		/* 250KHz */
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_i2c11 = {
	.name = "i2c-gpio",
	.id = 11,
	.dev.platform_data = &i2c11_platdata,
};

static int lps331_gpio_init(void)
{
	int ret = gpio_request(GPIO_BARO_INT, "lps331_irq");

	pr_info("%s\n", __func__);

	if (ret) {
		pr_err("%s, Failed to request gpio lps331_irq(%d)\n",
		       __func__, ret);
		return ret;
	}

	s3c_gpio_cfgpin(GPIO_BARO_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_BARO_INT, 2);
	s3c_gpio_setpull(GPIO_BARO_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_BARO_INT, S5P_GPIO_DRVSTR_LV1);
	return ret;
}

static struct lps331ap_platform_data lps331ap_pdata = {
	.irq = GPIO_BARO_INT,
};

static struct i2c_board_info i2c_devs11_emul[] __initdata = {
	{
		I2C_BOARD_INFO("lps331ap", 0x5D),
		.platform_data = &lps331ap_pdata,
	},
};
#endif

static struct platform_device *universal5410_sensor_devices[] __initdata = {
#if defined(CONFIG_SENSORS_SSP)
#if defined(CONFIG_MACH_V1)
	&s3c_device_i2c23,
#endif
	&exynos5_device_hs_i2c1,
#endif
#if defined(CONFIG_SENSORS_CM36651)
	&s3c_device_i2c9,
#endif
#if defined(CONFIG_SENSORS_AK8975C)
	&s3c_device_i2c10,
#endif
#if defined(CONFIG_SENSORS_LPS331)
	&s3c_device_i2c11,
#endif
};

#if defined(CONFIG_SENSORS_SSP)
struct exynos5_platform_i2c hs_i2c1_data __initdata = {
	.bus_number = 5,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 380000,
	.high_speed = 0,
	.cfg_gpio = NULL,
};
#ifdef CONFIG_SENSORS_SSP_C12SD
static struct platform_device uv_sensor = {
	.name = "uv_sensor",
	.dev.platform_data = &uv_pdata,
};
#endif
#endif

void __init exynos5_universal5410_sensor_init(void)
{
	int err = 0;

#if defined(CONFIG_SENSORS_LSM330DLC)
	/* Accelerometer & Gyro Sensor */
	err = accel_gpio_init();
	if (err < 0)
		pr_err("%s, accel_gpio_init fail(err=%d)\n", __func__, err);

	err = gyro_gpio_init();
	if (err < 0)
		pr_err("%s, gyro_gpio_init fail(err=%d)\n", __func__, err);
#endif

#if defined(CONFIG_SENSORS_SSP)
	err = initialize_ssp_gpio();
	if (err < 0)
		pr_err("%s, initialize_ssp_gpio fail(err=%d)\n",
			__func__, err);
#endif

#if defined(CONFIG_MACH_V1)
	if (system_rev >= 2) {
		/* hs-i2c device 3 is i2c7. */
#if defined(CONFIG_SENSORS_SSP)
		/* MCU does not support 400kHz with 8Mhz clock. */
		exynos5_hs_i2c1_set_platdata(&hs_i2c1_data);
#else
		exynos5_hs_i2c1_set_platdata(NULL);
#endif
		i2c_register_board_info(5, i2c_devs7, ARRAY_SIZE(i2c_devs7));
	} else {
		i2c_register_board_info(23, i2c_devs23_emul,
			ARRAY_SIZE(i2c_devs23_emul));
	}
#else
	/* hs-i2c device 3 is i2c7. */
#if defined(CONFIG_SENSORS_SSP)
	/* MCU does not support 400kHz with 8Mhz clock. */
	exynos5_hs_i2c1_set_platdata(&hs_i2c1_data);
#else
	exynos5_hs_i2c1_set_platdata(NULL);
#endif
	i2c_register_board_info(4, i2c_devs7, ARRAY_SIZE(i2c_devs7));
#endif

#if defined(CONFIG_SENSORS_CM36651)
	/* Optical Sensor */
	i2c_register_board_info(9, i2c_devs9_emul, ARRAY_SIZE(i2c_devs9_emul));
#endif

#if defined(CONFIG_SENSORS_AK8975C)
	/* Magnetic Sensor */
	err = ak8975c_gpio_init();
	if (err < 0)
		pr_err("%s, ak8975c_gpio_init fail(err=%d)\n", __func__, err);

	i2c_register_board_info(10, i2c_devs10_emul,
				ARRAY_SIZE(i2c_devs10_emul));
#endif

#if defined(CONFIG_SENSORS_LPS331)
	/* Pressure Sensor */
	err = lps331_gpio_init();
	if (err < 0)
		pr_err("%s, lps331_gpio_init fail(err=%d)\n", __func__, err);

	i2c_register_board_info(11, i2c_devs11_emul,
				ARRAY_SIZE(i2c_devs11_emul));
#endif

	platform_add_devices(universal5410_sensor_devices,
			     ARRAY_SIZE(universal5410_sensor_devices));

#ifdef CONFIG_SENSORS_SSP_C12SD
	err = platform_device_register(&uv_sensor);
	if (err < 0)
		pr_err("%s, C12SD platform device register failed (err=%d)\n",
			__func__, err);
#endif
}
