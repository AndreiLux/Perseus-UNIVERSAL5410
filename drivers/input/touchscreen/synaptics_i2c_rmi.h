/* Synaptics Register Mapped Interface (RMI4) I2C Physical Layer Driver.
 * Copyright (c) 2007-2012, Synaptics Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _SYNAPTICS_RMI4_H_
#define _SYNAPTICS_RMI4_H_

#define SYNAPTICS_RMI4_DRIVER_VERSION "DS5 1.0"
#include <linux/device.h>
#include <linux/i2c/synaptics_rmi.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <mach/sec_debug.h>
#endif

/*#define dev_dbg(dev, fmt, arg...) dev_info(dev, fmt, ##arg)*/

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define SYNAPTICS_DEVICE_NAME	"SYNAPTICS"

#define TSP_BOOSTER
#ifdef TSP_BOOSTER
#include <linux/pm_qos.h>
#define TOUCH_BOOSTER_OFF_TIME	500
#define TOUCH_BOOSTER_CHG_TIME	130

#define TOUCH_BOOSTER_CPU_FRQ_1	1600000
#define TOUCH_BOOSTER_MIF_FRQ_1	667000
#define TOUCH_BOOSTER_INT_FRQ_1	333000

#define TOUCH_BOOSTER_CPU_FRQ_2	650000
#define TOUCH_BOOSTER_MIF_FRQ_2	400000
#define TOUCH_BOOSTER_INT_FRQ_2	111000

#define TOUCH_BOOSTER_CPU_FRQ_3	650000
#define TOUCH_BOOSTER_MIF_FRQ_3	667000
#define TOUCH_BOOSTER_INT_FRQ_3	333000
#endif

#define SECURE_TSP
#define TYPE_B_PROTOCOL

#define PROXIMITY_TSP
#if defined(PROXIMITY_TSP)
#endif
#define HAND_GRIP_MODE
/* To support suface touch, firmware should support data
 * which is required related app ex) MT_ANGLE, MT_PALM ...
 * Synpatics IC report those data through F51's edge swipe
 * fucntionality.
 */
#define SURFACE_TOUCH
#if defined(SURFACE_TOUCH) && defined(PROXIMITY_TSP)
#define EDGE_SWIPE
#endif

#define	USE_OPEN_CLOSE
#ifdef USE_OPEN_CLOSE
/*#define USE_OPEN_DWORK*/
#endif

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

#define SYNAPTICS_HW_RESET_TIME	80
#define SYNAPTICS_REZERO_TIME 100
#define SYNAPTICS_POWER_MARGIN_TIME	150

#define SYNAPTICS_PRODUCT_ID_NONE	0
#define SYNAPTICS_PRODUCT_ID_S5000	1
#define SYNAPTICS_PRODUCT_ID_S5050	2

#define SYNAPTICS_MAX_FW_PATH	64

#define SYNAPTICS_DEFAULT_UMS_FW "/sdcard/synaptics.fw"

#define DATE_OF_FIRMWARE_BIN_OFFSET	0xEF00
#define IC_REVISION_BIN_OFFSET	0xEF02
#define FW_VERSION_BIN_OFFSET	0xEF03

#define DATE_OF_FIRMWARE_BIN_OFFSET_S5050	0x016D00
#define IC_REVISION_BIN_OFFSET_S5050		0x016D02
#define FW_VERSION_BIN_OFFSET_S5050			0x016D03

#define PDT_PROPS (0X00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x000A)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1a)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)

#define SYNAPTICS_RMI4_PRODUCT_INFO_SIZE 2
#define SYNAPTICS_RMI4_DATE_CODE_SIZE 3
#define SYNAPTICS_RMI4_PRODUCT_ID_SIZE 10
#define SYNAPTICS_RMI4_BUILD_ID_SIZE 3
#define SYNAPTICS_RMI4_PRODUCT_ID_LENGTH 10

#define MAX_NUMBER_OF_BUTTONS 4
#define MAX_INTR_REGISTERS 4
#define MAX_NUMBER_OF_FINGERS 10

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define DRIVER_NAME "synaptics_rmi4_i2c"
/*#define REPORT_2D_Z*/
#define REPORT_2D_W

#define F12_FINGERS_TO_SUPPORT 10

#define INVALID_X	65535
#define INVALID_Y	65535

#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#ifdef CONFIG_GLOVE_TOUCH
#define GLOVE_FEATURE_EN	0x20
#define GLOVE_CLEAR_DEFAULT	0x00
#endif

#ifdef PROXIMITY_TSP
#define USE_CUSTOM_REZERO
#define HOVER_Z_MAX (255)

#define F51_PROXIMITY_ENABLES_OFFSET (0)
#define F51_GPIP_EDGE_EXCLUSION_RX_OFFSET (47)

/* Define for proximity enables(F51_CUSTOM_CTRL00) */
#define FINGER_HOVER_EN (1 << 0)
#define AIR_SWIPE_EN (1 << 1)
#define LARGE_OBJ_EN (1 << 2)
#define HOVER_PINCH_EN (1 << 3)
#define LARGE_OBJ_WAKEUP_GESTURE_EN (1 << 4)
/* Reserved 5 */
#define ENABLE_HANDGRIP_RECOG (1 << 6)
#define SLEEP_PROXIMITY (1 << 7)

#define F51_GENERAL_CONTROL_OFFSET (1)
/* Define for General Control(F51_CUSTOM_CTRL01) */
#define JIG_TEST_EN	(1 << 0)
#define JIG_COMMAND_EN	(1 << 1)
#define NO_PROXIMITY_ON_TOUCH (1 << 2)
#define CONTINUOUS_LOAD_REPORT (1 << 3)
#define HOST_REZERO_COMMAND (1 << 4)
#define EDGE_SWIPE_EN (1 << 5)
#define HSYNC_STATUS (1 << 6)
#define HOST_ID (1 << 7)

/* Define for proximity Controls(F51_CUSTOM_QUERY04) */
#define HAS_FINGER_HOVER (1 << 0)
#define HAS_AIR_SWIPE (1 << 1)
#define HAS_LARGE_OBJ (1 << 2)
#define HAS_HOVER_PINCH (1 << 3)
#define HAS_EDGE_SWIPE (1 << 4)
#define HAS_SINGLE_FINGER (1 << 5)
#define HAS_GRIP_SUPPRESSION (1 << 6)
#define HAS_PALM_REJECTION (1 << 7)

#ifdef EDGE_SWIPE
#define EDGE_SWIPE_DATA_OFFSET	9

#define EDGE_SWIPE_WIDTH_MAX	255
#define EDGE_SWIPE_ANGLE_MIN	(-90)
#define EDGE_SWIPE_ANGLE_MAX	90
#define EDGE_SWIPE_PALM_MAX		1
#endif
#define F51_FINGER_TIMEOUT 50 /* ms */
#endif

#define POLLING_PERIOD 1 /* ms */
#define SYN_I2C_RETRY_TIMES 3
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS 200
#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12

/* Define for Device Control(F01_RMI_CTRL00) */
#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_ON (1 << 2)
#define CHARGER_CONNECTED (1 << 5)
#define REPORT_RATE (1 << 6)
#define CONFIGURED (1 << 7)

#define TSP_NEEDTO_REBOOT	(-ECONNREFUSED)
#define MAX_TSP_REBOOT		3

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	unsigned char query_base_addr;
	unsigned char cmd_base_addr;
	unsigned char ctrl_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count;
	unsigned char fn_number;
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for data registers
 * @ctrl_base: 16-bit base address for command registers
 * @data_base: 16-bit base address for control registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

/*
 * struct synaptics_rmi4_fn - function handler data structure
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @size_of_data_register_block: data register block size
 * @data1_offset: offset to data1 register from data base address
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char size_of_data_register_block;
	unsigned char data1_offset;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: rmi protocol major version number
 * @version_minor: rmi protocol minor version number
 * @manufacturer_id: manufacturer id
 * @product_props: product properties information
 * @product_info: product info array
 * @date_code: device manufacture date
 * @tester_id: tester id array
 * @serial_number: device serial number
 * @product_id_string: device product id
 * @support_fn_list: linked list for function handlers
 * @exp_fn_list: linked list for expanded function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	unsigned char date_code[SYNAPTICS_RMI4_DATE_CODE_SIZE];
	unsigned short tester_id;
	unsigned short serial_number;
	unsigned char product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char build_id[SYNAPTICS_RMI4_BUILD_ID_SIZE];
	struct list_head support_fn_list;
	struct list_head exp_fn_list;
};

/**
 * struct synaptics_finger - Represents fingers.
 * @ state: finger status.
 * @ mcount: moving counter for debug.
 */
struct synaptics_finger {
	unsigned char state;
	unsigned short mcount;
};

#ifdef PROXIMITY_TSP
#ifdef EDGE_SWIPE
struct synaptics_rmi4_surface {
	int width_major;
	int palm;
	int angle;
	int wx;
	int wy;
};
#endif
#ifdef HAND_GRIP_MODE
struct synaptics_rmi4_handgrip {
	unsigned char hand_grip_mode;
	unsigned char hand_grip;
	unsigned char old_hand_grip;
	unsigned char old_code;
};
#endif

struct synaptics_rmi4_f51_handle {
	unsigned char proximity_enables;
	unsigned char general_control;
	unsigned short proximity_enables_addr;
	unsigned short general_control_addr;
	unsigned char num_of_data_sources;
	unsigned char proximity_controls;
#ifdef EDGE_SWIPE
	struct synaptics_rmi4_surface surface_data;
#endif
#ifdef HAND_GRIP_MODE
	struct synaptics_rmi4_handgrip handgrip_data;
#endif
};
#endif

/*
 * struct synaptics_rmi4_data - rmi4 device instance data
 * @i2c_client: pointer to associated i2c client
 * @input_dev: pointer to associated input device
 * @board: constant pointer to platform data
 * @rmi4_mod_info: device information
 * @regulator: pointer to associated regulator
 * @rmi4_io_ctrl_mutex: mutex for i2c i/o control
 * @early_suspend: instance to support early suspend power management
 * @current_page: current page in sensor to acess
 * @button_0d_enabled: flag for 0d button support
 * @full_pm_cycle: flag for full power management cycle in early suspend stage
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f01
 * @f01_cmd_base_addr: command base address for f01
 * @f01_ctrl_base_addr: control base address for f01
 * @f01_data_base_addr: data base address for f01
 * @irq: attention interrupt
 * @sensor_max_x: sensor maximum x value
 * @sensor_max_y: sensor maximum y value
 * @irq_enabled: flag for indicating interrupt enable status
 * @touch_stopped: flag to stop interrupt thread processing
 * @fingers_on_2d: flag to indicate presence of fingers in 2d area
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @wait: wait queue for touch data polling in interrupt thread
 * @i2c_read: pointer to i2c read function
 * @i2c_write: pointer to i2c write function
 * @irq_enable: pointer to irq enable function
 */
struct synaptics_rmi4_data {
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	const struct synaptics_rmi4_platform_data *board;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct regulator *regulator;
	struct mutex rmi4_reset_mutex;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex rmi4_reflash_mutex;
	struct timer_list f51_finger_timer;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	unsigned char *firmware_image;

	struct completion init_done;
	struct synaptics_finger finger[MAX_NUMBER_OF_FINGERS];

	unsigned char current_page;
	unsigned char button_0d_enabled;
	unsigned char full_pm_cycle;
	unsigned char num_of_rx;
	unsigned char num_of_tx;
	unsigned int num_of_node;
	unsigned char num_of_fingers;
	unsigned char max_touch_width;
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned short num_of_intr_regs;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned short f12_ctrl11_addr;
	unsigned short f34_ctrl_base_addr;
	int irq;
	int sensor_max_x;
	int sensor_max_y;
	int touch_threshold;
	int gloved_sensitivity;
	bool flash_prog_mode;
	bool irq_enabled;
	bool touch_stopped;
	bool fingers_on_2d;
	bool f51_finger;
	bool sensor_sleep;
	bool stay_awake;
	bool staying_awake;
	bool tsp_probe;

	int ic_product_id;			/* product id of ic */
	int ic_revision_of_ic;		/* revision of reading from IC */
	int fw_version_of_ic;		/* firmware version of IC */
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_bin;		/* firmware version of binary */
	int fw_release_date_of_ic;	/* Config release data from IC */
	int panel_revision;			/* Octa panel revision */
	bool doing_reflash;
	int rebootcount;

#ifdef TSP_BOOSTER
	bool dvfs_lock_status;
	struct delayed_work work_dvfs_off;
	struct delayed_work work_dvfs_chg;
	struct mutex dvfs_lock;
	struct pm_qos_request tsp_cpu_qos;
	struct pm_qos_request tsp_mif_qos;
	struct pm_qos_request tsp_int_qos;
	unsigned char boost_level;
#endif

#ifdef CONFIG_GLOVE_TOUCH
	unsigned char glove_mode_feature;
	unsigned char glove_mode_enables;
	unsigned short glove_mode_enables_addr;
	bool fast_glove_state;
	bool touchkey_glove_mode_status;
#endif
	unsigned char ddi_type;

#ifdef SECURE_TSP
	int secure_mode_status;
#endif

	int hover_status_in_normal_mode;

#ifdef PROXIMITY_TSP
	struct synaptics_rmi4_f51_handle *f51;
#endif
#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif
	struct delayed_work rezero_work;

	struct mutex rmi4_device_mutex;

#ifdef SYNAPTICS_RMI_INFORM_CHARGER
	int ta_status;
	void (*register_cb)(struct synaptics_rmi_callbacks *);
	struct synaptics_rmi_callbacks callbacks;
#endif

	int (*i2c_read)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*i2c_write)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*stop_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*start_device)(struct synaptics_rmi4_data *rmi4_data);
};

#ifdef TSP_BOOSTER
enum BOOST_LEVEL {
	TSP_BOOSTER_DISABLE = 0,
	TSP_BOOSTER_LEVEL1,
	TSP_BOOSTER_LEVEL2,
	TSP_BOOSTER_LEVEL3,
	TSP_BOOSTER_LEVEL_MAX,
};

enum BOOST_MODE {
	TSP_BOOSTER_OFF = 0,
	TSP_BOOSTER_ON,
	TSP_BOOSTER_FORCE_OFF,
};
#endif

enum exp_fn {
	RMI_DEV = 0,
	RMI_F54,
	RMI_FW_UPDATER,
	RMI_LAST,
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	bool initialized;
	int (*func_init)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
	struct list_head link;
};

struct synaptics_rmi4_exp_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

int synaptics_rmi4_new_function(enum exp_fn fn_type,
		struct synaptics_rmi4_data *rmi4_data,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
				unsigned char intr_mask));

int rmidev_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmi4_f54_module_register(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f54_set_control(struct synaptics_rmi4_data *rmi4_data);
int rmi4_fw_update_module_register(struct synaptics_rmi4_data *rmi4_data);

int synaptics_fw_updater(unsigned char *fw_data);
int synaptics_rmi4_fw_update_on_probe(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_proximity_enables(struct synaptics_rmi4_data *rmi4_data, unsigned char enables);
int synaptics_rmi4_glove_mode_enables(struct synaptics_rmi4_data *rmi4_data);
int synaptics_proximity_no_sleep_set(struct synaptics_rmi4_data *rmi4_data, bool enables);
void synaptics_rmi4_release_all_finger(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f12_ctrl11_set(struct synaptics_rmi4_data *rmi4_data, unsigned char data);
int synaptics_rmi4_set_tsp_test_result_in_config(int value);
int synaptics_rmi4_read_tsp_test_result(void);
int synaptics_rmi4_force_calibration(void);
int synaptics_rmi4_f51_grip_edge_exclusion_rx(struct synaptics_rmi4_data *rmi4_data, bool enables);

extern struct class *sec_class;

static inline ssize_t synaptics_rmi4_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_warn(dev, "%s Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_rmi4_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	dev_warn(dev, "%s Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}
#endif
