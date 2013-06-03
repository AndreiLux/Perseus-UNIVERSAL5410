/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __ARM_MACH_EXYNOS_USB3_DRD_H
#define __ARM_MACH_EXYNOS_USB3_DRD_H __FILE__

#include <linux/platform_device.h>
#include <linux/platform_data/dwc3-exynos.h>

extern struct platform_device exynos5_device_usb3_drd0;
extern struct platform_device exynos5_device_usb3_drd1;
#ifdef CONFIG_USB_HOST_NOTIFY
extern struct platform_device host_notifier_device;
#endif

extern void exynos5_usb3_drd0_set_platdata(struct dwc3_exynos_data *pd);
extern void exynos5_usb3_drd1_set_platdata(struct dwc3_exynos_data *pd);
extern int exynos_drd_switch_id_event(struct platform_device *pdev, int state);
extern int exynos_drd_switch_vbus_event(struct platform_device *pdev,
		bool vbus_active);

#endif /* __ARM_MACH_EXYNOS_USB3_DRD_H */
