#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include "board-universal5410.h"
#include <asm/system_info.h>
#if defined(CONFIG_PN65N_NFC)
#include <linux/nfc/pn65n.h>
#elif defined(CONFIG_BCM2079X_NFC_I2C)
#include <linux/nfc/bcm2079x.h>
#endif

/* GPIO_LEVEL_NONE = 2, GPIO_LEVEL_LOW = 0 */
#if defined(CONFIG_PN65N_NFC)
static unsigned int nfc_gpio_table[][4] = {
	{GPIO_NFC_IRQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
};
#elif defined(CONFIG_BCM2079X_NFC_I2C)
static unsigned int nfc_gpio_table[][4] = {
	{GPIO_NFC_IRQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 1, S3C_GPIO_PULL_NONE},
};
#endif

static inline void nfc_setup_gpio(void)
{
	int err = 0;
	int array_size = ARRAY_SIZE(nfc_gpio_table);
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = nfc_gpio_table[i][0];

		err = s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(nfc_gpio_table[i][1]));
		if (err < 0)
			pr_err("%s, s3c_gpio_cfgpin gpio(%d) fail(err = %d)\n",
				__func__, i, err);

		err = s3c_gpio_setpull(gpio, nfc_gpio_table[i][3]);
		if (err < 0)
			pr_err("%s, s3c_gpio_setpull gpio(%d) fail(err = %d)\n",
				__func__, i, err);

		if (nfc_gpio_table[i][2] != 2)
			gpio_set_value(gpio, nfc_gpio_table[i][2]);
	}
}

#if defined(CONFIG_PN65N_NFC)
static struct pn65n_i2c_platform_data pn65n_pdata = {
	.irq_gpio = GPIO_NFC_IRQ,
	.ven_gpio = GPIO_NFC_EN,
	.firm_gpio = GPIO_NFC_FIRMWARE,
};
#elif defined(CONFIG_BCM2079X_NFC_I2C)
static struct bcm2079x_platform_data bcm2079x_i2c_pdata = {
	.irq_gpio = GPIO_NFC_IRQ,
	.en_gpio = GPIO_NFC_EN,
	.wake_gpio = GPIO_NFC_FIRMWARE,
};
#endif

static struct i2c_board_info i2c_dev_nfc[] __initdata = {
#if defined(CONFIG_PN65N_NFC)
	{
		I2C_BOARD_INFO("pn65n", 0x2b),
		.irq = IRQ_EINT(14),
		.platform_data = &pn65n_pdata,
	},
#elif defined(CONFIG_BCM2079X_NFC_I2C)
	{
		I2C_BOARD_INFO("bcm2079x-i2c", 0x77),
		.irq = IRQ_EINT(11),
		.platform_data = &bcm2079x_i2c_pdata,
	},
#endif
};

#if defined(CONFIG_MACH_HA)
static struct i2c_gpio_platform_data i2c_nfc_platdata = {
	.sda_pin		= GPIO_NFC_SDA_18V,
	.scl_pin		= GPIO_NFC_SCL_18V,
	.udelay		= 2,	/* 250 kHz */
};

static struct platform_device s3c_device_i2c13 = {
	.name		= "i2c-gpio",
	.id			= 13,
	.dev.platform_data	= &i2c_nfc_platdata,
};
#endif

void __init exynos5_universal5410_nfc_init(void)
{
#if defined(CONFIG_MACH_HA)
	int ret;
#endif

#ifdef CONFIG_UNIVERSAL5410_REV06
	if (system_rev < 9)
		return;
#endif
#ifdef CONFIG_UNIVERSAL5410_3G_REV06
	if (system_rev < 8)
		return;
#endif
	nfc_setup_gpio();

#if defined(CONFIG_MACH_HA)
	ret = i2c_register_board_info(13, i2c_dev_nfc, ARRAY_SIZE(i2c_dev_nfc));
	if (ret < 0) {
		pr_err("%s, i2c13 adding i2c fail(err=%d)\n", __func__, ret);
	}

	ret = platform_device_register(&s3c_device_i2c13);
	if (ret < 0) {
		pr_err("%s, i2c13 adding device(err=%d)\n", __func__, ret);
	}
#else
	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_dev_nfc, ARRAY_SIZE(i2c_dev_nfc));
	platform_device_register(&s3c_device_i2c3);
#endif
}
