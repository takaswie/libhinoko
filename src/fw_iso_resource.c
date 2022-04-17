// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

/**
 * HinokoFwIsoResource:
 * An object to initiate requests and listen events of isochronous resource allocation/deallocation.
 *
 * A [class@FwIsoResource] is an object to initiate requests and listen events of isochronous
 * resource allocation/deallocation by file descriptor owned internally. This object is designed to
 * be used for any derived object.
 */
typedef struct {
	int fd;
} HinokoFwIsoResourcePrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoResource, hinoko_fw_iso_resource, G_TYPE_OBJECT)

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

#define generate_syscall_error(error, errno, format, arg)		\
	g_set_error(error, HINOKO_FW_ISO_RESOURCE_ERROR,		\
		    HINOKO_FW_ISO_RESOURCE_ERROR_FAILED,		\
		    format " %d(%s)", arg, errno, strerror(errno))

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

typedef struct {
	GSource src;
	HinokoFwIsoResource *self;
	gpointer tag;
	gsize len;
	guint8 *buf;
} FwIsoResourceSource;

enum fw_iso_resource_sig_type {
	FW_ISO_RESOURCE_SIG_ALLOCATED = 0,
	FW_ISO_RESOURCE_SIG_DEALLOCATED,
	FW_ISO_RESOURCE_SIG_COUNT,
};
static guint fw_iso_resource_sigs[FW_ISO_RESOURCE_SIG_COUNT] = { 0 };

static void fw_iso_resource_finalize(GObject *obj)
{
	HinokoFwIsoResource *self = HINOKO_FW_ISO_RESOURCE(obj);
	HinokoFwIsoResourcePrivate *priv =
			hinoko_fw_iso_resource_get_instance_private(self);

	if (priv->fd >= 0)
		close(priv->fd);

	G_OBJECT_CLASS(hinoko_fw_iso_resource_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_resource_class_init(HinokoFwIsoResourceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_resource_finalize;

	/**
	 * HinokoFwIsoResource::allocated:
	 * @self: A [class@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable): A [struct@GLib.Error]. Error can be generated with
	 *	   domain of Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Emitted when allocation of isochronous resource finishes.
	 */
	fw_iso_resource_sigs[FW_ISO_RESOURCE_SIG_ALLOCATED] =
		g_signal_new("allocated",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinokoFwIsoResourceClass, allocated),
			     NULL, NULL,
			     hinoko_sigs_marshal_VOID__UINT_UINT_OBJECT,
			     G_TYPE_NONE,
			     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_ERROR);

	/**
	 * HinokoFwIsoResource::deallocated:
	 * @self: A [class@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable): A [struct@GLib.Error]. Error can be generated with
	 *	   domain of Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Emitted when deallocation of isochronous resource finishes.
	 */
	fw_iso_resource_sigs[FW_ISO_RESOURCE_SIG_DEALLOCATED] =
		g_signal_new("deallocated",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinokoFwIsoResourceClass, deallocated),
			     NULL, NULL,
			     hinoko_sigs_marshal_VOID__UINT_UINT_OBJECT,
			     G_TYPE_NONE,
			     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_ERROR);
}

static void hinoko_fw_iso_resource_init(HinokoFwIsoResource *self)
{
	HinokoFwIsoResourcePrivate *priv =
			hinoko_fw_iso_resource_get_instance_private(self);
	priv->fd = -1;
}

/**
 * hinoko_fw_iso_resource_new:
 *
 * Allocate and return an instance of [class@FwIsoResource].
 *
 * Returns: A [class@FwIsoResource].
 */
HinokoFwIsoResource *hinoko_fw_iso_resource_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE, NULL);
}

/**
 * hinoko_fw_iso_resource_open:
 * @self: A [class@FwIsoResource].
 * @path: A path of any Linux FireWire character device.
 * @open_flag: The flag of open(2) system call. O_RDONLY is forced to fulfil
 *	       internally.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.FileError
 *	   and Hinoko.FwIsoResourceError.
 *
 * Open Linux FireWire character device to delegate any request for isochronous
 * resource management to Linux FireWire subsystem.
 */
void hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path,
				 gint open_flag, GError **error)
{
	HinokoFwIsoResourcePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_resource_get_instance_private(self);

	if (priv->fd >= 0) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_OPENED);
		return;
	}

	open_flag |= O_RDONLY;
	priv->fd = open(path, open_flag);
	if (priv->fd < 0) {
		GFileError code = g_file_error_from_errno(errno);
		if (code != G_FILE_ERROR_FAILED)
			generate_file_error(error, code, "open(%s)", path);
		else
			generate_syscall_error(error, errno, "open(%s)", path);
	}
}

/**
 * hinoko_fw_iso_resource_allocate_once_async:
 * @self: A [class@FwIsoResource].
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate allocation of isochronous resource without any wait. When the
 * allocation finishes, [signal@FwIsoResource::allocated] signal is emit to notify the result,
 * channel, and bandwidth.
 */
void hinoko_fw_iso_resource_allocate_once_async(HinokoFwIsoResource *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **error)
{
	HinokoFwIsoResourcePrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_resource_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return;
	}

	g_return_if_fail(channel_candidates != NULL);
	g_return_if_fail(channel_candidates_count > 0);
	g_return_if_fail(bandwidth > 0);

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	if (ioctl(priv->fd, FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE, &res) < 0)
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE");
}

/**
 * hinoko_fw_iso_resource_deallocate_once_async:
 * @self: A [class@FwIsoResource].
 * @channel: The channel number to be deallocated.
 * @bandwidth: The amount of bandwidth to be deallocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate deallocation of isochronous resource without any wait. When the
 * deallocation finishes, [signal@FwIsoResource::deallocated] signal is emit to notify the result,
 * channel, and bandwidth.
 */
void hinoko_fw_iso_resource_deallocate_once_async(HinokoFwIsoResource *self,
						  guint channel,
						  guint bandwidth,
						  GError **error)
{
	HinokoFwIsoResourcePrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_resource_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return;
	}

	g_return_if_fail(channel < 64);
	g_return_if_fail(bandwidth > 0);

	res.channels = 1ull << channel;
	res.bandwidth = bandwidth;

	if (ioctl(priv->fd, FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE, &res) < 0)
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE");
}

struct waiter {
	GMutex mutex;
	GCond cond;
	GError *error;
	gboolean handled;
};

static void handle_event_signal(HinokoFwIsoResource *self, guint channel,
				guint bandwidth, const GError *error,
				gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	g_mutex_lock(&w->mutex);
	if (error != NULL)
		w->error = g_error_copy(error);
	w->handled = TRUE;
	g_cond_signal(&w->cond);
	g_mutex_unlock(&w->mutex);
}

/**
 * hinoko_fw_iso_resource_allocate_once_sync:
 * @self: A [class@FwIsoResource].
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate allocation of isochronous resource and wait for [signal@FwIsoResource::allocated]
 * signal.
 */
void hinoko_fw_iso_resource_allocate_once_sync(HinokoFwIsoResource *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth,
					       GError **error)
{
	struct waiter w;
	guint64 expiration;
	gulong handler_id;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	g_mutex_init(&w.mutex);
	g_cond_init(&w.cond);
	w.error = NULL;
	w.handled = FALSE;

	// For safe, use 100 msec for timeout.
	expiration = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;

	handler_id = g_signal_connect(self, "allocated",
				      (GCallback)handle_event_signal, &w);

	hinoko_fw_iso_resource_allocate_once_async(self, channel_candidates,
						   channel_candidates_count,
						   bandwidth, error);
	if (*error != NULL) {
		g_signal_handler_disconnect(self, handler_id);
		return;
	}

	g_mutex_lock(&w.mutex);
	while (w.handled == FALSE) {
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_mutex_unlock(&w.mutex);

	if (w.handled == FALSE)
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT);
	else if (w.error != NULL)
		*error = w.error;	// Delegate ownership.
}

/**
 * hinoko_fw_iso_resource_deallocate_once_sync:
 * @self: A [class@FwIsoResource].
 * @channel: The channel number to be deallocated.
 * @bandwidth: The amount of bandwidth to be deallocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * [signal@FwIsoResource::deallocated] signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_deallocate_once_sync(HinokoFwIsoResource *self,
						 guint channel,
						 guint bandwidth,
						 GError **error)
{
	struct waiter w;
	guint64 expiration;
	gulong handler_id;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	g_mutex_init(&w.mutex);
	g_cond_init(&w.cond);
	w.error = NULL;
	w.handled = FALSE;

	// For safe, use 100 msec for timeout.
	expiration = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;

	handler_id = g_signal_connect(self, "deallocated",
				      (GCallback)handle_event_signal, &w);

	hinoko_fw_iso_resource_deallocate_once_async(self, channel, bandwidth,
						     error);
	if (*error != NULL) {
		g_signal_handler_disconnect(self, handler_id);
		return;
	}

	g_mutex_lock(&w.mutex);
	while (w.handled == FALSE) {
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_mutex_unlock(&w.mutex);

	if (w.handled == FALSE)
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT);
	else if (w.error != NULL)
		*error = w.error;	// Delegate ownership.
}

// For internal use.
void hinoko_fw_iso_resource_ioctl(HinokoFwIsoResource *self,
				  unsigned long request, void *argp,
				  GError **error)
{
	HinokoFwIsoResourcePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(request == FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE ||
			 request == FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE);

	priv = hinoko_fw_iso_resource_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return;
	}

	if (ioctl(priv->fd, request, argp) < 0) {
		const char *arg;

		switch (request) {
		case FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE:
			arg = "FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE";
			break;
		case FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE:
			arg = "FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE";
			break;
		default:
			arg = "Unknown";
			break;
		}
		generate_syscall_error(error, errno, "ioctl(%s)", arg);
	}
}

static void handle_iso_resource_event(HinokoFwIsoResource *self,
				      struct fw_cdev_event_iso_resource *ev)
{
	guint channel;
	guint bandwidth;
	enum fw_iso_resource_sig_type signal_type;
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
		signal_type = FW_ISO_RESOURCE_SIG_ALLOCATED;
	else
		signal_type = FW_ISO_RESOURCE_SIG_DEALLOCATED;

	// To change state machine for auto mode.
	if (HINOKO_IS_FW_ISO_RESOURCE_AUTO(self)) {
		const char *signal_name;

		if (signal_type == FW_ISO_RESOURCE_SIG_ALLOCATED)
			signal_name = "allocated";
		else
			signal_name = "deallocated";

		hinoko_fw_iso_resource_auto_handle_event(HINOKO_FW_ISO_RESOURCE_AUTO(self),
							 channel, bandwidth, signal_name, error);
	}

	g_signal_emit(self, fw_iso_resource_sigs[signal_type], 0, channel, bandwidth, error);

	if (error != NULL)
		g_clear_error(&error);
}

static gboolean check_src(GSource *gsrc)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(gsrc, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)gsrc;
	HinokoFwIsoResource *self = src->self;
	HinokoFwIsoResourcePrivate *priv =
			hinoko_fw_iso_resource_get_instance_private(self);
	GIOCondition condition;
	ssize_t len;
	guint8 *buf;

	if (priv->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR)
		return G_SOURCE_REMOVE;

	len = read(priv->fd, src->buf, src->len);
	if (len <= 0) {
		if (errno == EAGAIN)
			return G_SOURCE_CONTINUE;

		return G_SOURCE_REMOVE;
	}

	buf = src->buf;
	while (len > 0) {
		union fw_cdev_event *ev = (union fw_cdev_event *)buf;
		size_t size = 0;

		switch (ev->common.type) {
		case FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED:
		case FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED:
			handle_iso_resource_event(self, &ev->iso_resource);
			size = sizeof(ev->iso_resource);
			break;
		default:
			break;
		}

		len -= size;
		buf += size;
	}


	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *gsrc)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)gsrc;

	g_free(src->buf);
	g_object_unref(src->self);
}

/**
 * hinoko_fw_iso_resource_create_source:
 * @self: A [class@FwIsoResource].
 * @gsrc: (out): A [struct@GLib.Source]
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * resource.
 */
void hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self,
					  GSource **gsrc, GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	long page_size = sysconf(_SC_PAGESIZE);
	HinokoFwIsoResourcePrivate *priv;
	FwIsoResourceSource *src;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_resource_get_instance_private(self);

	*gsrc = g_source_new(&funcs, sizeof(FwIsoResourceSource));

	g_source_set_name(*gsrc, "HinokoFwIsoResource");
	g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*gsrc, TRUE);

	src = (FwIsoResourceSource *)(*gsrc);

	src->buf = g_malloc0(page_size);

	src->len = (gsize)page_size;
	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);
	src->self = g_object_ref(self);
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
