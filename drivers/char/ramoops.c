/*
 * RAM Oops/Panic logger
 *
 * Copyright (C) 2010 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright (C) 2011 Kees Cook <keescook@chromium.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/pstore.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ramoops.h>

#define RAMOOPS_KERNMSG_HDR "===="
#define MIN_MEM_SIZE 4096UL

static int ramoops_pstore_open(struct pstore_info *psi);
static int ramoops_pstore_close(struct pstore_info *psi);
static ssize_t ramoops_pstore_read(u64 *id, enum pstore_type_id *type,
				   struct timespec *time,
				   char **buf,
				   struct pstore_info *psi);
static int ramoops_pstore_write(enum pstore_type_id type,
				enum kmsg_dump_reason reason, u64 *id,
				unsigned int part,
				size_t size, struct pstore_info *psi);
static int ramoops_pstore_erase(enum pstore_type_id type, u64 id,
				struct pstore_info *psi);

struct ramoops_context {
	void *virt_addr;
	phys_addr_t phys_addr;
	unsigned long size;
	size_t record_size;
	unsigned int count;
	unsigned int max_count;
	unsigned int read_count;
	struct pstore_info pstore;
};

static struct ramoops_context oops_cxt = {
	.pstore = {
		.owner	= THIS_MODULE,
		.name	= "ramoops",
		.open	= ramoops_pstore_open,
		.close	= ramoops_pstore_close,
		.read	= ramoops_pstore_read,
		.write	= ramoops_pstore_write,
		.erase	= ramoops_pstore_erase,
	},
};

static int ramoops_pstore_open(struct pstore_info *psi)
{
	struct ramoops_context *cxt = &oops_cxt;

	cxt->read_count = 0;
	return 0;
}

static int ramoops_pstore_close(struct pstore_info *psi)
{
	return 0;
}

static ssize_t ramoops_pstore_read(u64 *id, enum pstore_type_id *type,
				   struct timespec *time,
				   char **buf,
				   struct pstore_info *psi)
{
	ssize_t size;
	char *rambuf;
	struct ramoops_context *cxt = &oops_cxt;

	if (cxt->read_count >= cxt->max_count)
		return -EINVAL;
	*id = cxt->read_count++;
	/* Only supports dmesg output so far. */
	*type = PSTORE_TYPE_DMESG;
	/* TODO(kees): Bogus time for the moment. */
	time->tv_sec = 0;
	time->tv_nsec = 0;

	rambuf = cxt->virt_addr + (*id * cxt->record_size);
	size = strnlen(rambuf, cxt->record_size);
	*buf = kmalloc(size, GFP_KERNEL);
	if (*buf == NULL)
		return -ENOMEM;
	memcpy(*buf, rambuf, size);

	return size;
}

static int ramoops_pstore_write(enum pstore_type_id type,
				enum kmsg_dump_reason reason,
				u64 *id,
				unsigned int part,
				size_t size, struct pstore_info *psi)
{
	char *buf;
	size_t res;
	struct timeval timestamp;
	struct ramoops_context *cxt = &oops_cxt;
	size_t available = cxt->record_size;

	/* Only store dmesg dumps. */
	if (type != PSTORE_TYPE_DMESG)
		return -EINVAL;

	/* Only store crash dumps. */
	if (reason != KMSG_DUMP_OOPS &&
	    reason != KMSG_DUMP_PANIC)
		return -EINVAL;

	/* Explicitly only take the first part of any new crash.
	 * If our buffer is larger than kmsg_bytes, this can never happen,
	 * and if our buffer is smaller than kmsg_bytes, we don't want the
	 * report split across multiple records. */
	if (part != 1)
		return -ENOSPC;

	buf = cxt->virt_addr + (cxt->count * cxt->record_size);

	res = sprintf(buf, "%s", RAMOOPS_KERNMSG_HDR);
	buf += res;
	available -= res;

	do_gettimeofday(&timestamp);
	res = sprintf(buf, "%lu.%lu\n", (long)timestamp.tv_sec, (long)timestamp.tv_usec);
	buf += res;
	available -= res;

	if (size > available)
		size = available;

	memcpy(buf, cxt->pstore.buf, size);
	memset(buf + size, '\0', available - size);

	cxt->count = (cxt->count + 1) % cxt->max_count;

	return 0;
}

static int ramoops_pstore_erase(enum pstore_type_id type, u64 id,
				struct pstore_info *psi)
{
	char *buf;
	struct ramoops_context *cxt = &oops_cxt;

	if (id >= cxt->max_count)
		return -EINVAL;

	buf = cxt->virt_addr + (id * cxt->record_size);
	memset(buf, '\0', cxt->record_size);

	return 0;
}

static int __init ramoops_probe(struct platform_device *pdev)
{
	struct ramoops_platform_data *pdata = pdev->dev.platform_data;
	struct ramoops_context *cxt = &oops_cxt;
	int err = -EINVAL;

	/* Only a single ramoops area allowed at a time, so fail extra
	 * probes.
	 */
	if (cxt->max_count)
		goto fail3;

	if (!pdata->mem_size || !pdata->record_size) {
		pr_err("The memory size and the record size must be "
			"non-zero\n");
		goto fail3;
	}

	pdata->mem_size = rounddown_pow_of_two(pdata->mem_size);
	pdata->record_size = rounddown_pow_of_two(pdata->record_size);

	/* Check for the minimum memory size */
	if (pdata->mem_size < MIN_MEM_SIZE &&
			pdata->record_size < MIN_MEM_SIZE) {
		pr_err("memory size too small, minium is %lu\n", MIN_MEM_SIZE);
		goto fail3;
	}

	if (pdata->mem_size < pdata->record_size) {
		pr_err("The memory size must be larger than the "
			"records size\n");
		goto fail3;
	}

	cxt->max_count = pdata->mem_size / pdata->record_size;
	cxt->count = 0;
	cxt->size = pdata->mem_size;
	cxt->phys_addr = pdata->mem_address;
	cxt->record_size = pdata->record_size;

	cxt->pstore.bufsize = cxt->record_size;
	cxt->pstore.buf = kmalloc(cxt->pstore.bufsize, GFP_KERNEL);
	spin_lock_init(&cxt->pstore.buf_lock);
	if (!cxt->pstore.buf) {
		pr_err("cannot allocate pstore buffer\n");
		goto fail4;
	}

	if (!request_mem_region(cxt->phys_addr, cxt->size, "ramoops")) {
		pr_err("request mem region failed\n");
		err = -EINVAL;
		goto fail3;
	}

	cxt->virt_addr = ioremap(cxt->phys_addr,  cxt->size);
	if (!cxt->virt_addr) {
		pr_err("ioremap failed\n");
		goto fail2;
	}

	err = pstore_register(&cxt->pstore);
	if (err) {
		pr_err("registering with pstore failed\n");
		goto fail1;
	}

	return 0;

fail1:
	iounmap(cxt->virt_addr);
fail2:
	release_mem_region(cxt->phys_addr, cxt->size);
	cxt->max_count = 0;
fail3:
	kfree(cxt->pstore.buf);
fail4:
	cxt->pstore.bufsize = 0;
	return err;
}

static int __exit ramoops_remove(struct platform_device *pdev)
{
	struct ramoops_context *cxt = &oops_cxt;

	/* TODO(kees): It shouldn't be possible to remove ramoops since
	 * pstore doesn't support unregistering yet. When it does, remove
	 * this early return and add the unregister where noted below.
	 */
	return -EBUSY;

	iounmap(cxt->virt_addr);
	release_mem_region(cxt->phys_addr, cxt->size);
	cxt->max_count = 0;

	/* TODO(kees): When pstore supports unregistering, call it here. */
	kfree(cxt->pstore.buf);
	cxt->pstore.bufsize = 0;

	return 0;
}

static struct platform_driver ramoops_driver = {
	.remove		= __exit_p(ramoops_remove),
	.driver		= {
		.name	= "ramoops",
		.owner	= THIS_MODULE,
	},
};

static int __init ramoops_init(void)
{
	return platform_driver_probe(&ramoops_driver, ramoops_probe);
}

static void __exit ramoops_exit(void)
{
	platform_driver_unregister(&ramoops_driver);
}

module_init(ramoops_init);
module_exit(ramoops_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marco Stornelli <marco.stornelli@gmail.com>");
MODULE_DESCRIPTION("RAM Oops/Panic logger/driver");
