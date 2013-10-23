/*
 *  Validity finger print sensor driver
 *
 *  Copyright (C) 2012 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __VFS61XX_PLATFORM__H__
#define __VFS61XX_PLATFORM__H__

struct vfs61xx_platform_data {
	unsigned int drdy;
	unsigned int sleep;
	int (*vfs61xx_sovcc_on) (int);
};

#endif
