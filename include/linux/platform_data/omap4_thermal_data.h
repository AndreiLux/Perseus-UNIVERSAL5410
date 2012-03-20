#ifndef	OMAP4_THERMAL_DATA_H
#define	OMAP4_THERMAL_DATA_H

#include <linux/cpu_cooling.h>

struct omap4_thermal_data {
	int threshold;
	int trigger_levels[6];
	struct freq_pctg_table freq_tab[6];
	unsigned int freq_tab_count;
};

#endif
