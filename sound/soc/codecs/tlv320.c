/*
 * tlv320.c  --  TLV 320 ALSA Soc Audio driver
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright 2006 Atlab srl.
 *
 * Authors: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *          Nicola Perrino <nicola.perrino@atlab.it>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "tlv320.h"

#define AUDIO_NAME "tlv320"
#define TLV320_VERSION "0.1"

/*
 * Debug
 */

//#define TLV320_DEBUG 0

#ifdef TLV320_DEBUG
#define dbg(format, arg...) \
	printk(KERN_DEBUG AUDIO_NAME ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) \
	printk(KERN_ERR AUDIO_NAME ": " format "\n" , ## arg)
#define info(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) \
	printk(KERN_WARNING AUDIO_NAME ": " format "\n" , ## arg)


#define TLV320_VOICE_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000)


#define TLV320_VOICE_BITS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)


static int caps_charge = 2000;
static int setting = 1;
module_param(caps_charge, int, 0);
module_param(setting, int, 0);
MODULE_PARM_DESC(caps_charge, "TLV320 cap charge time (msecs)");

static struct workqueue_struct *tlv320_workq = NULL;
//static struct work_struct tlv320_dapm_work;

/* codec private data */
struct tlv320_priv {
	unsigned int sysclk;
	unsigned int pcmclk;
};


#ifdef TLV320AIC24K
/* ADDR table */
static const unsigned char tlv320_reg_addr[] = {
	0x00,	/* CONTROL REG 0 No Operation */
	0x01,	/* CONTROL REG 1  */
        0x02,	/* CONTROL REG 2  */
        0x03,	/* CONTROL REG 3A */
        0x03,	/* CONTROL REG 3B */
        0x03, 	/* CONTROL REG 3C */
        0x03,	/* CONTROL REG 3D */
        0x04,	/* CONTROL REG 4  */
        0x04,	/* CONTROL REG 4 Bis */
        0x05,	/* CONTROL REG 5A */
        0x05,	/* CONTROL REG 5B */
        0x05,	/* CONTROL REG 5C */
        0x05,	/* CONTROL REG 5D */
        0x06,	/* CONTROL REG 6A */
        0x06,	/* CONTROL REG 6B */
};

/*
 *  DATA case digital SET1:
 *     SSP -> DAC -> OUT
 *     IN  -> ADC -> SSP
 *     IN = HNSI (MIC)
 *     OUT = HDSO (SPKG)
 *  Usage: playback, capture streams
 */
static const unsigned char tlv320_reg_data_init_set1[] = {
	0x00,	/* CONTROL REG 0 No Operation */
	0x49,	/* CONTROL REG 1  */
        0x20,	/* CONTROL REG 2  */
        0x01,	/* CONTROL REG 3A */
        0x40,	/* CONTROL REG 3B */
        0x81,	/* CONTROL REG 3C */
        0xc0,	/* CONTROL REG 3D */
        0x02,//0x42(16khz),//0x10,	/* CONTROL REG 4  */
        0x88,//0x90,	/* CONTROL REG 4 Bis */
        0x00,	/* CONTROL REG 5A */
        0x40,//(0dB)	/* CONTROL REG 5B */
        0xbf,	/* CONTROL REG 5C */
        0xc0,	/* CONTROL REG 5D */
        0x02,//(HNSI) 	/* CONTROL REG 6A */
        0x81 //(HDSO) 	/* CONTROL REG 6B */
};

/*
 *  DATA case digital SET2:
 *     SSP -> DAC -> OUT
 *     IN  -> ADC -> SSP
 *     IN = HDSI (PHONE IN)
 *     OUT = HNSO (PHONE OUT)
 *  Usage: playback, capture streams
 */
static const unsigned char tlv320_reg_data_init_set2[] = {
	0x00,	/* CONTROL REG 0 No Operation */
	0x49,	/* CONTROL REG 1  */
        0x20,	/* CONTROL REG 2  */
        0x01,	/* CONTROL REG 3A */
        0x40,	/* CONTROL REG 3B */
        0x81,	/* CONTROL REG 3C */
        0xc0,	/* CONTROL REG 3D */
        0x02,//0x42(16khz),//0x10,	/* CONTROL REG 4  */
        0x88,//0x90,	/* CONTROL REG 4 Bis */
        0x00,	/* CONTROL REG 5A */
        0x52,//(-27dB) 	/* CONTROL REG 5B */
        0xbf,	/* CONTROL REG 5C */
        0xc0,	/* CONTROL REG 5D */
        0x01,//(PHONE IN) 	/* CONTROL REG 6A */
        0x82 //(PHONE OUT) 	/* CONTROL REG 6B */
};

/*
 *  DATA case analog:
 *     ADC, DAC, SSP off
 *     Headset input to output (HDSI2O -> 1)
 *     Handset input to output (HNSI2O -> 1)
 *  Usage: room monitor
 */
static const unsigned char tlv320_reg_data_init_set3[] = {
	0x00,	/* CONTROL REG 0 No Operation */
	0x08,   /* CONTROL REG 1  */
        0x20,   /* CONTROL REG 2  */
        0x11,   /* CONTROL REG 3A */
        0x40,	/* CONTROL REG 3B */
        0x80,   /* CONTROL REG 3C */
        0xc0,   /* CONTROL REG 3D */
        0x00,   /* CONTROL REG 4  */
        0x00,   /* CONTROL REG 5A */
        0x40,   /* CONTROL REG 5B */
        0x80,   /* CONTROL REG 5C */
        0xc0,   /* CONTROL REG 5D */
        0x60,   /* CONTROL REG 6A */
        0x80    /* CONTROL REG 6B */
};

#else // TLV320AIC14k

/* ADDR table */
static const unsigned char tlv320_reg_addr[] = {
	0x00,	/* CONTROL REG 0 No Operation */
	0x01,	/* CONTROL REG 1  */
        0x02,	/* CONTROL REG 2  */
        0x03,	/* CONTROL REG 3 */
        0x04,	/* CONTROL REG 4  */
        0x04,	/* CONTROL REG 4 Bis */
        0x05,	/* CONTROL REG 5A */
        0x05,	/* CONTROL REG 5B */
        0x05,	/* CONTROL REG 5C */
        0x05,	/* CONTROL REG 5D */
        0x06	/* CONTROL REG 6 */
};

/*
 *  DATA case digital:
 *     SSP -> DAC -> OUT
 *     IN  -> ADC -> SSP
 *  Usage: playback, capture streams
 */
static const unsigned char tlv320_reg_data_init_set1[] = {
	0x00,	/* CONTROL REG 0 No Operation */
	0x41,	/* CONTROL REG 1  */
        0x20,	/* CONTROL REG 2  */
        0x09,	/* CONTROL REG 3 */
        0x02,//0x42(16khz),//0x10,	/* CONTROL REG 4  */
        0x88,//0x90,	/* CONTROL REG 4 Bis */
        0x2A,	/* CONTROL REG 5A */
        0x6A,	/* CONTROL REG 5B */
        0xbc,	/* CONTROL REG 5C */
        0xc0,	/* CONTROL REG 5D */
        0x00	/* CONTROL REG 6 */
};
#endif
/*
 * read tlv320 register cache
 */
static inline unsigned int tlv320_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u8 *cache = codec->reg_cache;
	if (reg > ARRAY_SIZE(tlv320_reg_addr))
		return -1;
	return cache[reg];
}

/*
 * write tlv320 register cache
 */
static inline void tlv320_write_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	u8 *cache = codec->reg_cache;
	if (reg > ARRAY_SIZE(tlv320_reg_addr))
		return;
	cache[reg] = value;
}

/*
 * read tlv320
 */
static int tlv320_read (struct snd_soc_codec *codec, u8 reg)
{
	return i2c_smbus_read_byte_data(codec->control_data, reg);
}

/*
 * write tlv320
 */
static int tlv320_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	if (tlv320_reg_addr[reg] > 0x06)
		return -1;

	tlv320_write_reg_cache (codec, reg, value);

	return i2c_smbus_write_byte_data(codec->control_data, tlv320_reg_addr[reg], value);
}


/*
 * write block tlv320
 */
static int tlv320_write_block (struct snd_soc_codec *codec,
	const u8 *data, unsigned int len)
{
	int ret = -1;
	int i;

	for (i=0; i<len; i++) {
		dbg("addr = 0x%02x, data = 0x%02x", tlv320_reg_addr[i], data[i]);
		if ((ret = tlv320_write(codec, i, data[i])) < 0)
			break;
	}

	return ret;
}


static int tlv320_set_dai_fmt(struct snd_soc_codec_dai *codec_dai,
		unsigned int fmt)
{
	dbg("tlv320_set_dai_fmt enter");
	return 0;
}

/*
 * Set PCM DAI bit size and sample rate.
 */
static int tlv320_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	dbg("tlv320_pcm_hw_params enter");
	return 0;
}


static int tlv320_config_pcm_sysclk(struct snd_soc_codec_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	dbg("tlv320_config_pcm_sysclk enter");
	return 0;
}


/*
 *  Voice over PCM DAI
 */
struct snd_soc_codec_dai tlv320_dai[] = {
{	.name = "TLV320 Voice",
	.id = 1,
	.playback = {
		.stream_name = "Voice Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = TLV320_VOICE_RATES,
		.formats = TLV320_VOICE_BITS,},
	.capture = {
		.stream_name = "Voice Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = TLV320_VOICE_RATES,
		.formats = TLV320_VOICE_BITS,},
	.ops = {
		.hw_params = tlv320_pcm_hw_params,},
	.dai_ops = {
		.digital_mute = NULL,
		.set_fmt = tlv320_set_dai_fmt,
		.set_clkdiv = NULL,
		.set_pll = NULL,
		.set_sysclk = tlv320_config_pcm_sysclk,
	},
},


};
EXPORT_SYMBOL_GPL(tlv320_dai);


static void tlv320_work(struct work_struct *work)
{
#if 0
	struct snd_soc_codec *codec =
		container_of(work, struct snd_soc_codec, delayed_work.work);
	//wm8753_dapm_event(codec, codec->dapm_state);
#endif
}

/*
 * initialise the TLV320 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int tlv320_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	int ret = 0;

	codec->name = "TLV320";
	codec->owner = THIS_MODULE;
	codec->read = tlv320_read_reg_cache;
	codec->write = tlv320_write;
	codec->dai = tlv320_dai;
	codec->num_dai = ARRAY_SIZE(tlv320_dai);
	codec->reg_cache_size = ARRAY_SIZE(tlv320_reg_addr);

	codec->reg_cache =
			kzalloc(sizeof(u8) * ARRAY_SIZE(tlv320_reg_addr), GFP_KERNEL);
	if (codec->reg_cache == NULL)
		return -ENOMEM;
	memcpy(codec->reg_cache, tlv320_reg_addr,
		sizeof(u8) * ARRAY_SIZE(tlv320_reg_addr));
	codec->reg_cache_size = sizeof(u8) * ARRAY_SIZE(tlv320_reg_addr);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		kfree(codec->reg_cache);
		return ret;
	}

	queue_delayed_work(tlv320_workq,
		&codec->delayed_work, msecs_to_jiffies(caps_charge));

	ret = snd_soc_register_card(socdev);
	if (ret < 0) {
		snd_soc_free_pcms(socdev);
		snd_soc_dapm_free(socdev);
	}

	return ret;
}

/* If the i2c layer weren't so broken, we could pass this kind of data
   around */
static struct snd_soc_device *tlv320_socdev;

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)

#define I2C_DRIVERID_TLV320 0xfefe /* liam -  need a proper id */

static unsigned short normal_i2c[] = { 0, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static struct i2c_driver tlv320_i2c_driver;
static struct i2c_client client_template;

static int tlv320_codec_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct snd_soc_device *socdev = tlv320_socdev;
	struct tlv320_setup_data *setup = socdev->codec_data;
	struct snd_soc_codec *codec = socdev->codec;
	struct i2c_client *i2c;
	int ret, len;
        const unsigned char *data;

	if (addr != setup->i2c_address)
		return -ENODEV;

	client_template.adapter = adap;
	client_template.addr = addr;

	i2c = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (i2c == NULL){
		kfree(codec);
		return -ENOMEM;
	}
	memcpy(i2c, &client_template, sizeof(struct i2c_client));
	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = i2c_attach_client(i2c);
	if (ret < 0) {
		err("failed to attach codec at addr %x\n", addr);
		goto err;
	}

	ret = tlv320_init(socdev);
	if (ret < 0) {
		err("failed to initialise TLV320\n");
		goto err;
	}

	switch(setting) {
	case 1:
		data = tlv320_reg_data_init_set1;
		len = sizeof(tlv320_reg_data_init_set1);
		break;
	case 2:
		data = tlv320_reg_data_init_set2;
		len = sizeof(tlv320_reg_data_init_set2);
		break;
	case 3:
		data = tlv320_reg_data_init_set3;
		len = sizeof(tlv320_reg_data_init_set3);
		break;
	default:
		data = tlv320_reg_data_init_set1;
		len = sizeof(tlv320_reg_data_init_set1);
		break;
	}

	ret = tlv320_write_block(codec, data, len);

	if (ret < 0) {
		err("attach error: init status %d\n", ret);
	} else {
		info("attach: chip tlv320 at address 0x%02x",
			tlv320_read(codec, 0x02) << 1);
	}

        //tlv320_write(codec, CODEC_REG6B, 0x80);
#if 0
	int value;
	int i;

	for (i=0; i<len; i++) {
		value = tlv320_read(codec, tlv320_reg_addr[i]);
		dbg("read addr = 0x%02x, data = 0x%02x", tlv320_reg_addr[i], value);
		mdelay(10);
	}

#endif


	return ret;

err:
	kfree(codec);
	kfree(i2c);
	return ret;
}

static int tlv320_i2c_detach(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	i2c_detach_client(client);
	kfree(codec->reg_cache);
	kfree(client);
	return 0;
}

static int tlv320_i2c_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, tlv320_codec_probe);
}

/* tlv320 i2c codec control layer */
static struct i2c_driver tlv320_i2c_driver = {
	.driver = {
		.name = "tlv320 I2C Codec",
		.owner = THIS_MODULE,
	},
	.id =             I2C_DRIVERID_TLV320,
	.attach_adapter = tlv320_i2c_attach,
	.detach_client =  tlv320_i2c_detach,
	.command =        NULL,
};

static struct i2c_client client_template = {
	.name =   "tlv320",
	.driver = &tlv320_i2c_driver,
};
#endif

static int tlv320_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct tlv320_setup_data *setup;
	struct snd_soc_codec *codec;
	int ret = 0;
	struct tlv320_priv *tlv320;

	info("TLV320 Audio Codec %s", TLV320_VERSION);

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	tlv320 = kzalloc(sizeof(struct tlv320_priv), GFP_KERNEL);
	if (tlv320 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	codec->private_data = tlv320;

	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	tlv320_socdev = socdev;

	INIT_DELAYED_WORK(&codec->delayed_work, tlv320_work);
	tlv320_workq = create_workqueue("tlv320");
	if (tlv320_workq == NULL) {
		kfree(codec);
		return -ENOMEM;
	}
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		normal_i2c[0] = setup->i2c_address;
		codec->hw_write = (hw_write_t)i2c_master_send;
		ret = i2c_add_driver(&tlv320_i2c_driver);
		if (ret != 0)
			printk(KERN_ERR "can't add i2c driver");
	}
#else
		/* Add other interfaces here */
#endif
	return ret;
}

/* power down chip */
static int tlv320_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if (tlv320_workq)
		destroy_workqueue(tlv320_workq);
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	i2c_del_driver(&tlv320_i2c_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_tlv320 = {
	.probe = 	tlv320_probe,
	.remove = 	tlv320_remove,
};

EXPORT_SYMBOL_GPL(soc_codec_dev_tlv320);

MODULE_DESCRIPTION("ASoC TLV320 driver");
MODULE_AUTHOR("Nicola Perrino");
MODULE_LICENSE("GPL");
