#include "drmP.h"
#include "i915_drv.h"
#include <linux/dma-buf.h>

struct sg_table *i915_gem_map_dma_buf(struct dma_buf_attachment *attachment,
				      enum dma_data_direction dir)
					  
{
	struct drm_i915_gem_object *obj = attachment->dmabuf->priv;
	struct drm_device *dev = obj->base.dev;
	int npages = obj->base.size / PAGE_SIZE;
	struct sg_table *sg = NULL;
	int ret;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return NULL;

	if (!obj->pages) {
		ret = i915_gem_object_get_pages_gtt(obj, __GFP_NORETRY | __GFP_NOWARN);
		if (ret)
			goto out;
	}

	sg = drm_prime_pages_to_sg(obj->pages, npages);
out:
	mutex_unlock(&dev->struct_mutex);
	return sg;
}

void i915_gem_unmap_dma_buf(struct dma_buf_attachment *attachment,
			    struct sg_table *sg, enum dma_data_direction dir)
{
	sg_free_table(sg);
	kfree(sg);
}

void i915_gem_dmabuf_release(struct dma_buf *dma_buf)
{
	struct drm_i915_gem_object *obj = dma_buf->priv;

	if (obj->base.export_dma_buf == dma_buf) {
		/* drop the reference on the export fd holds */
		obj->base.export_dma_buf = NULL;
		drm_gem_object_unreference_unlocked(&obj->base);
	}
}

struct dma_buf_ops i915_dmabuf_ops =  {
	.map_dma_buf = i915_gem_map_dma_buf,
	.unmap_dma_buf = i915_gem_unmap_dma_buf,
	.release = i915_gem_dmabuf_release,
};

struct dma_buf * i915_gem_prime_export(struct drm_device *dev,
				struct drm_gem_object *gem_obj, int flags)
{
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);

	return dma_buf_export(obj, &i915_dmabuf_ops,
						  obj->base.size, 0600);
}

struct drm_gem_object * i915_gem_prime_import(struct drm_device *dev,
				struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	struct drm_i915_gem_object *obj;
	int npages;
	int size;
	struct scatterlist *iter;
	int ret;
	int i;

	/* need to attach */
	attach = dma_buf_attach(dma_buf, dev->dev);
	if (IS_ERR(attach))
		return ERR_PTR(-EINVAL);

	sg = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sg)) {
		ret = PTR_ERR(sg);
		goto fail_detach;
	}

	size = dma_buf->size;
	npages = size / PAGE_SIZE;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL) {
		ret = -ENOMEM;
		goto fail_unmap;
	}

	ret = drm_gem_private_object_init(dev, &obj->base, size);
	if (ret) {
		ret = -ENOMEM;
		kfree(obj);
		goto fail_unmap;
	}

	obj->pages = drm_malloc_ab(npages, sizeof(struct page *));
	if (obj->pages == NULL) {
		DRM_ERROR("obj pages is NULL %d\n", npages);
		ret = -ENOMEM;
		goto fail_unmap;
	}

	for_each_sg(sg->sgl, iter, npages, i)
		obj->pages[i] = sg_page(iter);

	obj->base.import_attach = attach;
	obj->sg = sg;

	return &obj->base;

fail_unmap:
	dma_buf_unmap_attachment(attach, sg, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dma_buf, attach);
	return ERR_PTR(ret);
}
