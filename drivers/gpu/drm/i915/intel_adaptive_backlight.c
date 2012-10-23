/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_drv.h"
#include "intel_fixedpoint.h"

/*
 * Some notes about the adaptive backlight implementation:
 * - If we let it run as designed, it will generate a lot of interrupts which
 *   tends to wake the CPU up and waste power. This is a bad idea for a power
 *   saving feature. Instead, we couple it to the vblank interrupt since that
 *   means we drew something. This means that we do not react to non-vsynced
 *   GL updates, or updates to the front buffer, and also adds a little bit of
 *   extra latency. But it is an acceptable tradeoff to make.
 * - Ivy bridge has a hardware issue where the color correction doesn't seem
 *   to work. When you enable the ENH_MODIF_TBL_ENABLE bit, not only does the
 *   correction not work, but it becomes impossible to read the levels.
 *   Instead, as a workaround, we don't set that bit on ivy bridge and
 *   (ab)use the gamma ramp registers to do the correction.
 */

/*
 * This function takes a histogram of buckets as input and determines an
 * acceptable target backlight level.
 */
static int histogram_find_correction_level(int *levels)
{
	int i, sum = 0;
	int ratio, distortion, prev_distortion = 0, off, final_ratio, target;

	for (i = 0; i < ENH_NUM_BINS; i++)
		sum += levels[i];

	/* Allow 0.33/256 distortion per pixel, on average */
	target = sum / 3;

	/* Special case where we only have less than 100 pixels
	 * outside of the darkest bin.
	 */
	if (sum - levels[0] <= 100)
		return 70;

	for (ratio = ENH_NUM_BINS - 1; ratio >= 0 ; ratio--) {
		distortion = 0;
		for (i = ratio; i < ENH_NUM_BINS; i++) {
			int pixel_distortion = (i - ratio)*8;
			int num_pixels = levels[i];
			distortion += num_pixels * pixel_distortion;
		}
		if (distortion > target)
			break;
		else
			prev_distortion = distortion;
	}

	ratio++;

	/* If we're not exactly at the border between two buckets, extrapolate
	 * to get 3 extra bits of accuracy.
	 */
	if (distortion - prev_distortion)
		off = 8 * (target - prev_distortion) /
		      (distortion - prev_distortion);
	else
		off = 0;

	final_ratio = ratio * 255 / 31 + off;

	if (final_ratio > 255)
		final_ratio = 255;

	/* Never aim for less than 50% of the total backlight */
	if (final_ratio < 128)
		final_ratio = 128;

	return final_ratio;
}

static void get_levels(struct drm_device *dev, int pipe, int *levels)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < ENH_NUM_BINS; i++) {
		u32 hist_ctl = ENH_HIST_ENABLE |
			       ENH_MODIF_TBL_ENABLE |
			       ENH_PIPE(pipe) |
			       HIST_MODE_YUV |
			       ENH_MODE_ADDITIVE |
			       i;

		/* Ivb workaround, see the explanation at the top */
		if (INTEL_INFO(dev)->gen == 7)
			hist_ctl &= ~ENH_MODIF_TBL_ENABLE;

		I915_WRITE(BLM_HIST_CTL, hist_ctl);

		levels[i] = I915_READ(BLM_HIST_ENH);
	}
}

/* Multiplier is 16.16 fixed point */
static void set_levels(struct drm_device *dev, int pipe, int multiplier)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	if (INTEL_INFO(dev)->gen == 7) {
		/* Ivb workaround, see the explanation at the top */
		for (i = 0; i < 256; i++) {
			int v = intel_fixed_div(i, multiplier);
			if (v > 255)
				v = 255;
			v = v | (v << 8) | (v << 16);
			I915_WRITE(LGC_PALETTE(pipe) + i * 4, v);
		}

		return;
	}

	for (i = 0; i < ENH_NUM_BINS; i++) {
		int base_value = i * 8 * 4;
		int level = base_value -
			    intel_fixed_mul(base_value, multiplier);
		I915_WRITE(BLM_HIST_CTL, ENH_HIST_ENABLE |
					 ENH_MODIF_TBL_ENABLE |
					 ENH_PIPE(pipe) |
					 HIST_MODE_YUV |
					 ENH_MODE_ADDITIVE |
					 BIN_REGISTER_SET |
					 i);
		I915_WRITE(BLM_HIST_ENH, level);
	}
}

/* Compute the current step. Returns true if we need to change the levels,
 * false otherwise.
 */
static bool adaptive_backlight_current_step(drm_i915_private_t *dev_priv,
					   int correction_level)
{
	int delta, direction;

	direction = (correction_level >
			dev_priv->backlight_correction_level);

	if (direction == dev_priv->backlight_correction_direction) {
		dev_priv->backlight_correction_count++;
	} else {
		dev_priv->backlight_correction_count = 0;
		dev_priv->backlight_correction_direction = direction;
	}

	delta = abs(correction_level -
			dev_priv->backlight_correction_level)/4;

	if (delta < 1)
		delta = 1;

	/* For increasing the brightness, we do it instantly.
	 * For lowering the brightness, we require at least 10 frames
	 * below the current value. This avoids ping-ponging of the
	 * backlight level.
	 *
	 * We also never increase the backlight by more than 6% per
	 * frame, and never lower it by more than 3% per frame, because
	 * the backlight needs time to adjust and the LCD correction
	 * would be "ahead" otherwise.
	 */
	if (correction_level > dev_priv->backlight_correction_level) {
		if (delta > 15)
			delta = 15;
		dev_priv->backlight_correction_level += delta;
	} else if ((dev_priv->backlight_correction_count > 10) &&
			(correction_level < dev_priv->backlight_correction_level)) {
		if (delta > 7)
			delta = 7;
		dev_priv->backlight_correction_level -= delta;
	} else {
		return false;
	}

	return true;
}

/*
 * This function computes the backlight correction level for an acceptable
 * distortion and fills up the correction bins adequately.
 */
static void
adaptive_backlight_correct(struct drm_device *dev, int pipe)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int correction_level;
	int multiplier, one_over_gamma;
	int levels[ENH_NUM_BINS];

	get_levels(dev, pipe, levels);

	/* Find the correction level for an acceptable distortion */
	correction_level = histogram_find_correction_level(levels);

	/* If we're already at our correction target, then there is
	 * nothing to do
	 */
	if (dev_priv->backlight_correction_level == correction_level)
		return;

	/* Decide by how much to move this step. If we didn't move, return */
	if (!adaptive_backlight_current_step(dev_priv, correction_level))
		return;

	dev_priv->set_backlight(dev, dev_priv->backlight_level);

	/* We need to invert the gamma correction of the LCD values,
	 * but not of the backlight which is linear.
	 */
	one_over_gamma = intel_fixed_div(FIXED_ONE,
			dev_priv->adaptive_backlight_panel_gamma);
	multiplier = intel_fixed_pow(dev_priv->backlight_correction_level * 256,
			one_over_gamma);

	set_levels(dev, pipe, multiplier);
}

void intel_adaptive_backlight(struct drm_device *dev, int pipe_vblank_event)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int pipe;
	struct drm_connector *connector;
	struct intel_crtc *intel_crtc;

	if (!dev_priv->adaptive_backlight_enabled)
		return;

	/* Find the connector */
	if (dev_priv->int_lvds_connector)
		connector = dev_priv->int_lvds_connector;
	else if (dev_priv->int_edp_connector)
		connector = dev_priv->int_edp_connector;
	else
		return;

	if (!connector)
		return;

	if (!connector->encoder)
		return;

	if (!connector->encoder->crtc)
		return;

	/* Find the pipe for the panel. */
	intel_crtc = to_intel_crtc(connector->encoder->crtc);
	pipe = intel_crtc->pipe;

	/* The callback happens for both pipe A & B. Now that we know which
	 * pipe we're doing adaptive backlight on, check that it's the right
	 * one. Bail if it isn't.
	 */
	if (pipe != pipe_vblank_event)
		return;

	/* Make sure we ack the previous event. Even though we do not get the
	 * IRQs (see above explanation), we must still ack the events otherwise
	 * the histogram data doesn't get updated any more.
	 */
	I915_WRITE(BLM_HIST_GUARD_BAND, BLM_HIST_INTR_ENABLE |
					BLM_HIST_EVENT_STATUS |
					(1 << BLM_HIST_INTR_DELAY_SHIFT));


	adaptive_backlight_correct(dev, pipe);
}

void intel_adaptive_backlight_enable(struct drm_i915_private *dev_priv)
{
	dev_priv->backlight_correction_level = 256;
	dev_priv->backlight_correction_count = 0;
	dev_priv->backlight_correction_direction = 0;
	/* Default gamma is 2.2 as 16.16 fixed point */
	if (!dev_priv->adaptive_backlight_panel_gamma)
		dev_priv->adaptive_backlight_panel_gamma = 144179;

	dev_priv->adaptive_backlight_enabled = true;
}

void intel_adaptive_backlight_disable(struct drm_i915_private *dev_priv,
				      struct drm_connector *connector)
{
	struct intel_crtc *intel_crtc;
	int pipe;
	struct drm_device *dev = dev_priv->dev;

	dev_priv->adaptive_backlight_enabled = false;

	dev_priv->backlight_correction_level = 256;

	dev_priv->set_backlight(dev, dev_priv->backlight_level);

	/* Find the pipe */
	if (!connector->encoder)
		return;

	if (!connector->encoder->crtc)
		return;

	intel_crtc = to_intel_crtc(connector->encoder->crtc);
	pipe = intel_crtc->pipe;

	/* Reset the levels to default */
	set_levels(dev, pipe, FIXED_ONE);
}

