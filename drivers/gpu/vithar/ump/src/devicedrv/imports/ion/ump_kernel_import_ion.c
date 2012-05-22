/*
 *
 * (C) COPYRIGHT 2011-2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#include <ump/ump_kernel_interface.h>
#include <linux/dma-mapping.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>

#define MAX_FDS 3

extern struct platform_device *mali_device;

struct dmabuf_info {
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
};

struct dmabuf_wrapping_info
{
	int n;
	struct dmabuf_info info[];
};

static void import_ion_release_helper(struct dmabuf_info *info)
{
	dma_unmap_sg(&mali_device->dev, info->sg_table->sgl,
		     info->sg_table->nents, DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(info->attachment, info->sg_table,
				 DMA_BIDIRECTIONAL);
	dma_buf_detach(info->dma_buf, info->attachment);
	dma_buf_put(info->dma_buf);
}

static void import_ion_final_release_callback(const ump_dd_handle handle,
					      void * priv)
{
	struct dmabuf_wrapping_info *info;
	int i;

	BUG_ON(!priv);
	info = priv;

	for (i = 0; i < info->n; i++)
		import_ion_release_helper(&info->info[i]);

	module_put(THIS_MODULE);
	kfree(info);
}

static int import_ion_import_helper(int fd, struct dmabuf_info *info)
{
	int ret;

	info->dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(info->dma_buf))
		goto out;

	info->attachment = dma_buf_attach(info->dma_buf, &mali_device->dev);
	if (IS_ERR_OR_NULL(info->attachment))
		goto out1;

	info->sg_table = dma_buf_map_attachment(info->attachment,
						DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(info->sg_table))
		goto out2;

	ret = dma_map_sg(&mali_device->dev, info->sg_table->sgl,
		       info->sg_table->nents,
		       DMA_BIDIRECTIONAL);
	if (ret == 0)
		goto out3;
	return 0;
out3:
	dma_buf_unmap_attachment(info->attachment, info->sg_table,
				 DMA_BIDIRECTIONAL);
out2:
	dma_buf_detach(info->dma_buf, info->attachment);
out1:
	dma_buf_put(info->dma_buf);
out:
	return -1;
}

static ump_dd_handle import_ion_import(void *custom_session_data, void *pfd,
				       ump_alloc_flags flags)
{
	int *fds = pfd;
	ump_dd_handle ump_handle;
	struct scatterlist *sg;
	ump_dd_physical_block_64 *phys_blocks;
	int num_fds;
	int i, k = 0;
	unsigned int nents = 0;
	struct dmabuf_wrapping_info *info;

	BUG_ON(!custom_session_data);
	BUG_ON(!pfd);

	if (get_user(num_fds, fds++) || num_fds > MAX_FDS)
		return UMP_DD_INVALID_MEMORY_HANDLE;

	info = kzalloc(sizeof(*info) + (sizeof(struct dmabuf_info) * num_fds),
		       GFP_KERNEL);
	if (!info)
		return UMP_DD_INVALID_MEMORY_HANDLE;

	info->n = num_fds;
	for (i = 0; i < info->n; i++) {
		int fd, ret;
		get_user(fd, fds + i);
		ret = import_ion_import_helper(fd, &info->info[i]);
		if (ret)
			goto out;
		nents += info->info[i].sg_table->nents;
	}


	phys_blocks = vmalloc(nents * sizeof(*phys_blocks));
	if (phys_blocks == NULL)
		goto out;

	for (i = 0; i < info->n; i++) {
		struct dmabuf_info *buf_info = &info->info[i];
		unsigned long j;
		for_each_sg(buf_info->sg_table->sgl, sg,
			    buf_info->sg_table->nents, j) {
			phys_blocks[k].addr = sg_phys(sg);
			phys_blocks[k].size = sg_dma_len(sg);
			k++;
		}
	}
	BUG_ON(k != nents);

	ump_handle = ump_dd_create_from_phys_blocks_64(phys_blocks, nents,
						       flags, NULL,
						       import_ion_final_release_callback,
						       info);

	vfree(phys_blocks);

	if (ump_handle == UMP_DD_INVALID_MEMORY_HANDLE)
		goto out;
	/*
	 * As we have a final release callback installed
	 * we must keep the module locked until
	 * the callback has been triggered
	 * */
	__module_get(THIS_MODULE);
	return ump_handle;

out:
	pr_err("Failed to import handle\n");
	for (i--; i >= 0; i--)
		import_ion_release_helper(&info->info[i]);

	kfree(info);
	return UMP_DD_INVALID_MEMORY_HANDLE;
}

static int import_ion_session_begin(void** custom_session_data)
{
	*custom_session_data = mali_device;
	return 0;
}

static void import_ion_session_end(void* custom_session_data)
{
	return;
}


struct ump_import_handler import_handler_ion =
{
	.linux_module =  THIS_MODULE,
	.session_begin = import_ion_session_begin,
	.session_end =   import_ion_session_end,
	.import =        import_ion_import
};

static int __init import_ion_initialize_module(void)
{
	/* register with UMP */
	return ump_import_module_register(UMP_EXTERNAL_MEM_TYPE_ION,
					  &import_handler_ion);
}

static void __exit import_ion_cleanup_module(void)
{
	/* unregister import handler */
	ump_import_module_unregister(UMP_EXTERNAL_MEM_TYPE_ION);
}

/* Setup init and exit functions for this module */
module_init(import_ion_initialize_module);
module_exit(import_ion_cleanup_module);

/* And some module information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
