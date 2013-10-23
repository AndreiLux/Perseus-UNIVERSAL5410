/*
 * Copyright 2011 Validity Sensors, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/vfs61xx_platform.h>
#include "vfs61xx.h"


/* Pass to VFSSPI_IOCTL_GET_FREQ_TABLE command */
/**
* vfsspi_iocFreqTable - structure to get supported SPI baud rates
*
* @table:table which contains supported SPI baud rates
* @tblSize:table size
*/
struct vfsspi_iocFreqTable {
	unsigned int *table;
	unsigned int  tblSize;
};


/* The spi driver private structure. */
struct vfsspi_devData {
	dev_t devt;		/* Device ID */
	spinlock_t vfsSpiLock;	/* The lock for the spi device */
	struct spi_device *spi;	/* The spi device */
	struct list_head deviceEntry;	/* Device entry list */
	struct mutex bufferMutex;	/* The lock for the transfer buffer */
	unsigned int isOpened;	/* Indicates that driver is opened */
	unsigned char *buffer;	/* buffer for transmitting data */
	unsigned char *nullBuffer;	/* buffer for transmitting zeros */
	unsigned char *streamBuffer;
	unsigned int *freqTable;
	unsigned int freqTableSize;
	size_t streamBufSize;
	/* Storing user info data (device info obtained from announce packet) */
	struct vfsspi_iocUserData userInfoData;
	unsigned int drdyPin;	/* DRDY GPIO pin number */
	unsigned int sleepPin;	/* Sleep GPIO pin number */
	/* User process ID,
	 * to which the kernel signal indicating DRDY event is to be sent */
	int userPID;
	/* Signal ID which kernel uses to
	 * indicating user mode driver that DRDY is asserted */
	int signalID;
	unsigned int curSpiSpeed;	/* Current baud rate */
	struct vfs61xx_platform_data *pdata;
};

/* The initial baud rate for communicating with Validity sensor.
 * The initial clock is configured with low speed as sensor can boot
 * with external oscillator clock. */
#define SLOW_BAUD_RATE  2500000
/* Max baud rate supported by Validity sensor. */
#define MAX_BAUD_RATE   15000000
/* The coefficient which is multiplying with value retrieved from the
 * VFSSPI_IOCTL_SET_CLK IOCTL command for getting the final baud rate. */
#define BAUD_RATE_COEF  1000

#define PLATFORM_BIG_ENDIAN
#define MSM8974_SPI_TABLE

#ifdef MSM8974_SPI_TABLE
unsigned int freqTable[] = {
	960000,
	4800000,
	9600000,
	15000000,
	19200000,
	25000000,
	50000000,
};
#else
unsigned int freqTable[] = {
	1100000,
	5400000,
	10800000,
	15060000,
	24000000,
	25600000,
	27000000,
	48000000,
	51200000,
};
#endif

struct spi_device *gDevSpi;
struct class *vfsSpiDevClass;
int gpio_irq;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static LIST_HEAD(deviceList);
static DEFINE_MUTEX(deviceListMutex);
static DEFINE_MUTEX(kernel_lock);
static int dataToRead;

void shortToLittleEndian(char *buf, size_t len)
{
#ifdef PLATFORM_BIG_ENDIAN
	int i = 0;
	int j = 0;
	char LSB, MSB;

	for (i = 0; i < len; i++, j++) {
		LSB = buf[i];
		i++;

		MSB = buf[i];
		buf[j] = MSB;

		j++;
		buf[j] = LSB;
	}
#else
	/* Empty function */
#endif
}				/* shortToLittleEndian */

int vfsspi_sendDrdySignal(struct vfsspi_devData *vfsSpiDev)
{
	struct task_struct *t;
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (vfsSpiDev->userPID != 0) {
		rcu_read_lock();
		/* find the task_struct associated with userpid */
		pr_debug("%s Searching task with PID=%08x\n",
			__func__, vfsSpiDev->userPID);
		t = pid_task(find_pid_ns(vfsSpiDev->userPID, &init_pid_ns),
			     PIDTYPE_PID);
		if (t == NULL) {
			pr_err("%s No such pid\n", __func__);
			rcu_read_unlock();
			return -ENODEV;
		}
		rcu_read_unlock();
		/* notify DRDY signal to user process */
		ret =
		    send_sig_info(vfsSpiDev->signalID, (struct siginfo *)1, t);
		if (ret < 0)
			pr_err("%s Error sending signal\n", __func__);
	} else {
		pr_debug("%s pid not received yet\n", __func__);
	}

	return ret;
}

irqreturn_t vfsspi_irq(int irq, void *context)
{
	struct vfsspi_devData *vfsSpiDev = context;

	pr_debug("%s\n", __func__);

	dataToRead = 1;
	wake_up_interruptible(&wq);

	vfsspi_sendDrdySignal(vfsSpiDev);

	return IRQ_HANDLED;
}

/* Return no.of bytes written to device. Negative number for errors */
static inline ssize_t vfsspi_writeSync(struct vfsspi_devData *vfsSpiDev,
	unsigned char *buf, size_t len)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.rx_buf = NULL;
	t.tx_buf = buf;
	t.len = len;
	t.speed_hz = vfsSpiDev->curSpiSpeed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(vfsSpiDev->spi, &m);

	if (status == 0)
		status = m.actual_length;

	pr_debug("%s vfsspi_writeSync,length=%d\n",
		__func__, m.actual_length);

	return status;
}

/* Return no.of bytes read >0. negative integer incase of error. */
inline ssize_t vfsspi_readSync(struct vfsspi_devData *vfsSpiDev,
	unsigned char *buf, size_t len)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	spi_message_init(&m);
	memset(&t, 0x0, sizeof(t));

	t.tx_buf = NULL;
	t.rx_buf = buf;
	t.len = len;
	t.speed_hz = vfsSpiDev->curSpiSpeed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(vfsSpiDev->spi, &m);

	pr_debug("%s: status=%d\n", __func__, status);
	if (status == 0) {
		pr_debug("%s: m.actual_length=%d\n",
			__func__, m.actual_length);

		/* FIXME: This is temporary workaround for Fluid,
		   instead of returning actual read data length
		   spi_sync is returning 0 */
		status = len;
	}
	pr_debug("%s length=%d\n",
		__func__, len);
	return status;
}

ssize_t vfsspi_write(struct file *filp, const char __user *buf, size_t count,
		     loff_t *fPos)
{
	struct vfsspi_devData *vfsSpiDev = NULL;
	ssize_t status = 0;

	pr_debug("%s\n", __func__);

	if (count > DEFAULT_BUFFER_SIZE || count == 0)
		return -EMSGSIZE;

	vfsSpiDev = filp->private_data;

	mutex_lock(&vfsSpiDev->bufferMutex);

	if (vfsSpiDev->buffer) {
		unsigned long missing = 0;

		missing = copy_from_user(vfsSpiDev->buffer, buf, count);

		shortToLittleEndian((char *)vfsSpiDev->buffer, count);

		if (missing == 0)
			status = vfsspi_writeSync(vfsSpiDev,
				vfsSpiDev->buffer, count);
		else
			status = -EFAULT;
	}

	mutex_unlock(&vfsSpiDev->bufferMutex);

	return status;
}

ssize_t vfsspi_read(struct file *filp, char __user *buf, size_t count,
		    loff_t *fPos)
{
	struct vfsspi_devData *vfsSpiDev = NULL;
	unsigned char *readBuf = NULL;
	ssize_t status    = 0;
	pr_debug("%s\n", __func__);

	vfsSpiDev = filp->private_data;
	if (vfsSpiDev->streamBuffer != NULL
		&& count <= vfsSpiDev->streamBufSize)
		readBuf = vfsSpiDev->streamBuffer;
	else if (count <= DEFAULT_BUFFER_SIZE)
		readBuf = vfsSpiDev->buffer;
	else
		return -EMSGSIZE;

	if (buf == NULL)
		return -EFAULT;

	mutex_lock(&vfsSpiDev->bufferMutex);

	status = vfsspi_readSync(vfsSpiDev, readBuf, count);

	if (status > 0) {
		unsigned long missing = 0;
		/* data read. Copy to user buffer. */
		shortToLittleEndian((char *)readBuf, status);

		missing = copy_to_user(buf, readBuf, status);

		if (missing == status) {
			pr_err("%s copy_to_user failed\n", __func__);
			/* Nothing was copied to user space buffer. */
			status = -EFAULT;
		} else {
			status = status - missing;
			pr_debug("%s status=%d\n", __func__, status);
		}
	} else
		pr_err("%s err status=%d\n", __func__, status);

	mutex_unlock(&vfsSpiDev->bufferMutex);

	return status;
}

int vfsspi_xfer(struct vfsspi_devData *vfsSpiDev, struct vfsspi_iocTransfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("%s\n", __func__);

	if (vfsSpiDev == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || tr->len <= 0)
		return -EMSGSIZE;

	if (tr->txBuffer != NULL) {

		if (copy_from_user(vfsSpiDev->nullBuffer, tr->txBuffer, tr->len)
			!= 0) {
			pr_err("%s copy_from_user err\n", __func__);
			return -EFAULT;
		}
		shortToLittleEndian((char *)vfsSpiDev->nullBuffer, tr->len);
	} else
		pr_err("%s tr->txBuffer is NULL\n", __func__);

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = vfsSpiDev->nullBuffer;
	t.rx_buf = vfsSpiDev->buffer;
	t.len = tr->len;
	t.speed_hz = vfsSpiDev->curSpiSpeed;

	spi_message_add_tail(&t, &m);

	status = spi_sync(vfsSpiDev->spi, &m);

	if (status == 0) {
		if (tr->rxBuffer != NULL) {
			unsigned missing = 0;

			shortToLittleEndian((char *)vfsSpiDev->buffer, tr->len);

			missing =
			    copy_to_user(tr->rxBuffer, vfsSpiDev->buffer,
					 tr->len);

			if (missing != 0)
				tr->len = tr->len - missing;
		}
	}
	pr_debug("%s length=%d, status=%d\n",
		__func__, tr->len, status);
	return status;
}

void vfsspi_suspend(struct vfsspi_devData *vfsSpiDev)
{
	pr_info("%s\n", __func__);
	if (vfsSpiDev != NULL) {
		spin_lock(&vfsSpiDev->vfsSpiLock);
		dataToRead = 0;
		gpio_set_value(vfsSpiDev->sleepPin, 1);
		spin_unlock(&vfsSpiDev->vfsSpiLock);
	}
}

void vfsspi_hardReset(struct vfsspi_devData *vfsSpiDev)
{
	pr_info("%s\n", __func__);

	if (vfsSpiDev != NULL) {
		dataToRead = 0;
		gpio_set_value(vfsSpiDev->sleepPin, 1);
		usleep_range(1000, 1100);

		gpio_set_value(vfsSpiDev->sleepPin, 0);
		usleep_range(5000, 5100);
	}
}

#undef TEST_FIXED_FREQUENCY

long vfsspi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retVal = 0;
	struct vfsspi_devData *vfsSpiDev = NULL;
	struct vfsspi_iocTransfer *dup = NULL;
	struct vfsspi_iocRegSignal usrSignal;
	struct vfsspi_iocUserData tmpUserData;
	unsigned short clock = 0;
	unsigned short drdy_enable_flag;
	unsigned int streamDataSize;
	struct vfsspi_iocFreqTable tmpFreqData;

	pr_debug("%s\n", __func__);

	if (_IOC_TYPE(cmd) != VFSSPI_IOCTL_MAGIC) {
		pr_err("%s invalid cmd= 0x%X Received= 0x%X Expected= 0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), VFSSPI_IOCTL_MAGIC);
		return -ENOTTY;
	}

	vfsSpiDev = filp->private_data;

	mutex_lock(&vfsSpiDev->bufferMutex);

	switch (cmd) {

	case VFSSPI_IOCTL_DEVICE_SUSPEND:
		pr_debug("VFSSPI_IOCTL_DEVICE_SUSPEND:\n");
		vfsspi_suspend(vfsSpiDev);
		break;

	case VFSSPI_IOCTL_DEVICE_RESET:
		pr_debug("%s VFSSPI_IOCTL_DEVICE_RESET:\n", __func__);
		vfsspi_hardReset(vfsSpiDev);
		break;

	case VFSSPI_IOCTL_RW_SPI_MESSAGE:
		pr_debug("%s VFSSPI_IOCTL_RW_SPI_MESSAGE\n", __func__);
		dup = kmalloc(sizeof(struct vfsspi_iocTransfer), GFP_KERNEL);
		if (dup != NULL) {
			if (copy_from_user(dup, (void *)arg,
				sizeof(struct vfsspi_iocTransfer)) == 0) {
				retVal = vfsspi_xfer(vfsSpiDev, dup);
				if (retVal == 0) {
					if (copy_to_user((void *)arg, dup,
						sizeof(
						struct vfsspi_iocTransfer))
						!= 0) {
						pr_err("%s copy to user fail\n",
							__func__);
						retVal = -EFAULT;
					}
				}
			} else {
				pr_err("%s copy from user fail\n", __func__);
				retVal = -EFAULT;
			}
			kfree(dup);
		} else {
			retVal = -ENOMEM;
		}
		break;

	case VFSSPI_IOCTL_SET_CLK:
		pr_debug("%s VFSSPI_IOCTL_SET_CLK", __func__);
		if (copy_from_user(&clock, (void *)arg,
		     sizeof(unsigned short)) == 0) {
			struct spi_device *spidev = NULL;

			spin_lock_irq(&vfsSpiDev->vfsSpiLock);
			spidev = spi_dev_get(vfsSpiDev->spi);
			spin_unlock_irq(&vfsSpiDev->vfsSpiLock);
			pr_debug("%s: clock=%d\n", __func__, clock);
			if (spidev != NULL) {
				switch (clock) {
				case 0:
					/* Running baud rate. */
					pr_debug("%s Running baud rate.\n",
						__func__);
					spidev->max_speed_hz = MAX_BAUD_RATE;
					vfsSpiDev->curSpiSpeed = MAX_BAUD_RATE;
					break;

				case 0xFFFF:
					/* Slow baud rate */
					pr_debug("%s slow baud rate.\n",
						__func__);
					spidev->max_speed_hz = SLOW_BAUD_RATE;
					vfsSpiDev->curSpiSpeed = SLOW_BAUD_RATE;
					break;

				default:
					pr_debug("%s baud rate is %d.\n",
						__func__, clock);
#ifdef TEST_FIXED_FREQUENCY
					if (clock <= 4800)
						vfsSpiDev->curSpiSpeed =
							SLOW_BAUD_RATE;
					if (vfsSpiDev->curSpiSpeed
						> MAX_BAUD_RATE)
						vfsSpiDev->curSpiSpeed
							= MAX_BAUD_RATE;
					spidev->max_speed_hz =
						vfsSpiDev->curSpiSpeed;
#else
					vfsSpiDev->curSpiSpeed =
						clock * BAUD_RATE_COEF;
					if (vfsSpiDev->curSpiSpeed
						> MAX_BAUD_RATE)
						vfsSpiDev->curSpiSpeed
						= MAX_BAUD_RATE;
					spidev->max_speed_hz =
						vfsSpiDev->curSpiSpeed;
#endif
					break;
				}
					spi_dev_put(spidev);
			}

		} else {
			pr_err("%s Failed copy from user.\n", __func__);
			retVal = -EFAULT;
		}

		break;

	case VFSSPI_IOCTL_CHECK_DRDY:
		retVal = -ETIMEDOUT;
		pr_debug("%s: VFSSPI_IOCTL_CHECK_DRDY",
			__func__);
		dataToRead = 0;

		if (gpio_get_value(vfsSpiDev->drdyPin)
			== DRDY_ACTIVE_STATUS)
			retVal = 0;
		else {
			unsigned long timeout =
				msecs_to_jiffies(DRDY_TIMEOUT_MS);
			unsigned long startTime = jiffies;
			unsigned long endTime = 0;

			do {
				wait_event_interruptible_timeout(wq,
					dataToRead != 0, timeout);
				dataToRead = 0;
				if (gpio_get_value(vfsSpiDev->drdyPin)
					== DRDY_ACTIVE_STATUS) {
					retVal = 0;
					break;
				}

				endTime = jiffies;
				/* Timed out for waiting to wake up event. */
				if (endTime - startTime >= timeout)
					timeout = 0;
				else {
				/* Thread is woke up by spurious event.
				Calculate a new timeout and
				continue to wait DRDY signal assertion. */
					timeout -= endTime - startTime;
					startTime = endTime;
				}
			} while (timeout > 0);
		}
		dataToRead = 0;
		break;

	case VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL:
		pr_debug("%s VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL\n", __func__);

		if (copy_from_user(&usrSignal, (void *)arg,
			sizeof(usrSignal)) != 0) {
			pr_err("%s Failed copy from user.\n", __func__);
			retVal = -EFAULT;
		} else {
			vfsSpiDev->userPID = usrSignal.userPID;
			vfsSpiDev->signalID = usrSignal.signalID;
		}
		break;

	case VFSSPI_IOCTL_SET_USER_DATA:
		pr_debug("%s VFSSPI_IOCTL_SET_USER_DATA\n", __func__);
		if ((void *)arg == NULL) {
			pr_err("%s VFSSPI_IOCTL_SET_USER_DATA is failed\n",
				__func__);
			retVal = -EINVAL;
			break;
		}

		if (copy_from_user(&tmpUserData, (void *)arg,
			sizeof(tmpUserData)) == 0) {
			if (vfsSpiDev->userInfoData.buffer != NULL)
				kfree(vfsSpiDev->userInfoData.buffer);
			vfsSpiDev->userInfoData.buffer =
			    kmalloc(tmpUserData.len, GFP_KERNEL);
			if (vfsSpiDev->userInfoData.buffer != NULL) {
				vfsSpiDev->userInfoData.len =
				    tmpUserData.len;
				if (tmpUserData.buffer != NULL) {
					if (copy_from_user
						(vfsSpiDev->userInfoData.buffer,
						tmpUserData.buffer,
						tmpUserData.len) != 0) {
						pr_err("%s cp from user fail\n",
							__func__);
						retVal = -EFAULT;
					}
				} else {
					retVal = -EFAULT;
				}
			} else {
				retVal = -ENOMEM;
			}
		} else {
			pr_err("%s copy from user failed\n", __func__);
			retVal = -EFAULT;
		}
		break;

	case VFSSPI_IOCTL_GET_USER_DATA:
		retVal = -EFAULT;

		pr_debug("%s VFSSPI_IOCTL_GET_USER_DATA\n", __func__);

		if (vfsSpiDev->userInfoData.buffer != NULL
		    && (void *)arg != NULL) {
			if (copy_from_user(&tmpUserData, (void *)arg,
				sizeof(struct vfsspi_iocUserData)) != 0) {
				pr_err("%s copy from user failed\n", __func__);
				break;
			}
			if (tmpUserData.len ==
				vfsSpiDev->userInfoData.len
				&& tmpUserData.buffer != NULL) {
				pr_err("%s userInfoData incorrect\n", __func__);
				break;
			}
			if (copy_to_user(tmpUserData.buffer,
				vfsSpiDev->userInfoData.buffer,
				tmpUserData.len) == 0) {
				pr_err("%s copy to user fail\n", __func__);
				break;
			}
			if (copy_to_user((void *)arg,
				&(tmpUserData),
				sizeof(struct vfsspi_iocUserData))
				!= 0) {
				pr_err("%s cp to user fail\n", __func__);
				break;
			}
			retVal = 0;
		} else
			pr_err("%s VFSSPI_IOCTL_GET_USER_DATA failed\n",
				__func__);
		break;

	case VFSSPI_IOCTL_SET_DRDY_INT:
		pr_debug("%s VFSSPI_IOCTL_SET_DRDY_INT\n", __func__);

		if (copy_from_user(&drdy_enable_flag,
			(void *)arg, sizeof(drdy_enable_flag)) != 0) {
			pr_err("%s Failed copy from user.\n", __func__);
			retVal = -EFAULT;
		} else {
			if (drdy_enable_flag == 0)
				free_irq(gpio_irq, vfsSpiDev);
			else {
				if (request_irq(gpio_irq, vfsspi_irq,
					DRDY_IRQ_FLAG,
					"vfsspi_irq", vfsSpiDev) < 0) {
					pr_err("%s  Unable set DRDY INT\n",
						__func__);
					retVal = -EBUSY;
				}
			}
		}
		break;

	case VFSSPI_IOCTL_STREAM_READ_START:
		pr_debug("VFSSPI_IOCTL_STREAM_READ_START");
		if (copy_from_user(&streamDataSize, (void *)arg,
			sizeof(unsigned int)) != 0) {
			pr_err("Failed copy from user.\n");
			retVal = -EFAULT;
		} else {
			if (vfsSpiDev->streamBuffer != NULL)
				kfree(vfsSpiDev->streamBuffer);
			vfsSpiDev->streamBuffer =
				kmalloc(streamDataSize, GFP_KERNEL);

			if (vfsSpiDev->streamBuffer == NULL)
				retVal = -ENOMEM;
			else
				vfsSpiDev->streamBufSize = streamDataSize;
		}
		break;

	case VFSSPI_IOCTL_STREAM_READ_STOP:
		pr_debug("VFSSPI_IOCTL_STREAM_READ_STOP");
		if (vfsSpiDev->streamBuffer != NULL) {
			kfree(vfsSpiDev->streamBuffer);
			vfsSpiDev->streamBuffer = NULL;
			vfsSpiDev->streamBufSize = 0;
		}
		break;
	case VFSSPI_IOCTL_GET_FREQ_TABLE:
		pr_info("%s: VFSSPI_IOCTL_GET_FREQ_TABLE\n",
			__func__);

		retVal = -EINVAL;
		if (copy_from_user(&tmpFreqData, (void *)arg,
			sizeof(tmpFreqData)) != 0) {
			pr_err("Failed copy from user.\n");
			break;
		}
		tmpFreqData.tblSize = 0;
		if (vfsSpiDev->freqTable != NULL) {
			tmpFreqData.tblSize = vfsSpiDev->freqTableSize;
			if (tmpFreqData.table != NULL) {
				if (copy_to_user(tmpFreqData.table,
					vfsSpiDev->freqTable,
					vfsSpiDev->freqTableSize) != 0) {
					pr_err("Failed copy to user.\n");
					break;
				}
			}
		}
		if (copy_to_user((void *)arg, &(tmpFreqData),
			sizeof(tmpFreqData)) == 0)
			retVal = 0;
		else
			pr_err("copy to user failed\n");
		break;
	default:
		retVal = -EFAULT;
		break;
	}
	mutex_unlock(&vfsSpiDev->bufferMutex);
	return retVal;
}

int vfsspi_open(struct inode *inode, struct file *filp)
{
	struct vfsspi_devData *vfsSpiDev = NULL;
	int status = -ENXIO;

	pr_info("%s\n", __func__);

	mutex_lock(&kernel_lock);
	mutex_lock(&deviceListMutex);
	list_for_each_entry(vfsSpiDev, &deviceList, deviceEntry) {
		if (vfsSpiDev->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status == 0) {
		if (vfsSpiDev->isOpened != 0) {
			status = -EBUSY;
		} else {
			vfsSpiDev->userPID = 0;
			if (vfsSpiDev->buffer == NULL) {
				vfsSpiDev->nullBuffer =
				    kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
				vfsSpiDev->buffer =
				    kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);

				if (vfsSpiDev->buffer == NULL
				    || vfsSpiDev->nullBuffer == NULL) {
					status = -ENOMEM;
				} else {
					vfsSpiDev->isOpened = 1;
					filp->private_data = vfsSpiDev;
					nonseekable_open(inode, filp);
				}
			}
		}
	}

	mutex_unlock(&deviceListMutex);
	mutex_unlock(&kernel_lock);

	pr_debug("%s drdy(%d)=%d, sleepPin(%d)=%d\n", __func__,
		vfsSpiDev->drdyPin, gpio_get_value(vfsSpiDev->drdyPin),
		vfsSpiDev->sleepPin, gpio_get_value(vfsSpiDev->sleepPin));

	return status;
}

int vfsspi_release(struct inode *inode, struct file *filp)
{
	struct vfsspi_devData *vfsSpiDev = NULL;
	int status = 0;

	pr_info("%s\n", __func__);

	mutex_lock(&deviceListMutex);
	vfsSpiDev = filp->private_data;
	filp->private_data = NULL;
	vfsSpiDev->isOpened = 0;
	if (vfsSpiDev->buffer != NULL) {
		kfree(vfsSpiDev->buffer);
		vfsSpiDev->buffer = NULL;
	}

	if (vfsSpiDev->nullBuffer != NULL) {
		kfree(vfsSpiDev->nullBuffer);
		vfsSpiDev->nullBuffer = NULL;
	}

	if (vfsSpiDev->streamBuffer != NULL) {
		kfree(vfsSpiDev->streamBuffer);
		vfsSpiDev->streamBuffer = NULL;
		vfsSpiDev->streamBufSize = 0;
	}

	mutex_unlock(&deviceListMutex);
	return status;
}

int vfsspi_platformInit(struct vfsspi_devData *vfsSpiDev)
{
	int status = 0;

	pr_info("%s\n", __func__);

	if (vfsSpiDev != NULL) {
		if (gpio_request(vfsSpiDev->drdyPin, "vfsspi_drdy") < 0)
			return -EBUSY;

		if (gpio_request(vfsSpiDev->sleepPin, "vfsspi_sleep"))
			return -EBUSY;

		status = gpio_direction_output(vfsSpiDev->sleepPin, 0);

		status = gpio_direction_input(vfsSpiDev->drdyPin);

		if (status < 0) {
			pr_err("%s gpio_direction_input DRDY failed\n",
				__func__);
			return -EBUSY;
		}

		gpio_irq = gpio_to_irq(vfsSpiDev->drdyPin);

		if (gpio_irq < 0) {
			pr_err("%s gpio_to_irq failed\n", __func__);
			return -EBUSY;
		}

		if (request_irq
		    (gpio_irq, vfsspi_irq, DRDY_IRQ_FLAG, "vfsspi_irq",
		     vfsSpiDev) < 0) {
			pr_err("%s request_irq failed\n", __func__);
			return -EBUSY;
		}
		pr_debug("%s drdy value =%d\n", __func__,
			gpio_get_value(vfsSpiDev->drdyPin));
		vfsSpiDev->freqTable = freqTable;
		vfsSpiDev->freqTableSize = sizeof(freqTable);
	} else {
		status = -EFAULT;
	}
	pr_info("%s vfsspi_platofrminit, status=%d\n", __func__, status);
	return status;
}

void vfsspi_platformUninit(struct vfsspi_devData *vfsSpiDev)
{
	pr_info("%s\n", __func__);

	if (vfsSpiDev != NULL) {
		vfsSpiDev->freqTable = NULL;
		vfsSpiDev->freqTableSize = 0;
		free_irq(gpio_irq, vfsSpiDev);
		gpio_free(vfsSpiDev->sleepPin);
		gpio_free(vfsSpiDev->drdyPin);
	}
}

int vfsspi_probe(struct spi_device *spi)
{
	int status = 0;
	struct vfsspi_devData *vfsSpiDev = NULL;
	struct device *dev = NULL;
	struct vfs61xx_platform_data *pdata;

	pr_info("%s\n", __func__);

	vfsSpiDev = kzalloc(sizeof(*vfsSpiDev), GFP_KERNEL);

	if (vfsSpiDev == NULL)
		return -ENOMEM;

	pdata = (struct vfs61xx_platform_data *) spi->dev.platform_data;

	if (pdata) {
		pdata->vfs61xx_sovcc_on(1);
		vfsSpiDev->sleepPin = pdata->sleep;
		vfsSpiDev->drdyPin = pdata->drdy;
		vfsSpiDev->pdata = pdata;
	}

	/* Initialize driver data. */
	vfsSpiDev->curSpiSpeed = SLOW_BAUD_RATE;
	vfsSpiDev->userInfoData.buffer = NULL;
	vfsSpiDev->spi = spi;
	spin_lock_init(&vfsSpiDev->vfsSpiLock);
	mutex_init(&vfsSpiDev->bufferMutex);

	INIT_LIST_HEAD(&vfsSpiDev->deviceEntry);

	status = vfsspi_platformInit(vfsSpiDev);

	if (status == 0) {
		spi->bits_per_word = BITS_PER_WORD;
		spi->max_speed_hz = SLOW_BAUD_RATE;
		spi->mode = SPI_MODE_0;

		status = spi_setup(spi);

		if (status == 0) {
			mutex_lock(&deviceListMutex);

			/* Create device node */
			vfsSpiDev->devt = MKDEV(VFSSPI_MAJOR, 0);
			dev =
			    device_create(vfsSpiDevClass, &spi->dev,
					  vfsSpiDev->devt, vfsSpiDev, "vfsspi");
			status = IS_ERR(dev) ? PTR_ERR(dev) : 0;

			if (status == 0)
				list_add(&vfsSpiDev->deviceEntry, &deviceList);

			mutex_unlock(&deviceListMutex);

			if (status == 0) {
				spi_set_drvdata(spi, vfsSpiDev);
			} else {
				pr_err("%s device_create failed %d\n",
				     __func__, status);
				kfree(vfsSpiDev);
			}
		} else {
			gDevSpi = spi;
			pr_err("%s spi_setup() is failed! status= %d\n",
				__func__, status);
			vfsspi_platformUninit(vfsSpiDev);
			kfree(vfsSpiDev);
		}
	} else {
		vfsspi_platformUninit(vfsSpiDev);
		kfree(vfsSpiDev);
	}

	pr_info("%s success...\n", __func__);
	return status;
}

int vfsspi_remove(struct spi_device *spi)
{
	int status = 0;

	struct vfsspi_devData *vfsSpiDev = NULL;

	pr_info("%s\n", __func__);

	vfsSpiDev = spi_get_drvdata(spi);

	if (vfsSpiDev != NULL) {
		gDevSpi = spi;
		spin_lock_irq(&vfsSpiDev->vfsSpiLock);
		vfsSpiDev->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&vfsSpiDev->vfsSpiLock);

		mutex_lock(&deviceListMutex);

		vfsspi_platformUninit(vfsSpiDev);

		if (vfsSpiDev->userInfoData.buffer != NULL)
			kfree(vfsSpiDev->userInfoData.buffer);

		/* Remove device entry. */
		list_del(&vfsSpiDev->deviceEntry);
		device_destroy(vfsSpiDevClass, vfsSpiDev->devt);

		kfree(vfsSpiDev);
		mutex_unlock(&deviceListMutex);
	}

	return status;
}

/* SPI driver info */
struct spi_driver vfsspi_spi = {
	.driver = {
		.name = "validity_fingerprint",
		.owner = THIS_MODULE,
	},
	.probe = vfsspi_probe,
	.remove = __devexit_p(vfsspi_remove),
};

/* file operations associated with device */
const struct file_operations vfsspi_fops = {
	.owner = THIS_MODULE,
	.write = vfsspi_write,
	.read = vfsspi_read,
	.unlocked_ioctl = vfsspi_ioctl,
	.open = vfsspi_open,
	.release = vfsspi_release,
};

static int __init vfsspi_init(void)
{
	int status = 0;

	pr_info("%s\n", __func__);

	/* register major number for character device */
	status =
	    register_chrdev(VFSSPI_MAJOR, "validity_fingerprint", &vfsspi_fops);

	if (status < 0) {
		pr_err("%s register_chrdev failed\n", __func__);
		return status;
	}

	vfsSpiDevClass = class_create(THIS_MODULE, "validity_fingerprint");

	if (IS_ERR(vfsSpiDevClass)) {
		pr_err
		    ("%s vfsspi_init: class_create() is failed\n", __func__);
		unregister_chrdev(VFSSPI_MAJOR, vfsspi_spi.driver.name);
		return PTR_ERR(vfsSpiDevClass);
	}

	status = spi_register_driver(&vfsspi_spi);

	if (status < 0) {
		pr_err("%s : register spi drv is failed\n", __func__);
		unregister_chrdev(VFSSPI_MAJOR, vfsspi_spi.driver.name);
		return status;
	}
	pr_info("%s init is successful\n", __func__);

	return status;
}

static void __exit vfsspi_exit(void)
{
	pr_info("%s\n", __func__);

	spi_unregister_driver(&vfsspi_spi);
	class_destroy(vfsSpiDevClass);

	unregister_chrdev(VFSSPI_MAJOR, vfsspi_spi.driver.name);
}

module_init(vfsspi_init);
module_exit(vfsspi_exit);

MODULE_LICENSE("GPL");
