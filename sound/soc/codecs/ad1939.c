/*
 * AD1939 I2S Codec driver
 *
 * (c) 2007 MSC Vertriebsges.m.b.H, <mlau@msc-ge.com>
 *
 * licensed under the GPLv2
 */

/*
 * Analog Devices AD1939 I2S codec; with I2C or SPI interface.
 *  supports 2 channels in plain I2S mode,
 *  and up to 16 channels in TDM modes.
 * As Master, the codec always assumes a frame is a multiple of 32bits!
 * TDM Modes:
 *	- multiple channels over I2S
 *	- 32 fixed slot width, TDM stereo is I2S with 32bits/sample.
 *
 * TODO: driver switches between stereo and TDM mode based on the number
 *	 of  channels  for record / playback (ad1939_hw_params()).  This
 *	 should be  overhauled if someone  wants to daisy-chain a few of
 *	 these  together  (to  get 8 DAC channels total).  Maybe use TDM
 *	 mode  exclusively  when codec  is LRCK/BCK  master, and let the
 *	 user set WHICH TDM mode to use through setup_data.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "ad1939.h"

#define AUDIO_NAME "AD1939"

#define msg(x...) printk(KERN_INFO AUDIO_NAME ": " x)

/* #define CODEC_DEBUG */

#ifdef CODEC_DEBUG
#define dbg(x...)	printk(KERN_INFO AUDIO_NAME ": " x)
#else
#define dbg(x...)	do {} while(0)
#endif

struct ad1939_private {
	struct snd_soc_codec *codec;
	unsigned long sysclk;
	unsigned char tdm_mode;
};

/* default register contents after reset */
static const u16 ad1939_regcache[AD1939_REGCOUNT] __devinitdata = {
	0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int ad1939_i2c_write(struct snd_soc_codec *codec, unsigned int r,
			    unsigned int v)
{
	struct i2c_msg msg;
	struct i2c_client *c;
	u8 data[2];
	u16 *cache = codec->reg_cache;
	int ret;

	if (cache[r] == v)
		return 0;

	/* oh noes, now we have to talk to the codec */
	c = (struct i2c_client *)codec->control_data;
	data[0] = r & 0xff;
	data[1] = v & 0xff;
	msg.addr = c->addr;
	msg.flags = 0;	/* write */
	msg.buf = &data[0];
	msg.len = 2;

	ret = i2c_transfer(c->adapter, &msg, 1);
	if (ret == 1)
		cache[r] = v;
	return (ret == 1) ? 0 : -EIO;
}

static unsigned int ad1939_i2c_read(struct snd_soc_codec *codec,
				    unsigned int r)
{
	struct i2c_msg msg[2];
	struct i2c_client *c;
	u16 *cache = codec->reg_cache;
	u8 data[2];
	int ret;

	/* the PLLCTL1 has one read-only bit: PLL lock indicator.
	 * all other regs keep what was set
	 */
	if (likely(r != AD1939_PLLCTL1))
		return cache[r];

	c = (struct i2c_client *)codec->control_data;
	data[0] = r & 0xff;
	msg[0].addr = c->addr;
	msg[0].flags = 0;
	msg[0].buf = &data[0];
	msg[0].len = 1;
	
	msg[1].addr = c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = &data[1];
	msg[1].len = 1;

	ret = i2c_transfer(c->adapter, &msg[0], 2);
	return (ret == 2) ? data[1] : -EIO;
}
#endif

static inline unsigned int ad1939_read(struct snd_soc_codec *codec,
				unsigned int r)
{
	return codec->read(codec, r);
}

static inline int ad1939_write(struct snd_soc_codec *codec,
			       unsigned int r, unsigned int v)
{
	return codec->write(codec, r, v);
}

/***** controls ******/

static const char *dac_deemph[] = {"Flat", "48kHz", "44.1kHz", "32kHz"};
static const char *dac_outpol[] = {"Normal", "Inverted"};

static const struct soc_enum ad1939_enum[] = {
      /*SOC_ENUM_SINGLE(register, startbit, choices, choices-texts) */
	SOC_ENUM_SINGLE(AD1939_DACCTL2, 1, 4, dac_deemph),
	SOC_ENUM_SINGLE(AD1939_DACCTL2, 5, 1, dac_outpol),
};

static const struct snd_kcontrol_new ad1939_snd_ctls[] = {
SOC_DOUBLE_R("Master Playback", AD1939_VOL1L, AD1939_VOL1R, 0, 255, 1),
SOC_DOUBLE_R("Channel 2 Playback", AD1939_VOL2L, AD1939_VOL2R, 0, 255, 1),
SOC_DOUBLE_R("Channel 3 Playback", AD1939_VOL3L, AD1939_VOL3R, 0, 255, 1),
SOC_DOUBLE_R("Channel 4 Playback", AD1939_VOL4L, AD1939_VOL4R, 0, 255, 1),
SOC_ENUM("DAC Deemphasis", ad1939_enum[0]),
SOC_ENUM("DAC output polarity", ad1939_enum[1]),
};

/* add non dapm controls */
static int ad1939_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(ad1939_snd_ctls); i++) {
		err = snd_ctl_add(codec->card,
			snd_soc_cnew(&ad1939_snd_ctls[i], codec, NULL));
		if (err < 0)
			return err;
	}
	return 0;
}


/***** chip interface config ******/


static int ad1939_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct ad1939_private *ad = codec->private_data;
	unsigned char dac0, dac1, dac2, adc0, adc1, adc2;
	unsigned long rate;
	unsigned int bits;

	dbg("ad1939_hw_params");

	dac0 = ad1939_read(codec, AD1939_DACCTL0);
	dac1 = ad1939_read(codec, AD1939_DACCTL1);
	dac2 = ad1939_read(codec, AD1939_DACCTL2);
	adc0 = ad1939_read(codec, AD1939_ADCCTL0);
	adc1 = ad1939_read(codec, AD1939_ADCCTL1);
	adc2 = ad1939_read(codec, AD1939_ADCCTL2);

	rate = params_rate(params);
	bits = params->msbits;

	dbg("bits %d srate %lu/%d chans %d\n", bits, rate,
	     params->rate_den, params_channels(params));

	/*
	 * sample rate
	 */
	dac0 &= ~(3<<1);	/* 48kHz */
	adc0 &= ~(3<<6);	/* 48kHz */
	switch (rate) {
	case 32000 ... 48000:
		break;
	case 64000 ... 96000:
		dac0 |= (1<<1);
		adc0 |= (1<<6);
		break;
	case 128000 ... 192000:
		dac0 |= (2<<1);
		adc0 |= (2<<6);
		break;
	default:
		dbg("rejecting srate %lu\n", rate);
		return -EINVAL;
	}

	/*
	 * sample width (bits)
	 */
	dac2 &= ~(3<<3);	/* 24 bits */
	adc1 &= ~(3<<0);	/* 24 bits */
	switch (bits) {
	case 16: dac2 |= (3<<3); adc1 |= (3<<0); break;
	case 20: dac2 |= (1<<3); adc1 |= (1<<0); break;
	case 24: break;
	default:
		dbg("rejecting bits %d\n", bits);
		return -EINVAL;
	}

	/*
	 * channels
	 */
	dac0 &= ~(3<<6);	/* DAC I2S stereo */
	dac1 &= ~(3<<1);	/* 2 channels */
	adc1 &= ~(3<<5);	/* ADC I2S stereo */
	adc2 &= ~(3<<4);	/* 2 channels */
	switch (params_channels(params)) {
	case 2:	/* I2S stereo mode */
		break;
	case 4:	/* TDM mode */
		dac0 |= (ad->tdm_mode & 3) << 6;
		dac1 |= (1<<1);
		adc1 |= (ad->tdm_mode & 3) << 5;
		adc2 |= (1<<4);
		break;
	case 8:	/* TDM mode */
		dac0 |= (ad->tdm_mode & 3) << 6;
		dac1 |= (2<<1);
		adc1 |= (ad->tdm_mode & 3) << 5;
		adc2 |= (2<<4);
		break;
	case 16: /* TDM mode */
		dac0 |= (ad->tdm_mode & 3) << 6;
		dac1 |= (3<<1);
		adc1 |= (ad->tdm_mode & 3) << 5;
		adc2 |= (3<<4);
		break;
	default:
		dbg("%d channels not supported\n",
			params_channels(params));
		return -EINVAL;
	}

	ad1939_write(codec, AD1939_DACCTL0, dac0);
	ad1939_write(codec, AD1939_DACCTL1, dac1);
	ad1939_write(codec, AD1939_DACCTL2, dac2);
	ad1939_write(codec, AD1939_ADCCTL0, adc0);
	ad1939_write(codec, AD1939_ADCCTL1, adc1);
	ad1939_write(codec, AD1939_ADCCTL2, adc2);

	return 0;
}

static int ad1939_set_dai_fmt(struct snd_soc_codec_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned char dac0, dac1, adc1, adc2;

	dbg("ad1939_set_dai_fmt(0x%08lx)", fmt);

	dac0 = ad1939_read(codec, AD1939_DACCTL0);
	dac1 = ad1939_read(codec, AD1939_DACCTL1);
	adc1 = ad1939_read(codec, AD1939_ADCCTL1);
	adc2 = ad1939_read(codec, AD1939_ADCCTL2);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		dac1 |= (1<<4) | (1<<5); /* LRCK master, BCK master */
		adc2 |= (1<<3) | (1<<6); /* LRCK master, BCK master */
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		dac1 |= (1<<4);		/* LRCK master */
		dac1 &= ~(1<<5);	/* BCK slave */
		adc2 |= (1<<3);		/* LRCK master */
		adc2 &= ~(1<<6);	/* BCK slave */
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		dac1 &= ~(1<<4);	/* LRCK slave */
		dac1 |= (1<<5);		/* BCK master */
		adc2 &= ~(1<<3);	/* LRCK slave */
		adc2 |= (1<<6);		/* BCK master */
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		dac1 &= ~((1<<4) | (1<<5)); /* LRCK BCK slave */
		adc2 &= ~((1<<3) | (1<<6)); /* LRCK BCK slave */
		break;
	default:
		dbg("invalid master/slave configuration\n");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		dac0 &= ~(7<<3); /* DAC: SDATA delay 1 */
		adc1 &= ~(7<<2); /* ADC: SDATA delay 1 */
		break;
	case SND_SOC_DAIFMT_MSB: /* LEFT_J */
		dac0 |= (1<<3);	/* no SDATA delay */
		adc1 |= (1<<2); /* no SDATA delay */
		break;
#if 0
	case SND_SOC_DAIFMT_LSB:
		/* FIXME: need to know if in TDM/Master mode and sample
		 * size, then program bitdelay accordingly
		 */
		break;
#endif
	default:
		dbg("invalid I2S interface format\n");
		return -EINVAL;
	}

	/* clock inversion */
	dac1 &= ~((1<<7) | (1<<3)); /* norm BCK LRCK */
	adc2 &= ~((1<<1) | (1<<2)); /* norm BCK LRCK */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		dac1 |= (1<<3);		/* inv LRCK */
		adc2 |= (1<<2);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		dac1 |= (1<<7);		/* inv BCK */
		adc2 |= (1<<1);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		dac1 |= (1<<3) | (1<<7); /* inv LRCK BCK */
		adc2 |= (1<<1) | (1<<2);
		break;
	default:
		dbg("invalid clock inversion configuration\n");
		return -EINVAL;
	}

	ad1939_write(codec, AD1939_DACCTL0, dac0);
	ad1939_write(codec, AD1939_DACCTL1, dac1);
	ad1939_write(codec, AD1939_ADCCTL1, adc1);
	ad1939_write(codec, AD1939_ADCCTL2, adc2);

	return 0;
}

static int ad1939_dapm_event(struct snd_soc_codec *codec, int event)
{
	unsigned char pll0, adc0, dac0;

	/* the codec doesn't really have  sophisticated PM like the
	 * WM8731 for example; one can merely turn off DAC, ADC and
	 * the internal PLL
	 */
	pll0 = ad1939_read(codec, AD1939_PLLCTL0) & 0xfe;
	dac0 = ad1939_read(codec, AD1939_DACCTL0) & 0xfe;
	adc0 = ad1939_read(codec, AD1939_ADCCTL0) & 0xfe;

	switch (event)
	{
	case SNDRV_CTL_POWER_D1:
	case SNDRV_CTL_POWER_D2:
	case SNDRV_CTL_POWER_D0:
		ad1939_write(codec, AD1939_PLLCTL0, pll0);
		ad1939_write(codec, AD1939_DACCTL0, dac0);
		ad1939_write(codec, AD1939_ADCCTL0, adc0);
		break;
	case SNDRV_CTL_POWER_D3hot:
	case SNDRV_CTL_POWER_D3cold:
		/* turn of internal PLL and DAC/ADCs */
		ad1939_write(codec, AD1939_PLLCTL0, pll0 | 1);
		ad1939_write(codec, AD1939_DACCTL0, dac0 | 1);
		ad1939_write(codec, AD1939_ADCCTL0, adc0 | 1);
		break;
	}
	codec->dapm_state = event;
	return 0;
}

static int ad1939_digmute(struct snd_soc_codec_dai *dai, int mute)
{
	dai->codec->write(dai->codec, AD1939_DACMUTE, mute ? 0xff : 0);
	return 0;
}

static int ad1939_set_dai_sysclk(struct snd_soc_codec_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct ad1939_private *ad = codec->private_data;

	dbg("sysck id %d f %d dir %d\n", clk_id, freq, dir);
	switch (freq) {
	case 11288000:
		ad->sysclk = freq;
		break;
	default:
		dbg("invalid sysclk\n");
		return -EINVAL;
	}
	return 0;
}

#define AD1939_RATES	\
	(SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | \
	 SNDRV_PCM_RATE_192000)

#define AD1939_FORMATS	\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

struct snd_soc_codec_dai ad1939_dai = {
	.name = "AD1939",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 4,	/* 4/8 in single/dualline TDM */
		.rates = AD1939_RATES,
		.formats = AD1939_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 4,	/* yes, 2 DACs! */
		.rates = AD1939_RATES,
		.formats = AD1939_FORMATS,},
	.ops = {
		.hw_params = ad1939_hw_params,
	},
	.dai_ops = {
		.digital_mute = ad1939_digmute,
		.set_sysclk = ad1939_set_dai_sysclk,
		.set_fmt = ad1939_set_dai_fmt,
	}
};
EXPORT_SYMBOL_GPL(ad1939_dai);


static int ad1939_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	struct ad1939_setup_data *setup = socdev->codec_data;
	struct ad1939_private *ad = codec->private_data;
	unsigned char r0, r1;
	int ret;

	codec->owner = THIS_MODULE;
	codec->dapm_event = ad1939_dapm_event;
	codec->dai = &ad1939_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(ad1939_regcache);
	codec->reg_cache = kzalloc(sizeof(ad1939_regcache), GFP_KERNEL);
	if (!codec->reg_cache)
		return -ENOMEM;

	/* "initialize" the codec with default data */
	for (r0 = 0; r0 < AD1939_REGCOUNT; r0++)
		ad1939_write(codec, r0, ad1939_regcache[r0]);

	/*
	 * remember TDM mode and setup clock routing
	 */
	ad->tdm_mode = setup->tdm_mode;
	/* use default TDM mode if noone wants one */
	if ((ad->tdm_mode > AD1939_TDM_MODE_DUALLINE) || (ad->tdm_mode < 1))
		ad->tdm_mode = AD1939_TDM_MODE_TDM;

	r0 = ad1939_read(codec, AD1939_PLLCTL0) & ~(3<<5);
	r1 = ad1939_read(codec, AD1939_PLLCTL1) & 3;

	r0 |= (setup->pll_src & 3) << 5;
	r0 |= (1<<7);	/* enable internal master clock (i.e. the DAC/ADCs) */
	r1 |= setup->dac_adc_clksrc & 3;
	ad1939_write(codec, AD1939_PLLCTL0, r0);
	ad1939_write(codec, AD1939_PLLCTL1, r1);

	/* Bitclock sources for the ADC and DAC I2S interfaces */
	r0 = ad1939_read(codec, AD1939_DACCTL1);
	r1 = ad1939_read(codec, AD1939_ADCCTL2);
	r0 &= ~AD1939_BCLKSRC_DAC_PLL;
	r1 &= ~AD1939_BCLKSRC_ADC_PLL;
	r0 |= setup->dac_adc_clksrc & AD1939_BCLKSRC_DAC_PLL;
	r1 |= setup->dac_adc_clksrc & AD1939_BCLKSRC_ADC_PLL;
	ad1939_write(codec, AD1939_DACCTL1, r0);
	ad1939_write(codec, AD1939_ADCCTL2, r1);

	ad1939_add_controls(codec);
	ad1939_dapm_event(codec, SNDRV_CTL_POWER_D0);

	ret = snd_soc_register_card(socdev);
	if (ret < 0) {
		msg("failed to register card\n");
		goto card_err;
	}

	return 0;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
	kfree(codec->reg_cache);
	return ret;
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

static unsigned short normal_i2c[] = { 0, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static struct i2c_driver ad1939_i2c_driver;
static struct i2c_client client_template;

/* If the i2c layer weren't so broken, we could pass this kind of data
   around */
static struct snd_soc_device *ad1939_socdev;

static int ad1939_codec_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct snd_soc_device *socdev = ad1939_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct ad1939_setup_data *setup = socdev->codec_data;
	struct i2c_client *i2c;
	int ret;

	if (addr != setup->i2c_address)
		return -ENODEV;

	client_template.adapter = adap;
	client_template.addr = addr;

	i2c = kmemdup(&client_template, sizeof(client_template), GFP_KERNEL);
	if (i2c == NULL) {
		kfree(codec);
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = i2c_attach_client(i2c);
	if (ret < 0) {
		msg("failed to attach codec at addr %x\n", addr);
		goto err;
	}

	codec->read = ad1939_i2c_read;
	codec->write = ad1939_i2c_write;

	ret = ad1939_init(socdev);
	if (ret < 0) {
		msg("failed to initialise AD1939 codec\n");
		goto err;
	}
	return ret;

err:
	kfree(codec);
	kfree(i2c);
	return ret;
}

static int ad1939_i2c_detach(struct i2c_client *client)
{
	struct snd_soc_codec* codec = i2c_get_clientdata(client);

	i2c_detach_client(client);
	kfree(codec->reg_cache);
	kfree(client);

	return 0;
}

static int ad1939_i2c_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, ad1939_codec_probe);
}

/* corgi i2c codec control layer */
static struct i2c_driver ad1939_i2c_driver = {
	.driver = {
		.name = "AD1939",
		.owner = THIS_MODULE,
	},
	.id =	103, /* I2C_DRIVERID_AD1939 */
	.attach_adapter = ad1939_i2c_attach,
	.detach_client =  ad1939_i2c_detach,
	.command =        NULL,
};

static struct i2c_client client_template = {
	.name =   "AD1939",
	.driver = &ad1939_i2c_driver,
};
#endif	/* I2C */

static int ad1939_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct ad1939_setup_data *setup;
	struct snd_soc_codec *codec;
	struct ad1939_private *ad;
	int ret;

	ret = -ENOMEM;
	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		goto out;

	ad = kzalloc(sizeof(struct ad1939_private), GFP_KERNEL);
	if (ad == NULL) {
		kfree(codec);
		goto out;
	}
	ad->codec = codec;
	codec->private_data = ad;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	ad1939_socdev = socdev;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		normal_i2c[0] = setup->i2c_address;
		ret = i2c_add_driver(&ad1939_i2c_driver);
		if (ret != 0)
			msg("can't add i2c driver");
	}
#endif
	ret = 0;
out:
	return ret;
}

static int ad1939_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if (codec->control_data)
		ad1939_dapm_event(codec, SNDRV_CTL_POWER_D3cold);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&ad1939_i2c_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_ad1939 = {
	.probe = 	ad1939_probe,
	.remove = 	ad1939_remove,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ad1939);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC AD1939 I2S Codec driver");
MODULE_AUTHOR("Manuel Lauss <mlau@msc-ge.com>");
