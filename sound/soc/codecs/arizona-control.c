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

//#define SYSFS_DBG

static struct snd_soc_codec *codec = NULL;
static int ignore_next = 0;

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t store_arizona_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

enum control_type {
	CTL_VIRTUAL = 0,
	CTL_VISIBLE,
	CTL_HIDDEN,
};

#define _ctl(name_, type_, reg_, mask_, shift_, hook_)\
{ 									\
	.attribute = {							\
			.attr = {					\
				  .name = name_,			\
				  .mode = 0664,				\
				},					\
			.show = show_arizona_property,			\
			.store = store_arizona_property,		\
		     },							\
	.type	= type_ ,						\
	.reg 	= reg_ ,						\
	.mask 	= mask_ ,						\
	.shift 	= shift_ ,						\
	.value 	= -1 ,							\
	.ctlval = 0 ,							\
	.hook 	= hook_							\
}

struct arizona_control {
	const struct device_attribute	attribute;
	enum control_type 		type;
	u16				reg;
	u16				mask;
	u8				shift;
	int				value;
	u16				ctlval;
	unsigned int 			(*hook)(struct arizona_control *ctl);
};

/* Prototypes */

#define _write(reg, regval)	\
	ignore_next = true;	\
	codec->write(codec, reg, regval);

#define _read(reg) codec->read(codec, reg)

static inline void _ctl_set(struct arizona_control *ctl, int val)
{
	unsigned int regval = _read(ctl->reg);
	
	ctl->value = val;
	regval = (regval & ~ctl->mask) | (ctl->value << ctl->shift);
	_write(ctl->reg, regval);
}

static unsigned int _pair(struct arizona_control *ctl, int offset,
			   unsigned int value)
{
	_write(ctl->reg + offset,
		(_read(ctl->reg) & ~ctl->mask) | (value << ctl->shift));

	return value;
}

static unsigned int _delta(struct arizona_control *ctl,
			   unsigned int val, int offset)
{
	if ((val + offset) > (ctl->mask >> ctl->shift))
		return (ctl->mask >> ctl->shift);

	if ((val + offset) < 0 )
		return 0;

	return (val + offset);
}

/* Value hooks */

static unsigned int __simple(struct arizona_control *ctl)
{
	return ctl->value;
}

static unsigned int __delta(struct arizona_control *ctl)
{
	return _delta(ctl, ctl->ctlval, ctl->value);
}

static unsigned int __eq_pair(struct arizona_control *ctl)
{
	return _pair(ctl, 22, ctl->value);
}

static unsigned int __eq_gain(struct arizona_control *ctl)
{
	return _pair(ctl, 22, _delta(ctl, ctl->value, 12));
}

static unsigned int __hp_volume(struct arizona_control *ctl)
{
	return ctl->ctlval == 112 ? ctl->value : ctl->ctlval;
}

static unsigned int hp_callback(struct arizona_control *ctl);

/* Sound controls */

enum sound_control {
	HPLVOL = 0, HPRVOL, DCKLVOL, DCKRVOL, EPVOL, SPKVOL,
	EQ_HP, EQ1ENA, EQ2ENA, EQ1MIX1, EQ2MIX1,
	HPOUT1L1, HPOUT1R1,
	HPEQB1G, HPEQB2G, HPEQB3G, HPEQB4G, HPEQB5G,
};

static struct arizona_control ctls[] = {
	/* Volumes */
	_ctl("headphone_left", CTL_VISIBLE, ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_MASK, ARIZONA_OUT1L_VOL_SHIFT, __hp_volume),
	_ctl("headphone_right", CTL_VISIBLE, ARIZONA_DAC_DIGITAL_VOLUME_1R, 
		ARIZONA_OUT1R_VOL_MASK, ARIZONA_OUT1R_VOL_SHIFT, __hp_volume),

	_ctl("dock_left", CTL_VISIBLE, ARIZONA_DAC_DIGITAL_VOLUME_2L, 
		ARIZONA_OUT2L_VOL_MASK, ARIZONA_OUT2L_VOL_SHIFT, __simple),
	_ctl("dock_right", CTL_VISIBLE, ARIZONA_DAC_DIGITAL_VOLUME_2R,
		ARIZONA_OUT2R_VOL_MASK, ARIZONA_OUT2R_VOL_SHIFT, __simple),

	_ctl("earpiece_volume", CTL_VISIBLE, ARIZONA_DAC_DIGITAL_VOLUME_3L,
		ARIZONA_OUT3L_VOL_MASK, ARIZONA_OUT3L_VOL_SHIFT, __delta),

	_ctl("speaker_volume", CTL_VISIBLE, ARIZONA_DAC_DIGITAL_VOLUME_4L,
		ARIZONA_OUT4L_VOL_MASK, ARIZONA_OUT4L_VOL_SHIFT, __delta),

	/* Master switches */

	_ctl("switch_eq_headphone", CTL_VIRTUAL, 0, 0, 0, hp_callback),

	/* Internal switches */

	_ctl("eq1_switch", CTL_HIDDEN, ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_MASK,
		ARIZONA_EQ1_ENA_SHIFT, __simple),
	_ctl("eq2_switch", CTL_HIDDEN, ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_MASK,
		ARIZONA_EQ2_ENA_SHIFT, __simple),

	/* Mixers */

	_ctl("eq1_input1_source",
		CTL_HIDDEN, ARIZONA_EQ1MIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("eq2_input1_source",
		CTL_HIDDEN, ARIZONA_EQ2MIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("hpout1l_input1_source",
		CTL_HIDDEN, ARIZONA_OUT1LMIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("hpout1r_input1_source",
		CTL_HIDDEN, ARIZONA_OUT1RMIX_INPUT_1_SOURCE, 0xff, 0, __simple),

	/* EQ Configurables */

	_ctl("eq_hp_gain_1", CTL_VISIBLE, ARIZONA_EQ1_1, ARIZONA_EQ1_B1_GAIN_MASK,
		ARIZONA_EQ1_B1_GAIN_SHIFT, __eq_gain),
	_ctl("eq_hp_gain_2", CTL_VISIBLE, ARIZONA_EQ1_1, ARIZONA_EQ1_B2_GAIN_MASK,
		ARIZONA_EQ1_B2_GAIN_SHIFT, __eq_gain),
	_ctl("eq_hp_gain_3", CTL_VISIBLE, ARIZONA_EQ1_1, ARIZONA_EQ1_B3_GAIN_MASK,
		ARIZONA_EQ1_B3_GAIN_SHIFT, __eq_gain),
	_ctl("eq_hp_gain_4", CTL_VISIBLE, ARIZONA_EQ1_2, ARIZONA_EQ1_B4_GAIN_MASK,
		ARIZONA_EQ1_B4_GAIN_SHIFT, __eq_gain),
	_ctl("eq_hp_gain_5", CTL_VISIBLE, ARIZONA_EQ1_2, ARIZONA_EQ1_B5_GAIN_MASK,
		ARIZONA_EQ1_B5_GAIN_SHIFT, __eq_gain),

};

static unsigned int hp_callback(struct arizona_control *ctl)
{
	static bool eq_bridge = false;
	static bool eq_bridge_live = false;

	if (ctl->type == CTL_VIRTUAL) {
		if (ctl == &ctls[EQ_HP]) {
			eq_bridge = !!ctl->value;
		}
	}

	if (eq_bridge != eq_bridge_live) {
		if (eq_bridge) {
			_ctl_set(&ctls[HPOUT1L1], 0);
			_ctl_set(&ctls[HPOUT1R1], 0);
			_ctl_set(&ctls[EQ1MIX1], 32);
			_ctl_set(&ctls[EQ2MIX1], 33);
			_ctl_set(&ctls[HPOUT1L1], 80);
			_ctl_set(&ctls[HPOUT1R1], 81);
			_ctl_set(&ctls[EQ1ENA], 1);
			_ctl_set(&ctls[EQ2ENA], 1);
		} else {
			_ctl_set(&ctls[EQ1ENA], 0);
			_ctl_set(&ctls[EQ2ENA], 0);
			_ctl_set(&ctls[EQ1MIX1], 0);
			_ctl_set(&ctls[EQ2MIX1], 0);
			_ctl_set(&ctls[HPOUT1L1], 32);
			_ctl_set(&ctls[HPOUT1R1], 33);
		}

		eq_bridge_live = eq_bridge;
	}

	return ctl->value;
}

static bool is_delta(struct arizona_control *ctl)
{
	if (ctl->hook == __delta	||
	    ctl->hook == __eq_gain	  )
		return true;

	return false;
}

void arizona_control_regmap_hook(struct regmap *pmap, unsigned int reg,
		      		unsigned int *val)
{
	struct arizona_control *ctl = &ctls[0];

	if (codec == NULL || pmap != codec->control_data)
		return;

#if 0
	printk("%s: pre: %x -> %u\n", __func__, reg, *val);
#endif

	if (ignore_next) {
		ignore_next = false;
		return;
	}

	while (ctl < (&ctls[0] + ARRAY_SIZE(ctls))) {
		if (ctl->reg == reg && ctl->type != CTL_VIRTUAL) {
			ctl->ctlval = (*val & ctl->mask) >> ctl->shift;

			if (ctl->value == -1) {
				if (is_delta(ctl))
					ctl->value = 0;
				else
					ctl->value = ctl->ctlval;
			}

			if (ctl->hook && ctl->type != CTL_HIDDEN) {
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

	if (ctl->value == -1) {
		ctl->ctlval = (_read(ctl->reg) & ctl->mask) >> ctl->shift;
		if (is_delta(ctl))
			ctl->value = 0;
		else
			ctl->value = ctl->ctlval;
	}

#ifdef SYSFS_DBG
	return sprintf(buf, "%d %d %u", ctl->value,
		(_read(ctl->reg) & ctl->mask) >> ctl->shift, ctl->ctlval);
#endif
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

	if (ctl->type != CTL_VIRTUAL) {
		if (val > (ctl->mask >> ctl->shift))
			val = (ctl->mask >> ctl->shift);

		if (val < -(ctl->mask >> ctl->shift))
			val = -(ctl->mask >> ctl->shift);

		ctl->value = val;

		regval = _read(ctl->reg);
		regval &= ~ctl->mask;
		regval |= ctl->hook(ctl) << ctl->shift;

		_write(ctl->reg, regval);
	} else {
		ctl->value = val;
		ctl->hook(ctl);
	}

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

	for(i = 0; i < ARRAY_SIZE(ctls); i++) {
#ifndef SYSFS_DBG
		if (ctls[i].type != CTL_HIDDEN)
#endif
			ret = sysfs_create_file(&sound_dev.this_device->kobj,
						&ctls[i].attribute.attr);
	}
}

