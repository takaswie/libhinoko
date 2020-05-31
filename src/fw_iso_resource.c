// SPDX-License-Identifier: LGPL-2.1-or-later
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "internal.h"

struct _HinokoFwIsoResourcePrivate {
	int fd;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoResource, hinoko_fw_iso_resource, G_TYPE_OBJECT)

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
	 * @self: A #HinokoFwIsoResource.
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @err_code: 0 if successful, else any error code.
	 *
	 * When allocation of isochronous resource finishes, the ::allocated
	 * handler is called to notify the result, channel, and bandwidth.
	 */
	fw_iso_resource_sigs[FW_ISO_RESOURCE_SIG_ALLOCATED] =
		g_signal_new("allocated",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinokoFwIsoResourceClass, allocated),
			     NULL, NULL,
			     hinoko_sigs_marshal_VOID__UINT_UINT_UINT,
			     G_TYPE_NONE,
			     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

	/**
	 * HinokoFwIsoResource::deallocated:
	 * @self: A #HinokoFwIsoResource.
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @err_code: 0 if successful, else any error code.
	 *
	 * When deallocation of isochronous resource finishes, the ::deallocated
	 * handler is called to notify the result, channel, and bandwidth.
	 */
	fw_iso_resource_sigs[FW_ISO_RESOURCE_SIG_DEALLOCATED] =
		g_signal_new("deallocated",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinokoFwIsoResourceClass, deallocated),
			     NULL, NULL,
			     hinoko_sigs_marshal_VOID__UINT_UINT_UINT,
			     G_TYPE_NONE,
			     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
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
 * Allocate and return an instance of #HinokoFwIsoResource.
 *
 * Returns: A #HinokoFwIsoResource.
 */
HinokoFwIsoResource *hinoko_fw_iso_resource_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE, NULL);
}

/**
 * hinoko_fw_iso_resource_open:
 * @self: A #HinokoFwIsoResource.
 * @path: A path of any Linux FireWire character device.
 * @open_flag: The flag of open(2) system call. O_RDONLY is forced to fulfil
 *	       internally.
 * @exception: A #GError.
 *
 * Open Linux FireWire character device to delegate any request for isochronous
 * resource management to Linux FireWire subsystem.
 */
void hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path,
				 gint open_flag, GError **exception)
{
	HinokoFwIsoResourcePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	priv = hinoko_fw_iso_resource_get_instance_private(self);

	open_flag |= O_RDONLY;
	priv->fd = open(path, open_flag);
	if (priv->fd < 0)
		generate_error(exception, errno);
}

/**
 * hinoko_fw_iso_resource_allocate_once_async:
 * @self: A #HinokoFwIsoResource.
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @exception: A #GError.
 *
 * Initiate allocation of isochronous resource without any wait. When the
 * allocation finishes, ::allocated signal is emit to notify the result,
 * channel, and bandwidth.
 */
void hinoko_fw_iso_resource_allocate_once_async(HinokoFwIsoResource *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **exception)
{
	HinokoFwIsoResourcePrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	priv = hinoko_fw_iso_resource_get_instance_private(self);

	g_return_if_fail(channel_candidates != NULL);
	g_return_if_fail(channel_candidates_count > 0);
	g_return_if_fail(bandwidth > 0);

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	if (ioctl(priv->fd, FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE, &res) < 0)
		generate_error(exception, errno);
}

/**
 * hinoko_fw_iso_resource_deallocate_once_async:
 * @self: A #HinokoFwIsoResource.
 * @channel: The channel number to be deallocated.
 * @bandwidth: The amount of bandwidth to be deallocated.
 * @exception: A #GError.
 *
 * Initiate deallocation of isochronous resource without any wait. When the
 * deallocation finishes, ::deallocated signal is emit to notify the result,
 * channel, and bandwidth.
 */
void hinoko_fw_iso_resource_deallocate_once_async(HinokoFwIsoResource *self,
						  guint channel,
						  guint bandwidth,
						  GError **exception)
{
	HinokoFwIsoResourcePrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));
	priv = hinoko_fw_iso_resource_get_instance_private(self);

	g_return_if_fail(channel < 64);
	g_return_if_fail(bandwidth > 0);

	res.channels = 1ull << channel;
	res.bandwidth = bandwidth;

	if (ioctl(priv->fd, FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE, &res) < 0)
		generate_error(exception, errno);
}

struct waiter {
	GMutex mutex;
	GCond cond;
	guint err_code;
	gboolean handled;
};

static void handle_event_signal(HinokoFwIsoResource *self, guint channel,
				guint bandwidth, guint err_code,
				gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	g_mutex_lock(&w->mutex);
	if (err_code > 0)
		w->err_code = err_code;
	w->handled = TRUE;
	g_cond_signal(&w->cond);
	g_mutex_unlock(&w->mutex);
}

/**
 * hinoko_fw_iso_resource_allocate_once_sync:
 * @self: A #HinokoFwIsoResource.
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @exception: A #GError.
 *
 * Initiate allocation of isochronous resource and wait for ::allocated signal.
 */
void hinoko_fw_iso_resource_allocate_once_sync(HinokoFwIsoResource *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth,
					       GError **exception)
{
	struct waiter w;
	guint64 expiration;
	gulong handler_id;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));

	g_mutex_init(&w.mutex);
	g_cond_init(&w.cond);
	w.err_code = 0;
	w.handled = FALSE;

	// For safe, use 100 msec for timeout.
	expiration = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;

	handler_id = g_signal_connect(self, "allocated",
				      (GCallback)handle_event_signal, &w);

	hinoko_fw_iso_resource_allocate_once_async(self, channel_candidates,
						   channel_candidates_count,
						   bandwidth, exception);
	if (*exception != NULL) {
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
		generate_error(exception, ETIMEDOUT);
	else if (w.err_code > 0)
		generate_error(exception, w.err_code);
}

/**
 * hinoko_fw_iso_resource_deallocate_once_sync:
 * @self: A #HinokoFwIsoResource.
 * @channel: The channel number to be deallocated.
 * @bandwidth: The amount of bandwidth to be deallocated.
 * @exception: A #GError.
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * ::deallocated signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_deallocate_once_sync(HinokoFwIsoResource *self,
						 guint channel,
						 guint bandwidth,
						 GError **exception)
{
	struct waiter w;
	guint64 expiration;
	gulong handler_id;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self));

	g_mutex_init(&w.mutex);
	g_cond_init(&w.cond);
	w.err_code = 0;
	w.handled = FALSE;

	// For safe, use 100 msec for timeout.
	expiration = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;

	handler_id = g_signal_connect(self, "deallocated",
				      (GCallback)handle_event_signal, &w);

	hinoko_fw_iso_resource_deallocate_once_async(self, channel, bandwidth,
						     exception);
	if (*exception != NULL) {
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
		generate_error(exception, ETIMEDOUT);
	else if (w.err_code > 0)
		generate_error(exception, w.err_code);
}

static void handle_iso_resource_event(HinokoFwIsoResource *self,
				      struct fw_cdev_event_iso_resource *ev)
{
	guint channel;
	guint bandwidth;
	guint err_code;
	int sig_type;

	if (ev->channel >= 0) {
		channel = ev->channel;
		bandwidth = ev->bandwidth;
		err_code = 0;
	} else {
		channel = 0;
		bandwidth = 0;
		err_code = -ev->channel;
	}

	if (ev->type == FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED)
		sig_type = FW_ISO_RESOURCE_SIG_ALLOCATED;
	else
		sig_type = FW_ISO_RESOURCE_SIG_DEALLOCATED;

	g_signal_emit(self, fw_iso_resource_sigs[sig_type],
		      0, channel, bandwidth, err_code);
}

static gboolean check_src(GSource *gsrc)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an exception, return
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
 * @self: A #hinokoFwIsoResource.
 * @gsrc: (out): A #GSource.
 * @exception: A #GError.
 *
 * Create Gsource for GMainContext to dispatch events for isochronous resource.
 */
void hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self,
					  GSource **gsrc, GError **exception)
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
	priv = hinoko_fw_iso_resource_get_instance_private(self);

	*gsrc = g_source_new(&funcs, sizeof(FwIsoResourceSource));
	if (*gsrc == NULL) {
		generate_error(exception, ENOMEM);
		return;
	}

	g_source_set_name(*gsrc, "HinokoFwIsoResource");
	g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*gsrc, TRUE);

	src = (FwIsoResourceSource *)(*gsrc);

	src->buf = g_malloc0(page_size);
	if (src->buf == NULL) {
		generate_error(exception, ENOMEM);
		g_source_unref(*gsrc);
		return;
	}
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
