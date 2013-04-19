/*
*
* drivers/media/isdbtmm/tuner_drv_wrap.c
*
* MM Tuner Driver
*
* Copyright (C) (2013, Samsung Electronics)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

/******************************************************************************
 * include
 ******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include "tuner_drv.h"
#include <asm/system_info.h>

#ifdef TUNER_CONFIG_IRQ_PC_LINUX
#include "../../../i2c-parport-x/i2c-parport.h"
#endif  /* TUNER_CONFIG_IRQ_PC_LINUX */

/******************************************************************************/
/* function                                                                   */
/******************************************************************************/
int tuner_drv_ctl_power( int data );
int tuner_drv_set_interrupt( void );
void tuner_drv_release_interrupt( void );
void tuner_drv_gpio_config_poweron( void );
void tuner_drv_gpio_config_poweroff( void );

/******************************************************************************
 * code area
 ******************************************************************************/
/******************************************************************************
 *    function:   tuner_drv_ctl_power
 *    brief   :   power control of a driver
 *    date    :   2011.08.26
 *    author  :   M.Takahashi(*)
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   data                 setting data
 *    output  :   none
 ******************************************************************************/
int tuner_drv_ctl_power( int data )
{	
	/* power on */
	if( data == TUNER_DRV_CTL_POWON )
	{
		DEBUG_PRINT("tuner_drv_ctl_power poweron\n");
		
		/* poweron gpio config setting */
		tuner_drv_gpio_config_poweron();
		
		/* TMM_PWR_EN high */
		gpio_set_value(GPIO_TMM_PWR_EN, GPIO_LEVEL_HIGH);
		
		/* 15ms sleep */
		usleep_range(15000, 15000);
		
		/* TMM_RST high */
		gpio_set_value(GPIO_TMM_RST, GPIO_LEVEL_HIGH);
		
		/* 2ms sleep */
		usleep_range(2000, 2000);
	}
	/* power off */
	else
	{
		DEBUG_PRINT("tuner_drv_ctl_power poweroff\n");
		
		/* poweroff gpio config setting */
		tuner_drv_gpio_config_poweroff();
		
		/* 1ms sleep */
		usleep_range(1000, 1000);
		
		/* TMM_RST low */
		gpio_set_value(GPIO_TMM_RST, GPIO_LEVEL_LOW);
		
		/* 2ms sleep */
		usleep_range(2000, 2000);
		
		/* TMM_PWR_EN low */
		gpio_set_value(GPIO_TMM_PWR_EN, GPIO_LEVEL_LOW);
	}

	return 0;
}

/******************************************************************************
 *    function:   tuner_drv_set_interrupt
 *    brief   :   interruption registration control of a driver
 *    date    :   2011.08.26
 *    author  :   M.Takahashi(*)
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   pdev
 *    output  :   none
 ******************************************************************************/
int tuner_drv_set_interrupt( void )
{
#ifndef TUNER_CONFIG_IRQ_PC_LINUX
    int ret;
	
	if(system_rev == 0x01)
	{
		/* HW-REV-0.3(0x01) */
		ret = request_irq( GPIO_TMM_INT_REV03,
                       tuner_interrupt,
                       IRQF_DISABLED | IRQF_TRIGGER_RISING,
                       "mm_tuner",
                       NULL );
	}
	else
	{
		/* HW-REV-0.0(0x0b), HW-REV-0.1(0x09) */
		ret = request_irq( GPIO_TMM_INT_REV00,
						   tuner_interrupt,
						   IRQF_DISABLED | IRQF_TRIGGER_RISING,
						   "mm_tuner",
						   NULL );
	}

    if( ret != 0 )
    {
        return -1;
    }
#else  /* TUNER_CONFIG_IRQ_PC_LINUX */
    i2c_set_interrupt( tuner_interrupt );
#endif /* TUNER_CONFIG_IRQ_PC_LINUX */
    return 0;
}

/******************************************************************************
 *    function:   tuner_drv_release_interrupt
 *    brief   :   interruption registration release control of a driver
 *    date    :   2011.08.26
 *    author  :   M.Takahashi(*)
 *
 *    return  :   none
 *    input   :   none
 *    output  :   none
 ******************************************************************************/
void tuner_drv_release_interrupt( void )
{
#ifndef TUNER_CONFIG_IRQ_PC_LINUX
	if(system_rev == 0x01)
	{
		/* HW-REV-0.3(0x01) */
		free_irq( GPIO_TMM_INT_REV03, NULL );
	}
	else
	{
		/* HW-REV-0.0(0x0b), HW-REV-0.1(0x09) */
		free_irq( GPIO_TMM_INT_REV00, NULL );
	}
#else  /* TUNER_CONFIG_IRQ_PC_LINUX */
    i2c_release_interrupt( NULL );
#endif /* TUNER_CONFIG_IRQ_PC_LINUX */
}

#ifdef TUNER_CONFIG_IRQ_LEVELTRIGGER
/******************************************************************************
 *    function:   tuner_drv_enable_interrupt
 *    brief   :   interruption registration enable control of a driver
 *    date    :   2011.09.18
 *    author  :   M.Takahashi(*)(*)
 *
 *    return  :   none
 *    input   :   none
 *    output  :   none
 ******************************************************************************/
void tuner_drv_enable_interrupt( void )
{
#ifndef TUNER_CONFIG_IRQ_PC_LINUX
	if(system_rev == 0x01)
	{
		/* HW-REV-0.3(0x01) */
		enable_irq( GPIO_TMM_INT_REV03)
	}
	else
	{
		/* HW-REV-0.0(0x0b), HW-REV-0.1(0x09) */
		enable_irq( GPIO_TMM_INT_REV00)
	}
#else  /* TUNER_CONFIG_IRQ_PC_LINUX */
    i2c_set_interrupt( tuner_interrupt );
#endif /* TUNER_CONFIG_IRQ_PC_LINUX */
}

/******************************************************************************
 *    function:   tuner_drv_disable_interrupt
 *    brief   :   interruption registration disable control of a driver
 *    date    :   2011.09.18
 *    author  :   M.Takahashi(*)(*)
 *
 *    return  :   none
 *    input   :   none
 *    output  :   none
 ******************************************************************************/
void tuner_drv_disable_interrupt( void )
{
#ifndef TUNER_CONFIG_IRQ_PC_LINUX
	if(system_rev == 0x01)
	{
		/* HW-REV-0.3(0x01) */
		disable_irq( GPIO_TMM_INT_REV03);
	}
	else
	{
		/* HW-REV-0.0(0x0b), HW-REV-0.1(0x09) */
		disable_irq( GPIO_TMM_INT_REV00);
	}
#else  /* TUNER_CONFIG_IRQ_PC_LINUX */
    i2c_release_interrupt( NULL );
#endif /* TUNER_CONFIG_IRQ_PC_LINUX */
}
#endif /* TUNER_CONFIG_IRQ_LEVELTRIGGER */

/******************************************************************************
 *    function:   tuner_drv_gpio_config_poweron
 *    brief   :   interruption registration disable control of a driver
 *    date    :   2012.12.18
 *    author  :   K.Matsumaru(*)(*)
 *
 *    return  :   none
 *    input   :   none
 *    output  :   none
 ******************************************************************************/
void tuner_drv_gpio_config_poweron( void )
{
	if(system_rev == 0x01)
	{
		/* HW-REV-0.3(0x01) */
		s3c_gpio_cfgpin(GPIO_TMM_INT_REV03, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TMM_INT_REV03, S3C_GPIO_PULL_NONE);
	}
	else
	{
		/* HW-REV-0.0(0x0b), HW-REV-0.1(0x09) */
		s3c_gpio_cfgpin(GPIO_TMM_INT_REV00, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TMM_INT_REV00, S3C_GPIO_PULL_NONE);
	}
	s3c_gpio_cfgpin(GPIO_TMM_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TMM_RST, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TMM_RST, GPIO_LEVEL_LOW);

	s3c_gpio_cfgpin(GPIO_TMM_PWR_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TMM_PWR_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TMM_PWR_EN, GPIO_LEVEL_LOW);
	
	s3c_gpio_cfgpin(GPIO_LTE_SPI_CLK, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_CLK, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_LTE_SPI_MISO, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_MISO, S3C_GPIO_PULL_NONE);
	
	s3c_gpio_cfgpin(GPIO_LTE_SPI_CSN, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_CSN, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_LTE_SPI_MOSI, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_MOSI, S3C_GPIO_PULL_NONE);
}

/******************************************************************************
 *    function:   tuner_drv_gpio_config_poweroff
 *    brief   :   interruption registration disable control of a driver
 *    date    :   2012.12.18
 *    author  :   K.Matsumaru(*)(*)
 *
 *    return  :   none
 *    input   :   none
 *    output  :   none
 ******************************************************************************/
void tuner_drv_gpio_config_poweroff( void )
{
	if(system_rev == 0x01)
	{
		/* HW-REV-0.3(0x01) */
		s3c_gpio_cfgpin(GPIO_TMM_INT_REV03, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TMM_INT_REV03, S3C_GPIO_PULL_NONE);
	}
	else
	{
		/* HW-REV-0.0(0x0b), HW-REV-0.1(0x09) */
		s3c_gpio_cfgpin(GPIO_TMM_INT_REV00, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TMM_INT_REV00, S3C_GPIO_PULL_NONE);
	}
	s3c_gpio_cfgpin(GPIO_TMM_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TMM_RST, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TMM_RST, GPIO_LEVEL_LOW);

	s3c_gpio_cfgpin(GPIO_TMM_PWR_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TMM_PWR_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TMM_PWR_EN, GPIO_LEVEL_LOW);
	
	s3c_gpio_cfgpin(GPIO_LTE_SPI_CLK, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_CLK, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(GPIO_LTE_SPI_MISO, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_MISO, S3C_GPIO_PULL_DOWN);
	
	s3c_gpio_cfgpin(GPIO_LTE_SPI_CSN, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_CSN, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(GPIO_LTE_SPI_MOSI, GPIO_SPI_SFN);
	s3c_gpio_setpull(GPIO_LTE_SPI_MOSI, S3C_GPIO_PULL_DOWN);
}

