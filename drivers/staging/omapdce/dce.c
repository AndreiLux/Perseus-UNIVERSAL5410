/*
 * drivers/staging/omapdce/dce.c
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Rob Clark <rob.clark@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/rpmsg.h>

#include "../omapdrm/omap_drm.h"
#include "../omapdrm/omap_drv.h"
#include "omap_dce.h"

#include "dce_rpc.h"

/* TODO: split util stuff into other file..  maybe split out some of the
 * _process() munging stuff..
 */

#define DBG(fmt,...)  DRM_DEBUG(fmt"\n", ##__VA_ARGS__)
#define VERB(fmt,...) if (0) DRM_DEBUG(fmt, ##__VA_ARGS__) /* verbose debug */

/* Removeme.. */
static long mark(long *last)
{
    struct timeval t;
    do_gettimeofday(&t);
    if (last) {
        return t.tv_usec - *last;
    }
    return t.tv_usec;
}

#define MAX_ENGINES	32
#define MAX_CODECS	32
#define MAX_LOCKED_BUFFERS	32  /* actually, 16 should be enough if reorder queue is disabled */
#define MAX_BUFFER_OBJECTS	((2 * MAX_LOCKED_BUFFERS) + 4)
#define MAX_TRANSACTIONS	MAX_CODECS

struct dce_buffer {
	int32_t id;		/* zero for unused */
	struct drm_gem_object *y, *uv;
};

struct dce_engine {
	uint32_t engine;
};

struct dce_codec {
	enum omap_dce_codec codec_id;
	uint32_t codec;
	struct dce_buffer locked_buffers[MAX_LOCKED_BUFFERS];
};

struct dce_file_priv {
	struct drm_device *dev;
	struct drm_file *file;

	/* NOTE: engine/codec end up being pointers (or similar) on
	 * coprocessor side.. so these are not exposed directly to
	 * userspace.  Instead userspace sees per-file unique handles
	 * which index into the table of engines/codecs.
	 */
	struct dce_engine engines[MAX_ENGINES];

	struct dce_codec codecs[MAX_CODECS];

	/* the token returned to userspace is an index into this table
	 * of msg request-ids.  This avoids any chance that userspace might
	 * try to guess another processes txn req_id and try to intercept
	 * the other processes reply..
	 */
	uint16_t req_ids[MAX_CODECS];
	atomic_t next_token;
};

/* per-transaction data.. indexed by req_id%MAX_REQUESTS
 */

struct omap_dce_txn {
	struct dce_file_priv *priv;  /* file currently using thix txn slot */
	struct dce_rpc_hdr *rsp;
	int len;
	int bo_count;
	struct drm_gem_object *objs[MAX_BUFFER_OBJECTS];
};
static struct omap_dce_txn txns[MAX_TRANSACTIONS];

/* note: eventually we could perhaps have per drm_device private data
 * and pull all this into a struct.. but in rpmsg_cb we don't have a
 * way to get at that, so for now all global..
 */
struct rpmsg_channel *rpdev;
static int dce_mapper_id = -1;
static atomic_t next_req_id = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(wq);
static DEFINE_MUTEX(lock);  // TODO probably more locking needed..

/*
 * Utils:
 */

#define hdr(r) ((void *)(r))

/* initialize header and assign request id: */
#define MKHDR(x) (struct dce_rpc_hdr){ .msg_id = DCE_RPC_##x, .req_id = atomic_inc_return(&next_req_id) }

/* GEM buffer -> paddr, plus add the buffer to the txn bookkeeping of
 * associated buffers that eventually need to be cleaned up when the
 * transaction completes
 */
static struct drm_gem_object * get_paddr(struct dce_file_priv *priv,
		struct dce_rpc_hdr *req, uint32_t *paddrp, int bo)
{
	struct omap_dce_txn *txn = &txns[req->req_id % ARRAY_SIZE(txns)];
	struct drm_gem_object *obj;
	dma_addr_t paddr;
	int ret;
long t;

	if (txn->bo_count >= ARRAY_SIZE(txn->objs)) {
		DBG("too many buffers!");
		return ERR_PTR(-ENOMEM);
	}

	obj = drm_gem_object_lookup(priv->dev, priv->file, bo);
	if (!obj) {
		DBG("bad handle: %d", bo);
		return ERR_PTR(-ENOENT);
	}

t = mark(NULL);
	ret = omap_gem_get_paddr(obj, &paddr, true);
DBG("get_paddr in %ld us", mark(&t));
	if (ret) {
		DBG("cannot map: %d", ret);
		return ERR_PTR(ret);
	}

	/* the coproc can only see 32bit addresses.. this might need
	 * to be revisited in the future with some conversion between
	 * device address and host address.  But currently they are
	 * the same.
	 */
	*paddrp = (uint32_t)paddr;

	txn->objs[txn->bo_count++] = obj;

	DBG("obj=%p", obj);

	return obj;
}

static int rpsend(struct dce_file_priv *priv, uint32_t *token,
		struct dce_rpc_hdr *req, int len)
{
	struct omap_dce_txn *txn =
			&txns[req->req_id % ARRAY_SIZE(txns)];

	WARN_ON(txn->priv);

	/* assign token: */
	if (token) {
		*token = atomic_inc_return(&priv->next_token) + 1;
		priv->req_ids[(*token-1) % ARRAY_SIZE(priv->req_ids)] = req->req_id;

		txn->priv = priv;

		// XXX  wait for paddrs to become valid!

	} else {
		/* message with no response: */
		req->req_id = 0xffff;          /* just for debug */
		WARN_ON(txn->bo_count > 0);    /* this is not valid */

		memset(txn, 0, sizeof(*txn));
	}

	return rpmsg_send(rpdev, req, len);
}

static int rpwait(struct dce_file_priv *priv, uint32_t token,
		struct dce_rpc_hdr **rsp, int len)
{
	uint16_t req_id = priv->req_ids[(token-1) % ARRAY_SIZE(priv->req_ids)];
	struct omap_dce_txn *txn = &txns[req_id % ARRAY_SIZE(txns)];
	int ret;

	if (txn->priv != priv) {
		dev_err(priv->dev->dev, "not my txn\n");
		return -EINVAL;
	}

	ret = wait_event_interruptible(wq, (txn->rsp));
	if (ret) {
		DBG("ret=%d", ret);
		return ret;
	}

	if (txn->len < len) {
		dev_err(priv->dev->dev, "rsp too short: %d < %d\n", txn->len, len);
		ret = -EINVAL;
		goto fail;
	}

	*rsp = txn->rsp;

fail:
	/* clear out state: */
	memset(txn, 0, sizeof(*txn));

	return ret;
}

static void txn_cleanup(struct omap_dce_txn *txn)
{
	int i;

	mutex_lock(&lock);

	kfree(txn->rsp);
	txn->rsp = NULL;

	/* unpin/unref buffers associated with this transaction */
	for (i = 0; i < txn->bo_count; i++) {
		struct drm_gem_object *obj = txn->objs[i];
		DBG("obj=%p", obj);
		omap_gem_put_paddr(obj);
		drm_gem_object_unreference_unlocked(obj);
	}
	txn->bo_count = 0;

	mutex_unlock(&lock);
}

static void rpcomplete(struct dce_rpc_hdr *rsp, int len)
{
	struct omap_dce_txn *txn = &txns[rsp->req_id % ARRAY_SIZE(txns)];

	if (!txn->priv) {
		/* we must of cleaned up already (killed process) */
		printk(KERN_ERR "dce: unexpected response.. killed process?\n");
		kfree(rsp);
		return;
	}

	txn_cleanup(txn);

	txn->len = len;
	txn->rsp = rsp;

	wake_up_all(&wq);
}

static int rpabort(struct dce_rpc_hdr *req, int ret)
{
	struct omap_dce_txn *txn =
			&txns[req->req_id % ARRAY_SIZE(txns)];

	DBG("txn failed: msg_id=%u, req_id=%u, ret=%d",
			(uint32_t)req->msg_id, (uint32_t)req->req_id, ret);

	txn_cleanup(txn);

	/* clear out state: */
	memset(txn, 0, sizeof(*txn));

	return ret;
}

/* helpers for tracking engine instances and mapping engine handle to engine
 * instance:
 */

static uint32_t engine_register(struct dce_file_priv *priv, uint32_t engine)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(priv->engines); i++) {
		if (!priv->engines[i].engine) {
			priv->engines[i].engine = engine;
			return i+1;
		}
	}
	dev_err(priv->dev->dev, "too many engines\n");
	return 0;
}

static void engine_unregister(struct dce_file_priv *priv, uint32_t eng_handle)
{
	priv->engines[eng_handle-1].engine = 0;
}

static bool engine_valid(struct dce_file_priv *priv, uint32_t eng_handle)
{
	return (eng_handle > 0) &&
			(eng_handle <= ARRAY_SIZE(priv->engines)) &&
			(priv->engines[eng_handle-1].engine);
}

static int engine_get(struct dce_file_priv *priv, uint32_t eng_handle,
		uint32_t *engine)
{
	if (!engine_valid(priv, eng_handle))
		return -EINVAL;
	*engine = priv->engines[eng_handle-1].engine;
	return 0;
}

/* helpers for tracking codec instances and mapping codec handle to codec
 * instance:
 */

static void codec_unlockbuf(struct dce_file_priv *priv,
		uint32_t codec_handle, int32_t id);

static uint32_t codec_register(struct dce_file_priv *priv, uint32_t codec,
		enum omap_dce_codec codec_id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(priv->codecs); i++) {
		if (!priv->codecs[i].codec) {
			priv->codecs[i].codec_id = codec_id;
			priv->codecs[i].codec = codec;
			return i+1;
		}
	}
	dev_err(priv->dev->dev, "too many codecs\n");
	return 0;
}

static void codec_unregister(struct dce_file_priv *priv,
		uint32_t codec_handle)
{
	codec_unlockbuf(priv, codec_handle, 0);
	priv->codecs[codec_handle-1].codec = 0;
	priv->codecs[codec_handle-1].codec_id = 0;
}

static bool codec_valid(struct dce_file_priv *priv, uint32_t codec_handle)
{
	return (codec_handle > 0) &&
			(codec_handle <= ARRAY_SIZE(priv->codecs)) &&
			(priv->codecs[codec_handle-1].codec);
}

static int codec_get(struct dce_file_priv *priv, uint32_t codec_handle,
		uint32_t *codec, uint32_t *codec_id)
{
	if (!codec_valid(priv, codec_handle))
		return -EINVAL;
	*codec    = priv->codecs[codec_handle-1].codec;
	*codec_id = priv->codecs[codec_handle-1].codec_id;
	return 0;
}

static int codec_lockbuf(struct dce_file_priv *priv,
		uint32_t codec_handle, int32_t id,
		struct drm_gem_object *y, struct drm_gem_object *uv)
{
	struct dce_codec *codec = &priv->codecs[codec_handle-1];
	int i;

	for (i = 0; i < ARRAY_SIZE(codec->locked_buffers); i++) {
		struct dce_buffer *buf = &codec->locked_buffers[i];
		if (buf->id == 0) {
			dma_addr_t paddr;

			DBG("lock[%d]: y=%p, uv=%p", id, y, uv);

			/* for now, until the codecs support relocated buffers, keep
			 * an extra ref and paddr to keep it pinned
			 */
			drm_gem_object_reference(y);
			omap_gem_get_paddr(y, &paddr, true);

			if (uv) {
				drm_gem_object_reference(uv);
				omap_gem_get_paddr(uv, &paddr, true);
			}

			buf->id = id;
			buf->y = y;
			buf->uv = uv;

			return 0;
		}
	}
	dev_err(priv->dev->dev, "too many locked buffers!\n");
	return -ENOMEM;
}

static void codec_unlockbuf(struct dce_file_priv *priv,
		uint32_t codec_handle, int32_t id)
{
	struct dce_codec *codec = &priv->codecs[codec_handle-1];
	int i;

	for (i = 0; i < ARRAY_SIZE(codec->locked_buffers); i++) {
		struct dce_buffer *buf = &codec->locked_buffers[i];
		/* if id==0, unlock all buffers.. */
		if (((id == 0) && (buf->id != 0)) ||
			((id != 0) && (buf->id == id))) {
			struct drm_gem_object *y, *uv;

			y  = buf->y;
			uv = buf->uv;

			DBG("unlock[%d]: y=%p, uv=%p", buf->id, y, uv);

			/* release extra ref */
			omap_gem_put_paddr(y);
			drm_gem_object_unreference_unlocked(y);

			if (uv) {
				omap_gem_put_paddr(uv);
				drm_gem_object_unreference_unlocked(uv);
			}

			buf->id = 0;
			buf->y = NULL;
			buf->uv = NULL;

			/* if id==0, unlock all buffers.. */
			if (id != 0)
				return;
		}
	}
}


/*
 * Ioctl Handlers:
 */

static int engine_close(struct dce_file_priv *priv, uint32_t engine);
static int codec_delete(struct dce_file_priv *priv, uint32_t codec,
		enum omap_dce_codec codec_id);

static int ioctl_engine_open(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	struct drm_omap_dce_engine_open *arg = data;
	struct dce_rpc_engine_open_rsp *rsp;
	int ret;

	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		struct dce_rpc_engine_open_req req = {
				.hdr = MKHDR(ENGINE_OPEN),
		};
		strncpy(req.name, arg->name, sizeof(req.name));
		ret = rpsend(priv, &arg->token, hdr(&req), sizeof(req));
		if (ret)
			return ret;
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret)
		return ret;

	arg->eng_handle = engine_register(priv, rsp->engine);
	arg->error_code = rsp->error_code;

	if (!engine_valid(priv, arg->eng_handle)) {
		engine_close(priv, rsp->engine);
		ret = -ENOMEM;
	}

	kfree(rsp);

	return ret;
}

static int engine_close(struct dce_file_priv *priv, uint32_t engine)
{
	struct dce_rpc_engine_close_req req = {
			.hdr = MKHDR(ENGINE_CLOSE),
			.engine = engine,
	};
	return rpsend(priv, NULL, hdr(&req), sizeof(req));
}

static int ioctl_engine_close(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	struct drm_omap_dce_engine_close *arg = data;
	uint32_t engine;
	int ret;

	ret = engine_get(priv, arg->eng_handle, &engine);
	if (ret)
		return ret;

	engine_unregister(priv, arg->eng_handle);

	return engine_close(priv, engine);
}

static int ioctl_codec_create(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	struct drm_omap_dce_codec_create *arg = data;
	struct dce_rpc_codec_create_rsp *rsp;
	int ret;

	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		struct dce_rpc_codec_create_req req = {
				.hdr = MKHDR(CODEC_CREATE),
				.codec_id = arg->codec_id,
		};

		strncpy(req.name, arg->name, sizeof(req.name));

		ret = engine_get(priv, arg->eng_handle, &req.engine);
		if (ret)
			return ret;

		ret = PTR_RET(get_paddr(priv, hdr(&req), &req.sparams, arg->sparams_bo));
		if (ret)
			goto rpsend_out;

		ret = rpsend(priv, &arg->token, hdr(&req), sizeof(req));
rpsend_out:
		if (ret)
			return rpabort(hdr(&req), ret);
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret)
		return ret;

	arg->codec_handle = codec_register(priv, rsp->codec, arg->codec_id);

	if (!codec_valid(priv, arg->codec_handle)) {
		codec_delete(priv, rsp->codec, arg->codec_id);
		ret = -ENOMEM;
	}

	kfree(rsp);

	return ret;
}

static int ioctl_codec_control(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	struct drm_omap_dce_codec_control *arg = data;
	struct dce_rpc_codec_control_rsp *rsp;
	int ret;

	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		struct dce_rpc_codec_control_req req = {
				.hdr = MKHDR(CODEC_CONTROL),
				.cmd_id = arg->cmd_id,
		};

		ret = codec_get(priv, arg->codec_handle, &req.codec, &req.codec_id);
		if (ret)
			return ret;

		ret = PTR_RET(get_paddr(priv, hdr(&req), &req.dparams, arg->dparams_bo));
		if (ret)
			goto rpsend_out;

		ret = PTR_RET(get_paddr(priv, hdr(&req), &req.status, arg->status_bo));
		if (ret)
			goto rpsend_out;

		ret = rpsend(priv, &arg->token, hdr(&req), sizeof(req));
rpsend_out:
		if (ret)
			return rpabort(hdr(&req), ret);
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret)
		return ret;

	arg->result = rsp->result;

	kfree(rsp);

	return 0;
}

struct viddec3_in_args {
	int32_t size;		/* struct size */
	int32_t num_bytes;
	int32_t input_id;
};
struct videnc2_in_args {
	int32_t size;
	int32_t input_id;
	int32_t control;
};

union xdm2_buf_size {
    struct {
        int32_t width;
        int32_t height;
    } tiled;
    int32_t bytes;
};
struct xdm2_single_buf_desc {
    uint32_t buf;
    int16_t  mem_type;	/* XXX should be XDM_MEMTYPE_BO */
    int16_t  usage_mode;
    union xdm2_buf_size buf_size;
    int32_t  accessMask;
};
struct xdm2_buf_desc {
    int32_t num_bufs;
    struct xdm2_single_buf_desc descs[16];
};

struct video2_buf_desc {
    int32_t num_planes;
    int32_t num_meta_planes;
    int32_t data_layout;
    struct xdm2_single_buf_desc plane_desc[3];
    struct xdm2_single_buf_desc metadata_plane_desc[3];
    /* rest of the struct isn't interesting to kernel.. if you are
     * curious look at IVIDEO2_BufDesc in ivideo.h in codec-engine
     */
    uint32_t data[30];
};
#define XDM_MEMTYPE_RAW       0
#define XDM_MEMTYPE_TILED8    1
#define XDM_MEMTYPE_TILED16   2
#define XDM_MEMTYPE_TILED32   3
#define XDM_MEMTYPE_TILEDPAGE 4

/* copy_from_user helper that also checks to avoid overrunning
 * the 'to' buffer and advances dst ptr
 */
static inline int cfu(void **top, uint64_t from, int n, void *end)
{
	void *to = *top;
	int ret;
	if ((to + n) >= end) {
		DBG("dst buffer overflow!");
		return -EFAULT;
	}
	ret = copy_from_user(to, (char __user *)(uintptr_t)from, n);
	*top = to + n;
	return ret;
}

static inline struct drm_gem_object * handle_single_buf_desc(
		struct dce_file_priv *priv, struct dce_rpc_hdr *req,
		struct xdm2_single_buf_desc *desc)
{
	struct drm_gem_object *obj;
	uint32_t flags;

	/* maybe support remapping user ptrs later on.. */
	if (desc->mem_type != XDM_MEMTYPE_BO)
		return ERR_PTR(-EINVAL);

	obj = get_paddr(priv, req, &desc->buf, desc->buf);
	if (IS_ERR(obj))
		return obj;

	flags = omap_gem_flags(obj);
	switch(flags & OMAP_BO_TILED) {
	case OMAP_BO_TILED_8:
		desc->mem_type = XDM_MEMTYPE_TILED8;
		break;
	case OMAP_BO_TILED_16:
		desc->mem_type = XDM_MEMTYPE_TILED16;
		break;
	case OMAP_BO_TILED_32:
		desc->mem_type = XDM_MEMTYPE_TILED32;
		break;
	default:
		// XXX this is where it gets a bit messy..  some codecs
		// might want to see XDM_MEMTYPE_RAW for bitstream buffers
		desc->mem_type = XDM_MEMTYPE_TILEDPAGE;
		break;
	}

	if (flags & OMAP_BO_TILED) {
		uint16_t w, h;
		omap_gem_tiled_size(obj, &w, &h);
		desc->buf_size.tiled.width = w;
		desc->buf_size.tiled.height = h;
	} else {
		desc->buf_size.bytes = obj->size;
	}

	// XXX not sure if the codecs care about usage_mode.. but we
	// know if the buffer is cached or not so we could set DATASYNC
	// bit if needed..

	return obj;
}

static inline int handle_buf_desc(struct dce_file_priv *priv,
		void **ptr, void *end, struct dce_rpc_hdr *req, uint64_t usr,
		struct drm_gem_object **o1, struct drm_gem_object **o2, uint8_t *len)
{
	struct xdm2_buf_desc *bufs = *ptr;
	int i, ret;

	/* read num_bufs field: */
	ret = cfu(ptr, usr, 4, end);
	if (ret)
		return ret;

	/* read rest of structure: */
	ret = cfu(ptr, usr+4, bufs->num_bufs * sizeof(bufs->descs[0]), end);
	if (ret)
		return ret;

	*len = (4 + bufs->num_bufs * sizeof(bufs->descs[0])) / 4;

	/* handle buffer mapping.. */
	for (i = 0; i < bufs->num_bufs; i++) {
		struct drm_gem_object *obj =
				handle_single_buf_desc(priv, req, &bufs->descs[i]);
		if (IS_ERR(obj)) {
			return PTR_ERR(obj);
		}
		if (i == 0)
			*o1 = obj;
		if (o2 && (i == 1))
			*o2 = obj;

	}

	return 0;
}

/*
 *   VIDDEC3_process			VIDENC2_process
 *   VIDDEC3_InArgs *inArgs		VIDENC2_InArgs *inArgs
 *   XDM2_BufDesc *outBufs		XDM2_BufDesc *outBufs
 *   XDM2_BufDesc *inBufs		VIDEO2_BufDesc *inBufs
 */

static inline int handle_videnc2(struct dce_file_priv *priv,
		void **ptr, void *end, int32_t *input_id,
		struct dce_rpc_codec_process_req *req,
		struct drm_omap_dce_codec_process *arg)
{
	WARN_ON(1); // XXX not implemented
	//	codec_lockbuf(priv, arg->codec_handle, in_args->input_id, y, uv);
	return -EFAULT;
}

static inline int handle_viddec3(struct dce_file_priv *priv,
		void **ptr, void *end, int32_t *input_id,
		struct dce_rpc_codec_process_req *req,
		struct drm_omap_dce_codec_process *arg)
{
	struct drm_gem_object *in, *y = NULL, *uv = NULL;
	struct viddec3_in_args *in_args = *ptr;
	int ret;

	/* handle in_args: */
	ret = cfu(ptr, arg->in_args, sizeof(*in_args), end);
	if (ret)
		return ret;

	if (in_args->size > sizeof(*in_args)) {
		int sz = in_args->size - sizeof(*in_args);
		/* this param can be variable length */
		ret = cfu(ptr, arg->in_args + sizeof(*in_args), sz, end);
		if (ret)
			return ret;
		/* in case the extra part size is not multiple of 4 */
		*ptr += round_up(sz, 4) - sz;
	}

	req->in_args_len = round_up(in_args->size, 4) / 4;

	/* handle out_bufs: */
	ret = handle_buf_desc(priv, ptr, end, hdr(req), arg->out_bufs,
			&y, &uv, &req->out_bufs_len);
	if (ret)
		return ret;

	/* handle in_bufs: */
	ret = handle_buf_desc(priv, ptr, end, hdr(req), arg->in_bufs,
			&in, NULL, &req->in_bufs_len);
	if (ret)
		return ret;

	*input_id = in_args->input_id;
	codec_lockbuf(priv, arg->codec_handle, in_args->input_id, y, uv);

	return 0;
}

#define RPMSG_BUF_SIZE		(512)  // ugg, would be nice not to hard-code..

static int ioctl_codec_process(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	struct drm_omap_dce_codec_process *arg = data;
	struct dce_rpc_codec_process_rsp *rsp;
	int ret, i;

	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		/* worst-case size allocation.. */
		struct dce_rpc_codec_process_req *req = kzalloc(RPMSG_BUF_SIZE, GFP_KERNEL);
		void *ptr = &req->data[0];
		void *end = ((void *)req) + RPMSG_BUF_SIZE;
		int32_t input_id = 0;

		req->hdr = MKHDR(CODEC_PROCESS);

		ret = codec_get(priv, arg->codec_handle, &req->codec, &req->codec_id);
		if (ret)
			goto rpsend_out;

		ret = PTR_RET(get_paddr(priv, hdr(&req), &req->out_args, arg->out_args_bo));
		if (ret)
			return rpabort(hdr(req), ret);

		/* the remainder of the req varies depending on codec family */
		switch (req->codec_id) {
		case OMAP_DCE_VIDENC2:
			ret = handle_videnc2(priv, &ptr, end, &input_id, req, arg);
			break;
		case OMAP_DCE_VIDDEC3:
			ret = handle_viddec3(priv, &ptr, end, &input_id, req, arg);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret)
			goto rpsend_out;

		ret = rpsend(priv, &arg->token, hdr(req), ptr - (void *)req);
rpsend_out:
		kfree(req);
		if (ret) {
			/* if input buffer is already locked, unlock it now so we
			 * don't have a leak:
			 */
			if (input_id)
				codec_unlockbuf(priv, arg->codec_handle, input_id);
			return rpabort(hdr(req), ret);
		}
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret)
		return ret;

	for (i = 0; i < rsp->count; i++) {
		codec_unlockbuf(priv, arg->codec_handle, rsp->freebuf_ids[i]);
	}

	arg->result = rsp->result;

	kfree(rsp);

	return 0;
}

static int codec_delete(struct dce_file_priv *priv, uint32_t codec,
		enum omap_dce_codec codec_id)
{
	struct dce_rpc_codec_delete_req req = {
			.hdr = MKHDR(CODEC_DELETE),
			.codec_id = codec_id,
			.codec = codec,
	};
	return rpsend(priv, NULL, hdr(&req), sizeof(req));
}

static int ioctl_codec_delete(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	struct drm_omap_dce_codec_delete *arg = data;
	uint32_t codec, codec_id;
	int ret;

	ret = codec_get(priv, arg->codec_handle, &codec, &codec_id);
	if (ret)
		return ret;

	codec_unregister(priv, arg->codec_handle);

	return codec_delete(priv, codec, codec_id);
}

/* NOTE: these are not public because the actual ioctl NR is dynamic..
 * use drmCommandXYZ(fd, dce_base + idx, ..)
 */
#define DRM_IOCTL_OMAP_DCE_ENGINE_OPEN		DRM_IOWR(DRM_OMAP_DCE_ENGINE_OPEN, struct drm_omap_dce_engine_open)
#define DRM_IOCTL_OMAP_DCE_ENGINE_CLOSE		DRM_IOW (DRM_OMAP_DCE_ENGINE_CLOSE, struct drm_omap_dce_engine_close)
#define DRM_IOCTL_OMAP_DCE_CODEC_CREATE		DRM_IOWR(DRM_OMAP_DCE_CODEC_CREATE, struct drm_omap_dce_codec_create)
#define DRM_IOCTL_OMAP_DCE_CODEC_CONTROL	DRM_IOWR(DRM_OMAP_DCE_CODEC_CONTROL, struct drm_omap_dce_codec_control)
#define DRM_IOCTL_OMAP_DCE_CODEC_PROCESS	DRM_IOWR(DRM_OMAP_DCE_CODEC_PROCESS, struct drm_omap_dce_codec_process)
#define DRM_IOCTL_OMAP_DCE_CODEC_DELETE		DRM_IOW (DRM_OMAP_DCE_CODEC_DELETE, struct drm_omap_dce_codec_delete)

static struct drm_ioctl_desc dce_ioctls[] = {
		DRM_IOCTL_DEF_DRV(OMAP_DCE_ENGINE_OPEN, ioctl_engine_open, DRM_UNLOCKED|DRM_AUTH),
		DRM_IOCTL_DEF_DRV(OMAP_DCE_ENGINE_CLOSE, ioctl_engine_close, DRM_UNLOCKED|DRM_AUTH),
		DRM_IOCTL_DEF_DRV(OMAP_DCE_CODEC_CREATE, ioctl_codec_create, DRM_UNLOCKED|DRM_AUTH),
		DRM_IOCTL_DEF_DRV(OMAP_DCE_CODEC_CONTROL, ioctl_codec_control, DRM_UNLOCKED|DRM_AUTH),
		DRM_IOCTL_DEF_DRV(OMAP_DCE_CODEC_PROCESS, ioctl_codec_process, DRM_UNLOCKED|DRM_AUTH),
		DRM_IOCTL_DEF_DRV(OMAP_DCE_CODEC_DELETE, ioctl_codec_delete, DRM_UNLOCKED|DRM_AUTH),
};

/*
 * Plugin API:
 */

static int dce_load(struct drm_device *dev, unsigned long flags)
{
	dce_mapper_id = omap_drm_register_mapper();
	return 0;
}

static int dce_unload(struct drm_device *dev)
{
	omap_drm_unregister_mapper(dce_mapper_id);
	dce_mapper_id = -1;
	// XXX should block until pending txns are done..
	return 0;
}

static int dce_open(struct drm_device *dev, struct drm_file *file)
{
	struct dce_file_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	priv->dev = dev;
	priv->file = file;
	omap_drm_file_set_priv(file, dce_mapper_id, priv);
	return 0;
}

static int dce_release(struct drm_device *dev, struct drm_file *file)
{
	struct dce_file_priv *priv = omap_drm_file_priv(file, dce_mapper_id);
	int i;

	// XXX not sure if this is legit..  maybe we end up with this scenario
	// when drm device file is opened prior to dce module being loaded??
	WARN_ON(!priv);
	if (!priv)
		return 0;

	/* cleanup any remaining codecs and engines on behalf of the process,
	 * in case the process crashed or didn't clean up properly for itself:
	 */

	for (i = 0; i < ARRAY_SIZE(priv->codecs); i++) {
		uint32_t codec = priv->codecs[i].codec;
		if (codec) {
			enum omap_dce_codec codec_id = priv->codecs[i].codec_id;
			codec_unregister(priv, i+1);
			codec_delete(priv, codec, codec_id);
		}
	}

	for (i = 0; i < ARRAY_SIZE(priv->engines); i++) {
		uint32_t engine = priv->engines[i].engine;
		if (engine) {
			engine_unregister(priv, i+1);
			engine_close(priv, engine);
		}
	}

	for (i = 0; i < ARRAY_SIZE(txns); i++) {
		if (txns[i].priv == priv) {
			txn_cleanup(&txns[i]);
			memset(&txns[i], 0, sizeof(txns[i]));
		}
	}

	kfree(priv);

	return 0;
}

static struct omap_drm_plugin plugin = {
		.name = "dce",

		.load = dce_load,
		.unload = dce_unload,
		.open = dce_open,
		.release = dce_release,

		.ioctls = dce_ioctls,
		.num_ioctls = ARRAY_SIZE(dce_ioctls),
		.ioctl_base = 0,  /* initialized when plugin is registered */
};

/*
 * RPMSG API:
 */

static int rpmsg_probe(struct rpmsg_channel *_rpdev)
{
	struct dce_rpc_connect_req req = {
			.hdr = MKHDR(CONNECT),
			.chipset_id = GET_OMAP_TYPE,
			.debug = drm_debug ? 1 : 3,
	};
	int ret;

	DBG("");
	rpdev = _rpdev;

	/* send connect msg: */
	ret = rpsend(NULL, NULL, hdr(&req), sizeof(req));
	if (ret) {
		DBG("rpsend failed: %d", ret);
		return ret;
	}

	return omap_drm_register_plugin(&plugin);
}

static void __devexit rpmsg_remove(struct rpmsg_channel *_rpdev)
{
	DBG("");
	omap_drm_unregister_plugin(&plugin);
	rpdev = NULL;
}

static void rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
		int len, void *priv, u32 src)
{
	void *data2;
	DBG("len=%d, src=%d", len, src);
	/* note: we have to copy the data, because the ptr is no more valid
	 * once this fxn returns, and it could take a while for the requesting
	 * thread to pick up the data.. maybe there is a more clever way to
	 * handle this..
	 */
	data2 = kzalloc(len, GFP_KERNEL);
	memcpy(data2, data, len);
	rpcomplete(data2, len);
}

static struct rpmsg_device_id rpmsg_id_table[] = {
		{ .name = "rpmsg-dce" },
		{ },
};

static struct rpmsg_driver rpmsg_driver = {
		.drv.name       = KBUILD_MODNAME,
		.drv.owner      = THIS_MODULE,
		.id_table       = rpmsg_id_table,
		.probe          = rpmsg_probe,
		.callback       = rpmsg_cb,
		.remove         = __devexit_p(rpmsg_remove),
};

static int __init omap_dce_init(void)
{
	DBG("");
	return register_rpmsg_driver(&rpmsg_driver);
}

static void __exit omap_dce_fini(void)
{
	DBG("");
	unregister_rpmsg_driver(&rpmsg_driver);
}

module_init(omap_dce_init);
module_exit(omap_dce_fini);

MODULE_AUTHOR("Rob Clark <rob.clark@linaro.org>");
MODULE_DESCRIPTION("OMAP DRM Video Decode/Encode");
MODULE_LICENSE("GPL v2");
