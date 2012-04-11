/*
 * stm_fw.c
 *
 * System Trace Module (STM) Framework Driver Implementation
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "stm_fw.h"
#include "stm_msg.h"

/*****************************************************************************
 * Local definitions and statics variables
 *****************************************************************************/
#define MAX_SIZE_FILENAME       40
#define MAX_CONFIG_STRING_BYTES 64

struct stm_chn_descriptor {
	int chn;		/* Only valid if write_dentry is not NULL */
	bool chn_enable;
	struct dentry *write_dentry; /* NULL if not a user mode channel */
	struct dentry *config_dentry; /* Always set for any chan type */
	bool  write_protect;	   /* Protect writes to the stm channel */
	struct mutex lock;
	enum msg_formats msg_format;
	enum msg_types msg_type;
	bool timestamp_enable;
	bool checksum_enable;
	bool ost_enable;
	char ost_entity;
	char ost_protocol;
	struct list_head chn_list_link;
	char chn_filename[MAX_SIZE_FILENAME];
	char cfg_filename[MAX_SIZE_FILENAME];
};

struct stm_device {
	struct mutex lock;
	struct stm_operations *stm_ops;  /* If NULL all elements are invalid */
	struct dentry *parent;
	struct dentry *request;
	struct list_head chn_list_head; /* List HEAD of stm_chn_descriptors */
};

static struct stm_device dev;

/*****************************************************************************
 * Support function prototypes
 *****************************************************************************/
static uint8_t cal_checksum(char *buf, int cnt);

static void rm_chn_descriptor(struct stm_chn_descriptor *chn_descriptor);

static int parse_to_user(char *buf, loff_t buf_pos,
			  char __user *user_buf, size_t count, loff_t *ppos,
			  char *delims);

static int parse_ulongs_from_string(int num_longs, int max_ulong, long *data,
				    char *kbuf, char *delims);

static int parse_strings_from_string(int num_strings, char *data[],
						    char *kbuf, char *delims);

/*****************************************************************************
 *The following file operation functions are for the STM user mode
 * debugfs message writes.
 *****************************************************************************/

/* Transport over STM user write to file. The function will act as if file is
 * simply being appended to (ppos incremented by count).
 * If count > STM_MAX_XFER_BYTES, break transfers into STM_MAX_XFER_BYTES
 * chuncks. If timestamp added, it gets added to each chunck.
 */
/* TODO: Add support for dealing with messages > STM_MAX_XFER_BYTES where
 *       the next call to stm_file_write is a continuation from the previous
 *       write. If timestamp added, it would only be sent with the last write
 *       when all data is sent. In user and vendor modes will need to add a
 *       flag to the channel descriptor indicating the next write is a
 *       continuation of the previous.
 */
static ssize_t stm_file_write(struct file *filp, const char __user *user_buf,
						    size_t count, loff_t *ppos)
{
	struct stm_chn_descriptor *chn_discriptor = filp->private_data;
	char *buf = NULL;
	char *kbuf = NULL;
	bool uspace = false;
	bool timestamp = false;
	uint msg_cnt = 0;
	int ret = 0;
	int header = 0;
	int checksum = 0;

	if (!chn_discriptor)
		return -EFAULT;

	if (!chn_discriptor->chn_enable)
		return -EPERM;

	if (chn_discriptor->write_protect)
		if (mutex_lock_interruptible(&chn_discriptor->lock))
			return -ERESTARTSYS;

	switch (chn_discriptor->msg_format) {

	case USER:
		timestamp = chn_discriptor->timestamp_enable;

		/*determine how much space is needed */
		if (chn_discriptor->checksum_enable)
			checksum = 1;

		if (chn_discriptor->ost_enable)
			header = ((count + checksum) < 256) ? 4 : 8;

		if (count + header + checksum > STM_MAX_XFER_BYTES) {
			count = STM_MAX_XFER_BYTES - (header + checksum);
			timestamp = false;
		}

		/* TODO:: If kmalloc sleeps (low memory case) then unprotected
		 *	messages could get out of order. Consider putting
		 *	in a small buffer (256 bytes) in the chn_discriptor.
		 *	In the unprotected case the user mode client is
		 *	resposnible for using different channels per thread.
		 */

		kbuf = kmalloc(count + header + checksum, GFP_KERNEL);
		if (kbuf == NULL) {
			ret = -ENOMEM;
			goto stm_file_write_exit;
		}

		if (copy_from_user(kbuf + header, user_buf, count)) {
			ret = -EFAULT;
			goto stm_file_write_exit;
		}

		if (chn_discriptor->ost_enable) {
			if (header == 4)
				*(uint32_t *)kbuf =  (uint32_t)(OST_VERSION
					| (chn_discriptor->ost_entity << 16)
					| (chn_discriptor->ost_protocol << 8)
					| (count + checksum));
			else {
				*(uint32_t *)kbuf =  (uint32_t)(OST_VERSION
					| (chn_discriptor->ost_entity << 16)
					| (chn_discriptor->ost_protocol << 8)
					| OST_SHORT_HEADER_LENGTH_LIMIT);
				*(uint32_t *)(kbuf+1) =
						  (uint32_t)(count + checksum);
			}
		}

		/* TODO:: Implement checksum. Required for INVARIANT messages.
		 *	May need to move the checksum to the first byte if
		 *	continuous (> STM_MAX_XFER_BYTES) transfer implemented.
		 */
		/* Note:: TI host tools do not support checksum ( or INVARIANT
		 * messages).
		 */
		if (checksum)
			*(uint8_t *)(kbuf + header + count) =
					    cal_checksum(kbuf, count + header);

		buf = kbuf;
		uspace = false;
		msg_cnt = header + count + checksum;
		break;

	case VENDOR:
		buf = (char *)user_buf;
		uspace = true;
		/* Note module specific driver can treat timestamp as false*/
		timestamp = true;
		msg_cnt = count;
		break;

	case RAW:
		buf = (char *)user_buf;
		uspace = true;
		timestamp = true;
		msg_cnt = count;
		break;

	} /* End of switch */

	ret = dev.stm_ops->write(chn_discriptor->chn, buf,
						   msg_cnt, uspace, timestamp);

	/* Check for user mode case */
	if (msg_cnt > count)
		ret = count;

	*ppos += ret;

stm_file_write_exit:

	kfree(kbuf);

	if (chn_discriptor->write_protect)
			mutex_unlock(&chn_discriptor->lock);

	return ret;
}

/* Open STM message write file */
static int stm_file_open(struct inode *inode, struct file *filp)
{
	/* Not muct to do other than move the file descriptor
	 * pointer from the inode to the filp
	 */
	filp->private_data = inode->i_private;
	return 0;
}

static const struct file_operations user_chn_fops = {
	.write =    stm_file_write,
	.open =     stm_file_open,
};

/*****************************************************************************
 *The following file operation functions are for the STM
 * debugfs config read/writes.
 *****************************************************************************/

enum config_id {
	CFGID_CHN_ENABLE,
	CFGID_CHN_DISABLE,
	CFGID_PROTECTED,
	CFGID_UNPROTECTED,
	CFGID_FMT_USER,
	CFGID_FMT_VENDOR,
	CFGID_FMT_RAW,
	CFGID_SET_HEADER,
	CFGID_TIMESTAMP_ENABLE,
	CFGID_TIMESTAMP_DISABLE,
	CFGID_CHECKSUM_ENABLE,
	CFGID_CHECKSUM_DISABLE,
	CFGID_LAST
};


static char *config_cmds[] = {
	[CFGID_CHN_ENABLE] = "chn_enable",
	[CFGID_CHN_DISABLE] = "chn_disable",
	[CFGID_PROTECTED] = "protected",
	[CFGID_UNPROTECTED] = "unprotected",
	[CFGID_FMT_USER] = "fmt_user",
	[CFGID_FMT_VENDOR] = "fmt_vendor",
	[CFGID_FMT_RAW] = "fmt_raw",
	[CFGID_SET_HEADER] = "set_header",
	[CFGID_TIMESTAMP_ENABLE] = "timestamp_enable",
	[CFGID_TIMESTAMP_DISABLE] = "timestamp_disable",
	[CFGID_CHECKSUM_ENABLE] = "checksum_enable",
	[CFGID_CHECKSUM_DISABLE] = "checksum_disable"
};


/* Parse STM channel configuration string from user write to file */
static ssize_t config_file_write(struct file *file, const char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct stm_chn_descriptor *chn_discriptor = file->private_data;
	char *kbuf = NULL;
	enum config_id cfg_id;
	int cfg_len;
	const int num_ulongs = 2;
	const int max_ulong = 255;
	long data[num_ulongs];
	char delims[] = " ";
	int err;

	if (count > MAX_CONFIG_STRING_BYTES) {
		count = -EINVAL;
		goto cfg_wrt_exit1;
	}

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf) {
		count = -ENOMEM;
		goto cfg_wrt_exit1;
	}

	copy_from_user(kbuf, user_buf, count);

	/* Check buf against list of supported commands (config_cmds) */
	for (cfg_id = CFGID_CHN_ENABLE; cfg_id < CFGID_LAST; cfg_id++) {
		cfg_len = strlen(config_cmds[cfg_id]);
		if (!strncmp(kbuf, config_cmds[cfg_id], cfg_len))
			break;
	}

	if (mutex_lock_interruptible(&chn_discriptor->lock))
		return -ERESTARTSYS;

	switch (cfg_id) {
	case CFGID_CHN_ENABLE:
		chn_discriptor->chn_enable = true;
		break;
	case CFGID_CHN_DISABLE:
		chn_discriptor->chn_enable = false;
		break;
	case CFGID_PROTECTED:
		chn_discriptor->write_protect = true;
		break;
	case CFGID_UNPROTECTED:
		chn_discriptor->write_protect = false;
		break;
	case CFGID_TIMESTAMP_ENABLE:
		chn_discriptor->timestamp_enable = true;
		break;
	case CFGID_TIMESTAMP_DISABLE:
		chn_discriptor->timestamp_enable = false;
		break;
	case CFGID_CHECKSUM_ENABLE:
		chn_discriptor->checksum_enable = true;
		break;
	case CFGID_CHECKSUM_DISABLE:
		chn_discriptor->checksum_enable = false;
		break;
	case CFGID_SET_HEADER:
		chn_discriptor->ost_enable = false;
		err = parse_ulongs_from_string(num_ulongs, max_ulong, data,
								kbuf, delims);
		if (err < 0) {
			count = err;
			goto cfg_wrt_exit2;
		}
		chn_discriptor->ost_entity = (char)data[0];
		chn_discriptor->ost_protocol = (char)data[1];
		chn_discriptor->ost_enable = true;
		break;
	case CFGID_FMT_USER:
		chn_discriptor->msg_format = USER;
		count = dev.stm_ops->config_write(chn_discriptor->chn,
								  kbuf, count);
		break;
	case CFGID_FMT_VENDOR:
		chn_discriptor->msg_format = VENDOR;
		count = dev.stm_ops->config_write(chn_discriptor->chn,
								  kbuf, count);
		break;
	case CFGID_FMT_RAW:
		chn_discriptor->msg_format = RAW;
		count = dev.stm_ops->config_write(chn_discriptor->chn,
								  kbuf, count);
		break;
	default:
		count = dev.stm_ops->config_write(chn_discriptor->chn,
								  kbuf, count);
	}

cfg_wrt_exit2:
	mutex_unlock(&chn_discriptor->lock);
cfg_wrt_exit1:
	if ((int)count < 0)
		printk(KERN_ALERT "stm_fw: invalid command format for %s, error %d\n",
					  chn_discriptor->cfg_filename, count);

	kfree(kbuf);

	return count;
}

/* Provide channel configuration data on user read from file */
static ssize_t config_file_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct stm_chn_descriptor *chn_discriptor = file->private_data;
	const int max_buf = 256;
	char buf[max_buf];
	int buf_cnt = 0;
	char delims[] = ",";
	loff_t buf_pos = 0;

	if (!dev.stm_ops->config_read)
		return 0;

	if (mutex_lock_interruptible(&chn_discriptor->lock))
		return -ERESTARTSYS;

	/* config_read puts formatted comma seperated strings in buf */
	buf_cnt = dev.stm_ops->config_read(chn_discriptor->chn, buf, max_buf);

	mutex_unlock(&chn_discriptor->lock);

	if (buf_cnt < 0)
		return buf_cnt;

	return parse_to_user(buf, buf_pos, user_buf, count, ppos, delims);
}

static int config_file_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static const struct file_operations config_fops = {
	.write =    config_file_write,
	.read =     config_file_read,
	.open =     config_file_open
};


/*****************************************************************************
 *The following file operation functions are for the STM
 * debugfs request file read/writes.
 *****************************************************************************/

/* Get global state information on read from the request file */
static ssize_t request_file_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	const int max_buf = 256;
	char buf[max_buf];
	int buf_cnt = 0;
	int len = 0;
	loff_t buf_pos = 0;
	char delims[] = ",";
	struct stm_global_info info;

	dev.stm_ops->get_global_info(&info);

	/*Get the fixed info we know we have room for */
	buf_cnt = snprintf(buf, max_buf,
			    "Total Channel Count %d\n",
			    info.total_chn_cnt);
	buf_cnt += snprintf(buf+buf_cnt, max_buf - buf_cnt,
			    "Reserved Channel Count %d\n",
			    info.reserved_chn_cnt);
	buf_cnt += snprintf(buf+buf_cnt, max_buf - buf_cnt,
			    "User Allocated Channels %d\n",
			    info.user_allocated_chn_cnt);
	buf_cnt += snprintf(buf+buf_cnt, max_buf - buf_cnt,
			    "STM Module Clock Enable %d\n",
			    info.stm_module_clock_on);
	buf_cnt += snprintf(buf+buf_cnt, max_buf - buf_cnt,
			    "STM Module Enable %d\n",
			    info.stm_module_enabled);

	if (buf_cnt > max_buf)
		return -ENOBUFS;

	/* Read buf_cnt bytes from kernel buf to user_buf. If successful ppos
	    * advanced by number of bytes read from kernel buf.
	    */
	if (buf_cnt <= count && buf_pos == *ppos) {
		len = simple_read_from_buffer(user_buf, count, ppos,
							buf, buf_cnt);
		if (len < 0)
			return len;

		count -= buf_cnt;
		user_buf += buf_cnt;
		buf_pos = buf_cnt;
	}

	if (info.attribute_list)
		return parse_to_user(info.attribute_list, buf_pos,
					      user_buf, count, ppos, delims);

	return 0; /* EOF */

}

/* Request file write commands */

enum request_id {
	USER_CHANNEL_REQUEST,
	PRINTK_CHANNEL_REQUEST,
	TRACEPOINT_CHANEL_REQUEST,
	LTTNG_CHANNEL_REQUEST,
	REMOVE_CHANNEL_REQUEST,
	LAST_REQUEST
};

static char *request_cmds[] = {
	[USER_CHANNEL_REQUEST] = "user",
	[PRINTK_CHANNEL_REQUEST] = "printk",
	[TRACEPOINT_CHANEL_REQUEST] = "tracepoint_ftrace",
	[LTTNG_CHANNEL_REQUEST] = "lttng",
	[REMOVE_CHANNEL_REQUEST] = "rm"
};

/* Request the creation of a channel file set (name & name_config) or execution
 * of a global command (effects all channels).
 */
static ssize_t request_file_write(struct file *filp,
				  const char __user *user_buf,
				  size_t count, loff_t *f_pos)
{
	int err = 0;
	char *err_msg = "stm_fw: error creating <debugfs> file ";
	char *buf = NULL;
	enum request_id req_id;
	char filename[MAX_SIZE_FILENAME];
	struct stm_chn_descriptor *chn_descriptor = NULL;
	char *username = NULL;
	int len;
	struct list_head *ptr;
	bool name_hit = false;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	copy_from_user(buf, user_buf, count);

	/* Check buf against the list of supported commands */
	for (req_id = USER_CHANNEL_REQUEST; req_id < LAST_REQUEST; req_id++)
		if (!strncmp(buf, request_cmds[req_id],
					strlen(request_cmds[req_id])))
			break;

	if (req_id < LAST_REQUEST) {
		/*
		 * Parse out the filename. For all cases currently supported.
		 * If case not support next switch takes default to call
		 * vendor drvier's write config function.
		 */
		char *delims = " ";
		err = parse_strings_from_string(1, &username, buf, delims);
		if (err < 0)
			goto err_exit;
	}

	if (mutex_lock_interruptible(&dev.lock))
		return -ERESTARTSYS;

	switch (req_id) {
	case USER_CHANNEL_REQUEST:
	case PRINTK_CHANNEL_REQUEST:
	case TRACEPOINT_CHANEL_REQUEST:
	case LTTNG_CHANNEL_REQUEST:

		/*
		 * For these cases try to allocate a STM channel and create
		 * write file (for user case only) and config file.
		 */


		/*
		 * Search other channel descriptors to confirm
		 * filename is unique.
		 */
		list_for_each(ptr, &dev.chn_list_head) {
			chn_descriptor = list_entry(ptr,
						    struct stm_chn_descriptor,
						    chn_list_link);
			if (!strcmp(chn_descriptor->chn_filename, username)) {
				err = -EINVAL;
				goto err_exit;
			}
		}

		/* Malloc the channel descriptor */
		chn_descriptor = kmalloc(sizeof *chn_descriptor, GFP_KERNEL);
		if (!chn_descriptor) {
			err = -ENOMEM;
			goto err_exit;
		}

		/* Allocate a channel */
		chn_descriptor->chn = dev.stm_ops->allocate_channel(
							request_cmds[req_id]);
		if (chn_descriptor->chn < 0) {
			err = chn_descriptor->chn;
			goto alloc_err_exit;
		}

		/* Initialize chn descriptor */
		chn_descriptor->write_dentry = NULL;
		chn_descriptor->config_dentry = NULL;
		chn_descriptor->chn_enable = true;
		chn_descriptor->write_protect = true;

		mutex_init(&chn_descriptor->lock);
		chn_descriptor->msg_format = VENDOR;
		chn_descriptor->msg_type = GUARANTEED;
		chn_descriptor->timestamp_enable = true;
		chn_descriptor->checksum_enable = false;

		/* If user command create debugfs message write file */
		if (req_id == USER_CHANNEL_REQUEST) {

			len = strlen(username);
			if (len > MAX_SIZE_FILENAME || !len) {
				err = -EINVAL;
				goto alloc_err_exit;
			}

			strcpy(chn_descriptor->chn_filename, username);

			chn_descriptor->write_dentry = debugfs_create_file(
					username, 0666, dev.parent,
					(void *)chn_descriptor,
					&user_chn_fops);

			if (!chn_descriptor->write_dentry) {
				pr_err("%s %s\n", err_msg, username);
				err = -ENODEV;
				goto alloc_err_exit;

			}
		}

		/* For all other channels create debugfs configuration file */

		len = snprintf(filename, MAX_SIZE_FILENAME, "%s_config",
								    username);
		if (len > MAX_SIZE_FILENAME) {
			err = -ENAMETOOLONG;
			goto alloc_err_exit;
		}

		strcpy(chn_descriptor->cfg_filename, filename);

		chn_descriptor->config_dentry = debugfs_create_file(filename,
				    0666, dev.parent, (void *)chn_descriptor,
					    &config_fops);

		if (!chn_descriptor->config_dentry) {
			pr_err("%s %s\n", err_msg, filename);
			err = -ENODEV;
			goto alloc_err_exit;
		}

		list_add_tail(&chn_descriptor->chn_list_link,
							&dev.chn_list_head);

		break;

	case REMOVE_CHANNEL_REQUEST:

		len = snprintf(filename, MAX_SIZE_FILENAME,
						"%s_config", username);
		if (len > MAX_SIZE_FILENAME || !len) {
			err = -EINVAL;
			goto err_exit;
		}

		list_for_each(ptr, &dev.chn_list_head) {
			chn_descriptor = list_entry(ptr,
						    struct stm_chn_descriptor,
						    chn_list_link);
			if (!strcmp(chn_descriptor->chn_filename, username) ||
			    !strcmp(chn_descriptor->cfg_filename, filename)) {
				name_hit = true;
				break;
			}
		 }

		if (!name_hit) {
			err = -EINVAL;
			goto err_exit;
		}

		rm_chn_descriptor(chn_descriptor);

		list_del(&chn_descriptor->chn_list_link);

		kfree(chn_descriptor);

		break;

	default:
		err = dev.stm_ops->config_write(-1, buf, count);
		if (err < 0) {
			err = -EINVAL;
			goto err_exit;
		}

	} /* End of the switch */

	kfree(buf);
	*f_pos += count;
	mutex_unlock(&dev.lock);
	return count;

alloc_err_exit:
	rm_chn_descriptor(chn_descriptor);

err_exit:
	kfree(buf);

	mutex_unlock(&dev.lock);

	return err;


}

static int request_file_open(struct inode *inode, struct file *file)
{
	/*
	 * Since we only allow one STM module to be open at any time,
	 * nothing really to do here.
	 */
	return 0;
}

static const struct file_operations request_fop = {
	.write = request_file_write,
	.read =	 request_file_read,
	.open =	 request_file_open,
};


/* Register module specific operations with Framework Drvier */
int stm_register(struct stm_operations *module_ops)
{
	int ret = 0;
	char *err_msg = "stm_fw: error creating <debugfs>/stm ";

	/*
	 * if module_ops NULL or mandatory ops not provided do not bother to
	 * create stm dir or request file
	 */
	if (module_ops == NULL ||
	    module_ops->get_global_info == NULL ||
	    module_ops->write == NULL ||
	    module_ops->allocate_channel == NULL ||
	    module_ops->free_channel == NULL ||
	    module_ops->config_write == NULL)
		ret = -EINVAL;

	if (mutex_lock_interruptible(&dev.lock))
		return -ERESTARTSYS;

	/* Check if already registered */
	if (dev.stm_ops) {
		ret = -EPERM;
		goto stm_register_err;
	}

	dev.parent = debugfs_create_dir("stm", NULL);
	if (((int)dev.parent == -ENODEV) || (dev.parent == NULL)) {

		if ((int)dev.parent == -ENODEV)
			pr_err("%s - debugfs not mounted\n", err_msg);
		if (dev.parent == NULL)
			pr_err("%s - error unknown\n", err_msg);
		ret = -ENODEV;
		goto stm_register_err;
	}

	dev.request = debugfs_create_file("request", 0666, dev.parent, NULL,
								 &request_fop);
	if (dev.request == NULL) {
		pr_err("%s/request\n", err_msg);
		debugfs_remove(dev.parent);
		ret = -ENODEV;
		goto stm_register_err;
	}

	pr_err("stm_fw: vendor driver %s registered\n", module_ops->name);

	dev.stm_ops = module_ops;
	INIT_LIST_HEAD(&dev.chn_list_head);
stm_register_err:
	mutex_unlock(&dev.lock);
	return ret;

}
EXPORT_SYMBOL(stm_register);

int stm_unregister(char *name)
{
	int ret = 0;
	struct stm_chn_descriptor *chn_descriptor;
	struct list_head *ptr;

	if (mutex_lock_interruptible(&dev.lock))
		return -ERESTARTSYS;

	if (strcmp(dev.stm_ops->name, name)) {
		ret = -EINVAL;
		goto stm_unregister_err;
	}

	list_for_each(ptr, &dev.chn_list_head) {
		chn_descriptor = list_entry(ptr, struct stm_chn_descriptor,
								chn_list_link);

		if (chn_descriptor->write_dentry)
			debugfs_remove(chn_descriptor->write_dentry);

		if (chn_descriptor->config_dentry)
			debugfs_remove(chn_descriptor->config_dentry);

		if (chn_descriptor->chn >= 0)
			dev.stm_ops->free_channel(chn_descriptor->chn);

		kfree(chn_descriptor);
	}

	debugfs_remove_recursive(dev.parent);
	dev.stm_ops = NULL;

	pr_info("stm_fw: vendor driver %s removed\n", dev.stm_ops->name);

stm_unregister_err:
	mutex_unlock(&dev.lock);

	return 0;

}
EXPORT_SYMBOL(stm_unregister);

static int stm_fw_init(void)
{
	dev.stm_ops = NULL;
	dev.parent = NULL;
	dev.request = NULL;
	mutex_init(&dev.lock);

	return 0;
}

static void stm_fw_exit(void)
{

	if (dev.stm_ops)
		stm_unregister(dev.stm_ops->name);  /* rm the request file*/

	pr_info("stm_fw: driver exit\n");
}

module_init(stm_fw_init);
module_exit(stm_fw_exit);
MODULE_LICENSE("Dual BSD/GPL");

/*****************************************************************************/
/* Support functions							 */
/*****************************************************************************/

/* TODO:: Implement cal_checksum(). */

static uint8_t cal_checksum(char *buf, int cnt)
{
	return 0;
}

static void rm_chn_descriptor(struct stm_chn_descriptor *chn_descriptor)
{
	if (!chn_descriptor)
		return;

	if (chn_descriptor->write_dentry)
		debugfs_remove(chn_descriptor->write_dentry);

	if (chn_descriptor->config_dentry)
		debugfs_remove(chn_descriptor->config_dentry);

	if (chn_descriptor->chn >= 0)
		dev.stm_ops->free_channel(chn_descriptor->chn);

	kfree(chn_descriptor);
}


/* Parse a comma seperated list of properties/attributes (items) from a
 * string and copy to user_buf.
 */
static int parse_to_user(char *buf, loff_t buf_pos,
			 char __user *user_buf, size_t count, loff_t *ppos,
			 char *delims)
{

	char *pbuf = buf;
	char **next = &pbuf;
	char *item = NULL;
	int item_len = 0;
	/*
	 * If this is not the first write back to
	 * the user buffer buf_pos will be non-zero
	 */
	int total_len = buf_pos;
	int copy_len;

	item = strsep(next, delims);
	do {
		item_len = strlen(item);
		if ((item_len <= count) && (buf_pos == *ppos)) {
			loff_t copy_pos = 0;
			/* copy item to user buf */
			copy_len = simple_read_from_buffer(user_buf, count,
						 &copy_pos, item, item_len);

			if (copy_len < 0)
				return copy_len;

			if (copy_len > 0) {
				total_len += copy_len;
				count -= item_len;
				*ppos = total_len;
				user_buf += item_len;
			}

		}
		buf_pos += item_len;
		item = strsep(next, delims);
	} while (item);

	if (!total_len && !item)
		return 0; /* Return EOF */

	return total_len;
}

/* Note, this function assumes first string is a command (which has already
 * been evaluated) is to be thrown away.
 */
static int parse_ulongs_from_string(int num_longs, int max_ulong, long *data,
						    char *kbuf, char *delims)
{
	/* Parse the set header command */
	int i;
	int ret;
	char **item = &kbuf;
	/* Get command - throw it away */
	char *param = strsep(item, delims);

	/* Get next two parameter values */
	for (i = 0; i < num_longs; i++) {
		param = strsep(item, delims);
		if (!param)
			return -EINVAL;

		ret = kstrtol(param, 10, &data[i]);
		if (ret)
			return ret;

		if (data[i] < 0 || data[i] > max_ulong)
			return -EINVAL;
	}
	return 0;
}

/* Note, this function assumes first string is a command (which has already
 * been evaluated) is to be thrown away.
 */
static int parse_strings_from_string(int num_strings, char *data[],
						    char *kbuf, char *delims)
{
	/* Parse the set header command */
	int i;
	char **item = &kbuf;
	/* Get command - throw it away */
	char *param = strsep(item, delims);

	/* Get next two parameter values */
	for (i = 0; i < num_strings; i++) {
		param = strsep(item, delims);
		if (!param)
			return -EINVAL;

		data[i] = param;
	}

	return 0;
}

