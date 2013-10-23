/* linux/arch/arm/mach-exynos4/dev-audio.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd
 *	Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/audio.h>
#include <plat/cpu.h>
#include <plat/srp.h>

#include <mach/map.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <mach/regs-audss.h>

static const char *rclksrc[] = {
	[0] = "busclk",
	[1] = "i2sclk",
};

struct exynos_gpio_cfg {
	unsigned int	addr;
	unsigned int	num;
	unsigned int	bit;
};

static int exynos_cfg_i2s_gpio(struct platform_device *pdev)
{
	/* configure GPIO for i2s port */
	struct exynos_gpio_cfg exynos4_cfg[3] = {
				{ EXYNOS4_GPZ(0),  7, S3C_GPIO_SFN(2) },
				{ EXYNOS4_GPC0(0), 5, S3C_GPIO_SFN(2) },
				{ EXYNOS4_GPC1(0), 5, S3C_GPIO_SFN(2) }
	};
	struct exynos_gpio_cfg exynos5_cfg[3] = {
				{ EXYNOS5_GPZ(0),  7, S3C_GPIO_SFN(2) },
				{ EXYNOS5_GPB0(0), 5, S3C_GPIO_SFN(2) },
				{ EXYNOS5_GPB1(0), 5, S3C_GPIO_SFN(2) }
	};
	struct exynos_gpio_cfg exynos5410_cfg[3] = {
				{ EXYNOS5410_GPZ(0),  7, S3C_GPIO_SFN(2) },
				{ EXYNOS5410_GPB0(0), 5, S3C_GPIO_SFN(2) },
				{ EXYNOS5410_GPB1(0), 5, S3C_GPIO_SFN(2) }
	};
	struct exynos_gpio_cfg exynos5420_cfg[3] = {
				{ EXYNOS5420_GPZ(0),  7, S3C_GPIO_SFN(2) },
				{ EXYNOS5420_GPB0(0), 5, S3C_GPIO_SFN(2) },
				{ EXYNOS5420_GPB1(0), 5, S3C_GPIO_SFN(2) }
	};

	if (pdev->id < 0 || pdev->id > 2) {
		printk(KERN_ERR "Invalid Device %d\n", pdev->id);
		return -EINVAL;
	}

	if (soc_is_exynos5250())
		s3c_gpio_cfgpin_range(exynos5_cfg[pdev->id].addr,
			exynos5_cfg[pdev->id].num, exynos5_cfg[pdev->id].bit);
	else if (soc_is_exynos5410())
		s3c_gpio_cfgpin_range(exynos5410_cfg[pdev->id].addr,
			exynos5410_cfg[pdev->id].num, exynos5410_cfg[pdev->id].bit);
	else if (soc_is_exynos5420())
		s3c_gpio_cfgpin_range(exynos5420_cfg[pdev->id].addr,
			exynos5420_cfg[pdev->id].num, exynos5420_cfg[pdev->id].bit);
	else if (soc_is_exynos4210() || soc_is_exynos4212() || soc_is_exynos4412())
		s3c_gpio_cfgpin_range(exynos4_cfg[pdev->id].addr,
			exynos4_cfg[pdev->id].num, exynos4_cfg[pdev->id].bit);

	return 0;
}

static struct s3c_audio_pdata i2sv5_pdata = {
	.cfg_gpio = exynos_cfg_i2s_gpio,
	.type = {
		.i2s = {
			.quirks = QUIRK_PRI_6CHAN | QUIRK_SEC_DAI
					 | QUIRK_NEED_RSTCLR,
			.src_clk = rclksrc,
			.idma_addr = EXYNOS4_AUDSS_INT_MEM,
		},
	},
};

static struct resource exynos4_i2s0_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_I2S0, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_I2S0_TX),
	[2] = DEFINE_RES_DMA(DMACH_I2S0_RX),
	[3] = DEFINE_RES_DMA(DMACH_I2S0S_TX),
};

static struct resource exynos5_i2s0_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_I2S0, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_I2S0_TX),
	[2] = DEFINE_RES_DMA(DMACH_I2S0_RX),
	[3] = DEFINE_RES_DMA(DMACH_I2S0S_TX),
};

struct platform_device exynos4_device_i2s0 = {
	.name = "samsung-i2s",
	.id = 0,
	.num_resources = ARRAY_SIZE(exynos4_i2s0_resource),
	.resource = exynos4_i2s0_resource,
	.dev = {
		.platform_data = &i2sv5_pdata,
	},
};

struct platform_device exynos5_device_i2s0 = {
	.name = "samsung-i2s",
	.id = 0,
	.num_resources = ARRAY_SIZE(exynos5_i2s0_resource),
	.resource = exynos5_i2s0_resource,
	.dev = {
		.platform_data = &i2sv5_pdata,
	},
};

static const char *rclksrc_v3[] = {
	[0] = "sclk_i2s",
	[1] = "no_such_clock",
};

static struct s3c_audio_pdata i2sv3_pdata = {
	.cfg_gpio = exynos_cfg_i2s_gpio,
	.type = {
		.i2s = {
			.quirks = QUIRK_NO_MUXPSR,
			.src_clk = rclksrc_v3,
		},
	},
};

static struct resource exynos4_i2s1_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_I2S1, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_I2S1_TX),
	[2] = DEFINE_RES_DMA(DMACH_I2S1_RX),
};

static struct resource exynos5_i2s1_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_I2S1, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_I2S1_TX),
	[2] = DEFINE_RES_DMA(DMACH_I2S1_RX),
};

struct platform_device exynos4_device_i2s1 = {
	.name = "samsung-i2s",
	.id = 1,
	.num_resources = ARRAY_SIZE(exynos4_i2s1_resource),
	.resource = exynos4_i2s1_resource,
	.dev = {
		.platform_data = &i2sv3_pdata,
	},
};

struct platform_device exynos5_device_i2s1 = {
	.name = "samsung-i2s",
	.id = 1,
	.num_resources = ARRAY_SIZE(exynos5_i2s1_resource),
	.resource = exynos5_i2s1_resource,
	.dev = {
		.platform_data = &i2sv3_pdata,
	},
};

static struct resource exynos4_i2s2_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_I2S2, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_I2S2_TX),
	[2] = DEFINE_RES_DMA(DMACH_I2S2_RX),
};

static struct resource exynos5_i2s2_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_I2S2, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_I2S2_TX),
	[2] = DEFINE_RES_DMA(DMACH_I2S2_RX),
};

struct platform_device exynos4_device_i2s2 = {
	.name = "samsung-i2s",
	.id = 2,
	.num_resources = ARRAY_SIZE(exynos4_i2s2_resource),
	.resource = exynos4_i2s2_resource,
	.dev = {
		.platform_data = &i2sv3_pdata,
	},
};

struct platform_device exynos5_device_i2s2 = {
	.name = "samsung-i2s",
	.id = 2,
	.num_resources = ARRAY_SIZE(exynos5_i2s2_resource),
	.resource = exynos5_i2s2_resource,
	.dev = {
		.platform_data = &i2sv3_pdata,
	},
};

/* PCM Controller platform_devices */

static int exynos_pcm_cfg_gpio(struct platform_device *pdev)
{
	/* configure GPIO for pcm port */
	struct exynos_gpio_cfg exynos4_cfg[3] = {
				{ EXYNOS4_GPZ(0),  5, S3C_GPIO_SFN(3) },
				{ EXYNOS4_GPC0(0), 5, S3C_GPIO_SFN(3) },
				{ EXYNOS4_GPC1(0), 5, S3C_GPIO_SFN(3) }
	};
	struct exynos_gpio_cfg exynos5_cfg[3] = {
				{ EXYNOS5_GPZ(0),  5, S3C_GPIO_SFN(3) },
				{ EXYNOS5_GPB0(0), 5, S3C_GPIO_SFN(3) },
				{ EXYNOS5_GPB1(0), 5, S3C_GPIO_SFN(3) }
	};
	struct exynos_gpio_cfg exynos5410_cfg[3] = {
				{ EXYNOS5410_GPZ(0),  5, S3C_GPIO_SFN(3) },
				{ EXYNOS5410_GPB0(0), 5, S3C_GPIO_SFN(3) },
				{ EXYNOS5410_GPB1(0), 5, S3C_GPIO_SFN(3) }
	};
	struct exynos_gpio_cfg exynos5420_cfg[3] = {
				{ EXYNOS5420_GPZ(0),  5, S3C_GPIO_SFN(3) },
				{ EXYNOS5420_GPB0(0), 5, S3C_GPIO_SFN(3) },
				{ EXYNOS5420_GPB1(0), 5, S3C_GPIO_SFN(3) }
	};

	if (pdev->id < 0 || pdev->id > 2) {
		printk(KERN_ERR "Invalid Device %d\n", pdev->id);
		return -EINVAL;
	}

	if (soc_is_exynos5250())
		s3c_gpio_cfgpin_range(exynos5_cfg[pdev->id].addr,
			exynos5_cfg[pdev->id].num, exynos5_cfg[pdev->id].bit);
	else if (soc_is_exynos5410())
		s3c_gpio_cfgpin_range(exynos5410_cfg[pdev->id].addr,
			exynos5410_cfg[pdev->id].num, exynos5410_cfg[pdev->id].bit);
	else if (soc_is_exynos5420())
		s3c_gpio_cfgpin_range(exynos5420_cfg[pdev->id].addr,
			exynos5420_cfg[pdev->id].num, exynos5420_cfg[pdev->id].bit);
	else if (soc_is_exynos4210() || soc_is_exynos4212() || soc_is_exynos4412())
		s3c_gpio_cfgpin_range(exynos4_cfg[pdev->id].addr,
			exynos4_cfg[pdev->id].num, exynos4_cfg[pdev->id].bit);

	return 0;
}

static struct s3c_audio_pdata s3c_pcm_pdata = {
	.cfg_gpio = exynos_pcm_cfg_gpio,
};

static struct resource exynos4_pcm0_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_PCM0, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_PCM0_TX),
	[2] = DEFINE_RES_DMA(DMACH_PCM0_RX),
};

static struct resource exynos5_pcm0_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_PCM0, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_PCM0_TX),
	[2] = DEFINE_RES_DMA(DMACH_PCM0_RX),
};

struct platform_device exynos4_device_pcm0 = {
	.name = "samsung-pcm",
	.id = 0,
	.num_resources = ARRAY_SIZE(exynos4_pcm0_resource),
	.resource = exynos4_pcm0_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};

struct platform_device exynos5_device_pcm0 = {
	.name = "samsung-pcm",
	.id = 0,
	.num_resources = ARRAY_SIZE(exynos5_pcm0_resource),
	.resource = exynos5_pcm0_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};

static struct resource exynos4_pcm1_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_PCM1, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_PCM1_TX),
	[2] = DEFINE_RES_DMA(DMACH_PCM1_RX),
};

static struct resource exynos5_pcm1_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_PCM1, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_PCM1_TX),
	[2] = DEFINE_RES_DMA(DMACH_PCM1_RX),
};

struct platform_device exynos4_device_pcm1 = {
	.name = "samsung-pcm",
	.id = 1,
	.num_resources = ARRAY_SIZE(exynos4_pcm1_resource),
	.resource = exynos4_pcm1_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};

struct platform_device exynos5_device_pcm1 = {
	.name = "samsung-pcm",
	.id = 1,
	.num_resources = ARRAY_SIZE(exynos5_pcm1_resource),
	.resource = exynos5_pcm1_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};

static struct resource exynos4_pcm2_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_PCM2, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_PCM2_TX),
	[2] = DEFINE_RES_DMA(DMACH_PCM2_RX),
};

static struct resource exynos5_pcm2_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_PCM2, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_PCM2_TX),
	[2] = DEFINE_RES_DMA(DMACH_PCM2_RX),
};

struct platform_device exynos4_device_pcm2 = {
	.name = "samsung-pcm",
	.id = 2,
	.num_resources = ARRAY_SIZE(exynos4_pcm2_resource),
	.resource = exynos4_pcm2_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};

struct platform_device exynos5_device_pcm2 = {
	.name = "samsung-pcm",
	.id = 2,
	.num_resources = ARRAY_SIZE(exynos5_pcm2_resource),
	.resource = exynos5_pcm2_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};

/* AC97 Controller platform devices */

static int exynos_ac97_cfg_gpio(struct platform_device *pdev)
{
	/* configure GPIO for ac97 port */
	if (soc_is_exynos5250())
		s3c_gpio_cfgpin_range(EXYNOS5_GPB0(0), 5, S3C_GPIO_SFN(4));
	else if (soc_is_exynos4210() || soc_is_exynos4212() || soc_is_exynos4412())
		s3c_gpio_cfgpin_range(EXYNOS4_GPC0(0), 5, S3C_GPIO_SFN(4));

	return 0;
}

static struct resource exynos4_ac97_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_AC97, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_AC97_PCMOUT),
	[2] = DEFINE_RES_DMA(DMACH_AC97_PCMIN),
	[3] = DEFINE_RES_DMA(DMACH_AC97_MICIN),
	[4] = DEFINE_RES_IRQ(EXYNOS4_IRQ_AC97),
};

static struct s3c_audio_pdata s3c_ac97_pdata = {
	.cfg_gpio = exynos_ac97_cfg_gpio,
};

static u64 exynos_ac97_dmamask = DMA_BIT_MASK(32);

struct platform_device exynos4_device_ac97 = {
	.name = "samsung-ac97",
	.id = -1,
	.num_resources = ARRAY_SIZE(exynos4_ac97_resource),
	.resource = exynos4_ac97_resource,
	.dev = {
		.platform_data = &s3c_ac97_pdata,
		.dma_mask = &exynos_ac97_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

/* S/PDIF Controller platform_device */

static int exynos_spdif_cfg_gpio(struct platform_device *pdev)
{
	/* configure GPIO for SPDIF port */
	if (soc_is_exynos5250())
		s3c_gpio_cfgpin_range(EXYNOS5_GPB1(0), 2, S3C_GPIO_SFN(4));
	else if (soc_is_exynos5410())
		s3c_gpio_cfgpin_range(EXYNOS5410_GPB1(0), 2, S3C_GPIO_SFN(4));
	else if (soc_is_exynos5420())
		s3c_gpio_cfgpin_range(EXYNOS5420_GPB1(0), 2, S3C_GPIO_SFN(4));
	else if (soc_is_exynos4210() || soc_is_exynos4212() || soc_is_exynos4412())
		s3c_gpio_cfgpin_range(EXYNOS4_GPC1(0), 2, S3C_GPIO_SFN(4));

	return 0;
}

static struct resource exynos4_spdif_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_SPDIF, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_SPDIF),
};

static struct resource exynos5_spdif_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_SPDIF, 0x100),
	[1] = DEFINE_RES_DMA(DMACH_SPDIF),
};

static struct s3c_audio_pdata samsung_spdif_pdata = {
	.cfg_gpio = exynos_spdif_cfg_gpio,
};

static u64 exynos_spdif_dmamask = DMA_BIT_MASK(32);

struct platform_device exynos4_device_spdif = {
	.name = "samsung-spdif",
	.id = -1,
	.num_resources = ARRAY_SIZE(exynos4_spdif_resource),
	.resource = exynos4_spdif_resource,
	.dev = {
		.platform_data = &samsung_spdif_pdata,
		.dma_mask = &exynos_spdif_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

struct platform_device exynos5_device_spdif = {
	.name = "samsung-spdif",
	.id = -1,
	.num_resources = ARRAY_SIZE(exynos5_spdif_resource),
	.resource = exynos5_spdif_resource,
	.dev = {
		.platform_data = &samsung_spdif_pdata,
		.dma_mask = &exynos_spdif_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct resource exynos4_srp_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS_PA_AUDSS_INTMEM, 0x39000),
	[1] = DEFINE_RES_MEM(EXYNOS_PA_AUDSS_COMMBOX, 0x200),
};

static struct resource exynos5_srp_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS_PA_AUDSS_INTMEM, 0x49000),
	[1] = DEFINE_RES_MEM(EXYNOS_PA_AUDSS_COMMBOX, 0x200),
};

static u64 exynos_srp_dmamask = DMA_BIT_MASK(32);

static struct exynos_srp_pdata exynos4_pdata = {
	.type = SRP_HW_RESET,
	.use_iram = true,
	.iram_size = 256 * 1024,
	.icache_size = 64 * 1024,
	.dmem_size = 128 * 1024,
	.cmem_size = 36 * 1024,
	.commbox_size = 0x200,
	.ibuf = {
		.base = EXYNOS4_PA_SYSRAM1,
		.size = 16 * 1024,
		.offset = 0x30000,
		.num = 2,
	},
	.obuf = {
		.base = EXYNOS_PA_AUDSS_INTMEM,
		.size = 32 * 1024,
		.offset = 0x4,
		.num = 2,
	},
	.idma = {
		.base = EXYNOS4_PA_SYSRAM1,
		.offset = 0x38000,
	},
};

static struct exynos_srp_pdata exynos5_pdata = {
	.type = SRP_SW_RESET,
	.use_iram = false,
	.icache_size = 96 * 1024,
	.dmem_size = 160 * 1024,
	.cmem_size = 36 * 1024,
	.commbox_size = 0x308,
	.ibuf = {
		.base = EXYNOS_PA_AUDSS_INTMEM,
		.size = 16 * 1024,
		.offset = 0x8104,
		.num = 2,
	},
	.obuf = {
		.base = EXYNOS_PA_AUDSS_INTMEM,
		.size = 16 * 1024,
		.offset = 0x10104,
		.num = 2,
	},
	.idma = {
		.base = EXYNOS_PA_AUDSS_INTMEM,
		.offset = 0x4,
	},
};

struct platform_device exynos4_device_srp = {
	.name = "samsung-rp",
	.id = -1,
	.num_resources = ARRAY_SIZE(exynos4_srp_resource),
	.resource = exynos4_srp_resource,
	.dev = {
		.dma_mask = &exynos_srp_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &exynos4_pdata,
	},
};

struct platform_device exynos5_device_srp = {
	.name = "samsung-rp",
	.id = -1,
	.num_resources = ARRAY_SIZE(exynos5_srp_resource),
	.resource = exynos5_srp_resource,
	.dev = {
		.dma_mask = &exynos_srp_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &exynos5_pdata,
	},
};
