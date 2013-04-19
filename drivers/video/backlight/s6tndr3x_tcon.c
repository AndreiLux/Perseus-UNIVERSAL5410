/*
 * s6tndr3x_tcon.c
 * Samsung Mobile S6TNDR3X TCON Device Driver
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/s6tndr3x_tcon.h>

#include "v1-s6tndr3x-tune.h"

#if defined(CONFIG_TCON_TUNING)
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#endif

struct s6tndr3x_data *g_s6tndr3x_data;

static int i2c_read_byte(struct i2c_client *client, u8 page, u8 addr, u8 *data)
{
	int ret = 0;
	int retry = 5;
	struct i2c_msg msgs[2] = {
		{.flags = 0, .buf = &addr, .len = 1},
		{.flags = I2C_M_RD, .buf = data, .len = 1},
	};

	msgs[0].addr = msgs[1].addr = ((client->addr)|(page<<1))>>1;

try_read_byte:
	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret != 2) {
		if (!retry) {
			dev_err(&client->dev, "failed to transfer (tcon-read) i2c data (%d)\n", ret);
			return -EIO;
		}
	retry--;
	goto try_read_byte;
	}
	return 0;
}


static int i2c_write_byte(struct i2c_client *client, u8 page, u8 addr, u8 val)
{
	int ret = 0;
	int retry = 5;
	u8 data[2] = {addr, val};
	struct i2c_msg msgs = {
		.flags = 0, .buf = data, .len = 2,
	};

	msgs.addr = ((client->addr) | (page<<1))>>1;

try_write_byte:
	ret = i2c_transfer(client->adapter, &msgs, 1);
	if (ret != 1) {
		if (!retry) {
			dev_err(&client->dev, "failed to transfer (tcon-write) i2c data (%d)\n", ret);
			return -EIO;
		}
		retry--;
		goto try_write_byte;
	}
	return 0;
}

#if 0
static int check(struct i2c_client *client, struct tcon_reg_info *val, int cnt)
{
	int i;
	int ret;
	int trycnt;
	u8 data;

	for (i = 0; i < cnt; i++) {
		ret = i2c_read_byte(client, val[i].page, val[i].addr, &data);
		printk(KERN_INFO "check: read i2c : %d, p : %x, a : %x, val: %x\n", i, val[i].page, val[i].addr, data);
	}
}
#endif


#define MAX_TUNE_SIZE   50

static int tcon_get_tune_size(struct tcon_reg_info *val)
{
	int ret = 0;
	int i;

	for (i = 0; i < MAX_TUNE_SIZE; i++) {
		if (val[i].page == 0xff)
			break;
		ret++;
	}
	if (ret >= MAX_TUNE_SIZE)
		ret = -EINVAL;

	return ret;
}


static int tcon_write_decay_value(struct i2c_client *client, struct tcon_val *val)
{
	int i;
	int ret;
	int trycnt = 0;
	u8 data;

	if (!client) {
		dev_err(&client->dev, "wrong client\n");
		return -EINVAL;
	}

	for (i = 0; i < MAX_DECAY_TUNE_REG; i++) {
		if (val[i].page == 0xff)
			break;
write_i2c_value:
		ret = i2c_write_byte(client, val[i].page, val[i].addr, val[i].value);
		if (ret) {
			if (trycnt > 5) {
				dev_err(&client->dev, "i2c write failed\n");
				return -EIO;
			}
			trycnt++;
			goto write_i2c_value;
		}
#if 0
		dev_info(&client->dev, "tcon_write_tune_value write addr : %x, data : %x\n", val[i].addr, val[i].value);

		ret = i2c_read_byte(client, val[i].page, val[i].addr, &data);
		if (ret) {
			dev_err(&client->dev, "i2c read failed\n");
			return -EIO;
	}
		dev_info(&client->dev, "tcon_write_tune_value readed addr : %x, data : %x\n", val[i].addr, data);
#endif
	}
	return 0;
}
static int tcon_write_tune_value(struct i2c_client *client, struct tcon_reg_info *val, int cnt)
{
	int i;
	int ret;
	int trycnt;
	u8 data;

    if (!client) {
        dev_err(&client->dev, "wrong client\n");
		return -EINVAL;
    }

	for (i = 0; i < cnt; i++) {
		if (val[i].mask != 0xff) {
			trycnt = 0;
read_i2c_value:
			ret = i2c_read_byte(client, val[i].page, val[i].addr, &data);
			if (ret) {
				if (trycnt > 5) {
					dev_err(&client->dev, "i2c read failed\n");
					return -EIO;
				}
				trycnt++;
				goto read_i2c_value;
			}
/*			
			dev_info(&client->dev, "ret : %d\n", ret);
			dev_info(&client->dev, "tcon_write_tune_value readed addr : %x, data : %x\n", val[i].addr, data);
*/
			data &= ~(val[i].mask);
		}
		data |= val[i].value;
		trycnt = 0;
write_i2c_value:
		ret = i2c_write_byte(client, val[i].page, val[i].addr, data);
		if (ret) {
			if (trycnt > 5) {
				dev_err(&client->dev, "i2c write failed\n");
				return -EIO;
			}
			trycnt++;
			goto write_i2c_value;
		}
		dev_info(&client->dev, "tcon_write_tune_value readed addr : %x, data : %x\n", val[i].addr, data);
	}
	return 0;
}

#if defined(CONFIG_TCON_TUNING)

#define TCON_MAX_TUNE_VALUE 100
#define MAX_FILE_NAME       50
#define TUNING_FILE_PATH    "/sdcard/tuning/"


static int parse_tuning_value(char *src, int len, struct tcon_val *tune_val)
{
	int i, count, ret;
	int index = 0;
	char *str_line[TCON_MAX_TUNE_VALUE];
	char *sstart;
	char *c;
	unsigned int page, reg, data;

	c = src;
	count = 0;
	sstart = c;

	for (i = 0; i < len; i++, c++) {
		char a = *c;
		if ((a == '\r') || (a == '\n')) {
			if (c > sstart) {
				str_line[count] = sstart;
				count++;
			}
			*c = '\0';
			sstart = c+1;
		}
	}

	if (c > sstart) {
		str_line[count] = sstart;
		count++;
	}

	for (i = 0; i < count; i++) {
		ret = sscanf(str_line[i], "0x%x, 0x%x, 0x%x", &page, &reg, &data);
		if (ret == 3) {
			tune_val[index].page = (unsigned char)page;
			tune_val[index].addr = (unsigned char)reg;
			tune_val[index++].value = (unsigned char)data;
		}
	}
	return index;
}

static int load_tuning_value(struct i2c_client *client, char *filename)
{
	int ret;
	int size, i, cnt;
	char *dp = NULL;
	loff_t pos = 0;
	mm_segment_t fs;
	struct file *filp;
	struct tcon_val *tune_val = NULL;

	tune_val = kmalloc(sizeof(struct tcon_val) * TCON_MAX_TUNE_VALUE, GFP_KERNEL);
	if (!tune_val) {
		printk(KERN_ERR "[TCON:ERR]:%s:kmalloc for tune_val failed\n", __func__);
		return -ENOMEM;
	}

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "[TCON:ERR]:%s:file open failed\n", __func__);
		ret = -ENOENT;
		goto load_tuning_value_err1;
	}

	size = filp->f_path.dentry->d_inode->i_size;
	printk(KERN_ERR "[TCON:INFO]:%s:loading file size : %d\n", __func__, size);

	dp = kmalloc(size, GFP_KERNEL);
	if (!dp) {
		printk(KERN_ERR "[TCON:ERR]:%s:kmalloc failed\n", __func__);
		ret = -ENOMEM;
		goto load_tuning_value_err2;
	}
	memset(dp, 0, size);

	ret = vfs_read(filp, (char __user *)dp, size, &pos);
	if (ret != size) {
		printk(KERN_ERR "[TCON:ERR]%s:vfs_read failed ret : %d\n", __func__, ret);
		ret = -EPERM;
		goto load_tuning_value_err3;
	}

	cnt = parse_tuning_value(dp, size, tune_val);
	if (cnt == 0) {
		printk(KERN_ERR "[TCON:ERR]:%s:parse_tuning_value() failed\n", __func__);
		ret = -EINVAL;
		goto load_tuning_value_err3;
	}

	for (i = 0; i < cnt; i++) {
		printk(KERN_ERR "page : %2x, addr : %2x, value : %2x\n",\
			tune_val[i].page, tune_val[i].addr, tune_val[i].value);
		ret = i2c_write_byte(client, tune_val[i].page, tune_val[i].addr, tune_val[i].value);
		if (ret) {
			printk(KERN_ERR "[TCON:ERR]:%s:i2c_write failed\n", __func__);
			ret = -EIO;
			goto load_tuning_value_err3;
		}
	}

	kfree(tune_val);
	kfree(dp);
	filp_close(filp, current->files);
	set_fs(fs);
	return 0;

load_tuning_value_err3:
	kfree(dp);
load_tuning_value_err2:
	filp_close(filp, current->files);
load_tuning_value_err1:
	kfree(tune_val);
	set_fs(fs);
	return ret;
}



static ssize_t show_tcon_tuning(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	int ret = 0;
/*
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);
	struct i2c_client *client = s6tndr3x_data->client;
*/
	return ret;
}


static ssize_t store_tcon_tuning(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
	int ret;
	char filename[MAX_FILE_NAME] = {0,};
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);
	struct i2c_client *client = s6tndr3x_data->client;

	printk(KERN_INFO "[TCON:INFO]:%s:tune file name : %s\n", __func__, buf);

	memset(filename, 0, sizeof(filename));
	strcpy(filename, "/data/tcon/");
	strncat(filename, buf, count-1);

	ret = load_tuning_value(client, filename);
	if (ret)
		printk(KERN_ERR "[TCON:ERR]:%s:load_tuning_value() failed\n", __func__);

	return count;
}


TCON_DEVICE_RW_ATTR(tcon_tuning);



static ssize_t show_tcon_reg(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	int ret = 0;

	return ret;
}

static ssize_t store_tcon_reg(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
	/*
	read :
		echo -n "page_addr, reg_addr" > ./tcon_reg
		echo -n "0x05, 0x07" > ./tcon_reg
	write :
		echo -n "page_addr, reg_addr, value" > ./tcon_reg
		echo -n "0x05, 0x07, 0x08" > ./tcon_reg
	*/
	int ret;
	unsigned char read_data;
	unsigned int page, reg, value;
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);
	struct i2c_client *client = s6tndr3x_data->client;

	ret = sscanf(buf, "0x%x, 0x%x, 0x%x", &page, &reg, &value);

	if (ret == 2) {
		printk(KERN_INFO "Read TCON... page : %x, reg : %x\n", page, reg);
		ret = i2c_read_byte(client, (u8)page, (u8)reg, &read_data);
		if (ret) {
			printk(KERN_ERR "[TCON:ERR]:%s:i2c_read_byte() failed\n",\
				__func__);
			return count;
			}
	printk(KERN_INFO "[TCON:INFO]:%s:Read Done : %x\n", __func__, read_data);
	} else if (ret == 3) {
		printk(KERN_INFO "Write TCON... page : %x, reg : %x, data : %x\n", page, reg, value);
		ret = i2c_write_byte(client, (u8)page, (u8)reg, (u8)value);
		if (ret) {
			printk(KERN_ERR "[TCON:ERR]:%s:i2c_write_byte() failed\n",\
				__func__);
			return count;
		}
		printk(KERN_INFO "[TCON:INFO]:%s:Write Done\n", __func__);
	} else {
		printk(KERN_INFO "[TCON:ERR]:%s:Wrong Command Type\n", __func__);
	}

	return count;
}

TCON_DEVICE_RW_ATTR(tcon_reg);
#endif


int s6tndr3x_tune_value(struct s6tndr3x_data *data)
{
	int ret = 0, size = 0;
    struct tcon_reg_info *tune_value;
    struct s6tndr3x_priv_data *pdata = data->pdata;

    if (!pdata->tcon_ready) {
		dev_err(data->dev, "tcon is not ready.. \n");
		return -EIO;
    }

    dev_info(data->dev, "[set tcon] : mode : %d, br : %d, lux : %d\n", data->mode, data->auto_br, data->lux);

	mutex_lock(&data->lock);

    tune_value = v1_tune_value[data->auto_br][data->lux][data->mode];
    if (!tune_value) {
		dev_err(data->dev, "tcon value is null\n");
		ret = -EINVAL;
		goto tune_value_exit;
    }
	size = tcon_get_tune_size(tune_value);
	if (size < 0) {
		dev_err(data->dev, "tcon tune array size is wrong (%d)\n", size);
		ret = -EINVAL;
		goto tune_value_exit;
	}
	ret = tcon_write_tune_value(data->client, tune_value, size);
	if (ret) {
		dev_err(data->dev, "failed to write tune value to tcon\n");
		goto tune_value_exit;
	}

tune_value_exit:
	g_s6tndr3x_data = data;

	mutex_unlock(&data->lock);

	return ret;
}

int s6tndr3x_tune(u8 ready)
{
	int ret = 0;
	if (ready > 0) {
		g_s6tndr3x_data->pdata->tcon_ready = 1;
		ret = s6tndr3x_tune_value(g_s6tndr3x_data);
	} else {
		g_s6tndr3x_data->pdata->tcon_ready = 0;
	}

	return ret;
}


EXPORT_SYMBOL(s6tndr3x_tune);


int s6tndr3x_tune_decay(unsigned char ratio)
{
	int index;
	int ret = 0;
	struct tcon_val *tune_val;
	struct s6tndr3x_priv_data *pdata;

	if (g_s6tndr3x_data == NULL) {
		return -EINVAL;
	}

	pdata = g_s6tndr3x_data->pdata;
	index = g_s6tndr3x_data->bl_lookup_tbl[ratio / BL_LOOKUP_RES];

	if (index != g_s6tndr3x_data->curr_decay) {
		tune_val = pdata->decay_tune_val[index];
		ret = tcon_write_decay_value(g_s6tndr3x_data->client, tune_val);
		if (ret == 0)
			g_s6tndr3x_data->curr_decay = index;
	}
	return ret;
}
EXPORT_SYMBOL(s6tndr3x_tune_decay);
static ssize_t show_lux(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", s6tndr3x_data->lux);

}

static ssize_t store_lux(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{

    int ret;
	unsigned int value;
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);

    ret = kstrtouint(buf, 10, &value);
    if (ret)
		return ret;

    if (value >= TCON_LEVEL_MAX) {
		dev_err(s6tndr3x_data->dev, "undefied tcon illumiate value : %d\n\n", value);
		return count;
    }

    if (value != s6tndr3x_data->lux) {
		s6tndr3x_data->lux = value-TCON_LEVEL_1;
		ret = s6tndr3x_tune_value(s6tndr3x_data);
		if (ret)
			dev_err(s6tndr3x_data->dev, "failed to tune tcon\n");
	}
	return count;
}

TCON_DEVICE_RW_ATTR(lux);


static ssize_t show_mode(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", s6tndr3x_data->mode);
}

static ssize_t store_mode(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
	int ret;
	unsigned int value;
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);

    ret = kstrtouint(buf, 10, &value);
    if (ret)
		return ret;

    if (value >= TCON_MODE_MAX) {
		dev_err(s6tndr3x_data->dev, "undefied tcon mode value : %d\n\n", value);
		return count;
    }

    if (value != s6tndr3x_data->mode) {
		s6tndr3x_data->mode = value;
		ret = s6tndr3x_tune_value(s6tndr3x_data);
		if (ret)
			dev_err(s6tndr3x_data->dev, "failed to tune tcon\n");
	}
	return count;
}

TCON_DEVICE_RW_ATTR(mode);

static ssize_t show_auto_br(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", s6tndr3x_data->auto_br);
}

static ssize_t store_auto_br(struct device *dev,
			    struct device_attribute *dev_attr,
			    const char *buf, size_t count)
{
    int ret;
    unsigned int value;
    struct s6tndr3x_data *s6tndr3x_data = dev_get_drvdata(dev);

    ret = kstrtouint(buf, 10, &value);
    if (ret)
        return ret;

    if (value >= TCON_AUTO_BR_MAX) {
        dev_err(s6tndr3x_data->dev, "undefied tcon auto br value : %d\n\n", value);
        return count;
    }

    if (value != s6tndr3x_data->auto_br) {
        s6tndr3x_data->auto_br = value;
        ret = s6tndr3x_tune_value(s6tndr3x_data);
        if (ret)
            dev_err(s6tndr3x_data->dev, "failed to tune tcon\n");
    }
    return count;
}


TCON_DEVICE_RW_ATTR(auto_br);


static struct attribute *s6tndr3x_sysfs_attributes[] = {
#if defined(CONFIG_TCON_TUNING)
	&dev_attr_tcon_tuning.attr,
	&dev_attr_tcon_reg.attr,
#endif
	&dev_attr_lux.attr,
	&dev_attr_mode.attr,
	&dev_attr_auto_br.attr,
	NULL
};

static const struct attribute_group s6tndr3x_sysfs_group = {
	.attrs = s6tndr3x_sysfs_attributes,
};


static int __devinit s5tndr3x_probe( struct i2c_client *client,
						const struct i2c_device_id *id)
{
    int i, index = 0;
	int ret = 0;
    static struct class *s5tndr3x_class;
    struct s6tndr3x_data *s6tndr3x_data = NULL;
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct s6tndr3x_priv_data *pdata = client->dev.platform_data;
	
	dev_info(&client->dev, "TCON Start..\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "i2c_check_functionality failed\n");
		return -EIO;
	}

	s6tndr3x_data = kzalloc(sizeof(s6tndr3x_data), GFP_KERNEL);
	if (!s6tndr3x_data) {
		dev_err(&client->dev, "malloc for priv data failed\n");
		return -ENOMEM;
	}
    s6tndr3x_data->pdata = pdata;
    s6tndr3x_data->client = client;

    printk("tcon ready status : %d\n", pdata->tcon_ready);
    
	s6tndr3x_data->mode = 0; /* basic */
	s6tndr3x_data->lux = 0;
	s6tndr3x_data->auto_br = 1;
	if (pdata->decay_cnt) {
		s6tndr3x_data->curr_decay = pdata->decay_cnt;
		
		for (i = 0; i < MAX_LOOKTBL_CNT; i++) {
			printk("i : %d, decay_level : %d\n", i, pdata->decay_level[index]);
			if ((i * BL_LOOKUP_RES) >= pdata->decay_level[index])
				index++;

			if (index >= pdata->decay_cnt) {
				s6tndr3x_data->bl_lookup_tbl[i] = index - 1;
            } else {
				s6tndr3x_data->bl_lookup_tbl[i] = index;
			}
		}
	}
    mutex_init(&s6tndr3x_data->lock);

	i2c_set_clientdata(client, s6tndr3x_data);

	g_s6tndr3x_data = s6tndr3x_data;

	/* sysfs setting */
	s5tndr3x_class = class_create(THIS_MODULE, "tcon");
	if (IS_ERR(s5tndr3x_class)) {
		dev_err(&client->dev, "Failed to create class for TCON\n");
        goto return_mem_free;
    }
  
	s6tndr3x_data->dev = device_create(s5tndr3x_class,\
		NULL, 0, &s6tndr3x_data, "tcon");
	if (IS_ERR(s6tndr3x_data->dev)) {
		dev_err(&client->dev, "Failed to create device for TCON\n");
        goto return_mem_free;
	}   

	ret = sysfs_create_group(&s6tndr3x_data->dev->kobj, &s6tndr3x_sysfs_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs for TCON\n");
		goto return_sysfs_remove;
	}
    dev_set_drvdata(s6tndr3x_data->dev, s6tndr3x_data);
    
	return ret;

return_sysfs_remove:
	sysfs_remove_group(&client->dev.kobj, &s6tndr3x_sysfs_group);

return_mem_free:
	kfree(s6tndr3x_data);
	return ret;
}


static int s5tndr3x_remove(struct i2c_client *client)
{
	struct s6tndr3x_data *s6tndr3x_data = NULL;

	s6tndr3x_data = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &s6tndr3x_sysfs_group);
	kfree(s6tndr3x_data);

	return 0;
}

static const struct i2c_device_id s6tndr3x_id_tbl[] = {
	{"s6tndr3-tcon", 0},
	{}
};

static struct i2c_driver s6tndr3x_tcon = {
	.driver = {
	.name = "s6tndr3-tcon",
	},
	.probe = s5tndr3x_probe,
	.remove = s5tndr3x_remove,
	.shutdown = NULL,
	.id_table = s6tndr3x_id_tbl,
};


#if 0 

static int __init s6tndr3x_tcon_init(void)
{
	printk(KERN_INFO "############s6tndr3x_tcon_init\n");
	return i2c_add_driver(&s6tndr3x_tcon);
}

static void __exit s6tndr3x_tcon_exit(void)
{
	i2c_del_driver(&s6tndr3x_tcon);
}


module_init(s6tndr3x_tcon_init);
module_exit(s6tndr3x_tcon_exit);
#endif

module_i2c_driver(s6tndr3x_tcon);

MODULE_DESCRIPTION("Samsung Mobile S6TNDR3X TCON & Pentile Driver");
MODULE_AUTHOR("minwoo7945.kim@samsung.com");
MODULE_LICENSE("GPL");
