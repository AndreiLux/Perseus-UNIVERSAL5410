/*
 * OMAP4460+ SCM device file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: J Keerthy <j-keerthy@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <plat/omap_device.h>
#include "pm.h"
#include <plat/scm.h>

static DEFINE_IDR(scm_device_idr);

static int scm_dev_init(struct omap_hwmod *oh, void *user)
{
	struct	omap4plus_scm_pdata		*scm_pdata;
	struct	omap_device			*od;
	struct	omap4plus_scm_dev_attr	*scm_dev_attr;
	int					ret = 0;
	int					num;

	scm_pdata =
	    kzalloc(sizeof(*scm_pdata), GFP_KERNEL);
	if (!scm_pdata) {
		dev_err(&oh->od->pdev.dev,
			"Unable to allocate memory for scm pdata\n");
		return -ENOMEM;
	}

	ret = idr_pre_get(&scm_device_idr, GFP_KERNEL);
	if (ret < 0)
		goto fail_id;
	ret = idr_get_new(&scm_device_idr, scm_pdata, &num);
	if (ret < 0)
		goto fail_id;
	scm_dev_attr = oh->dev_attr;
	scm_pdata->cnt = scm_dev_attr->cnt;
	scm_pdata->rev = scm_dev_attr->rev;
	if (cpu_is_omap446x())
		scm_pdata->accurate = 1;

	od = omap_device_build("omap4plus_scm", num,
		oh, scm_pdata, sizeof(*scm_pdata), NULL, 0, false);

	if (IS_ERR(od)) {
		dev_warn(&oh->od->pdev.dev,
			"Could not build omap_device for %s\n", oh->name);
		ret = PTR_ERR(od);
	}

fail_id:
	kfree(scm_pdata);

	return ret;
}

static int __init omap4460plus_devinit_scm(void)
{
	if (!(cpu_is_omap446x() || cpu_is_omap443x() || cpu_is_omap543x()))
		return 0;

	return omap_hwmod_for_each_by_class("ctrl_module",
			scm_dev_init, NULL);
}

arch_initcall(omap4460plus_devinit_scm);
