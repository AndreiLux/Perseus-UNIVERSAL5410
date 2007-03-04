/*
 * mxc-pcm.h :- ASoC platform header for Freescale i.MX
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MXC_PCM_H
#define _MXC_PCM_H

#include <asm/arch/dma.h>

/* temp until imx-regs.h is up2date */
#define SSI1_STX0   (SSI1_BASE_ADDR + 0x00)
#define SSI1_STX0_PHYS   __PHYS_REG(SSI1_BASE_ADDR + 0x00)
#define SSI1_STX1   (SSI1_BASE_ADDR + 0x04)
#define SSI1_STX1_PHYS   __PHYS_REG(SSI1_BASE_ADDR + 0x04)
#define SSI1_SRX0   (SSI1_BASE_ADDR + 0x08)
#define SSI1_SRX0_PHYS   __PHYS_REG(SSI1_BASE_ADDR + 0x08)
#define SSI1_SRX1   (SSI1_BASE_ADDR + 0x0c)
#define SSI1_SRX1_PHYS   __PHYS_REG(SSI1_BASE_ADDR + 0x0c)
#define SSI1_SCR    (SSI1_BASE_ADDR + 0x10)
#define SSI1_SISR   (SSI1_BASE_ADDR + 0x14)
#define SSI1_SIER   (SSI1_BASE_ADDR + 0x18)
#define SSI1_STCR   (SSI1_BASE_ADDR + 0x1c)
#define SSI1_SRCR   (SSI1_BASE_ADDR + 0x20)
#define SSI1_STCCR  (SSI1_BASE_ADDR + 0x24)
#define SSI1_SRCCR  (SSI1_BASE_ADDR + 0x28)
#define SSI1_SFCSR  (SSI1_BASE_ADDR + 0x2c)
#define SSI1_STR    (SSI1_BASE_ADDR + 0x30)
#define SSI1_SOR    (SSI1_BASE_ADDR + 0x34)
#define SSI1_SACNT  (SSI1_BASE_ADDR + 0x38)
#define SSI1_SACADD (SSI1_BASE_ADDR + 0x3c)
#define SSI1_SACDAT (SSI1_BASE_ADDR + 0x40)
#define SSI1_SATAG  (SSI1_BASE_ADDR + 0x44)
#define SSI1_STMSK  (SSI1_BASE_ADDR + 0x48)
#define SSI1_SRMSK  (SSI1_BASE_ADDR + 0x4c)

#define SSI2_STX0   (SSI2_BASE_ADDR + 0x00)
#define SSI2_STX0_PHYS   __PHYS_REG(SSI2_BASE_ADDR + 0x00)
#define SSI2_STX1   (SSI2_BASE_ADDR + 0x04)
#define SSI2_STX1_PHYS   __PHYS_REG(SSI2_BASE_ADDR + 0x04)
#define SSI2_SRX0   (SSI2_BASE_ADDR + 0x08)
#define SSI2_SRX0_PHYS   __PHYS_REG(SSI2_BASE_ADDR + 0x08)
#define SSI2_SRX1   (SSI2_BASE_ADDR + 0x0c)
#define SSI2_SRX1_PHYS   __PHYS_REG(SSI2_BASE_ADDR + 0x0c)
#define SSI2_SCR    (SSI2_BASE_ADDR + 0x10)
#define SSI2_SISR   (SSI2_BASE_ADDR + 0x14)
#define SSI2_SIER   (SSI2_BASE_ADDR + 0x18)
#define SSI2_STCR   (SSI2_BASE_ADDR + 0x1c)
#define SSI2_SRCR   (SSI2_BASE_ADDR + 0x20)
#define SSI2_STCCR  (SSI2_BASE_ADDR + 0x24)
#define SSI2_SRCCR  (SSI2_BASE_ADDR + 0x28)
#define SSI2_SFCSR  (SSI2_BASE_ADDR + 0x2c)
#define SSI2_STR    (SSI2_BASE_ADDR + 0x30)
#define SSI2_SOR    (SSI2_BASE_ADDR + 0x34)
#define SSI2_SACNT  (SSI2_BASE_ADDR + 0x38)
#define SSI2_SACADD (SSI2_BASE_ADDR + 0x3c)
#define SSI2_SACDAT (SSI2_BASE_ADDR + 0x40)
#define SSI2_SATAG  (SSI2_BASE_ADDR + 0x44)
#define SSI2_STMSK  (SSI2_BASE_ADDR + 0x48)
#define SSI2_SRMSK  (SSI2_BASE_ADDR + 0x4c)

#define SSI_SCR_CLK_IST        (1 << 9)
#define SSI_SCR_TCH_EN         (1 << 8)
#define SSI_SCR_SYS_CLK_EN     (1 << 7)
#define SSI_SCR_I2S_MODE_NORM  (0 << 5)
#define SSI_SCR_I2S_MODE_MSTR  (1 << 5)
#define SSI_SCR_I2S_MODE_SLAVE (2 << 5)
#define SSI_SCR_SYN            (1 << 4)
#define SSI_SCR_NET            (1 << 3)
#define SSI_SCR_RE             (1 << 2)
#define SSI_SCR_TE             (1 << 1)
#define SSI_SCR_SSIEN          (1 << 0)

#define SSI_SISR_CMDAU         (1 << 18)
#define SSI_SISR_CMDDU         (1 << 17)
#define SSI_SISR_RXT           (1 << 16)
#define SSI_SISR_RDR1          (1 << 15)
#define SSI_SISR_RDR0          (1 << 14)
#define SSI_SISR_TDE1          (1 << 13)
#define SSI_SISR_TDE0          (1 << 12)
#define SSI_SISR_ROE1          (1 << 11)
#define SSI_SISR_ROE0          (1 << 10)
#define SSI_SISR_TUE1          (1 << 9)
#define SSI_SISR_TUE0          (1 << 8)
#define SSI_SISR_TFS           (1 << 7)
#define SSI_SISR_RFS           (1 << 6)
#define SSI_SISR_TLS           (1 << 5)
#define SSI_SISR_RLS           (1 << 4)
#define SSI_SISR_RFF1          (1 << 3)
#define SSI_SISR_RFF0          (1 << 2)
#define SSI_SISR_TFE1          (1 << 1)
#define SSI_SISR_TFE0          (1 << 0)

#define SSI_SIER_RDMAE         (1 << 22)
#define SSI_SIER_RIE           (1 << 21)
#define SSI_SIER_TDMAE         (1 << 20)
#define SSI_SIER_TIE           (1 << 19)
#define SSI_SIER_CMDAU_EN      (1 << 18)
#define SSI_SIER_CMDDU_EN      (1 << 17)
#define SSI_SIER_RXT_EN        (1 << 16)
#define SSI_SIER_RDR1_EN       (1 << 15)
#define SSI_SIER_RDR0_EN       (1 << 14)
#define SSI_SIER_TDE1_EN       (1 << 13)
#define SSI_SIER_TDE0_EN       (1 << 12)
#define SSI_SIER_ROE1_EN       (1 << 11)
#define SSI_SIER_ROE0_EN       (1 << 10)
#define SSI_SIER_TUE1_EN       (1 << 9)
#define SSI_SIER_TUE0_EN       (1 << 8)
#define SSI_SIER_TFS_EN        (1 << 7)
#define SSI_SIER_RFS_EN        (1 << 6)
#define SSI_SIER_TLS_EN        (1 << 5)
#define SSI_SIER_RLS_EN        (1 << 4)
#define SSI_SIER_RFF1_EN       (1 << 3)
#define SSI_SIER_RFF0_EN       (1 << 2)
#define SSI_SIER_TFE1_EN       (1 << 1)
#define SSI_SIER_TFE0_EN       (1 << 0)

#define SSI_STCR_TXBIT0        (1 << 9)
#define SSI_STCR_TFEN1         (1 << 8)
#define SSI_STCR_TFEN0         (1 << 7)
#define SSI_STCR_TFDIR         (1 << 6)
#define SSI_STCR_TXDIR         (1 << 5)
#define SSI_STCR_TSHFD         (1 << 4)
#define SSI_STCR_TSCKP         (1 << 3)
#define SSI_STCR_TFSI          (1 << 2)
#define SSI_STCR_TFSL          (1 << 1)
#define SSI_STCR_TEFS          (1 << 0)

#define SSI_SRCR_RXBIT0        (1 << 9)
#define SSI_SRCR_RFEN1         (1 << 8)
#define SSI_SRCR_RFEN0         (1 << 7)
#define SSI_SRCR_RFDIR         (1 << 6)
#define SSI_SRCR_RXDIR         (1 << 5)
#define SSI_SRCR_RSHFD         (1 << 4)
#define SSI_SRCR_RSCKP         (1 << 3)
#define SSI_SRCR_RFSI          (1 << 2)
#define SSI_SRCR_RFSL          (1 << 1)
#define SSI_SRCR_REFS          (1 << 0)

#define SSI_STCCR_DIV2         (1 << 18)
#define SSI_STCCR_PSR          (1 << 15)
#define SSI_STCCR_WL(x)        ((((x) - 2) >> 1) << 13)
#define SSI_STCCR_DC(x)        (((x) & 0x1f) << 8)
#define SSI_STCCR_PM(x)        (((x) & 0xff) << 0)

#define SSI_SRCCR_DIV2         (1 << 18)
#define SSI_SRCCR_PSR          (1 << 15)
#define SSI_SRCCR_WL(x)        ((((x) - 2) >> 1) << 13)
#define SSI_SRCCR_DC(x)        (((x) & 0x1f) << 8)
#define SSI_SRCCR_PM(x)        (((x) & 0xff) << 0)


#define SSI_SFCSR_RFCNT1(x)   (((x) & 0xf) << 28)
#define SSI_SFCSR_TFCNT1(x)   (((x) & 0xf) << 24)
#define SSI_SFCSR_RFWM1(x)    (((x) & 0xf) << 20)
#define SSI_SFCSR_TFWM1(x)    (((x) & 0xf) << 16)
#define SSI_SFCSR_RFCNT0(x)   (((x) & 0xf) << 12)
#define SSI_SFCSR_TFCNT0(x)   (((x) & 0xf) <<  8)
#define SSI_SFCSR_RFWM0(x)    (((x) & 0xf) <<  4)
#define SSI_SFCSR_TFWM0(x)    (((x) & 0xf) <<  0)

#define SSI_STR_TEST          (1 << 15)
#define SSI_STR_RCK2TCK       (1 << 14)
#define SSI_STR_RFS2TFS       (1 << 13)
#define SSI_STR_RXSTATE(x)    (((x) & 0xf) << 8)
#define SSI_STR_TXD2RXD       (1 <<  7)
#define SSI_STR_TCK2RCK       (1 <<  6)
#define SSI_STR_TFS2RFS       (1 <<  5)
#define SSI_STR_TXSTATE(x)    (((x) & 0xf) << 0)

#define SSI_SOR_CLKOFF        (1 << 6)
#define SSI_SOR_RX_CLR        (1 << 5)
#define SSI_SOR_TX_CLR        (1 << 4)
#define SSI_SOR_INIT          (1 << 3)
#define SSI_SOR_WAIT(x)       (((x) & 0x3) << 1)
#define SSI_SOR_SYNRST        (1 << 0)

#define SSI_SACNT_FRDIV(x)    (((x) & 0x3f) << 5)
#define SSI_SACNT_WR          (x << 4)
#define SSI_SACNT_RD          (x << 3)
#define SSI_SACNT_TIF         (x << 2)
#define SSI_SACNT_FV          (x << 1)
#define SSI_SACNT_AC97EN      (x << 0)


/* AUDMUX registers */
#define AUDMUX_HPCR1         (IMX_AUDMUX_BASE + 0x00)
#define AUDMUX_HPCR2         (IMX_AUDMUX_BASE + 0x04)
#define AUDMUX_HPCR3         (IMX_AUDMUX_BASE + 0x08)
#define AUDMUX_PPCR1         (IMX_AUDMUX_BASE + 0x10)
#define AUDMUX_PPCR2         (IMX_AUDMUX_BASE + 0x14)
#define AUDMUX_PPCR3         (IMX_AUDMUX_BASE + 0x18)

#define AUDMUX_HPCR_TFSDIR         (1 << 31)
#define AUDMUX_HPCR_TCLKDIR        (1 << 30)
#define AUDMUX_HPCR_TFCSEL_TX      (0 << 26)
#define AUDMUX_HPCR_TFCSEL_RX      (8 << 26)
#define AUDMUX_HPCR_TFCSEL(x)      (((x) & 0x7) << 26)
#define AUDMUX_HPCR_RFSDIR         (1 << 25)
#define AUDMUX_HPCR_RCLKDIR        (1 << 24)
#define AUDMUX_HPCR_RFCSEL_TX      (0 << 20)
#define AUDMUX_HPCR_RFCSEL_RX      (8 << 20)
#define AUDMUX_HPCR_RFCSEL(x)      (((x) & 0x7) << 20)
#define AUDMUX_HPCR_RXDSEL(x)      (((x) & 0x7) << 13)
#define AUDMUX_HPCR_SYN            (1 << 12)
#define AUDMUX_HPCR_TXRXEN         (1 << 10)
#define AUDMUX_HPCR_INMEN          (1 <<  8)
#define AUDMUX_HPCR_INMMASK(x)     (((x) & 0xff) << 0)

#define AUDMUX_PPCR_TFSDIR         (1 << 31)
#define AUDMUX_PPCR_TCLKDIR        (1 << 30)
#define AUDMUX_PPCR_TFCSEL_TX      (0 << 26)
#define AUDMUX_PPCR_TFCSEL_RX      (8 << 26)
#define AUDMUX_PPCR_TFCSEL(x)      (((x) & 0x7) << 26)
#define AUDMUX_PPCR_RFSDIR         (1 << 25)
#define AUDMUX_PPCR_RCLKDIR        (1 << 24)
#define AUDMUX_PPCR_RFCSEL_TX      (0 << 20)
#define AUDMUX_PPCR_RFCSEL_RX      (8 << 20)
#define AUDMUX_PPCR_RFCSEL(x)      (((x) & 0x7) << 20)
#define AUDMUX_PPCR_RXDSEL(x)      (((x) & 0x7) << 13)
#define AUDMUX_PPCR_SYN            (1 << 12)
#define AUDMUX_PPCR_TXRXEN         (1 << 10)

#define SDMA_TXFIFO_WATERMARK				0x4
#define SDMA_RXFIFO_WATERMARK				0x6

struct mxc_pcm_dma_params {
	char *name;			/* stream identifier */
	dma_channel_params params;
};

extern struct snd_soc_cpu_dai mxc_ssi_dai[3];

/* platform data */
extern struct snd_soc_platform mxc_soc_platform;
extern struct snd_ac97_bus_ops mxc_ac97_ops;

#endif
