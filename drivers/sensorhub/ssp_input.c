/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include "ssp.h"

/*************************************************************************/
/* SSP Kernel -> HAL input evnet function                                */
/*************************************************************************/
void convert_acc_data(s16 *iValue)
{
	if (*iValue > MAX_ACCEL_2G)
		*iValue = ((MAX_ACCEL_4G - *iValue)) * (-1);
}

void report_acc_data(struct ssp_data *data, struct sensor_value *accdata)
{
	convert_acc_data(&accdata->x);
	convert_acc_data(&accdata->y);
	convert_acc_data(&accdata->z);

	data->buf[ACCELEROMETER_SENSOR].x = accdata->x - data->accelcal.x;
	data->buf[ACCELEROMETER_SENSOR].y = accdata->y - data->accelcal.y;
	data->buf[ACCELEROMETER_SENSOR].z = accdata->z - data->accelcal.z;

	if (!(data->buf[ACCELEROMETER_SENSOR].x >> 15 == accdata->x >> 15) &&\
		!(data->accelcal.x >> 15 == accdata->x >> 15)) {
		pr_debug("[SSP] : accel x is overflowed!\n");
		data->buf[ACCELEROMETER_SENSOR].x =
			(data->buf[ACCELEROMETER_SENSOR].x > 0 ? MIN_ACCEL_2G : MAX_ACCEL_2G);
	}
	if (!(data->buf[ACCELEROMETER_SENSOR].y >> 15 == accdata->y >> 15) &&\
		!(data->accelcal.y >> 15 == accdata->y >> 15)) {
		pr_debug("[SSP] : accel y is overflowed!\n");
		data->buf[ACCELEROMETER_SENSOR].y =
			(data->buf[ACCELEROMETER_SENSOR].y > 0 ? MIN_ACCEL_2G : MAX_ACCEL_2G);
	}
	if (!(data->buf[ACCELEROMETER_SENSOR].z >> 15 == accdata->z >> 15) &&\
		!(data->accelcal.z >> 15 == accdata->z >> 15)) {
		pr_debug("[SSP] : accel z is overflowed!\n");
		data->buf[ACCELEROMETER_SENSOR].z =
			(data->buf[ACCELEROMETER_SENSOR].z > 0 ? MIN_ACCEL_2G : MAX_ACCEL_2G);
	}

	input_report_rel(data->acc_input_dev, REL_X,
		data->buf[ACCELEROMETER_SENSOR].x);
	input_report_rel(data->acc_input_dev, REL_Y,
		data->buf[ACCELEROMETER_SENSOR].y);
	input_report_rel(data->acc_input_dev, REL_Z,
		data->buf[ACCELEROMETER_SENSOR].z);
	input_sync(data->acc_input_dev);
}

void report_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata)
{
	long lTemp[3] = {0,};

	data->buf[GYROSCOPE_SENSOR].x = gyrodata->x - data->gyrocal.x;
	data->buf[GYROSCOPE_SENSOR].y = gyrodata->y - data->gyrocal.y;
	data->buf[GYROSCOPE_SENSOR].z = gyrodata->z - data->gyrocal.z;

	if (!(data->buf[GYROSCOPE_SENSOR].x >> 15 == gyrodata->x >> 15) &&\
		!(data->gyrocal.x >> 15 == gyrodata->x >> 15)) {
		pr_debug("[SSP] : gyro x is overflowed!\n");
		data->buf[GYROSCOPE_SENSOR].x =
			(data->buf[GYROSCOPE_SENSOR].x >= 0 ? MIN_GYRO : MAX_GYRO);
	}
	if (!(data->buf[GYROSCOPE_SENSOR].y >> 15 == gyrodata->y >> 15) &&\
		!(data->gyrocal.y >> 15 == gyrodata->y >> 15)) {
		pr_debug("[SSP] : gyro y is overflowed!\n");
		data->buf[GYROSCOPE_SENSOR].y =
			(data->buf[GYROSCOPE_SENSOR].y >= 0 ? MIN_GYRO : MAX_GYRO);
	}
	if (!(data->buf[GYROSCOPE_SENSOR].z >> 15 == gyrodata->z >> 15) &&\
		!(data->gyrocal.z >> 15 == gyrodata->z >> 15)) {
		pr_debug("[SSP] : gyro z is overflowed!\n");
		data->buf[GYROSCOPE_SENSOR].z =
			(data->buf[GYROSCOPE_SENSOR].z >= 0 ? MIN_GYRO : MAX_GYRO);
	}

	if (data->uGyroDps == GYROSCOPE_DPS500) {
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
	} else if (data->uGyroDps == GYROSCOPE_DPS250)	{
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x >> 1;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y >> 1;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z >> 1;
	} else if (data->uGyroDps == GYROSCOPE_DPS2000)	{
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x << 2;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y << 2;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z << 2;
	} else {
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
	}

	input_report_rel(data->gyro_input_dev, REL_RX, lTemp[0]);
	input_report_rel(data->gyro_input_dev, REL_RY, lTemp[1]);
	input_report_rel(data->gyro_input_dev, REL_RZ, lTemp[2]);
	input_sync(data->gyro_input_dev);
}

void report_gesture_data(struct ssp_data *data, struct sensor_value *gesdata)
{
	data->buf[GESTURE_SENSOR].data[0] = gesdata->data[0];
	data->buf[GESTURE_SENSOR].data[1] = gesdata->data[1];
	data->buf[GESTURE_SENSOR].data[2] = gesdata->data[2];
	data->buf[GESTURE_SENSOR].data[3] = gesdata->data[3];
	data->buf[GESTURE_SENSOR].data[4] = data->uTempCount;

	input_report_abs(data->gesture_input_dev,
		ABS_RUDDER, data->buf[GESTURE_SENSOR].data[0]);
	input_report_abs(data->gesture_input_dev,
		ABS_WHEEL, data->buf[GESTURE_SENSOR].data[1]);
	input_report_abs(data->gesture_input_dev,
		ABS_GAS, data->buf[GESTURE_SENSOR].data[2]);
	input_report_abs(data->gesture_input_dev,
		ABS_BRAKE, data->buf[GESTURE_SENSOR].data[3]);
	input_report_abs(data->gesture_input_dev,
		ABS_THROTTLE, data->buf[GESTURE_SENSOR].data[4]);
	input_sync(data->gesture_input_dev);

	data->uTempCount++;

}

void report_pressure_data(struct ssp_data *data, struct sensor_value *predata)
{
	data->buf[PRESSURE_SENSOR].pressure[0] =
		predata->pressure[0] - data->iPressureCal;
	data->buf[PRESSURE_SENSOR].pressure[1] = predata->pressure[1];

	/* pressure */
	input_report_rel(data->pressure_input_dev, REL_HWHEEL,
		data->buf[PRESSURE_SENSOR].pressure[0]);
	/* temperature */
	input_report_rel(data->pressure_input_dev, REL_WHEEL,
		data->buf[PRESSURE_SENSOR].pressure[1]);
	input_sync(data->pressure_input_dev);
}

void report_light_data(struct ssp_data *data, struct sensor_value *lightdata)
{
	data->buf[LIGHT_SENSOR].r = lightdata->r;
	data->buf[LIGHT_SENSOR].g = lightdata->g;
	data->buf[LIGHT_SENSOR].b = lightdata->b;
	data->buf[LIGHT_SENSOR].w = lightdata->w;

	input_report_rel(data->light_input_dev, REL_HWHEEL,
		data->buf[LIGHT_SENSOR].r + 1);
	input_report_rel(data->light_input_dev, REL_DIAL,
		data->buf[LIGHT_SENSOR].g + 1);
	input_report_rel(data->light_input_dev, REL_WHEEL,
		data->buf[LIGHT_SENSOR].b + 1);
	input_report_rel(data->light_input_dev, REL_MISC,
		data->buf[LIGHT_SENSOR].w + 1);
	input_sync(data->light_input_dev);
}

void report_prox_data(struct ssp_data *data, struct sensor_value *proxdata)
{
	ssp_dbg("[SSP] Proximity Sensor Detect : %u, raw : %u\n",
		proxdata->prox[0], proxdata->prox[1]);

	data->buf[PROXIMITY_SENSOR].prox[0] = proxdata->prox[0];
	data->buf[PROXIMITY_SENSOR].prox[1] = proxdata->prox[1];

	input_report_abs(data->prox_input_dev, ABS_DISTANCE,
		(!proxdata->prox[0]));
	input_sync(data->prox_input_dev);

	wake_lock_timeout(&data->ssp_wake_lock, 3 * HZ);
}

void report_prox_raw_data(struct ssp_data *data,
	struct sensor_value *proxrawdata)
{
	if (data->uFactoryProxAvg[0]++ >= PROX_AVG_READ_NUM) {
		data->uFactoryProxAvg[2] /= PROX_AVG_READ_NUM;
		data->buf[PROXIMITY_RAW].prox[1] = (u8)data->uFactoryProxAvg[1];
		data->buf[PROXIMITY_RAW].prox[2] = (u8)data->uFactoryProxAvg[2];
		data->buf[PROXIMITY_RAW].prox[3] = (u8)data->uFactoryProxAvg[3];

		data->uFactoryProxAvg[0] = 0;
		data->uFactoryProxAvg[1] = 0;
		data->uFactoryProxAvg[2] = 0;
		data->uFactoryProxAvg[3] = 0;
	} else {
		data->uFactoryProxAvg[2] += proxrawdata->prox[0];

		if (data->uFactoryProxAvg[0] == 1)
			data->uFactoryProxAvg[1] = proxrawdata->prox[0];
		else if (proxrawdata->prox[0] < data->uFactoryProxAvg[1])
			data->uFactoryProxAvg[1] = proxrawdata->prox[0];

		if (proxrawdata->prox[0] > data->uFactoryProxAvg[3])
			data->uFactoryProxAvg[3] = proxrawdata->prox[0];
	}

	data->buf[PROXIMITY_RAW].prox[0] = proxrawdata->prox[0];
}

void report_temp_humidity_data(struct ssp_data *data,
	struct sensor_value *temp_humi_data)
{
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[0] =
		temp_humi_data->data[0];
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[1] =
		temp_humi_data->data[1];
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[2] =
		temp_humi_data->data[2];

	/* Temperature */
	input_report_rel(data->temp_humi_input_dev, REL_HWHEEL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[0]);
	/* Humidity */
	input_report_rel(data->temp_humi_input_dev, REL_DIAL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[1]);

	/* Compensation engine cmd */
	if (data->comp_engine_cmd != SHTC1_CMD_NONE) {
		input_report_rel(data->temp_humi_input_dev, REL_WHEEL,
			data->comp_engine_cmd);
		data->comp_engine_cmd = SHTC1_CMD_NONE;
	}
	input_sync(data->temp_humi_input_dev);
	if (data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[2])
		wake_lock_timeout(&data->ssp_wake_lock, 2 * HZ);
}

void report_sig_motion_data(struct ssp_data *data,
	struct sensor_value *sig_motion_data)
{
	data->buf[SIG_MOTION_SENSOR].sig_motion = sig_motion_data->sig_motion;

	input_report_rel(data->sig_motion_input_dev, REL_MISC,
		data->buf[SIG_MOTION_SENSOR].sig_motion);
	input_sync(data->sig_motion_input_dev);
}

#ifdef FEATURE_STEP_SENSOR
void report_step_det_data(struct ssp_data *data,
	struct sensor_value *sig_motion_data)
{
	data->buf[STEP_DETECTOR].step_det = sig_motion_data->step_det;

	input_report_rel(data->step_det_input_dev, REL_MISC,
		data->buf[STEP_DETECTOR].step_det + 1);
	input_sync(data->step_det_input_dev);
}

void report_step_cnt_data(struct ssp_data *data,
	struct sensor_value *sig_motion_data)
{
	data->buf[STEP_COUNTER].step_diff = sig_motion_data->step_diff;

	data->step_count_total += data->buf[STEP_COUNTER].step_diff;

	input_report_rel(data->step_cnt_input_dev, REL_MISC,
		data->step_count_total + 1);
	input_sync(data->step_cnt_input_dev);
}
#endif

int initialize_event_symlink(struct ssp_data *data)
{
	int iRet = 0;

	iRet = sensors_create_symlink(data->acc_input_dev);
	if (iRet < 0)
		goto iRet_acc_sysfs_create_link;

	iRet = sensors_create_symlink(data->gyro_input_dev);
	if (iRet < 0)
		goto iRet_gyro_sysfs_create_link;

	iRet = sensors_create_symlink(data->pressure_input_dev);
	if (iRet < 0)
		goto iRet_prs_sysfs_create_link;

	iRet = sensors_create_symlink(data->light_input_dev);
	if (iRet < 0)
		goto iRet_light_sysfs_create_link;

	iRet = sensors_create_symlink(data->prox_input_dev);
	if (iRet < 0)
		goto iRet_prox_sysfs_create_link;

	iRet = sensors_create_symlink(data->temp_humi_input_dev);
	if (iRet < 0)
		goto iRet_temp_humi_sysfs_create_link;

	iRet = sensors_create_symlink(data->gesture_input_dev);
	if (iRet < 0)
		goto iRet_gesture_sysfs_create_link;

	iRet = sensors_create_symlink(data->sig_motion_input_dev);
	if (iRet < 0)
		goto iRet_sig_motion_sysfs_create_link;

#ifdef FEATURE_STEP_SENSOR
	iRet = sensors_create_symlink(data->step_det_input_dev);
	if (iRet < 0)
		goto iRet_step_det_sysfs_create_link;

	iRet = sensors_create_symlink(data->step_cnt_input_dev);
	if (iRet < 0)
		goto iRet_step_cnt_sysfs_create_link;
#endif

	return SUCCESS;

#ifdef FEATURE_STEP_SENSOR
iRet_step_cnt_sysfs_create_link:
	sensors_remove_symlink(data->step_det_input_dev);
iRet_step_det_sysfs_create_link:
	sensors_remove_symlink(data->sig_motion_input_dev);
#endif
iRet_sig_motion_sysfs_create_link:
	sensors_remove_symlink(data->gesture_input_dev);
iRet_gesture_sysfs_create_link:
	sensors_remove_symlink(data->temp_humi_input_dev);
iRet_temp_humi_sysfs_create_link:
	sensors_remove_symlink(data->prox_input_dev);
iRet_prox_sysfs_create_link:
	sensors_remove_symlink(data->light_input_dev);
iRet_light_sysfs_create_link:
	sensors_remove_symlink(data->pressure_input_dev);
iRet_prs_sysfs_create_link:
	sensors_remove_symlink(data->gyro_input_dev);
iRet_gyro_sysfs_create_link:
	sensors_remove_symlink(data->acc_input_dev);
iRet_acc_sysfs_create_link:
	pr_err("[SSP]: %s - could not create event symlink\n", __func__);

	return FAIL;
}

void remove_event_symlink(struct ssp_data *data)
{
	sensors_remove_symlink(data->acc_input_dev);
	sensors_remove_symlink(data->gyro_input_dev);
	sensors_remove_symlink(data->pressure_input_dev);
	sensors_remove_symlink(data->light_input_dev);
	sensors_remove_symlink(data->prox_input_dev);
	sensors_remove_symlink(data->temp_humi_input_dev);
	sensors_remove_symlink(data->gesture_input_dev);
	sensors_remove_symlink(data->sig_motion_input_dev);
#ifdef FEATURE_STEP_SENSOR
	sensors_remove_symlink(data->step_det_input_dev);
	sensors_remove_symlink(data->step_cnt_input_dev);
#endif
}

int initialize_input_dev(struct ssp_data *data)
{
	int iRet = 0;

	data->acc_input_dev = input_allocate_device();
	if (data->acc_input_dev == NULL)
		goto err_initialize_acc_input_dev;

	data->acc_input_dev->name = "accelerometer_sensor";
	input_set_capability(data->acc_input_dev, EV_REL, REL_X);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Y);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->acc_input_dev);
	if (iRet < 0) {
		input_free_device(data->acc_input_dev);
		goto err_initialize_acc_input_dev;
	}
	input_set_drvdata(data->acc_input_dev, data);

	data->gyro_input_dev = input_allocate_device();
	if (data->gyro_input_dev == NULL)
		goto err_initialize_gyro_input_dev;

	data->gyro_input_dev->name = "gyro_sensor";
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RZ);
	iRet = input_register_device(data->gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->gyro_input_dev);
		goto err_initialize_gyro_input_dev;
	}
	input_set_drvdata(data->gyro_input_dev, data);

	data->pressure_input_dev = input_allocate_device();
	if (data->pressure_input_dev == NULL)
		goto err_initialize_pressure_input_dev;

	data->pressure_input_dev->name = "pressure_sensor";
	input_set_capability(data->pressure_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->pressure_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->pressure_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->pressure_input_dev);
	if (iRet < 0) {
		input_free_device(data->pressure_input_dev);
		goto err_initialize_pressure_input_dev;
	}
	input_set_drvdata(data->pressure_input_dev, data);

	data->light_input_dev = input_allocate_device();
	if (data->light_input_dev == NULL)
		goto err_initialize_light_input_dev;

	data->light_input_dev->name = "light_sensor";
	input_set_capability(data->light_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->light_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->light_input_dev, EV_REL, REL_WHEEL);
	input_set_capability(data->light_input_dev, EV_REL, REL_MISC);
	iRet = input_register_device(data->light_input_dev);
	if (iRet < 0) {
		input_free_device(data->light_input_dev);
		goto err_initialize_light_input_dev;
	}
	input_set_drvdata(data->light_input_dev, data);

	data->prox_input_dev = input_allocate_device();
	if (data->prox_input_dev == NULL)
		goto err_initialize_proximity_input_dev;

	data->prox_input_dev->name = "proximity_sensor";
	input_set_capability(data->prox_input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(data->prox_input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	iRet = input_register_device(data->prox_input_dev);
	if (iRet < 0) {
		input_free_device(data->prox_input_dev);
		goto err_initialize_proximity_input_dev;
	}
	input_set_drvdata(data->prox_input_dev, data);

	data->temp_humi_input_dev = input_allocate_device();
	if (data->temp_humi_input_dev == NULL)
		goto err_initialize_temp_humi_input_dev;

	data->temp_humi_input_dev->name = "temp_humidity_sensor";
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->temp_humi_input_dev);
	if (iRet < 0) {
		input_free_device(data->temp_humi_input_dev);
		goto err_initialize_temp_humi_input_dev;
	}
	input_set_drvdata(data->temp_humi_input_dev, data);

	data->gesture_input_dev = input_allocate_device();
	if (data->gesture_input_dev == NULL)
		goto err_initialize_gesture_input_dev;

	data->gesture_input_dev->name = "gesture_sensor";
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_RUDDER);
	input_set_abs_params(data->gesture_input_dev, ABS_RUDDER, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_WHEEL);
	input_set_abs_params(data->gesture_input_dev, ABS_WHEEL, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_GAS);
	input_set_abs_params(data->gesture_input_dev, ABS_GAS, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_BRAKE);
	input_set_abs_params(data->gesture_input_dev, ABS_BRAKE, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_THROTTLE);
	input_set_abs_params(data->gesture_input_dev, ABS_THROTTLE, 0, 1024, 0, 0);
	iRet = input_register_device(data->gesture_input_dev);
	if (iRet < 0) {
		input_free_device(data->gesture_input_dev);
		goto err_initialize_gesture_input_dev;
	}
	input_set_drvdata(data->gesture_input_dev, data);

	data->sig_motion_input_dev = input_allocate_device();
	if (data->sig_motion_input_dev == NULL)
		goto err_initialize_sig_motion_input_dev;
	data->sig_motion_input_dev->name = "sig_motion_sensor";
	input_set_capability(data->sig_motion_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->sig_motion_input_dev);
	if (iRet < 0) {
		input_free_device(data->sig_motion_input_dev);
		goto err_initialize_sig_motion_input_dev;
	}
	input_set_drvdata(data->sig_motion_input_dev, data);

#ifdef FEATURE_STEP_SENSOR
	data->step_det_input_dev = input_allocate_device();
	if (data->step_det_input_dev == NULL)
		goto err_initialize_step_det_input_dev;
	data->step_det_input_dev->name = "step_det_sensor";
	input_set_capability(data->step_det_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->step_det_input_dev);
	if (iRet < 0) {
		input_free_device(data->step_det_input_dev);
		goto err_initialize_step_det_input_dev;
	}
	input_set_drvdata(data->step_det_input_dev, data);

	data->step_cnt_input_dev = input_allocate_device();
	if (data->step_cnt_input_dev == NULL)
		goto err_initialize_step_cnt_input_dev;
	data->step_cnt_input_dev->name = "step_cnt_sensor";
	input_set_capability(data->step_cnt_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->step_cnt_input_dev);
	if (iRet < 0) {
		input_free_device(data->step_cnt_input_dev);
		goto err_initialize_step_cnt_input_dev;
	}
	input_set_drvdata(data->step_cnt_input_dev, data);
#endif

	return SUCCESS;

#ifdef FEATURE_STEP_SENSOR
err_initialize_step_cnt_input_dev:
	pr_err("[SSP]: %s - could not allocate step counter input device\n", __func__);
	input_unregister_device(data->step_det_input_dev);
err_initialize_step_det_input_dev:
	pr_err("[SSP]: %s - could not allocate step detector input device\n", __func__);
	input_unregister_device(data->sig_motion_input_dev);
#endif
err_initialize_sig_motion_input_dev:
	pr_err("[SSP]: %s - could not allocate sig motion input device\n", __func__);
	input_unregister_device(data->gesture_input_dev);
err_initialize_gesture_input_dev:
	pr_err("[SSP]: %s - could not allocate gesture input device\n", __func__);
	input_unregister_device(data->temp_humi_input_dev);
err_initialize_temp_humi_input_dev:
	pr_err("[SSP]: %s - could not allocate temp_humi input device\n", __func__);
	input_unregister_device(data->prox_input_dev);
err_initialize_proximity_input_dev:
	pr_err("[SSP]: %s - could not allocate proximity input device\n", __func__);
	input_unregister_device(data->light_input_dev);
err_initialize_light_input_dev:
	pr_err("[SSP]: %s - could not allocate light input device\n", __func__);
	input_unregister_device(data->pressure_input_dev);
err_initialize_pressure_input_dev:
	pr_err("[SSP]: %s - could not allocate pressure input device\n", __func__);
	input_unregister_device(data->gyro_input_dev);
err_initialize_gyro_input_dev:
	pr_err("[SSP]: %s - could not allocate gyro input device\n", __func__);
	input_unregister_device(data->acc_input_dev);
err_initialize_acc_input_dev:
	pr_err("[SSP]: %s - could not allocate acc input device\n", __func__);
	return ERROR;
}

void remove_input_dev(struct ssp_data *data)
{
	input_unregister_device(data->acc_input_dev);
	input_unregister_device(data->gyro_input_dev);
	input_unregister_device(data->gesture_input_dev);
	input_unregister_device(data->pressure_input_dev);
	input_unregister_device(data->light_input_dev);
	input_unregister_device(data->prox_input_dev);
	input_unregister_device(data->temp_humi_input_dev);
	input_unregister_device(data->sig_motion_input_dev);
#ifdef FEATURE_STEP_SENSOR
	input_unregister_device(data->step_det_input_dev);
	input_unregister_device(data->step_cnt_input_dev);
#endif
}
