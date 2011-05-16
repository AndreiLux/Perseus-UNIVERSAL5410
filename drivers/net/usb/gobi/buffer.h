/* buffer.h - refcounted buffer implementation
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

#ifndef GOBI_BUFFER_H
#define GOBI_BUFFER_H

#include <linux/kref.h>
#include <linux/types.h>

struct buffer;

struct buffer *buffer_new(size_t sz);
void buffer_get(struct buffer *buf);
void buffer_put(struct buffer *buf);
size_t buffer_size(struct buffer *buf);
void *buffer_data(struct buffer *buf);

#endif /* !GOBI_BUFFER_H */
