/*
 * cs4251x.h -- Cirrus/Crystal CS42516/42518 I2C Codec Soc Audio driver
 *
 * Copyright 2007 MSC Vertriebsges.m.b.h, <mlau@msc-ge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * The CS4251X is quite complex; please have a look at the manual before
 * using it: http:///www.cirrus.com/en/pubs/proDatasheet/CS42518_F1.pdf
 */

/*
 * FIXME: A LOT of this code assumes that the CODEC_SP interface is used
 *	  for playback and the SAI_SP for recording (because this is the
 *	  configuration I'm working with).  Look  at  the hw_params
 *	  function for example (setting the Fs ratio based on OMCK)
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

#include "cs4251x.h"

#define AUDIO_NAME "cs4251x"

/* #define CS4251X_DEBUG */

#ifdef CS4251X_DEBUG
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

/* codec private data */
struct cs_priv {
	struct snd_soc_codec	*codec;
	struct work_struct	irq_work;
	unsigned long sysclk;
	int irq;
};

#define FS_RATIO_CNT	3
/* oversampling clock ratios.
 * OMCK/ratio = samplerate.
 */
static const unsigned int cs_fs_ratios[FS_RATIO_CNT] = {
	512, 256, 128
};

/* codec register values after reset */
static const u16 cs4251x_regcache[CS4251X_REGNUM] __devinitdata = {
	0x00, 0xE3, 0x81, 0x00, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x09, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
	0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

/*
 * write to the cs4251x register space
 */
static int cs_i2c_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
	struct i2c_msg msg;
	struct i2c_client *c;
	u16 *cache = codec->reg_cache;
	u8 data[2];
	int ret;	

	reg &= 0x7f;	/* kill the auto-addr-increment bit */

	/* exclude read-only regs */
	switch (reg) {
	case 0 ... 1:
	case 0x07 ... 0x0c:
	case 0x20:
	case 0x25:
	case 0x26:
	case 0x30 ... 0x51:
		return 0;
	}

	if (value == cache[reg])
		return 0;

	c = (struct i2c_client *)codec->control_data;
	data[0] = reg & 0xff;
	data[1] = value & 0xff;
	msg.addr = c->addr;
	msg.flags = 0;	/* write */
	msg.buf = &data[0];
	msg.len = 2;

	ret = i2c_transfer(c->adapter, &msg, 1);
	if (ret == 1)
		cache[reg] = value;
	return (ret == 1) ? 0 : -EIO;
}

static unsigned int cs_i2c_read(struct snd_soc_codec *codec,
				     unsigned int reg)
{
	struct i2c_msg msg[2];
	struct i2c_client *c;
	u8 data[2];
	u16 *cache = codec->reg_cache;
	int ret;

	/* check for the uncachables */
	switch (reg) {
	case 1:
	case 0x07 ... 0x0c:
	case 0x20:
	case 0x25:
	case 0x26:
	case 0x30 ... 0x51:
		break;
	default:
		return cache[reg];
	}

	c = (struct i2c_client *)codec->control_data;
	data[0] = reg & 0xff;
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

int cs4251x_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	return codec->write(codec, reg, value);
}
EXPORT_SYMBOL_GPL(cs4251x_write);

unsigned int cs4251x_read(struct snd_soc_codec *codec, unsigned int reg)
{
	return codec->read(codec, reg);
}
EXPORT_SYMBOL_GPL(cs4251x_read);

/**
 * cs4251x_gpio_set - set gpio output level (high/low)
 * @codec:	snd_soc_codec structure for this codec.
 * @gpio:	index of GPIO pin to set (0-6).
 * @val:	0 for low-leve, 1 for high-level.
 */
void cs4251x_gpio_set(struct snd_soc_codec *codec, unsigned char gpio, 
			unsigned char val)
{
	unsigned char v;

	if (gpio > 6)
		return;

	v = cs4251x_read(codec, CS4251X_GPIO_0 + gpio);
	v &= 0x3f;
	if (val)
		v |= 3 << 6;	/* GPO, high level */
	else	
		v |= 2 << 6;	/* GPO, low level */
	cs4251x_write(codec, CS4251X_GPIO_0 + gpio, v);
}
EXPORT_SYMBOL_GPL(cs4251x_gpio_set);

/**
 * cs4251x_gpio_mode - set gpio/rxp pin mode.
 * @codec:	snd_soc_codec structure for this codec.
 * @gpio:	index of GPIO pin to set (0-6).
 * @val:	see GPIO_MODE_xxx constants.
 */
void cs4251x_gpio_mode(struct snd_soc_codec *codec, unsigned char gpio,
			unsigned char mode, unsigned char polarity,
			unsigned char funcmode)
{
	unsigned char v;

	if (unlikely(gpio > 6))
		return;

	switch (mode)
	{
	case CS4251X_GPIO_MODE_RXP:	v = (0 << 6); break;
	case CS4251X_GPIO_MODE_GPOHI:	v = (3 << 6); break;
	case CS4251X_GPIO_MODE_GPOLOW:
		v = (2 << 6) | (funcmode & 1) | ((funcmode & 1) << 1);
		break;
	case CS4251X_GPIO_MODE_MUTE:
		v = (1 << 6) | ((polarity & 1) << 5) | 
			(funcmode & CS4251X_FUNCMODE_MUTE_MASK);
		break;
	default:
		return;
	}
	cs4251x_write(codec, CS4251X_GPIO_0 + gpio, v);
}
EXPORT_SYMBOL_GPL(cs4251x_gpio_mode);

static void cs_irq_work(struct work_struct *data)
{
	struct cs_priv *cs = 
		container_of(data, struct cs_priv, irq_work);
	unsigned char r;

	r = cs4251x_read(cs->codec, CS4251X_IRQSTAT);
	dbg("IRQ: istat 0x%02x", r);
}

static irqreturn_t cs_irq(int irq, void *dev_id)
{
	struct cs_priv *cs = dev_id;
	schedule_work(&cs->irq_work);
	return IRQ_HANDLED;
}

static const char *cs_vol_transition[] = {
	"Immediate", "Zero-Cross", "Soft-Ramp", 
	"Soft-Ramp on Zero-Crossings"
};
static const char *cs_off_on[] = {"Off", "On"};

static const struct soc_enum cs_enum[] = {
      /*SOC_ENUM_SINGLE(reg, startbit, choicecount, choices-texts), */
	SOC_ENUM_SINGLE(CS4251X_VOLTRANSCTL, 4, 4, cs_vol_transition),
	SOC_ENUM_SINGLE(CS4251X_VOLTRANSCTL, 6, 2, cs_off_on),
	SOC_ENUM_SINGLE(CS4251X_VOLTRANSCTL, 3, 2, cs_off_on),
	SOC_ENUM_SINGLE(CS4251X_VOLTRANSCTL, 1, 2, cs_off_on),
	SOC_ENUM_SINGLE(CS4251X_VOLTRANSCTL, 0, 2, cs_off_on),
	SOC_ENUM_SINGLE(CS4251X_FUNCMODE, 1, 2, cs_off_on),
	SOC_ENUM_SINGLE(CS4251X_FUNCMODE, 0, 2, cs_off_on),
};

static const struct snd_kcontrol_new cs_snd_controls[] = {
SOC_DOUBLE_R("Master Playback Volume", CS4251X_L1VOL, CS4251X_R1VOL, 0, 255, 1),
SOC_DOUBLE_R("Channel 2 Playback Volume", CS4251X_L2VOL, CS4251X_R2VOL, 0, 255, 1),
SOC_DOUBLE_R("Channel 3 Playback Volume", CS4251X_L3VOL, CS4251X_R3VOL, 0, 255, 1),
SOC_DOUBLE_R("Channel 4 Playback Volume", CS4251X_L4VOL, CS4251X_R4VOL, 0, 255, 1),

SOC_DOUBLE_R("Capture Volume", CS4251X_LADCGAIN, CS4251X_RADCGAIN, 0, 255, 0),

SOC_ENUM("Volume Transition", cs_enum[0]),
SOC_ENUM("Soft Volume Ramp-Up after Error", cs_enum[3]),
SOC_ENUM("Soft Volume Ramp-Down before Filter Change", cs_enum[4]),
SOC_ENUM("DAC Auto-Mute", cs_enum[2]),
SOC_ENUM("DAC Deemphasis", cs_enum[5]),
SOC_ENUM("Receiver Deemphasis", cs_enum[6]),

};

/* add non dapm controls */
static int cs_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(cs_snd_controls); i++) {
		err = snd_ctl_add(codec->card,
		   snd_soc_cnew(&cs_snd_controls[i], codec, NULL));

		if (err < 0)
			return err;
	}

	return 0;
}

static int cs_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct cs_priv *cs = codec->private_data;
	unsigned long rate;
	unsigned char funcmode;
	int i;

	dbg("cs_hw_params");

	rate = params->rate_num / params->rate_den;
	for (i = 0; i < FS_RATIO_CNT; i++) {

		dbg("%d sck %lu rat %d res %lu rate %lu", i, cs->sysclk,
			cs_fs_ratios[i],
			(cs->sysclk / cs_fs_ratios[i]), rate);

		if ((cs->sysclk / cs_fs_ratios[i]) == rate) {
			/* found a suitable ratio, program it */
			funcmode = cs4251x_read(codec, CS4251X_FUNCMODE);
			/* set both CODEC_SP and SAI_SP */
			dbg("funcmode: was    %x", funcmode);
			funcmode &= ~(15<<4);
			funcmode |= (i<<4) | (i<<6);
			dbg("funcmode: is now %x", funcmode);
			cs4251x_write(codec, CS4251X_FUNCMODE, funcmode);
			return 0;
		}
	}
	/* eek, nothing suitable found */
	return -EINVAL;
}

static int cs_mute(struct snd_soc_codec_dai *dai, int mute)
{
	cs4251x_write(dai->codec, CS4251X_MUTE, mute ? 0xff : 0);
	return 0;
}

/*
 * someone wants to tell us the OMCK frequency we're running with.
 */
static int cs_set_dai_sysclk(struct snd_soc_codec_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs_priv *cs = codec->private_data;
	unsigned char ckctl;
	unsigned long rates;

	dbg("cs_set_dai_sysclk(%d, %ul, %d)", clk_id, freq, dir);

	ckctl = cs4251x_read(codec, CS4251X_CLKCTL);
	ckctl &= ~(3<<4);
	
	/* NOTE: these clock rates come from the datasheet... */
	switch (freq) {
	case 11289600:
		rates = SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_88200;
		break;
	case 12288000:
		rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000;
		break;
	case 16934400:
	case 18432000:
		/* well, none that ALSA has #defines for:
		 * 33075Hz, 66150Hz, 132300Hz, 36, 72, 144kHz
		 */
		rates = 0;
		ckctl |= (1<<4);
		break;
	case 22579200:
		rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_176400;
		ckctl |= (2<<4);
		break;
	case 24576000:
		rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000;
		ckctl |= (2<<4);
		break;
	default:
		dbg("invalid sysclk %uHz", freq);
		return -EINVAL;
	}

	/* tell the codec about the new OMCK range */
	cs4251x_write(codec, CS4251X_CLKCTL, ckctl);
	cs->sysclk = freq;	/* need to know in hw_params */

#if 0	/* does not work, boooooooo */
	/* update the DAI's supported rates mask */
	codec_dai->playback.rates = rates;
	codec_dai->capture.rates = rates;
#endif
	return 0;
}

/*
 * set Interface Format.
 * FIXME: this code applies the same values to the CODEC_SP and SAI_SP
 *	 interfaces, although technically they're independent.
 */
static int cs_set_dai_fmt(struct snd_soc_codec_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 iffmt, misc;

	dbg("cs_set_dai_fmt(0x%08x)", fmt);

	iffmt = cs4251x_read(codec, CS4251X_IFFMT);
	misc = cs4251x_read(codec, CS4251X_MISCCTL);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		misc |= 3;	/* CODEC_SP and SAI in master mode */
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		misc &= ~2;	/* CODEC_SP_and SAI in slave mode */
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iffmt &= ~0xc0;
		iffmt |= 1 << 6;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iffmt &= ~0xc0;
		iffmt |= 1 << 7;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iffmt &= ~0xc0;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	/* set iface */
	cs4251x_write(codec, CS4251X_IFFMT, iffmt);
	cs4251x_write(codec, CS4251X_MISCCTL, misc);

	return 0;
}

static int cs_dapm_event(struct snd_soc_codec *codec, int event)
{
	switch (event)
	{
	case SNDRV_CTL_POWER_D0:	/* full on */
	case SNDRV_CTL_POWER_D1:
	case SNDRV_CTL_POWER_D2:
		cs4251x_write(codec, CS4251X_PM, 0);
		break;
	case SNDRV_CTL_POWER_D3hot:	/* Off, with power */
	case SNDRV_CTL_POWER_D3cold:	/* Off, without power */
		/* all DAC/ADCs off, power down */
		cs4251x_write(codec, CS4251X_PM, 0xff);
		break;
	}
	codec->dapm_state = event;
	return 0;
}

/* FIXME: really depends on sysclk! */
#define CS4251X_RATES	\
	(SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |	\
	 SNDRV_PCM_RATE_192000)

#define CS4251X_FORMATS	\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

/*
 * This is slightly incorrect: The codec has 4 CODEC_SP interfaces (each
 * connected to a DAC; 1st also connected to ADCs) and 1 SAI_SP interface
 * (connected to ADC and SPDIF receiver). The 4 CODEC_SP and the SAI_SP
 * can be configured independently.
 */
struct snd_soc_codec_dai cs4251x_dai = {
	.name = "CS4251x",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS4251X_RATES,    /* depends on clock source */
		.formats = CS4251X_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS4251X_RATES,
		.formats = CS4251X_FORMATS,},
	.ops = {
		.hw_params = cs_hw_params,
	},
	.dai_ops = {
		.digital_mute = cs_mute,
		.set_sysclk = cs_set_dai_sysclk,
		.set_fmt = cs_set_dai_fmt,
	}
};
EXPORT_SYMBOL_GPL(cs4251x_dai);

/*
 * initialise the cs42516/8/28 codec.
 * register the mixer and dsp interfaces with the kernel
 */
static int cs_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	struct cs_priv *cs = codec->private_data;
	struct cs4251x_setup_data *setup = socdev->codec_data;
	int reg, ret;

	codec->owner = THIS_MODULE;
	codec->dapm_event = cs_dapm_event;
	codec->dai = &cs4251x_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(cs4251x_regcache);
	codec->reg_cache = kzalloc(sizeof(cs4251x_regcache), GFP_KERNEL);
	if (!codec->reg_cache)
		return -ENOMEM;

	ret = -ENODEV;
	/* init the chip, populate regcache */
	for (reg = 1; reg < CS4251X_REGNUM; reg++)
		cs4251x_write(codec, reg, cs4251x_regcache[reg]);

	/* identify the chip */
	reg = cs4251x_read(codec, CS4251X_CHIPID);
	if (unlikely(reg < 0))
		goto pcm_err;

	switch (reg & 0xf0)
	{
	case 0xe0:
		codec->name = "CS42518";
		break;
	case 0xf0:
		codec->name = "CS42516/CS42528";
		break;
	default:
		info("unknown chip-id %02x; skipping", reg);
		goto pcm_err;
	}

	/* Hello, World! */
	printk(KERN_INFO "%s Rev. %c I2S Audio Codec\n",
		codec->name, 'A' - 1 + (reg & 15));

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1,
			       SNDRV_DEFAULT_STR1);
	if (unlikely(ret < 0)) {
		err("failed to create pcms");
		goto pcm_err;
	}

	cs_add_controls(codec);
	cs_dapm_event(codec, SNDRV_CTL_POWER_D0);

	ret = snd_soc_register_card(socdev);
	if (unlikely(ret < 0)) {
		err("failed to register card\n");
		goto card_err;
	}

	cs->irq = setup->irq;
	if (cs->irq != CS4251X_NOIRQ) {
		INIT_WORK(&cs->irq_work, cs_irq_work);
		reg = request_irq(cs->irq, cs_irq, 0, "cs4251x", cs);
		if (unlikely(reg)) {
			info("irq attach failed");
			cs->irq = CS4251X_NOIRQ;
		}
	}

	return 0;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static unsigned short normal_i2c[] = { 0, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static struct i2c_driver cs_i2c_driver;
static struct i2c_client client_template;

/* If the i2c layer weren't so broken, we could pass this kind of data
   around */
static struct snd_soc_device *cs_socdev;

static int cs_codec_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct snd_soc_device *socdev = cs_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct cs4251x_setup_data *setup = socdev->codec_data;
	struct i2c_client *i2c;
	int ret;

	if (addr != setup->i2c_address)
		return -ENODEV;

	client_template.adapter = adap;
	client_template.addr = addr;

	i2c = kmemdup(&client_template, sizeof(client_template), GFP_KERNEL);
	if (unlikely(i2c == NULL)) {
		kfree(codec);
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = i2c_attach_client(i2c);
	if (unlikely(ret < 0)) {
		err("failed to attach codec at addr %x", addr);
		goto err;
	}

	codec->read = cs_i2c_read;
	codec->write = cs_i2c_write;

	ret = cs_init(socdev);
	if (unlikely(ret < 0)) {
		err("failed to initialise CS4251x");
		goto err;
	}
	return ret;

err:
	kfree(codec);
	kfree(i2c);
	return ret;
}

static int cs_i2c_detach(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	struct cs_priv *cs = codec->private_data;

	if (cs->irq != CS4251X_NOIRQ) {
		free_irq(cs->irq, cs);
		flush_scheduled_work();
	}

	i2c_detach_client(client);
	kfree(codec->reg_cache);
	kfree(client);

	return 0;
}

static int cs_i2c_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, cs_codec_probe);
}

static struct i2c_driver cs_i2c_driver = {
	.driver = {
		.name	= "cs4251x",
		.owner	= THIS_MODULE,
	},
	.id		= 102, /* I2C_DRIVERID_CS4251X */
	.attach_adapter	= cs_i2c_attach,
	.detach_client	= cs_i2c_detach,
};

static struct i2c_client client_template = {
	.name =   "CS425XX",
	.driver = &cs_i2c_driver,
};
#endif

static int cs_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct cs4251x_setup_data *setup = socdev->codec_data;
	struct snd_soc_codec *codec;
	struct cs_priv *cs;
	int ret;

	ret = -ENOMEM;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		goto out;

	cs = kzalloc(sizeof(struct cs_priv), GFP_KERNEL);
	if (cs == NULL) {
		kfree(codec);
		goto out;
	}
	ret = 0;

	cs->codec = codec;
	codec->private_data = cs;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	cs_socdev = socdev;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		normal_i2c[0] = setup->i2c_address;
		ret = i2c_add_driver(&cs_i2c_driver);
		if (ret)
			err("can't add i2c driver");
	}
#endif

out:
	return ret;
}

/* power down chip */
static int cs_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if (codec->control_data)
		cs_dapm_event(codec, SNDRV_CTL_POWER_D3cold);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&cs_i2c_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_cs4251x = {
	.probe		= cs_probe,
	.remove		= cs_remove,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_cs4251x);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC CS42516/42518 I2S codec driver");
MODULE_AUTHOR("Manuel Lauss <mlau@msc-ge.com>");
