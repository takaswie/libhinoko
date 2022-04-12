// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

/**
 * SECTION:fw_iso_ctx
 * @Title: HinokoFwIsoCtx
 * @Short_description: An abstract object to maintain isochronous context.
 * @include: fw_iso_ctx.h
 *
 * A #HinokoFwIsoCtx is an abstract object to maintain isochronous context by
 * UAPI of Linux FireWire subsystem. All of operations utilize ioctl(2) with
 * subsystem specific request commands. This object is designed for internal
 * use, therefore a few method and properties are available for applications.
 */
typedef struct {
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

	guint curr_offset;
	gboolean running;
} HinokoFwIsoCtxPrivate;
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(HinokoFwIsoCtx, hinoko_fw_iso_ctx, G_TYPE_OBJECT)

/**
 * hinoko_fw_iso_ctx_error_quark:
 *
 * Return the GQuark for error domain of GError which has code in #HinokoFwIsoCtxError.
 *
 * Returns: A #GQuark.
 */
G_DEFINE_QUARK(hinoko-fw-iso-ctx-error-quark, hinoko_fw_iso_ctx_error)

static const char *const err_msgs[] = {
	[HINOKO_FW_ISO_CTX_ERROR_ALLOCATED] =
		"The instance is already associated to any firewire character device",
	[HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED] =
		"The instance is not associated to any firewire character device",
	[HINOKO_FW_ISO_CTX_ERROR_MAPPED] =
		"The intermediate buffer is already mapped to the process",
	[HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED] =
		"The intermediate buffer is not mapped to the process",
	[HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED] = "No chunk registered before starting",
};

#define generate_local_error(exception, code) \
	g_set_error_literal(exception, HINOKO_FW_ISO_CTX_ERROR, code, err_msgs[code])

#define generate_syscall_error(exception, errno, format, arg)		\
	g_set_error(exception, HINOKO_FW_ISO_CTX_ERROR,			\
		    HINOKO_FW_ISO_CTX_ERROR_FAILED,			\
		    format " %d(%s)", arg, errno, strerror(errno))

#define generate_file_error(exception, code, format, arg) \
	g_set_error(exception, G_FILE_ERROR, code, format, arg)

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
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 */
gboolean hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				    HinokoFwIsoCtxMode mode, HinokoFwScode scode, guint channel,
				    guint header_size, GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_get_info info = {0};
	struct fw_cdev_create_iso_context create = {0};

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(self), FALSE);
	g_return_val_if_fail(path != NULL && strlen(path) > 0, FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);

	// Linux firewire stack supports three types of isochronous context
	// described in 1394 OHCI specification.
	g_return_val_if_fail(mode == HINOKO_FW_ISO_CTX_MODE_TX ||
			 mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE ||
			 mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE,
			 FALSE);

	g_return_val_if_fail(scode == HINOKO_FW_SCODE_S100 ||
			 scode == HINOKO_FW_SCODE_S200 ||
			 scode == HINOKO_FW_SCODE_S400 ||
			 scode == HINOKO_FW_SCODE_S800 ||
			 scode == HINOKO_FW_SCODE_S1600 ||
			 scode == HINOKO_FW_SCODE_S3200,
			 FALSE);

	// IEEE 1394 specification supports isochronous channel up to 64.
	g_return_val_if_fail(channel < 64, FALSE);

	// Headers should be aligned to quadlet.
	g_return_val_if_fail(header_size % 4 == 0, FALSE);

	if (mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE) {
		// At least, 1 quadlet is required for iso_header.
		g_return_val_if_fail(header_size >= 4, FALSE);
		g_return_val_if_fail(channel < 64, FALSE);
	} else if (mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		// Needless.
		g_return_val_if_fail(header_size == 0, FALSE);
		g_return_val_if_fail(channel == 0, FALSE);
	}

	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd >= 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_ALLOCATED);
		return FALSE;
	}

	priv->fd = open(path, O_RDWR);
	if  (priv->fd < 0) {
		GFileError code = g_file_error_from_errno(errno);
		if (code != G_FILE_ERROR_FAILED)
			generate_file_error(exception, code, "open(%s)", path);
		else
			generate_syscall_error(exception, errno, "open(%s)", path);
		return FALSE;
	}

	// Support FW_CDEV_VERSION_AUTO_FLUSH_ISO_OVERFLOW.
	info.version = 5;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		generate_syscall_error(exception, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_INFO");
		close(priv->fd);
		priv->fd = -1;
		return FALSE;
	}

	create.type = mode;
	create.channel = channel;
	create.speed = scode;
	create.closure = (__u64)self;
	create.header_size = header_size;

	if (ioctl(priv->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create) < 0) {
		generate_syscall_error(exception, errno, "ioctl(%s)", "FW_CDEV_IOC_CREATE_ISO_CONTEXT");
		close(priv->fd);
		priv->fd = -1;
		return FALSE;
	}

	priv->handle = create.handle;
	priv->mode = mode;
	priv->header_size = header_size;

	return TRUE;
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
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 */
gboolean hinoko_fw_iso_ctx_map_buffer(HinokoFwIsoCtx *self, guint bytes_per_chunk,
				      guint chunks_per_buffer, GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	unsigned int datum_size;
	int prot;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(self), FALSE);
	g_return_val_if_fail(bytes_per_chunk > 0, FALSE);
	g_return_val_if_fail(chunks_per_buffer > 0, FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (priv->addr != NULL) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_MAPPED);
		return FALSE;
	}

	datum_size = sizeof(struct fw_cdev_iso_packet);
	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX)
		datum_size += priv->header_size;

	priv->data = g_malloc_n(chunks_per_buffer, datum_size);

	priv->alloc_data_length = chunks_per_buffer * datum_size;

	prot = PROT_READ;
	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX)
		prot |= PROT_WRITE;

	// Align to size of page.
	priv->addr = mmap(NULL, bytes_per_chunk * chunks_per_buffer, prot,
			  MAP_SHARED, priv->fd, 0);
	if (priv->addr == MAP_FAILED) {
		generate_syscall_error(exception, errno,
				       "mmap(%d)", bytes_per_chunk * chunks_per_buffer);
		return FALSE;
	}

	priv->bytes_per_chunk = bytes_per_chunk;
	priv->chunks_per_buffer = chunks_per_buffer;

	return TRUE;
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
 * @clock_id: The numerical ID of clock source for the reference timestamp. One
 *            CLOCK_REALTIME(0), CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(2)
 *            is available in UAPI of Linux kernel.
 * @cycle_timer: (inout): A #HinokoCycleTimer to store data of cycle timer.
 * @exception: A #GError.
 *
 * Retrieve the value of cycle timer register. This method call is available
 * once any isochronous context is created.
 */
void hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self, gint clock_id,
				       HinokoCycleTimer *const *cycle_timer,
				       GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(cycle_timer != NULL);
	g_return_if_fail(exception != NULL && *exception == NULL);
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return;
	}

	(*cycle_timer)->clk_id = clock_id;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_CYCLE_TIMER2, *cycle_timer) < 0)
		generate_syscall_error(exception, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_CYCLE_TIMER2");
}

/**
 * hinoko_fw_iso_ctx_set_rx_channels:
 * @self: A #HinokoFwIsoCtx.
 * @channel_flags: Flags for channels to listen to.
 * @exception: A #GError.
 *
 * Indicate channels to listen to for IR context in buffer-fill mode.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 */
gboolean hinoko_fw_iso_ctx_set_rx_channels(HinokoFwIsoCtx *self, guint64 *channel_flags,
					   GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_set_iso_channels set = {0};

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(self), FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);

	priv = hinoko_fw_iso_ctx_get_instance_private(self);
	g_return_val_if_fail(priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE, FALSE);

	if (priv->fd < 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	set.channels = *channel_flags;
	set.handle = priv->handle;
	if (ioctl(priv->fd, FW_CDEV_IOC_SET_ISO_CHANNELS, &set) < 0) {
		generate_syscall_error(exception, errno, "ioctl(%s)", "FW_CDEV_IOC_SET_ISO_CHANNELS");
		return FALSE;
	}

	*channel_flags = set.channels;

	return TRUE;
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
 * @schedule_interrupt: schedule hardware interrupt at isochronous cycle for the chunk.
 * @exception: A #GError.
 *
 * Register data on buffer for payload of isochronous context.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 */
gboolean hinoko_fw_iso_ctx_register_chunk(HinokoFwIsoCtx *self, gboolean skip,
					  HinokoFwIsoCtxMatchFlag tags, guint sy,
					  const guint8 *header, guint header_length,
					  guint payload_length, gboolean schedule_interrupt,
					  GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;

	struct fw_cdev_iso_packet *datum;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(self), FALSE);
	g_return_val_if_fail(skip == TRUE || skip == FALSE, FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);

	g_return_val_if_fail(tags == 0 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3,
			     FALSE);

	g_return_val_if_fail(sy < 16, FALSE);

	priv = hinoko_fw_iso_ctx_get_instance_private(self);
	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		if (!skip) {
			g_return_val_if_fail(header_length == priv->header_size, FALSE);
			g_return_val_if_fail(payload_length <= priv->bytes_per_chunk, FALSE);
		} else {
			g_return_val_if_fail(payload_length == 0, FALSE);
			g_return_val_if_fail(header_length == 0, FALSE);
			g_return_val_if_fail(header == NULL, FALSE);
		}
	} else if (priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE ||
		   priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		g_return_val_if_fail(tags == 0, FALSE);
		g_return_val_if_fail(sy == 0, FALSE);
		g_return_val_if_fail(header == NULL, FALSE);
		g_return_val_if_fail(header_length == 0, FALSE);
		g_return_val_if_fail(payload_length == 0, FALSE);
	}

	g_return_val_if_fail(priv->data_length + sizeof(*datum) + header_length <= priv->alloc_data_length,
			     FALSE);

	if (priv->fd < 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (priv->addr == NULL) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED);
		return FALSE;
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

	if (schedule_interrupt)
		datum->control |= FW_CDEV_ISO_INTERRUPT;

	return TRUE;
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
			generate_syscall_error(exception, errno,
					       "ioctl(%s)", "FW_CDEV_IOC_QUEUE_ISO");
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

	g_warn_if_fail(chunk_count == priv->registered_chunk_count);

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

static gboolean check_src(GSource *gsrc)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an exception, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(gsrc, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static void handle_irq_event(struct fw_cdev_event_iso_interrupt *ev,
			     GError **exception)
{
	if (HINOKO_IS_FW_ISO_RX_SINGLE((gpointer)ev->closure)) {
		HinokoFwIsoRxSingle *ctx = HINOKO_FW_ISO_RX_SINGLE((gpointer)ev->closure);

		hinoko_fw_iso_rx_single_handle_event(ctx, ev, exception);
	} else if (HINOKO_IS_FW_ISO_TX((gpointer)ev->closure)) {
		HinokoFwIsoTx *ctx = HINOKO_FW_ISO_TX((gpointer)ev->closure);

		hinoko_fw_iso_tx_handle_event(ctx, ev, exception);
	} else {
		return;
	}

	if (*exception != NULL)
		return;

	fw_iso_ctx_queue_chunks(HINOKO_FW_ISO_CTX((gpointer)ev->closure), exception);
}

static void handle_irq_mc_event(struct fw_cdev_event_iso_interrupt_mc *ev,
				GError **exception)
{
	if (HINOKO_IS_FW_ISO_RX_MULTIPLE((gpointer)ev->closure)) {
		HinokoFwIsoRxMultiple *ctx = HINOKO_FW_ISO_RX_MULTIPLE((gpointer)ev->closure);

		hinoko_fw_iso_rx_multiple_handle_event(ctx, ev, exception);
	} else {
		return;
	}

	if (*exception != NULL)
		return;

	fw_iso_ctx_queue_chunks(HINOKO_FW_ISO_CTX((gpointer)ev->closure), exception);
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)gsrc;
	HinokoFwIsoCtx *self = src->self;
	HinokoFwIsoCtxPrivate *priv =
				hinoko_fw_iso_ctx_get_instance_private(self);
	GIOCondition condition;
	GError *exception;
	int len;
	guint8 *buf;

	if (priv->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR)
		return G_SOURCE_REMOVE;

	len = read(priv->fd, src->buf, src->len);
	if (len < 0) {
		if (errno != EAGAIN) {
			generate_file_error(&exception, g_file_error_from_errno(errno),
					    "read %s", strerror(errno));
			goto error;
		}

		return G_SOURCE_CONTINUE;
	}

	buf = src->buf;
	while (len > 0) {
		union fw_cdev_event *ev = (union fw_cdev_event *)buf;
		size_t size = 0;

		switch (ev->common.type) {
		case FW_CDEV_EVENT_ISO_INTERRUPT:
			exception = NULL;
			handle_irq_event(&ev->iso_interrupt, &exception);
			if (exception != NULL)
				goto error;
			size = sizeof(ev->iso_interrupt) +
			       ev->iso_interrupt.header_length;
			break;
		case FW_CDEV_EVENT_ISO_INTERRUPT_MULTICHANNEL:
			exception = NULL;
			handle_irq_mc_event(&ev->iso_interrupt_mc, &exception);
			if (exception != NULL)
				goto error;
			size = sizeof(ev->iso_interrupt_mc);
			break;
		default:
			break;
		}

		len -= size;
		buf += size;
	}

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
error:
	fw_iso_ctx_stop(self, exception);
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
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinokoFwIsoCtxPrivate *priv;
	FwIsoCtxSource *src;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(gsrc != NULL);
	g_return_if_fail(exception != NULL && *exception == NULL);

	priv = hinoko_fw_iso_ctx_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return;
	}

	*gsrc = g_source_new(&funcs, sizeof(FwIsoCtxSource));

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
 * @exception: A #GError.
 *
 * Start isochronous context.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 */
gboolean hinoko_fw_iso_ctx_start(HinokoFwIsoCtx *self, const guint16 *cycle_match, guint32 sync,
				 HinokoFwIsoCtxMatchFlag tags, GError **exception)
{
	struct fw_cdev_start_iso arg = {0};
	HinokoFwIsoCtxPrivate *priv;
	gint cycle;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(self), FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (priv->addr == NULL) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED);
		return FALSE;
	}

	if (cycle_match == NULL) {
		cycle = -1;
	} else {
		g_return_val_if_fail(cycle_match[0] < 4, FALSE);
		g_return_val_if_fail(cycle_match[1] < 8000, FALSE);

		cycle = (cycle_match[0] << 13) | cycle_match[1];
	}

	if (priv->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		g_return_val_if_fail(sync == 0, FALSE);
		g_return_val_if_fail(tags == 0, FALSE);
	} else {
		g_return_val_if_fail(sync < 16, FALSE);
		g_return_val_if_fail(tags <= (HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 |
					  HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 |
					  HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 |
					  HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3), FALSE);
	}

	// Not prepared.
	if (priv->data_length == 0) {
		generate_local_error(exception, HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED);
		return FALSE;
	}

	fw_iso_ctx_queue_chunks(self, exception);
	if (*exception != NULL)
		return FALSE;

	arg.sync = sync;
	arg.cycle = cycle;
	arg.tags = tags;
	arg.handle = priv->handle;
	if (ioctl(priv->fd, FW_CDEV_IOC_START_ISO, &arg) < 0) {
		generate_syscall_error(exception, errno, "ioctl(%s)", "FW_CDEV_IOC_START_ISO");
		return FALSE;
	}

	priv->running = TRUE;

	return TRUE;
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
 * @frames: (array length=frame_size)(out)(transfer none)(nullable): The array
 * 	    to fill the same data frame as @frame_size.
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

/**
 * hinoko_fw_iso_ctx_flush_completions:
 * @self: A #HinokoFwIsoCtx.
 * @exception: A #GError.
 *
 * Flush isochronous context until recent isochronous cycle. The call of function forces the
 * context to queue any type of interrupt event for the recent isochronous cycle. Application can
 * process the content of isochronous packet without waiting for actual hardware interrupt.
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_ctx_flush_completions(HinokoFwIsoCtx *self, GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (ioctl(priv->fd, FW_CDEV_IOC_FLUSH_ISO) < 0)
		generate_syscall_error(exception, errno, "ioctl(%s)", "FW_CDEV_IOC_FLUSH_ISO");
}
