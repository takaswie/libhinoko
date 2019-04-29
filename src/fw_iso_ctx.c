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
	guint accumulated_chunk_count;
	guint chunks_per_irq;

	guint curr_offset;
	gboolean running;
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
	gpointer tag;
	unsigned int len;
	void *buf;
	HinokoFwIsoCtx *self;
} FwIsoCtxSource;

enum fw_iso_ctx_prop_type {
	FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK = 1,
	FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER,
	FW_ISO_CTX_PROP_TYPE_REGISTERED_CHUNK_COUNT,
	FW_ISO_CTX_PROP_TYPE_COUNT,
};
static GParamSpec *fw_iso_ctx_props[FW_ISO_CTX_PROP_TYPE_COUNT] = { NULL, };

enum fw_iso_ctx_sig_type {
	FW_ISO_CTX_SIG_TYPE_STOPPED = 1,
	FW_ISO_CTX_SIG_TYPE_COUNT,
};
static guint fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_COUNT] = { 0 };

static void fw_iso_ctx_get_property(GObject *obj, guint id, GValue *val,
				    GParamSpec *spec)
{
	HinokoFwIsoCtx *self = HINOKO_FW_ISO_CTX(obj);
	HinokoFwIsoCtxPrivate *priv =
				hinoko_fw_iso_ctx_get_instance_private(self);

	switch (id) {
	case FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK:
		g_value_set_uint(val, priv->bytes_per_chunk);
		break;
	case FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER:
		g_value_set_uint(val, priv->chunks_per_buffer);
		break;
	case FW_ISO_CTX_PROP_TYPE_REGISTERED_CHUNK_COUNT:
		g_value_set_uint(val, priv->registered_chunk_count);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_iso_ctx_set_property(GObject *obj, guint id, const GValue *val,
				    GParamSpec *spec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
}

static void fw_iso_ctx_finalize(GObject *obj)
{
	HinokoFwIsoCtx *self = HINOKO_FW_ISO_CTX(obj);

	hinoko_fw_iso_ctx_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_ctx_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_ctx_class_init(HinokoFwIsoCtxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_ctx_get_property;
	gobject_class->set_property = fw_iso_ctx_set_property;
	gobject_class->finalize = fw_iso_ctx_finalize;

	fw_iso_ctx_props[FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK] =
		g_param_spec_uint("bytes-per-chunk", "bytes-per-chunk",
				  "The number of bytes for chunk in buffer.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);
	fw_iso_ctx_props[FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER] =
		g_param_spec_uint("chunks-per-buffer", "chunks-per-buffer",
				  "The number of chunks in buffer.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);
	fw_iso_ctx_props[FW_ISO_CTX_PROP_TYPE_REGISTERED_CHUNK_COUNT] =
		g_param_spec_uint("registered-chunk-count",
				  "registered-chunk-count",
				  "The number of chunk to be registered.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_ISO_CTX_PROP_TYPE_COUNT,
					  fw_iso_ctx_props);

	/**
	 * HinokoFwIsoCtx::stopped:
	 * @self: A #HinokoFwIsoCtx.
	 * @error: (nullable): A #GError.
	 *
	 * When isochronous context is stopped, #HinokoFwIsoCtx::stopped is
	 * emitted.
	 */
	fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_STOPPED] =
		g_signal_new("stopped",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(HinokoFwIsoCtxClass, stopped),
			NULL, NULL,
			g_cclosure_marshal_VOID__BOXED,
			G_TYPE_NONE, 1, G_TYPE_ERROR);
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
	struct fw_cdev_get_info info = {0};
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
	    scode != HINOKO_FW_SCODE_S3200) {
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

	// Support FW_CDEV_VERSION_AUTO_FLUSH_ISO_OVERFLOW.
	info.version = 5;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		raise(exception, errno);
		close(priv->fd);
		priv->fd = -1;
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

	hinoko_fw_iso_ctx_stop(self);

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
 * hinoko_fw_iso_ctx_set_rx_channels:
 * @self: A #HinokoFwIsoCtx.
 * @channel_flags: Flags for channels to listen to.
 * @exception: A #GError.
 *
 * Indicate channels to listen to for IR context in buffer-fill mode.
 */
void hinoko_fw_iso_ctx_set_rx_channels(HinokoFwIsoCtx *self,
				       guint64 *channel_flags,
				       GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_set_iso_channels set = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	if (priv->mode != HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		raise(exception, EINVAL);
		return;
	}

	set.channels = *channel_flags;
	set.handle = priv->handle;
	if (ioctl(priv->fd, FW_CDEV_IOC_SET_ISO_CHANNELS, &set) < 0) {
		raise(exception, errno);
		return;
	}

	*channel_flags = set.channels;
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
 * @exception: A #GError.
 *
 * Register data on buffer for payload of isochronous context.
 */
void hinoko_fw_iso_ctx_register_chunk(HinokoFwIsoCtx *self, gboolean skip,
				      HinokoFwIsoCtxMatchFlag tags, guint sy,
				      const guint8 *header, guint header_length,
				      guint payload_length, GError **exception)
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
	++priv->registered_chunk_count;

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
}

static void fw_iso_ctx_queue_chunks(HinokoFwIsoCtx *self, GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	guint data_offset = 0;
	int chunk_count = 0;
	unsigned int bytes_per_buffer;
	guint buf_offset;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	bytes_per_buffer = priv->bytes_per_chunk * priv->chunks_per_buffer;
	buf_offset = priv->curr_offset;

	while (data_offset < priv->data_length) {
		struct fw_cdev_queue_iso arg = {0};
		guint buf_length = 0;
		guint data_length = 0;

		while (buf_offset + buf_length < bytes_per_buffer &&
		       data_offset + data_length < priv->data_length) {
			struct fw_cdev_iso_packet *datum;
			guint payload_length;
			guint header_length;
			guint datum_length;

			datum = (struct fw_cdev_iso_packet *)
				(priv->data + data_offset + data_length);
			payload_length = datum->control & 0x0000ffff;
			header_length = (datum->control & 0xff000000) >> 24;

			if (buf_offset + buf_length + payload_length >
							bytes_per_buffer) {
				buf_offset = 0;
				break;
			}

			datum_length = sizeof(*datum);
			if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX)
				datum_length += header_length;

			if (++priv->accumulated_chunk_count %
				priv->chunks_per_irq == 0)
				datum->control |= FW_CDEV_ISO_INTERRUPT;
			if (priv->accumulated_chunk_count >= G_MAXINT) {
				priv->accumulated_chunk_count %=
							priv->chunks_per_irq;
			}

			g_debug("%3d: %3d-%3d/%3d: %6d-%6d/%6d: %d",
				chunk_count,
				data_offset + data_length,
				data_offset + data_length + datum_length,
				priv->alloc_data_length,
				buf_offset + buf_length,
				buf_offset + buf_length + payload_length,
				bytes_per_buffer,
				!!(datum->control & FW_CDEV_ISO_INTERRUPT));

			buf_length += payload_length;
			data_length += datum_length;
			++chunk_count;
		}

		arg.packets = (__u64)(priv->data + data_offset);
		arg.size = data_length;
		arg.data = (__u64)(priv->addr + buf_offset);
		arg.handle = priv->handle;
		if (ioctl(priv->fd, FW_CDEV_IOC_QUEUE_ISO, &arg) < 0) {
			raise(exception, errno);
			return;
		}

		g_debug("%3d: %3d-%3d/%3d: %6d-%6d/%6d",
			chunk_count,
			data_offset, data_offset + data_length,
			priv->alloc_data_length,
			buf_offset, buf_offset + buf_length,
			bytes_per_buffer);

		buf_offset += buf_length;
		buf_offset %= bytes_per_buffer;

		data_offset += data_length;
	}

	if (chunk_count != priv->registered_chunk_count) {
		raise(exception, EIO);
		return;
	}

	priv->curr_offset = buf_offset;

	priv->data_length = 0;
	priv->registered_chunk_count = 0;
}

static void fw_iso_ctx_stop(HinokoFwIsoCtx *self, GError *exception)
{
	struct fw_cdev_stop_iso arg = {0};
	HinokoFwIsoCtxPrivate *priv =
				hinoko_fw_iso_ctx_get_instance_private(self);

	if (!priv->running)
		return;

	arg.handle = priv->handle;
	ioctl(priv->fd, FW_CDEV_IOC_STOP_ISO, &arg);

	priv->running = FALSE;
	priv->registered_chunk_count = 0;
	priv->data_length = 0;
	priv->curr_offset = 0;

	g_signal_emit(self, fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_STOPPED], 0,
		      exception, NULL);
}

static gboolean prepare_src(GSource *src, gint *timeout)
{
	// Although saving CPU time, use timeout.
	*timeout = 200;

	// This source is not ready, let's poll(2).
	return FALSE;
}

static gboolean check_src(GSource *gsrc)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;
	GIOCondition condition;
	GError *exception;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR) {
		raise(&exception, EIO);
		g_source_remove_unix_fd(gsrc, src->tag);
		fw_iso_ctx_stop(src->self, exception);
		return FALSE;
	}

	// Don't go to dispatch if nothing available.
	return !!(condition & G_IO_IN);
}

static void handle_irq_event(struct fw_cdev_event_iso_interrupt *ev, int len,
			     GError **exception)
{
	// Linux FireWire subsytem can truncate event up to the size of given
	// buffer.
	if (len != sizeof(*ev) + ev->header_length) {
		raise(exception, EIO);
		return;
	}

	if (HINOKO_IS_FW_ISO_RX_SINGLE(ev->closure)) {
		HinokoFwIsoRxSingle *ctx = HINOKO_FW_ISO_RX_SINGLE(ev->closure);

		hinoko_fw_iso_rx_single_handle_event(ctx, ev, exception);
	} else if (HINOKO_IS_FW_ISO_TX(ev->closure)) {
		HinokoFwIsoTx *ctx = HINOKO_FW_ISO_TX(ev->closure);

		hinoko_fw_iso_tx_handle_event(ctx, ev, exception);
	} else {
		return;
	}

	if (*exception != NULL)
		return;

	fw_iso_ctx_queue_chunks(HINOKO_FW_ISO_CTX(ev->closure), exception);
}

static void handle_irq_mc_event(struct fw_cdev_event_iso_interrupt_mc *ev,
				GError **exception)
{
	if (HINOKO_IS_FW_ISO_RX_MULTIPLE(ev->closure)) {
		HinokoFwIsoRxMultiple *ctx =
					HINOKO_FW_ISO_RX_MULTIPLE(ev->closure);

		hinoko_fw_iso_rx_multiple_handle_event(ctx, ev, exception);
	} else {
		return;
	}

	if (*exception != NULL)
		return;

	fw_iso_ctx_queue_chunks(HINOKO_FW_ISO_CTX(ev->closure), exception);
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;
	HinokoFwIsoCtx *self = src->self;
	HinokoFwIsoCtxPrivate *priv =
				hinoko_fw_iso_ctx_get_instance_private(self);
	union fw_cdev_event *ev;
	GError *exception = NULL;
	int len;

	len = read(priv->fd, src->buf, src->len);
	if (len <= 0) {
		if (len < 0 && errno != EAGAIN) {
			raise(&exception, errno);
			goto remove;
		}

		return G_SOURCE_CONTINUE;
	}

	ev = (union fw_cdev_event *)src->buf;

	if (ev->common.type == FW_CDEV_EVENT_ISO_INTERRUPT)
		handle_irq_event(&ev->iso_interrupt, len, &exception);
	else if (ev->common.type == FW_CDEV_EVENT_ISO_INTERRUPT_MULTICHANNEL)
		handle_irq_mc_event(&ev->iso_interrupt_mc, &exception);

	if (exception != NULL)
		goto remove;

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
remove:
	g_source_remove_unix_fd(gsrc, src->tag);
	fw_iso_ctx_stop(src->self, exception);
	return G_SOURCE_REMOVE;
}

static void finalize_src(GSource *gsrc)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;

	g_free(src->buf);
	g_object_unref(src->self);
}

/**
 * hinoko_fw_iso_ctx_create_source:
 * @self: A #hinokoFwIsoCtx.
 * @gsrc: (out): A #GSource.
 * @exception: A #GError.
 *
 * Create Gsource for GMainContext to dispatch events for isochronous context.
 */
void hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **gsrc,
				     GError **exception)
{
	static GSourceFuncs funcs = {
		.prepare	= prepare_src,
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinokoFwIsoCtxPrivate *priv;
	FwIsoCtxSource *src;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	*gsrc = g_source_new(&funcs, sizeof(FwIsoCtxSource));
	if (*gsrc == NULL) {
		raise(exception, ENOMEM);
		return;
	}

	g_source_set_name(*gsrc, "HinokoFwIsoCtx");
	g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*gsrc, TRUE);

	src = (FwIsoCtxSource *)(*gsrc);

	if (priv->mode != HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		// MEMO: Linux FireWire subsystem queues isochronous event
		// independently of interrupt flag when the same number of
		// bytes as one page is stored in the buffer of header. To
		// avoid truncated read, keep enough size.
		src->len = sizeof(struct fw_cdev_event_iso_interrupt) +
			   sysconf(_SC_PAGESIZE);
	} else {
		src->len = sizeof(struct fw_cdev_event_iso_interrupt_mc);
	}
	src->buf = g_malloc0(src->len);
	if (src->buf == NULL) {
		raise(exception, ENOMEM);
		g_source_unref(*gsrc);
		return;
	}

	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);
	src->self = g_object_ref(self);
}

/**
 * hinoko_fw_iso_ctx_start:
 * @self: A #HinokoFwIsoCtx.
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable):
 * 		 The isochronous cycle to start packet processing. The first
 * 		 element should be the second part of isochronous cycle, up to
 * 		 3. The second element should be the cycle part of isochronous
 * 		 cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing,
 * 	  up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @chunks_per_irq: The number of chunks per interval of interrupt.
 * @exception: A #GError.
 *
 * Start isochronous context.
 */
void hinoko_fw_iso_ctx_start(HinokoFwIsoCtx *self, const guint16 *cycle_match,
			     guint32 sync, HinokoFwIsoCtxMatchFlag tags,
			     guint chunks_per_irq, GError **exception)
{
	struct fw_cdev_start_iso arg = {0};
	HinokoFwIsoCtxPrivate *priv;
	gint cycle;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0 || priv->addr == NULL) {
		raise(exception, ENXIO);
		return;
	}

	if (cycle_match == NULL) {
		cycle = -1;
	} else {
		if (cycle_match[0] > 3) {
			raise(exception, EINVAL);
			return;
		}
		if (cycle_match[1] > 7900) {
			raise(exception, EINVAL);
			return;
		}

		cycle = (cycle_match[0] << 13) | cycle_match[1];
	}

	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		if (sync > 0) {
			raise(exception, EINVAL);
			return;
		}

		if (tags > 0) {
			raise(exception, EINVAL);
			return;
		}
	} else {
		if (sync > 15) {
			raise(exception, EINVAL);
			return;
		}

		if (tags > (HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 |
			    HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 |
			    HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 |
			    HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3)) {
			raise(exception, EINVAL);
			return;
		}
	}

	// Not prepared.
	if (priv->data_length == 0) {
		raise(exception, ENODATA);
		return;
	}

	if (chunks_per_irq * 2 > priv->chunks_per_buffer) {
		raise(exception, EINVAL);
		return;
	}
	priv->chunks_per_irq = chunks_per_irq;
	priv->accumulated_chunk_count = 0;

	fw_iso_ctx_queue_chunks(self, exception);
	if (*exception != NULL)
		return;

	arg.sync = sync;
	arg.cycle = cycle;
	arg.tags = tags;
	arg.handle = priv->handle;
	if (ioctl(priv->fd, FW_CDEV_IOC_START_ISO, &arg) < 0) {
		raise(exception, errno);
		return;
	}

	priv->running = TRUE;
}

/**
 * hinoko_fw_iso_ctx_stop:
 * @self: A #HinokoFwIsoCtx.
 *
 * Stop isochronous context.
 */
void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));

	fw_iso_ctx_stop(self, NULL);
}

/**
 * hinoko_fw_iso_ctx_read_frames:
 * @self: A #HinokoFwIsoCtx.
 * @offset: offset from head of buffer.
 * @length: the number of bytes to read.
 * @frames: (array length=frame_size) (element-type guint8) (out) (nullable):
 * 	    A #GByteArray, filled with the same number of bytes as @frame_size.
 * @frame_size: this value is for a case to truncate due to the end of buffer.
 *
 * Read frames to given buffer.
 */
void hinoko_fw_iso_ctx_read_frames(HinokoFwIsoCtx *self, guint offset,
				   guint length, const guint8 **frames,
				   guint *frame_size)
{
	HinokoFwIsoCtxPrivate *priv;
	unsigned int bytes_per_buffer;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	bytes_per_buffer = priv->bytes_per_chunk * priv->chunks_per_buffer;
	if (offset > bytes_per_buffer) {
		*frames = NULL;
		*frame_size = 0;
		return;
	}

	*frames = priv->addr + offset;

	if (offset + length < bytes_per_buffer)
		*frame_size = length;
	else
		*frame_size = bytes_per_buffer - offset;
}
