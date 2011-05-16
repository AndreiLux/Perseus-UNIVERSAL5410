/* buffer.c - refcounted buffer implementation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2011  The Chromium OS Authors
 */

#include <linux/kref.h>
#include <linux/slab.h>

#include "buffer.h"

struct buffer {
	struct kref ref;
	size_t sz;
	char data[0];
};

struct buffer *buffer_new(size_t sz)
{
	struct buffer *buf = kmalloc(sizeof(*buf) + sz, GFP_KERNEL);
	if (!buf)
		return NULL;
	kref_init(&buf->ref);
	buf->sz = sz;
	return buf;
}

void buffer_get(struct buffer *buf)
{
	kref_get(&buf->ref);
}

static void free_buf(struct kref *ref)
{
	struct buffer *buf = container_of(ref, struct buffer, ref);
	kfree(buf);
}

void buffer_put(struct buffer *buf)
{
	kref_put(&buf->ref, free_buf);
}

size_t buffer_size(struct buffer *buf)
{
	return buf->sz;
}

void *buffer_data(struct buffer *buf)
{
	return &buf->data[0];
}
