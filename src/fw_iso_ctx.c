// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/firewire-cdev.h>

/**
 * SECTION:fw_iso_ctx
 * @Title: HinokoFwIsoCtx
 * @Short_description: An abstract object to maintain isochronous context.
 *
 * A #HinokoFwIsoCtx is an abstract object to maintain isochronous context by
 * UAPI of Linux FireWire subsystem. All of operations utilize ioctl(2) with
 * subsystem specific request commands. This object is designed for internal
 * use, therefore a few method and properties are available for applications.
 */
struct _HinokoFwIsoCtxPrivate {
	int fd;
	guint handle;

	HinokoFwIsoCtxMode mode;
	guint header_size;
	guchar *addr;
	guint bytes_per_chunk;
	guint chunks_per_buffer;

	// The number of entries equals to the value of chunks_per_buffer.
	guint8 *data;
	guint data_length;
	guint alloc_data_length;

	guint registered_chunk_count;
};
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(HinokoFwIsoCtx, hinoko_fw_iso_ctx,
				    G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoCtx", hinoko_fw_iso_ctx)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_ctx_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

typedef struct {
	GSource src;
	int fd;
	gpointer tag;
	unsigned int len;
	void *buf;
} FwIsoCtxSource;

static void fw_iso_ctx_finalize(GObject *obj)
{
	HinokoFwIsoCtx *self = HINOKO_FW_ISO_CTX(obj);

	hinoko_fw_iso_ctx_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_ctx_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_ctx_class_init(HinokoFwIsoCtxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_ctx_finalize;
}

static void hinoko_fw_iso_ctx_init(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv =
				hinoko_fw_iso_ctx_get_instance_private(self);

	priv->fd = -1;
}

/**
 * hinoko_fw_iso_ctx_allocate:
 * @self: A #HinokoFwIsoCtx.
 * @path: A path to any Linux FireWire character device.
 * @mode: The mode of context, one of #HinokoFwIsoCtxMode enumerations.
 * @scode: The speed of context, one of #HinokoFwScode enumerations.
 * @channel: The numerical channel of context up to 64.
 * @header_size: The number of bytes for header of isochronous context.
 * @exception: A #GError.
 *
 * Allocate a isochronous context to 1394 OHCI controller. A local node of the
 * node corresponding to the given path is used as the controller, thus any
 * path is accepted as long as process has enough permission for the path.
 */
void hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				HinokoFwIsoCtxMode mode, HinokoFwScode scode,
				guint channel, guint header_size,
				GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_create_iso_context create = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (path == NULL || strlen(path) == 0) {
		raise (exception, EINVAL);
		return;
	}

	// Linux firewire stack supports three types of isochronous context
	// described in 1394 OHCI specification.
	if (mode != HINOKO_FW_ISO_CTX_MODE_TX &&
	    mode != HINOKO_FW_ISO_CTX_MODE_RX_SINGLE &&
	    mode != HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		raise(exception, EINVAL);
		return;
	}

	if (scode != HINOKO_FW_SCODE_S100 &&
	    scode != HINOKO_FW_SCODE_S200 &&
	    scode != HINOKO_FW_SCODE_S400 &&
	    scode != HINOKO_FW_SCODE_S800 &&
	    scode != HINOKO_FW_SCODE_S1600 &&
	    scode != HINOKO_FW_SCODE_S3200 &&
	    scode != HINOKO_FW_SCODE_BETA) {
		raise(exception, EINVAL);
		return;
	}

	// IEEE 1394 specification supports isochronous channel up to 64.
	if (channel > 64) {
		raise(exception, EINVAL);
		return;
	}

	// Headers should be aligned to quadlet.
	if (header_size % 4 > 0) {
		raise(exception, EINVAL);
		return;
	}

	if (mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE) {
		// At least, 1 quadlet is required for iso_header.
		if (header_size < 4) {
			raise(exception, EINVAL);
			return;
		}
		if (channel > 64) {
			raise(exception, EINVAL);
			return;
		}
	} else if (mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		// Needless.
		if (header_size > 0) {
			raise(exception, EINVAL);
			return;
		}

		// Needless.
		if (channel > 0) {
			raise(exception, EINVAL);
			return;
		}
	}

	if (priv->fd >= 0) {
		raise(exception, EBUSY);
		return;
	}

	priv->fd = open(path, O_RDWR);
	if  (priv->fd < 0) {
		raise(exception, errno);
		return;
	}

	create.type = mode;
	create.channel = channel;
	create.speed = scode;
	create.closure = (__u64)self;
	create.header_size = header_size;

	if (ioctl(priv->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create) < 0) {
		raise(exception, errno);
		close(priv->fd);
		priv->fd = -1;
		return;
	}

	priv->handle = create.handle;
	priv->mode = mode;
	priv->header_size = header_size;
}

/**
 * hinoko_fw_iso_ctx_release:
 * @self: A #HinokoFwIsoCtx.
 *
 * Release allocated isochronous context from 1394 OHCI controller.
 */
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	hinoko_fw_iso_ctx_unmap_buffer(self);

	if (priv->fd >= 0)
		close(priv->fd);

	priv->fd = -1;
}

/**
 * hinoko_fw_iso_ctx_map_buffer:
 * @self: A #HinokoFwIsoCtx.
 * @bytes_per_chunk: The number of bytes per chunk in buffer going to be
 *		     allocated.
 * @chunks_per_buffer: The number of chunks in buffer going to be allocated.
 * @exception: A #GError.
 *
 * Map intermediate buffer to share payload of isochronous context with 1394
 * OHCI controller.
 */
void hinoko_fw_iso_ctx_map_buffer(HinokoFwIsoCtx *self, guint bytes_per_chunk,
				  guint chunks_per_buffer, GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	unsigned int datum_size;
	int prot;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	if (priv->addr != NULL) {
		raise(exception, EBUSY);
		return;
	}

	if (bytes_per_chunk == 0 ||
	    chunks_per_buffer == 0) {
		raise(exception, EINVAL);
		return;
	}

	datum_size = sizeof(struct fw_cdev_iso_packet);
	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX)
		datum_size += priv->header_size;

	priv->data = calloc(chunks_per_buffer, datum_size);
	if (priv->data == NULL) {
		raise(exception, ENOMEM);
		return;
	}
	priv->alloc_data_length = chunks_per_buffer * datum_size;

	prot = PROT_READ;
	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX)
		prot |= PROT_WRITE;

	// Align to size of page.
	priv->addr = mmap(NULL, bytes_per_chunk * chunks_per_buffer, prot,
			  MAP_SHARED, priv->fd, 0);
	if (priv->addr == MAP_FAILED) {
		raise(exception, errno);
		return;
	}

	priv->bytes_per_chunk = bytes_per_chunk;
	priv->chunks_per_buffer = chunks_per_buffer;
}

/**
 * hinoko_fw_iso_ctx_unmap_buffer:
 * @self: A #HinokoFwIsoCtx.
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload
 * of isochronous context.
 */
void hinoko_fw_iso_ctx_unmap_buffer(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->addr != NULL) {
		munmap(priv->addr,
		       priv->bytes_per_chunk * priv->chunks_per_buffer);
	}

	if (priv->data != NULL)
		free(priv->data);

	priv->addr = NULL;
	priv->data = NULL;
}

/**
 * hinoko_fw_iso_ctx_get_cycle_timer:
 * @self: A #HinokoFwIsoCtx.
 * @cycle_timer: (array fixed-size=3) (element-type guint16) (out caller-allocates):
 *		 The value of cycle timer register of
 *		 1394 OHCI, including three elements; second, cycle and offset.
 * @tv: (nullable): Fill with the nearest system time with CLOCK_MONOTONIC_RAW.
 * @exception: A #GError.
 *
 * Retrieve the value of cycle timer register. When @tv argument is given,
 * it's filled for the nearest system time with CLOCK_MONOTONIC_RAW flag. This
 * method call is available once any isochronous context is created.
 */
void hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self,
				       guint16 *cycle_timer, GTimeVal *tv,
				       GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_get_cycle_timer2 time;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENODATA);
		return;
	}

	time.clk_id = CLOCK_MONOTONIC_RAW;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_CYCLE_TIMER2, (void *)&time) < 0) {
		raise(exception, errno);
		return;
	}

	cycle_timer[0] = (time.cycle_timer & 0xfe000000) >> 25;
	cycle_timer[1] = (time.cycle_timer & 0x01fff000) >> 12;
	cycle_timer[2] = time.cycle_timer & 0x00000fff;

	if (tv) {
		tv->tv_sec = time.tv_sec;
		tv->tv_usec = time.tv_nsec;
	}
}

/**
 * hinoko_fw_iso_ctx_register_chunk:
 * @self: A #HinokoFwIsoCtx.
 * @skip: Whether to skip packet transmission or not.
 * @tags: The value of tag field for isochronous packet to handle.
 * @sy: The value of sy field for isochronous packet to handle.
 * @header: (array length=header_length) (element-type guint8): The content of
 * 	    header for IT context, nothing for IR context.
 * @header_length: The number of bytes for @header.
 * @payload_length: The number of bytes for payload of isochronous context.
 * @interrupt: Whether to generate interrupt event for this chunk.
 * @exception: A #GError.
 *
 * Register data on buffer for payload of isochronous context.
 */
void hinoko_fw_iso_ctx_register_chunk(HinokoFwIsoCtx *self, gboolean skip,
				      HinokoFwIsoCtxMatchFlag tags, guint sy,
				      const guint8 *header, guint header_length,
				      guint payload_length, gboolean interrupt,
				      GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_iso_packet *datum;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (skip != TRUE && skip != FALSE) {
		raise(exception, EINVAL);
		return;
	}

	if (tags > 0 &&
	    tags != HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 &&
	    tags != HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 &&
	    tags != HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 &&
	    tags != HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3) {
		raise(exception, EINVAL);
		return;
	}

	if (sy > 15) {
		raise(exception, EINVAL);
		return;
	}

	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		if (!skip) {
			if (header_length != priv->header_size) {
				raise(exception, EINVAL);
				return;
			}

			if (payload_length > priv->bytes_per_chunk) {
				raise(exception, EINVAL);
				return;
			}
		} else {
			if (payload_length > 0 || header_length > 0 ||
			    header != NULL) {
				raise(exception, EINVAL);
				return;
			}
		}
	} else if (priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE ||
		   priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		if (tags > 0) {
			raise(exception, EINVAL);
			return;
		}

		if (sy > 0) {
			raise(exception, EINVAL);
			return;
		}

		if (header != NULL || header_length > 0) {
			raise(exception, EINVAL);
			return;
		}

		if (payload_length > 0) {
			raise(exception, EINVAL);
			return;
		}
	}

	if (priv->data_length + sizeof(*datum) + header_length >
						priv->alloc_data_length) {
		raise(exception, ENOSPC);
		return;
	}

	datum = (struct fw_cdev_iso_packet *)(priv->data + priv->data_length);
	priv->data_length += sizeof(*datum) + header_length;

	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		if (!skip)
			memcpy(datum->header, header, header_length);
	} else {
		payload_length = priv->bytes_per_chunk;

		if (priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE)
			header_length = priv->header_size;
	}

	datum->control =
		FW_CDEV_ISO_PAYLOAD_LENGTH(payload_length) |
		FW_CDEV_ISO_TAG(tags) |
		FW_CDEV_ISO_SY(sy) |
		FW_CDEV_ISO_HEADER_LENGTH(header_length);

	if (skip)
		datum->control |= FW_CDEV_ISO_SKIP;

	if (interrupt)
		datum->control |= FW_CDEV_ISO_INTERRUPT;
}

static gboolean prepare_src(GSource *src, gint *timeout)
{
	// Blocking this process to save CPU time.
	*timeout = 200;

	// This source is not ready, let's poll(2).
	return FALSE;
}

static gboolean check_src(GSource *gsrc)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;
	GIOCondition condition;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR)
		return FALSE;

	// Don't go to dispatch if nothing available.
	return !!(condition & G_IO_IN);
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;
	struct fw_cdev_event_common *common;
	int len;

	len = read(src->fd, src->buf, src->len);
	if (len <= 0)
		goto end;

	common = (struct fw_cdev_event_common *)src->buf;
	if (!HINOKO_IS_FW_ISO_CTX(common->closure))
		goto end;

	// TODO: dispatch event.
end:
	// Just be sure to continue to process this source.
	return TRUE;
}

static void finalize_src(GSource *gsrc)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;

	g_free(src->buf);
}

/**
 * hinoko_fw_iso_ctx_create_source:
 * @self: A #hinokoFwIsoCtx.
 * @src: (out): A #GSource.
 * @exception: A #GError.
 *
 * Create Gsource for GMainContext to dispatch events for isochronous context.
 */
void hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **src,
				     GError **exception)
{
	static GSourceFuncs funcs = {
		.prepare	= prepare_src,
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinokoFwIsoCtxPrivate *priv;
	void *buf;
	unsigned int len;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	/*
	 * MEMO: allocate one page because we cannot assume the size of
	 * transaction frame.
	 */
	len = sysconf(_SC_PAGESIZE);
	buf = g_malloc0(len);
	if (buf == NULL) {
		raise(exception, ENOMEM);
		return;
	}

	*src = g_source_new(&funcs, sizeof(FwIsoCtxSource));
	if (*src == NULL) {
		raise(exception, ENOMEM);
		g_free(buf);
		return;
	}

	g_source_set_name(*src, "HinokoFwIsoCtx");
	g_source_set_priority(*src, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*src, TRUE);

	((FwIsoCtxSource *)*src)->len = len;
	((FwIsoCtxSource *)*src)->buf = buf;
	((FwIsoCtxSource *)*src)->tag =
				g_source_add_unix_fd(*src, priv->fd, G_IO_IN);
	((FwIsoCtxSource *)*src)->fd = priv->fd;
}
