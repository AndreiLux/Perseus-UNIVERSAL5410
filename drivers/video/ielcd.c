/* linux/drivers/video/ielcd.c
 *
 * Register interface file for Samsung IELCD driver
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <plat/clock.h>
#include <plat/regs-fb.h>
#include <plat/regs-fb-v4.h>
#include <plat/regs-ielcd.h>
#include <plat/regs-mdnie.h>
#include <mach/gpio.h>
#include <mach/regs-clock.h>

#include "mdnie.h"

#define FIMD_REG_BASE			S5P_PA_FIMD1
#ifdef CONFIG_FB_EXYNOS_FIMD_V8
#define FIMD_MAP_SIZE			SZ_256K
#else
#define FIMD_MAP_SIZE			SZ_32K
#endif

#define s3c_ielcd_readl(addr)		readl(s3c_ielcd_base + addr)
#define s3c_ielcd_writel(addr, val)	writel(val, s3c_ielcd_base + addr)

static void __iomem *fimd_reg;
static void __iomem *mdnie_reg;
static struct resource *s3c_ielcd_mem;
static void __iomem *s3c_ielcd_base;

int s3c_ielcd_hw_init(void)
{
	s3c_ielcd_mem = request_mem_region(S3C_IELCD_PHY_BASE, S3C_IELCD_MAP_SIZE, "ielcd");
	if (IS_ERR_OR_NULL(s3c_ielcd_mem)) {
		pr_err("%s: fail to request_mem_region\n", __func__);
		return -ENOENT;
	}

	s3c_ielcd_base = ioremap(S3C_IELCD_PHY_BASE, S3C_IELCD_MAP_SIZE);
	if (IS_ERR_OR_NULL(s3c_ielcd_base)) {
		pr_err("%s: fail to ioremap\n", __func__);
		return -ENOENT;
	}

	fimd_reg = ioremap(FIMD_REG_BASE, FIMD_MAP_SIZE);
	if (fimd_reg == NULL) {
		pr_err("%s: fail to ioremap - fimd\n", __func__);
		return -ENOENT;
	}

	mdnie_reg = ioremap(0x14520000, S3C_IELCD_MAP_SIZE);
	if (mdnie_reg == NULL) {
		pr_err("%s: fail to ioremap - mdnie\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static int s3c_set_dp_mie_clkcon(int mode)
{
	void __iomem *base_reg = fimd_reg;

	writel(mode, base_reg + S3C_DP_MIE_CLKCON);

	return 0;
}

int s3c_fimd1_display_on(void)
{
	u32 cfg;
	void __iomem *base_reg = fimd_reg;

	cfg = readl(base_reg + VIDCON0);
	cfg |= (VIDCON0_ENVID | VIDCON0_ENVID_F);
	writel(cfg, base_reg + VIDCON0);

	return 0;
}

int s3c_fimd1_display_off(void)
{
	u32 cfg, active = 0, fimd_count = 0;
	void __iomem *base_reg = fimd_reg;

	/* check active area */
	cfg = readl(base_reg + VIDCON1);
	cfg &= VIDCON1_VSTATUS_MASK;
	if (cfg == VIDCON1_VSTATUS_ACTIVE)
		active = 1;

	/* FIMD per frame off in active area and direct off in non active area */
	cfg = readl(base_reg + VIDCON0);
	if (active)
		cfg &= ~(VIDCON0_ENVID_F);
	else
		cfg &= ~(VIDCON0_ENVID | VIDCON0_ENVID_F);
	writel(cfg, base_reg + VIDCON0);

	do {
		if (++fimd_count > 2000000) {
			printk(KERN_ERR "fimd off fail\n");
			return 1;
		}

		if (!(readl(base_reg + VIDCON0) & VIDCON0_ENVID_F))
			break;
	} while (1);

	return 0;
}

static void get_lcd_size(unsigned int *xres, unsigned int *yres)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;

	/* LCD size setting */
	cfg = readl(base_reg + VIDTCON2);
	*xres = ((cfg & VIDTCON2_HOZVAL_MASK) >> VIDTCON2_HOZVAL_SHIFT) + 1;
	*yres = ((cfg & VIDTCON2_LINEVAL_MASK) >> VIDTCON2_LINEVAL_SHIFT) + 1;
	*xres |= (cfg & VIDTCON2_HOZVAL_E_MASK) ? (1 << 11) : 0;	/* 11 is MSB */
	*yres |= (cfg & VIDTCON2_LINEVAL_E_MASK) ? (1 << 11) : 0;	/* 11 is MSB */
}

static int ielcd_logic_start(void)
{
	s3c_ielcd_writel(IELCD_AUXCON, IELCD_MAGIC_KEY);

	return 0;
}

static void ielcd_set_polarity(void)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;

	cfg = readl(base_reg + VIDCON1);
	cfg &= (VIDCON1_INV_VCLK | VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC | VIDCON1_INV_VDEN);
	s3c_ielcd_writel(IELCD_VIDCON1, cfg);
}

static void ielcd_set_timing(void)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;

	/*LCD verical porch setting*/
	cfg = readl(base_reg + VIDTCON0);
	s3c_ielcd_writel(IELCD_VIDTCON0, cfg);

	/*LCD horizontal porch setting*/
	cfg = readl(base_reg + VIDTCON1);
	s3c_ielcd_writel(IELCD_VIDTCON1, cfg);
}

static void ielcd_set_lcd_size(unsigned int xres, unsigned int yres)
{
	unsigned int cfg;

	cfg = (IELCD_VIDTCON2_LINEVAL_FOR_5410(yres-1) | IELCD_VIDTCON2_HOZVAL(xres-1));
	s3c_ielcd_writel(IELCD_VIDTCON2, cfg);
}

#ifdef CONFIG_S5P_DP
void s3c_ielcd_hw_trigger_check(void)
{
	unsigned int cfg;
	unsigned char count = 0;
	ktime_t start;

	start = ktime_get();

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg &= ~(SWTRGCMD_W0BUF | TRGMODE_W0BUF);
	cfg |= (SWTRGCMD_W0BUF | TRGMODE_W0BUF);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);

	do {
		cfg = s3c_ielcd_readl(IELCD_SHADOWCON);
		cfg |= IELCD_W0_SW_SHADOW_UPTRIG;
		s3c_ielcd_writel(IELCD_SHADOWCON, cfg);

		cfg = s3c_ielcd_readl(IELCD_VIDCON0);
		cfg |= IELCD_SW_SHADOW_UPTRIG;
		s3c_ielcd_writel(IELCD_VIDCON0, cfg);

		count++;
		if (count > 10000) {
			pr_err("ielcd start fail\n");
			BUG();
			break;
		}
	} while (s3c_ielcd_readl(IELCD_WINCON0) & (1 << 25));

	pr_debug("%s: time = %lld us\n", __func__, 
		ktime_us_delta(ktime_get(), start));
}
#endif

int s3c_ielcd_display_on(void)
{
	unsigned int cfg;

	cfg = s3c_ielcd_readl(IELCD_VIDCON0);
	cfg |= (VIDCON0_ENVID | VIDCON0_ENVID_F);
	s3c_ielcd_writel(IELCD_VIDCON0, cfg);

#ifdef CONFIG_S5P_DP
	s3c_ielcd_hw_trigger_check();
#endif

	return 0;
}

int s3c_ielcd_display_off(void)
{
	unsigned int cfg, ielcd_count = 0;

	cfg = s3c_ielcd_readl(IELCD_VIDCON0);
	cfg &= ~(VIDCON0_ENVID | VIDCON0_ENVID_F);

	s3c_ielcd_writel(IELCD_VIDCON0, cfg);

	do {
		if (++ielcd_count > 2000000) {
			pr_err("ielcd off fail\n");
			return 1;
		}

		if (!(s3c_ielcd_readl(IELCD_VIDCON1) & VIDCON1_LINECNT_MASK))
			return 0;
	} while (1);
}

static void s3c_ielcd_config_rgb(void)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;

	/* vclock divider setting , same as FIMD */
	cfg = readl(base_reg + VIDCON0);
	cfg &= ~(VIDCON0_VIDOUT_MASK | VIDCON0_VCLK_MASK);
	cfg |= VIDCON0_VIDOUT_RGB;
	cfg |= VIDCON0_VCLK_NORMAL;

	s3c_ielcd_writel(IELCD_VIDCON0, cfg);
}

#ifdef CONFIG_FB_I80IF
static void s3c_ielcd_config_i80(void)
{
	unsigned int cfg;

	cfg = VIDCON0_DSI_EN;
	cfg |= VIDCON0_VIDOUT_I80_LDI0;
	cfg |= VIDCON0_CLKVALUP;
	cfg |= VIDCON0_CLKVAL_F(0);
	s3c_ielcd_writel(IELCD_VIDCON0, cfg);

	cfg = I80IFCON_CS_SETUP(1 - 1)
			| I80IFCON_WR_SETUP(0)
			| I80IFCON_WR_ACT(1)
			| I80IFCON_WR_HOLD(0)
			| I80IFCON_RS_POL(0);
	cfg |= I80IFCON_EN;
	s3c_ielcd_writel(IELCD_I80IFCONA0, cfg);
}

void s3c_ielcd_hw_trigger_set_of_start(void)
{
	unsigned int cfg;
	unsigned char count = 0;
	void __iomem *base_reg = fimd_reg;
	void __iomem *temp_base_reg = mdnie_reg;

	writel(0x77, temp_base_reg+0x4);
	writel(0x0, temp_base_reg+0x3fc);

	writel(0x80000008, base_reg + TRIGCON);

	cfg = readl(base_reg + VIDCON0);
	cfg |= (VIDCON0_ENVID | VIDCON0_ENVID_F);
	writel(cfg, base_reg + VIDCON0);

	cfg = s3c_ielcd_readl(IELCD_VIDCON0);
	cfg |= (VIDCON0_ENVID | VIDCON0_ENVID_F);
	s3c_ielcd_writel(IELCD_VIDCON0, cfg);

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg |= (SWTRGCMD_I80_RGB | TRGMODE_I80_RGB);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg |= (SWTRGCMD_W0BUF | TRGMODE_W0BUF);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg &= ~(SWTRGCMD_W0BUF | TRGMODE_W0BUF);
	cfg |= (SWTRGCMD_W0BUF | TRGMODE_W0BUF);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);

	do {
		cfg = s3c_ielcd_readl(IELCD_SHADOWCON);
		cfg |= IELCD_W0_SW_SHADOW_UPTRIG;
		s3c_ielcd_writel(IELCD_SHADOWCON, cfg);

		cfg = s3c_ielcd_readl(IELCD_VIDCON0);
		cfg |= IELCD_SW_SHADOW_UPTRIG;
		s3c_ielcd_writel(IELCD_VIDCON0, cfg);

		count++;
		if (count > 10000) {
			pr_err("ielcd start fail\n");
			BUG();
			break;
		}
	} while (s3c_ielcd_readl(IELCD_WINCON0) & (1 << 25));

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg &= ~(SWTRGCMD_I80_RGB | TRGMODE_I80_RGB);
	cfg |= (HWTRGMASK_I80_RGB | HWTRGEN_I80_RGB);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);
}

void s3c_ielcd_fimd_instance_off(void)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;

	cfg = readl(base_reg + VIDCON0);
	cfg &= ~(VIDCON0_ENVID | VIDCON0_ENVID_F);
	writel(cfg, base_reg + VIDCON0);

	cfg = s3c_ielcd_readl(IELCD_VIDCON0);
	cfg &= ~(VIDCON0_ENVID | VIDCON0_ENVID_F);
	s3c_ielcd_writel(IELCD_VIDCON0, cfg);
}

void s3c_ielcd_hw_trigger_set(void)
{
	unsigned int cfg;
	void __iomem *base_reg = fimd_reg;
	void __iomem *temp_base_reg = mdnie_reg;

	writel(0x77, temp_base_reg+0x4);
	writel(0x0, temp_base_reg+0x3fc);

	writel(0x80000008, base_reg + TRIGCON);

	cfg = readl(base_reg + VIDCON0);
	cfg |= (VIDCON0_ENVID | VIDCON0_ENVID_F);
	writel(cfg, base_reg + VIDCON0);

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg |= (SWTRGCMD_I80_RGB | TRGMODE_I80_RGB);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);

	cfg = s3c_ielcd_readl(IELCD_VIDCON0);
	cfg |= (VIDCON0_ENVID | VIDCON0_ENVID_F);
	s3c_ielcd_writel(IELCD_VIDCON0, cfg);

	cfg = s3c_ielcd_readl(IELCD_SHADOWCON);
	cfg |= IELCD_W0_SW_SHADOW_UPTRIG;
	s3c_ielcd_writel(IELCD_SHADOWCON, cfg);

	cfg = s3c_ielcd_readl(IELCD_VIDCON0);
	cfg |= IELCD_SW_SHADOW_UPTRIG;
	s3c_ielcd_writel(IELCD_VIDCON0, cfg);

	cfg = s3c_ielcd_readl(IELCD_TRIGCON);
	cfg &= ~(SWTRGCMD_I80_RGB | TRGMODE_I80_RGB);
	cfg |= (HWTRGMASK_I80_RGB | HWTRGEN_I80_RGB);
	s3c_ielcd_writel(IELCD_TRIGCON, cfg);
}

void ielcd_cmd_clear(void)
{
	s3c_ielcd_writel(LDI_CMDCON0, 0);
	s3c_ielcd_writel(LDI_CMDCON1, 0);
}

void ielcd_cmd_set(u32 idx, u32 rs, u32 value)
{
	unsigned int  cfg;

	if (idx > 11)
		return;

	/* FIMD LDI_CMDCON0, Set CMD0 to Normal Command */
	cfg = s3c_ielcd_readl(LDI_CMDCON0);
	cfg &= ~ LDI_CMDCON0_CMD_EN_MASK(idx);
	cfg |= LDI_CMDCON0_CMD_EN(idx);
	s3c_ielcd_writel(LDI_CMDCON0, cfg);

	/*LDI_CMDCON1: Set CMD0_RS High */
	cfg = s3c_ielcd_readl(LDI_CMDCON1);
	cfg &= ~ LDI_CMDCON1_CMD_RS(idx);
	if (rs)
		cfg |= LDI_CMDCON1_CMD_RS(idx);
	s3c_ielcd_writel(LDI_CMDCON1, cfg);

	cfg = s3c_ielcd_readl(LDI_CMD(idx));
	cfg &= ~ (0xffffff);
	cfg |= value;
	s3c_ielcd_writel(LDI_CMD(idx), cfg);
}

void ielcd_cmd_dump(void)
{
	printk("CMDCON0 = 0x%08X, CMDCON1 = 0x%08X\n",
		s3c_ielcd_readl(LDI_CMDCON0), s3c_ielcd_readl(LDI_CMDCON1));
	printk("CMD00 = 0x%08X\n \
		CMD01 = 0x%08X\n \
		CMD02 = 0x%08X\n \
		CMD03 = 0x%08X\n \
		CMD04 = 0x%08X\n \
		CMD05 = 0x%08X\n \
		CMD06 = 0x%08X\n \
		CMD07 = 0x%08X\n \
		CMD08 = 0x%08X\n \
		CMD09 = 0x%08X\n \
		CMD10 = 0x%08X\n \
		CMD11 = 0x%08X\n" \
		, s3c_ielcd_readl(LDI_CMD(0)), s3c_ielcd_readl(LDI_CMD(1)) \
		, s3c_ielcd_readl(LDI_CMD(2)), s3c_ielcd_readl(LDI_CMD(3)) \
		, s3c_ielcd_readl(LDI_CMD(4)), s3c_ielcd_readl(LDI_CMD(5)) \
		, s3c_ielcd_readl(LDI_CMD(6)), s3c_ielcd_readl(LDI_CMD(7)) \
		, s3c_ielcd_readl(LDI_CMD(8)), s3c_ielcd_readl(LDI_CMD(9)) \
		, s3c_ielcd_readl(LDI_CMD(10)),
		s3c_ielcd_readl(LDI_CMD(11)));
}


int ielcd_cmd_start_status(void)
{
	return (s3c_ielcd_readl(IELCD_I80IFCONB0) & I80IFCONB_NORMAL_CMD_ST);
}

void ielcd_cmd_trigger(void)
{
	unsigned int  cfg;
	void __iomem *base_reg = fimd_reg;

	cfg = s3c_ielcd_readl(IELCD_I80IFCONB0);
	cfg &= ~ I80IFCONB_NORMAL_CMD_ST;
	cfg |= I80IFCONB_NORMAL_CMD_ST;
	s3c_ielcd_writel(IELCD_I80IFCONB0, cfg);

	/* hw trigger on */
	cfg = readl(base_reg + TRIGCON);
	if (!(cfg & HWTRGMASK_I80_RGB)) {
		s3c_ielcd_hw_trigger_set();
		cfg &= ~(SWTRGCMD_I80_RGB | TRGMODE_I80_RGB);
		cfg |= HWTRIGEN_PER_RGB | HWTRGMASK_I80_RGB | HWTRGEN_I80_RGB;
		writel(cfg, base_reg + TRIGCON);
	}
}
#endif

int s3c_ielcd_setup(void)
{
	unsigned int cfg, xres, yres;
	void __iomem *base_reg = fimd_reg;

	cfg = readl(base_reg + VIDCON0);
	cfg |= VIDCON0_VCLK_FREE;
	writel(cfg, base_reg + VIDCON0);

#ifdef CONFIG_S5P_DP
	/* mdnie path enable */
	s3c_set_dp_mie_clkcon(S3C_DP_MIE_CLKCON_DP);
#else
	s3c_set_dp_mie_clkcon(S3C_DP_MIE_CLKCON_MDNIE);
#endif

	get_lcd_size(&xres, &yres);

	ielcd_logic_start();

	ielcd_set_polarity();
	ielcd_set_timing();
	ielcd_set_lcd_size(xres, yres);

	/* window0 setting , fixed */
	cfg = WINCONx_ENLOCAL | WINCON2_BPPMODE_24BPP_888 | WINCONx_ENWIN;
	s3c_ielcd_writel(IELCD_WINCON0, cfg);

	/* window0 position setting */
	s3c_ielcd_writel(IELCD_VIDOSD0A, 0);
	cfg = ((xres - 1) << 11) | ((yres - 1) << 0);
	s3c_ielcd_writel(IELCD_VIDOSD0B, 0);

	/* window0 osd size setting */
	s3c_ielcd_writel(IELCD_VIDOSD0C, xres * yres);

	/* window0 page size(bytes) */
	s3c_ielcd_writel(IELCD_VIDW00ADD2, xres * 4);

#ifdef CONFIG_FB_I80IF
	s3c_ielcd_config_i80();
#else
	s3c_ielcd_config_rgb();
#endif

	return 0;
}
