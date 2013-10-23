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

#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>

#ifdef CONFIG_SENSORS_CORE
#include <linux/sensor/sensors_core.h>
#endif
#ifdef CONFIG_SENSORS_SSP
#include <linux/ssp_platformdata.h>
#endif
#ifdef CONFIG_SENSORS_VFS61XX
#include <linux/vfs61xx_platform.h>
#endif

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
#if defined(CONFIG_SENSORS_SSP_MAX88920)
static void gesture_power_on_off(bool onoff);
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
	.get_positions = ssp_get_positions,
#if defined(CONFIG_SENSORS_SSP_STM)
	.mcu_int1 = GPIO_MCU_AP_INT,
	.mcu_int2 = GPIO_MCU_AP_INT_2,
	.ap_int = GPIO_AP_MCU_INT,
#endif
#if defined(CONFIG_SENSORS_SSP_MAX88920)
	.gesture_power_on_off = gesture_power_on_off,
#endif
#ifdef CONFIG_SENSORS_SSP_SHTC1
	.cp_thm_adc_channel = CP_THM_CHANNEL_NUM,
	.cp_thm_adc_arr_size = ARRAY_SIZE(temp_table_cp),
	.cp_thm_adc_table = temp_table_cp,
#endif
};
#endif

#if defined(CONFIG_SENSORS_SSP_ATMEL)
static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("ssp", 0x18),
		.platform_data = &ssp_pdata,
		.irq = GPIO_MCU_AP_INT,
	},
};
#endif

#ifdef CONFIG_SENSORS_SSP
static int initialize_ssp_gpio(void)
{
	int err;

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
	*acc = K330_BOTTOM_LOWER_LEFT;
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
#endif

#if defined(CONFIG_SENSORS_SSP_ATMEL)
static struct platform_device *universal5410_sensor_devices[] __initdata = {
	&exynos5_device_hs_i2c1,
};

struct exynos5_platform_i2c hs_i2c1_data __initdata = {
	.bus_number = 5,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 380000,
	.high_speed = 0,
	.cfg_gpio = NULL,
};
#endif


#if defined(CONFIG_SENSORS_SSP_STM)
static struct s3c64xx_spi_csinfo spi0_csi[] = {
	[0] = {
		.line		= GPIO_SHUB_SPI_SSN,
		.set_level	= gpio_set_value,
		.fb_delay	= 0x0,
	},
};

static struct spi_board_info spi0_board_info[] __initdata = {
	{
		.modalias		= "ssp-spi",
		.max_speed_hz		= 500 * 1000,
		.bus_num		= 0,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq			= IRQ_EINT(27),
		.controller_data	= &spi0_csi[0],
		.platform_data		= &ssp_pdata,
	}
};
#endif


#ifdef CONFIG_SENSORS_VFS61XX
static int vfs61xx_regulator_onoff(int onoff)
{
	struct regulator *vfs_sovcc;
	struct regulator *vfs_vcc;
	pr_info("%s: %s\n", __func__, onoff? "on":"off");

	if (system_rev < 1) {
		vfs_vcc = regulator_get(NULL, "vtouch_1.8v");

		if (IS_ERR(vfs_vcc)) {
			pr_err("%s: cannot get vfs_sovcc\n", __func__);
			return -1;
		}
		if (onoff) {
				regulator_enable(vfs_vcc);
		} else
			regulator_disable(vfs_vcc);

		regulator_put(vfs_vcc);
	}

	vfs_sovcc = regulator_get(NULL, "vtouch_3.3v");

	if (IS_ERR(vfs_sovcc)) {
		pr_err("%s: cannot get vfs_sovcc\n", __func__);
		return -1;
	}
	if (onoff) {
			regulator_enable(vfs_sovcc);
	 } else
		regulator_disable(vfs_sovcc);

	regulator_put(vfs_sovcc);

	return 0;
}

static void vfs61xx_setup_gpio(void)
{
	s3c_gpio_cfgpin(GPIO_BTP_IRQ, S3C_GPIO_SFN(S3C_GPIO_INPUT));
	s3c_gpio_setpull(GPIO_BTP_IRQ, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_BTP_RST_N, S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
	s3c_gpio_setpull(GPIO_BTP_RST_N, S3C_GPIO_PULL_DOWN);
}

static struct vfs61xx_platform_data vfs61xx_pdata = {
	.drdy = GPIO_BTP_IRQ,
	.sleep = GPIO_BTP_RST_N,
	.vfs61xx_sovcc_on = vfs61xx_regulator_onoff,
};

static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line		= GPIO_BTP_SPI_CS_N,
		.set_level	= gpio_set_value,
		.fb_delay	= 0x0,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias		= "validity_fingerprint",
		.max_speed_hz		= 15 * 1000 * 1000,
		.bus_num		= 1,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq			= IRQ_EINT(25),
		.controller_data	= &spi1_csi[0],
		.platform_data		= &vfs61xx_pdata,
	}
};
#endif

void __init exynos5_universal5410_sensor_init(void)
{
	int err;

#if defined(CONFIG_SENSORS_SSP)
	err = initialize_ssp_gpio();
	if (err < 0)
		pr_err("%s, initialize_ssp_gpio fail(err=%d)\n",
			__func__, err);
#endif

#if defined(CONFIG_SENSORS_SSP_STM)
	exynos_spi_clock_setup(&s3c64xx_device_spi0.dev, 0);
	if (!exynos_spi_cfg_cs(spi0_csi[0].line, 0)) {
		pr_info("%s: spi0_set_platdata ...\n", __func__);
		s3c64xx_spi0_set_platdata(&s3c64xx_spi0_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi0_csi));

		spi_register_board_info(spi0_board_info,
			ARRAY_SIZE(spi0_board_info));

		pr_info("%s, %d, busbum = %d\n", __func__, __LINE__, spi0_board_info->bus_num);
	} else {
		pr_err("%s : Error requesting gpio for SPI-CH%d CS",
			__func__, spi0_board_info->bus_num);
	}
	platform_device_register(&s3c64xx_device_spi0);

#elif defined(CONFIG_SENSORS_SSP_ATMEL)
	/* MCU does not support 400kHz with 8Mhz clock. */
	exynos5_hs_i2c1_set_platdata(&hs_i2c1_data);
	i2c_register_board_info(4, i2c_devs7, ARRAY_SIZE(i2c_devs7));

	platform_add_devices(universal5410_sensor_devices,
			     ARRAY_SIZE(universal5410_sensor_devices));
#endif

#ifdef CONFIG_SENSORS_VFS61XX
	pr_info("%s: SENSORS_VFS61XX init\n", __func__);
	vfs61xx_setup_gpio();
	s3c64xx_spi1_pdata.dma_mode = PIO_MODE;
	exynos_spi_clock_setup(&s3c64xx_device_spi1.dev, 1);
	if (!exynos_spi_cfg_cs(spi1_csi[0].line, 1)) {
		pr_info("%s: spi1_set_platdata ...\n", __func__);
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));

		spi_register_board_info(spi1_board_info,
			ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err("%s : Error requesting gpio for SPI-CH%d CS",
			__func__, spi1_board_info->bus_num);
	}
	platform_device_register(&s3c64xx_device_spi1);
#endif
}
