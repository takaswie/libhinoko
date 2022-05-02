// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * HinokoFwIsoResource:
 * An interface object to listen events of isochronous resource allocation and deallocation.
 *
 * The [iface@FwIsoResource] should be implemented in GObject-derived object to listen events of
 * isochronous resource allocation and deallocation.
 *
 * Since: 0.7.
 */

G_DEFINE_INTERFACE(HinokoFwIsoResource, hinoko_fw_iso_resource, G_TYPE_OBJECT)

/**
 * hinoko_fw_iso_resource_error_quark:
 *
 * Return the [alias@GLib.Quark] for error domain of [struct@GLib.Error] which has code in
 * Hinoko.FwIsoResourceError.
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinoko-fw-iso-resource-error-quark, hinoko_fw_iso_resource_error)

static const char *const err_msgs[] = {
	[HINOKO_FW_ISO_RESOURCE_ERROR_OPENED] =
		"The instance is already associated to any firewire character device",
	[HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED] =
		"The instance is not associated to any firewire character device",
	[HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT] =
		"No event to the request arrives within timeout.",
};

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINOKO_FW_ISO_RESOURCE_ERROR, code, err_msgs[code])

#define generate_event_error(error, errno)				\
	g_set_error(error, HINOKO_FW_ISO_RESOURCE_ERROR,		\
		    HINOKO_FW_ISO_RESOURCE_ERROR_EVENT,			\
		    "%d %s", errno, strerror(errno))

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

typedef struct {
	GSource src;
	HinokoFwIsoResource *self;
	gpointer tag;
	gsize len;
	guint8 *buf;
	int fd;
	void (*handle_event)(HinokoFwIsoResource *self, const char *signal_name, guint channel,
			     guint bandwidth, const GError *error);
} FwIsoResourceSource;

static void hinoko_fw_iso_resource_default_init(HinokoFwIsoResourceInterface *iface)
{
	/**
	 * HinokoFwIsoResource::allocated:
	 * @self: A [iface@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error]. Error can be generated
	 *	   with domain of Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Emitted when allocation of isochronous resource finishes.
	 *
	 * Since: 0.7.
	 */
	g_signal_new(ALLOCATED_SIGNAL_NAME,
		     G_TYPE_FROM_INTERFACE(iface),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HinokoFwIsoResourceInterface, allocated),
		     NULL, NULL,
		     hinoko_sigs_marshal_VOID__UINT_UINT_BOXED,
		     G_TYPE_NONE,
		     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_ERROR);

	/**
	 * HinokoFwIsoResource::deallocated:
	 * @self: A [iface@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error]. Error can be generated
	 *	   with domain of Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Emitted when deallocation of isochronous resource finishes.
	 *
	 * Since: 0.7.
	 */
	g_signal_new(DEALLOCATED_SIGNAL_NAME,
		     G_TYPE_FROM_INTERFACE(iface),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(HinokoFwIsoResourceInterface, deallocated),
		     NULL, NULL,
		     hinoko_sigs_marshal_VOID__UINT_UINT_BOXED,
		     G_TYPE_NONE,
		     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_ERROR);
}

gboolean fw_iso_resource_open(int *fd, const gchar *path, gint open_flag, GError **error)
{
	g_return_val_if_fail(fd != NULL, FALSE);
	g_return_val_if_fail(path != NULL && strlen(path) > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (*fd >= 0) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_OPENED);
		return FALSE;
	}

	open_flag |= O_RDONLY;
	*fd = open(path, open_flag);
	if (*fd < 0) {
		GFileError code = g_file_error_from_errno(errno);
		if (code != G_FILE_ERROR_FAILED)
			generate_file_error(error, code, "open(%s)", path);
		else
			generate_syscall_error(error, errno, "open(%s)", path);
		return FALSE;
	}

	return TRUE;
}
/**
 * hinoko_fw_iso_resource_open:
 * @self: A [iface@FwIsoResource].
 * @path: A path of any Linux FireWire character device.
 * @open_flag: The flag of open(2) system call. O_RDONLY is forced to fulfil
 *	       internally.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.FileError
 *	   and Hinoko.FwIsoResourceError.
 *
 * Open Linux FireWire character device to delegate any request for isochronous
 * resource management to Linux FireWire subsystem.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path, gint open_flag,
				 GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	(void)HINOKO_FW_ISO_RESOURCE_GET_IFACE(self)->open(self, path, open_flag, error);
}

static void handle_event_signal(HinokoFwIsoResource *self, guint channel, guint bandwidth,
				const GError *error, gpointer user_data)
{
	struct fw_iso_resource_waiter *w = (struct fw_iso_resource_waiter *)user_data;

	g_mutex_lock(&w->mutex);
	if (error != NULL)
		w->error = g_error_copy(error);
	w->handled = TRUE;
	g_cond_signal(&w->cond);
	g_mutex_unlock(&w->mutex);
}

// For internal use.
void fw_iso_resource_waiter_init(HinokoFwIsoResource *self, struct fw_iso_resource_waiter *w,
				 const char *signal_name, guint timeout_ms)
{
	g_mutex_init(&w->mutex);
	g_cond_init(&w->cond);
	w->error = NULL;
	w->handled = FALSE;
	w->expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;
	w->handler_id = g_signal_connect(self, signal_name, G_CALLBACK(handle_event_signal), w);
}

// For internal use.
void fw_iso_resource_waiter_wait(HinokoFwIsoResource *self, struct fw_iso_resource_waiter *w,
				 GError **error)
{
	if (*error != NULL) {
		g_signal_handler_disconnect(self, w->handler_id);
		return;
	}

	g_mutex_lock(&w->mutex);
	while (w->handled == FALSE) {
		if (!g_cond_wait_until(&w->cond, &w->mutex, w->expiration))
			break;
	}
	g_signal_handler_disconnect(self, w->handler_id);
	g_mutex_unlock(&w->mutex);

	if (w->handled == FALSE)
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT);
	else if (w->error != NULL)
		*error = w->error;	// Delegate ownership.
}

static void handle_iso_resource_event(FwIsoResourceSource *src,
				      const struct fw_cdev_event_iso_resource *ev)
{
	guint channel;
	guint bandwidth;
	const char *signal_name;
	GError *error = NULL;

	if (ev->channel >= 0) {
		channel = ev->channel;
		bandwidth = ev->bandwidth;
	} else {
		channel = 0;
		bandwidth = 0;
		generate_event_error(&error, -ev->channel);
	}

	if (ev->type == FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED)
		signal_name = ALLOCATED_SIGNAL_NAME;
	else
		signal_name = DEALLOCATED_SIGNAL_NAME;

	src->handle_event(src->self, signal_name, channel, bandwidth, error);

	if (error != NULL)
		g_clear_error(&error);
}

static gboolean check_src(GSource *source)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)source;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(source, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *source, GSourceFunc cb, gpointer user_data)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)source;
	GIOCondition condition;
	ssize_t len;
	const union fw_cdev_event *ev;

	if (src->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(source, src->tag);
	if (condition & G_IO_ERR)
		return G_SOURCE_REMOVE;

	len = read(src->fd, src->buf, src->len);
	if (len <= 0) {
		if (errno == EAGAIN)
			return G_SOURCE_CONTINUE;

		return G_SOURCE_REMOVE;
	}

	ev = (const union fw_cdev_event *)src->buf;
	switch (ev->common.type) {
	case FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED:
	case FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED:
		handle_iso_resource_event(src, &ev->iso_resource);
		break;
	default:
		break;
	}

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *source)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)source;

	g_free(src->buf);
	g_object_unref(src->self);
}

gboolean fw_iso_resource_create_source(int fd, HinokoFwIsoResource *inst,
				       void (*handle_event)(HinokoFwIsoResource *self,
							    const char *signal_name, guint channel,
							    guint bandwidth, const GError *error),
				       GSource **source, GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	long page_size = sysconf(_SC_PAGESIZE);
	FwIsoResourceSource *src;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE(inst), FALSE);
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(error != NULL && *error == NULL, FALSE);

	*source = g_source_new(&funcs, sizeof(FwIsoResourceSource));

	g_source_set_name(*source, "HinokoFwIsoResource");

	src = (FwIsoResourceSource *)(*source);

	src->buf = g_malloc0(page_size);

	src->len = (gsize)page_size;
	src->tag = g_source_add_unix_fd(*source, fd, G_IO_IN);
	src->fd = fd;
	src->self = g_object_ref(inst);
	src->handle_event = handle_event;

	return TRUE;
}

/**
 * hinoko_fw_iso_resource_create_source:
 * @self: A [iface@FwIsoResource].
 * @source: (out): A [struct@GLib.Source]
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * resource.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self, GSource **source,
					  GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(source != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	(void)HINOKO_FW_ISO_RESOURCE_GET_IFACE(self)->create_source(self, source, error);
}

/**
 * hinoko_fw_iso_resource_calculate_bandwidth:
 * @bytes_per_payload: The number of bytes in payload of isochronous packet.
 * @scode: The speed of transmission.
 *
 * Calculate the amount of bandwidth expected to be consumed in allocation unit
 * by given parameters.
 *
 * Returns: The amount of bandwidth expected to be consumed.
 */
guint hinoko_fw_iso_resource_calculate_bandwidth(guint bytes_per_payload,
						 HinokoFwScode scode)
{
	guint bytes_per_packet;
	guint s400_bytes;

	// iso packets have three header quadlets and quadlet-aligned payload.
	bytes_per_packet = 3 * 4 + ((bytes_per_payload + 3) / 4) * 4;

	// convert to bandwidth units (quadlets at S1600 = bytes at S400).
	if (scode <= HINOKO_FW_SCODE_S400) {
		s400_bytes = bytes_per_packet *
			     (1 << (HINOKO_FW_SCODE_S400 - scode));
	} else {
		s400_bytes = bytes_per_packet /
			     (1 << (scode - HINOKO_FW_SCODE_S400));
	}

	return s400_bytes;
}
