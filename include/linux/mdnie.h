#ifndef __MDNIE_H__
#define __MDNIE__

struct platform_mdnie_data {
	unsigned int	display_type;
    unsigned int    support_pwm;
#if defined (CONFIG_S5P_MDNIE_PWM)
    int pwm_out_no;
	int pwm_out_func;
    char *name;
    int *br_table;
    int dft_bl;
#endif
	struct lcd_platform_data	*lcd_pd;
};

#endif
