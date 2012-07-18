/* qmi.c - QMI protocol implementation
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "qmi.h"
#include "buffer.h"
#include "structs.h"

#include <linux/string.h>

struct qmux {
	u8 tf;	/* always 1 */
	u16 len;
	u8 ctrl;
	u8 service;
	u8 qmicid;
} __attribute__((__packed__));

struct getcid_req {
	struct qmux header;
	u8 req;
	u8 tid;
	u16 msgid;
	u16 tlvsize;
	u8 service;
	u16 size;
	u8 qmisvc;
} __attribute__((__packed__));

struct releasecid_req {
	struct qmux header;
	u8 req;
	u8 tid;
	u16 msgid;
	u16 tlvsize;
	u8 rlscid;
	u16 size;
	u16 cid;
} __attribute__((__packed__));

struct ready_req {
	struct qmux header;
	u8 req;
	u8 tid;
	u16 msgid;
	u16 tlvsize;
} __attribute__((__packed__));

struct seteventreport_req {
	struct qmux header;
	u8 req;
	u16 tid;
	u16 msgid;
	u16 tlvsize;
	u8 reportchanrate;
	u16 size;
	u8 period;
	u32 mask;
} __attribute__((__packed__));

struct getpkgsrvcstatus_req {
	struct qmux header;
	u8 req;
	u16 tid;
	u16 msgid;
	u16 tlvsize;
} __attribute__((__packed__));

struct getmeidimei_req {
	struct qmux header;
	u8 req;
	u16 tid;
	u16 msgid;
	u16 tlvsize;
} __attribute__((__packed__));

const size_t qmux_size = sizeof(struct qmux);

struct buffer *qmictl_new_getcid(u8 tid, u8 svctype)
{
	struct getcid_req *req;
	struct buffer *buf = buffer_new(sizeof *req);
	if (!buf)
		return NULL;
	req = buffer_data(buf);
	req->req = 0x00;
	req->tid = tid;
	req->msgid = 0x0022;
	req->tlvsize = 0x0004;
	req->service = 0x01;
	req->size = 0x0001;
	req->qmisvc = svctype;
	return buf;
}

struct buffer *qmictl_new_releasecid(u8 tid, u16 cid)
{
	struct releasecid_req *req;
	struct buffer *buf = buffer_new(sizeof *req);
	if (!buf)
		return NULL;
	req = buffer_data(buf);
	req->req = 0x00;
	req->tid = tid;
	req->msgid = 0x0023;
	req->tlvsize = 0x05;
	req->rlscid = 0x01;
	req->size = 0x0002;
	req->cid = cid;
	return buf;
}

struct buffer *qmictl_new_ready(u8 tid)
{
	struct ready_req *req;
	struct buffer *buf = buffer_new(sizeof *req);
	if (!buf)
		return NULL;
	req = buffer_data(buf);
	req->req = 0x00;
	req->tid = tid;
	req->msgid = 0x21;
	req->tlvsize = 0;
	return buf;
}

struct buffer *qmiwds_new_seteventreport(u8 tid)
{
	struct seteventreport_req *req;
	struct buffer *buf = buffer_new(sizeof *req);
	if (!buf)
		return NULL;
	req = buffer_data(buf);
	req->req = 0x00;
	req->tid = tid;
	req->msgid = 0x0001;
	req->tlvsize = 0x0008;
	req->reportchanrate = 0x11;
	req->size = 0x0005;
	req->period = 0x01;
	req->mask = 0x000000ff;
	return buf;
}

struct buffer *qmiwds_new_getpkgsrvcstatus(u8 tid)
{
	struct getpkgsrvcstatus_req *req;
	struct buffer *buf = buffer_new(sizeof *req);
	if (!buf)
		return NULL;
	req = buffer_data(buf);
	req->req = 0x00;
	req->tid = tid;
	req->msgid = 0x22;
	req->tlvsize = 0x0000;
	return buf;
}

struct buffer *qmidms_new_getmeidimei(u8 tid)
{
	struct getmeidimei_req *req;
	struct buffer *buf = buffer_new(sizeof *req);
	if (!buf)
		return NULL;
	req = buffer_data(buf);
	req->req = 0x00;
	req->tid = tid;
	req->msgid = 0x25;
	req->tlvsize = 0x0000;
	return buf;
}

int qmux_parse(u16 *cid, void *buf, size_t size)
{
	struct qmux *qmux = buf;

	BUG_ON(!buf);

	if (size < 12)
		return -ENOMEM;

	if (qmux->tf != 1 || qmux->len != size - 1 || qmux->ctrl != 0x80)
		return -EINVAL;

	*cid = (qmux->qmicid << 8) + qmux->service;
	return sizeof(*qmux);
}

int qmux_fill(u16 cid, struct buffer *buf)
{
	struct qmux *qmux = buffer_data(buf);

	if (buffer_size(buf) < sizeof(*qmux))
		return -ENOMEM;

	qmux->tf = 1;
	qmux->len = buffer_size(buf) - 1;
	qmux->ctrl = 0;
	qmux->service = cid & 0xff;
	qmux->qmicid = cid >> 8;
	return 0;
}

static int tlv_get(void *msg, u16 msgsize, u8 type, void *buf, u16 bufsize)
{
	u16 pos;
	u16 msize = 0;

	BUG_ON(!msg || !buf);

	for (pos = 4; pos + 3 < msgsize; pos += msize + 3) {
		msize = *(u16 *)(msg + pos + 1);

		if (*(u8 *)(msg + pos) != type)
			continue;

		if (bufsize < msize) {
			GOBI_ERROR("found type 0x%02x, "
			    "but value too big (%d > %d)",
			    type, msize, bufsize);
			return -ENOMEM;
		}

		memcpy(buf, msg + pos + 3, msize);

		return msize;
	}

	GOBI_WARN("didn't find type 0x%02x", type);
	return -ENOMSG;
}

int qmi_msgisvalid(void *msg, u16 size)
{
	u16 tlv[2];
	int result;
	u16 tsize;

	BUG_ON(!msg);

	result = tlv_get(msg, size, 2, tlv, 4);
	if (result < 0) {
		GOBI_ERROR("tlv_get failed: %d", result);
		return result;
	}
	tsize = result;

	if (tsize != 4) {
		GOBI_ERROR("size is wrong (%d != 4)", tsize);
		return -ENOMSG;
	}

	if (tlv[0] != 0) {
		GOBI_WARN("tlv[0]=%d, tlv[1]=%d", tlv[0], tlv[1]);
		return tlv[1];
	} else {
		return 0;
	}
}

int qmi_msgid(void *msg, u16 size)
{
	BUG_ON(!msg);

	if (size < 2) {
		GOBI_ERROR("message too short (%d < 2)", size);
		return -ENODATA;
	}

	return *(u16 *)msg;
}

int qmictl_alloccid_resp(void *buf, u16 size, u16 *cid)
{
	int result;
	u8 offset = sizeof(struct qmux) + 2;

	BUG_ON(!buf);

	if (size < offset) {
		GOBI_ERROR("message too short (%d < %d)", size, offset);
		return -ENOMEM;
	}

	buf = buf + offset;
	size -= offset;

	result = qmi_msgid(buf, size);
	if (result != 0x22) {
		GOBI_ERROR("wrong message ID (0x%02x != 0x22)", result);
		return -EFAULT;
	}

	result = qmi_msgisvalid(buf, size);
	if (result != 0) {
		GOBI_WARN("invalid message");
		return -EFAULT;
	}

	result = tlv_get(buf, size, 0x01, cid, 2);
	if (result < 0) {
		GOBI_ERROR("tlv_get failed: %d", result);
		return result;
	}
	if (result != 2) {
		GOBI_ERROR("size is wrong (%d != 2)", result);
		return -EFAULT;
	}

	return 0;
}

int qmictl_freecid_resp(void *buf, u16 size)
{
	int result;
	u8 offset = sizeof(struct qmux) + 2;

	BUG_ON(!buf);

	if (size < offset) {
		GOBI_ERROR("message too short (%d < %d)", size, offset);
		return -ENOMEM;
	}

	buf = buf + offset;
	size -= offset;

	result = qmi_msgid(buf, size);
	if (result != 0x23) {
		GOBI_ERROR("wrong message ID (0x%02x != 0x23)", result);
		return -EFAULT;
	}

	result = qmi_msgisvalid(buf, size);
	if (result != 0) {
		GOBI_ERROR("invalid message");
		return -EFAULT;
	}

	return 0;
}

int qmiwds_event_resp(void *buf, u16 size, struct qmiwds_stats *stats)
{
	int result;
	u8 status[2];

	u8 offset = sizeof(struct qmux) + 3;

	BUG_ON(!buf || !stats);

	if (size < offset) {
		GOBI_ERROR("message too short (%d < %d)", size, offset);
		return -ENOMEM;
	}

	buf = buf + offset;
	size -= offset;

	result = qmi_msgid(buf, size);
	if (result == 0x01) {
		/* TODO(ttuttle): Error checking? */
		/* TODO(ttuttle): Endianness? */
		tlv_get(buf, size, 0x10, &stats->txok, 4);
		tlv_get(buf, size, 0x11, &stats->rxok, 4);
		tlv_get(buf, size, 0x12, &stats->txerr, 4);
		tlv_get(buf, size, 0x13, &stats->rxerr, 4);
		tlv_get(buf, size, 0x14, &stats->txofl, 4);
		tlv_get(buf, size, 0x15, &stats->rxofl, 4);
		tlv_get(buf, size, 0x19, &stats->txbytesok, 8);
		tlv_get(buf, size, 0x1A, &stats->rxbytesok, 8);
	} else if (result == 0x22) {
		result = tlv_get(buf, size, 0x01, &status[0], 2);
		if (result < 0) {
			GOBI_ERROR("tlv_get failed: %d", result);
			return result;
		}

		if (result >= 1)
			stats->linkstate = status[0] == 0x02;
		if (result == 2)
			stats->reconfigure = status[1] == 0x01;
	} else {
		GOBI_ERROR("wrong message ID (0x%02x != 0x01, 0x22)", result);
		return -EFAULT;
	}

	return 0;
}

int qmidms_meidimei_resp(void *buf, u16 size, char *meid, int meidsize,
			 char *imei, int imeisize)
{
	int result;
	bool meid_valid = false;
	bool imei_valid = false;

	u8 offset = sizeof(struct qmux) + 3;

	BUG_ON(!buf);

	if (size < offset) {
		GOBI_ERROR("message too short (%d < %d)", size, offset);
		return -ENOMEM;
	}

	if (meidsize < MEID_SIZE) {
		GOBI_ERROR("buffer too small (%d < %d)", meidsize, MEID_SIZE);
		return -ENOMEM;
	}

	if (imeisize < IMEI_SIZE) {
		GOBI_ERROR("buffer too small (%d < %d)", imeisize, IMEI_SIZE);
		return -ENOMEM;
	}

	buf = buf + offset;
	size -= offset;

	result = qmi_msgid(buf, size);
	if (result != 0x25) {
		GOBI_ERROR("wrong message ID (0x%02x != 0x25)", result);
		return -EFAULT;
	}

	result = qmi_msgisvalid(buf, size);
	if (result) {
		GOBI_ERROR("invalid message");
		return -EFAULT;
	}

	result = tlv_get(buf, size, 0x12, meid, MEID_SIZE);
	if (result < 0) {
		GOBI_ERROR("MEID tlv_get failed: %d", result);
	} else if (result != MEID_SIZE) {
		GOBI_ERROR("MEID size is wrong (%d != %d)", result, MEID_SIZE);
		memset(meid, '0', meidsize);
	} else {
		meid_valid = true;
	}

	result = tlv_get(buf, size, 0x11, imei, IMEI_SIZE);
	if (result < 0) {
		GOBI_ERROR("IMEI tlv_get failed: %d", result);
	} else if (result != IMEI_SIZE) {
		GOBI_ERROR("IMEI size is wrong (%d != %d)", result, IMEI_SIZE);
		memset(imei, '0', imeisize);
	} else {
		imei_valid = true;
	}

	return (meid_valid || imei_valid) ? 0 : -EFAULT;
}
