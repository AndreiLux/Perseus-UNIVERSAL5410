/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>

#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c/synaptics_rmi.h>
#include "synaptics_i2c_rmi.h"

static void set_default_result(struct synaptics_rmi4_data *data)
{
	char delim = ':';
	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	memset(data->cmd_result, 0x00, ARRAY_SIZE(data->cmd_result));
	memcpy(data->cmd_result, data->cmd, strlen(data->cmd));
	strncat(data->cmd_result, &delim, 1);
}

static void set_cmd_result(struct synaptics_rmi4_data *data,
				char *buff, int len)
{
	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	strncat(data->cmd_result, buff, len);
}

static void not_support_cmd(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[16] = {0};
	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	set_default_result(data);
	snprintf(buff, sizeof(buff), "%s", "NA");
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_NOT_APPLICABLE;
	dev_info(&client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
}

#define SYNAPTICS_FW "/sdcard/synaptics.fw"

static int synaptics_load_fw_from_ums(struct synaptics_rmi4_data *data)
{
	struct file *fp;
	mm_segment_t old_fs;
	unsigned short fw_size, nread;
	int error = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(SYNAPTICS_FW, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&data->i2c_client->dev,
			"%s: failed to open %s.\n", __func__, SYNAPTICS_FW);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,
			fw_size, &fp->f_pos);
		dev_info(&data->i2c_client->dev,
			"%s: start, file path %s, size %u Bytes\n", __func__,
		       SYNAPTICS_FW, fw_size);
		if (nread != fw_size) {
			dev_err(&data->i2c_client->dev,
			       "%s: failed to read firmware file, nread %u Bytes\n",
			       __func__,
			       nread);
			error = -EIO;
		} else
			synaptics_fw_updater(fw_data, true);

		kfree(fw_data);
	}

	filp_close(fp, current->files);
 open_err:
	set_fs(old_fs);
	return error;
}

static void fw_update(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct device *dev = &data->i2c_client->dev;

	int retval;
	char buff[16] = {0};

	set_default_result(data);

	switch (data->cmd_param[0]) {

	case 1:
		retval = synaptics_load_fw_from_ums(data);
		msleep(1000);
		break;
	case 0:
		retval = synaptics_fw_updater(NULL, true);
		msleep(1000);
		break;
	}

	if (retval < 0) {
		snprintf(buff, sizeof(buff), "FAIL");
		set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
		data->cmd_state = CMD_STATUS_FAIL;
		dev_err(dev, "%s: failed, =%d\n",
			__func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "OK");
		set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
		data->cmd_state = CMD_STATUS_OK;
		dev_info(dev, "%s: success, =%d\n",
			__func__, retval);
	}

	dev_info(dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct device *dev = &data->i2c_client->dev;

	char buff[40] = {0};

	set_default_result(data);

	snprintf(buff, sizeof(buff), "SY%02X%02X",
			data->ic_revision, data->fw_ver_bin);

	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;

	dev_info(dev, "%s: %s(%04x)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[40] = {0};

	set_default_result(data);

	snprintf(buff, sizeof(buff), "SY%02X%02X",
			data->ic_revision, data->fw_ver_ic);

	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%04x)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[40] = {0};

	set_default_result(data);

	snprintf(buff, sizeof(buff), "%s_SY_%02d%02d", SYNAPTICS_DEVICE_NAME,
		data->fw_release_date >> 8, data->fw_release_date & 0x00FF);

	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_threshold(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[16] = {0};
/*
	u8 threshold = 0x40;
*/
	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	set_default_result(data);
/*
	if (threshold < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
		data->cmd_state = CMD_STATUS_FAIL;
		return;
	}
*/
	snprintf(buff, sizeof(buff), "%d", data->touch_threshold);

	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void module_off_master(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[3] = {0};

	mutex_lock(&data->cmd_lock);

	if (data->board->power(false))
		snprintf(buff, sizeof(buff), "%s", "NG");
	else
		snprintf(buff, sizeof(buff), "%s", "OK");

	mutex_unlock(&data->cmd_lock);

	set_default_result(data);
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		data->cmd_state = CMD_STATUS_OK;
	else
		data->cmd_state = CMD_STATUS_FAIL;

	dev_info(&client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[3] = {0};

	mutex_lock(&data->cmd_lock);

	if (data->board->power(true))
		snprintf(buff, sizeof(buff), "%s", "NG");
	else
		snprintf(buff, sizeof(buff), "%s", "OK");

	mutex_unlock(&data->cmd_lock);

	set_default_result(data);
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		data->cmd_state = CMD_STATUS_OK;
	else
		data->cmd_state = CMD_STATUS_FAIL;

	dev_info(&client->dev, "%s: %s\n", __func__, buff);

}

static void get_chip_vendor(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[16] = {0};

	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	set_default_result(data);

	snprintf(buff, sizeof(buff), "%s", "SYNAPTICS");
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[16] = {0};

	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	set_default_result(data);

	snprintf(buff, sizeof(buff), "%s", "S5000");
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;
	dev_info(&client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_x_num(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[16] = {0};

	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	set_default_result(data);

	snprintf(buff, sizeof(buff), "%u", data->num_of_tx);
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	struct i2c_client *client = data->i2c_client;

	char buff[16] = {0};

	dev_info(&data->i2c_client->dev, "%s\n", __func__);

	set_default_result(data);

	snprintf(buff, sizeof(buff), "%u", data->num_of_rx);
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static int check_rx_tx_num(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	char buff[TSP_CMD_STR_LEN] = {0};
	int node;

	dev_info(&data->i2c_client->dev, "%s: param[0] = %d, param[1] = %d\n",
			__func__, data->cmd_param[0], data->cmd_param[1]);

	if (data->cmd_param[0] < 0 ||
			data->cmd_param[0] >= data->num_of_tx ||
			data->cmd_param[1] < 0 ||
			data->cmd_param[1] >= data->num_of_rx) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
		data->cmd_state = CMD_STATUS_FAIL;

		dev_info(&data->i2c_client->dev, "%s: parameter error: %u,%u\n",
				__func__, data->cmd_param[0],
				data->cmd_param[1]);
		node = -1;
	} else {
		node = data->cmd_param[0] * data->num_of_tx +
						data->cmd_param[1];
		dev_info(&data->i2c_client->dev, "%s: node = %d\n", __func__,
						node);
		}
	return node;

}


static void run_rawcap_read(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	int i, ret, ret1, min_raw_cap = 32767, max_raw_cap = 0;
	char buff[16] = {0};
	int retry = 100;

	printk(KERN_ERR "%s\n", __func__);

	set_default_result(data);

	ret = synaptics_rmi4_f54_report_type(CMD_REPORT_TYPE_RAWCAP);
	ret1 = synaptics_rmi4_f54_get_report(CMD_GET_REPORT);

	if (ret < 0 || ret1 < 0) {
		dev_info(&data->i2c_client->dev,
			"%s: don't read rawcap\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
		data->cmd_state = CMD_STATUS_FAIL;
	} else {
		while (retry--) {
			if (data->read_rawcap_done == true)
				break;
			dev_info(&data->i2c_client->dev, "..\n");
			msleep(30);
		}

		for (i = 0; i < data->num_of_node; i++) {
			min_raw_cap = min(min_raw_cap, data->raw_cap_data[i]);
			max_raw_cap = max(max_raw_cap, data->raw_cap_data[i]);
		}
		dev_info(&data->i2c_client->dev, "%s: min =  %d max = %d\n",
				__func__, min_raw_cap, max_raw_cap);

		snprintf(buff, sizeof(buff), "%d,%d",
			min_raw_cap, max_raw_cap);
		set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));

		data->cmd_state = CMD_STATUS_OK;
	}
}

static void get_rawcap(void *device_data)
{
	struct synaptics_rmi4_data *data =
			(struct synaptics_rmi4_data *)device_data;
	char buff[16] = {0};
	int node;

	printk(KERN_ERR "%s\n", __func__);
	set_default_result(data);

	node = check_rx_tx_num(data);

	if (node < 0)
		return;

	snprintf(buff, sizeof(buff), "%u", data->raw_cap_data[node]);
	set_cmd_result(data, buff, strnlen(buff, sizeof(buff)));
	data->cmd_state = CMD_STATUS_OK;

	dev_info(&data->i2c_client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}
/* - function realted samsung factory test */

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char		*cmd_name;
	void			(*cmd_func)(void *device_data);
};

static struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("run_rawcap_read", run_rawcap_read),},
	{TSP_CMD("get_rawcap", get_rawcap),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};

/* Functions related to basic interface */
static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->i2c_client;

	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (data->cmd_is_running == true) {
		dev_err(&client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = true;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = CMD_STATUS_RUNNING;

	for (i = 0; i < ARRAY_SIZE(data->cmd_param); i++)
		data->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(data->cmd, 0x00, ARRAY_SIZE(data->cmd));
	memcpy(data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	dev_info(dev, "%s ==> %s\n", __func__, buff);
	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &data->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr,
			&data->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				ret = kstrtoint(buff, 10,
					data->cmd_param + param_cnt);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n",
			i, data->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(data);
err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);

	char buff[16] = {0};

	dev_info(&data->i2c_client->dev, "tsp cmd: status:%d\n",
			data->cmd_state);

	if (data->cmd_state == CMD_STATUS_WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (data->cmd_state == CMD_STATUS_RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (data->cmd_state == CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (data->cmd_state == CMD_STATUS_FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (data->cmd_state == CMD_STATUS_NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
				    *devattr, char *buf)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);

	dev_info(&data->i2c_client->dev,
		"tsp cmd: result: %s\n", data->cmd_result);

	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = false;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = CMD_STATUS_WAITING;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", data->cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

int __devinit synpatics_sysfs_init(struct synaptics_rmi4_data *ddata)
{
	int i;
	int ret;
	struct synaptics_rmi4_data *data = ddata;

	INIT_LIST_HEAD(&data->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &data->cmd_list_head);

	mutex_init(&data->cmd_lock);
	data->cmd_is_running = false;


	data->fac_dev_ts = device_create(sec_class,
			NULL, 0, data, "tsp");

	ret = IS_ERR(data->fac_dev_ts);
	if (ret) {
		dev_err(&data->i2c_client->dev, "%s: Failed to create device for the sysfs\n",
				__func__);
		ret = IS_ERR(data->fac_dev_ts);
		goto free_mem;
	}
	ret = sysfs_create_group(&data->fac_dev_ts->kobj,
				&touchscreen_attr_group);
	if (ret) {
		dev_err(&data->i2c_client->dev, "%s: Failed to create touchscreen sysfs group\n",
				__func__);
		goto free_mem;
	}

	return 0;

free_mem:
	kfree(data);
	return ret;
}

