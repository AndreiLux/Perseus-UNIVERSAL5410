
#ifndef _SEC_HAPTIC_H
#define _SEC_HAPTIC_H

/* DO NOT CHANGE - this is auto-generated */
#define VERSION_STR " v3.4.55.8\n"

#define VERSION_STR_LEN 16
#define WATCHDOG_TIMEOUT    10  /* 10 timer cycles = 50ms */
#define MODULE_NAME                         "tspdrv"
#define TSPDRV                              "/dev/"MODULE_NAME
#define TSPDRV_MAGIC_NUMBER                 0x494D4D52
#define TSPDRV_STOP_KERNEL_TIMER            _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 1)
#define TSPDRV_ENABLE_AMP                   _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 3)
#define TSPDRV_DISABLE_AMP                  _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 4)
#define TSPDRV_GET_NUM_ACTUATORS            _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 5)
#define VIBE_NAME_SIZE			64
#define SPI_HEADER_SIZE                     3   /* DO NOT CHANGE - SPI buffer header size */
#define VIBE_OUTPUT_SAMPLE_SIZE             50  /* DO NOT CHANGE - maximum number of samples */
#define VIBE_E_FAIL						-4	/* Generic error */
#define NUM_ACTUATORS				2
#define SPI_BUFFER_SIZE (NUM_ACTUATORS * (VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE))

struct samples_data {
	u8 index;
	u8 bit_detpth;
	u8 size;
	u8 data[VIBE_OUTPUT_SAMPLE_SIZE];
};

struct actuator_data {
	struct samples_data samples[2];
	s8 playing;
	u8 output;
};

struct vibe_drvdata {
	void (*set_force)(u8 index, int nforce);
	void (*chip_en)(bool en);
	struct actuator_data actuator[NUM_ACTUATORS];
	struct hrtimer g_tspTimer;
	struct wake_lock wlock;
	ktime_t g_ktFiveMs;
	size_t cch_device_name;
	bool is_playing;
	bool stop_requested;
	bool amp_enabled;
	bool timer_started;
	int motor_enabled;
	int watchdog_cnt;
	int num_actuators;
	char write_buff[SPI_BUFFER_SIZE];
	char device_name[(VIBE_NAME_SIZE + VERSION_STR_LEN)	* NUM_ACTUATORS];
};

extern int tspdrv_init(struct vibe_drvdata *data);

#endif  /* _SEC_HAPTIC_H */
