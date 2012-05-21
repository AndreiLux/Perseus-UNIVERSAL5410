/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/* i.MX6Q Thermal Implementation */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/thermal.h>
#include <linux/io.h>
#include <linux/syscalls.h>
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <linux/cpu_cooling.h>
#include <linux/platform_device.h>

/* register define of anatop */
#define HW_ANADIG_ANA_MISC0			0x00000150
#define HW_ANADIG_ANA_MISC0_SET			0x00000154
#define HW_ANADIG_ANA_MISC0_CLR			0x00000158
#define HW_ANADIG_ANA_MISC0_TOG			0x0000015c
#define BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF	0x00000008

#define HW_ANADIG_TEMPSENSE0			0x00000180
#define HW_ANADIG_TEMPSENSE0_SET		0x00000184
#define HW_ANADIG_TEMPSENSE0_CLR		0x00000188
#define HW_ANADIG_TEMPSENSE0_TOG		0x0000018c

#define BP_ANADIG_TEMPSENSE0_TEMP_VALUE		8
#define BM_ANADIG_TEMPSENSE0_TEMP_VALUE		0x000FFF00
#define BM_ANADIG_TEMPSENSE0_FINISHED		0x00000004
#define BM_ANADIG_TEMPSENSE0_MEASURE_TEMP	0x00000002
#define BM_ANADIG_TEMPSENSE0_POWER_DOWN		0x00000001

#define HW_ANADIG_TEMPSENSE1			0x00000190
#define HW_ANADIG_TEMPSENSE1_SET		0x00000194
#define HW_ANADIG_TEMPSENSE1_CLR		0x00000198
#define BP_ANADIG_TEMPSENSE1_MEASURE_FREQ	0
#define BM_ANADIG_TEMPSENSE1_MEASURE_FREQ	0x0000FFFF

#define HW_OCOTP_ANA1				0x000004E0

#define IMX6Q_THERMAL_POLLING_FREQUENCY_MS 1000

#define IMX6Q_THERMAL_STATE_TRP_PTS 3
/* assumption: always one critical trip point */
#define IMX6Q_THERMAL_TOTAL_TRP_PTS (IMX6Q_THERMAL_STATE_TRP_PTS + 1)

struct th_sys_trip_point {
	u32 temp; /* in celcius */
	enum thermal_trip_type type;
};

struct imx6q_sensor_data {
	int	c1, c2;
	char	name[16];
	u8	meas_delay;	/* in milliseconds */
	u32	noise_margin;   /* in millicelcius */
	int	last_temp;	/* in millicelcius */
	/*
	 * When noise filtering, if consecutive measurements are each higher
	 * up to consec_high_limit times, assume changing temperature readings
	 * to be valid and not noise.
	 */
	u32	consec_high_limit;
};

struct imx6q_thermal_data {
	struct th_sys_trip_point trp_pts[IMX6Q_THERMAL_TOTAL_TRP_PTS];
	struct freq_clip_table freq_tab[IMX6Q_THERMAL_STATE_TRP_PTS];
};

struct imx6q_thermal_zone {
	struct thermal_zone_device	*therm_dev;
	struct thermal_cooling_device	*cool_dev;
	struct imx6q_sensor_data	*sensor_data;
	struct imx6q_thermal_data	*thermal_data;
	struct thermal_zone_device_ops	dev_ops;
};

static struct imx6q_sensor_data g_sensor_data __initdata = {
	.name			= "imx6q-temp_sens",
	.meas_delay		= 1,	/* in milliseconds */
	.noise_margin		= 3000,
	.consec_high_limit	= 3,
};

/*
 * This data defines the various trip points that will trigger action
 * when crossed.
 */
static struct imx6q_thermal_data g_thermal_data __initdata = {
	.trp_pts[0] = {
		.temp	 = 85000,
		.type	 = THERMAL_TRIP_STATE_INSTANCE,
	},
	.freq_tab[0] = {
		.freq_clip_max = 800 * 1000,
	},
	.trp_pts[1] = {
		.temp	 = 90000,
		.type	 = THERMAL_TRIP_STATE_INSTANCE,
	},
	.freq_tab[1] = {
		.freq_clip_max = 400 * 1000,
	},
	.trp_pts[2] = {
		.temp	 = 95000,
		.type	 = THERMAL_TRIP_STATE_INSTANCE,
	},
	.freq_tab[2] = {
		.freq_clip_max = 200 * 1000,
	},
	.trp_pts[3] = {
		.temp	 = 100000,
		.type	 = THERMAL_TRIP_CRITICAL,
	},
};

static int th_sys_get_temp(struct thermal_zone_device *thermal,
				  unsigned long *temp);

static struct imx6q_thermal_zone	*th_zone;
static void __iomem			*anatop_base, *ocotp_base;

static inline int imx6q_get_temp(int *temp, struct imx6q_sensor_data *sd)
{
	unsigned int n_meas;
	unsigned int reg;

	do {
		/*
		 * For now we only using single measure.  Every time we measure
		 * the temperature, we will power on/down the anadig module
		 */
		writel_relaxed(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
			anatop_base + HW_ANADIG_TEMPSENSE0_CLR);

		writel_relaxed(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
			anatop_base + HW_ANADIG_TEMPSENSE0_SET);

		/*
		 * According to imx6q temp sensor designers,
		 * it may require up to ~17us to complete
		 * a measurement.  But this timing isn't checked on every part
		 * and specified in the datasheet, so we check the
		 * 'finished' status bit to be sure of completion.
		 */
		msleep(sd->meas_delay);

		reg = readl_relaxed(anatop_base + HW_ANADIG_TEMPSENSE0);

		writel_relaxed(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
				anatop_base + HW_ANADIG_TEMPSENSE0_CLR);

		writel_relaxed(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
				anatop_base + HW_ANADIG_TEMPSENSE0_SET);

	} while (!(reg & BM_ANADIG_TEMPSENSE0_FINISHED) &&
		 (reg & BM_ANADIG_TEMPSENSE0_TEMP_VALUE));

	n_meas = (reg & BM_ANADIG_TEMPSENSE0_TEMP_VALUE)
			>> BP_ANADIG_TEMPSENSE0_TEMP_VALUE;

	/* See imx6q_thermal_process_fuse_data for forumla derivation. */
	*temp = sd->c2 + (sd->c1 * n_meas);

	pr_debug("Temperature: %d\n", *temp / 1000);

	return 0;
}

static int th_sys_get_temp(struct thermal_zone_device *thermal,
				  unsigned long *temp)
{
	int i, total = 0, tmp = 0;
	const u8 loop = 5;
	u32 consec_high = 0;

	struct imx6q_sensor_data *sd;

	sd = ((struct imx6q_thermal_zone *)thermal->devdata)->sensor_data;

	/*
	 * Measure and handle noise
	 *
	 * While the imx6q temperature sensor is designed to minimize being
	 * affected by system noise, it's safest to run sanity checks and
	 * perform any necessary filitering.
	 */
	for (i = 0; (sd->noise_margin) && (i < loop); i++) {
		imx6q_get_temp(&tmp, sd);

		if ((abs(tmp - sd->last_temp) <= sd->noise_margin) ||
			(consec_high >= sd->consec_high_limit)) {
			sd->last_temp = tmp;
			i = 0;
			break;
		}
		if (tmp > sd->last_temp)
			consec_high++;

		/*
		 * ignore first measurement as the previous measurement was
		 * a long time ago.
		 */
		if (i)
			total += tmp;

		sd->last_temp = tmp;
	}

	if (sd->noise_margin && i)
		tmp = total / (loop - 1);

	/*
	 * The thermal framework code stores temperature in unsigned long. Also,
	 * it has references to "millicelcius" which limits the lowest
	 * temperature possible (compared to Kelvin).
	 */
	if (tmp > 0)
		*temp = tmp;
	else
		*temp = 0;

	return 0;
}

static int th_sys_get_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode *mode)
{
	*mode = THERMAL_DEVICE_ENABLED;
	return 0;
}

static int th_sys_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	struct imx6q_thermal_data *p;

	if (trip >= thermal->trips)
		return -EINVAL;

	p = ((struct imx6q_thermal_zone *)thermal->devdata)->thermal_data;

	*type = p->trp_pts[trip].type;

	return 0;
}

static int th_sys_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				 unsigned long *temp)
{
	struct imx6q_thermal_data *p;

	if (trip >= thermal->trips)
		return -EINVAL;

	p = ((struct imx6q_thermal_zone *)thermal->devdata)->thermal_data;

	*temp = p->trp_pts[trip].temp;

	return 0;
}

static int th_sys_get_crit_temp(struct thermal_zone_device *thermal,
				 unsigned long *temp)
{
	int i;
	struct imx6q_thermal_data *p;

	p = ((struct imx6q_thermal_zone *)thermal->devdata)->thermal_data;

	for (i = thermal->trips ; i > 0 ; i--) {
		if (p->trp_pts[i-1].type == THERMAL_TRIP_CRITICAL) {
			*temp = p->trp_pts[i-1].temp;
			return 0;
		}
	}

	return -EINVAL;
}

static int th_sys_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int i;
	struct thermal_cooling_device *cd;

	cd = ((struct imx6q_thermal_zone *)thermal->devdata)->cool_dev;

	/* if the cooling device is the one from imx6 bind it */
	if (cdev != cd)
		return 0;

	for (i = 0; i < IMX6Q_THERMAL_STATE_TRP_PTS; i++) {
		if (thermal_zone_bind_cooling_device(thermal, i, cdev)) {
			pr_err("error binding cooling dev\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int th_sys_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	int i;

	struct thermal_cooling_device *cd;

	cd = ((struct imx6q_thermal_zone *)thermal->devdata)->cool_dev;

	if (cdev != cd)
		return 0;

	for (i = 0; i < IMX6Q_THERMAL_STATE_TRP_PTS; i++) {
		if (thermal_zone_unbind_cooling_device(thermal, i, cdev)) {
			pr_err("error unbinding cooling dev\n");
			return -EINVAL;
		}
	}

	return 0;
}

static struct thermal_zone_device_ops g_dev_ops __initdata = {
	.bind = th_sys_bind,
	.unbind = th_sys_unbind,
	.get_temp = th_sys_get_temp,
	.get_mode = th_sys_get_mode,
	.get_trip_type = th_sys_get_trip_type,
	.get_trip_temp = th_sys_get_trip_temp,
	.get_crit_temp = th_sys_get_crit_temp,
};

static int imx6q_thermal_process_fuse_data(unsigned int fuse_data,
		 struct imx6q_sensor_data *sd)
{
	int t1, t2, n1, n2;

	if (fuse_data == 0 || fuse_data == 0xffffffff)
		return -EINVAL;

	/*
	 * Fuse data layout:
	 * [31:20] sensor value @ 25C
	 * [19:8] sensor value of hot
	 * [7:0] hot temperature value
	 */
	n1 = fuse_data >> 20;
	n2 = (fuse_data & 0xfff00) >> 8;
	t2 = fuse_data & 0xff;
	t1 = 25; /* t1 always 25C */

	pr_debug(" -- temperature sensor calibration data --\n");
	pr_debug("HW_OCOTP_ANA1: %x\n", fuse_data);
	pr_debug("n1: %d\nn2: %d\nt1: %d\nt2: %d\n", n1, n2, t1, t2);

	/*
	 * Derived from linear interpolation,
	 * Tmeas = T2 + (Nmeas - N2) * (T1 - T2) / (N1 - N2)
	 * We want to reduce this down to the minimum computation necessary
	 * for each temperature read.  Also, we want Tmeas in millicelcius
	 * and we don't want to lose precision from integer division. So...
	 * milli_Tmeas = 1000 * T2 + 1000 * (Nmeas - N2) * (T1 - T2) / (N1 - N2)
	 * Let constant c1 = 1000 * (T1 - T2) / (N1 - N2)
	 * milli_Tmeas = (1000 * T2) + c1 * (Nmeas - N2)
	 * milli_Tmeas = (1000 * T2) + (c1 * Nmeas) - (c1 * N2)
	 * Let constant c2 = (1000 * T2) - (c1 * N2)
	 * milli_Tmeas = c2 + (c1 * Nmeas)
	 */
	sd->c1 = (1000 * (t1 - t2)) / (n1 - n2);
	sd->c2 = (1000 * t2) - (sd->c1 * n2);

	pr_debug("c1: %i\n", sd->c1);
	pr_debug("c2: %i\n", sd->c2);

	return 0;
}

static void __exit imx6q_thermal_exit(void)
{
	if (th_zone && th_zone->therm_dev)
		thermal_zone_device_unregister(th_zone->therm_dev);

	if (th_zone && th_zone->cool_dev)
		cpufreq_cooling_unregister(th_zone->cool_dev);

	kfree(th_zone->sensor_data);
	kfree(th_zone->thermal_data);
	kfree(th_zone);

	if (ocotp_base)
		iounmap(ocotp_base);

	if (anatop_base)
		iounmap(anatop_base);

	pr_info("i.MX6Q: Kernel Thermal management unregistered\n");
}

static int __init imx6q_thermal_init(void)
{
	struct device_node *np_ocotp, *np_anatop;
	unsigned int fuse_data;
	int ret;

	np_ocotp = of_find_compatible_node(NULL, NULL, "fsl,imx6q-ocotp");
	np_anatop = of_find_compatible_node(NULL, NULL, "fsl,imx6q-anatop");

	if (!(np_ocotp && np_anatop))
		return -ENXIO; /* not a compatible platform */

	ocotp_base = of_iomap(np_ocotp, 0);

	if (!ocotp_base) {
		pr_err("Could not retrieve ocotp-base\n");
		ret = -ENXIO;
		goto err_unregister;
	}

	anatop_base = of_iomap(np_anatop, 0);

	if (!anatop_base) {
		pr_err("Could not retrieve anantop-base\n");
		ret = -ENXIO;
		goto err_unregister;
	}

	fuse_data = readl_relaxed(ocotp_base + HW_OCOTP_ANA1);

	th_zone = kzalloc(sizeof(struct imx6q_thermal_zone), GFP_KERNEL);
	if (!th_zone) {
		ret = -ENOMEM;
		goto err_unregister;
	}

	th_zone->dev_ops = g_dev_ops;

	th_zone->thermal_data =
		kzalloc(sizeof(struct imx6q_thermal_data), GFP_KERNEL);

	if (!th_zone->thermal_data) {
		ret = -ENOMEM;
		goto err_unregister;
	}

	memcpy(th_zone->thermal_data, &g_thermal_data,
				sizeof(struct imx6q_thermal_data));

	th_zone->sensor_data =
		kzalloc(sizeof(struct imx6q_sensor_data), GFP_KERNEL);

	if (!th_zone->sensor_data) {
		ret = -ENOMEM;
		goto err_unregister;
	}

	memcpy(th_zone->sensor_data, &g_sensor_data,
				sizeof(struct imx6q_sensor_data));

	ret = imx6q_thermal_process_fuse_data(fuse_data, th_zone->sensor_data);

	if (ret) {
		pr_err("Invalid temperature calibration data.\n");
		goto err_unregister;
	}

	if (!th_zone->sensor_data->meas_delay)
		th_zone->sensor_data->meas_delay = 1;

	/* Make sure sensor is in known good state for measurements */
	writel_relaxed(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
			anatop_base + HW_ANADIG_TEMPSENSE0_CLR);

	writel_relaxed(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
			anatop_base + HW_ANADIG_TEMPSENSE0_CLR);

	writel_relaxed(BM_ANADIG_TEMPSENSE1_MEASURE_FREQ,
			anatop_base + HW_ANADIG_TEMPSENSE1_CLR);

	writel_relaxed(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF,
			anatop_base + HW_ANADIG_ANA_MISC0_SET);

	writel_relaxed(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
			anatop_base + HW_ANADIG_TEMPSENSE0_SET);

	th_zone->cool_dev = cpufreq_cooling_register(
		(struct freq_clip_table *)th_zone->thermal_data->freq_tab,
		IMX6Q_THERMAL_STATE_TRP_PTS, cpumask_of(0),
		THERMAL_TRIP_STATE_INSTANCE);

	if (IS_ERR(th_zone->cool_dev)) {
		pr_err("Failed to register cpufreq cooling device\n");
		ret = -EINVAL;
		goto err_unregister;
	}

	th_zone->therm_dev = thermal_zone_device_register(
		th_zone->sensor_data->name, IMX6Q_THERMAL_TOTAL_TRP_PTS,
		th_zone, &th_zone->dev_ops, 0, 0, 0,
		IMX6Q_THERMAL_POLLING_FREQUENCY_MS);

	if (IS_ERR(th_zone->therm_dev)) {
		pr_err("Failed to register thermal zone device\n");
		ret = -EINVAL;
		goto err_unregister;
	}

	return 0;

err_unregister:
	imx6q_thermal_exit();
	return ret;
}

module_init(imx6q_thermal_init);
module_exit(imx6q_thermal_exit);

MODULE_AUTHOR("Freescale Semiconductor");
MODULE_DESCRIPTION("i.MX6Q SoC thermal driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:imx6q-thermal");
