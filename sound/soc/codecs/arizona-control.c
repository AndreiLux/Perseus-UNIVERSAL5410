/*
 * arizona_control.c - WolfonMicro WM51xx kernel-space register control
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: June 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/sysfs.h>
#include <linux/miscdevice.h>

#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/control.h>

static struct snd_soc_codec *codec = NULL;
static int ignore_next = 0;

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t store_arizona_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

#define _control(name_, reg_, mask_, shift_, width_, hook_)\
{ 									\
	.attribute = {							\
			.attr = {					\
				  .name = name_,			\
				  .mode = 0664,				\
				},					\
			.show = show_arizona_property,			\
			.store = store_arizona_property,		\
		     },							\
	.reg 	= reg_ ,						\
	.mask 	= mask_ ,						\
	.shift 	= shift_ ,						\
	.width  = width_ ,						\
	.value 	= -1 ,							\
	.ctlval = 0 ,							\
	.hook 	= hook_							\
}

struct arizona_control {
	const struct device_attribute	attribute;
	u16		reg;
	u16		mask;
	u8		shift;
	u8		width;
	int		value;
	u16		ctlval;
	unsigned int 	(*hook)(struct arizona_control *ctl);
};

static unsigned int generic_override(struct arizona_control *ctl)
{
	return ctl->value;
}

static unsigned int delta_override(struct arizona_control *ctl)
{
	return ((ctl->ctlval + ctl->value) & (ctl->mask >> ctl->shift));;
}

struct arizona_control sound_controls[] = {
	/* Volumes */

	_control("headphone_left" , ARIZONA_DAC_DIGITAL_VOLUME_1L, ARIZONA_OUT1L_VOL_MASK,
		 ARIZONA_OUT1L_VOL_SHIFT, ARIZONA_OUT1L_VOL_WIDTH, generic_override ),
	_control("headphone_right" , ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1R_VOL_MASK,
		 ARIZONA_OUT1R_VOL_SHIFT, ARIZONA_OUT1R_VOL_WIDTH, generic_override ),

	_control("dock_left" , ARIZONA_DAC_DIGITAL_VOLUME_2L, ARIZONA_OUT2L_VOL_MASK,
		 ARIZONA_OUT2L_VOL_SHIFT, ARIZONA_OUT2L_VOL_WIDTH, generic_override ),
	_control("dock_right" , ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2R_VOL_MASK,
		 ARIZONA_OUT2R_VOL_SHIFT, ARIZONA_OUT2R_VOL_WIDTH, generic_override ),

	_control("earpiece_volume" , ARIZONA_DAC_DIGITAL_VOLUME_3L, ARIZONA_OUT3L_VOL_MASK,
		 ARIZONA_OUT3L_VOL_SHIFT, ARIZONA_OUT3L_VOL_WIDTH, delta_override ),

	_control("speaker_volume" , ARIZONA_DAC_DIGITAL_VOLUME_4L, ARIZONA_OUT4L_VOL_MASK,
		 ARIZONA_OUT4L_VOL_SHIFT, ARIZONA_OUT4L_VOL_WIDTH, delta_override ),
};

void arizona_control_regmap_hook(struct regmap *pmap, unsigned int reg,
		      		unsigned int *val)
{
	struct arizona_control *ctl = &sound_controls[0];

	if (codec == NULL || pmap != codec->control_data)
		return;

#if 0
	printk("%s: %x -> %u\n", __func__, reg, *val);
#endif

	if (ignore_next) {
		ignore_next = 0;
		return;
	}

	while (ctl < (&sound_controls[0] + ARRAY_SIZE(sound_controls))) {
		if (ctl->reg == reg) {
			ctl->ctlval = (*val & ctl->mask) >> ctl->shift;

			if (ctl->value == -1) {
				if (ctl->hook == generic_override)
					ctl->value = ctl->ctlval;
				else
					ctl->value = 0;
			}

			if (ctl->hook) {
				*val &= ~ctl->mask;
				*val |= ctl->hook(ctl) << ctl->shift;
			}

			return;
		}
		++ctl;
	}
}

/**** Sysfs ****/

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct arizona_control *ctl = (struct arizona_control*)(attr);

	if (ctl->value == -1)
		ctl->ctlval = ctl->value = (codec->read(codec, ctl->reg) & ctl->mask) >> ctl->shift;

	return sprintf(buf, "%d", ctl->value);
};

static ssize_t store_arizona_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct arizona_control *ctl = (struct arizona_control*)(attr);
	unsigned int regval;
	int val;
	
	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val > (ctl->mask >> ctl->shift))
		val = (ctl->mask >> ctl->shift);

	if (val < -(ctl->mask >> ctl->shift))
		val = -(ctl->mask >> ctl->shift);

	ctl->value = val;

	regval = codec->read(codec, ctl->reg);
	regval &= ~ctl->mask;
	regval |= ctl->hook(ctl) << ctl->shift;

	ignore_next = 1;
	codec->write(codec, ctl->reg, regval);

	return count;
};

static struct miscdevice sound_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wolfson_control",
};

void arizona_control_init(struct snd_soc_codec *pcodec)
{
	int i, ret;

	codec = pcodec;

	misc_register(&sound_dev);

	for(i = 0; i < ARRAY_SIZE(sound_controls); i++) {
		ret = sysfs_create_file(&sound_dev.this_device->kobj,
					&sound_controls[i].attribute.attr);
	}
}

