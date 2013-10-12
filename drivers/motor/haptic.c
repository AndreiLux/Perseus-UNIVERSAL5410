#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <mach/map.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include "haptic.h"

#if 0
#define DEBUG_LOG
#endif

DEFINE_SEMAPHORE(sema);

/* Forward declarations */
static int vibe_process(void *data);

static struct vibe_drvdata *vibe_ddata;
static inline int VibeSemIsLocked(struct semaphore *lock)
{
	return (lock->count) != 1;
}

static enum hrtimer_restart tsp_timer_interrupt(struct hrtimer *timer)
{
	hrtimer_forward_now(timer, vibe_ddata->g_ktFiveMs);

	if (vibe_ddata->timer_started) {
		if (VibeSemIsLocked(&sema))
			up(&sema);
	}

	return HRTIMER_RESTART;
}

static void vibe_start_timer(void)
{
	int i;
	int res;

	vibe_ddata->watchdog_cnt = 0;

	if (!vibe_ddata->timer_started) {
		if (!VibeSemIsLocked(&sema))
			res = down_interruptible(&sema);

		vibe_ddata->timer_started = true;

		hrtimer_start(&vibe_ddata->g_tspTimer, vibe_ddata->g_ktFiveMs, HRTIMER_MODE_REL);

		for (i = 0; i < vibe_ddata->num_actuators; i++) {
			if ((vibe_ddata->actuator[i].samples[0].size)
				|| (vibe_ddata->actuator[i].samples[1].size)) {
				vibe_ddata->actuator[i].output = 0;
				return;
			}
		}
	}

	if (0 != vibe_process(NULL))
		return;

	res = down_interruptible(&sema);
	if (res != 0)
		printk(KERN_INFO
			"[VIB] %s down_interruptible interrupted by a signal.\n",
			__func__);
}

static void vibe_stop_timer(void)
{
	int i;

	if (vibe_ddata->timer_started) {
		vibe_ddata->timer_started = false;
		hrtimer_cancel(&vibe_ddata->g_tspTimer);
	}

	/* Reset samples buffers */
	for (i = 0; i < vibe_ddata->num_actuators; i++) {
		vibe_ddata->actuator[i].playing = -1;
		vibe_ddata->actuator[i].samples[0].size = 0;
		vibe_ddata->actuator[i].samples[1].size = 0;
	}
	vibe_ddata->stop_requested = false;
	vibe_ddata->is_playing = false;
}

static void vibe_timer_init(void)
{
	vibe_ddata->g_ktFiveMs = ktime_set(0, 5000000);

	hrtimer_init(&vibe_ddata->g_tspTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	vibe_ddata->g_tspTimer.function = tsp_timer_interrupt;
}

static void vibe_terminate_timer(void)
{
	vibe_stop_timer();
	if (VibeSemIsLocked(&sema))
		up(&sema);
}

static int vibe_disable(int index)
{
	if (vibe_ddata->amp_enabled) {
#if defined(DEBUG_LOG)
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

		vibe_ddata->amp_enabled = false;
		vibe_ddata->chip_en(false);

		if (vibe_ddata->motor_enabled == 1)
			vibe_ddata->motor_enabled = 0;
	}

	return 0;
}

static int vibe_enable(u8 index)
{
	if (!vibe_ddata->amp_enabled) {
#if defined(DEBUG_LOG)
		printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif
		vibe_ddata->amp_enabled = true;
		vibe_ddata->motor_enabled = 1;

		vibe_ddata->chip_en(true);
	}

	return 0;
}

static int vibe_init(void)
{
#if defined(DEBUG_LOG)
	printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif
	vibe_ddata->amp_enabled = false;
	vibe_ddata->motor_enabled = 0;
	return 0;
}

static int vibe_terminate(void)
{
	int i = 0;
#if defined(DEBUG_LOG)
	printk(KERN_DEBUG "[VIB] %s\n", __func__);
#endif

	for (i = 0; vibe_ddata->num_actuators > i; i++)
		vibe_disable(i);

	return 0;
}

static int vibe_setsamples(int index, u16 depth, u16 size, char *buff)
{
	char force;

	switch (depth) {
	case 8:
		if (size != 1) {
			printk(KERN_ERR "[VIB] %s size : %d\n", __func__, size);
			return VIBE_E_FAIL;
		}
		force = buff[0];
		break;

	case 16:
		if (size != 2)
			return VIBE_E_FAIL;
		force = ((u16 *)buff)[0] >> 8;
		break;

	default:
		return VIBE_E_FAIL;
	}

	vibe_ddata->set_force(index, force);

	return 0;
}

static int vibe_get_name(u8 index, char *szDevName, int nSize)
{
	return 0;
}

static int vibe_process(void *data)
{
	int i = 0;
	int not_playing = 0;

	for (i = 0; i < vibe_ddata->num_actuators; i++) {
		struct actuator_data *current_actuator = &(vibe_ddata->actuator[i]);

		if (-1 == current_actuator->playing) {
			not_playing++;
			if ((vibe_ddata->num_actuators == not_playing) &&
				((++vibe_ddata->watchdog_cnt) > WATCHDOG_TIMEOUT)) {
				char buff[1] = {0};
				vibe_setsamples(i, 8, 1, buff);
				vibe_disable(i);
				vibe_stop_timer();
				vibe_ddata->watchdog_cnt = 0;
			}
		} else {
			int index = (int)current_actuator->playing;
			if (VIBE_E_FAIL == vibe_setsamples(
				current_actuator->samples[index].index,
				current_actuator->samples[index].bit_detpth,
				current_actuator->samples[index].size,
				current_actuator->samples[index].data))
				hrtimer_forward_now(&vibe_ddata->g_tspTimer, vibe_ddata->g_ktFiveMs);

			current_actuator->output += current_actuator->samples[index].size;

			if (current_actuator->output >= current_actuator->samples[index].size) {
				current_actuator->samples[index].size = 0;

				(current_actuator->playing) ^= 1;
				current_actuator->output = 0;

				if (vibe_ddata->stop_requested) {
					current_actuator->playing = -1;
					vibe_disable(i);
				}
			}
		}
	}

	if (vibe_ddata->stop_requested) {
		vibe_stop_timer();

		vibe_ddata->watchdog_cnt = 0;

		if (VibeSemIsLocked(&sema))
			up(&sema);
		return 1;
	}

	return 0;
}

static int tspdrv_open(struct inode *inode, struct file *file)
{
#if defined(DEBUG_LOG)
	printk(KERN_INFO "[VIB] %s\n", __func__);
#endif

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	return 0;
}

static int tspdrv_release(struct inode *inode, struct file *file)
{
#if defined(DEBUG_LOG)
	printk(KERN_INFO "[VIB] %s\n", __func__);
#endif

	vibe_stop_timer();

	file->private_data = (void *)NULL;

	module_put(THIS_MODULE);

	return 0;
}

static ssize_t tspdrv_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	const size_t nBufSize = (vibe_ddata->cch_device_name > (size_t)(*ppos)) ?
		min(count, vibe_ddata->cch_device_name - (size_t)(*ppos)) : 0;

	if (0 == nBufSize)
		return 0;

	if (0 != copy_to_user(buf, vibe_ddata->device_name + (*ppos), nBufSize))	{
		printk(KERN_ERR "[VIB] copy_to_user failed.\n");
		return 0;
	}

	*ppos += nBufSize;
	return nBufSize;
}

static ssize_t tspdrv_write(struct file *file, const char *buf, size_t count,
	loff_t *ppos)
{
	int i = 0;

	*ppos = 0;

	if (file->private_data != (void *)TSPDRV_MAGIC_NUMBER) {
		printk(KERN_ERR "[VIB] unauthorized write.\n");
		return 0;
	}

	if ((count <= SPI_HEADER_SIZE) || (count > SPI_BUFFER_SIZE)) {
		printk(KERN_ERR "[VIB] invalid write buffer size.\n");
		return 0;
	}

	if (0 != copy_from_user(vibe_ddata->write_buff, buf, count)) {
		printk(KERN_ERR "[VIB] copy_from_user failed.\n");
		return 0;
	}

	while (i < count) {
		struct samples_data* input =
			(struct samples_data *)(&vibe_ddata->write_buff[i]);
		int buff = 0;

		if ((i + SPI_HEADER_SIZE) >= count) {
			printk(KERN_EMERG "[VIB] invalid buffer index.\n");
			return 0;
		}

		if (8 != input->bit_detpth)
			printk(KERN_WARNING
				"[VIB] invalid bit depth. Use default value (8)\n");

		if ((i + SPI_HEADER_SIZE + input->size) > count) {
			printk(KERN_EMERG "[VIB] invalid data size.\n");
			return 0;
		}

		if (vibe_ddata->num_actuators <= input->index) {
			printk(KERN_ERR "[VIB] invalid actuator index.\n");
			i += (SPI_HEADER_SIZE + input->size);
			continue;
		}

		if (0 == vibe_ddata->actuator[input->index].samples[0].size)
			buff = 0;
		else if (0 == vibe_ddata->actuator[input->index].samples[1].size)
			buff = 1;
		else {
			printk(KERN_ERR
				"[VIB] no room to store new samples.\n");
			return 0;
		}

		memcpy(&(vibe_ddata->actuator[input->index].samples[buff]),
			&vibe_ddata->write_buff[i],
			(SPI_HEADER_SIZE + input->size));

		if (-1 == vibe_ddata->actuator[input->index].playing) {
			vibe_ddata->actuator[input->index].playing = buff;
			vibe_ddata->actuator[input->index].output = 0;
		}

		i += (SPI_HEADER_SIZE + input->size);
	}

	vibe_ddata->is_playing = true;
	vibe_start_timer();

	return count;
}

static long tspdrv_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
#if defined(DEBUG_LOG)
	printk(KERN_INFO "[VIB] ioctl cmd[0x%x].\n", cmd);
#endif

	switch (cmd) {
	case TSPDRV_STOP_KERNEL_TIMER:
		if (true == vibe_ddata->is_playing)
			vibe_ddata->stop_requested = true;

		vibe_process(NULL);
		break;

	case TSPDRV_MAGIC_NUMBER:
		file->private_data = (void *)TSPDRV_MAGIC_NUMBER;
		break;

	case TSPDRV_ENABLE_AMP:
		wake_lock(&vibe_ddata->wlock);
		vibe_enable(arg);
		break;

	case TSPDRV_DISABLE_AMP:
		if (!vibe_ddata->stop_requested)
			vibe_disable(arg);
		wake_unlock(&vibe_ddata->wlock);
		break;

	case TSPDRV_GET_NUM_ACTUATORS:
		return vibe_ddata->num_actuators;
	}

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = tspdrv_read,
	.write = tspdrv_write,
	.unlocked_ioctl = tspdrv_ioctl,
	.open = tspdrv_open,
	.release = tspdrv_release,
	.llseek =	default_llseek
};

static struct miscdevice miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     MODULE_NAME,
	.fops =     &fops
};

int tspdrv_init(struct vibe_drvdata *data)
{
	int ret = 0, i = 0;

	printk(KERN_INFO "[VIB] %s\n", __func__);

	vibe_ddata = data;

	ret = misc_register(&miscdev);
	if (ret) {
		printk(KERN_ERR "[VIB] misc_register failed.\n");
		return ret;
	}

	vibe_init();
	vibe_timer_init();

	for (i = 0; i < data->num_actuators; i++) {
		char *name = data->device_name;
		vibe_get_name(i, name, VIBE_NAME_SIZE);

		strcat(name, VERSION_STR);
		data->cch_device_name += strlen(name);

		data->actuator[i].playing = -1;
		data->actuator[i].samples[0].size = 0;
		data->actuator[i].samples[1].size = 0;
	}

	wake_lock_init(&data->wlock, WAKE_LOCK_SUSPEND, "vib_present");
	return 0;
}

void tspdrv_exit(void)
{
	printk(KERN_INFO "[VIB] cleanup_module.\n");

	vibe_terminate_timer();
	vibe_terminate();

	misc_deregister(&miscdev);
	wake_lock_destroy(&vibe_ddata->wlock);
}

MODULE_DESCRIPTION("Immersion TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");
