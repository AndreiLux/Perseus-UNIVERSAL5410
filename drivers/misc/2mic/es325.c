/*
 * drivers/misc/es325.c - Audience ES325 Voice Processor driver
 *
 * Copyright (C) 2012 Google, Inc.
 * Copyright (C) 2012 Samsung Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_data/es325.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/clk.h>

/* To support sysfs node  */
static struct class *ns_class;
static struct device *es325_dev;

#define ES325_FIRMWARE_NAME		"es325_fw.bin"
#define MAX_CMD_SEND_SIZE		1024
#define RETRY_COUNT			5

/* ES325 commands and values */
#define ES325_BOOT			0x0001
#define ES325_BOOT_ACK			0x0001

#define ES325_VEQ_DISABLE		0x0000
#define ES325_VEQ_ENABLE		0x0001

#define ES325_SYNC_POLLING		0x80000000

#define ES325_RESET_IMMEDIATE		0x80020000

#define ES325_SET_POWER_STATE_SLEEP	0x80100001

#define ES325_GET_ALGORITHM_PARM	0x80160000
#define ES325_SET_ALGORITHM_PARM_ID	0x80170000
#define ES325_SET_ALGORITHM_PARM	0x80180000
#define ES325_AEC_MODE			0x0003
#define ES325_TX_AGC			0x0004
#define ES325_VEQ_MODE			0x0009
#define ES325_VEQ_NOISE_ESTIMATE_ADJ	0x0025
#define ES325_RX_AGC			0x0028
#define ES325_VEQ_MAX_GAIN		0x003D
#define ES325_TX_NS_LEVEL		0x004B
#define ES325_RX_NS_LEVEL		0x004C
#define ES325_ALGORITHM_RESET		0x001C

#define ES325_GET_VOICE_PROCESSING	0x80430000
#define ES325_SET_VOICE_PROCESSING	0x801C0000

#define ES325_GET_AUDIO_ROUTING		0x80270000
#define ES325_SET_AUDIO_ROUTING		0x80260000

#define ES325_SET_PRESET		0x80310000

#define ES325_GET_MIC_SAMPLE_RATE	0x80500000
#define ES325_SET_MIC_SAMPLE_RATE	0x80510000
#define ES325_8KHZ			0x0008
#define ES325_16KHZ			0x000A
#define ES325_48KHZ			0x0030

#define ES325_DIGITAL_PASSTHROUGH	0x80520000

struct es325_data {
	struct es325_platform_data	*pdata;
	struct device			*dev;
	struct i2c_client		*client;
	const struct firmware		*fw;
	struct mutex			lock;
	u32				passthrough[2];
	bool				asleep;
	bool				device_ready;
	struct clk			*clk;
};

static int es325_send_cmd(struct es325_data *es325, u32 command, u16 *response)
{
	u8 send[4];
	u8 recv[4];
	int ret = 0;
	int retry = RETRY_COUNT;

	send[0] = (command >> 24) & 0xff;
	send[1] = (command >> 16) & 0xff;
	send[2] = (command >> 8) & 0xff;
	send[3] = command & 0xff;

	ret = i2c_master_send(es325->client, send, 4);
	if (ret < 0) {
		dev_err(es325->dev, "i2c_master_send failed ret = %d\n", ret);
		return ret;
	}

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == ES325_SET_POWER_STATE_SLEEP)
		return ret;
	else if (command == ES325_SYNC_POLLING)
		usleep_range(1000, 2000);
	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	else
		usleep_range(10000, 11000);

	while (retry--) {
		ret = i2c_master_recv(es325->client, recv, 4);
		if (ret < 0) {
			dev_err(es325->dev, "i2c_master_recv failed\n");
			return ret;
		}
		/*
		 * Check that the first two bytes of the response match
		 * (the ack is in those bytes)
		 */
		if ((send[0] == recv[0]) && (send[1] == recv[1])) {
			if (response)
				*response = (recv[2] << 8) | recv[3];
			ret = 0;
			break;
		} else {
			dev_err(es325->dev,
				"incorrect ack (got 0x%.2x%.2x)\n",
				recv[0], recv[1]);
			ret = -EINVAL;
		}

		/* Wait before polling again */
		if (retry > 0)
			msleep(20);
	}

	return ret;
}

static int es325_load_firmware(struct es325_data *es325)
{
	int ret = 0;
	const u8 *i2c_cmds;
	int size;

	i2c_cmds = es325->fw->data;
	size = es325->fw->size;

	dev_info(es325->dev, "%s:++\n", __func__);
	while (size > 0) {
		ret = i2c_master_send(es325->client, i2c_cmds,
				min(size, MAX_CMD_SEND_SIZE));
		if (ret < 0) {
			dev_err(es325->dev, "i2c_master_send failed ret=%d\n",
				ret);
			break;
		}
		size -= MAX_CMD_SEND_SIZE;
		i2c_cmds += MAX_CMD_SEND_SIZE;
	}

	dev_info(es325->dev, "%s:--\n", __func__);
	return ret;
}

static int es325_reset(struct es325_data *es325)
{
	int ret = 0;
	struct es325_platform_data *pdata = es325->pdata;
	static const u8 boot[2] = {ES325_BOOT >> 8, ES325_BOOT};
	u8 ack;
	int retry = RETRY_COUNT;

	dev_info(es325->dev, "%s:++\n", __func__);

	while (retry--) {
		/* Reset ES325 chip */
		gpio_set_value(pdata->gpio_reset, 0);
		usleep_range(200, 400);
		gpio_set_value(pdata->gpio_reset, 1);

		/* Delay before sending i2c commands */
		usleep_range(5000, 6000);

		/*
		 * Send boot command and check response. The boot command
		 * is different from the others in that it's only 2 bytes,
		 * and the ack retry mechanism is different too.
		 */
		ret = i2c_master_send(es325->client, boot, 2);
		if (ret < 0) {
			dev_err(es325->dev, "i2c_master_send failed ret=%d\n",
				ret);
			continue;
		}
		usleep_range(100, 200);
		ret = i2c_master_recv(es325->client, &ack, 1);
		if (ret < 0) {
			dev_err(es325->dev, "i2c_master_recv failed\n");
			continue;
		}
		if (ack != ES325_BOOT_ACK) {
			dev_err(es325->dev, "boot ack incorrect (got 0x%.2x)\n",
									ack);
			continue;
		}

		ret = es325_load_firmware(es325);
		if (ret < 0) {
			dev_err(es325->dev, "load firmware error\n");
			continue;
		}

		/* Delay before issuing a sync command */
		msleep(20);

		ret = es325_send_cmd(es325, ES325_SYNC_POLLING, NULL);
		if (ret < 0) {
			dev_err(es325->dev, "sync error\n");
			continue;
		}

		break;
	}

	return ret;
}

static int es325_sleep(struct es325_data *es325)
{
	int ret = 0;
	struct es325_platform_data *pdata = es325->pdata;

	if (!pdata->use_sleep)
		return 0;

	if (es325->asleep)
		return ret;

	ret = es325_send_cmd(es325, ES325_SET_POWER_STATE_SLEEP, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set power state error\n");
		return ret;
	}

	/* The clock can be disabled after the device has had time to sleep */
	if (!es325->device_ready) /* boot case */
		msleep(20);
	else /* normal case */
		msleep(120);

	pdata->clk_enable(0, 0);

	es325->asleep = true;

	dev_info(es325->dev, "%s: go into sleep status\n", __func__);
	return ret;
}

static int es325_wake(struct es325_data *es325)
{
	int ret = 0;
	struct es325_platform_data *pdata = es325->pdata;

	if (!es325->asleep)
		return ret;

	pdata->clk_enable(1, 0);
	gpio_set_value(pdata->gpio_wakeup, 0);
	msleep(30);

	ret = es325_send_cmd(es325, ES325_SYNC_POLLING, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "sync error\n");

		/* Go back to sleep */
		pdata->clk_enable(0, 0);
		gpio_set_value(pdata->gpio_wakeup, 1);
		return ret;
	}

	gpio_set_value(pdata->gpio_wakeup, 1);
	es325->asleep = false;

	dev_info(es325->dev, "%s: wake up\n", __func__);
	return ret;
}

static int es325_set_passthrough(struct es325_data *es325, u32 *path)
{
	int ret;

	dev_info(es325->dev, "%s:++ bypass on\n", __func__);

	ret = es325_send_cmd(es325, ES325_DIGITAL_PASSTHROUGH | *path, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set passthrough error\n");
		return ret;
	}

	ret = es325_send_cmd(es325,
			ES325_DIGITAL_PASSTHROUGH | *(path + 1), NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set passthrough error\n");
		return ret;
	}

	/* The passthrough command must be followed immediately by sleep */
	ret = es325_sleep(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to sleep\n");
		return ret;
	}

	return ret;
}

static int es325_set_veq_parm(struct es325_data *es325, u32 parm, s16 val)
{
	int ret = 0;
	bool check_val = true;

	switch (parm) {
	case ES325_VEQ_MODE:
		dev_info(es325->dev, "set veq_mode [%d]\n", val);

		if (val > 1)
			check_val = false;
		break;

	case ES325_VEQ_NOISE_ESTIMATE_ADJ:
		dev_info(es325->dev, "set veq_noise_estimate_adj [%d] [0x%x]\n",
						val, ((u32)val & 0xFFFF));

		if ((val < -30) || (val > 30))
			check_val = false;
		break;

	case ES325_VEQ_MAX_GAIN:
		dev_info(es325->dev, "set veq_max_gain [%d]\n", val);

		if (val > 15)
			check_val = false;
		break;

	default:
		check_val = false;
		break;
	}

	if (!check_val) {
		dev_err(es325->dev, "invalid value\n");
		return -EINVAL;
	}

	ret = es325_send_cmd(es325, ES325_SET_ALGORITHM_PARM_ID | parm, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set algorithm parm id error\n");
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_SET_ALGORITHM_PARM |
						((u32)val & 0xFFFF), NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set algorithm parm error\n");
		return ret;
	}

	return ret;
}

static ssize_t es325_audio_routing_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	int ret;
	u16 val;

	if (!es325->device_ready)
		return -EAGAIN;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_GET_AUDIO_ROUTING, &val);
	if (ret < 0) {
		dev_err(es325->dev, "get audio routing error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return sprintf(buf, "%u\n", val);
}

static ssize_t es325_audio_routing_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!es325->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_SET_AUDIO_ROUTING | (u32)val, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set audio routing error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return size;
}

static ssize_t es325_preset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	dev_info(es325->dev, "%s:++\n", __func__);
	if (!es325->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	dev_info(es325->dev, "set preset [%d]\n",(u32)val);
	
	ret = es325_send_cmd(es325, ES325_SET_PRESET | (u32)val, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set preset error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return size;
}

static ssize_t es325_voice_processing_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	int ret;
	u16 val;

	if (!es325->device_ready)
		return -EAGAIN;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_GET_VOICE_PROCESSING, &val);
	if (ret < 0) {
		dev_err(es325->dev, "get voice processing error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return sprintf(buf, "%u\n", val);
}

static ssize_t es325_voice_processing_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!es325->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_SET_VOICE_PROCESSING | (u32)val,
									NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set voice processing error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return size;
}

static ssize_t es325_sleep_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct es325_data *es325 = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", es325->asleep ? 1 : 0);
}

static ssize_t es325_sleep_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	unsigned long state;
	int ret;

	if (!es325->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &state);
	if (ret)
		return -EINVAL;

	dev_info(es325->dev, "%s:++ state=%d\n", __func__, (u32)state);
	/* requested sleep state is different to current state */
	mutex_lock(&es325->lock);
	if (state)
		ret = es325_set_passthrough(es325, es325->passthrough);
	else
		ret = es325_wake(es325);

	if (ret < 0) {
		dev_err(es325->dev, "unable to change sleep state\n");
		mutex_unlock(&es325->lock);
		return ret;
	}
	mutex_unlock(&es325->lock);

	return size;
}

static ssize_t es325_algorithm_parm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	int ret;
	u16 val;
	u32 parm;

	if (!es325->device_ready)
		return -EAGAIN;

	if (strcmp(attr->attr.name, "aec_enable") == 0)
		parm = ES325_AEC_MODE;
	else if (strcmp(attr->attr.name, "tx_agc_enable") == 0)
		parm = ES325_TX_AGC;
	else if (strcmp(attr->attr.name, "rx_agc_enable") == 0)
		parm = ES325_RX_AGC;
	else if (strcmp(attr->attr.name, "tx_ns_level") == 0)
		parm = ES325_TX_NS_LEVEL;
	else if (strcmp(attr->attr.name, "rx_ns_level") == 0)
		parm = ES325_RX_NS_LEVEL;
	else
		return -EINVAL;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_GET_ALGORITHM_PARM | parm, &val);
	if (ret < 0) {
		dev_err(es325->dev, "get algorithm parm error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return sprintf(buf, "%u\n", val);
}

static ssize_t es325_algorithm_parm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	unsigned long val;
	unsigned long val_max = 1;
	int ret;
	u32 parm;

	if (!es325->device_ready)
		return -EAGAIN;

	if (strcmp(attr->attr.name, "aec_enable") == 0) {
		parm = ES325_AEC_MODE;
	} else if (strcmp(attr->attr.name, "tx_agc_enable") == 0) {
		parm = ES325_TX_AGC;
	} else if (strcmp(attr->attr.name, "rx_agc_enable") == 0) {
		parm = ES325_RX_AGC;
	} else if (strcmp(attr->attr.name, "tx_ns_level") == 0) {
		parm = ES325_TX_NS_LEVEL;
		val_max = 10;
	} else if (strcmp(attr->attr.name, "rx_ns_level") == 0) {
		parm = ES325_RX_NS_LEVEL;
		val_max = 10;
	} else if (strcmp(attr->attr.name, "algorithm_reset") == 0) {
		parm = ES325_ALGORITHM_RESET;
		val_max = 0xFFFF;
	} else {
		return -EINVAL;
	}

	/* Check that a valid value was obtained */
	ret = kstrtoul(buf, 0, &val);
	if (ret || (val > val_max))
		return -EINVAL;

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	ret = es325_send_cmd(es325, ES325_SET_ALGORITHM_PARM_ID | parm, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set algorithm parm id error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}
	ret = es325_send_cmd(es325, ES325_SET_ALGORITHM_PARM | (u32)val, NULL);
	if (ret < 0) {
		dev_err(es325->dev, "set algorithm parm error\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mutex_unlock(&es325->lock);
	return size;
}

static ssize_t es325_veq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct es325_data *es325 = dev_get_drvdata(dev);
	struct es325_platform_data *pdata = es325->pdata;
	struct es325_veq_parm veq;
	unsigned long val;
	s16 mode;
	int ret;

	dev_info(es325->dev, "%s:++\n", __func__);

	if (!es325->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret || ((u8)val >= pdata->veq_parm_table_size)) {
		dev_err(es325->dev, "invalid value\n");
		return -EINVAL;
	}

	dev_info(es325->dev, "set veq [%d]\n", (u8)val);

	mutex_lock(&es325->lock);
	ret = es325_wake(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to wake\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	mode = (!val) ? ES325_VEQ_DISABLE : ES325_VEQ_ENABLE;

	ret = es325_set_veq_parm(es325, ES325_VEQ_MODE, mode);
	if (ret < 0) {
		dev_err(es325->dev, "unable to set veq_mode\n");
		mutex_unlock(&es325->lock);
		return ret;
	}

	if (mode && pdata->veq_parm_table) {
		veq = pdata->veq_parm_table[(u8)val];

		ret = es325_set_veq_parm(es325, ES325_VEQ_NOISE_ESTIMATE_ADJ,
							veq.noise_estimate_adj);
		if (ret < 0) {
			dev_err(es325->dev,
				"unable to set veq_noise_estimate_adj\n");
			mutex_unlock(&es325->lock);
			return ret;
		}

		ret = es325_set_veq_parm(es325, ES325_VEQ_MAX_GAIN,
								veq.max_gain);
		if (ret < 0) {
			dev_err(es325->dev, "unable to set veq_max_gain\n");
			mutex_unlock(&es325->lock);
			return ret;
		}
	}
	mutex_unlock(&es325->lock);

	return size;
}


static DEVICE_ATTR(audio_routing, S_IRUGO | S_IWUSR,
		es325_audio_routing_show, es325_audio_routing_store);
static DEVICE_ATTR(preset, S_IWUSR,
		NULL, es325_preset_store);
static DEVICE_ATTR(voice_processing, S_IRUGO | S_IWUSR,
		es325_voice_processing_show, es325_voice_processing_store);
static DEVICE_ATTR(sleep, S_IRUGO | S_IWUSR,
		es325_sleep_show, es325_sleep_store);
static DEVICE_ATTR(veq, S_IWUSR,
		NULL, es325_veq_store);
static DEVICE_ATTR(aec_enable, S_IRUGO | S_IWUSR,
		es325_algorithm_parm_show, es325_algorithm_parm_store);
static DEVICE_ATTR(tx_agc_enable, S_IRUGO | S_IWUSR,
		es325_algorithm_parm_show, es325_algorithm_parm_store);
static DEVICE_ATTR(rx_agc_enable, S_IRUGO | S_IWUSR,
		es325_algorithm_parm_show, es325_algorithm_parm_store);
static DEVICE_ATTR(tx_ns_level, S_IRUGO | S_IWUSR,
		es325_algorithm_parm_show, es325_algorithm_parm_store);
static DEVICE_ATTR(rx_ns_level, S_IRUGO | S_IWUSR,
		es325_algorithm_parm_show, es325_algorithm_parm_store);
static DEVICE_ATTR(algorithm_reset, S_IWUSR,
		NULL, es325_algorithm_parm_store);

static struct attribute *es325_attributes[] = {
	&dev_attr_audio_routing.attr,
	&dev_attr_preset.attr,
	&dev_attr_voice_processing.attr,
	&dev_attr_sleep.attr,
	&dev_attr_veq.attr,
	&dev_attr_aec_enable.attr,
	&dev_attr_tx_agc_enable.attr,
	&dev_attr_rx_agc_enable.attr,
	&dev_attr_tx_ns_level.attr,
	&dev_attr_rx_ns_level.attr,
	&dev_attr_algorithm_reset.attr,
	NULL,
};

static const struct attribute_group es325_attribute_group = {
	.attrs = es325_attributes,
};

/*
 * This is the callback function passed to request_firmware_nowait(),
 * and will be called as soon as the firmware is ready.
 */
static void es325_firmware_ready(const struct firmware *fw, void *context)
{
	struct es325_data *es325 = (struct es325_data *)context;
	struct es325_platform_data *pdata = es325->pdata;
	int ret;

	dev_info(es325->dev, "%s:++\n", __func__);
	if (!fw) {
		dev_err(es325->dev, "firmware request failed\n");
		return;
	}
	es325->fw = fw;

	pdata->clk_enable(1, 0);

	ret = es325_reset(es325);
	if (ret < 0) {
		dev_err(es325->dev, "unable to reset device\n");
		goto err;
	}

	/* Enable passthrough if needed */
	ret = es325_set_passthrough(es325, es325->passthrough);
	if (ret < 0) {
		dev_err(es325->dev, "unable to enable digital passthrough\n");
		goto err;
	}

	es325->device_ready = true;

	dev_info(es325->dev, "%s:--\n", __func__);
err:
	release_firmware(es325->fw);
	es325->fw = NULL;
}

static int __devinit es325_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct es325_data *es325;
	struct es325_platform_data *pdata = client->dev.platform_data;
	int ret = 0;
	int i;

	es325 = kzalloc(sizeof(*es325), GFP_KERNEL);
	if (es325 == NULL) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	es325->client = client;
	i2c_set_clientdata(client, es325);

	es325->dev = &client->dev;
	dev_set_drvdata(es325->dev, es325);

	es325->pdata = pdata;

	for (i = 0; i < PASSTHROUGH_NUM; i++) {
		if ((pdata->pass[i].src < 0) || (pdata->pass[i].src > 4) ||
						(pdata->pass[i].dst < 0) ||
						(pdata->pass[i].dst > 4) ||
						!pdata->clk_enable) {
			dev_err(es325->dev, "invalid pdata\n");
			ret = -EINVAL;
			goto err_pdata;
		} else if ((pdata->pass[i].src != 0) &&
						(pdata->pass[i].dst != 0)) {
			es325->passthrough[i] = ((pdata->pass[i].src + 3) << 4)
					| ((pdata->pass[i].dst - 1) << 2);
		}
	}

	ret = gpio_request(pdata->gpio_wakeup, "ES325 wakeup");
	if (ret < 0) {
		dev_err(es325->dev, "error requesting wakeup gpio\n");
		goto err_gpio_wakeup;
	}
	gpio_direction_output(pdata->gpio_wakeup, 1);

	ret = gpio_request(pdata->gpio_reset, "ES325 reset");
	if (ret < 0) {
		dev_err(es325->dev, "error requesting reset gpio\n");
		goto err_gpio_reset;
	}
	gpio_direction_output(pdata->gpio_reset, 0);

	mutex_init(&es325->lock);

	dev_info(es325->dev, "request_firmware\n");
	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				ES325_FIRMWARE_NAME, es325->dev, GFP_KERNEL,
				es325, es325_firmware_ready);
#if 0
	/*
	 * This is not a critical failure, since the device can still be left
	 * in passthrough mode.
	 */
	ret = sysfs_create_group(&es325->dev->kobj, &es325_attribute_group);
	if (ret)
		dev_err(es325->dev, "failed to create sysfs group\n");
#endif

	/* To support sysfs node */
	ns_class = class_create(THIS_MODULE, "2mic");

	if (IS_ERR(ns_class))
		dev_err(es325->dev, "Failed to create class\n");

	es325_dev = device_create(ns_class, NULL, 0, es325, "es325");

	ret = sysfs_create_group(&es325_dev->kobj, &es325_attribute_group);
	if (ret)
		dev_err(es325_dev, "failed to create sysfs group\n");

	return 0;

err_gpio_reset:
	gpio_free(pdata->gpio_wakeup);
err_gpio_wakeup:
err_pdata:
	kfree(es325);
err_kzalloc:
	return ret;
}

static int __devexit es325_remove(struct i2c_client *client)
{
	struct es325_data *es325 = i2c_get_clientdata(client);

	sysfs_remove_group(&es325->dev->kobj, &es325_attribute_group);

	i2c_set_clientdata(client, NULL);

	gpio_free(es325->pdata->gpio_wakeup);
	gpio_free(es325->pdata->gpio_reset);
	kfree(es325);

	return 0;
}

static const struct i2c_device_id es325_id[] = {
	{"audience_es325", 0},
	{},
};

static struct i2c_driver es325_driver = {
	.driver = {
		.name = "audience_es325",
		.owner = THIS_MODULE,
	},
	.probe = es325_probe,
	.remove = __devexit_p(es325_remove),
	.id_table = es325_id,
};

static int __init es325_init(void)
{
	return i2c_add_driver(&es325_driver);
}

static void __exit es325_exit(void)
{
	i2c_del_driver(&es325_driver);
}

module_init(es325_init);
module_exit(es325_exit);

MODULE_DESCRIPTION("Audience ES325 Voice Processor driver");
MODULE_LICENSE("GPL");
