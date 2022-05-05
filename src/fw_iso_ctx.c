// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

#include <sys/mman.h>

/**
 * HinokoFwIsoCtx
 * An abstract object to maintain isochronous context.
 *
 * A [class@FwIsoCtx] is an abstract object to maintain isochronous context by
 * UAPI of Linux FireWire subsystem. All of operations utilize ioctl(2) with
 * subsystem specific request commands. This object is designed for internal
 * use, therefore a few method and properties are available for applications.
 */
typedef struct fw_iso_ctx_state HinokoFwIsoCtxPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(HinokoFwIsoCtx, hinoko_fw_iso_ctx, G_TYPE_OBJECT)

/**
 * hinoko_fw_iso_ctx_error_quark:
 *
 * Return the [alias@GLib.Quark] for error domain of [struct@GLib.Error] which has code in
 * Hinoko.FwIsoCtxError.
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinoko-fw-iso-ctx-error-quark, hinoko_fw_iso_ctx_error)

const char *const fw_iso_ctx_err_msgs[7] = {
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

typedef struct {
	GSource src;
	gpointer tag;
	unsigned int len;
	void *buf;
	HinokoFwIsoCtx *self;
	int fd;
	gboolean (*handle_event)(HinokoFwIsoCtx *self, const union fw_cdev_event *event,
				 GError **error);
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
	 * @self: A [class@FwIsoCtx].
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error].
	 *
	 * Emitted when isochronous context is stopped.
	 */
	fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_STOPPED] =
		g_signal_new(STOPPED_SIGNAL_NEME,
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

gboolean fw_iso_ctx_handle_event_real(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				      GError **error)
{
	return TRUE;
}

/**
 * hinoko_fw_iso_ctx_allocate:
 * @self: A [class@FwIsoCtx].
 * @path: A path to any Linux FireWire character device.
 * @mode: The mode of context, one of [enum@FwIsoCtxMode] enumerations.
 * @scode: The speed of context, one of [enum@FwScode] enumerations.
 * @channel: The numeric channel of context up to 64.
 * @header_size: The number of bytes for header of isochronous context.
 * @error: A [struct@GLib.Error].
 *
 * Allocate a isochronous context to 1394 OHCI controller. A local node of the
 * node corresponding to the given path is used as the controller, thus any
 * path is accepted as long as process has enough permission for the path.
 */
void hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				HinokoFwIsoCtxMode mode, HinokoFwScode scode,
				guint channel, guint header_size,
				GError **error)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	(void)fw_iso_ctx_state_allocate(priv, path, mode, scode, channel, header_size, error);
}

/**
 * hinoko_fw_iso_ctx_release:
 * @self: A [class@FwIsoCtx].
 *
 * Release allocated isochronous context from 1394 OHCI controller.
 */
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	hinoko_fw_iso_ctx_unmap_buffer(self);

	fw_iso_ctx_state_release(priv);
}

/**
 * hinoko_fw_iso_ctx_map_buffer:
 * @self: A [class@FwIsoCtx].
 * @bytes_per_chunk: The number of bytes per chunk in buffer going to be allocated.
 * @chunks_per_buffer: The number of chunks in buffer going to be allocated.
 * @error: A [struct@GLib.Error].
 *
 * Map intermediate buffer to share payload of isochronous context with 1394 OHCI controller.
 */
void hinoko_fw_iso_ctx_map_buffer(HinokoFwIsoCtx *self, guint bytes_per_chunk,
				  guint chunks_per_buffer, GError **error)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	(void)fw_iso_ctx_state_map_buffer(priv, bytes_per_chunk, chunks_per_buffer, error);
}

/**
 * hinoko_fw_iso_ctx_unmap_buffer:
 * @self: A [class@FwIsoCtx].
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload of isochronous context.
 */
void hinoko_fw_iso_ctx_unmap_buffer(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	hinoko_fw_iso_ctx_stop(self);

	fw_iso_ctx_state_unmap_buffer(priv);
}

/**
 * hinoko_fw_iso_ctx_get_cycle_timer:
 * @self: A [class@FwIsoCtx].
 * @clock_id: The numeric ID of clock source for the reference timestamp. One CLOCK_REALTIME(0),
 *	      CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(2) is available in UAPI of Linux kernel.
 * @cycle_timer: (inout): A [struct@CycleTimer] to store data of cycle timer.
 * @error: A [struct@GLib.Error].
 *
 * Retrieve the value of cycle timer register. This method call is available
 * once any isochronous context is created.
 */
void hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self, gint clock_id,
				       HinokoCycleTimer *const *cycle_timer,
				       GError **error)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(cycle_timer != NULL);
	g_return_if_fail(error == NULL || *error == NULL);
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return;
	}

	(*cycle_timer)->clk_id = clock_id;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_CYCLE_TIMER2, *cycle_timer) < 0)
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_CYCLE_TIMER2");
}

/**
 * hinoko_fw_iso_ctx_set_rx_channels:
 * @self: A [class@FwIsoCtx].
 * @channel_flags: Flags for channels to listen to.
 * @error: A [struct@GLib.Error].
 *
 * Indicate channels to listen to for IR context in buffer-fill mode.
 */
void hinoko_fw_iso_ctx_set_rx_channels(HinokoFwIsoCtx *self,
				       guint64 *channel_flags,
				       GError **error)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_set_iso_channels set = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_ctx_get_instance_private(self);
	g_return_if_fail(priv->mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE);

	if (priv->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return;
	}

	set.channels = *channel_flags;
	set.handle = priv->handle;
	if (ioctl(priv->fd, FW_CDEV_IOC_SET_ISO_CHANNELS, &set) < 0) {
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_SET_ISO_CHANNELS");
		return;
	}

	*channel_flags = set.channels;
}

/**
 * hinoko_fw_iso_ctx_register_chunk:
 * @self: A [class@FwIsoCtx].
 * @skip: Whether to skip packet transmission or not.
 * @tags: The value of tag field for isochronous packet to handle.
 * @sy: The value of sy field for isochronous packet to handle.
 * @header: (array length=header_length) (element-type guint8): The content of header for IT
 *	    context, nothing for IR context.
 * @header_length: The number of bytes for @header.
 * @payload_length: The number of bytes for payload of isochronous context.
 * @schedule_interrupt: schedule hardware interrupt at isochronous cycle for the chunk.
 * @error: A [struct@GLib.Error].
 *
 * Register data on buffer for payload of isochronous context.
 */
void hinoko_fw_iso_ctx_register_chunk(HinokoFwIsoCtx *self, gboolean skip,
				      HinokoFwIsoCtxMatchFlag tags, guint sy,
				      const guint8 *header, guint header_length,
				      guint payload_length, gboolean schedule_interrupt,
				      GError **error)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	(void)fw_iso_ctx_state_register_chunk(priv, skip, tags, sy, header, header_length,
					      payload_length, schedule_interrupt, error);
}

static gboolean check_src(GSource *source)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)source;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(source, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *source, GSourceFunc cb, gpointer user_data)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)source;
	HinokoFwIsoCtxPrivate *priv;
	GIOCondition condition;
	GError *error = NULL;
	int len;
	const union fw_cdev_event *event;

	condition = g_source_query_unix_fd(source, src->tag);
	if (condition & G_IO_ERR)
		return G_SOURCE_REMOVE;

	len = read(src->fd, src->buf, src->len);
	if (len < 0) {
		if (errno != EAGAIN) {
			generate_file_error(&error, g_file_error_from_errno(errno),
					    "read %s", strerror(errno));
			goto error;
		}

		return G_SOURCE_CONTINUE;
	}

	event = (const union fw_cdev_event *)src->buf;
	if (!src->handle_event(src->self, event, &error))
		goto error;

	priv = hinoko_fw_iso_ctx_get_instance_private(src->self);
	if (!fw_iso_ctx_state_queue_chunks(priv, &error))
		goto error;

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
error:
	hinoko_fw_iso_ctx_stop(src->self);
	g_signal_emit(src->self, fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_STOPPED], 0, error);
	g_clear_error(&error);
	return G_SOURCE_REMOVE;
}

static void finalize_src(GSource *source)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)source;

	g_free(src->buf);
	g_object_unref(src->self);
}

/**
 * hinoko_fw_iso_ctx_create_source:
 * @self: A [class@FwIsoCtx].
 * @source: (out): A [struct@GLib.Source].
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * context.
 */
void hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **source, GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinokoFwIsoCtxPrivate *priv;
	FwIsoCtxSource *src;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(source != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_ctx_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return;
	}

	*source = g_source_new(&funcs, sizeof(FwIsoCtxSource));

	g_source_set_name(*source, "HinokoFwIsoCtx");
	g_source_set_priority(*source, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*source, TRUE);

	src = (FwIsoCtxSource *)(*source);

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

	src->tag = g_source_add_unix_fd(*source, priv->fd, G_IO_IN);
	src->fd = priv->fd;
	src->self = g_object_ref(self);

	if (HINOKO_IS_FW_ISO_RX_SINGLE(self))
		src->handle_event = fw_iso_rx_single_handle_event;
	else if (HINOKO_IS_FW_ISO_RX_MULTIPLE(self))
		src->handle_event = fw_iso_rx_multiple_handle_event;
	else if (HINOKO_IS_FW_ISO_TX(self))
		src->handle_event = fw_iso_tx_handle_event;
	else
		src->handle_event = fw_iso_ctx_handle_event_real;
}

/**
 * hinoko_fw_iso_ctx_start:
 * @self: A [class@FwIsoCtx].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing, up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @error: A [struct@GLib.Error].
 *
 * Start isochronous context.
 */
void hinoko_fw_iso_ctx_start(HinokoFwIsoCtx *self, const guint16 *cycle_match, guint32 sync,
			     HinokoFwIsoCtxMatchFlag tags, GError **error)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(error == NULL || *error == NULL);
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	(void)fw_iso_ctx_state_start(priv, cycle_match, sync, tags, error);
}

/**
 * hinoko_fw_iso_ctx_stop:
 * @self: A [class@FwIsoCtx].
 *
 * Stop isochronous context.
 */
void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv;
	gboolean running;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	running = priv->running;

	fw_iso_ctx_state_stop(priv);

	if (priv->running != running)
		g_signal_emit(self, fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_STOPPED], 0, NULL);
}

/**
 * hinoko_fw_iso_ctx_read_frames:
 * @self: A [class@FwIsoCtx].
 * @offset: offset from head of buffer.
 * @length: the number of bytes to read.
 * @frames: (array length=frame_size)(out)(transfer none)(nullable): The array to fill the same
 *	    data frame as @frame_size.
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
 * @self: A [class@FwIsoCtx].
 * @error: A [struct@GLib.Error].
 *
 * Flush isochronous context until recent isochronous cycle. The call of function forces the
 * context to queue any type of interrupt event for the recent isochronous cycle. Application can
 * process the content of isochronous packet without waiting for actual hardware interrupt.
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_ctx_flush_completions(HinokoFwIsoCtx *self, GError **error)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (ioctl(priv->fd, FW_CDEV_IOC_FLUSH_ISO) < 0)
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_FLUSH_ISO");
}
