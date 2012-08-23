/*
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _EXYNOS_HDMI_H_
#define _EXYNOS_HDMI_H_

/* AVI header and aspect ratio */
#define HDMI_AVI_VERSION 		0x02
#define HDMI_AVI_LENGTH  		0x0d
#define AVI_PIC_ASPECT_RATIO_4_3 	(1 << 4)
#define AVI_PIC_ASPECT_RATIO_16_9 	(2 << 4)
#define AVI_SAME_AS_PIC_ASPECT_RATIO 	8

/* AUI header info */
#define HDMI_AUI_VERSION 		0x01
#define HDMI_AUI_LENGTH  		0x0a

/* HDMI infoframe to configure HDMI out packet header, AUI and AVI */
enum HDMI_PACKET_TYPE {
	/** refer to Table 5-8 Packet Type in HDMI specification v1.4a */
	/** InfoFrame packet type */
	HDMI_PACKET_TYPE_INFOFRAME = 0X80,
	/** Vendor-Specific InfoFrame */
	HDMI_PACKET_TYPE_VSI = HDMI_PACKET_TYPE_INFOFRAME + 1,
	/** Auxiliary Video information InfoFrame */
	HDMI_PACKET_TYPE_AVI = HDMI_PACKET_TYPE_INFOFRAME + 2,
	/** Audio information InfoFrame */
	HDMI_PACKET_TYPE_AUI = HDMI_PACKET_TYPE_INFOFRAME + 4
};

void hdmi_attach_ddc_client(struct i2c_client *ddc);
void hdmi_attach_hdmiphy_client(struct i2c_client *hdmiphy);

extern struct i2c_driver hdmiphy_driver;
extern struct i2c_driver ddc_driver;

#endif
