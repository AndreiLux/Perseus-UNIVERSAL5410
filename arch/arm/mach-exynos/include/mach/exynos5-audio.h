/*
 * exynos5_audio.h - Audio Management
 *
 *  Copyright (C) 2012 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __EXYNOS5_AUDIO_H__
#define __EXYNOS5_AUDIO_H__ __FILE__


void exynos5_audio_init(void);
void exynos5_audio_set_mclk(bool enable, bool forced);


#endif /* __EXYNOS5_AUDIO_H__ */
