#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <asm/system_info.h>
#include "board-universal5420.h"
#if defined(CONFIG_BCM2079X_NFC_I2C)
#include <linux/nfc/bcm2079x.h>
#endif


/* GPIO_LEVEL_NONE = 2, GPIO_LEVEL_LOW = 0 */
#if defined(CONFIG_BCM2079X_NFC_I2C)
static unsigned int nfc_gpio_table[][4] = {
	{GPIO_NFC_IRQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 1, S3C_GPIO_PULL_NONE},
};
#endif

static inline int nfc_setup_gpio(void)
{
	int ret = 0;
	int array_size = ARRAY_SIZE(nfc_gpio_table);
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = nfc_gpio_table[i][0];

		ret = s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(nfc_gpio_table[i][1]));
		if (ret < 0)
			pr_err("%s, s3c_gpio_cfgpin gpio(%d) fail(err = %d)\n",
				__func__, i, ret);

		ret = s3c_gpio_setpull(gpio, nfc_gpio_table[i][3]);
		if (ret < 0)
			pr_err("%s, s3c_gpio_setpull gpio(%d) fail(err = %d)\n",
				__func__, i, ret);

		if (nfc_gpio_table[i][2] != 2)
			gpio_set_value(gpio, nfc_gpio_table[i][2]);
	}

	return ret;
}

#if defined(CONFIG_BCM2079X_NFC_I2C)
static struct bcm2079x_platform_data bcm2079x_i2c_pdata = {
	.irq_gpio = GPIO_NFC_IRQ,
	.en_gpio = GPIO_NFC_EN,
	.wake_gpio = GPIO_NFC_FIRMWARE,
};
#endif

static struct i2c_board_info i2c_dev_nfc[] __initdata = {
#if defined(CONFIG_BCM2079X_NFC_I2C)
	{
		I2C_BOARD_INFO("bcm2079x-i2c", 0x77),
		.irq = IRQ_EINT(11),
		.platform_data = &bcm2079x_i2c_pdata,
	},
#endif
};

void __init exynos5_universal5420_nfc_init(void)
{
	int ret;

	pr_info("%s, is called\n", __func__);

	ret = nfc_setup_gpio();
	if (ret < 0)
		pr_err("%s, nfc_setup_gpio failed(err=%d)\n", __func__, ret);

	s3c_i2c3_set_platdata(NULL);
	ret = i2c_register_board_info(3, i2c_dev_nfc, ARRAY_SIZE(i2c_dev_nfc));
	if (ret < 0)
		pr_err("%s, failed to register_i2c3\n", __func__);

	ret = platform_device_register(&s3c_device_i2c3);
	if (ret < 0)
		pr_err("%s, NFC platform device register failed (err=%d)\n",
			__func__, ret);
}
