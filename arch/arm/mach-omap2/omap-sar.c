/*
 * OMAP4 Save Restore source file
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 * Tero Kristo <t-kristo@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include "iomap.h"
#include "pm.h"
#include <mach/ctrl_module_wkup_44xx.h>

#include "common.h"
#include "clockdomain.h"

#include "omap4-sar-layout.h"
#include "cm-regbits-44xx.h"
#include "cm-regbits-54xx.h"
#include "prcm44xx.h"
#include "cminst44xx.h"
#include "iomap.h"

/*
 * These SECURE control registers are used to work-around
 * DDR corruption on the second chip select on OMAP443x.
 */
#define OMAP4_CTRL_SECURE_EMIF1_SDRAM_CONFIG2_REG	0x0114
#define OMAP4_CTRL_SECURE_EMIF2_SDRAM_CONFIG2_REG	0x011c

static void __iomem *sar_ram_base;
static struct powerdomain *l3init_pwrdm;
	static struct clockdomain *l3init_clkdm;
static struct clk *usb_host_ck, *usb_tll_ck;

struct sar_ram_entry {
	void __iomem *io_base;
	u32 offset;
	u32 size;
	u32 ram_addr;
};

struct sar_overwrite_entry {
	u32 reg_addr;
	u32 sar_offset;
	bool valid;
};

enum {
	MEMIF_CLKSTCTRL_IDX,
	SHADOW_FREQ_CFG2_IDX,
	SHADOW_FREQ_CFG1_IDX,
	MEMIF_CLKSTCTRL_2_IDX,
	HSUSBHOST_CLKCTRL_IDX,
	HSUSBHOST_CLKCTRL_2_IDX,
	OW_IDX_SIZE
};

static struct sar_ram_entry *sar_ram_layout[3];
static struct sar_overwrite_entry *sar_overwrite_data;

static struct sar_overwrite_entry omap4_sar_overwrite_data[OW_IDX_SIZE] = {
	[MEMIF_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e0c },
	[SHADOW_FREQ_CFG2_IDX] = { .reg_addr = 0x4a004e2c },
	[SHADOW_FREQ_CFG1_IDX] = { .reg_addr = 0x4a004e30 },
	[MEMIF_CLKSTCTRL_2_IDX] = { .reg_addr = 0x4a009e0c },
	[HSUSBHOST_CLKCTRL_IDX] = { .reg_addr = 0x4a009e54 },
	[HSUSBHOST_CLKCTRL_2_IDX] = { .reg_addr = 0x4a009e54 },
};

static struct sar_overwrite_entry omap5_sar_overwrite_data[OW_IDX_SIZE] = {
	[MEMIF_CLKSTCTRL_IDX] = { .reg_addr = 0x4a009e24 },
	[MEMIF_CLKSTCTRL_2_IDX] = { .reg_addr = 0x4a009e24 },
	[SHADOW_FREQ_CFG1_IDX] = { .reg_addr = 0x4a004e38 },
	[HSUSBHOST_CLKCTRL_IDX] = { .reg_addr = 0x4a009e60 },
	[HSUSBHOST_CLKCTRL_2_IDX] = { .reg_addr = 0x4a009e60 },
};

static void check_overwrite_data(u32 io_addr, u32 ram_addr, int size)
{
	int i;

	while (size) {
		for (i = 0; i < OW_IDX_SIZE; i++) {
			if (sar_overwrite_data[i].reg_addr == io_addr &&
			    !sar_overwrite_data[i].valid) {
				sar_overwrite_data[i].sar_offset = ram_addr;
				sar_overwrite_data[i].valid = true;
				break;
			}
		}
		size--;
		io_addr += 4;
		ram_addr += 4;
	}
}

/*
 * omap_sar_save :
 * common routine to save the registers to  SAR RAM with the
 * given parameters
 * @nb_regs - Number of registers to saved
 * @sar_bank_offset - where to backup
 * @sar_layout - constant table containing the backup info
 */
static void sar_save(struct sar_ram_entry *entry)
{
	u32 reg_val, size, i;
	void __iomem *reg_read_addr, *sar_wr_addr;

	while (entry->size) {
		size = entry->size;
		reg_read_addr = entry->io_base + entry->offset;
		sar_wr_addr = sar_ram_base + entry->ram_addr;
		for (i = 0; i < size; i++) {
			reg_val = __raw_readl(reg_read_addr + i * 4);
			__raw_writel(reg_val, sar_wr_addr + i * 4);
		}
		entry++;
	}
}

static void save_sar_bank3(void)
{
	struct clockdomain *l4_secure_clkdm;

	/*
	 * Not supported on ES1.0 silicon
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN_ONCE(1, "omap4: SAR backup not supported on ES1.0 ..\n");
		return;
	}

	l4_secure_clkdm = clkdm_lookup("l4_secure_clkdm");
	clkdm_wakeup(l4_secure_clkdm);

	sar_save(sar_ram_layout[2]);

	clkdm_allow_idle(l4_secure_clkdm);
}

static int omap4_sar_not_accessible(void)
{
	u32 usbhost_state, usbtll_state;

	/*
	 * Make sure that USB host and TLL modules are not
	 * enabled before attempting to save the context
	 * registers, otherwise this will trigger an exception.
	 */
	usbhost_state = omap4_cminst_read_inst_reg(OMAP4430_CM2_PARTITION,
				OMAP4430_CM2_L3INIT_INST,
				OMAP4_CM_L3INIT_USB_HOST_CLKCTRL_OFFSET)
	    & (OMAP4430_STBYST_MASK | OMAP4430_IDLEST_MASK);

	usbtll_state = omap4_cminst_read_inst_reg(OMAP4430_CM2_PARTITION,
				OMAP4430_CM2_L3INIT_INST,
				OMAP4_CM_L3INIT_USB_TLL_CLKCTRL_OFFSET)
	    & OMAP4430_IDLEST_MASK;

	if ((usbhost_state == (OMAP4430_STBYST_MASK | OMAP4430_IDLEST_MASK)) &&
	    (usbtll_state == (OMAP4430_IDLEST_MASK)))
		return 0;
	else
		return -EBUSY;
}

 /*
  * omap_sar_save -
  * Save the context to SAR_RAM1 and SAR_RAM2 as per
  * omap4xxx_sar_ram1_layout and omap4xxx_sar_ram2_layout for the device OFF
  * mode
  */
int omap_sar_save(void)
{
	/*
	 * Not supported on ES1.0 silicon
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN_ONCE(1, "omap4: SAR backup not supported on ES1.0 ..\n");
		return -ENODEV;
	}

	if (omap4_sar_not_accessible()) {
		pr_debug("%s: USB SAR CNTX registers are not accessible!\n",
			 __func__);
		return -EBUSY;
	}

	/*
	 * SAR bits and clocks needs to be enabled
	 */
	clkdm_wakeup(l3init_clkdm);
	pwrdm_enable_hdwr_sar(l3init_pwrdm);
	clk_enable(usb_host_ck);
	clk_enable(usb_tll_ck);

	/* Save SAR BANK1 */
	sar_save(sar_ram_layout[0]);

	clk_disable(usb_host_ck);
	clk_disable(usb_tll_ck);
	pwrdm_disable_hdwr_sar(l3init_pwrdm);
	clkdm_allow_idle(l3init_clkdm);

	/* Save SAR BANK2 */
	sar_save(sar_ram_layout[1]);

	return 0;
}

/**
 * omap4_sar_overwrite :
 * This API overwrite some of the SAR locations as a special cases
 * The register content to be saved can be the register value before
 * going into OFF-mode or a value that is required on wake up. This means
 * that the restored register value can be different from the last value
 * of the register before going into OFF-mode
 *	- CM1 and CM2 configuration
 *		Bits 0 of the CM_SHADOW_FREQ_CONFIG1 regiser and the
 *		CM_SHADOW_FREQ_CONFIG2 register are self-clearing and must
 *		 be set at restore time. Thus, these data must always be
 *		overwritten in the SAR RAM.
 *	- Because USBHOSTHS and USBTLL restore needs a particular
 *		sequencing, the software must overwrite data read from
 *		the following registers implied in phase2a and phase 2b
 */
void omap_sar_overwrite(void)
{
	u32 val = 0;
	u32 offset = 0;

	if (sar_overwrite_data[MEMIF_CLKSTCTRL_IDX].valid)
		__raw_writel(0x2, sar_ram_base +
			sar_overwrite_data[MEMIF_CLKSTCTRL_IDX].sar_offset);

	if (sar_overwrite_data[SHADOW_FREQ_CFG1_IDX].valid) {
		offset = sar_overwrite_data[SHADOW_FREQ_CFG1_IDX].sar_offset;
		val = __raw_readl(sar_ram_base + offset);
		val |= 1 << OMAP4430_FREQ_UPDATE_SHIFT;
		val &= ~OMAP4430_DLL_OVERRIDE_2_2_MASK;
		__raw_writel(val, sar_ram_base + offset);
	}

	if (sar_overwrite_data[SHADOW_FREQ_CFG2_IDX].valid) {
		offset = sar_overwrite_data[SHADOW_FREQ_CFG2_IDX].sar_offset;
		val = __raw_readl(sar_ram_base + offset);
		/*
		 * FIXME: Implement FREQ UPDATE for L#/M5 before enabling
		 * val |= 1 << OMAP4430_FREQ_UPDATE_SHIFT;
		 */
		__raw_writel(val, sar_ram_base + offset);
	}

	if (sar_overwrite_data[MEMIF_CLKSTCTRL_2_IDX].valid)
		__raw_writel(0x3, sar_ram_base +
			sar_overwrite_data[MEMIF_CLKSTCTRL_2_IDX].sar_offset);

	if (sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].valid) {
		offset = sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].sar_offset;

		/* Overwriting Phase2a data to be restored */
		/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 1, MODULEMODE = 2 */
		__raw_writel(OMAP4430_SAR_MODE_MASK | 0x2,
			sar_ram_base + offset);
		/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 1, MODULEMODE = 1 */
		__raw_writel(OMAP4430_SAR_MODE_MASK | 0x1,
			sar_ram_base + offset + 4);
		/*
		 * CM2 CM_SDMA_STATICDEP : Enable static depedency for
		 * SAR modules
		 */
		__raw_writel(OMAP4430_L4WKUP_STATDEP_MASK |
			     OMAP4430_L4CFG_STATDEP_MASK |
			     OMAP4430_L3INIT_STATDEP_MASK |
			     OMAP4430_L3_1_STATDEP_MASK |
			     OMAP4430_L3_2_STATDEP_MASK |
			     OMAP4430_ABE_STATDEP_MASK,
			     sar_ram_base + offset + 8);
	}

	if (sar_overwrite_data[HSUSBHOST_CLKCTRL_2_IDX].valid) {
		offset = sar_overwrite_data[HSUSBHOST_CLKCTRL_2_IDX].sar_offset;

		/* Overwriting Phase2b data to be restored */
		/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
		val = __raw_readl(OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL);
		val &= (OMAP4430_CLKSEL_UTMI_P1_MASK |
			OMAP4430_CLKSEL_UTMI_P2_MASK);
		__raw_writel(val, sar_ram_base + offset);
		/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
		__raw_writel(0, sar_ram_base + offset + 4);
		/* CM2 CM_SDMA_STATICDEP : Clear the static depedency */
		__raw_writel(OMAP4430_L3_2_STATDEP_MASK,
			     sar_ram_base + offset + 8);
	}

	if (sar_overwrite_data[MEMIF_CLKSTCTRL_2_IDX].valid)
		__raw_writel(0x3, sar_ram_base +
			sar_overwrite_data[MEMIF_CLKSTCTRL_2_IDX].sar_offset);

	if (sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].valid) {
		offset = sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].sar_offset;

		/* Overwriting Phase2a data to be restored */
		/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 1, MODULEMODE = 2 */
		__raw_writel(0x00000012, sar_ram_base + offset);
		/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 1, MODULEMODE = 1 */
		__raw_writel(0x00000011, sar_ram_base + offset + 4);
		/*
		 * CM2 CM_SDMA_STATICDEP : Enable static depedency for
		 * SAR modules
		 */
		__raw_writel(0x000090e8, sar_ram_base + offset + 8);
	}

	if (sar_overwrite_data[HSUSBHOST_CLKCTRL_IDX].valid) {
		offset = sar_overwrite_data[HSUSBHOST_CLKCTRL_2_IDX].sar_offset;

		/* Overwriting Phase2b data to be restored */
		/* CM_L3INIT_USB_HOST_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
		val = __raw_readl(OMAP4430_CM_L3INIT_USB_HOST_CLKCTRL);
		val &= (OMAP4430_CLKSEL_UTMI_P1_MASK |
			OMAP4430_CLKSEL_UTMI_P2_MASK);
		__raw_writel(val, sar_ram_base + offset);
		/* CM_L3INIT_USB_TLL_CLKCTRL: SAR_MODE = 0, MODULEMODE = 0 */
		__raw_writel(0x0000000, sar_ram_base + offset + 4);
		/* CM2 CM_SDMA_STATICDEP : Clear the static depedency */
		__raw_writel(0x00000040, sar_ram_base + offset + 8);
	}
	/* readback to ensure data reaches to SAR RAM */
	barrier();
	val = __raw_readl(sar_ram_base + offset + 8);
}

void __iomem *omap4_get_sar_ram_base(void)
{
	return sar_ram_base;
}

static const u32 sar_rom_phases[] = {
	0, 0x30, 0x60
};

struct sar_module {
	void __iomem *io_base;
	u32 base;
	u32 size;
	bool invalid;
};

static struct sar_module *sar_modules;

static void sar_ioremap_modules(void)
{
	struct sar_module *mod;

	mod = sar_modules;

	while (mod->base) {
		if (!mod->invalid) {
			mod->io_base = ioremap(mod->base, mod->size);
			if (!mod->io_base)
				pr_err("%s: ioremap failed for %08x[%08x]\n",
					__func__, mod->base, mod->size);
			BUG_ON(!mod->io_base);
		}
		mod++;
	}
}

static int set_sar_io_addr(struct sar_ram_entry *entry, u32 addr)
{
	struct sar_module *mod;

	mod = sar_modules;

	while (mod->base) {
		if (addr >= mod->base && addr <= mod->base + mod->size) {
			if (mod->invalid)
				break;
			entry->io_base = mod->io_base;
			entry->offset = addr - mod->base;
			return 0;
		}
		mod++;
	}
	pr_warn("%s: no matching sar_module for %08x\n", __func__, addr);
	return -EINVAL;
}

static int sar_layout_generate(void)
{
	int phase;
	void __iomem *sarrom;
	u32 rombase, romend, rambase, ramend;
	u32 offset, next;
	u16 size;
	u32 ram_addr, io_addr;
	void *sarram;
	struct sar_ram_entry *entry[3];
	int bank;
	int ret = 0;

	pr_info("generating sar_ram layout...\n");

	if (cpu_is_omap44xx()) {
		rombase = OMAP44XX_SAR_ROM_BASE;
		romend = rombase + SZ_8K;
		rambase = OMAP44XX_SAR_RAM_BASE;
		ramend = rambase + SAR_BANK4_OFFSET - 1;
	} else {
		rombase = OMAP54XX_SAR_ROM_BASE;
		romend = rombase + SZ_8K;
		rambase = OMAP54XX_SAR_RAM_BASE;
		ramend = rambase + SAR_BANK4_OFFSET - 1;
	}

	sarrom = ioremap(rombase, SZ_8K);

	/* Allocate temporary memory for sar ram layout */
	sarram = kmalloc(SAR_BANK4_OFFSET, GFP_KERNEL);
	for (bank = 0; bank < 3; bank++)
		entry[bank] = sarram + SAR_BANK2_OFFSET * bank;

	for (phase = 0; phase < ARRAY_SIZE(sar_rom_phases); phase++) {
		offset = sar_rom_phases[phase];

		while (1) {
			next = __raw_readl(sarrom + offset);
			size = __raw_readl(sarrom + offset + 4) & 0xffff;
			ram_addr = __raw_readl(sarrom + offset + 8);
			io_addr = __raw_readl(sarrom + offset + 12);

			if (ram_addr >= rambase && ram_addr <= ramend) {
				/* Valid ram address, add entry */
				ram_addr -= rambase;
				bank = ram_addr / SAR_BANK2_OFFSET;
				if (!set_sar_io_addr(entry[bank], io_addr)) {
					entry[bank]->size = size;
					entry[bank]->ram_addr = ram_addr;
					check_overwrite_data(io_addr, ram_addr,
						size);
					entry[bank]++;
				}
			}

			if (next < rombase || next > romend)
				break;

			offset = next - rombase;
		}
	}

	for (bank = 0; bank < 3; bank++) {
		size = (u32)entry[bank] -
			(u32)(sarram + SAR_BANK2_OFFSET * bank);
		sar_ram_layout[bank] = kmalloc(size +
			sizeof(struct sar_ram_entry), GFP_KERNEL);
		if (!sar_ram_layout[bank]) {
			pr_err("%s: kmalloc failed\n", __func__);
			goto cleanup;
		}
		memcpy(sar_ram_layout[bank], sarram + SAR_BANK2_OFFSET * bank,
			size);
		memset((void *)sar_ram_layout[bank] + size, 0,
			sizeof(struct sar_ram_entry));
		entry[bank] = sar_ram_layout[bank];
	}

cleanup:
	kfree(sarram);
	iounmap(sarrom);
	pr_info("sar ram layout created\n");
	return ret;
}

static struct sar_module omap44xx_sar_modules[] = {
	{ .base = OMAP44XX_EMIF1_BASE, .size = SZ_1M },
	{ .base = OMAP44XX_EMIF2_BASE, .size = SZ_1M },
	{ .base = OMAP44XX_DMM_BASE, .size = SZ_1M },
	{ .base = OMAP4430_CM1_BASE, .size = SZ_8K },
	{ .base = OMAP4430_CM2_BASE, .size = SZ_8K },
	{ .base = OMAP44XX_C2C_BASE, .size = SZ_1M },
	{ .base = OMAP443X_CTRL_BASE, .size = SZ_4K },
	{ .base = L3_44XX_BASE_CLK1, .size = SZ_1M },
	{ .base = L3_44XX_BASE_CLK2, .size = SZ_1M },
	{ .base = L3_44XX_BASE_CLK3, .size = SZ_1M },
	{ .base = OMAP44XX_USBTLL_BASE, .size = SZ_1M },
	{ .base = OMAP44XX_UHH_CONFIG_BASE, .size = SZ_1M },
	{ .base = L4_44XX_PHYS, .size = SZ_4M },
	{ .base = L4_PER_44XX_PHYS, .size = SZ_4M },
	{ .base = 0 },
};

static struct sar_module omap54xx_sar_modules[] = {
	{ .base = OMAP54XX_EMIF1_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_EMIF2_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_DMM_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_CM_CORE_AON_BASE, .size = SZ_8K },
	{ .base = OMAP54XX_CM_CORE_BASE, .size = SZ_8K },
	{ .base = OMAP54XX_C2C_BASE, .size = SZ_1M },
	{ .base = OMAP543X_CTRL_BASE, .size = SZ_4K },
	{ .base = OMAP543X_SCM_BASE, .size = SZ_4K },
	{ .base = L3_54XX_BASE_CLK1, .size = SZ_1M },
	{ .base = L3_54XX_BASE_CLK2, .size = SZ_1M },
	{ .base = L3_54XX_BASE_CLK3, .size = SZ_1M },
#ifndef CONFIG_MACH_OMAP_5430ZEBU
	{ .base = OMAP54XX_USBTLL_BASE, .size = SZ_1M },
	{ .base = OMAP54XX_UHH_CONFIG_BASE, .size = SZ_1M },
#else
	{ .base = OMAP54XX_USBTLL_BASE, .size = SZ_1M, .invalid = true },
	{ .base = OMAP54XX_UHH_CONFIG_BASE, .size = SZ_1M, .invalid = true },
#endif
	{ .base = L4_54XX_PHYS, .size = SZ_4M },
	{ .base = L4_PER_54XX_PHYS, .size = SZ_4M },
	{ .base = OMAP54XX_LLIA_BASE, .size = SZ_64K },
	{ .base = OMAP54XX_LLIB_BASE, .size = SZ_64K },
	{ .base = 0 },
};

/*
 * SAR RAM used to save and restore the HW
 * context in low power modes
 */
static int __init omap_sar_ram_init(void)
{
	void *base;
	pr_info("omap_sar_ram_init\n");

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return -ENODEV;

	/*
	 * Static mapping, never released Actual SAR area used is 8K it's
	 * spaced over 16K address with some part is reserved.
	 */

	if (cpu_is_omap44xx()) {
		base = OMAP44XX_SAR_RAM_BASE;
		sar_modules = omap44xx_sar_modules;
		sar_overwrite_data = omap4_sar_overwrite_data;
	} else {
		base = OMAP54XX_SAR_RAM_BASE,
		sar_modules = omap54xx_sar_modules;
		sar_overwrite_data = omap5_sar_overwrite_data;
	}

	sar_ram_base = ioremap(base, SZ_16K);
	BUG_ON(!sar_ram_base);

	sar_ioremap_modules();

	sar_layout_generate();

	/*
	 * SAR BANK3 contains all firewall settings and it's saved through
	 * secure API on HS device. On GP device these registers are
	 * meaningless but still needs to be saved. Otherwise Auto-restore
	 * phase DMA takes an abort. Hence save these conents only once
	 * in init to avoid the issue while waking up from device OFF
	 */
	if (omap_type() == OMAP2_DEVICE_TYPE_GP)
		save_sar_bank3();

	/*
	 * L3INIT PD and clocks are needed for SAR save phase
	 */
	l3init_pwrdm = pwrdm_lookup("l3init_pwrdm");
	if (!l3init_pwrdm)
		pr_err("Failed to get l3init_pwrdm\n");

	l3init_clkdm = clkdm_lookup("l3_init_clkdm");
	if (!l3init_clkdm)
		pr_err("Failed to get l3_init_clkdm\n");

	if (cpu_is_omap54xx()) {
		usb_host_ck = clk_get(NULL, "usb_host_hs_fck");
		if (IS_ERR(usb_host_ck))
			pr_err("Could not get usb_host_ck\n");

		usb_tll_ck = clk_get(NULL, "usb_tll_hs_ick");
		if (IS_ERR(usb_tll_ck))
			pr_err("Could not get usb_tll_ck\n");
	} else {
		usb_host_ck = clk_get_sys("usbhs_omap", "hs_fck");
		if (IS_ERR(usb_host_ck))
			pr_err("Could not get usb_host_ck\n");

		usb_tll_ck = clk_get_sys("usbhs_omap", "usbtll_ick");
		if (IS_ERR(usb_tll_ck))
			pr_err("Could not get usb_tll_ck\n");
	}

	return 0;
}
early_initcall(omap_sar_ram_init);

