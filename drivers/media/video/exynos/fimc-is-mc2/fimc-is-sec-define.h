#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <linux/cma.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>
#include <linux/bug.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/smc.h>
#include <linux/regulator/consumer.h>
#include <media/exynos_fimc_is.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include "crc32.h"

#include <linux/types.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>

#define FW_CORE_VER		0
#define FW_PIXEL_SIZE		1
#define FW_ISP_COMPANY		3
#define FW_SENSOR_MAKER		4
#define FW_PUB_YEAR		5
#define FW_PUB_MON		6
#define FW_PUB_NUM		7
#define FW_MODULE_COMPANY	9
#define FW_VERSION_INFO		10

#define FW_ISP_COMPANY_BROADCOMM	'B'
#define FW_ISP_COMPANY_TN		'C'
#define FW_ISP_COMPANY_FUJITSU		'F'
#define FW_ISP_COMPANY_INTEL		'I'
#define FW_ISP_COMPANY_LSI		'L'
#define FW_ISP_COMPANY_MARVELL		'M'
#define FW_ISP_COMPANY_QUALCOMM		'Q'
#define FW_ISP_COMPANY_RENESAS		'R'
#define FW_ISP_COMPANY_STE		'S'
#define FW_ISP_COMPANY_TI		'T'
#define FW_ISP_COMPANY_DMC		'D'

#define FW_SENSOR_MAKER_SF		'F'
#define FW_SENSOR_MAKER_SLSI		'L'
#define FW_SENSOR_MAKER_SONY		'S'

#define FW_MODULE_COMPANY_SEMCO		'S'
#define FW_MODULE_COMPANY_GUMI		'O'
#define FW_MODULE_COMPANY_CAMSYS	'C'
#define FW_MODULE_COMPANY_PATRON	'P'
#define FW_MODULE_COMPANY_MCNEX		'M'
#define FW_MODULE_COMPANY_LITEON	'L'
#define FW_MODULE_COMPANY_VIETNAM	'V'
#define FW_MODULE_COMPANY_SHARP		'J'
#define FW_MODULE_COMPANY_NAMUGA	'N'
#define FW_MODULE_COMPANY_POWER_LOGIX	'A'
#define FW_MODULE_COMPANY_DI		'D'

#define FW_3L2		"C13LL"
#define FW_IMX135	"C13LS"
#define FW_IMX134_OLD	"D08LS"
#define FW_IMX134	"E08LS"

#define SDCARD_FW
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_FW_3L2				"fimc_is_fw2_3L2.bin"
#define FIMC_IS_FW_IMX134			"fimc_is_fw2_IMX134.bin"
#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw2.bin"
#define FIMC_IS_IMX135_SETF			"setfile_imx135.bin"
#define FIMC_IS_IMX134_SETF			"setfile_imx134.bin"
#define FIMC_IS_3L2_SETF			"setfile_3L2.bin"
#define FIMC_IS_6B2_SETF			"setfile_6b2.bin"
#define FIMC_IS_FW_PATH				"/system/vendor/firmware/"
#define FIMC_IS_FW_DUMP_PATH			"/data/"

#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_VERSION_SIZE			42
#define FIMC_IS_SETFILE_VER_OFFSET		0x40
#define FIMC_IS_SETFILE_VER_SIZE		52

#define FIMC_IS_CAL_SDCARD			"/data/cal_data.bin"
/*#define FIMC_IS_MAX_CAL_SIZE			(20 * 1024)*/
#define FIMC_IS_MAX_FW_SIZE			(2048 * 1024)
#define FIMC_IS_CAL_START_ADDR			(0x013D0000)
#define FIMC_IS_CAL_RETRY_CNT			(2)
#define FIMC_IS_FW_RETRY_CNT			(2)

#define HEADER_CRC32_LEN (128 / 2)
#define OEM_CRC32_LEN (192 / 2)
#define AWB_CRC32_LEN (32 / 2)
#define SHADING_CRC32_LEN (2336 / 2)

struct fimc_is_from_info {
	u32		bin_start_addr;
	u32		bin_end_addr;
	u32		oem_start_addr;
	u32		oem_end_addr;
	u32		awb_start_addr;
	u32		awb_end_addr;
	u32		shading_start_addr;
	u32		shading_end_addr;
	u32		setfile_start_addr;
	u32		setfile_end_addr;

	char		header_ver[12];
	char		cal_map_ver[4];
	char		setfile_ver[7];
	char		oem_ver[12];
	char		awb_ver[12];
	char		shading_ver[12];
};

enum fimc_is_camera_device {
	CAMERA_SINGLE_REAR,
	CAMERA_SINGLE_FRONT,
	CAMERA_DUAL_REAR,
	CAMERA_DUAL_FRONT,
};

int fimc_is_sec_set_force_caldata_dump(bool fcd);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos);

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo);

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name);
int fimc_is_sec_get_camid(void);
int fimc_is_sec_set_camid(int id);
int fimc_is_sec_get_pixel_size(char *header_ver);

bool fimc_is_sec_is_fw_exist(char *fw_name);
int fimc_is_sec_readfw(void);
int fimc_is_sec_readcal(int isSysfsRead);

int fimc_is_sec_fw_sel(struct device *dev, struct exynos5_platform_fimc_is *pdata, char *fw_name, char *setf_name, int isSysfsRead);
int fimc_is_sec_fw_revision(char *fw_ver);
bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2);

bool fimc_is_sec_check_fw_crc32(char *buf);
bool fimc_is_sec_check_cal_crc32(char *buf);
void fimc_is_sec_make_crc32_table(u32 *table, u32 id);

int fimc_is_sec_gpio_enable(struct exynos5_platform_fimc_is *pdata, char *name, bool on);
int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver);
int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on);

int fimc_is_spi_reset(void *buf, u32 rx_addr, size_t size);
int fimc_is_spi_read(void *buf, u32 rx_addr, size_t size);
