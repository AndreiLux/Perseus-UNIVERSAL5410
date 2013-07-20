/*
 * mdnie_control.c - mDNIe register sequence intercept and control
 *
 * @Author	: Andrei F. <https://github.com/AndreiLux>
 * @Date	: February 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/device.h>

#include "mdnie.h"

#define REFRESH_DELAY		HZ / 2
static struct delayed_work mdnie_refresh_work;

static bool reg_hook = 0;
static bool sequence_hook = 0;

/* Defined as negative deltas */
static int br_reduction = 75;
static int br_takeover_point = 30;
static int br_brightness_delta = 20;
static int br_component_reduction = 0;

static int black_component_increase = 0;
static int black_component_increase_value = 0;
static int black_component_increase_point = 255;

enum mdnie_registers {
	EFFECT_MASTER1	= 0x08,	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	EFFECT_MASTER2	= 0x09,	/*MCM off*/
	EFFECT_MASTER3	= 0x39,	/*UC off*/

	DE_EGTH		= 0xb0,	/*DE egth*/
	DE_PE		= 0xb2, /*DE pe*/
	DE_PF		= 0xb3,	/*DE pf*/
	DE_PB		= 0xb4,	/*DE pb*/
	DE_NE		= 0xb5,	/*DE ne*/
	DE_NF		= 0xb6,	/*DE nf*/
	DE_NB		= 0xb7,	/*DE nb*/
	DE_MAX_RATIO	= 0xb8,	/*DE max ratio*/
	DE_MIN_RATIO	= 0xb9,	/*DE min ratio*/

	CS_HG_RY	= 0xc0,	/*CS hg ry*/
	CS_HG_GC	= 0xc1,	/*CS hg gc*/
	CS_HG_BM	= 0xc2,	/*CS hg bm*/
	CS_WEIGHT_GRTH	= 0xc3,	/*CS weight grayTH*/

	SCR_RR_CR	= 0x71,	/*SCR RrCr*/
	SCR_RG_CG	= 0x72,	/*SCR RgCg*/
	SCR_RB_CB	= 0x73,	/*SCR RbCb*/

	SCR_GR_MR	= 0x74,	/*SCR GrMr*/
	SCR_GG_MG	= 0x75,	/*SCR GgMg*/
	SCR_GB_MB	= 0x76,	/*SCR GbMb*/

	SCR_BR_YR	= 0x77,	/*SCR BrYr*/
	SCR_BG_YG	= 0x78,	/*SCR BgYg*/
	SCR_BB_YB	= 0x79,	/*SCR BbYb*/

	SCR_KR_WR	= 0x7a,	/*SCR KrWr*/
	SCR_KG_WG	= 0x7b,	/*SCR KgWg*/
	SCR_KB_WB	= 0x7c,	/*SCR KbWb*/

	CC_CHSEL_STR	= 0x3f,	/*CC chsel strength*/
	CC_0		= 0x40,	/*CC lut r   0*/
	CC_1		= 0x41,	/*CC lut r  16 144*/
	CC_2		= 0x42,	/*CC lut r  32 160*/
	CC_3		= 0x43,	/*CC lut r  48 176*/
	CC_4		= 0x44,	/*CC lut r  64 192*/
	CC_5		= 0x45,	/*CC lut r  80 208*/
	CC_6		= 0x46,	/*CC lut r  96 224*/
	CC_7		= 0x47,	/*CC lut r 112 240*/
	CC_8		= 0x48	/*CC lut r 128 255*/
};

static unsigned short master_sequence[] = { 
	0x0000, 0x0000,	 0x0008, 0x0300,  0x0009, 0x0000,  0x000a, 0x0000,
	0x00b0, 0x0080,  0x00b2, 0x0000,  0x00b3, 0x0040,  0x00b4, 0x0040,
	0x00b5, 0x0040,  0x00b6, 0x0040,  0x00b7, 0x0040,  0x00b8, 0x1000,
	0x00b9, 0x0100,  0x00c0, 0x1010,  0x00c1, 0x1010,  0x00c2, 0x1010,

	0x00c3, 0x1804,  0x0000, 0x0001,  0x003f, 0x0080,  0x0040, 0x0000,
	0x0041, 0x1090,  0x0042, 0x1da0,  0x0043, 0x30b0,  0x0044, 0x40c0,
	0x0045, 0x50d0,  0x0046, 0x60e0,  0x0047, 0x70f0,  0x0048, 0x80ff,
	0x0071, 0xd981,  0x0072, 0x1cf6,  0x0073, 0x13ec,  0x0074, 0x52e0,

	0x0075, 0xee34,  0x0076, 0x1ff5,  0x0077, 0x1ce9,  0x0078, 0x1ff3,
	0x0079, 0xeb40,  0x007a, 0x00ff,  0x007b, 0x00fa,  0x007c, 0x00f3,
	0x00ff, 0x0000,  0xffff, 0x0000
};

static ssize_t show_mdnie_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t store_mdnie_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);

#define _effect(name_, reg_, mask_, shift_, regval_)\
{ 									\
	.attribute = {							\
			.attr = {					\
				  .name = name_,			\
				  .mode = 0664,				\
				},					\
			.show = show_mdnie_property,			\
			.store = store_mdnie_property,			\
		     },							\
	.reg 	= reg_ ,						\
	.mask 	= mask_ ,						\
	.shift 	= shift_ ,						\
	.value 	= 0 ,							\
	.regval = regval_						\
}

struct mdnie_effect {
	const struct device_attribute	attribute;
	u8				reg;
	u16				mask;
	u8				shift;
	int				value;
	u16				regval;
};

struct mdnie_effect mdnie_controls[] = {

	/* Master switches */

	_effect("s_channel_filters"	, EFFECT_MASTER1, (1 << 9), 9 , 1 ),
	_effect("s_gamma_curve"		, EFFECT_MASTER1, (1 << 8), 8 , 1 ),

	_effect("s_chroma_saturation"	, EFFECT_MASTER1, (1 << 5), 5 , 1 ),
	_effect("s_edge_enhancement"	, EFFECT_MASTER1, (1 << 4), 4 , 0 ),

	_effect("s_log"			, EFFECT_MASTER1, (1 << 3), 3 , 0 ),
	_effect("s_wiener"		, EFFECT_MASTER1, (1 << 2), 2 , 0 ),
	_effect("s_noise_reduction"	, EFFECT_MASTER1, (1 << 1), 1 , 0 ),
	_effect("s_high_dynamic_range"	, EFFECT_MASTER1, (1 << 0), 0 , 0 ),

	/* Ditigal edge enhancement */

	_effect("de_egth"		, DE_EGTH	, 0x00ff, 0	, 128	),

	_effect("de_positive_e"		, DE_PE		, 0x00ff, 0	, 48	),
	_effect("de_positive_f"		, DE_PF		, 0x00ff, 0	, 96	),
	_effect("de_positive_b"		, DE_PB		, 0x00ff, 0	, 96	),

	_effect("de_negative_e"		, DE_NE		, 0x00ff, 0	, 48	),
	_effect("de_negative_f"		, DE_NF		, 0x00ff, 0	, 96	),
	_effect("de_negative_b"		, DE_NB		, 0x00ff, 0	, 96	),

	_effect("de_min_ratio"		, DE_MIN_RATIO	, 0xffff, 0	, 256	),
	_effect("de_max_ratio"		, DE_MAX_RATIO	, 0xffff, 0	, 4096	),

	/* Chroma saturation */

	_effect("cs_weight"		, CS_WEIGHT_GRTH, 0xff00, 8	, 9	),
	_effect("cs_gray_threshold"	, CS_WEIGHT_GRTH, 0x00ff, 0	, 4	),

	_effect("cs_red"		, CS_HG_RY	, 0xff00, 8	, 18	),
	_effect("cs_green"		, CS_HG_GC	, 0xff00, 8	, 14	),
	_effect("cs_blue"		, CS_HG_BM	, 0xff00, 8	, 16	),

	_effect("cs_yellow"		, CS_HG_RY	, 0x00ff, 0	, 19	),
	_effect("cs_cyan"		, CS_HG_GC	, 0x00ff, 0	, 10	),
	_effect("cs_magenta"		, CS_HG_BM	, 0x00ff, 0	, 16	),

	/* Colour channel pass-through filters
	 * scr_x_y:
	 *	x = Channel
	 *	y = Channel component modifier
	 */

	_effect("scr_red_red"		, SCR_RR_CR	, 0xff00, 8	, 247	),
	_effect("scr_red_green"		, SCR_RG_CG	, 0xff00, 8	, 17	),
	_effect("scr_red_blue"		, SCR_RB_CB	, 0xff00, 8	, 0	),

	_effect("scr_cyan_red"		, SCR_RR_CR	, 0x00ff, 0	, 42	),
	_effect("scr_cyan_green"	, SCR_RG_CG	, 0x00ff, 0	, 240	),
	_effect("scr_cyan_blue"		, SCR_RB_CB	, 0x00ff, 0	, 255	),
	
	_effect("scr_green_red"		, SCR_GR_MR	, 0xff00, 8	, 64	),
	_effect("scr_green_green"	, SCR_GG_MG	, 0xff00, 8	, 245	),
	_effect("scr_green_blue"	, SCR_GB_MB	, 0xff00, 8	, 0	),

	_effect("scr_magenta_red"	, SCR_GR_MR	, 0x00ff, 0	, 255	),
	_effect("scr_magenta_green"	, SCR_GG_MG	, 0x00ff, 0	, 20	),
	_effect("scr_magenta_blue"	, SCR_GB_MB	, 0x00ff, 0	, 255	),
	
	_effect("scr_blue_red"		, SCR_BR_YR	, 0xff00, 8	, 0	),
	_effect("scr_blue_green"	, SCR_BG_YG	, 0xff00, 8	, 0	),
	_effect("scr_blue_blue"		, SCR_BB_YB	, 0xff00, 8	, 255	),

	_effect("scr_yellow_red"	, SCR_BR_YR	, 0x00ff, 0	, 255	),
	_effect("scr_yellow_green"	, SCR_BG_YG	, 0x00ff, 0	, 241	),
	_effect("scr_yellow_blue"	, SCR_BB_YB	, 0x00ff, 0	, 8	),

	_effect("scr_black_red"		, SCR_KR_WR	, 0xff00, 8	, 0	),
	_effect("scr_black_green"	, SCR_KG_WG	, 0xff00, 8	, 0	),
	_effect("scr_black_blue"	, SCR_KB_WB	, 0xff00, 8	, 0	),

	_effect("scr_white_red"		, SCR_KR_WR	, 0x00ff, 0	, 255	),
	_effect("scr_white_green"	, SCR_KG_WG	, 0x00ff, 0	, 245	),
	_effect("scr_white_blue"	, SCR_KB_WB	, 0x00ff, 0	, 246	),

	/* Greyscale gamma curve */

	_effect("cc_channel_strength"	, CC_CHSEL_STR	, 0xffff, 0	, 128	),
	
	_effect("cc_0"			, CC_0		, 0x00ff, 0	, 0	),
	_effect("cc_16"			, CC_1		, 0xff00, 8	, 24	),
	_effect("cc_32"			, CC_2		, 0xff00, 8	, 31	),
	_effect("cc_48"			, CC_3		, 0xff00, 8	, 49	),
	_effect("cc_64"			, CC_4		, 0xff00, 8	, 61	),
	_effect("cc_80"			, CC_5		, 0xff00, 8	, 82	),
	_effect("cc_96"			, CC_6		, 0xff00, 8	, 98	),
	_effect("cc_112"		, CC_7		, 0xff00, 8	, 114	),
	_effect("cc_128"		, CC_8		, 0xff00, 8	, 131	),
	_effect("cc_144"		, CC_1		, 0x00ff, 0	, 145	),
	_effect("cc_160"		, CC_2		, 0x00ff, 0	, 163	),
	_effect("cc_176"		, CC_3		, 0x00ff, 0	, 178	),
	_effect("cc_192"		, CC_4		, 0x00ff, 0	, 192	),
	_effect("cc_208"		, CC_5		, 0x00ff, 0	, 208	),
	_effect("cc_224"		, CC_6		, 0x00ff, 0	, 225	),
	_effect("cc_240"		, CC_7		, 0x00ff, 0	, 240	),
	_effect("cc_255"		, CC_8		, 0x00ff, 0	, 255	),
};

static int is_switch(unsigned int reg)
{
	switch(reg) {
		case EFFECT_MASTER1:
		case EFFECT_MASTER2:
		case EFFECT_MASTER3:
			return true;
		default:
			return false;
	}
}

static int effect_switch_hook(struct mdnie_effect *effect, unsigned short regval)
{
	return effect->value ? !regval : regval;
}

static int secondary_hook(struct mdnie_effect *effect, int val)
{
	val += effect->value;

	switch (effect->reg) {
		case SCR_KR_WR...SCR_KB_WB:
			if (effect->shift) {
				val += black_component_increase;
				break;
			}
		case SCR_RR_CR...SCR_BB_YB:
			val -= br_component_reduction;
	}

	return val;
}

unsigned short mdnie_reg_hook(unsigned short reg, unsigned short value)
{
	struct mdnie_effect *effect = (struct mdnie_effect*)&mdnie_controls;
	int i;
	int tmp, original;
	unsigned short regval;

	original = value;

	if (unlikely((!sequence_hook && !reg_hook) || g_mdnie->negative == NEGATIVE_ON))
		return value;

	for (i = 0; i < ARRAY_SIZE(mdnie_controls); i++) {
	    if (unlikely(effect->reg == reg)) {
		if (likely(sequence_hook)) {
			tmp = regval = effect->regval;
		} else {
			tmp = regval = (value & effect->mask) >> effect->shift;
		}

		if (likely(reg_hook)) {
			if (is_switch(reg))
				tmp = effect_switch_hook(effect, regval);
			else
				tmp = secondary_hook(effect, tmp);

			if(tmp > (effect->mask >> effect->shift))
				tmp = (effect->mask >> effect->shift);

			if(tmp < 0)
				tmp = 0;

			regval = (unsigned short)tmp;
		}

		value &= ~effect->mask;
		value |= regval << effect->shift;
#if 0
		printk("mdnie: hook on: 0x%X val: 0x%X -> 0x%X effect: \t%4d : \t%s \n",
			reg, original, value, tmp, effect->attribute.attr.name);
#endif
	    }
	    ++effect;
	}
	
	return value;
}

unsigned short *mdnie_sequence_hook(unsigned short *seq)
{
	if(!sequence_hook || g_mdnie->negative == NEGATIVE_ON)
		return seq;

	return (unsigned short *)&master_sequence;
}

void do_mdnie_refresh(struct work_struct *work)
{
	mdnie_update(g_mdnie, 1);
}

void mdnie_update_brightness(int brightness, bool is_auto, bool force)
{
	static int prev_brightness = 255;
	static int prev_auto = false;
	int weight, adjusted_brightness, tmp, refresh = 0;

	if (unlikely(force)) {
		brightness = prev_brightness;
		is_auto = prev_auto;
	}

	adjusted_brightness = brightness + (is_auto ? br_brightness_delta : 0);

	if(unlikely(adjusted_brightness < 1))
		adjusted_brightness = 1;

	tmp = black_component_increase;
	black_component_increase = (brightness > black_component_increase_point)
					? 0 : black_component_increase_value;

	if (tmp != black_component_increase)
		refresh += true;

	tmp = br_component_reduction;

	if (adjusted_brightness > br_takeover_point) {
		br_component_reduction = 0;
		goto update;
	}

	weight = 1000 - ((adjusted_brightness * 1000) / br_takeover_point);
	br_component_reduction = ((br_reduction) * weight) / 1000;

	if (tmp != br_component_reduction)
		refresh += true;

update:
	if (refresh)
		do_mdnie_refresh(NULL);

	prev_brightness = brightness;
	prev_auto = is_auto;

	return;
}

static inline void scheduled_refresh(void)
{
	cancel_delayed_work_sync(&mdnie_refresh_work);
	schedule_delayed_work_on(0, &mdnie_refresh_work, REFRESH_DELAY);
}

static inline void forced_brightness(void)
{ 
	mdnie_update_brightness(0, false, true);
}

/**** Sysfs ****/

static ssize_t show_mdnie_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mdnie_effect *effect = (struct mdnie_effect*)(attr);


	if(is_switch(effect->reg))
		return sprintf(buf, "%d", effect->value);
	
	return sprintf(buf, "%d", effect->value);
};

static ssize_t store_mdnie_property(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct mdnie_effect *effect = (struct mdnie_effect*)(attr);
	int val;
	
	if(sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if(is_switch(effect->reg)) {
		effect->value = val;
	} else {
		if(val > (effect->mask >> effect->shift))
			val = (effect->mask >> effect->shift);

		if(val < -(effect->mask >> effect->shift))
			val = -(effect->mask >> effect->shift);

		effect->value = val;
	}

	scheduled_refresh();

	return count;
};

#define MAIN_CONTROL(_name, _var, _callback) \
static ssize_t show_##_name(struct device *dev,					\
				    struct device_attribute *attr, char *buf)	\
{										\
	return sprintf(buf, "%d", _var);					\
};										\
static ssize_t store_##_name(struct device *dev,				\
				     struct device_attribute *attr,		\
				     const char *buf, size_t count)		\
{										\
	int val;								\
										\
	if(sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
										\
	_var = val;								\
										\
	_callback();								\
										\
	return count;								\
};

MAIN_CONTROL(reg_hook, reg_hook, scheduled_refresh);
MAIN_CONTROL(sequence_intercept, sequence_hook, scheduled_refresh);
MAIN_CONTROL(br_reduction, br_reduction, forced_brightness);
MAIN_CONTROL(br_takeover_point, br_takeover_point, forced_brightness);
MAIN_CONTROL(br_brightness_delta, br_brightness_delta, forced_brightness);

MAIN_CONTROL(black_increase_value, black_component_increase_value, forced_brightness);
MAIN_CONTROL(black_increase_point, black_component_increase_point, forced_brightness);

DEVICE_ATTR(hook_intercept, 0664, show_reg_hook, store_reg_hook);
DEVICE_ATTR(sequence_intercept, 0664, show_sequence_intercept, store_sequence_intercept);
DEVICE_ATTR(brightness_reduction, 0664, show_br_reduction, store_br_reduction);
DEVICE_ATTR(brightness_takeover_point, 0664, show_br_takeover_point, store_br_takeover_point);
DEVICE_ATTR(brightness_input_delta, 0664, show_br_brightness_delta, store_br_brightness_delta);

DEVICE_ATTR(black_increase_value, 0664, show_black_increase_value, store_black_increase_value);
DEVICE_ATTR(black_increase_point, 0664, show_black_increase_point, store_black_increase_point);

void init_intercept_control(struct kobject *kobj)
{
	int i, ret;
	struct kobject *subdir;

	subdir = kobject_create_and_add("hook_control", kobj);

	for(i = 0; i < ARRAY_SIZE(mdnie_controls); i++) {
		ret = sysfs_create_file(subdir, &mdnie_controls[i].attribute.attr);
	}

	ret = sysfs_create_file(kobj, &dev_attr_hook_intercept.attr);
	ret = sysfs_create_file(kobj, &dev_attr_sequence_intercept.attr);
	ret = sysfs_create_file(kobj, &dev_attr_brightness_reduction.attr);
	ret = sysfs_create_file(kobj, &dev_attr_brightness_takeover_point.attr);
	ret = sysfs_create_file(kobj, &dev_attr_brightness_input_delta.attr);

	ret = sysfs_create_file(kobj, &dev_attr_black_increase_value.attr);
	ret = sysfs_create_file(kobj, &dev_attr_black_increase_point.attr);

	INIT_DELAYED_WORK(&mdnie_refresh_work, do_mdnie_refresh);
}
