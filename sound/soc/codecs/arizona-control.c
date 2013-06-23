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

#if 0
#define SYSFS_DBG
#endif

#define NOT_INIT	123456

static struct snd_soc_codec *codec = NULL;
static int ignore_next = 0;

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t store_arizona_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

enum control_type {
	CTL_VIRTUAL = 0,
	CTL_ACTIVE,
	CTL_INERT,
	CTL_HIDDEN,
	CTL_MONITOR,
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
	.value 	= NOT_INIT ,						\
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
	ctl->value = val;
	_write(ctl->reg, 
		(_read(ctl->reg) & ~ctl->mask) | (ctl->hook(ctl) << ctl->shift));
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

static unsigned int __eq_hp_gain(struct arizona_control *ctl)
{
	return _pair(ctl, 22, _delta(ctl, ctl->value, 12));
}

static unsigned int __eq_sp_gain(struct arizona_control *ctl)
{
	return _delta(ctl, ctl->value, 12);
}

static unsigned int __hp_volume(struct arizona_control *ctl)
{
	return ctl->ctlval == 112 ? ctl->value : ctl->ctlval;
}

static unsigned int hp_callback(struct arizona_control *ctl);
static unsigned int hp_power(struct arizona_control *ctl);
static unsigned int sp_callback(struct arizona_control *ctl);
static unsigned int sp_power(struct arizona_control *ctl);
static unsigned int sp_path(struct arizona_control *ctl);
static unsigned int sp_volume(struct arizona_control *ctl);

/* Sound controls */

enum sound_control {
	HPLVOL = 0, HPRVOL, DCKLVOL, DCKRVOL, EPVOL, SPKVOL,
	EQ_HP, DRC_HP, EQ_SP, HP_MONO, SP_PRIV,
	EQ1ENA, EQ2ENA, EQ3ENA, DRC1LENA, DRC1RENA,
	POUT1L, POUT4L,
	EQ1MIX1, EQ1MIX2, EQ2MIX1, EQ3MIX1, EQ3MIX2, DRC1L1, DRC1R1,
	HPOUT1L1, HPOUT1L2, HPOUT1R1, HPOUT1R2, SPOUT1L1, SPOUT1L2,
	HPEQFREQS, SPEQFREQS,
	HPEQB1G, HPEQB2G, HPEQB3G, HPEQB4G, HPEQB5G,
	SPEQB1G, SPEQB2G, SPEQB3G, SPEQB4G, SPEQB5G,
	DRCAC, DRCK2,
};

static struct arizona_control ctls[] = {
	/* Volumes */
	_ctl("headphone_left", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_MASK, ARIZONA_OUT1L_VOL_SHIFT, __hp_volume),
	_ctl("headphone_right", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_1R, 
		ARIZONA_OUT1R_VOL_MASK, ARIZONA_OUT1R_VOL_SHIFT, __hp_volume),

	_ctl("dock_left", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_2L, 
		ARIZONA_OUT2L_VOL_MASK, ARIZONA_OUT2L_VOL_SHIFT, __simple),
	_ctl("dock_right", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_2R,
		ARIZONA_OUT2R_VOL_MASK, ARIZONA_OUT2R_VOL_SHIFT, __simple),

	_ctl("earpiece_volume", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_3L,
		ARIZONA_OUT3L_VOL_MASK, ARIZONA_OUT3L_VOL_SHIFT, __delta),

	_ctl("speaker_volume", CTL_ACTIVE, ARIZONA_DAC_DIGITAL_VOLUME_4L,
		ARIZONA_OUT4L_VOL_MASK, ARIZONA_OUT4L_VOL_SHIFT, sp_volume),

	/* Master switches */

	_ctl("switch_eq_headphone", CTL_VIRTUAL, 0, 0, 0, hp_callback),
	_ctl("switch_drc_headphone", CTL_VIRTUAL, 0, 0, 0, hp_callback),
	_ctl("switch_eq_speaker", CTL_VIRTUAL, 0, 0, 0, sp_callback),
	_ctl("switch_hp_mono", CTL_VIRTUAL, 0, 0, 0, hp_callback),
	_ctl("switch_sp_privacy", CTL_VIRTUAL, 0, 0, 0, sp_power),

	/* Internal switches */

	_ctl("eq1_switch", CTL_INERT, ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_MASK,
		ARIZONA_EQ1_ENA_SHIFT, __simple),
	_ctl("eq2_switch", CTL_INERT, ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_MASK,
		ARIZONA_EQ2_ENA_SHIFT, __simple),
	_ctl("eq3_switch", CTL_INERT, ARIZONA_EQ3_1, ARIZONA_EQ3_ENA_MASK,
		ARIZONA_EQ3_ENA_SHIFT, __simple),

	_ctl("drc1l_switch", CTL_INERT, ARIZONA_DRC1_CTRL1, ARIZONA_DRC1L_ENA_MASK,
		ARIZONA_DRC1L_ENA_SHIFT, __simple),
	_ctl("drc1r_switch", CTL_INERT, ARIZONA_DRC1_CTRL1, ARIZONA_DRC1R_ENA_MASK,
		ARIZONA_DRC1R_ENA_SHIFT, __simple),

	/* Path domain */

	_ctl("out1l_output_switch", CTL_MONITOR, ARIZONA_OUTPUT_ENABLES_1,
		ARIZONA_OUT1L_ENA_MASK, ARIZONA_OUT1L_ENA_SHIFT, hp_power),
	_ctl("out4l_output_switch", CTL_HIDDEN, ARIZONA_OUTPUT_ENABLES_1,
		ARIZONA_OUT4L_ENA_MASK, ARIZONA_OUT4L_ENA_SHIFT, sp_power),

	/* Mixers */

	_ctl("eq1_input1_source",
		CTL_INERT, ARIZONA_EQ1MIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("eq1_input2_source",
		CTL_INERT, ARIZONA_EQ1MIX_INPUT_2_SOURCE, 0xff, 0, __simple),
	_ctl("eq2_input1_source",
		CTL_INERT, ARIZONA_EQ2MIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("eq3_input1_source",
		CTL_INERT, ARIZONA_EQ3MIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("eq3_input2_source",
		CTL_INERT, ARIZONA_EQ3MIX_INPUT_2_SOURCE, 0xff, 0, __simple),
	_ctl("drc1l_input1_source",
		CTL_INERT, ARIZONA_DRC1LMIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("drc1r_input1_source",
		CTL_INERT, ARIZONA_DRC1RMIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("hpout1l_input1_source",
		CTL_INERT, ARIZONA_OUT1LMIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("hpout1l_input2_source",
		CTL_INERT, ARIZONA_OUT1LMIX_INPUT_2_SOURCE, 0xff, 0, __simple),
	_ctl("hpout1r_input1_source",
		CTL_INERT, ARIZONA_OUT1RMIX_INPUT_1_SOURCE, 0xff, 0, __simple),
	_ctl("hpout1r_input2_source",
		CTL_INERT, ARIZONA_OUT1RMIX_INPUT_2_SOURCE, 0xff, 0, __simple),
	_ctl("spout1l_input1_source",
		CTL_HIDDEN, ARIZONA_OUT4LMIX_INPUT_1_SOURCE, 0xff, 0, sp_path),
	_ctl("spout1l_input2_source",
		CTL_INERT, ARIZONA_OUT4LMIX_INPUT_2_SOURCE, 0xff, 0, __simple),

	/* EQ Configurables */

	_ctl("eq_hp_freqs", CTL_VIRTUAL, 0, 0, 0, __simple),
	_ctl("eq_sp_freqs", CTL_VIRTUAL, 0, 0, 0, __simple),

	_ctl("eq_hp_gain_1", CTL_ACTIVE, ARIZONA_EQ1_1, ARIZONA_EQ1_B1_GAIN_MASK,
		ARIZONA_EQ1_B1_GAIN_SHIFT, __eq_hp_gain),
	_ctl("eq_hp_gain_2", CTL_ACTIVE, ARIZONA_EQ1_1, ARIZONA_EQ1_B2_GAIN_MASK,
		ARIZONA_EQ1_B2_GAIN_SHIFT, __eq_hp_gain),
	_ctl("eq_hp_gain_3", CTL_ACTIVE, ARIZONA_EQ1_1, ARIZONA_EQ1_B3_GAIN_MASK,
		ARIZONA_EQ1_B3_GAIN_SHIFT, __eq_hp_gain),
	_ctl("eq_hp_gain_4", CTL_ACTIVE, ARIZONA_EQ1_2, ARIZONA_EQ1_B4_GAIN_MASK,
		ARIZONA_EQ1_B4_GAIN_SHIFT, __eq_hp_gain),
	_ctl("eq_hp_gain_5", CTL_ACTIVE, ARIZONA_EQ1_2, ARIZONA_EQ1_B5_GAIN_MASK,
		ARIZONA_EQ1_B5_GAIN_SHIFT, __eq_hp_gain),

	_ctl("eq_sp_gain_1", CTL_ACTIVE, ARIZONA_EQ3_1, ARIZONA_EQ3_B1_GAIN_MASK,
		ARIZONA_EQ3_B1_GAIN_SHIFT, __eq_sp_gain),
	_ctl("eq_sp_gain_2", CTL_ACTIVE, ARIZONA_EQ3_1, ARIZONA_EQ3_B2_GAIN_MASK,
		ARIZONA_EQ3_B2_GAIN_SHIFT, __eq_sp_gain),
	_ctl("eq_sp_gain_3", CTL_ACTIVE, ARIZONA_EQ3_1, ARIZONA_EQ3_B3_GAIN_MASK,
		ARIZONA_EQ3_B3_GAIN_SHIFT, __eq_sp_gain),
	_ctl("eq_sp_gain_4", CTL_ACTIVE, ARIZONA_EQ3_2, ARIZONA_EQ3_B4_GAIN_MASK,
		ARIZONA_EQ3_B4_GAIN_SHIFT, __eq_sp_gain),
	_ctl("eq_sp_gain_5", CTL_ACTIVE, ARIZONA_EQ3_2, ARIZONA_EQ3_B5_GAIN_MASK,
		ARIZONA_EQ3_B5_GAIN_SHIFT, __eq_sp_gain),

	/* DRC Configurables */

	_ctl("drc_hp_noise_gate", CTL_ACTIVE, ARIZONA_DRC1_CTRL1,
		ARIZONA_DRC1_NG_ENA_MASK, ARIZONA_DRC1_NG_ENA_SHIFT, __simple),
	_ctl("drc_hp_anticlip", CTL_ACTIVE, ARIZONA_DRC1_CTRL1,
		ARIZONA_DRC1_ANTICLIP_MASK, ARIZONA_DRC1_ANTICLIP_SHIFT, __simple),
	_ctl("drc_hp_knee2_operation", CTL_ACTIVE, ARIZONA_DRC1_CTRL1,
		ARIZONA_DRC1_KNEE2_OP_ENA_MASK, ARIZONA_DRC1_KNEE2_OP_ENA_SHIFT, __simple),
	_ctl("drc_hp_quick_release", CTL_ACTIVE, ARIZONA_DRC1_CTRL1,
		ARIZONA_DRC1_QR_MASK, ARIZONA_DRC1_QR_SHIFT, __simple),

	_ctl("drc_hp_attack_rate", CTL_ACTIVE, ARIZONA_DRC1_CTRL2,
		ARIZONA_DRC1_ATK_MASK, ARIZONA_DRC1_ATK_SHIFT, __simple),
	_ctl("drc_hp_decay_rate", CTL_ACTIVE, ARIZONA_DRC1_CTRL2,
		ARIZONA_DRC1_ATK_MASK, ARIZONA_DRC1_ATK_SHIFT, __simple),
	_ctl("drc_hp_min_gain", CTL_ACTIVE, ARIZONA_DRC1_CTRL2,
		ARIZONA_DRC1_MINGAIN_MASK, ARIZONA_DRC1_MINGAIN_SHIFT, __simple),
	_ctl("drc_hp_max_gain", CTL_ACTIVE, ARIZONA_DRC1_CTRL2,
		ARIZONA_DRC1_MAXGAIN_MASK, ARIZONA_DRC1_MAXGAIN_SHIFT, __simple),

	_ctl("drc_hp_noise_gate_min", CTL_ACTIVE, ARIZONA_DRC1_CTRL3,
		ARIZONA_DRC1_NG_MINGAIN_MASK, ARIZONA_DRC1_NG_MINGAIN_SHIFT, __simple),
	_ctl("drc_hp_noise_gate_expansion", CTL_ACTIVE, ARIZONA_DRC1_CTRL3,
		ARIZONA_DRC1_NG_EXP_MASK, ARIZONA_DRC1_NG_EXP_SHIFT, __simple),
	_ctl("drc_hp_quick_release_threshold", CTL_ACTIVE, ARIZONA_DRC1_CTRL3,
		ARIZONA_DRC1_QR_THR_MASK, ARIZONA_DRC1_QR_THR_SHIFT, __simple),
	_ctl("drc_hp_quick_release_decay", CTL_ACTIVE, ARIZONA_DRC1_CTRL3,
		ARIZONA_DRC1_QR_DCY_MASK, ARIZONA_DRC1_QR_DCY_SHIFT, __simple),
	_ctl("drc_hp_high_compression", CTL_ACTIVE, ARIZONA_DRC1_CTRL3,
		ARIZONA_DRC1_HI_COMP_MASK, ARIZONA_DRC1_HI_COMP_SHIFT, __simple),
	_ctl("drc_hp_low_compression", CTL_ACTIVE, ARIZONA_DRC1_CTRL3,
		ARIZONA_DRC1_LO_COMP_MASK, ARIZONA_DRC1_LO_COMP_SHIFT, __simple),

	_ctl("drc_hp_knee_input", CTL_ACTIVE, ARIZONA_DRC1_CTRL4,
		ARIZONA_DRC1_KNEE_IP_MASK, ARIZONA_DRC1_KNEE_IP_SHIFT, __simple),
	_ctl("drc_hp_knee_output", CTL_ACTIVE, ARIZONA_DRC1_CTRL4,
		ARIZONA_DRC1_KNEE_OP_MASK, ARIZONA_DRC1_KNEE_OP_SHIFT, __simple),

	_ctl("drc_hp_knee2_input", CTL_ACTIVE, ARIZONA_DRC1_CTRL5,
		ARIZONA_DRC1_KNEE2_IP_MASK, ARIZONA_DRC1_KNEE2_IP_SHIFT, __simple),
	_ctl("drc_hp_knee2_output", CTL_ACTIVE, ARIZONA_DRC1_CTRL5,
		ARIZONA_DRC1_KNEE2_OP_MASK, ARIZONA_DRC1_KNEE2_OP_SHIFT, __simple),
};

static unsigned int hp_power(struct arizona_control *ctl)
{
	_ctl_set(&ctls[EQ1ENA], ctl->ctlval && ctls[EQ_HP].value);
	_ctl_set(&ctls[EQ2ENA], ctl->ctlval && ctls[EQ_HP].value && !ctls[HP_MONO].value);
	_ctl_set(&ctls[DRC1LENA], ctl->ctlval && ctls[DRC_HP].value);
	_ctl_set(&ctls[DRC1RENA], ctl->ctlval && ctls[DRC_HP].value);

	return ctl->ctlval;
}

static unsigned int hp_callback(struct arizona_control *ctl)
{
	static bool eq_bridge = false;
	static bool eq_bridge_live = false;
	static bool drc_bridge = false;
	static bool drc_bridge_live = false;
	static bool mono_bridge = false;
	static bool mono_bridge_live = false;

	if (ctl->type == CTL_VIRTUAL) {
		if (ctl == &ctls[EQ_HP])
			eq_bridge = !!ctl->value;

		if (ctl == &ctls[DRC_HP])
			drc_bridge = !!ctl->value;

		if (ctl == &ctls[HP_MONO])
			mono_bridge = !!ctl->value;
	}

	if (eq_bridge != eq_bridge_live || drc_bridge != drc_bridge_live ||
	    mono_bridge != mono_bridge_live ) {
		if (eq_bridge) {
			_ctl_set(&ctls[EQ1MIX1], 32);
			_ctl_set(&ctls[EQ1MIX2], mono_bridge ? 33 : 0);
			_ctl_set(&ctls[EQ2MIX1], 33);
			if (drc_bridge) {
				_ctl_set(&ctls[DRC1L1], 80);
				_ctl_set(&ctls[DRC1R1], mono_bridge ? 80 : 81);
				_ctl_set(&ctls[HPOUT1L1], 88);
				_ctl_set(&ctls[HPOUT1R1], 89);
			} else {
				_ctl_set(&ctls[HPOUT1L1], 80);
				_ctl_set(&ctls[HPOUT1R1], mono_bridge ? 80 : 81);
			}
			_ctl_set(&ctls[HPOUT1L2], 0);
			_ctl_set(&ctls[HPOUT1R2], 0);
		} else if (drc_bridge) {
			_ctl_set(&ctls[DRC1L1], 32);
			_ctl_set(&ctls[DRC1R1], 33);
			_ctl_set(&ctls[HPOUT1L1], 88);
			_ctl_set(&ctls[HPOUT1R1], 89);
			_ctl_set(&ctls[HPOUT1L2], mono_bridge ? 89 : 0);
			_ctl_set(&ctls[HPOUT1R2], mono_bridge ? 88 : 0);
		} else {
			_ctl_set(&ctls[HPOUT1L1], 32);
			_ctl_set(&ctls[HPOUT1R1], 33);
			_ctl_set(&ctls[HPOUT1L2], mono_bridge ? 33 : 0);
			_ctl_set(&ctls[HPOUT1R2], mono_bridge ? 32 : 0);
		}
		
		_ctl_set(&ctls[DRC1LENA], drc_bridge);
		_ctl_set(&ctls[DRC1RENA], drc_bridge);
		_ctl_set(&ctls[EQ1ENA], eq_bridge);
		_ctl_set(&ctls[EQ2ENA], eq_bridge && !mono_bridge);

		eq_bridge_live = eq_bridge;
		drc_bridge_live = drc_bridge;
		mono_bridge_live = mono_bridge;
	}

	return ctl->value;
}

static unsigned int sp_volume(struct arizona_control *ctl)
{
	if (!ctls[EQ_SP].value)
		return __delta(ctl);
	else
		return _delta(ctl, ctl->ctlval, ctl->value - 7);
}

static unsigned int sp_path(struct arizona_control *ctl)
{
	return ctl->ctlval == 32 ? (ctls[EQ_SP].value ? 82 : ctl->ctlval) : ctl->ctlval;
}

static unsigned int sp_power(struct arizona_control *ctl)
{
	_ctl_set(&ctls[EQ3ENA], ctl->ctlval && ctls[EQ_SP].value);

	return ctl->ctlval && !(ctls[SP_PRIV].value && ctls[POUT1L].ctlval);
}

static unsigned int sp_callback(struct arizona_control *ctl)
{
	static bool eq_bridge = false;
	static bool eq_bridge_live = false;

	if (ctl->type == CTL_VIRTUAL) {
		if (ctl == &ctls[EQ_SP])
			eq_bridge = !!ctl->value;
	}

	if (eq_bridge != eq_bridge_live) {
		if (eq_bridge) {
			_ctl_set(&ctls[EQ3MIX1], 32);
			_ctl_set(&ctls[EQ3MIX2], 33);
			_ctl_set(&ctls[SPOUT1L1], 82);
			_ctl_set(&ctls[SPOUT1L2], 0);
		} else {
			_ctl_set(&ctls[SPOUT1L1], 32);
			_ctl_set(&ctls[SPOUT1L2], 33);
		}

		_ctl_set(&ctls[EQ3ENA], eq_bridge);
		_ctl_set(&ctls[SPKVOL], ctls[SPKVOL].value);

		eq_bridge_live = eq_bridge;
	}

	return ctl->value;
}

static bool is_delta(struct arizona_control *ctl)
{
	if (ctl->hook == __delta	||
	    ctl->hook == __eq_hp_gain	||  
	    ctl->hook == __eq_sp_gain	||  
	    ctl->hook == sp_volume	  )
		return true;

	return false;
}

static bool is_ignorable(struct arizona_control *ctl)
{
	switch (ctl->type) {
		case CTL_INERT:
		case CTL_MONITOR:
			return true;
		default:
			return false;
	}
}

void arizona_control_regmap_hook(struct regmap *pmap, unsigned int reg,
		      		unsigned int *val)
{
	struct arizona_control *ctl = &ctls[0];
	unsigned int virgin = *val;

	if (codec == NULL || pmap != codec->control_data)
		return;

#ifdef SYSFS_DBG
	printk("%s: pre: %x -> %u\n", __func__, reg, *val);
#endif

	if (ignore_next) {
		ignore_next = false;
		return;
	}

	while (ctl < (&ctls[0] + ARRAY_SIZE(ctls))) {
		if (ctl->reg == reg && ctl->type != CTL_VIRTUAL) {
			ctl->ctlval = (virgin & ctl->mask) >> ctl->shift;

			if (ctl->value == NOT_INIT) {
				if (is_delta(ctl))
					ctl->value = 0;
				else
					ctl->value = ctl->ctlval;
			}

			if (ctl->type == CTL_MONITOR) {
				ctl->value = ctl->ctlval;
				ctl->hook(ctl);
				goto next;
			}

			if (is_ignorable(ctl))
				goto next;

			*val &= ~ctl->mask;
			*val |= ctl->hook(ctl) << ctl->shift;
		}
next:
		++ctl;
	}
}

/**** Sysfs ****/

static unsigned int _eq_freq_store(unsigned int reg, bool pair,
				   const char *buf, size_t count)
{
	unsigned int vals[18] = { 0 };
	int bytes, len = 0, n = 0;

	while (n < 18) {
		sscanf(buf + len, "%x%n", &vals[n], &bytes);
		len += bytes;
		_write(reg + n, vals[n]);
		if (pair)
			_write(reg + n + 22, vals[n]);
		++n;
	}

	return count;
}

static unsigned int _eq_freq_show(unsigned int reg, char *buf)
{
	int len = 0, n = 0;

	while (n < 18)
		len += sprintf(buf + len, "0x%04x ", _read(reg + n++));

	return len;
}

static unsigned int eq_hp_freq_store(const char *buf, size_t count)
{
	return _eq_freq_store(ARIZONA_EQ1_3, true, buf, count);
}

static unsigned int eq_hp_freq_show(char *buf)
{
	return _eq_freq_show(ARIZONA_EQ1_3, buf);
}

static unsigned int eq_sp_freq_store(const char *buf, size_t count)
{
	return _eq_freq_store(ARIZONA_EQ3_3, false, buf, count);
}

static unsigned int eq_sp_freq_show(char *buf)
{
	return _eq_freq_show(ARIZONA_EQ3_3, buf);
}

static ssize_t show_arizona_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct arizona_control *ctl = (struct arizona_control*)(attr);

	if (ctl == &ctls[HPEQFREQS])
		return eq_hp_freq_show(buf);

	if (ctl == &ctls[SPEQFREQS])
		return eq_sp_freq_show(buf);

	if (ctl->value == NOT_INIT) {
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

	if (ctl == &ctls[HPEQFREQS])
		return eq_hp_freq_store(buf, count);

	if (ctl == &ctls[SPEQFREQS])
		return eq_sp_freq_store(buf, count);
	
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
		if (ctls[i].type != CTL_HIDDEN && !is_ignorable(&ctls[i]))
#endif
			ret = sysfs_create_file(&sound_dev.this_device->kobj,
						&ctls[i].attribute.attr);
	}
}

