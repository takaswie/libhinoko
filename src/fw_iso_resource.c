// SPDX-License-Identifier: LGPL-2.1-or-later
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

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
