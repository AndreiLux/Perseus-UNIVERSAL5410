/* linux/arch/arm/mach-xxxx/board-m0ctc-modems.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>

/* inlcude platform specific file */
//#include <linux/platform_data/modem_if.h>
#include <linux/platform_data/modem.h>

#include <mach/sec_modem.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-mem.h>
#include <plat/regs-srom.h>

#ifdef CONFIG_USBHUB_USB3503
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_data/usb3503.h>
#endif
#include <plat/devs.h>
#include <plat/ehci.h>

#if defined(CONFIG_LINK_DEVICE_PLD)
#include <linux/spi/spi.h>
#include <mach/pld_pdata.h>
#endif

#define SROM_CS0_BASE		0x04000000
#define SROM_WIDTH		0x01000000

#if defined(CONFIG_LINK_DEVICE_PLD)
#define SROM_NUM_ADDR_BITS	15
#else
#define SROM_NUM_ADDR_BITS	14
#endif

/*
 * For SROMC Configuration:
 * SROMC_ADDR_BYTE enable for byte access
 */
#define SROMC_DATA_16		0x1
#define SROMC_ADDR_BYTE		0x2
#define SROMC_WAIT_EN		0x4
#define SROMC_BYTE_EN		0x8
#define SROMC_MASK		0xF

/* Memory attributes */
enum sromc_attr {
	MEM_DATA_BUS_8BIT = 0x00000000,
	MEM_DATA_BUS_16BIT = 0x00000001,
	MEM_BYTE_ADDRESSABLE = 0x00000002,
	MEM_WAIT_EN = 0x00000004,
	MEM_BYTE_EN = 0x00000008,

};

/* DPRAM configuration */
struct sromc_cfg {
	enum sromc_attr attr;
	unsigned size;
	unsigned csn;		/* CSn #                        */
	unsigned addr;		/* Start address (physical)     */
	unsigned end;		/* End address (physical)       */
};

/* DPRAM access timing configuration */
struct sromc_access_cfg {
	u32 tacs;		/* Address set-up before CSn            */
	u32 tcos;		/* Chip selection set-up before OEn     */
	u32 tacc;		/* Access cycle                         */
	u32 tcoh;		/* Chip selection hold on OEn           */
	u32 tcah;		/* Address holding time after CSn       */
	u32 tacp;		/* Page mode access cycle at Page mode  */
	u32 pmc;		/* Page Mode config                     */
};

/* For MDM6600 EDPRAM (External DPRAM) */
#if defined(CONFIG_LINK_DEVICE_PLD)
#define MSM_EDPRAM_SIZE		0x10000	/* 8 KB */
#else
#define MSM_EDPRAM_SIZE		0x4000	/* 16 KB */
#endif

#define INT_MASK_REQ_ACK_F	0x0020
#define INT_MASK_REQ_ACK_R	0x0010
#define INT_MASK_RES_ACK_F	0x0008
#define INT_MASK_RES_ACK_R	0x0004
#define INT_MASK_SEND_F		0x0002
#define INT_MASK_SEND_R		0x0001

#define INT_MASK_REQ_ACK_RFS	0x0400	/* Request RES_ACK_RFS           */
#define INT_MASK_RES_ACK_RFS	0x0200	/* Response of REQ_ACK_RFS       */
#define INT_MASK_SEND_RFS	0x0100	/* Indicate sending RFS data     */

extern int system_rev;

/* Function prototypes */
static void config_dpram_port_gpio(void);
static void init_sromc(void);
static void setup_sromc(unsigned csn, struct sromc_cfg *cfg,
			struct sromc_access_cfg *acc_cfg);
static int __init init_modem(void);

#ifdef CONFIG_USBHUB_USB3503
static int host_port_enable(int port, int enable);
#endif

#if defined(CONFIG_CDMA_MODEM_MDM6600)
#if defined(CONFIG_LINK_DEVICE_PLD)
static struct sromc_cfg msm_edpram_cfg = {
	.attr = (MEM_DATA_BUS_8BIT),
	.size = 0x10000,
};
#else
static struct sromc_cfg msm_edpram_cfg = {
	.attr = (MEM_DATA_BUS_16BIT | MEM_WAIT_EN | MEM_BYTE_EN),
	.size = MSM_EDPRAM_SIZE,
};
#endif

#if defined(CONFIG_LINK_DEVICE_PLD)
static struct sromc_access_cfg msm_edpram_access_cfg[] = {
	[DPRAM_SPEED_LOW] = {
		.tacs = 0x2 << 28,
		.tcos = 0x2 << 24,
		.tacc = 0x5 << 16,
		.tcoh = 0x2 << 12,
		.tcah = 0x2 << 8,
		.tacp = 0x5 << 4,
		.pmc = 0x0 << 0,
	},
};
#else
static struct sromc_access_cfg msm_edpram_access_cfg[] = {
	[DPRAM_SPEED_LOW] = {
		.tacs = 0x2 << 28,
		.tcos = 0x2 << 24,
		.tacc = 0x3 << 16,
		.tcoh = 0x2 << 12,
		.tcah = 0x2 << 8,
		.tacp = 0x2 << 4,
		.pmc = 0x0 << 0,
	},
};
#endif
#endif

#if (MSM_EDPRAM_SIZE == 0x4000)
/*
	magic_code +
	access_enable +
	fmt_tx_head + fmt_tx_tail + fmt_tx_buff +
	raw_tx_head + raw_tx_tail + raw_tx_buff +
	fmt_rx_head + fmt_rx_tail + fmt_rx_buff +
	raw_rx_head + raw_rx_tail + raw_rx_buff +
	padding +
	mbx_cp2ap +
	mbx_ap2cp
 =	2 +
	2 +
	2 + 2 + 2044 +
	2 + 2 + 6128 +
	2 + 2 + 2044 +
	2 + 2 + 6128 +
	16 +
	2 +
	2
 =	16384
*/

#define MSM_DP_FMT_TX_BUFF_SZ	2044
#define MSM_DP_RAW_TX_BUFF_SZ	6128
#define MSM_DP_FMT_RX_BUFF_SZ	2044
#define MSM_DP_RAW_RX_BUFF_SZ	6128

#define MAX_MSM_EDPRAM_IPC_DEV	2	/* FMT, RAW */

struct msm_edpram_ipc_cfg {
	u16 magic;
	u16 access;

	u16 fmt_tx_head;
	u16 fmt_tx_tail;
	u8 fmt_tx_buff[MSM_DP_FMT_TX_BUFF_SZ];

	u16 raw_tx_head;
	u16 raw_tx_tail;
	u8 raw_tx_buff[MSM_DP_RAW_TX_BUFF_SZ];

	u16 fmt_rx_head;
	u16 fmt_rx_tail;
	u8 fmt_rx_buff[MSM_DP_FMT_RX_BUFF_SZ];

	u16 raw_rx_head;
	u16 raw_rx_tail;
	u8 raw_rx_buff[MSM_DP_RAW_RX_BUFF_SZ];

	u8 padding[16];
	u16 mbx_ap2cp;
	u16 mbx_cp2ap;
};


/*
------------------
Buffer : 15KByte
------------------
Reserved: 1014Byte
------------------
SIZE: 2Byte
------------------
TAG: 2Byte
------------------
COUNT: 2Byte
------------------
AP -> CP Intr : 2Byte
------------------
CP -> AP Intr : 2Byte
------------------
*/
#define DP_BOOT_CLEAR_OFFSET	4
#define DP_BOOT_RSRVD_OFFSET	0x3C00
#define DP_BOOT_SIZE_OFFSET	0x3FF6
#define DP_BOOT_TAG_OFFSET	0x3FF8
#define DP_BOOT_COUNT_OFFSET	0x3FFA

#define DP_BOOT_FRAME_SIZE_LIMIT     0x3C00	/* 15KB = 15360byte = 0x3C00 */

#elif (MSM_EDPRAM_SIZE == 0x10000)

/*
	mbx_ap2cp +			0x0
	magic_code +
	access_enable +
	padding +
	mbx_cp2ap +			0x1000
	magic_code +
	access_enable +
	padding +
	fmt_tx_head + fmt_tx_tail + fmt_tx_buff +		0x2000
	raw_tx_head + raw_tx_tail + raw_tx_buff +
	fmt_rx_head + fmt_rx_tail + fmt_rx_buff +		0x3000
	raw_rx_head + raw_rx_tail + raw_rx_buff +
  =	2 +
	4094 +
	2 +
	4094 +
	2 +
	2 +
	2 + 2 + 1020 +
	2 + 2 + 3064 +
	2 + 2 + 1020 +
	2 + 2 + 3064
 */

#define MSM_DP_FMT_TX_BUFF_SZ	1024
#define MSM_DP_RAW_TX_BUFF_SZ	3072
#define MSM_DP_FMT_RX_BUFF_SZ	1024
#define MSM_DP_RAW_RX_BUFF_SZ	3072

#define MAX_MSM_EDPRAM_IPC_DEV	2	/* FMT, RAW */

struct msm_edpram_ipc_cfg {
	u16 mbx_ap2cp;
	u16 magic_ap2cp;
	u16 access_ap2cp;
	u16 fmt_tx_head;
	u16 raw_tx_head;
	u16 fmt_rx_tail;
	u16 raw_rx_tail;
	u16 temp1;
	u8 padding1[4080];

	u16 mbx_cp2ap;
	u16 magic_cp2ap;
	u16 access_cp2ap;
	u16 fmt_tx_tail;
	u16 raw_tx_tail;
	u16 fmt_rx_head;
	u16 raw_rx_head;
	u16 temp2;
	u8 padding2[4080];

	u8 fmt_tx_buff[MSM_DP_FMT_TX_BUFF_SZ];
	u8 raw_tx_buff[MSM_DP_RAW_TX_BUFF_SZ];
	u8 fmt_rx_buff[MSM_DP_FMT_RX_BUFF_SZ];
	u8 raw_rx_buff[MSM_DP_RAW_RX_BUFF_SZ];

	u8 padding3[16384];

	u16 address_buffer;
};

#define DP_BOOT_CLEAR_OFFSET    0
#define DP_BOOT_RSRVD_OFFSET    0
#define DP_BOOT_SIZE_OFFSET     0x2
#define DP_BOOT_TAG_OFFSET      0x4
#define DP_BOOT_COUNT_OFFSET    0x6

#define DP_BOOT_FRAME_SIZE_LIMIT     0x1000	/* 15KB = 15360byte = 0x3C00 */

#else
/*
------------------
Buffer : 31KByte
------------------
Reserved: 1014Byte
------------------
SIZE: 2Byte
------------------
TAG: 2Byte
------------------
COUNT: 2Byte
------------------
AP -> CP Intr : 2Byte
------------------
CP -> AP Intr : 2Byte
------------------
*/
#define DP_BOOT_CLEAR_OFFSET	4
#define DP_BOOT_RSRVD_OFFSET	0x7C00
#define DP_BOOT_SIZE_OFFSET	0x7FF6
#define DP_BOOT_TAG_OFFSET	0x7FF8
#define DP_BOOT_COUNT_OFFSET	0x7FFA

#define DP_BOOT_FRAME_SIZE_LIMIT     0x7C00	/* 31KB = 31744byte = 0x7C00 */
#endif

#if defined(CONFIG_CDMA_MODEM_MDM6600)
static struct dpram_ipc_map msm_ipc_map;

static struct modemlink_dpram_data msm_edpram_ctrl = {
	.type = EXT_DPRAM,

	.dpram_irq = MSM_DPRAM_INT_IRQ,
	.dpram_irq_flags = IRQF_TRIGGER_FALLING,

	.max_ipc_dev = IPC_RFS,
	.ipc_map = &msm_ipc_map,

	.boot_size_offset = DP_BOOT_SIZE_OFFSET,
	.boot_tag_offset = DP_BOOT_TAG_OFFSET,
	.boot_count_offset = DP_BOOT_COUNT_OFFSET,
	.max_boot_frame_size = DP_BOOT_FRAME_SIZE_LIMIT,

#if defined(CONFIG_LINK_DEVICE_PLD)
	.aligned = 1,
#endif
};

/*
** CDMA target platform data
*/
#if defined(CONFIG_LINK_DEVICE_PLD)
static struct modem_io_t cdma_io_devices[] = {
	[0] = {
		.name = "cdma_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD)
	},
	[1] = {
		.name = "cdma_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[2] = {
		.name = "umts_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[3] = {
		.name = "cdma_ipc0",
		.id = 0x00,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[4] = {
		.name = "multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[5] = {
		.name = "cdma_CSD",
		.id = (1 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[6] = {
		.name = "cdma_FOTA",
		.id = (2 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[7] = {
		.name = "cdma_GPS",
		.id = (5 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[8] = {
		.name = "cdma_XTRA",
		.id = (6 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[9] = {
		.name = "cdma_CDMA",
		.id = (7 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[10] = {
		.name = "cdma_EFS",
		.id = (8 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[11] = {
		.name = "cdma_TRFB",
		.id = (9 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[12] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[13] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[14] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[15] = {
		.name = "rmnet3",
		.id = 0x2D,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[16] = {
		.name = "cdma_SMD",
		.id = (25 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[17] = {
		.name = "cdma_VTVD",
		.id = (26 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[18] = {
		.name = "cdma_VTAD",
		.id = (27 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[19] = {
		.name = "cdma_VTCTRL",
		.id = (28 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[20] = {
		.name = "cdma_VTENT",
		.id = (29 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
};

#else
static struct modem_io_t cdma_io_devices[] = {
	[0] = {
		.name = "cdma_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[1] = {
		.name = "cdma_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[2] = {
		.name = "umts_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[3] = {
		.name = "cdma_ipc0",
		.id = 0x00,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[4] = {
		.name = "multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[5] = {
		.name = "cdma_CSD",
		.id = (1 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[6] = {
		.name = "cdma_FOTA",
		.id = (2 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[7] = {
		.name = "cdma_GPS",
		.id = (5 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[8] = {
		.name = "cdma_XTRA",
		.id = (6 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[9] = {
		.name = "cdma_CDMA",
		.id = (7 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[10] = {
		.name = "cdma_EFS",
		.id = (8 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[11] = {
		.name = "cdma_TRFB",
		.id = (9 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[12] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[13] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[14] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[15] = {
		.name = "rmnet3",
		.id = 0x2D,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[16] = {
		.name = "cdma_SMD",
		.id = (25 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[17] = {
		.name = "cdma_VTVD",
		.id = (26 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[18] = {
		.name = "cdma_VTAD",
		.id = (27 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[19] = {
		.name = "cdma_VTCTRL",
		.id = (28 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[20] = {
		.name = "cdma_VTENT",
		.id = (29 | 0x20),
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
};
#endif

static struct modem_data cdma_modem_data = {
	.name = "mdm6600",

	.gpio_cp_on = GPIO_CP_MSM_PWRON,
	.gpio_cp_off = GPIO_MSM_OFF,
	.gpio_reset_req_n = GPIO_CP_MSM_PMU_RST,
	.gpio_cp_reset = GPIO_CP_MSM_RST,
	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_phone_active = GPIO_MSM_PHONE_ACTIVE,
	.gpio_flm_uart_sel = GPIO_BOOT_SW_SEL,

	.gpio_ipc_int2ap = GPIO_MSM_DPRAM_INT,

	.gpio_cp_dump_int = GPIO_CP_DUMP_INT,
	.gpio_cp_warm_reset = 0,

	.use_handover = false,

#if defined(CONFIG_SIM_DETECT)
	.gpio_sim_detect = GPIO_CP_SIM_DETECT,
#else
	.gpio_sim_detect = 0,
#endif

#if defined(CONFIG_LINK_DEVICE_PLD)
	.gpio_fpga2_creset = GPIO_FPGA2_CRESET,
	.gpio_fpga2_cdone = GPIO_FPGA2_CDONE,
	.gpio_fpga2_rst_n = GPIO_FPGA2_RST_N,
	.gpio_fpga2_cs_n = GPIO_FPGA2_CS_N,
#endif
	.gpio_usb_boot_en = GPIO_USB_BOOT_EN,
	.gpio_boot_sw_sel = GPIO_BOOT_SW_SEL,

	.modem_net = CDMA_NETWORK,
	.modem_type = QC_MDM6600,
#if defined(CONFIG_LINK_DEVICE_PLD)
	.link_types = LINKTYPE(LINKDEV_PLD),
#else
	.link_types = LINKTYPE(LINKDEV_DPRAM),
#endif
	.link_name = "mdm6600_edpram",
	.dpram = &msm_edpram_ctrl,

	.ipc_version = SIPC_VER_42,
	.max_ipc_dev = IPC_RFS,

	.num_iodevs = ARRAY_SIZE(cdma_io_devices),
	.iodevs = cdma_io_devices,
};

static struct resource cdma_modem_res[] = {
	[0] = {
		.name = "cp_active_irq",
		.start = MSM_PHONE_ACTIVE_IRQ,
		.end = MSM_PHONE_ACTIVE_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	[1] = {
		.name = "dpram_irq",
		.start = MSM_DPRAM_INT_IRQ,
		.end = MSM_DPRAM_INT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
#if defined(CONFIG_SIM_DETECT)
	[2] = {
		.name = "sim_irq",
		.start = CP_SIM_DETECT_IRQ,
		.end = CP_SIM_DETECT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
#endif
};

static struct platform_device cdma_modem = {
	.name = "modem_if",
	.id = 2,
	.num_resources = ARRAY_SIZE(cdma_modem_res),
	.resource = cdma_modem_res,
	.dev = {
		.platform_data = &cdma_modem_data,
	},
};

static void config_cdma_modem_gpio(void)
{
	int err;
	unsigned gpio_cp_on = cdma_modem_data.gpio_cp_on;
	unsigned gpio_cp_off = cdma_modem_data.gpio_cp_off;
	unsigned gpio_rst_req_n = cdma_modem_data.gpio_reset_req_n;
	unsigned gpio_cp_rst = cdma_modem_data.gpio_cp_reset;
	unsigned gpio_pda_active = cdma_modem_data.gpio_pda_active;
	unsigned gpio_phone_active = cdma_modem_data.gpio_phone_active;
	unsigned gpio_flm_uart_sel = cdma_modem_data.gpio_flm_uart_sel;
	unsigned gpio_ipc_int2ap = cdma_modem_data.gpio_ipc_int2ap;
	unsigned gpio_cp_dump_int = cdma_modem_data.gpio_cp_dump_int;
	unsigned gpio_sim_detect = cdma_modem_data.gpio_sim_detect;
	unsigned gpio_boot_sw_sel = cdma_modem_data.gpio_boot_sw_sel;

#if defined(CONFIG_LINK_DEVICE_PLD)
	unsigned gpio_fpga2_creset = cdma_modem_data.gpio_fpga2_creset;
	unsigned gpio_fpga2_cdone = cdma_modem_data.gpio_fpga2_cdone;
	unsigned gpio_fpga2_rst_n = cdma_modem_data.gpio_fpga2_rst_n;
	unsigned gpio_fpga2_cs_n = cdma_modem_data.gpio_fpga2_cs_n;
#endif
	pr_info("[MODEMS] <%s>\n", __func__);

	if (gpio_pda_active) {
		err = gpio_request(gpio_pda_active, "PDA_ACTIVE");
		if (err) {
			pr_err("fail to request gpio %s\n", "PDA_ACTIVE");
		} else {
			gpio_direction_output(gpio_pda_active, 0);
			s3c_gpio_setpull(gpio_pda_active, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio_pda_active, 0);
		}
	}

	if (gpio_phone_active) {
		err = gpio_request(gpio_phone_active, "MSM_ACTIVE");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_ACTIVE");
		} else {
			s3c_gpio_cfgpin(gpio_phone_active, S3C_GPIO_SFN(0xF));
			s3c_gpio_setpull(gpio_phone_active, S3C_GPIO_PULL_NONE);
			irq_set_irq_type(gpio_phone_active, IRQ_TYPE_EDGE_BOTH);
		}
	}
	if (gpio_cp_on) {
		err = gpio_request(gpio_cp_on, "MSM_ON");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_ON");
		} else {
			gpio_direction_output(gpio_cp_on, 0);
			s3c_gpio_setpull(gpio_cp_on, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio_cp_on, 0);
		}
	}

	if (gpio_cp_off) {
		err = gpio_request(gpio_cp_off, "MSM_OFF");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_OFF");
		} else {
			gpio_direction_input(gpio_cp_off);
			s3c_gpio_setpull(gpio_cp_off, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio_cp_off, 0);
		}
	}

	if (gpio_rst_req_n) {
		err = gpio_request(gpio_rst_req_n, "MSM_RST_REQ");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_RST_REQ");
		} else {
			gpio_direction_output(gpio_rst_req_n, 0);
			s3c_gpio_setpull(gpio_rst_req_n, S3C_GPIO_PULL_NONE);
		}
		gpio_set_value(gpio_rst_req_n, 0);
	}

	if (gpio_cp_rst) {
		err = gpio_request(gpio_cp_rst, "MSM_RST");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_RST");
		} else {
			gpio_direction_output(gpio_cp_rst, 0);
			s3c_gpio_setpull(gpio_cp_rst, S3C_GPIO_PULL_NONE);
		}
		gpio_set_value(gpio_cp_rst, 0);
	}

	if (gpio_ipc_int2ap) {
		err = gpio_request(gpio_ipc_int2ap, "MSM_DPRAM_INT");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_DPRAM_INT");
		} else {
			/* Configure as a wake-up source */
			s3c_gpio_cfgpin(gpio_ipc_int2ap, S3C_GPIO_SFN(0xF));
			s3c_gpio_setpull(gpio_ipc_int2ap, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_cp_dump_int) {
		err = gpio_request(gpio_cp_dump_int, "MSM_CP_DUMP_INT");
		if (err) {
			pr_err("fail to request gpio %s\n", "MSM_CP_DUMP_INT");
		} else {
			s3c_gpio_cfgpin(gpio_cp_dump_int, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio_cp_dump_int, S3C_GPIO_PULL_DOWN);
		}
	}

#if defined(CONFIG_LINK_DEVICE_PLD)
	if (gpio_fpga2_creset) {
		err = gpio_request(gpio_fpga2_creset, "FPGA2_CRESET");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA2_CRESET", gpio_fpga2_creset, err);
		} else {
			gpio_direction_output(gpio_fpga2_creset, 0);
			s3c_gpio_setpull(gpio_fpga2_creset, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga2_cdone) {
		err = gpio_request(gpio_fpga2_cdone, "FPGA2_CDONE");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA2_CDONE", gpio_fpga2_cdone, err);
		} else {
			s3c_gpio_cfgpin(gpio_fpga2_cdone, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio_fpga2_cdone, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga2_rst_n)	{
		err = gpio_request(gpio_fpga2_rst_n, "FPGA2_RESET");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA2_RESET", gpio_fpga2_rst_n, err);
		} else {
			gpio_direction_output(gpio_fpga2_rst_n, 0);
			s3c_gpio_setpull(gpio_fpga2_rst_n, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga2_cs_n)	{
		err = gpio_request(gpio_fpga2_cs_n, "SPI_CS2_1");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"SPI_CS2_1", gpio_fpga2_cs_n, err);
		} else {
			gpio_direction_output(gpio_fpga2_cs_n, 0);
			s3c_gpio_setpull(gpio_fpga2_cs_n, S3C_GPIO_PULL_NONE);
		}
	}
#endif

	if (gpio_sim_detect) {
		err = gpio_request(gpio_sim_detect, "MSM_SIM_DETECT");
		if (err) {
			pr_err("fail to request gpio %s: %d\n",
					"MSM_SIM_DETECT", err);
		} else {
			/* gpio_direction_input(gpio_sim_detect); */
			s3c_gpio_cfgpin(gpio_sim_detect, S3C_GPIO_SFN(0xF));
			s3c_gpio_setpull(gpio_sim_detect, S3C_GPIO_PULL_NONE);
			irq_set_irq_type(gpio_sim_detect, IRQ_TYPE_EDGE_BOTH);
		}
	}

	if (gpio_boot_sw_sel) {
		err = gpio_request(gpio_boot_sw_sel, "BOOT_SW_SEL");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"BOOT_SW_SEL", gpio_boot_sw_sel, err);
		} else {
			gpio_direction_output(gpio_boot_sw_sel, 0);
			s3c_gpio_setpull(gpio_boot_sw_sel, S3C_GPIO_PULL_NONE);
		}
	}
}

static u8 *msm_edpram_remap_mem_region(struct sromc_cfg *cfg)
{
	int dp_addr = 0;
	int dp_size = 0;
	u8 __iomem *dp_base = NULL;
	struct msm_edpram_ipc_cfg *ipc_map = NULL;
	struct dpram_ipc_device *dev = NULL;

	dp_addr = cfg->addr;
	dp_size = cfg->size;
	dp_base = (u8 *) ioremap_nocache(dp_addr, dp_size);
	if (!dp_base) {
		pr_err("[MDM] <%s> dpram base ioremap fail\n", __func__);
		return NULL;
	}
	pr_info("[MDM] <%s> DPRAM VA=0x%08X\n", __func__, (int)dp_base);

	msm_edpram_ctrl.base = (u8 __iomem *) dp_base;
	msm_edpram_ctrl.size = dp_size;

	/* Map for IPC */
	ipc_map = (struct msm_edpram_ipc_cfg *)dp_base;

#if defined(CONFIG_LINK_DEVICE_PLD)
	/* Magic code and access enable fields */
	msm_ipc_map.magic_ap2cp = (u16 __iomem *) &ipc_map->magic_ap2cp;
	msm_ipc_map.access_ap2cp = (u16 __iomem *) &ipc_map->access_ap2cp;

	msm_ipc_map.magic_cp2ap = (u16 __iomem *) &ipc_map->magic_cp2ap;
	msm_ipc_map.access_cp2ap = (u16 __iomem *) &ipc_map->access_cp2ap;

	msm_ipc_map.address_buffer = (u16 __iomem *) &ipc_map->address_buffer;
#else
	/* Magic code and access enable fields */
	msm_ipc_map.magic = (u16 __iomem *) &ipc_map->magic;
	msm_ipc_map.access = (u16 __iomem *) &ipc_map->access;
#endif

	/* FMT */
	dev = &msm_ipc_map.dev[IPC_FMT];

	strcpy(dev->name, "FMT");
	dev->id = IPC_FMT;

	dev->txq.head = (u16 __iomem *) &ipc_map->fmt_tx_head;
	dev->txq.tail = (u16 __iomem *) &ipc_map->fmt_tx_tail;
	dev->txq.buff = (u8 __iomem *) &ipc_map->fmt_tx_buff[0];
	dev->txq.size = MSM_DP_FMT_TX_BUFF_SZ;

	dev->rxq.head = (u16 __iomem *) &ipc_map->fmt_rx_head;
	dev->rxq.tail = (u16 __iomem *) &ipc_map->fmt_rx_tail;
	dev->rxq.buff = (u8 __iomem *) &ipc_map->fmt_rx_buff[0];
	dev->rxq.size = MSM_DP_FMT_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_F;
	dev->mask_res_ack = INT_MASK_RES_ACK_F;
	dev->mask_send = INT_MASK_SEND_F;

	/* RAW */
	dev = &msm_ipc_map.dev[IPC_RAW];

	strcpy(dev->name, "RAW");
	dev->id = IPC_RAW;

	dev->txq.head = (u16 __iomem *) &ipc_map->raw_tx_head;
	dev->txq.tail = (u16 __iomem *) &ipc_map->raw_tx_tail;
	dev->txq.buff = (u8 __iomem *) &ipc_map->raw_tx_buff[0];
	dev->txq.size = MSM_DP_RAW_TX_BUFF_SZ;

	dev->rxq.head = (u16 __iomem *) &ipc_map->raw_rx_head;
	dev->rxq.tail = (u16 __iomem *) &ipc_map->raw_rx_tail;
	dev->rxq.buff = (u8 __iomem *) &ipc_map->raw_rx_buff[0];
	dev->rxq.size = MSM_DP_RAW_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_R;
	dev->mask_res_ack = INT_MASK_RES_ACK_R;
	dev->mask_send = INT_MASK_SEND_R;

	/* Mailboxes */
	msm_ipc_map.mbx_ap2cp = (u16 __iomem *) &ipc_map->mbx_ap2cp;
	msm_ipc_map.mbx_cp2ap = (u16 __iomem *) &ipc_map->mbx_cp2ap;

	return dp_base;
}
#endif

/**
 *	DPRAM GPIO settings
 *
 *	SROM_NUM_ADDR_BITS value indicate the address line number or
 *	the mux/demux dpram type. if you want to set mux mode, define the
 *	SROM_NUM_ADDR_BITS to zero.
 *
 *	for CMC22x
 *	CMC22x has 16KB + a SFR register address.
 *	It used 14 bits (13bits for 16KB word address and 1 bit for SFR
 *	register)
 */
static void config_dpram_port_gpio(void)
{
	int addr_bits = SROM_NUM_ADDR_BITS;

	pr_info("[MDM] <%s> address line = %d bits\n", __func__, addr_bits);

	/*
	 ** Config DPRAM address/data GPIO pins
	 */

	/* Set GPIO for dpram address */
	switch (addr_bits) {
	case 0:
		break;

	case 13 ... 14:
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY3(0),
					EXYNOS5410_GPIO_Y3_NR,
					 S3C_GPIO_SFN(2));
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY4(0),
					 addr_bits - EXYNOS5410_GPIO_Y3_NR,
					 S3C_GPIO_SFN(2));
		pr_info("[MDM] <%s> last data gpio EXYNOS5410_GPY4(0) ~ %d\n",
			__func__, addr_bits - EXYNOS5410_GPIO_Y3_NR);
		break;

	case 15:
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY3(0),
					EXYNOS5410_GPIO_Y3_NR,
					 S3C_GPIO_SFN(2));
		s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY4(0),
					EXYNOS5410_GPIO_Y4_NR,
					 S3C_GPIO_SFN(2));
		pr_info("[MDM] <%s> last data gpio EXYNOS4_GPY4(0) ~ %d\n",
			__func__, EXYNOS5410_GPIO_Y4_NR);
		break;


	default:
		pr_err("[MDM/E] <%s> Invalid addr_bits!!!\n", __func__);
		return;
	}

	/* Set GPIO for dpram data - 16bit */
#if defined(CONFIG_LINK_DEVICE_PLD)
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY5(0), 8, S3C_GPIO_SFN(2));
#elif defined(CONFIG_LINK_DEVICE_DPRAM)
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY5(0), 8, S3C_GPIO_SFN(2));
	s3c_gpio_cfgrange_nopull(EXYNOS5410_GPY6(0), 8, S3C_GPIO_SFN(2));
#endif

	/* Setup SROMC CSn pins */
#if defined(CONFIG_CDMA_MODEM_MDM6600)
	s3c_gpio_cfgpin(GPIO_DPRAM_CSN0, S3C_GPIO_SFN(2));
#endif

#if defined(CONFIG_GSM_MODEM_ESC6270)
	s3c_gpio_cfgpin(GPIO_DPRAM_CSN1, S3C_GPIO_SFN(2));
#endif

	/* Config OEn, WEn */
	s3c_gpio_cfgrange_nopull(GPIO_DPRAM_REN, 2, S3C_GPIO_SFN(2));

#if !defined(CONFIG_LINK_DEVICE_PLD)
	/* Config LBn, UBn */
	s3c_gpio_cfgrange_nopull(GPIO_DPRAM_LBN, 2, S3C_GPIO_SFN(2));
#endif

	/* Config BUSY */
#if !defined(CONFIG_LINK_DEVICE_PLD)
	s3c_gpio_cfgpin(GPIO_DPRAM_BUSY, S3C_GPIO_SFN(2));
#endif

}

static void init_sromc(void)
{
	struct clk *clk = NULL;

	/* SROMC clk enable */
	clk = clk_get(NULL, "sromc");
	if (!clk) {
		pr_err("[MDM/E] <%s> SROMC clock gate fail\n", __func__);
		return;
	}
	clk_enable(clk);
}

static void setup_sromc(unsigned csn, struct sromc_cfg *cfg,
			struct sromc_access_cfg *acc_cfg)
{
	unsigned bw = 0;
	unsigned bc = 0;
	void __iomem *bank_sfr = S5P_SROM_BC0 + (4 * csn);

	pr_err("[MDM] <%s> SROMC settings for CS%d...\n", __func__, csn);

	bw = __raw_readl(S5P_SROM_BW);
	bc = __raw_readl(bank_sfr);
	pr_err("[MDM] <%s> Old SROMC settings = BW(0x%08X), BC%d(0x%08X)\n",
	       __func__, bw, csn, bc);

	/* Set the BW control field for the CSn */
	bw &= ~(SROMC_MASK << (csn * 4));

	if (cfg->attr & MEM_DATA_BUS_16BIT)
		bw |= (SROMC_DATA_16 << (csn * 4));

	if (cfg->attr & MEM_WAIT_EN)
		bw |= (SROMC_WAIT_EN << (csn * 4));

	if (cfg->attr & MEM_BYTE_EN)
		bw |= (SROMC_BYTE_EN << (csn * 4));

	writel(bw, S5P_SROM_BW);

	/* Set SROMC memory access timing for the CSn */
	bc = acc_cfg->tacs | acc_cfg->tcos | acc_cfg->tacc |
	    acc_cfg->tcoh | acc_cfg->tcah | acc_cfg->tacp | acc_cfg->pmc;

	writel(bc, bank_sfr);

	/* Verify SROMC settings */
	bw = __raw_readl(S5P_SROM_BW);
	bc = __raw_readl(bank_sfr);
	pr_err("[MDM] <%s> New SROMC settings = BW(0x%08X), BC%d(0x%08X)\n",
	       __func__, bw, csn, bc);
}

/* For ESC6270 modem */
#if defined(CONFIG_GSM_MODEM_ESC6270)
static struct dpram_ipc_map gsm_ipc_map;

#if defined(CONFIG_LINK_DEVICE_PLD)
static struct sromc_cfg gsm_edpram_cfg = {
	.attr = (MEM_DATA_BUS_8BIT),
	.size = 0x10000,
};
#else
static struct sromc_cfg gsm_edpram_cfg = {
	.attr = (MEM_DATA_BUS_16BIT | MEM_WAIT_EN | MEM_BYTE_EN),
	.size = MSM_EDPRAM_SIZE,
};
#endif

#if defined(CONFIG_LINK_DEVICE_PLD)
static struct sromc_access_cfg gsm_edpram_access_cfg[] = {
	[DPRAM_SPEED_LOW] = {
		.tacs = 0x2 << 28,
		.tcos = 0x2 << 24,
		.tacc = 0x5 << 16,
		.tcoh = 0x2 << 12,
		.tcah = 0x2 << 8,
		.tacp = 0x5 << 4,
		.pmc  = 0x0 << 0,
	},
};
#else
static struct sromc_access_cfg gsm_edpram_access_cfg[] = {
	[DPRAM_SPEED_LOW] = {
		.tacs = 0x2 << 28,
		.tcos = 0x2 << 24,
		.tacc = 0x3 << 16,
		.tcoh = 0x2 << 12,
		.tcah = 0x2 << 8,
		.tacp = 0x2 << 4,
		.pmc  = 0x0 << 0,
	},
};
#endif

static struct modemlink_dpram_control gsm_edpram_ctrl = {
	.dp_type = EXT_DPRAM,

	.dpram_irq = ESC_DPRAM_INT_IRQ,
	.dpram_irq_flags = IRQF_TRIGGER_FALLING,

	.max_ipc_dev = IPC_RFS,
	.ipc_map = &gsm_ipc_map,

	.boot_size_offset = DP_BOOT_SIZE_OFFSET,
	.boot_tag_offset = DP_BOOT_TAG_OFFSET,
	.boot_count_offset = DP_BOOT_COUNT_OFFSET,
	.max_boot_frame_size = DP_BOOT_FRAME_SIZE_LIMIT,

#if defined(CONFIG_LINK_DEVICE_PLD)
	.aligned = 1,
#endif
};

/*
** GSM target platform data
*/
#if defined(CONFIG_LINK_DEVICE_PLD)
static struct modem_io_t gsm_io_devices[] = {
	[0] = {
		.name = "gsm_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[1] = {
		.name = "gsm_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[2] = {
		.name = "gsm_rfs0",
		.id = 0x28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[3] = {
		.name = "gsm_multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[4] = {
		.name = "gsm_rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[5] = {
		.name = "gsm_rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[6] = {
		.name = "gsm_rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[7] = {
		.name = "gsm_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[8] = {
		.name = "gsm_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[9] = {
		.name = "gsm_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
	[10] = {
		.name = "gsm_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_PLD),
	},
};
#else
static struct modem_io_t gsm_io_devices[] = {
	[0] = {
		.name = "gsm_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[1] = {
		.name = "gsm_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[2] = {
		.name = "gsm_rfs0",
		.id = 0x28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[3] = {
		.name = "gsm_multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[4] = {
		.name = "gsm_rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[5] = {
		.name = "gsm_rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[6] = {
		.name = "gsm_rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[7] = {
		.name = "gsm_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[8] = {
		.name = "gsm_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[9] = {
		.name = "gsm_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
	[10] = {
		.name = "gsm_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_DPRAM),
	},
};
#endif

static struct modem_data gsm_modem_data = {
	.name = "esc6270",

	.gpio_cp_on = GPIO_CP2_MSM_PWRON,
	.gpio_cp_off = GPIO_ESC_OFF,
	.gpio_reset_req_n = 0,	/* GPIO_CP_MSM_PMU_RST, */
	.gpio_cp_reset = GPIO_CP2_MSM_RST,
	.gpio_pda_active = 0,
	.gpio_phone_active = GPIO_ESC_PHONE_ACTIVE,
	.gpio_flm_uart_sel = GPIO_BOOT_SW_SEL_CP2,

	.gpio_dpram_int = GPIO_ESC_DPRAM_INT,

	.gpio_cp_dump_int = GPIO_ESC_DUMP_INT,
	.gpio_cp_warm_reset = 0,

#if defined(CONFIG_SIM_DETECT)
	.gpio_sim_detect = GPIO_ESC_SIM_DETECT,
#else
	.gpio_sim_detect = 0,
#endif

#if defined(CONFIG_LINK_DEVICE_PLD)
	.gpio_fpga1_creset = GPIO_FPGA1_CRESET,
	.gpio_fpga1_cdone = GPIO_FPGA1_CDONE,
	.gpio_fpga1_rst_n = GPIO_FPGA1_RST_N,
	.gpio_fpga1_cs_n = GPIO_FPGA1_CS_N,
#endif
	//.gpio_usb_boot_en = 0,
	//.gpio_boot_sw_sel = 0,

	.use_handover = false,

	.modem_net = CDMA_NETWORK,
	.modem_type = QC_ESC6270,
#if defined(CONFIG_LINK_DEVICE_PLD)
	.link_types = LINKTYPE(LINKDEV_PLD),
#else
	.link_types = LINKTYPE(LINKDEV_DPRAM),
#endif
	.link_name = "esc6270_edpram",
	.dpram_ctl = &gsm_edpram_ctrl,

	.ipc_version = SIPC_VER_41,
	//.max_ipc_dev = IPC_RFS,

	.num_iodevs = ARRAY_SIZE(gsm_io_devices),
	.iodevs     = gsm_io_devices,
};

static struct resource gsm_modem_res[] = {
	[0] = {
		.name = "cp_active_irq",
		.start = ESC_PHONE_ACTIVE_IRQ,
		.end = ESC_PHONE_ACTIVE_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	[1] = {
		.name = "dpram_irq",
		.start = ESC_DPRAM_INT_IRQ,
		.end = ESC_DPRAM_INT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
#if defined(CONFIG_SIM_DETECT)
	[2] = {
		.name = "sim_irq",
		.start = ESC_SIM_DETECT_IRQ,
		.end = ESC_SIM_DETECT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
#endif
};

static struct platform_device gsm_modem = {
	.name = "modem_if",
	.id = 2,
	.num_resources = ARRAY_SIZE(gsm_modem_res),
	.resource = gsm_modem_res,
	.dev = {
		.platform_data = &gsm_modem_data,
	},
};

void config_gsm_modem_gpio(void)
{
	int err = 0;
	unsigned gpio_cp_on = gsm_modem_data.gpio_cp_on;
	unsigned gpio_cp_off = gsm_modem_data.gpio_cp_off;
	unsigned gpio_rst_req_n = gsm_modem_data.gpio_reset_req_n;
	unsigned gpio_cp_rst = gsm_modem_data.gpio_cp_reset;
	unsigned gpio_pda_active = gsm_modem_data.gpio_pda_active;
	unsigned gpio_phone_active = gsm_modem_data.gpio_phone_active;
	unsigned gpio_flm_uart_sel = gsm_modem_data.gpio_flm_uart_sel;
	unsigned gpio_ipc_int2ap = gsm_modem_data.gpio_dpram_int;
	unsigned gpio_cp_dump_int = gsm_modem_data.gpio_cp_dump_int;
	unsigned gpio_sim_detect = gsm_modem_data.gpio_sim_detect;

#if defined(CONFIG_LINK_DEVICE_PLD)
	unsigned gpio_fpga1_creset = gsm_modem_data.gpio_fpga1_creset;
	unsigned gpio_fpga1_cdone = gsm_modem_data.gpio_fpga1_cdone;
	unsigned gpio_fpga1_rst_n = gsm_modem_data.gpio_fpga1_rst_n;
	unsigned gpio_fpga1_cs_n = gsm_modem_data.gpio_fpga1_cs_n;
#endif

	pr_err("[MODEMS] <%s>\n", __func__);

	if (gpio_pda_active) {
		err = gpio_request(gpio_pda_active, "PDA_ACTIVE");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"PDA_ACTIVE", gpio_pda_active, err);
		} else {
			gpio_direction_output(gpio_pda_active, 1);
			s3c_gpio_setpull(gpio_pda_active, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio_pda_active, 0);
		}
	}

	if (gpio_phone_active) {
		err = gpio_request(gpio_phone_active, "ESC_ACTIVE");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_ACTIVE", gpio_phone_active, err);
			gpio_free(gpio_phone_active);
			err = gpio_request(gpio_phone_active, "ESC_ACTIVE");
		}

		if (!err) {
			s3c_gpio_cfgpin(gpio_phone_active, S3C_GPIO_SFN(0xF));
			s3c_gpio_setpull(gpio_phone_active, S3C_GPIO_PULL_NONE);
			irq_set_irq_type(gpio_phone_active, IRQ_TYPE_EDGE_BOTH);
			pr_err("Success to request gpio %s, gpio %d, errno %d\n",
					"ESC_ACTIVE", gpio_phone_active, err);
		} else {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_ACTIVE", gpio_phone_active, err);
	}
	}

	if (gpio_flm_uart_sel) {
		err = gpio_request(gpio_flm_uart_sel, "BOOT_SW_SEL2");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"BOOT_SW_SEL2", gpio_flm_uart_sel, err);
		} else {
			gpio_direction_output(gpio_flm_uart_sel, 1);
			s3c_gpio_setpull(gpio_flm_uart_sel, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio_flm_uart_sel, 1);
		}
	}

	if (gpio_cp_on) {
		err = gpio_request(gpio_cp_on, "ESC_ON");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_ON", gpio_cp_on, err);
		} else {
			gpio_direction_output(gpio_cp_on, 0);
			s3c_gpio_setpull(gpio_cp_on, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio_cp_on, S5P_GPIO_DRVSTR_LV1);
			gpio_set_value(gpio_cp_on, 0);
		}
	}

	if (gpio_cp_off) {
		err = gpio_request(gpio_cp_off, "ESC_OFF");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_OFF", (gpio_cp_off), err);
		} else {
			gpio_direction_input(gpio_cp_off);
			s3c_gpio_setpull(gpio_cp_off, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio_cp_off, 0);
		}
	}

	if (gpio_rst_req_n) {
		err = gpio_request(gpio_rst_req_n, "ESC_RST_REQ");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_RST_REQ", gpio_rst_req_n, err);
		} else {
			gpio_direction_output(gpio_rst_req_n, 1);
			s3c_gpio_setpull(gpio_rst_req_n, S3C_GPIO_PULL_NONE);
		}
		gpio_set_value(gpio_rst_req_n, 0);
	}

	if (gpio_cp_rst) {
		err = gpio_request(gpio_cp_rst, "ESC_RST");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_RST", gpio_cp_rst, err);
		} else {
			gpio_direction_output(gpio_cp_rst, 0);
			s3c_gpio_setpull(gpio_cp_rst, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio_cp_rst, S5P_GPIO_DRVSTR_LV4);
		}
		gpio_set_value(gpio_cp_rst, 0);
	}

	if (gpio_ipc_int2ap) {
		err = gpio_request(gpio_ipc_int2ap, "ESC_DPRAM_INT");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"ESC_DPRAM_INT", gpio_ipc_int2ap, err);
		} else {
			/* Configure as a wake-up source */
			s3c_gpio_cfgpin(gpio_ipc_int2ap, S3C_GPIO_SFN(0xF));
			s3c_gpio_setpull(gpio_ipc_int2ap, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_cp_dump_int) {
		err = gpio_request(gpio_cp_dump_int, "ESC_CP_DUMP_INT");
		if (err) {
			pr_err("fail to request gpio %s\n", "ESC_CP_DUMP_INT");
		} else {
			s3c_gpio_cfgpin(gpio_cp_dump_int, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio_cp_dump_int, S3C_GPIO_PULL_DOWN);
		}
	}

	err = gpio_request(GPIO_CP2_UART_RXD, "AP_CP2_UART_RXD");
	if (err) {
		pr_err("fail to request gpio %s, gpio %d, errno %d\n",
				"AP_CP2_UART_RXD", GPIO_CP2_UART_RXD, err);
	} else {
		s3c_gpio_cfgpin(GPIO_CP2_UART_RXD, S3C_GPIO_SFN(0x2));
		s3c_gpio_setpull(GPIO_CP2_UART_RXD, S3C_GPIO_PULL_NONE);
	}

	err = gpio_request(GPIO_CP2_UART_TXD, "AP_CP2_UART_TXD");
	if (err) {
		pr_err("fail to request gpio %s, gpio %d, errno %d\n",
				"AP_CP2_UART_TXD", GPIO_CP2_UART_TXD, err);
	} else {
		s3c_gpio_cfgpin(GPIO_CP2_UART_TXD, S3C_GPIO_SFN(0x2));
		s3c_gpio_setpull(GPIO_CP2_UART_TXD, S3C_GPIO_PULL_NONE);
	}

	if (gpio_sim_detect) {
		err = gpio_request(gpio_sim_detect, "ESC_SIM_DETECT");
		if (err) {
			pr_err("fail to request gpio %s: %d\n",
					"ESC_SIM_DETECT", err);
		} else {
			/* gpio_direction_input(gpio_sim_detect); */
			s3c_gpio_cfgpin(gpio_sim_detect, S3C_GPIO_SFN(0xF));
			s3c_gpio_setpull(gpio_sim_detect, S3C_GPIO_PULL_NONE);
		/* irq_set_irq_type(gpio_sim_detect, IRQ_TYPE_EDGE_BOTH); */
		}
	}

#if defined(CONFIG_LINK_DEVICE_PLD)
	if (gpio_fpga1_creset) {
		err = gpio_request(gpio_fpga1_creset, "FPGA1_CRESET");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA1_CRESET", gpio_fpga1_creset, err);
		} else {
			gpio_direction_output(gpio_fpga1_creset, 0);
			s3c_gpio_setpull(gpio_fpga1_creset, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga1_cdone) {
		err = gpio_request(gpio_fpga1_cdone, "FPGA1_CDONE");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA1_CDONE", gpio_fpga1_cdone, err);
		} else {
			s3c_gpio_cfgpin(gpio_fpga1_cdone, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio_fpga1_cdone, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga1_rst_n)	{
		err = gpio_request(gpio_fpga1_rst_n, "FPGA1_RST");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"FPGA1_RST", gpio_fpga1_rst_n, err);
		} else {
			gpio_direction_output(gpio_fpga1_rst_n, 0);
			s3c_gpio_setpull(gpio_fpga1_rst_n, S3C_GPIO_PULL_NONE);
		}
	}

	if (gpio_fpga1_cs_n)	{
		err = gpio_request(gpio_fpga1_cs_n, "SPI_CS1_1");
		if (err) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
					"SPI_CS2_1", gpio_fpga1_cs_n, err);
		} else {
			gpio_direction_output(gpio_fpga1_cs_n, 0);
			s3c_gpio_setpull(gpio_fpga1_cs_n, S3C_GPIO_PULL_NONE);
		}
	}
#endif

#if 1 // temp
	err = gpio_request(GPIO_PDA_ACTIVE, "PDA_ACTIVE");
	if (err) {
		pr_err("fail to request gpio %s\n", "PDA_ACTIVE");
	} else {
		gpio_direction_output(GPIO_PDA_ACTIVE, 1);
		s3c_gpio_setpull(GPIO_PDA_ACTIVE, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_PDA_ACTIVE, 1);
	}
#endif
	/* For IORA */
	if (system_rev >= 4) {
		s3c_gpio_cfgpin(EXYNOS5410_GPX3(7), S3C_GPIO_INPUT);
		s3c_gpio_setpull(EXYNOS5410_GPX3(7), S3C_GPIO_PULL_DOWN);
	}
}

static u8 *gsm_edpram_remap_mem_region(struct sromc_cfg *cfg)
{
	int dp_addr = 0;
	int dp_size = 0;
	u8 __iomem *dp_base = NULL;
	struct msm_edpram_ipc_cfg *ipc_map = NULL;
	struct dpram_ipc_device *dev = NULL;

	dp_addr = cfg->addr;
	dp_size = cfg->size;
	dp_base = (u8 *)ioremap_nocache(dp_addr, dp_size);
	if (!dp_base) {
		pr_err("[MDM] <%s> dpram base ioremap fail\n", __func__);
		return NULL;
	}
	pr_info("[MDM] <%s> DPRAM VA=0x%08X\n", __func__, (int)dp_base);

	gsm_edpram_ctrl.dp_base = (u8 __iomem *)dp_base;
	gsm_edpram_ctrl.dp_size = dp_size;

	/* Map for IPC */
	ipc_map = (struct msm_edpram_ipc_cfg *)dp_base;

	/* Magic code and access enable fields */
#if defined(CONFIG_LINK_DEVICE_PLD)
	/* Magic code and access enable fields */
	gsm_ipc_map.magic_ap2cp = (u16 __iomem *) &ipc_map->magic_ap2cp;
	gsm_ipc_map.access_ap2cp = (u16 __iomem *) &ipc_map->access_ap2cp;

	gsm_ipc_map.magic_cp2ap = (u16 __iomem *) &ipc_map->magic_cp2ap;
	gsm_ipc_map.access_cp2ap = (u16 __iomem *) &ipc_map->access_cp2ap;

	gsm_ipc_map.address_buffer = (u16 __iomem *) &ipc_map->address_buffer;
#else
	/* Magic code and access enable fields */
	gsm_ipc_map.magic = (u16 __iomem *)&ipc_map->magic;
	gsm_ipc_map.access = (u16 __iomem *)&ipc_map->access;
#endif

	/* FMT */
	dev = &gsm_ipc_map.dev[IPC_FMT];

	strcpy(dev->name, "FMT");
	dev->id = IPC_FMT;

	dev->txq.head = (u16 __iomem *)&ipc_map->fmt_tx_head;
	dev->txq.tail = (u16 __iomem *)&ipc_map->fmt_tx_tail;
	dev->txq.buff = (u8 __iomem *)&ipc_map->fmt_tx_buff[0];
	dev->txq.size = MSM_DP_FMT_TX_BUFF_SZ;

	dev->rxq.head = (u16 __iomem *)&ipc_map->fmt_rx_head;
	dev->rxq.tail = (u16 __iomem *)&ipc_map->fmt_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&ipc_map->fmt_rx_buff[0];
	dev->rxq.size = MSM_DP_FMT_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_F;
	dev->mask_res_ack = INT_MASK_RES_ACK_F;
	dev->mask_send = INT_MASK_SEND_F;

	/* RAW */
	dev = &gsm_ipc_map.dev[IPC_RAW];

	strcpy(dev->name, "RAW");
	dev->id = IPC_RAW;

	dev->txq.head = (u16 __iomem *)&ipc_map->raw_tx_head;
	dev->txq.tail = (u16 __iomem *)&ipc_map->raw_tx_tail;
	dev->txq.buff = (u8 __iomem *)&ipc_map->raw_tx_buff[0];
	dev->txq.size = MSM_DP_RAW_TX_BUFF_SZ;

	dev->rxq.head = (u16 __iomem *)&ipc_map->raw_rx_head;
	dev->rxq.tail = (u16 __iomem *)&ipc_map->raw_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&ipc_map->raw_rx_buff[0];
	dev->rxq.size = MSM_DP_RAW_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_R;
	dev->mask_res_ack = INT_MASK_RES_ACK_R;
	dev->mask_send = INT_MASK_SEND_R;

	/* Mailboxes */
	gsm_ipc_map.mbx_ap2cp = (u16 __iomem *)&ipc_map->mbx_ap2cp;
	gsm_ipc_map.mbx_cp2ap = (u16 __iomem *)&ipc_map->mbx_cp2ap;

	return dp_base;
}
#endif

#if defined(CONFIG_LINK_DEVICE_PLD)
#define PLD_BLOCK_SIZE	0x8000

static struct spi_device *p_spi;

static int pld_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	mif_err("pld spi proble.\n");

	p_spi = spi;
	p_spi->mode = SPI_MODE_0;
	p_spi->bits_per_word = 32;

	ret = spi_setup(p_spi);
	if (ret != 0) {
		mif_err("spi_setup ERROR : %d\n", ret);
		return ret;
	}

	dev_info(&p_spi->dev, "(%d) spi probe Done.\n", __LINE__);

	return ret;
}

static int pld_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver pld_spi_driver = {
	.probe = pld_spi_probe,
	.remove = __devexit_p(pld_spi_remove),
	.driver = {
		.name = "modem_if_spi",
		.bus = &spi_bus_type,
		.owner = ((struct module *)0),	/* THIS_MODULE, */
	},
};

static int config_spi_dev_init(void)
{
	int ret = 0;

	ret = spi_register_driver(&pld_spi_driver);
	if (ret < 0) {
		pr_info("spi_register_driver() fail : %d\n", ret);
		return ret;
	}

	pr_info("[%s] Done\n", __func__);
	return 0;
}

static int pld_send_fpga_bin(void)
{
	int retval = 0;

	unsigned gpio_fpga1_creset = gsm_modem_data.gpio_fpga1_creset;
	unsigned gpio_fpga1_cdone = gsm_modem_data.gpio_fpga1_cdone;
	unsigned gpio_fpga1_rst_n = gsm_modem_data.gpio_fpga1_rst_n;
	unsigned gpio_fpga1_cs_n = gsm_modem_data.gpio_fpga1_cs_n;

#if defined(CONFIG_CDMA_MODEM_MDM6600)
	unsigned gpio_fpga2_creset = cdma_modem_data.gpio_fpga2_creset;
	unsigned gpio_fpga2_cdone = cdma_modem_data.gpio_fpga2_cdone;
	unsigned gpio_fpga2_rst_n = cdma_modem_data.gpio_fpga2_rst_n;
	unsigned gpio_fpga2_cs_n = cdma_modem_data.gpio_fpga2_cs_n;
#endif

	unsigned gpio_fpga_spi_clk = GPIO_CP_FPGA_SPI_CLK;
	unsigned gpio_fpga_spi_mosi = GPIO_CP_FPGA_SPI_MOSI;

	unsigned int i, j;	/* loop variables */
	unsigned int spibit;	/* current FPGA configuration data storage */
	char dummy_data[8] = "abcdefg";
	unsigned int bin_size = sizeof(fpga_bin);

	mif_info("sizeofpld : 0%d ", bin_size);

	if (gpio_fpga1_cs_n)	{
		s3c_gpio_cfgpin(gpio_fpga1_cs_n, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio_fpga1_cs_n, S3C_GPIO_PULL_NONE);
		gpio_direction_output(gpio_fpga1_cs_n, 0);
	}

#if defined(CONFIG_CDMA_MODEM_MDM6600)
	if (gpio_fpga2_cs_n)	{
		s3c_gpio_cfgpin(gpio_fpga2_cs_n, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio_fpga2_cs_n, S3C_GPIO_PULL_NONE);
		gpio_direction_output(gpio_fpga2_cs_n, 0);
	}
#endif

	if (gpio_fpga_spi_clk)	{
		retval = gpio_request(gpio_fpga_spi_clk, "FPGA_SPI_CLK");
		if (retval) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
			"FPGA_SPI_CLK", gpio_fpga_spi_clk, retval);
		}
		s3c_gpio_cfgpin(gpio_fpga_spi_clk, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio_fpga_spi_clk, S3C_GPIO_PULL_NONE);
		gpio_direction_output(gpio_fpga_spi_clk, 0);
	}

	if (gpio_fpga_spi_mosi)	{
		retval = gpio_request(gpio_fpga_spi_mosi, "FPGA_SPI_MOSI");
		if (retval) {
			pr_err("fail to request gpio %s, gpio %d, errno %d\n",
			"FPGA_SPI_MOSI", gpio_fpga_spi_mosi, retval);
		}
		s3c_gpio_cfgpin(gpio_fpga_spi_mosi, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio_fpga_spi_mosi, S3C_GPIO_PULL_NONE);
		gpio_direction_output(gpio_fpga_spi_mosi, 0);
	}

	msleep(20);

	if (gpio_fpga1_creset) {
		gpio_direction_output(gpio_fpga1_creset, 1);
		s3c_gpio_setpull(gpio_fpga1_creset, S3C_GPIO_PULL_NONE);
	}

#if defined(CONFIG_CDMA_MODEM_MDM6600)
	if (gpio_fpga2_creset) {
		gpio_direction_output(gpio_fpga2_creset, 1);
		s3c_gpio_setpull(gpio_fpga2_creset, S3C_GPIO_PULL_NONE);
	}
#endif

	msleep(20);

	i = 0;
	while (i < 1000) {
		gpio_set_value(gpio_fpga_spi_clk, 0);
		i = i + 1;
		gpio_set_value(gpio_fpga_spi_clk, 0);
	}

	msleep(20);

	 /* Index 3:  */
	i = 0;
	while (i < bin_size) {
		j = 0;
		spibit = fpga_bin[i];
		while (j < 8) {
			gpio_set_value(gpio_fpga_spi_clk, 0);
			if (spibit & 0x80)
				gpio_set_value(gpio_fpga_spi_mosi, 1);
			else
				gpio_set_value(gpio_fpga_spi_mosi, 0);
			j = j + 1;
			gpio_set_value(gpio_fpga_spi_clk, 1);
			spibit = spibit << 1;
		}
		i = i + 1;
	}

	/* Index 3-1:	*/
	i = 0;
	while (i < sizeof(dummy_data)) {
		j = 0;
		spibit = dummy_data[i];
		while (j < 8) {
			gpio_set_value(gpio_fpga_spi_clk, 0);
			if (spibit & 0x80)
				gpio_set_value(gpio_fpga_spi_mosi, 1);
			else
				gpio_set_value(gpio_fpga_spi_mosi, 0);
			j = j + 1;
			gpio_set_value(gpio_fpga_spi_clk, 1);
			spibit = spibit << 1;
		}
		i = i + 1;
	}

	/* Index 4
		 Start up FPGA */
	i = 0;
	while (i < 100) {
		gpio_set_value(gpio_fpga_spi_clk, 0);
		i = i + 1;
		gpio_set_value(gpio_fpga_spi_clk, 1);
	}

	gpio_set_value(gpio_fpga_spi_clk, 0);

	msleep(20);

	mif_info("PLD_CDone1[%d] \n",
		gpio_get_value(gpio_fpga1_cdone));
#if defined(CONFIG_CDMA_MODEM_MDM6600)
	mif_info("PLD_CDone2[%d] \n",
		gpio_get_value(gpio_fpga2_cdone));
#endif

	if (gpio_fpga1_rst_n)	{
		gpio_direction_output(gpio_fpga1_rst_n, 1);
		s3c_gpio_setpull(gpio_fpga1_rst_n, S3C_GPIO_PULL_NONE);
	}

#if defined(CONFIG_CDMA_MODEM_MDM6600)
	if (gpio_fpga2_rst_n)	{
		gpio_direction_output(gpio_fpga2_rst_n, 1);
		s3c_gpio_setpull(gpio_fpga2_rst_n, S3C_GPIO_PULL_NONE);
	}
#endif

	return retval;
}
#endif

static int __init init_modem(void)
{
	struct sromc_cfg *cfg = NULL;
	struct sromc_access_cfg *acc_cfg = NULL;

#if defined(CONFIG_CDMA_MODEM_MDM6600)
#if defined(CONFIG_MACH_T0_CHN_CTC)
	msm_edpram_cfg.csn = 1;
#else
	msm_edpram_cfg.csn = 0;
#endif
	msm_edpram_cfg.addr = SROM_CS0_BASE + (SROM_WIDTH * msm_edpram_cfg.csn);
	msm_edpram_cfg.end = msm_edpram_cfg.addr + msm_edpram_cfg.size - 1;
#endif

	config_dpram_port_gpio();

#if defined(CONFIG_CDMA_MODEM_MDM6600)
	config_cdma_modem_gpio();
#endif

#if defined(CONFIG_GSM_MODEM_ESC6270)
	if (system_rev >= 4) {
		pr_err("[MODEMS] <%s> Change AP_CP2_FPGA_INT gpio to %d\n", __func__, GPIO_ESC_DPRAM_INT_REV02);
		gsm_modem_data.gpio_dpram_int = GPIO_ESC_DPRAM_INT_REV02;
		gsm_modem_data.gpio_cp_dump_int = GPIO_ESC_DUMP_INT_REV02;
		gsm_edpram_ctrl.dpram_irq = ESC_DPRAM_INT_IRQ_REV02;
		gsm_modem_res[1].start = ESC_DPRAM_INT_IRQ_REV02;
		gsm_modem_res[1].end = ESC_DPRAM_INT_IRQ_REV02;
#if defined(CONFIG_LINK_DEVICE_PLD)
		gsm_modem_data.gpio_fpga1_cdone = GPIO_FPGA1_CDONE_REV02;
#endif
	}
	config_gsm_modem_gpio();
#endif

#if defined(CONFIG_LINK_DEVICE_PLD)
	/* config_spi_dev_init(); */
	pld_send_fpga_bin();
#endif

	init_sromc();

#if defined(CONFIG_CDMA_MODEM_MDM6600)
	cfg = &msm_edpram_cfg;
	acc_cfg = &msm_edpram_access_cfg[DPRAM_SPEED_LOW];
	setup_sromc(cfg->csn, cfg, acc_cfg);

	if (!msm_edpram_remap_mem_region(&msm_edpram_cfg))
		return -1;
	platform_device_register(&cdma_modem);
#endif

/* For ESC6270 modem */
#if defined(CONFIG_GSM_MODEM_ESC6270)

#if defined(CONFIG_MACH_T0_CHN_CTC)
	gsm_edpram_cfg.csn = 0;
#else
	gsm_edpram_cfg.csn = 1;
#endif
	gsm_edpram_cfg.addr = SROM_CS0_BASE + (SROM_WIDTH * gsm_edpram_cfg.csn);
	gsm_edpram_cfg.end = gsm_edpram_cfg.addr + gsm_edpram_cfg.size - 1;

	cfg = &gsm_edpram_cfg;
	acc_cfg = &gsm_edpram_access_cfg[DPRAM_SPEED_LOW];
	setup_sromc(cfg->csn, cfg, acc_cfg);

	if (!gsm_edpram_remap_mem_region(&gsm_edpram_cfg))
		return -1;

	platform_device_register(&gsm_modem);
#endif
	return 0;
}

late_initcall(init_modem);
/*device_initcall(init_modem);*/

#ifdef CONFIG_USBHUB_USB3503
static int (*usbhub_set_mode) (struct usb3503_hubctl *, int);
static struct usb3503_hubctl *usbhub_ctl;

void set_host_states(struct platform_device *pdev, int type)
{
}

static int usb3503_hub_handler(void (*set_mode) (void), void *ctl)
{
	if (!set_mode || !ctl)
		return -EINVAL;

	usbhub_set_mode = (int (*)(struct usb3503_hubctl *, int))set_mode;
	usbhub_ctl = (struct usb3503_hubctl *)ctl;

	pr_info("[MDM] <%s> set_mode(%pF)\n", __func__, set_mode);

	return 0;
}

static int usb3503_hw_config(void)
{
	int err;

#if 0	/* It is not connected. shawn */
	err = gpio_request(GPIO_USB_HUB_CONNECT, "HUB_CONNECT");
	if (err) {
		pr_err("fail to request gpio %s\n", "HUB_CONNECT");
	} else {
		gpio_direction_output(GPIO_USB_HUB_CONNECT, 0);
		s3c_gpio_setpull(GPIO_USB_HUB_CONNECT, S3C_GPIO_PULL_NONE);
	}
	s5p_gpio_set_drvstr(GPIO_USB_HUB_CONNECT, S5P_GPIO_DRVSTR_LV1);
#endif

	err = gpio_request(GPIO_USB_BOOT_EN, "USB_BOOT_EN");
	if (err) {
		pr_err("fail to request gpio %s\n",
		       "USB_BOOT_EN");
	} else {
		gpio_direction_output(GPIO_USB_BOOT_EN, 0);
		s3c_gpio_setpull(GPIO_USB_BOOT_EN,
				 S3C_GPIO_PULL_NONE);
	}

	msleep(100);

	err = gpio_request(GPIO_USB_HUB_RST, "HUB_RST");
	if (err) {
		pr_err("fail to request gpio %s\n", "HUB_RST");
	} else {
		gpio_direction_output(GPIO_USB_HUB_RST, 0);
		s3c_gpio_setpull(GPIO_USB_HUB_RST, S3C_GPIO_PULL_NONE);
	}
	s5p_gpio_set_drvstr(GPIO_USB_HUB_RST, S5P_GPIO_DRVSTR_LV1);
	/* need to check drvstr 1 or 2 */

	/* for USB3503 26Mhz Reference clock setting */
	err = gpio_request(GPIO_USB_HUB_INT, "HUB_INT");
	if (err) {
		pr_err("fail to request gpio %s\n", "HUB_INT");
	} else {
		gpio_direction_output(GPIO_USB_HUB_INT, 1);
		s3c_gpio_setpull(GPIO_USB_HUB_INT, S3C_GPIO_PULL_NONE);
	}

	return 0;
}

static int usb3503_reset_n(int val)
{
	gpio_set_value(GPIO_USB_HUB_RST, 0);
	msleep(20);
	pr_info("[MDM] <%s> val = %d\n", __func__,
		gpio_get_value(GPIO_USB_HUB_RST));
	gpio_set_value(GPIO_USB_HUB_RST, !!val);

	pr_info("[MDM] <%s> val = %d\n", __func__,
		gpio_get_value(GPIO_USB_HUB_RST));

	udelay(5);		/* need it ? */
	return 0;
}

static struct usb3503_platform_data usb3503_pdata = {
	.initial_mode = USB3503_MODE_STANDBY,
	.reset_n = usb3503_reset_n,
	.register_hub_handler = usb3503_hub_handler,
	.port_enable = host_port_enable,
};

static struct i2c_board_info i2c_devs20_emul[] __initdata = {
	{
	 I2C_BOARD_INFO(USB3503_I2C_NAME, 0x08),
	 .platform_data = &usb3503_pdata,
	 },
};

/* I2C20_EMUL */
static struct i2c_gpio_platform_data i2c20_platdata = {
	.sda_pin = GPIO_USB_HUB_SDA,
	.scl_pin = GPIO_USB_HUB_SCL,
	/*FIXME: need to timming tunning...  */
	.udelay = 20,
};

static struct platform_device s3c_device_i2c20 = {
	.name = "i2c-gpio",
	.id = 20,
	.dev.platform_data = &i2c20_platdata,
};

static int __init init_usbhub(void)
{
	usb3503_hw_config();
	i2c_register_board_info(20, i2c_devs20_emul,
				ARRAY_SIZE(i2c_devs20_emul));

	platform_device_register(&s3c_device_i2c20);
	return 0;
}

device_initcall(init_usbhub);

static int host_port_enable(int port, int enable)
{
	int err;

	pr_info("[MDM] <%s> port(%d) control(%d)\n", __func__, port, enable);

	if (enable) {
		err = usbhub_set_mode(usbhub_ctl, USB3503_MODE_HUB);
		if (err < 0) {
			pr_err("[MDM] <%s> hub on fail\n", __func__);
			goto exit;
		}
		err = s5p_ehci_port_control(&s5p_device_ehci, port, 1);
		if (err < 0) {
			pr_err("[MDM] <%s> port(%d) enable fail\n", __func__,
			       port);
			goto exit;
		}
	} else {
		err = s5p_ehci_port_control(&s5p_device_ehci, port, 0);
		if (err < 0) {
			pr_err("[MDM] <%s> port(%d) enable fail\n", __func__,
			       port);
			goto exit;
		}
		err = usbhub_set_mode(usbhub_ctl, USB3503_MODE_STANDBY);
		if (err < 0) {
			pr_err("[MDM] <%s> hub off fail\n", __func__);
			goto exit;
		}

		gpio_direction_output(GPIO_USB_BOOT_EN, 0);
		s3c_gpio_setpull(GPIO_USB_BOOT_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_USB_BOOT_EN, 0);

		gpio_direction_output(GPIO_USB_BOOT_EN, 1);
		gpio_set_value(GPIO_USB_BOOT_EN, 1);

		gpio_direction_output(GPIO_BOOT_SW_SEL, 0);
		s3c_gpio_setpull(GPIO_BOOT_SW_SEL, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_BOOT_SW_SEL, 0);

		gpio_direction_output(GPIO_BOOT_SW_SEL, 1);
		gpio_set_value(GPIO_BOOT_SW_SEL, 1);
	}

 exit:
	return err;
}
#else
#if defined(CONFIG_MACH_J_CHN_CTC)
void set_host_states(struct platform_device *pdev, int type)
{
}
#elif !defined(CONFIG_MACH_J_CHN_CU)
void set_host_states(struct platform_device *pdev, int type)
{
	if (active_ctl.gpio_initialized) {
		pr_err(" [MODEM_IF] Active States =%d, %s\n", type, pdev->name);
		gpio_direction_output(modem_link_pm_data.gpio_link_active,
				      type);
	} else
		active_ctl.gpio_request_host_active = 1;
}
#endif
#endif
