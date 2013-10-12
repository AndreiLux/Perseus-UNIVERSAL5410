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
#include <asm/system_info.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#include "board-universal5420.h"

#ifdef CONFIG_SENSORS_SSP
#include <linux/ssp_platformdata.h>
#endif
#ifdef CONFIG_SENSORS_VFS61XX
#include <linux/vfs61xx_platform.h>
#endif
#ifdef CONFIG_SENSORS_BMI055
#include <linux/bma255_platformdata.h>
#include <linux/bmg160_platformdata.h>
#endif
#ifdef CONFIG_SENSORS_AK8963C
#include <linux/ak8963c_platformdata.h>
#endif

#ifdef CONFIG_SENSORS_BMI055
static void bma255_get_position(int *pos)
{
	*pos = BMA255_BOTTOM_UPPER_LEFT;
}

static void bmg160_get_position(int *pos)
{
	*pos = BMG160_BOTTOM_UPPER_LEFT;
}

static struct bma255_platform_data bma255_pdata = {
	.get_pos = bma255_get_position,
	.acc_int1 = GPIO_ACC_INT1,
	.acc_int2 = GPIO_ACC_INT2,
};

static struct bmg160_platform_data bmg160_pdata = {
	.get_pos = bmg160_get_position,
	.gyro_int = GPIO_GYRO_INT,
	.gyro_drdy = GPIO_GYRO_DRDY,
};
#endif

#ifdef CONFIG_SENSORS_AK8963C
static void ak8963c_get_position(int *pos)
{
	*pos = AK8963C_BOTTOM_LOWER_LEFT;
}

static struct ak8963c_platform_data ak8963c_pdata = {
	.get_pos = ak8963c_get_position,
	.m_rst_n = GPIO_M_RST_N,
	.m_sensor_int = GPIO_MSENSOR_INT,
};
#endif

#if defined(CONFIG_V1A) || defined(CONFIG_N1A)
static struct i2c_board_info i2c_devs3[] __initdata = {
	{
		I2C_BOARD_INFO("bma255-i2c", 0x18),
		.platform_data = &bma255_pdata,
	},
	{
		I2C_BOARD_INFO("bmg160-i2c", 0x68),
		.platform_data = &bmg160_pdata,
	},
	{
		I2C_BOARD_INFO("ak8963c-i2c", 0x0c),
		.platform_data = &ak8963c_pdata,
	},
	{
		I2C_BOARD_INFO("cm3323-i2c", 0x10),
	},
};
#endif

#if defined(CONFIG_V1A_3G) || defined(CONFIG_N1A_3G)
struct exynos5_platform_i2c hs_i2c0_data __initdata = {
	.bus_number = 4,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 100000,
	.high_speed = 0,
	.cfg_gpio = NULL,
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("sx9500-i2c", 0x28),
		.irq = GPIO_GRIP_DET_AP,
	},
};
#endif

#ifdef CONFIG_SENSORS_SSP
u8 ssp_magnetic_pdc[] = {110, 85, 171, 71, 203, 195, 0, 67,\
			208, 56, 175, 244, 206, 213, 0, 92, 250, 0,\
			55, 48, 189, 252, 171, 243, 13, 45, 250};

static int wakeup_mcu(void);
static int check_mcu_busy(void);
static int check_mcu_ready(void);
static int set_mcu_reset(int on);
static int check_ap_rev(void);
static void ssp_get_positions(int *acc, int *mag);

static struct ssp_platform_data ssp_pdata = {
	.wakeup_mcu = wakeup_mcu,
	.check_mcu_busy = check_mcu_busy,
	.check_mcu_ready = check_mcu_ready,
	.set_mcu_reset = set_mcu_reset,
	.check_ap_rev = check_ap_rev,
	.get_positions = ssp_get_positions,
	.mag_matrix_size = ARRAY_SIZE(ssp_magnetic_pdc),
	.mag_matrix = ssp_magnetic_pdc,
#ifdef CONFIG_SENSORS_SSP_SHTC1
	.cp_thm_adc_channel = CP_THM_CHANNEL_NUM,
	.cp_thm_adc_arr_size = ARRAY_SIZE(temp_table_cp),
	.cp_thm_adc_table = temp_table_cp,
	.batt_thm_adc_arr_size = ARRAY_SIZE(temp_table_batt),
	.batt_thm_adc_table = temp_table_batt,
	.chg_thm_adc_arr_size = ARRAY_SIZE(temp_table_chg),
	.chg_thm_adc_table = temp_table_chg,
#endif
#ifdef CONFIG_SENSORS_SSP_STM
	.ap_int = GPIO_AP_MCU_INT_18V,
	.mcu_int1 = GPIO_MCU_AP_INT_1_18V,
	.mcu_int2 = GPIO_MCU_AP_INT_2_18V,
#endif
};
#endif


#ifdef CONFIG_SENSORS_SSP
static int initialize_ssp_gpio(void)
{
	int ret;

	pr_info("%s, is called\n", __func__);

	ret = gpio_request(GPIO_AP_MCU_INT_18V, "AP_MCU_INT_PIN");
	if (ret)
		pr_err("%s, failed to request AP_MCU_INT for SSP\n", __func__);

	s3c_gpio_cfgpin(GPIO_AP_MCU_INT_18V, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_AP_MCU_INT_18V, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_AP_MCU_INT_18V, 1);

	ret = gpio_request(GPIO_MCU_AP_INT_2_18V, "MCU_AP_INT_PIN2");
	if (ret)
		pr_err("%s, failed to request MCU_AP_INT2 for SSP\n", __func__);

	s3c_gpio_cfgpin(GPIO_MCU_AP_INT_2_18V, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_MCU_AP_INT_2_18V, S3C_GPIO_PULL_NONE);

	ret = gpio_request(GPIO_MCU_nRST_18V, "AP_MCU_RESET");
	if (ret)
		pr_err("%s, failed to request AP_MCU_RESET for SSP\n",
			__func__);

	s3c_gpio_cfgpin(GPIO_MCU_nRST_18V, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MCU_nRST_18V, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_MCU_nRST_18V, 1);

	ret = gpio_request(GPIO_MCU_AP_INT_1_18V, "MCU_AP_INT_PIN");
	if (ret)
		pr_err("%s, failed to request MCU_AP_INT for SSP\n", __func__);

	s3c_gpio_setpull(GPIO_MCU_AP_INT_1_18V, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_MCU_AP_INT_1_18V);

	return ret;
}

static int wakeup_mcu(void)
{
	gpio_set_value(GPIO_AP_MCU_INT_18V, 0);
	udelay(1);
	gpio_set_value(GPIO_AP_MCU_INT_18V, 1);

	return 0;
}

static int check_mcu_busy(void)
{
	pr_info("%s\n", __func__);
	return gpio_get_value(GPIO_MCU_AP_INT_1_18V);
}

static int check_mcu_ready(void)
{
	pr_info("%s\n", __func__);
	return gpio_get_value(GPIO_MCU_AP_INT_2_18V);
}

static int set_mcu_reset(int on)
{
	if (on == 0)
		gpio_set_value(GPIO_MCU_nRST_18V, 0);
	else
		gpio_set_value(GPIO_MCU_nRST_18V, 1);

	return 0;
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
	*acc = 6;
	*mag = 3;

	pr_info("%s, position acc : %d, mag = %d\n", __func__, *acc, *mag);
}
#endif

#ifdef CONFIG_SENSORS_SSP_STM
int initialize_ssp_spi_gpio(struct platform_device *dev)
{
	int gpio;

	s3c_gpio_cfgpin(GPIO_SHUB_SPI_SCK, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_SHUB_SPI_SCK, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_SHUB_SPI_MISO, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_SHUB_SPI_MISO, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_SHUB_SPI_MOSI, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_SHUB_SPI_MOSI, S3C_GPIO_PULL_NONE);

	for (gpio = GPIO_SHUB_SPI_SCK;
		gpio <= GPIO_SHUB_SPI_MOSI; gpio++)
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	return 0;
}

void __init ssp_spi0_set_platdata(struct s3c64xx_spi_info *pd,
				      int src_clk_nr, int num_cs)
{
	if (!pd) {
		pr_err("%s:Need to pass platform data\n", __func__);
		return;
	}

	/* Reject invalid configuration */
	if (!num_cs || src_clk_nr < 0) {
		pr_err("%s: Invalid SPI configuration\n", __func__);
		return;
	}

	pd->num_cs = num_cs;
	pd->src_clk_nr = src_clk_nr;
	if (!pd->cfg_gpio)
		pd->cfg_gpio = initialize_ssp_spi_gpio;

	if (pd->dma_mode != PIO_MODE)
		pd->dma_mode = HYBRID_MODE;

	s3c_set_platdata(pd, sizeof(*pd), &s3c64xx_device_spi0);
}

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
		.max_speed_hz		= 5 * 1000 * 1000,
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
#define GPIO_BTP_LDO_EN	EXYNOS5420_GPY6(5)

static int vfs61xx_regulator_onoff(int onoff)
{

	pr_info("%s: %s\n", __func__, onoff ? "on" : "off");
	if (system_rev > 2)
		gpio_set_value(GPIO_BTP_LDO_EN, onoff);

	return 0;
}

static void vfs61xx_setup_gpio(void)
{
	int ret;

	if (system_rev > 2) {
		ret = gpio_request(GPIO_BTP_LDO_EN, "BTP_LDO_EN");
		if (ret)
			pr_err("%s, failed to request AP_MCU_INT for SSP\n",
				__func__);
		s3c_gpio_cfgpin(GPIO_BTP_LDO_EN,
		S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
		s3c_gpio_setpull(GPIO_BTP_LDO_EN, S3C_GPIO_PULL_DOWN);

		if (system_rev == 4) {	/* SHCT1 shares LDO with VFS */
			pr_info("%s, LDO enable first on system_rev=4, REV0.3\n",
				__func__);
			gpio_set_value(GPIO_BTP_LDO_EN, 1);
		} else
			gpio_set_value(GPIO_BTP_LDO_EN, 0);
	}

	s3c_gpio_cfgpin(GPIO_BTP_IRQ, S3C_GPIO_SFN(S3C_GPIO_INPUT));
	s3c_gpio_setpull(GPIO_BTP_IRQ, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_BTP_RST_N, S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
	gpio_set_value(GPIO_BTP_RST_N, 0);
	s3c_gpio_setpull(GPIO_BTP_RST_N, S3C_GPIO_PULL_NONE);
}

static struct vfs61xx_platform_data vfs61xx_pdata = {
	.drdy = GPIO_BTP_IRQ,
	.sleep = GPIO_BTP_RST_N,
	.vfs61xx_sovcc_on = vfs61xx_regulator_onoff,
	.orient = 1,
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
		.irq			= IRQ_EINT(15),
		.controller_data	= &spi1_csi[0],
		.platform_data		= &vfs61xx_pdata,
	}
};
#endif

void __init exynos5_universal5420_sensor_init(void)
{
	int ret = 0;

	pr_info("%s, is called\n", __func__);

#ifdef CONFIG_SENSORS_SSP
	ret = initialize_ssp_gpio();
	if (ret < 0)
		pr_err("%s, initialize_ssp_gpio fail(err=%d)\n", __func__, ret);
#endif

#if defined(CONFIG_V1A) || defined(CONFIG_N1A)
	s3c_i2c3_set_platdata(NULL);
	ret = i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
	if (ret < 0) {
		pr_err("%s, i2c3 adding i2c fail(err=%d)\n", __func__, ret);
	}

	ret = platform_device_register(&s3c_device_i2c3);
	if (ret < 0)
		pr_err("%s, sensor platform device register failed (err=%d)\n",
			__func__, ret);

#endif

#if defined(CONFIG_V1A_3G) || defined(CONFIG_N1A_3G)
	exynos5_hs_i2c0_set_platdata(&hs_i2c0_data);
	ret = i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	if (ret < 0) {
		pr_err("%s, i2c4 adding i2c fail(err=%d)\n", __func__, ret);
	}

	ret = platform_device_register(&exynos5_device_hs_i2c0);
	if (ret < 0)
		pr_err("%s, grip platform device register failed (err=%d)\n",
			__func__, ret);
#endif


#ifdef CONFIG_SENSORS_SSP_STM
	pr_info("%s, SSP_SPI_SETUP\n", __func__);
	if (!exynos_spi_cfg_cs(spi0_csi[0].line, 0)) {
		pr_info("%s, spi0_set_platdata ...\n", __func__);
		ssp_spi0_set_platdata(&s3c64xx_spi0_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi0_csi));

		spi_register_board_info(spi0_board_info,
			ARRAY_SIZE(spi0_board_info));
	} else {
		pr_err("%s, Error requesting gpio for SPI-CH%d CS",
			__func__, spi0_board_info->bus_num);
	}

	ret = platform_device_register(&s3c64xx_device_spi0);
	if (ret < 0)
		pr_err("%s, Failed to register spi0 plaform devices(err=%d)\n",
			__func__, ret);
#endif

#ifdef CONFIG_SENSORS_VFS61XX
	pr_info("%s: SENSORS_VFS61XX init\n", __func__);
	vfs61xx_setup_gpio();
	s3c64xx_spi1_pdata.dma_mode = PIO_MODE;
	if (system_rev > 4)
		vfs61xx_pdata.ldocontrol = 1;

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

