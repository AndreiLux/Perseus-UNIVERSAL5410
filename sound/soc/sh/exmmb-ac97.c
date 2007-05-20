/*
 * EXM32 Motherboard AC97 sound; for SH7760 based CPU Modules.
 *
 * Copyright (c) 2007 MSC Vertriebsges.m.b.H.
 * Written by Manuel Lauss <mlau@msc-ge.com>
 * see http://www.exm32.com
 *
 * Licensed under the GPLv2.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asm/exm7760/exm7760.h>
#include <asm/io.h>

#include <linux/exm32.h>
#include "../codecs/ac97.h"

/* #define MACH_DEBUG */

#ifdef MACH_DEBUG
#define MSG(x...)	printk(KERN_INFO "exm7760-ac97: " x)
#else
#define MSG(x...)	do {} while (0)
#endif

#define IPSEL 0xFE400034

/* platform specific structs can be declared here */
extern struct snd_soc_cpu_dai sh4_hac_dai[2];
extern struct snd_soc_platform sh7760_soc_platform;
extern struct snd_ac97_bus_ops soc_ac97_ops;

/*
 * Initialise the machine audio subsystem.
 */
static int exm7760_ac97_machine_init(struct snd_soc_codec *codec)
{
	MSG("machine_init() enter\n");

	snd_soc_dapm_sync_endpoints(codec);

	MSG("machine_init() leave\n");
	return 0;
}

static struct snd_soc_dai_link exm7760_ac97_dai = {
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &sh4_hac_dai[0],	/* SH7760 HAC channel 0 */
	.codec_dai = &ac97_dai,
	.init = exm7760_ac97_machine_init,
	.ops = NULL,
};

static struct snd_soc_machine snd_soc_machine_exm97 = {
	.name = "EXM32 Motherboard AC97",
	.dai_link = &exm7760_ac97_dai,
	.num_links = 1,
};

static struct snd_soc_device exm7760_ac97_snd_devdata = {
	.machine = &snd_soc_machine_exm97,
	.platform = &sh7760_soc_platform,
	.codec_dev = &soc_codec_dev_ac97,
};

static struct platform_device *exm7760_ac97_snd_device;

static int exm7760_ac97_init(void)
{
	int ret = -ENOMEM;
	unsigned short ipsel;

	MSG("module_init() enter\n");

	printk(KERN_INFO "MSC EXM32 Motherboard AC97 sound support\n");

	/* enable both AC97 controllers in pinmux reg */
	ipsel = ctrl_inw(IPSEL);
	ipsel |= (3<<10);
	ctrl_outw(ipsel, IPSEL);

	exm7760_ac97_snd_device = platform_device_alloc("soc-audio", -1);
	if (!exm7760_ac97_snd_device)
		goto out;

	platform_set_drvdata(exm7760_ac97_snd_device,
			     &exm7760_ac97_snd_devdata);
	exm7760_ac97_snd_devdata.dev = &exm7760_ac97_snd_device->dev;
	ret = platform_device_add(exm7760_ac97_snd_device);

	if (ret)
		platform_device_put(exm7760_ac97_snd_device);

out:
	MSG("module_init() exit (ret %d)\n", ret);
	return ret;
}

static void exm7760_ac97_exit(void)
{
	MSG("exm7760_ac97_exit enter\n");
	platform_device_del(exm7760_ac97_snd_device);
	MSG("exm7760_ac97_exit leave\n");
}

module_init(exm7760_ac97_init);
module_exit(exm7760_ac97_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MSC EXM32 Motherboard AC97 sound support");
MODULE_AUTHOR("Manuel Lauss <mlau@msc-ge.com>");
