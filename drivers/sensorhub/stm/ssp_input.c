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
	data->buf[ACCELEROMETER_SENSOR].x = accdata->x;
	data->buf[ACCELEROMETER_SENSOR].y = accdata->y;
	data->buf[ACCELEROMETER_SENSOR].z = accdata->z;

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
	data->buf[GYROSCOPE_SENSOR].x = gyrodata->x;
	data->buf[GYROSCOPE_SENSOR].y = gyrodata->y;
	data->buf[GYROSCOPE_SENSOR].z = gyrodata->z;

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

void report_gyro_uncaldata(struct ssp_data *data,
		struct sensor_value *gyrodata)
{
	data->buf[UNCALIBRATED_GYRO].uncal_x = gyrodata->uncal_x;
	data->buf[UNCALIBRATED_GYRO].uncal_y = gyrodata->uncal_y;
	data->buf[UNCALIBRATED_GYRO].uncal_z = gyrodata->uncal_z;
	data->buf[UNCALIBRATED_GYRO].offset_x= gyrodata->offset_x;
	data->buf[UNCALIBRATED_GYRO].offset_y= gyrodata->offset_y;
	data->buf[UNCALIBRATED_GYRO].offset_z= gyrodata->offset_z;

	input_report_rel(data->uncal_gyro_input_dev, REL_RX,
		data->buf[UNCALIBRATED_GYRO].uncal_x);
	input_report_rel(data->uncal_gyro_input_dev, REL_RY,
		data->buf[UNCALIBRATED_GYRO].uncal_y);
	input_report_rel(data->uncal_gyro_input_dev, REL_RZ,
		data->buf[UNCALIBRATED_GYRO].uncal_z);
	input_report_rel(data->uncal_gyro_input_dev, REL_HWHEEL,
		data->buf[UNCALIBRATED_GYRO].offset_x);
	input_report_rel(data->uncal_gyro_input_dev, REL_DIAL,
		data->buf[UNCALIBRATED_GYRO].offset_y);
	input_report_rel(data->uncal_gyro_input_dev, REL_WHEEL,
		data->buf[UNCALIBRATED_GYRO].offset_z);

	input_sync(data->uncal_gyro_input_dev);
}

void report_geomagnetic_raw_data(struct ssp_data *data,
	struct sensor_value *magrawdata)
{
	data->buf[GEOMAGNETIC_RAW].x = magrawdata->x;
	data->buf[GEOMAGNETIC_RAW].y = magrawdata->y;
	data->buf[GEOMAGNETIC_RAW].z = magrawdata->z;
}

void report_mag_data(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_SENSOR].cal_x = magdata->cal_x;
	data->buf[GEOMAGNETIC_SENSOR].cal_y = magdata->cal_y;
	data->buf[GEOMAGNETIC_SENSOR].cal_z = magdata->cal_z;
	data->buf[GEOMAGNETIC_SENSOR].cal_accu = magdata->cal_accu;

	input_report_rel(data->mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_SENSOR].cal_x);
	input_report_rel(data->mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_SENSOR].cal_y);
	input_report_rel(data->mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_SENSOR].cal_z);
	input_report_rel(data->mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_SENSOR].cal_accu + 1);

	input_sync(data->mag_input_dev);
}

void report_mag_uncaldata(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x = magdata->uncal_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y = magdata->uncal_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z = magdata->uncal_z;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x= magdata->offset_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y= magdata->offset_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z= magdata->offset_z;

	input_report_rel(data->uncal_mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncal_mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncal_mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncal_mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncal_mag_input_dev, REL_DIAL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncal_mag_input_dev, REL_WHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z);

	input_sync(data->uncal_mag_input_dev);
}

void report_grot_vec_sensordata(struct ssp_data *data,
	struct sensor_value *grvec_data)
{
	data->buf[GAME_ROTATION_VECTOR].quat_vec[0] = grvec_data->quat_vec[0];
	data->buf[GAME_ROTATION_VECTOR].quat_vec[1] = grvec_data->quat_vec[1];
	data->buf[GAME_ROTATION_VECTOR].quat_vec[2] = grvec_data->quat_vec[2];
	data->buf[GAME_ROTATION_VECTOR].quat_vec[3] = grvec_data->quat_vec[3];
	data->buf[GAME_ROTATION_VECTOR].vec_accu = grvec_data->vec_accu;

	input_report_rel(data->grot_vec_input_dev, REL_RX,
		data->buf[GAME_ROTATION_VECTOR].quat_vec[0]);
	input_report_rel(data->grot_vec_input_dev, REL_RY,
		data->buf[GAME_ROTATION_VECTOR].quat_vec[1]);
	input_report_rel(data->grot_vec_input_dev, REL_RZ,
		data->buf[GAME_ROTATION_VECTOR].quat_vec[2]);
	input_report_rel(data->grot_vec_input_dev, REL_HWHEEL,
		data->buf[GAME_ROTATION_VECTOR].quat_vec[3]);
	input_report_rel(data->grot_vec_input_dev, REL_DIAL,
		data->buf[GAME_ROTATION_VECTOR].vec_accu + 1);

	input_sync(data->grot_vec_input_dev);
}

void report_gesture_data(struct ssp_data *data, struct sensor_value *gesdata)
{
	int i = 0;
	for (i=0; i<19; i++) {
		data->buf[GESTURE_SENSOR].data[i] = gesdata->data[i];
	}

	input_report_abs(data->gesture_input_dev,
		ABS_X, data->buf[GESTURE_SENSOR].data[0]);
	input_report_abs(data->gesture_input_dev,
		ABS_Y, data->buf[GESTURE_SENSOR].data[1]);
	input_report_abs(data->gesture_input_dev,
		ABS_Z, data->buf[GESTURE_SENSOR].data[2]);
	input_report_abs(data->gesture_input_dev,
		ABS_RX, data->buf[GESTURE_SENSOR].data[3]);
	input_report_abs(data->gesture_input_dev,
		ABS_RY, data->buf[GESTURE_SENSOR].data[4]);
	input_report_abs(data->gesture_input_dev,
		ABS_RZ, data->buf[GESTURE_SENSOR].data[5]);
	input_report_abs(data->gesture_input_dev,
		ABS_THROTTLE, data->buf[GESTURE_SENSOR].data[6]);
	input_report_abs(data->gesture_input_dev,
		ABS_RUDDER, data->buf[GESTURE_SENSOR].data[7]);
	input_report_abs(data->gesture_input_dev,
		ABS_WHEEL, data->buf[GESTURE_SENSOR].data[8]);
	input_report_abs(data->gesture_input_dev,
		ABS_GAS, data->buf[GESTURE_SENSOR].data[9]);
	input_report_abs(data->gesture_input_dev,
		ABS_BRAKE, data->buf[GESTURE_SENSOR].data[10]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT0X, data->buf[GESTURE_SENSOR].data[11]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT0Y, data->buf[GESTURE_SENSOR].data[12]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT1X, data->buf[GESTURE_SENSOR].data[13]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT1Y, data->buf[GESTURE_SENSOR].data[14]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT2X, data->buf[GESTURE_SENSOR].data[15]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT2Y, data->buf[GESTURE_SENSOR].data[16]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT3X, data->buf[GESTURE_SENSOR].data[17]);
	input_report_abs(data->gesture_input_dev,
		ABS_HAT3Y, data->buf[GESTURE_SENSOR].data[18]);

	input_sync(data->gesture_input_dev);
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
#ifdef CONFIG_SENSORS_SSP_MAX88921
	data->buf[LIGHT_SENSOR].ir_cmp = lightdata->ir_cmp;
	data->buf[LIGHT_SENSOR].amb_pga = lightdata->amb_pga;
#endif

	input_report_rel(data->light_input_dev, REL_HWHEEL,
		data->buf[LIGHT_SENSOR].r + 1);
	input_report_rel(data->light_input_dev, REL_DIAL,
		data->buf[LIGHT_SENSOR].g + 1);
	input_report_rel(data->light_input_dev, REL_WHEEL,
		data->buf[LIGHT_SENSOR].b + 1);
	input_report_rel(data->light_input_dev, REL_MISC,
		data->buf[LIGHT_SENSOR].w + 1);
#ifdef CONFIG_SENSORS_SSP_MAX88921
	input_report_rel(data->light_input_dev, REL_RY,
		data->buf[LIGHT_SENSOR].ir_cmp + 1);
	input_report_rel(data->light_input_dev, REL_RZ,
		data->buf[LIGHT_SENSOR].amb_pga + 1);
#endif
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
		data->buf[PROXIMITY_RAW].prox[1] = (u16)data->uFactoryProxAvg[1];
		data->buf[PROXIMITY_RAW].prox[2] = (u16)data->uFactoryProxAvg[2];
		data->buf[PROXIMITY_RAW].prox[3] = (u16)data->uFactoryProxAvg[3];

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

	input_sync(data->temp_humi_input_dev);
	if (data->buf[TEMPERATURE_HUMIDITY_SENSOR].data[2])
		wake_lock_timeout(&data->ssp_wake_lock, 2 * HZ);
}

void report_step_det_data(struct ssp_data *data,
		struct sensor_value *stepdet_data)
{
	data->buf[STEP_DETECTOR].step_det = stepdet_data->step_det;

	input_report_rel(data->step_det_input_dev, REL_MISC,
		data->buf[STEP_DETECTOR].step_det + 1);
	input_sync(data->step_det_input_dev);
}

void report_sig_motion_sensordata(struct ssp_data *data,
	struct sensor_value *sig_motion_data)
{
	data->buf[SIGNIFICANT_MOTION].sig_motion = sig_motion_data->sig_motion;

	input_report_rel(data->sig_motion_input_dev, REL_MISC,
		data->buf[SIGNIFICANT_MOTION].sig_motion);
	input_sync(data->sig_motion_input_dev);
}

void report_bulk_comp_data(struct ssp_data *data)
{
	input_report_rel(data->temp_humi_input_dev, REL_WHEEL,
		data->bulk_buffer->len);
	input_sync(data->temp_humi_input_dev);
}

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

	iRet = sensors_create_symlink(data->gesture_input_dev);
	if (iRet < 0)
		goto iRet_gesture_sysfs_create_link;

	iRet = sensors_create_symlink(data->light_input_dev);
	if (iRet < 0)
		goto iRet_light_sysfs_create_link;

	iRet = sensors_create_symlink(data->prox_input_dev);
	if (iRet < 0)
		goto iRet_prox_sysfs_create_link;

	iRet = sensors_create_symlink(data->temp_humi_input_dev);
	if (iRet < 0)
		goto iRet_temp_humi_sysfs_create_link;

	iRet = sensors_create_symlink(data->mag_input_dev);
	if (iRet < 0)
		goto iRet_mag_sysfs_create_link;

	iRet = sensors_create_symlink(data->uncal_mag_input_dev);
	if (iRet < 0)
		goto iRet_uncal_mag_sysfs_create_link;

	iRet = sensors_create_symlink(data->step_det_input_dev);
	if (iRet < 0)
		goto iRet_step_det_sysfs_create_link;

	iRet = sensors_create_symlink(data->sig_motion_input_dev);
	if (iRet < 0)
		goto iRet_sig_motion_sysfs_create_link;

	iRet = sensors_create_symlink(data->uncal_gyro_input_dev);
	if (iRet < 0)
		goto iRet_uncal_gyro_sysfs_create_link;

	iRet = sensors_create_symlink(data->grot_vec_input_dev);
	if (iRet < 0)
		goto iRet_grot_vec_sysfs_create_link;

	return SUCCESS;

iRet_grot_vec_sysfs_create_link:
	sensors_remove_symlink(data->uncal_gyro_input_dev);
iRet_uncal_gyro_sysfs_create_link:
	sensors_remove_symlink(data->sig_motion_input_dev);
iRet_sig_motion_sysfs_create_link:
	sensors_remove_symlink(data->step_det_input_dev);
iRet_step_det_sysfs_create_link:
	sensors_remove_symlink(data->uncal_mag_input_dev);
iRet_uncal_mag_sysfs_create_link:
	sensors_remove_symlink(data->mag_input_dev);
iRet_mag_sysfs_create_link:
	sensors_remove_symlink(data->temp_humi_input_dev);
iRet_temp_humi_sysfs_create_link:
	sensors_remove_symlink(data->prox_input_dev);
iRet_prox_sysfs_create_link:
	sensors_remove_symlink(data->light_input_dev);
iRet_light_sysfs_create_link:
	sensors_remove_symlink(data->gesture_input_dev);
iRet_gesture_sysfs_create_link:
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
	sensors_remove_symlink(data->gesture_input_dev);
	sensors_remove_symlink(data->light_input_dev);
	sensors_remove_symlink(data->prox_input_dev);
	sensors_remove_symlink(data->temp_humi_input_dev);
	sensors_remove_symlink(data->mag_input_dev);
	sensors_remove_symlink(data->uncal_mag_input_dev);
	sensors_remove_symlink(data->step_det_input_dev);
	sensors_remove_symlink(data->sig_motion_input_dev);
	sensors_remove_symlink(data->uncal_gyro_input_dev);
	sensors_remove_symlink(data->grot_vec_input_dev);
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
#ifdef CONFIG_SENSORS_SSP_MAX88921
	input_set_capability(data->light_input_dev, EV_REL, REL_RY);
	input_set_capability(data->light_input_dev, EV_REL, REL_RZ);
#endif
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
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_X);
	input_set_abs_params(data->gesture_input_dev, ABS_X, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_Y);
	input_set_abs_params(data->gesture_input_dev, ABS_Y, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(data->gesture_input_dev, ABS_Z, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_RX);
	input_set_abs_params(data->gesture_input_dev, ABS_RX, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_RY);
	input_set_abs_params(data->gesture_input_dev, ABS_RY, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_RZ);
	input_set_abs_params(data->gesture_input_dev, ABS_RZ, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_THROTTLE);
	input_set_abs_params(data->gesture_input_dev, ABS_THROTTLE, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_RUDDER);
	input_set_abs_params(data->gesture_input_dev, ABS_RUDDER, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_WHEEL);
	input_set_abs_params(data->gesture_input_dev, ABS_WHEEL, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_GAS);
	input_set_abs_params(data->gesture_input_dev, ABS_GAS, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_BRAKE);
	input_set_abs_params(data->gesture_input_dev, ABS_BRAKE, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT0X);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT0X, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT0Y);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT0Y, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT1X);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT1X, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT1Y);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT1Y, 0, 1024, 0, 0);

	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT2X);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT2X, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT2Y);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT2Y, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT3X);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT3X, 0, 1024, 0, 0);
	input_set_capability(data->gesture_input_dev, EV_ABS, ABS_HAT3Y);
	input_set_abs_params(data->gesture_input_dev, ABS_HAT3Y, 0, 1024, 0, 0);
	iRet = input_register_device(data->gesture_input_dev);
	if (iRet < 0) {
		input_free_device(data->gesture_input_dev);
		goto err_initialize_gesture_input_dev;
	}
	input_set_drvdata(data->gesture_input_dev, data);

	data->mag_input_dev = input_allocate_device();
	if (data->mag_input_dev == NULL)
		goto err_initialize_mag_input_dev;

	data->mag_input_dev->name = "geomagnetic_sensor";
	input_set_capability(data->mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->mag_input_dev, EV_REL, REL_HWHEEL);
	iRet = input_register_device(data->mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->mag_input_dev);
		goto err_initialize_mag_input_dev;
	}
	input_set_drvdata(data->mag_input_dev, data);

	data->uncal_mag_input_dev = input_allocate_device();
	if (data->uncal_mag_input_dev == NULL)
		goto err_initialize_uncal_mag_input_dev;

	data->uncal_mag_input_dev->name = "uncal_geomagnetic_sensor";
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->uncal_mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncal_mag_input_dev);
		goto err_initialize_uncal_mag_input_dev;
	}
	input_set_drvdata(data->uncal_mag_input_dev, data);

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

	data->uncal_gyro_input_dev = input_allocate_device();
	if (data->uncal_gyro_input_dev == NULL)
		goto err_initialize_uncal_gyro_input_dev;

	data->uncal_gyro_input_dev->name = "uncal_gyro_sensor";
	input_set_capability(data->uncal_gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncal_gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncal_gyro_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncal_gyro_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncal_gyro_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncal_gyro_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->uncal_gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncal_gyro_input_dev);
		goto err_initialize_uncal_gyro_input_dev;
	}
	input_set_drvdata(data->uncal_gyro_input_dev, data);

	data->grot_vec_input_dev = input_allocate_device();
	if (data->grot_vec_input_dev == NULL)
		goto err_initialize_grot_vec_input_dev;

	data->grot_vec_input_dev->name = "game_rotation_vector";
	input_set_capability(data->grot_vec_input_dev, EV_REL, REL_RX);
	input_set_capability(data->grot_vec_input_dev, EV_REL, REL_RY);
	input_set_capability(data->grot_vec_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->grot_vec_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->grot_vec_input_dev, EV_REL, REL_DIAL);
	iRet = input_register_device(data->grot_vec_input_dev);
	if (iRet < 0) {
		input_free_device(data->grot_vec_input_dev);
		goto err_initialize_grot_vec_input_dev;
	}
	input_set_drvdata(data->grot_vec_input_dev, data);

	return SUCCESS;

err_initialize_grot_vec_input_dev:
	pr_err("[SSP]: %s - could not allocate grot vec input device\n", __func__);
	input_unregister_device(data->uncal_gyro_input_dev);
err_initialize_uncal_gyro_input_dev:
	pr_err("[SSP]: %s - could not allocate uncal gyro input device\n", __func__);
	input_unregister_device(data->sig_motion_input_dev);
err_initialize_sig_motion_input_dev:
	pr_err("[SSP]: %s - could not allocate sig motion input device\n", __func__);
	input_unregister_device(data->step_det_input_dev);
err_initialize_step_det_input_dev:
	pr_err("[SSP]: %s - could not allocate step det input device\n", __func__);
	input_unregister_device(data->uncal_mag_input_dev);
err_initialize_uncal_mag_input_dev:
	pr_err("[SSP]: %s - could not allocate uncal mag input device\n", __func__);
	input_unregister_device(data->mag_input_dev);
err_initialize_mag_input_dev:
	pr_err("[SSP]: %s - could not allocate mag input device\n", __func__);
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
	input_unregister_device(data->pressure_input_dev);
	input_unregister_device(data->gesture_input_dev);
	input_unregister_device(data->light_input_dev);
	input_unregister_device(data->prox_input_dev);
	input_unregister_device(data->temp_humi_input_dev);
	input_unregister_device(data->mag_input_dev);
	input_unregister_device(data->uncal_mag_input_dev);
	input_unregister_device(data->step_det_input_dev);
	input_unregister_device(data->sig_motion_input_dev);
	input_unregister_device(data->uncal_gyro_input_dev);
	input_unregister_device(data->grot_vec_input_dev);
}
