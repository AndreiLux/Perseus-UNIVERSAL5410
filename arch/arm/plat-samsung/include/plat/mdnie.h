#ifndef __MDNIE_H__
#define __MDNIE_H__

#define END_SEQ			0xffff

enum MODE {
	DYNAMIC,
	STANDARD,
#if !defined(CONFIG_FB_MDNIE_PWM)
	NATURAL,
#endif
	MOVIE,
	AUTO,
	MODE_MAX,
};

enum SCENARIO {
	UI_MODE,
	VIDEO_MODE,
	VIDEO_WARM_MODE,
	VIDEO_COLD_MODE,
	CAMERA_MODE,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	BROWSER_MODE,
	EBOOK_MODE,
	SCENARIO_MAX,
	DMB_NORMAL_MODE = 20,
	DMB_WARM_MODE,
	DMB_COLD_MODE,
	DMB_MODE_MAX,
	COLOR_TONE_1 = 40,
	COLOR_TONE_2,
	COLOR_TONE_3,
	COLOR_TONE_MAX,
};

enum OUTDOOR {
	OUTDOOR_OFF,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

enum TONE {
	TONE_NORMAL,
	TONE_WARM,
	TONE_COLD,
	TONE_MAX,
};

enum CABC {
	CABC_OFF,
#if defined(CONFIG_FB_MDNIE_PWM)
	CABC_ON,
#endif
	CABC_MAX,
};

enum POWER_LUT {
	LUT_DEFAULT,
	LUT_VIDEO,
	LUT_MAX,
};

enum NEGATIVE {
	NEGATIVE_OFF,
	NEGATIVE_ON,
	NEGATIVE_MAX,
};

enum BYPASS {
	BYPASS_OFF,
	BYPASS_ON,
	BYPASS_MAX,
};

enum ACCESSIBILITY {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	ACCESSIBILITY_MAX,
};

struct mdnie_tuning_info {
	char *name;
	unsigned short * const sequence;
};

struct mdnie_info {
	struct resource			*mem;
	void __iomem 			*base;

	struct clk			*bus_clk;
	struct clk			*clk;

	struct device			*dev;
	struct mutex			dev_lock;
	struct mutex			lock;

	unsigned int enable;
	enum SCENARIO scenario;
	enum MODE mode;
	enum TONE tone;
	enum OUTDOOR outdoor;
	enum CABC cabc;
	enum BYPASS bypass;
	unsigned int tuning;
	unsigned int negative;
	unsigned int accessibility;
	unsigned int color_correction;
	char path[50];

	struct notifier_block fb_notif;
};

extern struct mdnie_info *g_mdnie;

int s3c_mdnie_set_size(unsigned int hsize, unsigned int vsize);

void mdnie_update(struct mdnie_info *mdnie, u8 force);
int s3c_mdnie_hw_init(void);

extern int mdnie_calibration(unsigned short x, unsigned short y, int *r);
extern int mdnie_request_firmware(const char *path, u16 **buf, char *name);
extern int mdnie_open_file(const char *path, char **fp);

#endif /* __MDNIE_H__ */
