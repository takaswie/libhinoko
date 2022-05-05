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

G_DEFINE_ABSTRACT_TYPE(HinokoFwIsoCtx, hinoko_fw_iso_ctx, G_TYPE_OBJECT)

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

enum fw_iso_ctx_sig_type {
	FW_ISO_CTX_SIG_TYPE_STOPPED = 1,
	FW_ISO_CTX_SIG_TYPE_COUNT,
};
static guint fw_iso_ctx_sigs[FW_ISO_CTX_SIG_TYPE_COUNT] = { 0 };

static void fw_iso_ctx_get_property(GObject *obj, guint id, GValue *val, GParamSpec *spec)
{
	return;
}

static void hinoko_fw_iso_ctx_class_init(HinokoFwIsoCtxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_ctx_get_property;

	g_object_class_install_property(gobject_class, FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK,
		g_param_spec_uint(BYTES_PER_CHUNK_PROP_NAME, "bytes-per-chunk",
				  "The number of bytes for chunk in buffer.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER,
		g_param_spec_uint(CHUNKS_PER_BUFFER_PROP_NAME, "chunks-per-buffer",
				  "The number of chunks in buffer.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, FW_ISO_CTX_PROP_TYPE_REGISTERED_CHUNK_COUNT,
		g_param_spec_uint(REGISTERED_CHUNK_COUNT_PROP_NAME,
				  "registered-chunk-count",
				  "The number of chunk to be registered.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));

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
	return;
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
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(cycle_timer != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	HINOKO_FW_ISO_CTX_CLASS(self)->get_cycle_timer(self, clock_id, cycle_timer, error);
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

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
error:
	hinoko_fw_iso_ctx_stop(src->self);
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
 * fw_iso_ctx_state_create_source:
 * @state: A [struct@FwIsoCtxState].
 * @source: (out): A [struct@GLib.Source].
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * context.
 */
gboolean fw_iso_ctx_state_create_source(struct fw_iso_ctx_state *state, HinokoFwIsoCtx *inst,
					gboolean (*handle_event)(HinokoFwIsoCtx *inst,
								 const union fw_cdev_event *event,
								 GError **error),
					GSource **source, GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	FwIsoCtxSource *src;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(inst), FALSE);
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	*source = g_source_new(&funcs, sizeof(FwIsoCtxSource));

	g_source_set_name(*source, "HinokoFwIsoCtx");

	src = (FwIsoCtxSource *)(*source);

	if (state->mode != HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
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

	src->tag = g_source_add_unix_fd(*source, state->fd, G_IO_IN);
	src->fd = state->fd;
	src->self = g_object_ref(inst);
	src->handle_event = handle_event;

	return TRUE;
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
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(source != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	(void)HINOKO_FW_ISO_CTX_CLASS(self)->create_source(self, source, error);
}

/**
 * hinoko_fw_iso_ctx_stop:
 * @self: A [class@FwIsoCtx].
 *
 * Stop isochronous context.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));

	HINOKO_FW_ISO_CTX_CLASS(self)->stop(self);
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
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(error == NULL || *error == NULL);

	(void)HINOKO_FW_ISO_CTX_CLASS(self)->flush_completions(self, error);
}
