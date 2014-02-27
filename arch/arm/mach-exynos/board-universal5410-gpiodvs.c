/*
 * Samsung Mobile VE Group.
 *
 * drivers/gpio/gpio_dvs/smdk4412_gpio_dvs.c - Read GPIO info. from SMDK4412
 *
 * Copyright (C) 2013, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>

#include <linux/secgpio_dvs.h>

static u32 exynos5_gpio_num[] = {
	EXYNOS5410_GPA0(0),
	EXYNOS5410_GPA0(1),
	EXYNOS5410_GPA0(2),
	EXYNOS5410_GPA0(3),
	EXYNOS5410_GPA0(4),
	EXYNOS5410_GPA0(5),
	EXYNOS5410_GPA0(6),
	EXYNOS5410_GPA0(7),
	EXYNOS5410_GPA1(0),
	EXYNOS5410_GPA1(1),
	EXYNOS5410_GPA1(2),
	EXYNOS5410_GPA1(3),
	EXYNOS5410_GPA1(4),
	EXYNOS5410_GPA1(5),
	EXYNOS5410_GPA2(0),
	EXYNOS5410_GPA2(1),
	EXYNOS5410_GPA2(2),
	EXYNOS5410_GPA2(3),
	EXYNOS5410_GPA2(4),
	EXYNOS5410_GPA2(5),
	EXYNOS5410_GPA2(6),
	EXYNOS5410_GPA2(7),

	EXYNOS5410_GPB0(0),
	EXYNOS5410_GPB0(1),
	EXYNOS5410_GPB0(2),
	EXYNOS5410_GPB0(3),
	EXYNOS5410_GPB0(4),
	EXYNOS5410_GPB1(0),
	EXYNOS5410_GPB1(1),
	EXYNOS5410_GPB1(2),
	EXYNOS5410_GPB1(3),
	EXYNOS5410_GPB1(4),
	EXYNOS5410_GPB2(0),
	EXYNOS5410_GPB2(1),
	EXYNOS5410_GPB2(2),
	EXYNOS5410_GPB2(3),
	EXYNOS5410_GPB3(0),
	EXYNOS5410_GPB3(1),
	EXYNOS5410_GPB3(2),
	EXYNOS5410_GPB3(3),

	EXYNOS5410_GPC0(0),
	EXYNOS5410_GPC0(1),
	EXYNOS5410_GPC0(2),
	EXYNOS5410_GPC0(3),
	EXYNOS5410_GPC0(4),
	EXYNOS5410_GPC0(5),
	EXYNOS5410_GPC0(6),
	EXYNOS5410_GPC3(0),
	EXYNOS5410_GPC3(1),
	EXYNOS5410_GPC3(2),
	EXYNOS5410_GPC3(3),
	EXYNOS5410_GPC1(0),
	EXYNOS5410_GPC1(1),
	EXYNOS5410_GPC1(2),
	EXYNOS5410_GPC1(3),
	EXYNOS5410_GPC1(4),
	EXYNOS5410_GPC1(5),
	EXYNOS5410_GPC1(6),
	EXYNOS5410_GPC2(0),
	EXYNOS5410_GPC2(1),
	EXYNOS5410_GPC2(2),
	EXYNOS5410_GPC2(3),
	EXYNOS5410_GPC2(4),
	EXYNOS5410_GPC2(5),
	EXYNOS5410_GPC2(6),

	EXYNOS5410_GPM5(0),
	EXYNOS5410_GPM5(1),

	EXYNOS5410_GPD1(0),
	EXYNOS5410_GPD1(1),
	EXYNOS5410_GPD1(2),
	EXYNOS5410_GPD1(3),
	EXYNOS5410_GPD1(4),
	EXYNOS5410_GPD1(5),
	EXYNOS5410_GPD1(6),
	EXYNOS5410_GPD1(7),

	EXYNOS5410_GPE0(0),
	EXYNOS5410_GPE0(1),
	EXYNOS5410_GPE0(2),
	EXYNOS5410_GPE0(3),
	EXYNOS5410_GPE0(4),
	EXYNOS5410_GPE0(5),
	EXYNOS5410_GPE0(6),
	EXYNOS5410_GPE0(7),
	EXYNOS5410_GPE1(0),
	EXYNOS5410_GPE1(1),

	EXYNOS5410_GPF0(0),
	EXYNOS5410_GPF0(1),
	EXYNOS5410_GPF0(2),
	EXYNOS5410_GPF0(3),
	EXYNOS5410_GPF0(4),
	EXYNOS5410_GPF0(5),
	EXYNOS5410_GPF1(0),
	EXYNOS5410_GPF1(1),
	EXYNOS5410_GPF1(2),
	EXYNOS5410_GPF1(3),
	EXYNOS5410_GPF1(4),
	EXYNOS5410_GPF1(5),
	EXYNOS5410_GPF1(6),
	EXYNOS5410_GPF1(7),

	EXYNOS5410_GPG0(0),
	EXYNOS5410_GPG0(1),
	EXYNOS5410_GPG0(2),
	EXYNOS5410_GPG0(3),
	EXYNOS5410_GPG0(4),
	EXYNOS5410_GPG0(5),
	EXYNOS5410_GPG0(6),
	EXYNOS5410_GPG0(7),
	EXYNOS5410_GPG1(0),
	EXYNOS5410_GPG1(1),
	EXYNOS5410_GPG1(2),
	EXYNOS5410_GPG1(3),
	EXYNOS5410_GPG1(4),
	EXYNOS5410_GPG1(5),
	EXYNOS5410_GPG1(6),
	EXYNOS5410_GPG1(7),
	EXYNOS5410_GPG2(0),
	EXYNOS5410_GPG2(1),

	EXYNOS5410_GPH0(0),
	EXYNOS5410_GPH0(1),
	EXYNOS5410_GPH0(2),
	EXYNOS5410_GPH0(3),
	EXYNOS5410_GPH1(0),
	EXYNOS5410_GPH1(1),
	EXYNOS5410_GPH1(2),
	EXYNOS5410_GPH1(3),
	EXYNOS5410_GPH1(4),
	EXYNOS5410_GPH1(5),
	EXYNOS5410_GPH1(6),
	EXYNOS5410_GPH1(7),

	EXYNOS5410_GPM7(0),
	EXYNOS5410_GPM7(1),
	EXYNOS5410_GPM7(2),
	EXYNOS5410_GPM7(3),
	EXYNOS5410_GPM7(4),
	EXYNOS5410_GPM7(5),
	EXYNOS5410_GPM7(6),
	EXYNOS5410_GPM7(7),

	EXYNOS5410_GPY0(0),
	EXYNOS5410_GPY0(1),
	EXYNOS5410_GPY0(2),
	EXYNOS5410_GPY0(3),
	EXYNOS5410_GPY0(4),
	EXYNOS5410_GPY0(5),
	EXYNOS5410_GPY1(0),
	EXYNOS5410_GPY1(1),
	EXYNOS5410_GPY1(2),
	EXYNOS5410_GPY1(3),
	EXYNOS5410_GPY2(0),
	EXYNOS5410_GPY2(1),
	EXYNOS5410_GPY2(2),
	EXYNOS5410_GPY2(3),
	EXYNOS5410_GPY2(4),
	EXYNOS5410_GPY2(5),
	EXYNOS5410_GPY3(0),
	EXYNOS5410_GPY3(1),
	EXYNOS5410_GPY3(2),
	EXYNOS5410_GPY3(3),
	EXYNOS5410_GPY3(4),
	EXYNOS5410_GPY3(5),
	EXYNOS5410_GPY3(6),
	EXYNOS5410_GPY3(7),
	EXYNOS5410_GPY4(0),
	EXYNOS5410_GPY4(1),
	EXYNOS5410_GPY4(2),
	EXYNOS5410_GPY4(3),
	EXYNOS5410_GPY4(4),
	EXYNOS5410_GPY4(5),
	EXYNOS5410_GPY4(6),
	EXYNOS5410_GPY4(7),
	EXYNOS5410_GPY5(0),
	EXYNOS5410_GPY5(1),
	EXYNOS5410_GPY5(2),
	EXYNOS5410_GPY5(3),
	EXYNOS5410_GPY5(4),
	EXYNOS5410_GPY5(5),
	EXYNOS5410_GPY5(6),
	EXYNOS5410_GPY5(7),
	EXYNOS5410_GPY6(0),
	EXYNOS5410_GPY6(1),
	EXYNOS5410_GPY6(2),
	EXYNOS5410_GPY6(3),
	EXYNOS5410_GPY6(4),
	EXYNOS5410_GPY6(5),
	EXYNOS5410_GPY6(6),
	EXYNOS5410_GPY6(7),
	EXYNOS5410_GPY7(0),
	EXYNOS5410_GPY7(1),
	EXYNOS5410_GPY7(2),
	EXYNOS5410_GPY7(3),
	EXYNOS5410_GPY7(4),
	EXYNOS5410_GPY7(5),
	EXYNOS5410_GPY7(6),
	EXYNOS5410_GPY7(7),

	EXYNOS5410_GPX0(0),
	EXYNOS5410_GPX0(1),
	EXYNOS5410_GPX0(2),
	EXYNOS5410_GPX0(3),
	EXYNOS5410_GPX0(4),
	EXYNOS5410_GPX0(5),
	EXYNOS5410_GPX0(6),
	EXYNOS5410_GPX0(7),
	EXYNOS5410_GPX1(0),
	EXYNOS5410_GPX1(1),
	EXYNOS5410_GPX1(2),
	EXYNOS5410_GPX1(3),
	EXYNOS5410_GPX1(4),
	EXYNOS5410_GPX1(5),
	EXYNOS5410_GPX1(6),
	EXYNOS5410_GPX1(7),
	EXYNOS5410_GPX2(0),
	EXYNOS5410_GPX2(1),
	EXYNOS5410_GPX2(2),
	EXYNOS5410_GPX2(3),
	EXYNOS5410_GPX2(4),
	EXYNOS5410_GPX2(5),
	EXYNOS5410_GPX2(6),
	EXYNOS5410_GPX2(7),
	EXYNOS5410_GPX3(0),
	EXYNOS5410_GPX3(1),
	EXYNOS5410_GPX3(2),
	EXYNOS5410_GPX3(3),
	EXYNOS5410_GPX3(4),
	EXYNOS5410_GPX3(5),
	EXYNOS5410_GPX3(6),
	EXYNOS5410_GPX3(7),

	EXYNOS5410_GPV0(0),
	EXYNOS5410_GPV0(1),
	EXYNOS5410_GPV0(2),
	EXYNOS5410_GPV0(3),
	EXYNOS5410_GPV0(4),
	EXYNOS5410_GPV0(5),
	EXYNOS5410_GPV0(6),
	EXYNOS5410_GPV0(7),
	EXYNOS5410_GPV1(0),
	EXYNOS5410_GPV1(1),
	EXYNOS5410_GPV1(2),
	EXYNOS5410_GPV1(3),
	EXYNOS5410_GPV1(4),
	EXYNOS5410_GPV1(5),
	EXYNOS5410_GPV1(6),
	EXYNOS5410_GPV1(7),
	EXYNOS5410_GPV2(0),
	EXYNOS5410_GPV2(1),
	EXYNOS5410_GPV2(2),
	EXYNOS5410_GPV2(3),
	EXYNOS5410_GPV2(4),
	EXYNOS5410_GPV2(5),
	EXYNOS5410_GPV2(6),
	EXYNOS5410_GPV2(7),
	EXYNOS5410_GPV3(0),
	EXYNOS5410_GPV3(1),
	EXYNOS5410_GPV3(2),
	EXYNOS5410_GPV3(3),
	EXYNOS5410_GPV3(4),
	EXYNOS5410_GPV3(5),
	EXYNOS5410_GPV3(6),
	EXYNOS5410_GPV3(7),
	EXYNOS5410_GPV4(0),
	EXYNOS5410_GPV4(1),

	EXYNOS5410_GPJ0(0),
	EXYNOS5410_GPJ0(1),
	EXYNOS5410_GPJ0(2),
	EXYNOS5410_GPJ0(3),
	EXYNOS5410_GPJ0(4),
	EXYNOS5410_GPJ1(0),
	EXYNOS5410_GPJ1(1),
	EXYNOS5410_GPJ1(2),
	EXYNOS5410_GPJ1(3),
	EXYNOS5410_GPJ1(4),
	EXYNOS5410_GPJ1(5),
	EXYNOS5410_GPJ1(6),
	EXYNOS5410_GPJ1(7),
	EXYNOS5410_GPJ2(0),
	EXYNOS5410_GPJ2(1),
	EXYNOS5410_GPJ2(2),
	EXYNOS5410_GPJ2(3),
	EXYNOS5410_GPJ2(4),
	EXYNOS5410_GPJ2(5),
	EXYNOS5410_GPJ2(6),
	EXYNOS5410_GPJ2(7),
	EXYNOS5410_GPJ3(0),
	EXYNOS5410_GPJ3(1),
	EXYNOS5410_GPJ3(2),
	EXYNOS5410_GPJ3(3),
	EXYNOS5410_GPJ3(4),
	EXYNOS5410_GPJ3(5),
	EXYNOS5410_GPJ3(6),
	EXYNOS5410_GPJ3(7),
	EXYNOS5410_GPJ4(0),
	EXYNOS5410_GPJ4(1),

	EXYNOS5410_GPK0(0),
	EXYNOS5410_GPK0(1),
	EXYNOS5410_GPK0(2),
	EXYNOS5410_GPK0(3),
	EXYNOS5410_GPK0(4),
	EXYNOS5410_GPK0(5),
	EXYNOS5410_GPK0(6),
	EXYNOS5410_GPK0(7),
	EXYNOS5410_GPK1(0),
	EXYNOS5410_GPK1(1),
	EXYNOS5410_GPK1(2),
	EXYNOS5410_GPK1(3),
	EXYNOS5410_GPK1(4),
	EXYNOS5410_GPK1(5),
	EXYNOS5410_GPK1(6),
	EXYNOS5410_GPK1(7),
	EXYNOS5410_GPK2(0),
	EXYNOS5410_GPK2(1),
	EXYNOS5410_GPK2(2),
	EXYNOS5410_GPK2(3),
	EXYNOS5410_GPK2(4),
	EXYNOS5410_GPK2(5),
	EXYNOS5410_GPK2(6),
	EXYNOS5410_GPK2(7),
	EXYNOS5410_GPK3(0),
	EXYNOS5410_GPK3(1),
	EXYNOS5410_GPK3(2),
	EXYNOS5410_GPK3(3),
	EXYNOS5410_GPK3(4),
	EXYNOS5410_GPK3(5),
	EXYNOS5410_GPK3(6),
};

#define GET_RESULT_GPIO(a, b, c)	\
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))

/****************************************************************/
/* Define value in accordance with
	the specification of each BB vendor. */
#define AP_GPIO_COUNT (sizeof(exynos5_gpio_num)/sizeof(u32))
/****************************************************************/


/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result_t gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};

#ifdef SECGPIO_SLEEP_DEBUGGING
static struct sleepdebug_gpiotable sleepdebug_table;
#endif

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void check_gpio_status(unsigned char phonestate)
{
	u32 i, gpio;
	u32 cfg, val, pud;

	u8 temp_io = 0, temp_pdpu = 0, temp_lh = 0;

	pr_info("[GPIO_DVS][%s] state : %s\n", __func__,
		(phonestate == PHONE_INIT) ? "init" : "sleep");

	for (i = 0; i < AP_GPIO_COUNT; i++) {
		gpio = exynos5_gpio_num[i];

		if (phonestate == PHONE_INIT ||
			((gpio >= EXYNOS5410_GPX0(0)) &&
				(gpio <= EXYNOS5410_GPX3(7)))) {

			cfg = s3c_gpio_getcfg(gpio);
			if (cfg == S3C_GPIO_INPUT)
				temp_io = 0x01;	/* GPIO_IN */
			else if (cfg == S3C_GPIO_OUTPUT)
				temp_io = 0x02;	/* GPIO_OUT */
			else if (cfg == S3C_GPIO_SFN(0xF))
				temp_io = 0x03;	/* INT */
			else
				temp_io = 0x0;		/* FUNC */

			pud = s3c_gpio_getpull(gpio);

		} else {		/* if(phonestate == PHONE_SLEEP) { */
			cfg = s5p_gpio_get_pd_cfg(gpio);

			if (cfg == S5P_GPIO_PD_INPUT)
				temp_io = 0x01;	/* GPIO_IN */
			else if (cfg == S5P_GPIO_PD_OUTPUT0 ||
					cfg ==  S5P_GPIO_PD_OUTPUT1)
				temp_io = 0x02;	/* GPIO_OUT */
			else if (cfg == S5P_GPIO_PD_PREV_STATE)
				temp_io = 0x04;	/* PREV */
			else
				temp_io = 0xF;	/* not alloc. */

			pud = s5p_gpio_get_pd_pull(gpio);
		}

		if (phonestate == PHONE_INIT ||
			((gpio >= EXYNOS5410_GPX0(0)) &&
				(gpio <= EXYNOS5410_GPX3(7)))) {

			if (temp_io == 0x0)	/* FUNC */
				val = 0;
			else
				val = gpio_get_value(gpio);

		} else {
			if (cfg == S5P_GPIO_PD_OUTPUT0)
				val = 0;
			else if (cfg == S5P_GPIO_PD_OUTPUT1)
				val = 1;
			else
				val = 0;
		}

		if (pud == S3C_GPIO_PULL_NONE ||
				pud == S5P_GPIO_PD_UPDOWN_DISABLE)
			temp_pdpu = 0x00;
		else if (pud == S3C_GPIO_PULL_DOWN ||
				pud == S5P_GPIO_PD_DOWN_ENABLE)
			temp_pdpu = 0x01;
		else if (pud == S3C_GPIO_PULL_UP ||
				pud == S5P_GPIO_PD_UP_ENABLE)
			temp_pdpu = 0x02;
		else {
			temp_pdpu = 0x07;
			pr_err("[GPIO_DVS] i : %d, gpio : %d, pud : %d\n",
					i, gpio, pud);
		}

		if (val == 0)
			temp_lh = 0x00;
		else if (val == 1)
			temp_lh = 0x01;
		else
			pr_err("[GPIO_DVS] i : %d, gpio : %d, val : %d\n",
					i, gpio, val);

		checkgpiomap_result[phonestate][i] =
			GET_RESULT_GPIO(temp_io, temp_pdpu, temp_lh);
	}

	pr_info("[GPIO_DVS][%s] --\n", __func__);

	return;
}
/****************************************************************/
void set_gpio_pdpu(int gpionum, unsigned long temp_gpio_reg)
{
	int gpio;
	gpio = exynos5_gpio_num[gpionum];
	if ((gpio >= EXYNOS5410_GPX0(0)) && (gpio <= EXYNOS5410_GPX3(7)))
		s3c_gpio_setpull(gpio, temp_gpio_reg);
	else
		s5p_gpio_set_pd_pull(gpio, temp_gpio_reg);
}

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
void setgpio_for_sleepdebug(int gpionum, unsigned char io_pdpu_lh)
{
	unsigned char temp_io, temp_pdpu, temp_lh;
	int temp_data = io_pdpu_lh;
	int gpio;

	temp_io = (0xF0 & io_pdpu_lh) >> 4;
	temp_pdpu = (0x0E & io_pdpu_lh) >> 1;
	temp_lh = 0x01 & io_pdpu_lh;


	pr_info("[GPIO_DVS][%s] gpionum=%d, io_pdpu_lh=%d\n",
		__func__, gpionum, io_pdpu_lh);

	gpio = exynos5_gpio_num[gpionum];

	/* in case of 'INPUT', set PD/PU */
	if (temp_io == 0x01) {
		/* 0x0:NP, 0x1:PD, 0x2:PU */
		if ((gpio >= EXYNOS5410_GPX0(0)) &&
				(gpio <= EXYNOS5410_GPX3(7))) {
			if (temp_pdpu == 0x0)
				temp_data = S3C_GPIO_PULL_NONE;
			else if (temp_pdpu == 0x1)
				temp_data = S3C_GPIO_PULL_DOWN;
			else if (temp_pdpu == 0x2)
				temp_data = S3C_GPIO_PULL_UP;
		} else {
			if (temp_pdpu == 0x0)
				temp_data = S5P_GPIO_PD_UPDOWN_DISABLE;
			else if (temp_pdpu == 0x1)
				temp_data = S5P_GPIO_PD_DOWN_ENABLE;
			else if (temp_pdpu == 0x2)
				temp_data = S5P_GPIO_PD_UP_ENABLE;
		}

		set_gpio_pdpu(gpionum, temp_data);
	}
	/* in case of 'OUTPUT', set L/H */
	else if (temp_io == 0x02) {
		pr_info("[GPIO_DVS][%s] %d gpio set %d\n",
			__func__, gpionum, temp_lh);
		if ((gpio >= EXYNOS5410_GPX0(0)) && (gpio <= EXYNOS5410_GPX3(7)))
			gpio_set_value(gpio, temp_lh);
		else {
			if (temp_lh == 0x0)
				s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
			else
				s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT1);
		}
	}


}
/****************************************************************/

#ifdef SECGPIO_SLEEP_DEBUGGING
/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void undo_sleepgpio(void)
{
	int i;
	int gpio_num;

	pr_info("[GPIO_DVS][%s] ++\n", __func__);

	for (i = 0; i < sleepdebug_table.gpio_count; i++) {
		gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;
		/*
		 * << Caution >>
		 * If it's necessary,
		 * change the following function to another appropriate one
		 * or delete it
		 */
		//setgpio_for_sleepdebug(gpio_num, gpiomap_result.sleep[gpio_num]);
	}

	pr_info("[GPIO_DVS][%s] --\n", __func__);
	return;
}
/****************************************************************/

/********************* Fixed Code Area !***************************/
static void set_sleepgpio(void)
{
	int i;
	int gpio_num;
	unsigned char set_data;

	pr_info("[GPIO_DVS][%s] ++, cnt=%d\n",
		__func__, sleepdebug_table.gpio_count);

	for (i = 0; i < sleepdebug_table.gpio_count; i++) {
		pr_info("[GPIO_DVS][%d] gpio_num(%d), io(%d), pupd(%d), lh(%d)\n",
			i, sleepdebug_table.gpioinfo[i].gpio_num,
			sleepdebug_table.gpioinfo[i].io,
			sleepdebug_table.gpioinfo[i].pupd,
			sleepdebug_table.gpioinfo[i].lh);

		set_data = GET_RESULT_GPIO(
			sleepdebug_table.gpioinfo[i].io,
			sleepdebug_table.gpioinfo[i].pupd,
			sleepdebug_table.gpioinfo[i].lh);

		gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;
		setgpio_for_sleepdebug(gpio_num, set_data);
	}

	pr_info("[GPIO_DVS][%s] --\n", __func__);
	return;
}
#endif /* SECGPIO_SLEEP_DEBUGGING */

static struct gpio_dvs_t gpio_dvs = {
	.result = &gpiomap_result,
	.check_gpio_status = check_gpio_status,
	.count = AP_GPIO_COUNT,
#ifdef SECGPIO_SLEEP_DEBUGGING
	.sdebugtable = &sleepdebug_table,
	.set_sleepgpio = set_sleepgpio,
	.undo_sleepgpio = undo_sleepgpio,
#endif
};

static struct platform_device secgpio_dvs_device = {
	.name	= "secgpio_dvs",
	.id		= -1,
	.dev.platform_data = &gpio_dvs,
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	return platform_add_devices(
		secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
/* For compatibility of IORA Init Breakpoint, I have modified exceptively the following function. */
/* arch_initcall(secgpio_dvs_device_init); */
postcore_initcall(secgpio_dvs_device_init);
/****************************************************************/
