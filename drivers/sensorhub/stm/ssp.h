/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_PRJ_H__
#define __SSP_PRJ_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/wakelock.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/ssp_platformdata.h>
#ifdef CONFIG_SENSORS_SSP_STM
#include <linux/spi/spi.h>
#endif
#ifdef CONFIG_SENSORS_SSP_SENSORHUB
#include "ssp_sensorhub.h"
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#undef CONFIG_HAS_EARLYSUSPEND
#endif

#define SSP_DBG		1

#define SUCCESS		1
#define FAIL		0
#define ERROR		-1

#define FACTORY_DATA_MAX	99

#if SSP_DBG
#define SSP_FUNC_DBG 1
#define SSP_DATA_DBG 0

/* ssp mcu device ID */
#define DEVICE_ID			0x55


#define ssp_dbg(dev, format, ...) do { \
	printk(KERN_INFO dev, format, ##__VA_ARGS__); \
	} while (0)
#else
#define ssp_dbg(dev, format, ...)
#endif

#if SSP_FUNC_DBG
#define func_dbg() do { \
	printk(KERN_INFO "[SSP]: %s\n", __func__); \
	} while (0)
#else
#define func_dbg()
#endif

#if SSP_DATA_DBG
#define data_dbg(dev, format, ...) do { \
	printk(KERN_INFO dev, format, ##__VA_ARGS__); \
	} while (0)
#else
#define data_dbg(dev, format, ...)
#endif

#define SSP_SW_RESET_TIME		3000
#define DEFUALT_POLLING_DELAY	(200 * NSEC_PER_MSEC)
#define PROX_AVG_READ_NUM		80
#define DEFAULT_RETRIES		3
#define DATA_PACKET_SIZE		960

/* SSP Binary Type */
enum {
	KERNEL_BINARY = 0,
	KERNEL_CRASHED_BINARY,
	UMS_BINARY,
};

/* Sensor Sampling Time Define */
enum {
	SENSOR_NS_DELAY_FASTEST = 10000000,	/* 10msec */
	SENSOR_NS_DELAY_GAME = 20000000,	/* 20msec */
	SENSOR_NS_DELAY_UI = 66700000,	/* 66.7msec */
	SENSOR_NS_DELAY_NORMAL = 200000000,	/* 200msec */
};

enum {
	SENSOR_MS_DELAY_FASTEST = 10,	/* 10msec */
	SENSOR_MS_DELAY_GAME = 20,		/* 20msec */
	SENSOR_MS_DELAY_UI = 66,		/* 66.7msec */
	SENSOR_MS_DELAY_NORMAL = 200,	/* 200msec */
};

enum {
	SENSOR_CMD_DELAY_FASTEST = 0,	/* 10msec */
	SENSOR_CMD_DELAY_GAME,		/* 20msec */
	SENSOR_CMD_DELAY_UI,		/* 66.7msec */
	SENSOR_CMD_DELAY_NORMAL,		/* 200msec */
};

/*
 * SENSOR_DELAY_SET_STATE
 * Check delay set to avoid sending ADD instruction twice
 */
enum {
	INITIALIZATION_STATE = 0,
	NO_SENSOR_STATE,
	ADD_SENSOR_STATE,
	RUNNING_SENSOR_STATE,
};

/* Firmware download STATE */
enum {
	FW_DL_STATE_FAIL = -1,
	FW_DL_STATE_NONE = 0,
	FW_DL_STATE_NEED_TO_SCHEDULE,
	FW_DL_STATE_SCHEDULED,
	FW_DL_STATE_DOWNLOADING,
	FW_DL_STATE_SYNC,
	FW_DL_STATE_DONE,
};

/* for MSG2SSP_AP_GET_THERM */
enum {
	ADC_BATT = 0,
	ADC_CHG,
};

#define SSP_INVALID_REVISION	99999
#define SSP_INVALID_REVISION2	0xFFFFFF


/* Gyroscope DPS */
#define GYROSCOPE_DPS250		250
#define GYROSCOPE_DPS500		500
#define GYROSCOPE_DPS2000		2000

/* Gesture Sensor Current */
#define DEFUALT_IR_CURRENT		100 //0xF0

/* kernel -> ssp manager cmd*/
#define SSP_LIBRARY_SLEEP_CMD	(1 << 5)
#define SSP_LIBRARY_LARGE_DATA_CMD	(1 << 6)
#define SSP_LIBRARY_WAKEUP_CMD	(1 << 7)

/* AP -> SSP Instruction */
#define MSG2SSP_INST_BYPASS_SENSOR_ADD	0xA1
#define MSG2SSP_INST_BYPASS_SENSOR_REMOVE	0xA2
#define MSG2SSP_INST_REMOVE_ALL		0xA3
#define MSG2SSP_INST_CHANGE_DELAY		0xA4
#define MSG2SSP_INST_LIBRARY_ADD		0xB1
#define MSG2SSP_INST_LIBRARY_REMOVE		0xB2
#define MSG2SSP_INST_LIB_NOTI		0xB4
#define MSG2SSP_INST_LIB_DATA		0xC1

#define MSG2SSP_AP_STT			0xC8
#define MSG2SSP_AP_MCU_SET_GYRO_CAL		0xCD
#define MSG2SSP_AP_MCU_SET_ACCEL_CAL		0xCE
#define MSG2SSP_AP_STATUS_WAKEUP		0xD1
#define MSG2SSP_AP_STATUS_SLEEP		0xD2
#define MSG2SSP_AP_STATUS_RESUME		0xD3
#define MSG2SSP_AP_STATUS_SUSPEND		0xD4
#define MSG2SSP_AP_STATUS_RESET		0xD5
#define MSG2SSP_AP_STATUS_POW_CONNECTED	0xD6
#define MSG2SSP_AP_STATUS_POW_DISCONNECTED	0xD7
#define MSG2SSP_AP_TEMPHUMIDITY_CAL_DONE	0xDA
#define MSG2SSP_AP_MCU_SET_DUMPMODE		0xDB
#define MSG2SSP_AP_MCU_DUMP_FINISH		0xDC




#define MSG2SSP_AP_WHOAMI			0x0F
#define MSG2SSP_AP_FIRMWARE_REV		0xF0
#define MSG2SSP_AP_SENSOR_FORMATION		0xF1
#define MSG2SSP_AP_SENSOR_PROXTHRESHOLD	0xF2
#define MSG2SSP_AP_SENSOR_BARCODE_EMUL	0xF3
#define MSG2SSP_AP_SENSOR_SCANNING		0xF4
#define MSG2SSP_AP_SET_MAGNETIC_HWOFFSET	0xF5
#define MSG2SSP_AP_GET_MAGNETIC_HWOFFSET	0xF6
#define MSG2SSP_AP_SENSOR_GESTURE_CURRENT	0xF7
#define MSG2SSP_AP_GET_THERM		0xF8
#define MSG2SSP_AP_GET_BIG_DATA		0xF9
#define MSG2SSP_AP_SET_BIG_DATA		0xFA
#define MSG2SSP_AP_START_BIG_DATA		0xFB
#define MSG2SSP_AP_MCU_SANITY_CHECK		0xFC
#define MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX	0xFD

#define MSG2SSP_AP_FUSEROM			0X01

/* voice data */
#define TYPE_WAKE_UP_VOICE_SERVICE			0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_AM		0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_GRAMMER	0x02

/* Factory Test */
#define ACCELEROMETER_FACTORY	0x80
#define GYROSCOPE_FACTORY		0x81
#define GEOMAGNETIC_FACTORY		0x82
#define PRESSURE_FACTORY		0x85
#define GESTURE_FACTORY		0x86
#define TEMPHUMIDITY_CRC_FACTORY	0x88
#define GYROSCOPE_TEMP_FACTORY	0x8A
#define GYROSCOPE_DPS_FACTORY	0x8B
#define MCU_FACTORY		0x8C
#define MCU_SLEEP_FACTORY		0x8D

/* Factory data length */
#define ACCEL_FACTORY_DATA_LENGTH		1
#define GYRO_FACTORY_DATA_LENGTH		36
#define MAGNETIC_FACTORY_DATA_LENGTH		26
#define PRESSURE_FACTORY_DATA_LENGTH		1
#define MCU_FACTORY_DATA_LENGTH		5
#define	GYRO_TEMP_FACTORY_DATA_LENGTH	2
#define	GYRO_DPS_FACTORY_DATA_LENGTH	1
#define TEMPHUMIDITY_FACTORY_DATA_LENGTH	1
#define MCU_SLEEP_FACTORY_DATA_LENGTH	FACTORY_DATA_MAX
#define GESTURE_FACTORY_DATA_LENGTH		4

/* SSP -> AP ACK about write CMD */
#define MSG_ACK		0x80	/* ACK from SSP to AP */
#define MSG_NAK		0x70	/* NAK from SSP to AP */

/* Accelerometer sensor*/
/* 16bits */
#define MAX_ACCEL_1G	16384
#define MAX_ACCEL_2G	32767
#define MIN_ACCEL_2G	-32768
#define MAX_ACCEL_4G	65536

#define MAX_GYRO		32767
#define MIN_GYRO		-32768

#define MCU_IS_SANE	0x20
#define MAX_COMP_BUFF	60

/* temphumidity sensor*/
struct shtc1_buffer {
	u16 batt[MAX_COMP_BUFF];
	u16 chg[MAX_COMP_BUFF];
	s16 temp[MAX_COMP_BUFF];
	u16 humidity[MAX_COMP_BUFF];
	u16 baro[MAX_COMP_BUFF];
	u16 gyro[MAX_COMP_BUFF];
	char len;
};

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
	REMOVE_LIBRARY,
	ADD_LIBRARY,
};

/* SENSOR_TYPE */
enum {
	ACCELEROMETER_SENSOR = 0,
	GYROSCOPE_SENSOR,
	GEOMAGNETIC_UNCALIB_SENSOR,
	GEOMAGNETIC_RAW,
	GEOMAGNETIC_SENSOR,
	PRESSURE_SENSOR,
	GESTURE_SENSOR,
	PROXIMITY_SENSOR,
	TEMPERATURE_HUMIDITY_SENSOR,
	LIGHT_SENSOR,
	PROXIMITY_RAW,
	ORIENTATION_SENSOR = 11,
	STEP_DETECTOR,
	SIGNIFICANT_MOTION,
	UNCALIBRATED_GYRO,
	GAME_ROTATION_VECTOR,
	SENSOR_MAX,	/*  = 16 */
};

#define SSP_BYPASS_SENSORS_EN_ALL (1 << ACCELEROMETER_SENSOR |\
	1 << GYROSCOPE_SENSOR | 1 << GEOMAGNETIC_UNCALIB_SENSOR |\
	1 << GEOMAGNETIC_SENSOR | 1 << PRESSURE_SENSOR |\
	1 << TEMPERATURE_HUMIDITY_SENSOR | 1 << LIGHT_SENSOR|\
	1 << UNCALIBRATED_GYRO | 1 << GAME_ROTATION_VECTOR) /* PROXIMITY_SENSOR is not continuos */

struct sensor_value {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		struct {		/* calibtated */
			s16 cal_x;
			s16 cal_y;
			s16 cal_z;
			u8 cal_accu;
		};
		struct {		/* uncalibtated */
			s16 uncal_x;
			s16 uncal_y;
			s16 uncal_z;
			s16 offset_x;
			s16 offset_y;
			s16 offset_z;
		};
		struct {
			u16 r;
			u16 g;
			u16 b;
			u16 w;
#ifdef CONFIG_SENSORS_SSP_MAX88921
			u16 ir_cmp;
			u16 amb_pga;
#endif
		};
		struct {		/* quaternion vector */
			s32 quat_vec[4];
			u8 vec_accu;
		};
		u16 prox[4];
		s16 data[19];
		s32 pressure[3];
		u8 step_det;
		u8 sig_motion;
	};
} __attribute__((__packed__));

extern struct class *sensors_event_class;

struct calibraion_data {
	s16 x;
	s16 y;
	s16 z;
};

struct hw_offset_data {
	char x;
	char y;
	char z;
};


/* ssp_msg options bit*/
#define SSP_SPI		0	/* read write mask */
#define SSP_GYRO_DPS	2	/* gyro dps mask */
#define SSP_INDEX		2	/* data index mask */

#define SSP_SPI_MASK	(3 << SSP_SPI)	/* read write mask */
#define SSP_GYRO_DPS_MASK	(3 << SSP_GYRO_DPS)
#define SSP_INDEX_MASK	(63 << SSP_INDEX)	/* dump index mask. Index is up to 64 */

struct ssp_msg {
	u8 cmd;
	u16 length;
	u8 options;
	u32 data;

	struct list_head list;
	struct completion *done;
	char *buffer;
	u8 free_buffer;
	bool *dead_hook;
	bool dead;
} __attribute__((__packed__));

enum {
	AP2HUB_READ = 0,
	AP2HUB_WRITE,
	HUB2AP_WRITE,
	AP2HUB_READY
};

enum {
	BIG_TYPE_DUMP = 0,
	BIG_TYPE_READ_LIB,
	/*+snamy.jeong 0706 for voice model download & pcm dump*/
	BIG_TYPE_VOICE_NET,
	BIG_TYPE_VOICE_GRAM,
	BIG_TYPE_VOICE_PCM,
	/*-snamy.jeong 0706 for voice model download & pcm dump*/
	BIG_TYPE_TEMP,
	BIG_TYPE_MAX,
};

struct ssp_data {
	struct input_dev *acc_input_dev;
	struct input_dev *gyro_input_dev;
	struct input_dev *mag_input_dev;
	struct input_dev *gesture_input_dev;
	struct input_dev *pressure_input_dev;
	struct input_dev *light_input_dev;
	struct input_dev *prox_input_dev;
	struct input_dev *temp_humi_input_dev;
	struct input_dev *uncal_mag_input_dev;
	struct input_dev *uncal_gyro_input_dev;
	struct input_dev *step_det_input_dev;
	struct input_dev *sig_motion_input_dev;
	struct input_dev *grot_vec_input_dev;

	struct device *mcu_device;
	struct device *acc_device;
	struct device *gyro_device;
	struct device *mag_device;
	struct device *prs_device;
	struct device *prox_device;
	struct device *light_device;
	struct device *ges_device;
	struct device *temphumidity_device;
	struct miscdevice shtc1_device;

#ifdef CONFIG_SENSORS_SSP_STM
	struct spi_device *spi;
#endif
	struct i2c_client *client;
	struct wake_lock ssp_wake_lock;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct delayed_work work_firmware;
	struct work_struct work_debug;
	struct calibraion_data accelcal;
	struct calibraion_data gyrocal;
	struct hw_offset_data magoffset;
	struct sensor_value buf[SENSOR_MAX];

/*snamy.jeong@samsung.com temporary code for voice data sending to mcu*/
	struct device *voice_device;

	bool bSspShutdown;
	bool bAccelAlert;
	bool bProximityRawEnabled;
	bool bGeomagneticRawEnabled;
	bool bBarcodeEnabled;
	bool bMcuDumpMode;
	bool bBinaryChashed;
	bool bProbeIsDone;
	bool bDumping;

	unsigned int uProxCanc;
	unsigned int uProxHiThresh;
	unsigned int uProxLoThresh;
	unsigned int uProxHiThresh_default;
	unsigned int uProxLoThresh_default;
	unsigned int uIr_Current;
	unsigned char uFuseRomData[3];
	unsigned char uMagCntlRegData;
	char *pchLibraryBuf;
	char chLcdLdi[2];
	int iIrq;
	int iLibraryLength;
	int aiCheckStatus[SENSOR_MAX];

	unsigned int uIrqFailCnt;
	unsigned int uSsdFailCnt;
	unsigned int uResetCnt;
	unsigned int uInstFailCnt;
	unsigned int uTimeOutCnt;
	unsigned int uIrqCnt;
	unsigned int uBusyCnt;
	unsigned int uMissSensorCnt;
	unsigned int uSanityCnt;
	unsigned int uDumpCnt;

	unsigned int uGyroDps;
	unsigned int uSensorState;
	unsigned int uCurFirmRev;
	unsigned int uFactoryProxAvg[4];
	char uLastResumeState;
	char uLastAPState;
	s32 iPressureCal;

	atomic_t aSensorEnable;
	int64_t adDelayBuf[SENSOR_MAX];

	int (*wakeup_mcu)(void);
	int (*check_mcu_ready)(void);
	int (*check_mcu_busy)(void);
	int (*set_mcu_reset)(int);
	void (*get_sensor_data[SENSOR_MAX])(char *, int *,
		struct sensor_value *);
	void (*report_sensor_data[SENSOR_MAX])(struct ssp_data *,
		struct sensor_value *);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	struct ssp_sensorhub_data *hub_data;
#endif
	int ap_rev;
	int accel_position;
	int mag_position;
	int fw_dl_state;
	u8 mag_matrix_size;
	u8 *mag_matrix;
#ifdef CONFIG_SENSORS_SSP_SHTC1
	char *comp_engine_ver;
	struct platform_device *pdev_pam_temp;
	struct s3c_adc_client *adc_client;
	u8 cp_thm_adc_channel;
	u8 cp_thm_adc_arr_size;
	u8 batt_thm_adc_arr_size;
	u8 chg_thm_adc_arr_size;
	struct thm_adc_table *cp_thm_adc_table;
	struct thm_adc_table *batt_thm_adc_table;
	struct thm_adc_table *chg_thm_adc_table;
	struct mutex cp_temp_adc_lock;
	struct mutex bulk_temp_read_lock;
	struct shtc1_buffer* bulk_buffer;
#endif
#ifdef CONFIG_SENSORS_SSP_STM
	struct mutex comm_mutex;
	struct mutex pending_mutex;
#endif

	int ap_int;
	int mcu_int1;
	int mcu_int2;
	struct list_head pending_list;
	void (*ssp_big_task[BIG_TYPE_MAX])(struct work_struct *);
};

struct ssp_big {
	struct ssp_data* data;
	struct work_struct work;
	u32 length;
	u32 addr;
};

void ssp_enable(struct ssp_data *, bool);
int ssp_spi_async(struct ssp_data *, struct ssp_msg *);
int ssp_spi_sync(struct ssp_data *, struct ssp_msg *, int);
void clean_pending_list(struct ssp_data *);
void toggle_mcu_reset(struct ssp_data *);
int initialize_mcu(struct ssp_data *);
int initialize_input_dev(struct ssp_data *);
int initialize_sysfs(struct ssp_data *);
void initialize_function_pointer(struct ssp_data *);
void initialize_accel_factorytest(struct ssp_data *);
void initialize_prox_factorytest(struct ssp_data *);
void initialize_light_factorytest(struct ssp_data *);
void initialize_gyro_factorytest(struct ssp_data *);
void initialize_pressure_factorytest(struct ssp_data *);
void initialize_magnetic_factorytest(struct ssp_data *);
void initialize_gesture_factorytest(struct ssp_data *data);
void initialize_temphumidity_factorytest(struct ssp_data *data);
void remove_accel_factorytest(struct ssp_data *);
void remove_gyro_factorytest(struct ssp_data *);
void remove_prox_factorytest(struct ssp_data *);
void remove_light_factorytest(struct ssp_data *);
void remove_pressure_factorytest(struct ssp_data *);
void remove_magnetic_factorytest(struct ssp_data *);
void remove_gesture_factorytest(struct ssp_data *data);
void remove_temphumidity_factorytest(struct ssp_data *data);
void sensors_remove_symlink(struct input_dev *);
void destroy_sensor_class(void);
int initialize_event_symlink(struct ssp_data *);
int sensors_create_symlink(struct input_dev *);
int accel_open_calibration(struct ssp_data *);
int gyro_open_calibration(struct ssp_data *);
int pressure_open_calibration(struct ssp_data *);
int proximity_open_calibration(struct ssp_data *);
int check_fwbl(struct ssp_data *);
void remove_input_dev(struct ssp_data *);
void remove_sysfs(struct ssp_data *);
void remove_event_symlink(struct ssp_data *);
int ssp_send_cmd(struct ssp_data *, char, int);
int send_instruction(struct ssp_data *, u8, u8, u8 *, u8);
int select_irq_msg(struct ssp_data *);
int get_chipid(struct ssp_data *);
int get_fuserom_data(struct ssp_data *);
int set_big_data_start(struct ssp_data *, u8 , u32);
int mag_open_hwoffset(struct ssp_data *);
int mag_store_hwoffset(struct ssp_data *);
int set_hw_offset(struct ssp_data *);
int get_hw_offset(struct ssp_data *);
int set_gyro_cal(struct ssp_data *);
int set_accel_cal(struct ssp_data *);
int set_sensor_position(struct ssp_data *);
int set_magnetic_static_matrix(struct ssp_data *);
int sanity_check(struct ssp_data *data);
void sync_sensor_state(struct ssp_data *);
void set_proximity_threshold(struct ssp_data *, unsigned int, unsigned int);
void set_proximity_barcode_enable(struct ssp_data *, bool);
void set_gesture_current(struct ssp_data *, unsigned char);
unsigned int get_delay_cmd(u8);
unsigned int get_msdelay(int64_t);
unsigned int get_sensor_scanning_info(struct ssp_data *);
unsigned int get_firmware_rev(struct ssp_data *);
int forced_to_download_binary(struct ssp_data *, int);
int parse_dataframe(struct ssp_data *, char *, int);
void enable_debug_timer(struct ssp_data *);
void disable_debug_timer(struct ssp_data *);
int initialize_debug_timer(struct ssp_data *);
int proximity_open_lcd_ldi(struct ssp_data *);
void report_acc_data(struct ssp_data *, struct sensor_value *);
void report_gyro_data(struct ssp_data *, struct sensor_value *);
void report_gyro_uncaldata(struct ssp_data *, struct sensor_value *);
void report_mag_data(struct ssp_data *, struct sensor_value *);
void report_mag_uncaldata(struct ssp_data *, struct sensor_value *);
void report_geomagnetic_raw_data(struct ssp_data *, struct sensor_value *);
void report_grot_vec_sensordata(struct ssp_data *, struct sensor_value *);
void report_step_det_data(struct ssp_data *, struct sensor_value *);
void report_sig_motion_sensordata(struct ssp_data *, struct sensor_value *);
void report_gesture_data(struct ssp_data *, struct sensor_value *);
void report_pressure_data(struct ssp_data *, struct sensor_value *);
void report_light_data(struct ssp_data *, struct sensor_value *);
void report_prox_data(struct ssp_data *, struct sensor_value *);
void report_prox_raw_data(struct ssp_data *, struct sensor_value *);
void report_temp_humidity_data(struct ssp_data *, struct sensor_value *);
void report_bulk_comp_data(struct ssp_data *data);
unsigned int get_module_rev(struct ssp_data *data);
int print_mcu_debug(char *, int *, int);
void reset_mcu(struct ssp_data *);
void convert_acc_data(s16 *);
int sensors_register(struct device *, void *,
	struct device_attribute*[], char *);
void sensors_unregister(struct device *,
	struct device_attribute*[]);
ssize_t mcu_reset_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_dump_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_revision_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_update_ums_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_crashed_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_factorytest_store(struct device *, struct device_attribute *,
	const char *, size_t);
ssize_t mcu_factorytest_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_model_name_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_sleep_factorytest_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_sleep_factorytest_store(struct device *,
	struct device_attribute *, const char *, size_t);
unsigned int ssp_check_sec_dump_mode(void);




#ifdef CONFIG_SENSORS_SSP_STM
void ssp_dump_task(struct work_struct *work);
void ssp_read_big_library_task(struct work_struct *work);
void ssp_send_big_library_task(struct work_struct *work);
void ssp_pcm_dump_task(struct work_struct *work);
void ssp_temp_task(struct work_struct *work);
#endif

#endif
