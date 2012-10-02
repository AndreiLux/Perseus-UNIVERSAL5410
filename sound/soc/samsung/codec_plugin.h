/* sound/soc/samsung/codec_plugin.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Plugin
 *
 * Copyright (c) 2012 Samsung Electronics Co. Ltd.
 *	Rahul Sharma <rahul.sharma@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SAMSUNG_PLUGIN_H
#define __SND_SOC_SAMSUNG_PLUGIN_H

struct audio_plugin_ops {
	int (*set_state)(struct device *dev, int enable);

	int (*get_state)(struct device *dev, int *is_enabled);

	int (*hw_params)(struct device *dev, struct snd_pcm_substream *,
		struct snd_pcm_hw_params *, struct snd_soc_dai *);

	int (*trigger)(struct device *dev, struct snd_pcm_substream *, int,
		struct snd_soc_dai *);
};

struct audio_codec_plugin {
	struct device *dev;
	struct audio_plugin_ops ops;
	int (*jack_cb)(int plugged);
};

#endif /* __SND_SOC_SAMSUNG_PLUGIN_H */
