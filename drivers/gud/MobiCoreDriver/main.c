/**
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MobiCore driver module.(interface to the secure world SWD)
 * @addtogroup MCD_MCDIMPL_KMOD_IMPL
 * @{
 * @file
 * MobiCore Driver Kernel Module.
 * This driver represents the command proxy on the lowest layer, from the
 * secure world to the non secure world, and vice versa.

 * This driver offers IOCTL commands, for access to the secure world, and has
 * the interface from the secure world to the normal world.
 * The access to the driver is possible with a file descriptor,
 * which has to be created by the fd = open(/dev/mobicore) command or
 * fd = open(/dev/mobicore-user)
 */
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/completion.h>

#include "main.h"
#include "android.h"
#include "fastcall.h"

#include "arm.h"
#include "mem.h"
#include "ops.h"
#include "pm.h"
#include "debug.h"
#include "logging.h"

/** MobiCore interrupt context data */
struct mc_context ctx;

/*
#############################################################################
##
## Convenience functions for Linux API functions
##
#############################################################################*/
#ifdef CONFIG_SMP
/**
 * Force migration of current task to CPU0(where the monitor resides)
 *
 * @return Error code or 0 for success
 */
static int goto_cpu0(void)
{
	int ret = 0;
	struct cpumask mask = CPU_MASK_CPU0;

	MCDRV_DBG_VERBOSE("System has %d CPU's, we are on CPU #%d\n"
		"\tBinding this process to CPU #0.\n"
		"\tactive mask is %lx, setting it to mask=%lx\n",
		nr_cpu_ids,
		raw_smp_processor_id(),
		cpu_active_mask->bits[0],
		mask.bits[0]);
	ret = set_cpus_allowed_ptr(current, &mask);
	if (ret != 0)
		MCDRV_DBG_ERROR("set_cpus_allowed_ptr=%d.\n", ret);
	MCDRV_DBG_VERBOSE("And now we are on CPU #%d\n",
				raw_smp_processor_id());

	return ret;
}

/**
 * Restore CPU mask for current to ALL Cpus(reverse of goto_cpu0)
 *
 * @return Error code or 0 for success
 */
static int __attribute__ ((unused)) goto_all_cpu(void)
{
	int ret = 0;

	struct cpumask mask =  CPU_MASK_ALL;

	MCDRV_DBG_VERBOSE("System has %d CPU's, we are on CPU #%d\n"
		  "\tBinding this process to CPU #0.\n"
		  "\tactive mask is %lx, setting it to mask=%lx\n",
		  nr_cpu_ids,
		  raw_smp_processor_id(),
		  cpu_active_mask->bits[0],
		  mask.bits[0]);
	ret = set_cpus_allowed_ptr(current, &mask);
	if (ret != 0)
		MCDRV_DBG_ERROR("set_cpus_allowed_ptr=%d.\n", ret);
	MCDRV_DBG_VERBOSE("And now we are on CPU #%d\n",
				raw_smp_processor_id());

	return ret;
}
#else
#define goto_cpu0()	()
#define goto_all_cpu()	()
#endif

/* Get process context from file pointer */
struct mc_instance *get_instance(struct file *file)
{
	if (!file)
		return NULL;

	return (struct mc_instance *)(file->private_data);
}

/* Get a unique ID */
unsigned int get_unique_id(void)
{
	return (unsigned int)atomic_inc_return(&ctx.unique_counter);
}

/* Clears the reserved bit of each page and frees the pages */
static inline void free_continguous_pages(void *addr, unsigned int order)
{
	struct page *page = virt_to_page(addr);
	int i;
	for (i = 0; i < (1<<order); i++) {
		MCDRV_DBG_VERBOSE("free page at 0x%p\n", page);
		ClearPageReserved(page);
		page++;
	}

	MCDRV_DBG_VERBOSE("freeing addr:%p, order:%x\n", addr, order);
	free_pages((unsigned long)addr, order);
}

/* Frees the memory associated with a bufer */
static int free_buffer(struct mc_buffer *buffer)
{
	if (buffer->handle == 0)
		return -EINVAL;

	if (buffer->virt_kernel_addr == 0)
		return -EINVAL;

	MCDRV_DBG_VERBOSE("phys_addr=0x%p, virt_addr=0x%p\n",
			buffer->phys_addr,
			buffer->virt_kernel_addr);

	free_continguous_pages(buffer->virt_kernel_addr, buffer->order);
	memset(buffer, 0, sizeof(struct mc_buffer));
	return 0;
}

/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int mc_register_wsm_l2(struct mc_instance *instance,
	uint32_t buffer, uint32_t len,
	uint32_t *handle, uint32_t *phys)
{
	int ret = 0;
	struct mc_l2_table *table = NULL;
	struct task_struct *task = current;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (len == 0) {
		MCDRV_DBG_ERROR("len=0 is not supported!\n");
		return -EINVAL;
	}

	table = mc_alloc_l2_table(instance, task, (void *)buffer, len);

	if (IS_ERR(table)) {
		MCDRV_DBG_ERROR("new_used_l2_table() failed\n");
		return -EINVAL;
	}

	/* set response */
	*handle = table->handle;
	*phys = (uint32_t)table->phys;

	MCDRV_DBG_VERBOSE("handle: %d, phys=%p\n", *handle, (void *)*phys);

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/**
 *
 * @param instance
 * @param arg
 *
 * @return 0 if no error
 *
 */
static int mc_unregister_wsm_l2(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* free table (if no further locks exist) */
	mc_free_l2_table(instance, handle);

	return ret;
}

static int mc_lock_wsm_l2(struct mc_instance *instance, uint32_t handle,
	unsigned long *phys)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
		return -EPERM;
	}

	mutex_lock(&instance->lock);
	ret = mc_lock_l2_table(instance, handle, phys);
	mutex_unlock(&instance->lock);

	return ret;
}

static int mc_unlock_wsm_l2(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
		return -EPERM;
	}

	mutex_lock(&instance->lock);
	ret = mc_free_l2_table(instance, handle);
	mutex_unlock(&instance->lock);

	return ret;
}

/**
 * Free a WSM buffer allocated with mobicore_allocate_wsm
 * @param instance
 * @param handle		handle of the buffer
 *
 * @return 0 if no error
 *
 */
int mc_free_instance(struct mc_instance *instance, uint32_t handle)
{
	unsigned int i, ret;
	struct mc_buffer *buffer;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (mutex_lock_interruptible(&instance->lock))
		return -ERESTARTSYS;

	/* search for the given address in the buffers list */
	for (i = 0; i < MC_DRV_KMOD_MAX_CONTG_BUFFERS; i++) {
		buffer = &(instance->buffers[i]);
		if (buffer->handle == handle)
			break;
	}
	if (i == MC_DRV_KMOD_MAX_CONTG_BUFFERS) {
		MCDRV_DBG_ERROR("contigous buffer not found\n");
		ret = -EINVAL;
		goto err;
	}

	ret = free_buffer(buffer);
err:
	mutex_unlock(&instance->lock);
	return ret;
}


static int mc_get_buffer(struct mc_instance *instance,
	struct mc_buffer **buffer, unsigned long len)
{
	struct mc_buffer *cbuffer = 0;
	void *addr = 0;
	void *phys = 0;
	unsigned int order;
	unsigned long allocated_size;
	int i;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (len == 0) {
		MCDRV_DBG_ERROR("cannot allocate size 0\n");
		return -ENOMEM;
	}

	order = get_order(len);
	if (order > MAX_ORDER) {
		MCDRV_DBG_ERROR("Buffer size too large\n");
		return -ENOMEM;
	}
	allocated_size = (1 << order) * PAGE_SIZE;

	if (mutex_lock_interruptible(&instance->lock))
		return -ERESTARTSYS;

	/* search for a free slot in the buffer list. */
	for (i = 0; i < MC_DRV_KMOD_MAX_CONTG_BUFFERS; i++) {
		if (instance->buffers[i].handle == 0) {
			cbuffer = &instance->buffers[i];
			break;
		}
	}

	if (i == MC_DRV_KMOD_MAX_CONTG_BUFFERS) {
		MCDRV_DBG_WARN("MMAP_WSM request: "
			"no free slot for wsm buffer available\n");
		mutex_unlock(&instance->lock);
		return -ENOBUFS;
	}

	MCDRV_DBG_VERBOSE("size %ld -> order %d --> %ld (2^n pages)\n",
		len, order, allocated_size);

	addr = (void *)__get_free_pages(GFP_USER | __GFP_ZERO, order);

	if (addr == NULL) {
		MCDRV_DBG_WARN("get_free_pages failed, "
				"no contiguous region available.\n");
		cbuffer->handle = 0;
		return -ENOMEM;
	}
	phys = (void *)virt_to_phys(addr);
	cbuffer->handle = get_unique_id();
	cbuffer->phys_addr = phys;
	cbuffer->virt_kernel_addr = addr;
	mutex_unlock(&instance->lock);

	MCDRV_DBG("allocated phys=0x%p - 0x%p, "
		"size=%ld, kernel_virt=0x%p, handle=%d\n",
		phys, (void *)((unsigned int)phys+allocated_size),
		allocated_size, addr, cbuffer->handle);
	*buffer = cbuffer;
	return 0;
}

void *get_mci_base_phys(unsigned int len)
{
	if (ctx.mci_base.phys_addr)
		return ctx.mci_base.phys_addr;
	else {
		unsigned int order = get_order(len);
		ctx.mcp = NULL;
		ctx.mci_base.order = order;
		ctx.mci_base.virt_kernel_addr =
			(void *)__get_free_pages(GFP_USER | __GFP_ZERO, order);
		if (ctx.mci_base.virt_kernel_addr == NULL) {
			MCDRV_DBG_WARN("get_free_pages failed, "
				"no contiguous region available.\n");
			memset(&ctx.mci_base, 0, sizeof(ctx.mci_base));
			return NULL;
		}
		ctx.mci_base.phys_addr =
			(void *)virt_to_phys(ctx.mci_base.virt_kernel_addr);
		return ctx.mci_base.phys_addr;
	}
}

static int mc_fd_mmap(struct file *file, struct vm_area_struct *vmarea)
{
	struct mc_instance *instance = get_instance(file);
	unsigned long len = vmarea->vm_end - vmarea->vm_start;
	void *paddr = (void *)(vmarea->vm_pgoff << PAGE_SHIFT);
	unsigned int pfn;
	struct mc_buffer *contg_buffer = 0;
	int ret = 0, i;

	MCDRV_DBG_VERBOSE("enter (vma start=0x%p, size=%ld, mci=%p)\n",
		(void *)vmarea->vm_start, len, ctx.mci_base.phys_addr);

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (len == 0) {
		MCDRV_DBG_ERROR("cannot allocate size 0\n");
		return -ENOMEM;
	}
	if (paddr) {
		if (mutex_lock_interruptible(&instance->lock))
			return -ERESTARTSYS;

		/* search for a free slot in the buffer list. */
		for (i = 0; i < MC_DRV_KMOD_MAX_CONTG_BUFFERS; i++) {
			contg_buffer = &(instance->buffers[i]);
			if (contg_buffer->phys_addr == paddr)
				break;
		}
		mutex_unlock(&instance->lock);

		if (i == MC_DRV_KMOD_MAX_CONTG_BUFFERS) {
			MCDRV_DBG_WARN("MMAP_WSM : WSM buffer not found\n");
			return -ENOBUFS;
		}
	} else {
		if (!is_daemon(instance))
			return -EPERM;

		paddr = get_mci_base_phys(len);
		if (!paddr)
			return -EFAULT;
	}

	vmarea->vm_flags |= VM_RESERVED;
	/* Convert kernel address to user address. Kernel address begins
	 * at PAGE_OFFSET, user address range is below PAGE_OFFSET.
	 * Remapping the area is always done, so multiple mappings
	 * of one region are possible. Now remap kernel address
	 * space into user space */
	pfn = (unsigned int)paddr >> PAGE_SHIFT;
	ret = (int)remap_pfn_range(vmarea, vmarea->vm_start, pfn, len,
			vmarea->vm_page_prot);
	if (ret != 0)
		MCDRV_DBG_WARN("remap_pfn_range failed\n");

	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return ret;
}

/*
#############################################################################
##
## IoCtl handlers
##
#############################################################################*/
static inline int ioctl_check_pointer(unsigned int cmd, int __user *uarg)
{
	int err = 0;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	return 0;
}

/**
 * This function will be called from user space as ioctl(...).
 * @param file	pointer to file
 * @param cmd	command
 * @param arg	arguments
 *
 * @return int 0 for OK and an errno in case of error
 */
static long mc_fd_user_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mc_instance *instance = get_instance(file);
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (ioctl_check_pointer(cmd, uarg))
		return -EFAULT;

	switch (cmd) {
	case MC_IO_FREE:
		ret = mc_free_instance(instance, (uint32_t)arg);
		break;

	case MC_IO_REG_WSM:{
		struct mc_ioctl_reg_wsm reg;
		if (copy_from_user(&reg, uarg, sizeof(reg)))
			return -EFAULT;

		ret = mc_register_wsm_l2(instance, reg.buffer,
			reg.len, &reg.handle, &reg.table_phys);
		if (!ret) {
			if (copy_to_user(uarg, &reg, sizeof(reg))) {
				ret = -EFAULT;
				mc_unregister_wsm_l2(instance, reg.handle);
			}
		}
		break;
	}
	case MC_IO_UNREG_WSM:
		ret = mc_unregister_wsm_l2(instance, (uint32_t)arg);
		break;

	case MC_IO_VERSION:
		ret = put_user(mc_get_version(), uarg);
		if (ret)
			MCDRV_DBG_ERROR("IOCTL_GET_VERSION failed to put data");
		break;

	case MC_IO_MAP_WSM:{
		struct mc_ioctl_map map;
		struct mc_buffer *buffer = 0;
		if (copy_from_user(&map, uarg, sizeof(map)))
			return -EFAULT;

		/* Setup the WSM buffer structure! */
		if (mc_get_buffer(instance, &buffer, map.len))
			return -EFAULT;

		map.handle = buffer->handle;
		map.phys_addr = (unsigned long)buffer->phys_addr;
		map.reused = 0;
		if (copy_to_user(uarg, &map, sizeof(map)))
			ret = -EFAULT;

		ret = 0;
		break;
	}
	default:
		MCDRV_DBG_ERROR("unsupported cmd=%d\n", cmd);
		ret = -ENOIOCTLCMD;
		break;

	} /* end switch(cmd) */

#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif

	return (int)ret;
}

static long mc_fd_admin_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mc_instance *instance = get_instance(file);
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
		return -EPERM;
	}

	if (ioctl_check_pointer(cmd, uarg))
		return -EFAULT;

	switch (cmd) {
	case MC_IO_INIT: {
		struct mc_ioctl_init init;
		ctx.mcp = NULL;
		if (!ctx.mci_base.phys_addr) {
			MCDRV_DBG_ERROR("Cannot init MobiCore without MCI!");
			return -EINVAL;
		}
		if (copy_from_user(&init, uarg, sizeof(init)))
			return -EFAULT;

		ctx.mcp = ctx.mci_base.virt_kernel_addr + init.mcp_offset;
		ret = mc_init((uint32_t)ctx.mci_base.phys_addr, init.nq_offset,
			init.nq_length, init.mcp_offset, init.mcp_length);
		break;
	}
	case MC_IO_INFO: {
		struct mc_ioctl_info info;
		if (copy_from_user(&info, uarg, sizeof(info)))
			return -EFAULT;

		ret = mc_info(info.ext_info_id, &info.state,
			&info.ext_info);

		if (!ret) {
			if (copy_to_user(uarg, &info, sizeof(info)))
				ret = -EFAULT;
		}
		break;
	}
	case MC_IO_YIELD:
		ret = mc_yield();
		break;

	case MC_IO_NSIQ:
		ret = mc_nsiq();
		break;

	case MC_IO_LOCK_WSM: {
		uint32_t handle;
		unsigned long phys = 0;
		if (get_user(handle, uarg))
			return -EFAULT;

		ret = mc_lock_wsm_l2(instance, handle, &phys);
		if (put_user(phys, uarg)) {
			mc_unlock_wsm_l2(instance, handle);
			return -EFAULT;
		}
		break;
	}
	case MC_IO_UNLOCK_WSM:
		ret = mc_unlock_wsm_l2(instance, (uint32_t)arg);
		break;

	case MC_IO_MAP_MCI:{
		struct mc_ioctl_map map;
		if (copy_from_user(&map, uarg, sizeof(map)))
			return -EFAULT;

		map.reused = (ctx.mci_base.phys_addr != 0);
		map.phys_addr = (unsigned long)get_mci_base_phys(map.len);
		if (!map.phys_addr) {
			MCDRV_DBG_ERROR("Failed to setup MCI buffer!");
			return -EFAULT;
		}

		if (copy_to_user(uarg, &map, sizeof(map)))
			ret = -EFAULT;
		ret = 0;
		break;
	}
	case MC_IO_MAP_PWSM:{
		break;
	}

	/* The rest is handled commonly by user IOCTL */
	default:
		ret = mc_fd_user_ioctl(file, cmd, arg);
	} /* end switch(cmd) */

#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif

	return (int)ret;
}

/**
 * This function will be called from user space as read(...).
 * The read function is blocking until a interrupt occurs. In that case the
 * event counter is copied into user space and the function is finished.
 * @param *file
 * @param *buffer  buffer where to copy to(userspace)
 * @param buffer_len	 number of requested data
 * @param *pos	 not used
 * @return ssize_t	ok case: number of copied data
 *			error case: return errno
 */
static ssize_t mc_fd_read(struct file *file, char *buffer, size_t buffer_len,
	loff_t *pos)
{
	int ret = 0, ssiq_counter;
	struct mc_instance *instance = get_instance(file);

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* avoid debug output on non-error, because this is call quite often */
	MCDRV_DBG_VERBOSE("enter\n");

	/* only the MobiCore Daemon is allowed to call this function */
	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR("caller not MobiCore Daemon\n");
		return -EPERM;
	}

	if (buffer_len < sizeof(unsigned int)) {
		MCDRV_DBG_ERROR("invalid length\n");
		return -EINVAL;
	}

	for (;;) {
		if (wait_for_completion_interruptible(&ctx.isr_comp)) {
			MCDRV_DBG_VERBOSE("read interrupted\n");
			return -ERESTARTSYS;
		}

		ssiq_counter = atomic_read(&ctx.isr_counter);
		MCDRV_DBG_VERBOSE("ssiq_counter=%i, ctx.counter=%i\n",
			ssiq_counter, ctx.daemon_ctx.ssiq_counter);

		if (ssiq_counter != ctx.evt_counter) {
			/* read data and exit loop without error */
			ctx.evt_counter = ssiq_counter;
			ret = 0;
			break;
		}

		/* end loop if non-blocking */
		if (file->f_flags & O_NONBLOCK) {
			MCDRV_DBG_ERROR("non-blocking read\n");
			return -EAGAIN;
		}

		if (signal_pending(current)) {
			MCDRV_DBG_VERBOSE("received signal.\n");
			return -ERESTARTSYS;
		}
	}

	/* read data and exit loop */
	ret = copy_to_user(buffer, &ctx.evt_counter, sizeof(unsigned int));

	if (ret != 0) {
		MCDRV_DBG_ERROR("copy_to_user failed\n");
		return -EFAULT;
	}

	ret = sizeof(unsigned int);

	return (ssize_t)ret;
}

/**
 * Initialize a new mobicore API instance object
 *
 * @return Instance or NULL if no allocation was possible.
 */
struct mc_instance *mc_alloc_instance(void)
{
	struct mc_instance *instance;
	pid_t pid_vnr;

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (instance == NULL)
		return NULL;

	/* get a unique ID for this instance (PIDs are not unique) */
	instance->handle = get_unique_id();

	/* get the PID of the calling process. We avoid using
	 *	current->pid directly, as 2.6.24 introduced PID
	 *	namespaces. See also http://lwn.net/Articles/259217 */
	pid_vnr = task_pid_vnr(current);
	instance->pid_vnr = pid_vnr;

	mutex_init(&instance->lock);

	return instance;
}

/**
 * Release a mobicore instance object and all objects related to it
 * @param instance instance
 * @return 0 if Ok or -E ERROR
 */
int mc_release_instance(struct mc_instance *instance)
{
	int i;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (mutex_lock_interruptible(&instance->lock)) {
		return -ERESTARTSYS;
	}
	mc_clear_l2_tables(instance);

	/* release all mapped data */
	for (i = 0; i < MC_DRV_KMOD_MAX_CONTG_BUFFERS; i++)
		free_buffer(&instance->buffers[i]);

	mutex_unlock(&instance->lock);

	/* release instance context */
	kfree(instance);

	return 0;
}

/**
 * This function will be called from user space as fd = open(...).
 * A set of internal instance data are created and initialized.
 *
 * @param inode
 * @param file
 * @return 0 if OK or -ENOMEM if no allocation was possible.
 */
static int mc_fd_user_open(struct inode *inode, struct file *file)
{
	struct mc_instance	*instance;

	MCDRV_DBG_VERBOSE("enter\n");

	instance = mc_alloc_instance();
	if (instance == NULL)
		return -ENOMEM;

	/* store instance data reference */
	file->private_data = instance;

	return 0;
}

static int mc_fd_admin_open(struct inode *inode, struct file *file)
{
	struct mc_instance *instance;

	/* The daemon is already set so we can't allow anybody else to open
	 * the admin interface. */
	if (ctx.daemon_inst) {
		MCDRV_DBG_ERROR("Daemon is already connected");
		return -EPERM;
	}
	/* Setup the usual variables */
	mc_fd_user_open(inode, file);
	instance = get_instance(file);

	MCDRV_DBG("accept this as MobiCore Daemon\n");

	/* Set the caller's CPU mask to CPU0*/
	if (goto_cpu0() != 0) {
		mc_release_instance(instance);
		file->private_data = NULL;
		MCDRV_DBG("changing core failed!\n");
		return -EFAULT;
	}

	ctx.daemon_inst = instance;
	instance->admin = true;
	init_completion(&ctx.isr_comp);
	/* init ssiq event counter */
	ctx.evt_counter = atomic_read(&(ctx.isr_counter));

#ifdef MC_MEM_TRACES
	/* The traces have to be setup on CPU-0 since we must
	 * do a fastcall to MobiCore. */
	if (!ctx.mci_base.phys_addr)
		/* Do the work only if MCI base is not
		 * initialized properly */
		work_on_cpu(0, mobicore_log_setup, NULL);
#endif
	return 0;
}

/**
 * This function will be called from user space as close(...).
 * The instance data are freed and the associated memory pages are unreserved.
 *
 * @param inode
 * @param file
 *
 * @return 0
 */
static int mc_fd_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct mc_instance *instance = get_instance(file);

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* check if daemon closes us. */
	if (is_daemon(instance)) {
		/* TODO: cleanup? ctx.mc_l2_tables remains */
		MCDRV_DBG_WARN("WARNING: MobiCore Daemon died\n");
		ctx.daemon_inst = NULL;
	}

	ret = mc_release_instance(instance);

	/* ret is quite irrelevant here as most apps don't care about the
	 * return value from close() and it's quite difficult to recover */
	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);

	return (int)ret;
}

/*
 * This function represents the interrupt function of the mcDrvModule.
 * It signals by incrementing of an event counter and the start of the read
 * waiting queue, the read function a interrupt has occurred.
 */
static irqreturn_t mc_ssiq_isr(int intr, void *context)
{
	/* increment interrupt event counter */
	atomic_inc(&(ctx.isr_counter));

	/* signal the daemon */
	complete(&ctx.isr_comp);

	return IRQ_HANDLED;
}

/* function table structure of this device driver. */
static const struct file_operations mc_admin_fops = {
	.owner		= THIS_MODULE,
	.open		= mc_fd_admin_open,
	.release	= mc_fd_release,
	.unlocked_ioctl	= mc_fd_admin_ioctl,
	.mmap		= mc_fd_mmap,
	.read		= mc_fd_read,
};

static struct miscdevice mc_admin_device = {
	.name	= MC_ADMIN_DEVNODE,
	.mode	= (S_IRWXU),
	.minor	= MISC_DYNAMIC_MINOR,
	.fops	= &mc_admin_fops,
};

/* function table structure of this device driver. */
static const struct file_operations mc_user_fops = {
	.owner		= THIS_MODULE,
	.open		= mc_fd_user_open,
	.release	= mc_fd_release,
	.unlocked_ioctl	= mc_fd_user_ioctl,
	.mmap		= mc_fd_mmap,
};

static struct miscdevice mc_user_device = {
	.name	= MC_USER_DEVNODE,
	.mode	= (S_IRWXU | S_IRWXG | S_IRWXO),
	.minor	= MISC_DYNAMIC_MINOR,
	.fops	= &mc_user_fops,
};

/**
 * This function is called the kernel during startup or by a insmod command.
 * This device is installed and registered as miscdevice, then interrupt and
 * queue handling is set up
 */
static int __init mobicore_init(void)
{
	int ret = 0;

	MCDRV_DBG("enter (Build " __TIMESTAMP__ ")\n");
	MCDRV_DBG("mcDrvModuleApi version is %i.%i\n",
			MCDRVMODULEAPI_VERSION_MAJOR,
			MCDRVMODULEAPI_VERSION_MINOR);
#ifdef MOBICORE_COMPONENT_BUILD_TAG
	MCDRV_DBG("%s\n", MOBICORE_COMPONENT_BUILD_TAG);
#endif
	/* Hardware does not support ARM TrustZone -> Cannot continue! */
	if (!has_security_extensions()) {
		MCDRV_DBG_ERROR(
			"Hardware does't support ARM TrustZone!\n");
		return -ENODEV;
	}

	/* Running in secure mode -> Cannot load the driver! */
	if (is_secure_mode()) {
		MCDRV_DBG_ERROR("Running in secure MODE!\n");
		return -ENODEV;
	}

	init_completion(&ctx.isr_comp);
	/* set up S-SIQ interrupt handler */
	ret = request_irq(MC_INTR_SSIQ, mc_ssiq_isr, IRQF_TRIGGER_RISING,
			MC_ADMIN_DEVNODE, &ctx);
	if (ret != 0) {
		MCDRV_DBG_ERROR("interrupt request failed\n");
		goto error;
	}

#ifdef MC_PM_RUNTIME
	ret = mc_pm_initialize(&ctx);
	if (ret != 0) {
		MCDRV_DBG_ERROR("Power Management init failed!\n");
		goto free_isr;
	}
#endif

	ret = misc_register(&mc_admin_device);
	if (ret != 0) {
		MCDRV_DBG_ERROR("admin device register failed\n");
		goto free_isr;
	}

	ret = misc_register(&mc_user_device);
	if (ret != 0) {
		MCDRV_DBG_ERROR("user device register failed\n");
		goto free_admin;
	}

	/* initialize event counter for signaling of an IRQ to zero */
	atomic_set(&(ctx.isr_counter), 0);

	ret = mc_init_l2_tables();

	/* initialize unique number counter which we can use for
	 * handles. It is limited to 2^32, but this should be
	 * enough to be roll-over safe for us. We start with 1
	 * instead of 0. */
	atomic_set(&ctx.unique_counter, 1);

	memset(&ctx.mci_base, 0, sizeof(ctx.mci_base));
	MCDRV_DBG("initialized\n");
	return 0;

free_admin:
	misc_deregister(&mc_admin_device);
free_isr:
	free_irq(MC_INTR_SSIQ, &ctx);
error:
	return ret;
}

/**
 * This function removes this device driver from the Linux device manager .
 */
static void __exit mobicore_exit(void)
{
	MCDRV_DBG_VERBOSE("enter\n");
#ifdef MC_MEM_TRACES
	mobicore_log_free();
#endif

	mc_release_l2_tables();

#ifdef MC_PM_RUNTIME
	mc_pm_free();
#endif

	free_irq(MC_INTR_SSIQ, &ctx);

	misc_deregister(&mc_admin_device);
	misc_deregister(&mc_user_device);
	MCDRV_DBG_VERBOSE("exit");
}

/* Linux Driver Module Macros */
module_init(mobicore_init);
module_exit(mobicore_exit);
MODULE_AUTHOR("Giesecke & Devrient GmbH");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MobiCore driver");

/** @} */
