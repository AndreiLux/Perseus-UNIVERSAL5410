#include <linux/export.h>
#include <linux/dma-buf.h>
#include <linux/hash.h>
#include "drmP.h"

/*
 * DMA-BUF/GEM Object references and lifetime overview:
 *
 * On the export the dma_buf holds a reference to the exporting GEM
 * object. It takes this reference in handle_to_fd_ioctl, when it
 * first calls .prime_export and stores the exporting GEM object in
 * the dma_buf priv. This reference is released when the dma_buf
 * object goes away in the driver .release function.
 *
 * On the import the importing GEM object holds a reference to the
 * dma_buf (which in turn holds a ref to the exporting GEM object).
 * It takes that reference in the fd_to_handle ioctl.
 * It calls dma_buf_get, creates an attachment to it and stores the
 * attachment in the GEM object. When this attachment is destroyed
 * when the imported object is destroyed, we remove the attachment
 * and drop the reference to the dma_buf.
 *
 * Thus the chain of references always flows in one direction
 * (avoiding loops): importing_gem -> dmabuf -> exporting_gem
 */

struct drm_prime_member {
	struct list_head entry;
	struct dma_buf *dma_buf;
	uint32_t handle;
};

int drm_prime_handle_to_fd_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;
	struct drm_gem_object *obj;
	int flags;

	if (!drm_core_check_feature(dev, DRIVER_PRIME))
		return -EINVAL;

	if (!dev->driver->prime_export)
		return -ENOSYS;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj)
		return -ENOENT;

	/* don't allow imported buffers to be re-exported */
	if (obj->import_attach) {
		drm_gem_object_unreference_unlocked(obj);
		return -EINVAL;
	}

	/* we only want to pass DRM_CLOEXEC which is == O_CLOEXEC */
	flags = args->flags & DRM_CLOEXEC;
	if (obj->export_dma_buf) {
		get_file(obj->export_dma_buf->file);
		args->fd = dma_buf_fd(obj->export_dma_buf, flags);
		drm_gem_object_unreference_unlocked(obj);
	} else {
		obj->export_dma_buf = dev->driver->prime_export(dev, obj, flags);
		if (IS_ERR_OR_NULL(obj->export_dma_buf)) {
			/* normally the created dma-buf takes ownership of the ref,
			 * but if that fails then drop the ref
			 */
			drm_gem_object_unreference_unlocked(obj);
			return PTR_ERR(obj->export_dma_buf);
		}
		args->fd = dma_buf_fd(obj->export_dma_buf, flags);
	}
	return 0;
}

int drm_prime_fd_to_handle_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;
	struct dma_buf *dma_buf;
	struct drm_gem_object *obj;
	uint32_t handle;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_PRIME))
		return -EINVAL;

	if (!dev->driver->prime_import)
		return -ENOSYS;

	dma_buf = dma_buf_get(args->fd);
	if (IS_ERR(dma_buf))
		return PTR_ERR(dma_buf);

	ret = drm_prime_lookup_fd_handle_mapping(&file_priv->prime,
			dma_buf, &handle);
	if (!ret) {
		dma_buf_put(dma_buf);
		args->handle = handle;
		return 0;
	}

	/* never seen this one, need to import */
	obj = dev->driver->prime_import(dev, dma_buf);
	if (IS_ERR_OR_NULL(obj)) {
		ret = PTR_ERR(obj);
		goto fail_put;
	}

	ret = drm_gem_handle_create(file_priv, obj, &handle);
	drm_gem_object_unreference_unlocked(obj);
	if (ret)
		goto fail_put;

	ret = drm_prime_insert_fd_handle_mapping(&file_priv->prime,
			dma_buf, handle);
	if (ret)
		goto fail;

	args->handle = handle;
	return 0;

fail:
	/* hmm, if driver attached, we are relying on the free-object path
	 * to detach.. which seems ok..
	 */
	drm_gem_object_handle_unreference_unlocked(obj);
fail_put:
	dma_buf_put(dma_buf);
	return ret;
}

struct sg_table *drm_prime_pages_to_sg(struct page **pages, int nr_pages)
{
	struct sg_table *sg = NULL;
	struct scatterlist *iter;
	int i;
	int ret;

	sg = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sg)
		goto out;

	ret = sg_alloc_table(sg, nr_pages, GFP_KERNEL);
	if (ret)
		goto out;

	for_each_sg(sg->sgl, iter, nr_pages, i)
		sg_set_page(iter, pages[i], PAGE_SIZE, 0);

	return sg;
out:
	kfree(sg);
	return NULL;
}
EXPORT_SYMBOL(drm_prime_pages_to_sg);

/* helper function to cleanup a GEM/prime object */
void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg)
{
	struct dma_buf_attachment *attach;
	struct dma_buf *dma_buf;
	attach = obj->import_attach;
	if (sg)
		dma_buf_unmap_attachment(attach, sg, DMA_BIDIRECTIONAL);
	dma_buf = attach->dmabuf;
	dma_buf_detach(attach->dmabuf, attach);
	/* remove the reference */
	dma_buf_put(dma_buf);
}
EXPORT_SYMBOL(drm_prime_gem_destroy);

void drm_prime_init_file_private(struct drm_prime_file_private *prime_fpriv)
{
	INIT_LIST_HEAD(&prime_fpriv->head);
}
EXPORT_SYMBOL(drm_prime_init_file_private);

void drm_prime_destroy_file_private(struct drm_prime_file_private *prime_fpriv)
{
	struct drm_prime_member *member, *safe;
	list_for_each_entry_safe(member, safe, &prime_fpriv->head, entry) {
		list_del(&member->entry);
		kfree(member);
	}	
}
EXPORT_SYMBOL(drm_prime_destroy_file_private);

int drm_prime_insert_fd_handle_mapping(struct drm_prime_file_private *prime_fpriv, struct dma_buf *dma_buf, uint32_t handle)
{
	struct drm_prime_member *member;

	member = kmalloc(sizeof(*member), GFP_KERNEL);
	if (!member)
		return -ENOMEM;

	member->dma_buf = dma_buf;
	member->handle = handle;
	list_add(&member->entry, &prime_fpriv->head);
	return 0;
}
EXPORT_SYMBOL(drm_prime_insert_fd_handle_mapping);

int drm_prime_lookup_fd_handle_mapping(struct drm_prime_file_private *prime_fpriv, struct dma_buf *dma_buf, uint32_t *handle)
{
	struct drm_prime_member *member;

	list_for_each_entry(member, &prime_fpriv->head, entry) {
		if (member->dma_buf == dma_buf) {
			*handle = member->handle;
			return 0;
		}
	}
	return -ENOENT;
}
EXPORT_SYMBOL(drm_prime_lookup_fd_handle_mapping);

void drm_prime_remove_fd_handle_mapping(struct drm_prime_file_private *prime_fpriv, struct dma_buf *dma_buf)
{
	struct drm_prime_member *member, *safe;

	list_for_each_entry_safe(member, safe, &prime_fpriv->head, entry) {
		if (member->dma_buf == dma_buf) {
			list_del(&member->entry);
			kfree(member);
		}
	}
}
EXPORT_SYMBOL(drm_prime_remove_fd_handle_mapping);

static struct drm_gem_object *get_obj_from_dma_buf(struct drm_device *dev,
						   struct dma_buf *buf)
{
	struct drm_gem_object *obj;
	struct hlist_head *bucket =
		&dev->dma_buf_hash[hash_ptr(buf, DRM_DMA_BUF_HASH_BITS)];
	struct hlist_node *tmp;

	hlist_for_each_entry(obj, tmp, bucket, brown) {
		if (obj->export_dma_buf == buf)
			return obj;
	}

	return NULL;
}

int drm_prime_add_dma_buf(struct drm_device *dev, struct drm_gem_object *obj)
{
	struct drm_gem_object *tmp;
	unsigned long hash;

	if ((tmp = get_obj_from_dma_buf(dev, obj->export_dma_buf))) {
		DRM_DEBUG_PRIME("%p found DRM hash\n", obj->export_dma_buf);
		if (WARN_ON(tmp != obj))
			return -1;
		return 0;
	}

	hash = hash_ptr(obj->export_dma_buf, DRM_DMA_BUF_HASH_BITS);
	hlist_add_head(&obj->brown, &dev->dma_buf_hash[hash]);

	DRM_DEBUG_PRIME("%p added to DRM hash\n", obj->export_dma_buf);

	return 0;
}
EXPORT_SYMBOL(drm_prime_add_dma_buf);

int drm_prime_lookup_obj(struct drm_device *dev, struct dma_buf *buf,
			 struct drm_gem_object **obj)
{
	*obj = get_obj_from_dma_buf(dev, buf);
	return 0;
}
EXPORT_SYMBOL(drm_prime_lookup_obj);
