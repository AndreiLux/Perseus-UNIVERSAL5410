#include "fimc-is-sec-define.h"

bool CRC32_FW_CHECK = true;
bool CRC32_CHECK = true;
bool CRC32_HEADER_CHECK = true;
u8 cal_map_version[4] = {0,};
u32 FIMC_IS_MAX_CAL_SIZE = (20 * 1024);
static bool is_caldata_read = false;
static bool force_caldata_dump = false;
static int cam_id;
bool is_dumped_fw_loading_needed = false;

struct class *camera_class;
struct device *camera_front_dev; /*sys/class/camera/front*/
struct device *camera_rear_dev; /*sys/class/camera/rear*/
static struct fimc_is_from_info sysfs_finfo;
static struct fimc_is_from_info sysfs_pinfo;

int fimc_is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		pr_info("forced caldata dump enabled!!\n");
	return 0;
}

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo;
	return 0;
}

int fimc_is_sec_set_camid(int id)
{
	cam_id = id;
	return 0;
}

int fimc_is_sec_get_camid()
{
	return cam_id;
}

bool fimc_is_sec_is_fw_exist(char *fw_name)
{
	bool ret = false;
	struct file *fp = NULL;
	char fw_path[100];
	mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	memset(fw_path, 0, sizeof(fw_path));
	snprintf(fw_path, sizeof(fw_path), "%s%s",
		FIMC_IS_FW_PATH, fw_name);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("failed to filp_open - %s", fw_path);
		ret = false;
	} else {
		pr_err("find file! - %s", fw_path);
		filp_close(fp, current->files);
		ret = true;
	}

	set_fs(old_fs);

	return ret;
}

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name)
{
	char buf[1];
	loff_t pos = 0;
	int pixelSize;

	read_data_from_file("/data/CameraID.txt", buf, 1, &pos);
	if (buf[0] == '0')
		cam_id = CAMERA_SINGLE_REAR;
	else if (buf[0] == '1')
		cam_id = CAMERA_SINGLE_FRONT;
	else if (buf[0] == '2')
		cam_id = CAMERA_DUAL_REAR;
	else if (buf[0] == '3')
		cam_id = CAMERA_DUAL_FRONT;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
		snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
		snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
	} else {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			if (fimc_is_sec_is_fw_exist(FIMC_IS_FW)) {
				snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
				snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			} else if (fimc_is_sec_is_fw_exist(FIMC_IS_FW_3L2)) {
				snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
				snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
			} else if (fimc_is_sec_is_fw_exist(FIMC_IS_FW_IMX134)) {
				snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
				snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
			} else {
				snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
				snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			}
		}
	}

	if (cam_id == CAMERA_SINGLE_FRONT ||
		cam_id == CAMERA_DUAL_FRONT) {
		snprintf(setf_name, sizeof(FIMC_IS_6B2_SETF), "%s", FIMC_IS_6B2_SETF);
	}
	return 0;
}

int fimc_is_sec_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_PUB_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_PUB_MON] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_PUB_NUM] - 48) * 10;
	revision = revision + (int)fw_ver[FW_PUB_NUM + 1] - 48;

	return revision;
}

bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER]
		|| fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE]
		|| fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1]
		|| fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY]
		|| fw_ver1[FW_SENSOR_MAKER] != fw_ver2[FW_SENSOR_MAKER]) {
		return false;
	}

	return true;
}

bool fimc_is_sec_check_cal_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	printk(KERN_INFO "+++ %s\n", __func__);


	CRC32_CHECK = true;

	/* Header data */
	check_base = 0;
	checksum = 0;
	checksum_base = ((check_base & 0xfffff000) + 0xffc) / 4;

	checksum = getCRC((u16 *)&buf32[check_base],
						HEADER_CRC32_LEN, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		CRC32_CHECK = false;
		CRC32_HEADER_CHECK = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: Header checksum address has error(0x%08X)", checksum_base * 4);
	} else {
		CRC32_HEADER_CHECK = true;
	}

	/* OEM */
	check_base = sysfs_finfo.oem_start_addr / 4;
	checksum = 0;
	check_length = (sysfs_finfo.oem_end_addr - sysfs_finfo.oem_start_addr + 1) / 2;
	checksum_base = ((sysfs_finfo.oem_end_addr & 0xfffff000) + 0xffc) / 4;

	checksum = getCRC((u16 *)&buf32[check_base],
					check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		CRC32_CHECK = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: OEM checksum address has error(0x%08X)", checksum_base * 4);
	}

	/* AWB */
	check_base = sysfs_finfo.awb_start_addr / 4;
	checksum = 0;
	check_length = (sysfs_finfo.awb_end_addr - sysfs_finfo.awb_start_addr + 1) / 2;
	checksum_base = ((sysfs_finfo.awb_end_addr & 0xfffff000) + 0xffc) / 4;

	checksum = getCRC((u16 *)&buf32[check_base],
					check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		CRC32_CHECK = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: AWB checksum address has error(0x%08X)", checksum_base * 4);
	}

	/* Shading */
	check_base = sysfs_finfo.shading_start_addr / 4;
	checksum = 0;
	check_length = (sysfs_finfo.shading_end_addr - sysfs_finfo.shading_start_addr + 1) / 2;
	checksum_base = ((sysfs_finfo.shading_end_addr & 0xfffff000) + 0xffc) / 4;

	checksum = getCRC((u16 *)&buf32[check_base],
					check_length, NULL, NULL);
	if (checksum_base < 0x80000 && checksum_base > 0 && checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
		CRC32_CHECK = false;
	} else if (checksum_base > 0x80000 || checksum_base < 0) {
		err("Camera: Shading checksum address has error(0x%08X)", checksum_base * 4);
	}

	return CRC32_CHECK;
}

bool fimc_is_sec_check_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;

	buf32 = (u32 *)buf;

	pr_info("Camera: Start checking CRC32 FW\n\n");

	CRC32_FW_CHECK = true;

	checksum = 0;

	checksum = getCRC((u16 *)&buf32[0],
						(sysfs_finfo.setfile_end_addr - sysfs_finfo.bin_start_addr + 1)/2, NULL, NULL);
	if (checksum != buf32[(0x1FFFFC - 0x5000)/4]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[(0x1FFFFC - 0x5000)/4]);
		CRC32_FW_CHECK = false;
	}

	pr_info("Camera: End checking CRC32 FW\n\n");

	return CRC32_FW_CHECK;
}

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664);
	if (fd < 0) {
		err("open file error!!\n");
		return -EINVAL;
	}
	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		vfs_fsync(fp, 0);
		fput(fp);
	}
	sys_close(fd);
	set_fs(old_fs);

	return count;
}

ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(name, O_RDONLY, 0664);
	if (fd < 0) {
		err("open file error!!\n");
		set_fs(old_fs);
		return -EINVAL;
	}
	fp = fget(fd);
	if (fp) {
		tx = vfs_read(fp, buf, count, pos);
		fput(fp);
	}
	sys_close(fd);
	set_fs(old_fs);

	return count;
}

void fimc_is_sec_make_crc32_table(u32 *table, u32 id)
{
	u32 i, j, k;

	for (i = 0; i < 256; ++i) {
		k = i;
		for (j = 0; j < 8; ++j) {
			if (k & 1)
				k = (k >> 1) ^ id;
			else
				k >>= 1;
		}
		table[i] = k;
	}
}

#if 0 /* unused */
static void fimc_is_read_sensor_version(void)
{
	int ret;
	char buf[0x50];

	memset(buf, 0x0, 0x50);

	printk(KERN_INFO "+++ %s\n", __func__);

	ret = fimc_is_spi_read(buf, 0x0, 0x50);

	printk(KERN_INFO "--- %s\n", __func__);

	if (ret) {
		err("fimc_is_spi_read - can't read sensor version\n");
	}

	err("Manufacturer ID(0x40): 0x%02x\n", buf[0x40]);
	err("Pixel Number(0x41): 0x%02x\n", buf[0x41]);
}

static void fimc_is_read_sensor_version2(void)
{
	char *buf;
	char *cal_data;
	u32 cur;
	u32 count = SETFILE_SIZE/READ_SIZE;
	u32 extra = SETFILE_SIZE%READ_SIZE;

	printk(KERN_ERR "%s\n\n\n\n", __func__);


	buf = (char *)kmalloc(READ_SIZE, GFP_KERNEL);
	cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

	memset(buf, 0x0, READ_SIZE);
	memset(cal_data, 0x0, SETFILE_SIZE);


	for (cur = 0; cur < SETFILE_SIZE; cur += READ_SIZE) {
		fimc_is_spi_read(buf, cur, READ_SIZE);
		memcpy(cal_data+cur, buf, READ_SIZE);
		memset(buf, 0x0, READ_SIZE);
	}

	if (extra != 0) {
		fimc_is_spi_read(buf, cur, extra);
		memcpy(cal_data+cur, buf, extra);
		memset(buf, 0x0, extra);
	}

	pr_info("Manufacturer ID(0x40): 0x%02x\n", cal_data[0x40]);
	pr_info("Pixel Number(0x41): 0x%02x\n", cal_data[0x41]);


	pr_info("Manufacturer ID(0x4FE7): 0x%02x\n", cal_data[0x4FE7]);
	pr_info("Pixel Number(0x4FE8): 0x%02x\n", cal_data[0x4FE8]);
	pr_info("Manufacturer ID(0x4FE9): 0x%02x\n", cal_data[0x4FE9]);
	pr_info("Pixel Number(0x4FEA): 0x%02x\n", cal_data[0x4FEA]);

	kfree(buf);
	kfree(cal_data);

}

static int fimc_is_get_cal_data(void)
{
	int err = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long ret = 0;
	u8 mem0 = 0, mem1 = 0;
	u32 CRC = 0;
	u32 DataCRC = 0;
	u32 IntOriginalCRC = 0;
	u32 crc_index = 0;
	int retryCnt = 2;
	u32 header_crc32 =	0x1000;
	u32 oem_crc32 =		0x2000;
	u32 awb_crc32 =		0x3000;
	u32 shading_crc32 = 0x6000;
	u32 shading_header = 0x22C0;

	char *cal_data;

	CRC32_CHECK = true;
	printk(KERN_INFO "%s\n\n\n\n", __func__);
	printk(KERN_INFO "+++ %s\n", __func__);

	fimc_is_spi_read(cal_map_version, 0x60, 0x4);
	printk(KERN_INFO "cal_map_version = %.4s\n", cal_map_version);

	if (cal_map_version[3] == '5') {
		shading_crc32 = 0x6000;
		shading_header = 0x22C0;
	} else if (cal_map_version[3] == '6') {
		shading_crc32 = 0x4000;
		shading_header = 0x920;
	} else {
		shading_crc32 = 0x5000;
		shading_header = 0x22C0;
	}

	/* Make CRC Table */
	fimc_is_sec_make_crc32_table((u32 *)&crc_table, 0xEDB88320);


	retry:
		cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

		memset(cal_data, 0x0, SETFILE_SIZE);

		mem0 = 0, mem1 = 0;
		CRC = 0;
		DataCRC = 0;
		IntOriginalCRC = 0;
		crc_index = 0;

		fimc_is_spi_read(cal_data, 0, SETFILE_SIZE);

		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x80)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made HEADER CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[header_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[header_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[header_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[header_crc32-1]&0x00ff);
		printk(KERN_INFO "Original HEADER CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			CRC32_CHECK = false;

		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0xC0)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x1000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x1000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made OEM CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[oem_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[oem_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[oem_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[oem_crc32-1]&0x00ff);
		printk(KERN_INFO "Original OEM CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			CRC32_CHECK = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x20)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x2000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x2000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made AWB CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[awb_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[awb_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[awb_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[awb_crc32-1]&0x00ff);
		printk(KERN_INFO "Original AWB CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			CRC32_CHECK = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (shading_header)/2; crc_index++) {

			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x3000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x3000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made SHADING CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[shading_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[shading_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[shading_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[shading_crc32-1]&0x00ff);
		printk(KERN_INFO "Original SHADING CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			CRC32_CHECK = false;


		old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (CRC32_CHECK == true) {
			printk(KERN_INFO "make cal_data.bin~~~~ \n");
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		} else {
			if (retryCnt > 0) {
				set_fs(old_fs);
				retryCnt--;
				goto retry;
			}
		}

/*
		{
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		}
*/

		if (fp != NULL)
			filp_close(fp, current->files);

	out:
		set_fs(old_fs);
		kfree(cal_data);
		return err;

}

#endif

int fimc_is_sec_readcal(int isSysfsRead)
{
	int ret = 0;
	char *buf = NULL;
	loff_t pos = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	char spi_buf[0x50];

	/* reset spi */
	memset(spi_buf, 0x0, 0x50);
	ret = fimc_is_spi_reset(spi_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	fimc_is_spi_read(cal_map_version, 0x60, 0x4);
	printk(KERN_INFO "Camera: Cal map_version = %c%c%c%c\n", cal_map_version[0],
			cal_map_version[1], cal_map_version[2], cal_map_version[3]);

	if (cal_map_version[3] == '5') {
		FIMC_IS_MAX_CAL_SIZE = 24*1024;
		pr_info("FIMC_IS_MAX_CAL_SIZE = 24*1024\n");
	} else {
		FIMC_IS_MAX_CAL_SIZE = 20*1024;
		pr_info("FIMC_IS_MAX_CAL_SIZE = 20*1024\n");
	}

	buf = kmalloc(FIMC_IS_MAX_CAL_SIZE, GFP_KERNEL);
	if (!buf) {
		err("kmalloc fail");
		ret = -ENOMEM;
		goto exit;
	}

crc_retry:

	/* read cal data */
	pr_info("Camera: SPI read cal data\n\n");
	ret = fimc_is_spi_read(buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&buf[0]);
	sysfs_finfo.bin_end_addr = *((u32 *)&buf[4]);
	pr_info("Binary start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.oem_start_addr = *((u32 *)&buf[8]);
	sysfs_finfo.oem_end_addr = *((u32 *)&buf[12]);
	pr_info("OEM start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	sysfs_finfo.awb_start_addr = *((u32 *)&buf[16]);
	sysfs_finfo.awb_end_addr = *((u32 *)&buf[20]);
	pr_info("AWB start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&buf[24]);
	sysfs_finfo.shading_end_addr = *((u32 *)&buf[28]);
	pr_info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&buf[32]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&buf[36]);
	if (sysfs_finfo.setfile_start_addr == 0xffffffff) {
		sysfs_finfo.setfile_start_addr = *((u32 *)&buf[40]);
		sysfs_finfo.setfile_end_addr = *((u32 *)&buf[44]);
	}

	if (sysfs_finfo.setfile_end_addr < 0x5000 || sysfs_finfo.setfile_end_addr > 0x1fffff) {
		err("setfile end_addr has error!!  0x%08x", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}

	pr_info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &buf[64], 11);
	sysfs_finfo.header_ver[11] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &buf[96], 4);
	memcpy(sysfs_finfo.setfile_ver, &buf[100], 6);
	sysfs_finfo.setfile_ver[6] = '\0';
	memcpy(sysfs_finfo.oem_ver, &buf[8160], 11);
	sysfs_finfo.oem_ver[11] = '\0';
	memcpy(sysfs_finfo.awb_ver, &buf[12256], 11);
	sysfs_finfo.awb_ver[11] = '\0';
	memcpy(sysfs_finfo.shading_ver, &buf[16352], 11);
	sysfs_finfo.shading_ver[11] = '\0';


	/* debug info dump */
#if defined(FROM_DEBUG)
	pr_info("++++ FROM data info\n");
	pr_info("1. Header info\n");
	pr_info("Module info : %s\n", sysfs_finfo.header_ver);
	pr_info(" ID : %c\n", sysfs_finfo.header_ver[0]);
	pr_info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[1],
							sysfs_finfo.header_ver[2]);
	pr_info(" ISP ID : %c\n", sysfs_finfo.header_ver[3]);
	pr_info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[5]);
	pr_info(" Module ver : %c\n", sysfs_finfo.header_ver[6]);
	pr_info(" Year : %c\n", sysfs_finfo.header_ver[7]);
	pr_info(" Month : %c\n", sysfs_finfo.header_ver[8]);
	pr_info(" Release num : %c%c\n", sysfs_finfo.header_ver[9],
							sysfs_finfo.header_ver[10]);
	pr_info("Cal data map ver : %s\0\n", sysfs_finfo.cal_map_ver);
	pr_info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	pr_info("2. OEM info\n");
	pr_info("Module info : %s\n", sysfs_finfo.oem_ver);
	pr_info("3. AWB info\n");
	pr_info("Module info : %s\n", sysfs_finfo.awb_ver);
	pr_info("4. Shading info\n");
	pr_info("Module info : %s\n", sysfs_finfo.shading_ver);
	pr_info("---- FROM data info\n");
#endif

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (!isSysfsRead) {
		if (write_data_to_file(FIMC_IS_CAL_SDCARD, buf,
					FIMC_IS_MAX_CAL_SIZE, &pos) < 0) {
			ret = -EIO;
			goto exit;
		}
		pr_info("Camera: Cal Data was dumped successfully\n");
	} else {
		pr_info("Camera: sysfs read. Cal Data will not dump\n");
	}

	kfree(buf);
	return 0;
exit:
	if (buf)
		kfree(buf);
	return ret;
}

int fimc_is_sec_readfw(void)
{
	int ret = 0;
	char *buf = NULL;
	loff_t pos = 0;
	char fw_path[100];
	char setfile_path[100];
	char setf_name[50];
	int retry = FIMC_IS_FW_RETRY_CNT;
	int pixelSize;

	pr_info("Camera: FW, Setfile need to be dumped\n");

	buf = vmalloc(FIMC_IS_MAX_FW_SIZE);
	if (!buf) {
		err("vmalloc fail");
		ret = -ENOMEM;
		goto exit;
	}

	crc_retry:

	/* read fw data */
	pr_info("Camera: Start SPI read fw data\n\n");
	ret = fimc_is_spi_read(buf, 0x5000, FIMC_IS_MAX_FW_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	pr_info("Camera: End SPI read fw data\n\n");

/*
	{
	for (tp = 0; tp < FIMC_IS_MAX_FW_SIZE; tp++) {
		printk("%x", buf[tp]);
		if ((tp % 8) == 0)
			printk(" ");
		if ((tp % 128) == 0)
			printk("\n");
	}

	}
*/
	/* CRC check */
	if (!fimc_is_sec_check_fw_crc32(buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW);
	if (write_data_to_file(fw_path, buf,
					sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	pr_info("Camera: FW Data has dumped successfully\n");

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
		snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
		snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
	} else {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			if (fimc_is_sec_is_fw_exist(FIMC_IS_FW)) {
				snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			} else if (fimc_is_sec_is_fw_exist(FIMC_IS_FW_3L2)) {
				snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
			} else if (fimc_is_sec_is_fw_exist(FIMC_IS_FW_IMX134)) {
				snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
			} else {
				snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			}
		}
	}

	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, setf_name);
	pos = 0;

	if (write_data_to_file(setfile_path,
			buf+(sysfs_finfo.setfile_start_addr - sysfs_finfo.bin_start_addr),
			sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	pr_info("Camera: Setfile has dumped successfully\n");
	pr_info("Camera: FW, Setfile were dumped successfully\n");

exit:
	if (buf)
		vfree(buf);
	return ret;
}

int fimc_is_sec_gpio_enable(struct exynos5_platform_fimc_is *pdata, char *name, bool on)
{
	struct gpio_set *gpio;
	int ret = 0;
	int i = 0;

	for (i = 0; i < FIMC_IS_MAX_GPIO_NUM; i++) {
			gpio = &pdata->gpio_info->cfg[i];
			if (strcmp(gpio->name, name) == 0)
				break;
			else
				continue;
	}

	if (i == FIMC_IS_MAX_GPIO_NUM) {
		pr_err("GPIO %s is not found!!\n", name);
		ret = -EINVAL;
		goto exit;
	}

	ret = gpio_request(gpio->pin, gpio->name);
	if (ret) {
		pr_err("Request GPIO error(%s)\n", gpio->name);
		goto exit;
	}

	if (on) {
		switch (gpio->act) {
		case GPIO_PULL_NONE:
			s3c_gpio_cfgpin(gpio->pin, gpio->value);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			break;
		case GPIO_OUTPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_INPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_RESET:
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio->pin, 0);
			gpio_direction_output(gpio->pin, 1);
			break;
		default:
			pr_err("unknown act for gpio\n");
			break;
		}
	} else {
		s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_DOWN);
	}

	gpio_free(gpio->pin);

exit:
	return ret;
}

int fimc_is_sec_get_pixel_size(char *header_ver)
{
	int pixelsize = 0;

	pixelsize += (int) (header_ver[FW_PIXEL_SIZE] - 0x30) * 10;
	pixelsize += (int) (header_ver[FW_PIXEL_SIZE + 1] - 0x30);

	return pixelsize;
}

int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver)
{
	struct regulator *regulator = NULL;
	int ret = 0;
	int minV, maxV;
	int pixelSize = 0;

	regulator = regulator_get(dev, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator)) {
		pr_err("%s : regulator_get fail\n",
			__func__);
		return -EINVAL;
	}
	pixelSize = fimc_is_sec_get_pixel_size(header_ver);

	if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SONY) {
		if (pixelSize == 13) {
			minV = 1050000;
			maxV = 1050000;
		} else if (pixelSize == 8) {
			minV = 1100000;
			maxV = 1100000;
		} else {
			minV = 1050000;
			maxV = 1050000;
		}
	} else if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		minV = 1200000;
		maxV = 1200000;
	} else {
		minV = 1050000;
		maxV = 1050000;
	}

	ret = regulator_set_voltage(regulator, minV, maxV);

	if (ret >= 0)
		pr_info("%s : set_core_voltage %d, %d successfully\n",
				__func__, minV, maxV);
	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on)
{
	struct regulator *regulator = NULL;
	int ret = 0;

	if (on) {
		regulator = regulator_get(dev, name);
		if (IS_ERR(regulator)) {
			pr_err("%s : regulator_get(%s) fail\n",
				__func__, name);
			ret = -EINVAL;
			goto exit;
		} else if (!regulator_is_enabled(regulator)) {
			ret = regulator_enable(regulator);
			if (ret) {
				pr_err("%s : regulator_enable(%s) fail\n",
					__func__, name);
				regulator_put(regulator);
				ret = -EINVAL;
				goto exit;
			}
		}
		regulator_put(regulator);
	} else {
		regulator = regulator_get(dev, name);
		if (IS_ERR(regulator)) {
			pr_err("%s : regulator_get(%s) fail\n",
				__func__, name);
			ret = -EINVAL;
			goto exit;
		} else if (regulator_is_enabled(regulator)) {
			ret = regulator_disable(regulator);
			if (ret) {
				pr_err("%s : regulator_disable(%s) fail\n",
					__func__, name);
				regulator_put(regulator);
				ret = -EINVAL;
				goto exit;
			}
		}
		regulator_put(regulator);
	}
exit:
	return ret;
}

int fimc_is_sec_fw_sel(struct device *dev, struct exynos5_platform_fimc_is *pdata, char *fw_name, char *setf_name, int isSysfsRead)
{
	int ret = 0;
	char fw_path[100];
	char dump_fw_path[100];
	char dump_fw_version[12] = {0, };
	char phone_fw_version[12] = {0, };
	int from_fw_revision = 0;
	int dump_fw_revision = 0;
	int phone_fw_revision = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *fw_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = true;
	bool is_ldo_enabled = false;
	int pixelSize = 0;

	if ((!is_caldata_read || force_caldata_dump) &&
			(cam_id == CAMERA_SINGLE_REAR || cam_id == CAMERA_DUAL_FRONT)) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			pr_info("forced caldata dump!!\n");

		if (cam_id == CAMERA_DUAL_FRONT) {
			ret = fimc_is_sec_ldo_enable(dev, "cam_isp_sensor_io_1.8v", true);
			if (ret < 0) {
				err("fimc_is_sec_ldo_enable(true) failed!\n");
				goto exit;
			}

			ret = fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_SCLK", true);
			if (ret < 0) {
				err("fimc_is_sec_gpio_enable(GPIO_CAM_SPI0_SCLK, true) failed!\n");
				is_ldo_enabled = true;
				goto exit;
			}

			ret = fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_SSN", true);
			if (ret < 0) {
				err("fimc_is_sec_gpio_enable(GPIO_CAM_SPI0_SSN, true) failed!\n");
				is_ldo_enabled = true;
				goto exit;
			}

			ret = fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_MISP", true);
			if (ret < 0) {
				err("fimc_is_sec_gpio_enable(GPIO_CAM_SPI0_MISP, true) failed!\n");
				is_ldo_enabled = true;
				goto exit;
			}

			ret = fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_MOSI", true);
			if (ret < 0) {
				err("fimc_is_sec_gpio_enable(GPIO_CAM_SPI0_MOSI, true) failed!\n");
				is_ldo_enabled = true;
				goto exit;
			}

			is_ldo_enabled = true;
		}

		pr_info("read cal data from FROM\n");
		if ((fimc_is_sec_readcal(isSysfsRead) != -EIO) &&
				CRC32_HEADER_CHECK) {
			if (!isSysfsRead)
				is_caldata_read = true;
		}

		/*select AF actuator*/
		if (!CRC32_HEADER_CHECK && sysfs_finfo.bin_start_addr != 0) {
			pr_info("Camera : CRC32 error for all section.\n");
			ret = -EIO;
			goto exit;
		}

		ret = fimc_is_sec_core_voltage_select(dev, sysfs_finfo.header_ver);
		if (ret < 0) {
			err("failed to fimc_is_sec_core_voltage_select (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134_OLD)) {
			/* hack :  support old version module */
			pr_info("Camera : change f-rom version from %s, to %s",
					sysfs_finfo.header_ver, FW_IMX134);
			memcpy(sysfs_finfo.header_ver, FW_IMX134, 5);
		}

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
			snprintf(fw_path, sizeof(fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_3L2);
			snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
			snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
			snprintf(fw_path, sizeof(fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW);
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
			snprintf(fw_path, sizeof(fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_IMX134);
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
			if (pixelSize == 13) {
				snprintf(fw_path, sizeof(fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW);
				snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
				snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			} else if (pixelSize == 8) {
				snprintf(fw_path, sizeof(fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_IMX134);
				snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
				snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
			} else {
				if (fimc_is_sec_is_fw_exist(FIMC_IS_FW)) {
					snprintf(fw_path, sizeof(fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW);
					snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
					snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
				} else if (fimc_is_sec_is_fw_exist(FIMC_IS_FW_3L2)) {
					snprintf(fw_path, sizeof(fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW_3L2);
					snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
					snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
				} else if (fimc_is_sec_is_fw_exist(FIMC_IS_FW_IMX134)) {
					snprintf(fw_path, sizeof(fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW_IMX134);
					snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
					snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
				} else {
					snprintf(fw_path, sizeof(fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW);
					snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
					snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
				}
			}
		}
		if (cam_id != CAMERA_SINGLE_REAR) {
			snprintf(setf_name, sizeof(FIMC_IS_6B2_SETF), "%s", FIMC_IS_6B2_SETF);
		}

		snprintf(dump_fw_path, sizeof(dump_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW);


		pr_info("Camera: f-rom fw version: %s\n", sysfs_finfo.header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(dump_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_info("Camera: There is no dumped firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_info("start, file path %s, size %ld Bytes\n",
			dump_fw_path, fsize);
		fw_buf = vmalloc(fsize);
		if (!fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
			goto read_phone_fw;
		}
		nread = vfs_read(fp, (char __user *)fw_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			pr_err("failed to read firmware file, %ld Bytes\n", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_fw_version, fw_buf+nread-11, 11);
		pr_info("Camera: dumped fw version: %s\n", dump_fw_version);

read_phone_fw:
		if (fw_buf) {
			vfree(fw_buf);
			fw_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_err("Camera: Failed open phone firmware\n");
			ret = -EIO;
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_info("start, file path %s, size %ld Bytes\n",
			fw_path, fsize);
		fw_buf = vmalloc(fsize);
		if (!fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		nread = vfs_read(fp, (char __user *)fw_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			pr_err("failed to read firmware file, %ld Bytes\n", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_fw_version, fw_buf + nread - 11, 11);
		strncpy(sysfs_pinfo.header_ver, fw_buf + nread - 11, 11);
		pr_info("Camera: phone fw version: %s\n", phone_fw_version);
read_phone_fw_exit:
		if (fw_buf) {
			vfree(fw_buf);
			fw_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		from_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.header_ver);
		phone_fw_revision = fimc_is_sec_fw_revision(phone_fw_version);
		if (is_dump_existed)
			dump_fw_revision = fimc_is_sec_fw_revision(dump_fw_version);

		if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version)) ||
				(from_fw_revision > phone_fw_revision)) {
			is_dumped_fw_loading_needed = true;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver,
							dump_fw_version)) {
					is_dump_needed = true;
				} else if (from_fw_revision > dump_fw_revision) {
					is_dump_needed = true;
				} else {
					is_dump_needed = false;
				}
			} else {
				is_dump_needed = true;
			}
		} else {
			is_dump_needed = false;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(phone_fw_version,
							dump_fw_version)) {
					is_dumped_fw_loading_needed = false;
				} else if (phone_fw_revision > dump_fw_revision) {
					is_dumped_fw_loading_needed = false;
				} else {
					is_dumped_fw_loading_needed = true;
				}
			} else {
				is_dumped_fw_loading_needed = false;
			}
		}
#if 0
		if (sysfs_finfo.header_ver[0] == 'O') {
			/* hack: gumi module using phone fw */
			is_dumped_fw_loading_needed = false;
			is_dump_needed = false;
		} else
#endif
		if (sysfs_finfo.header_ver[FW_ISP_COMPANY] != FW_ISP_COMPANY_LSI) {
			ret = -EINVAL;
			goto exit;
		}

		if (is_dump_needed) {
			ret = fimc_is_sec_readfw();
			if (ret < 0) {
				if (!CRC32_FW_CHECK) {
					is_dumped_fw_loading_needed = false;
					ret = 0;
				} else
					goto exit;
			}
		}
	}

exit:

	if (is_ldo_enabled) {
		fimc_is_sec_ldo_enable(dev, "cam_isp_sensor_io_1.8v", false);
		fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_SCLK", false);
		fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_SSN", false);
		fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_MISP", false);
		fimc_is_sec_gpio_enable(pdata, "GPIO_CAM_SPI0_MOSI", false);
	}

	return ret;
}
