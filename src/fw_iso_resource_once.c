// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_private.h"

/**
 * HinokoFwIsoResourceOnce:
 * An object to initiate requests and listen events of isochronous resource allocation/deallocation
 * by one shot.
 *
 * The [class@FwIsoResourceOnce] is an object to initiate requests and listen events of isochronous
 * resource allocation/deallocation by file descriptor owned internally. The allocated resource
 * is left even if this object is destroyed, thus application is responsible for deallocation.
 */
typedef struct {
	int fd;
} HinokoFwIsoResourceOncePrivate;

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoResourceOnce, hinoko_fw_iso_resource_once, HINOKO_TYPE_FW_ISO_RESOURCE,
			G_ADD_PRIVATE(HinokoFwIsoResourceOnce))

static void fw_iso_resource_once_finalize(GObject *obj)
{
	HinokoFwIsoResourceOnce *self = HINOKO_FW_ISO_RESOURCE_ONCE(obj);
	HinokoFwIsoResourceOncePrivate *priv =
		hinoko_fw_iso_resource_once_get_instance_private(self);

	if (priv->fd >= 0)
		close(priv->fd);

	G_OBJECT_CLASS(hinoko_fw_iso_resource_once_parent_class)->finalize(obj);
}

static gboolean fw_iso_resource_once_open(HinokoFwIsoResource *inst, const gchar *path,
					  gint open_flag, GError **error);

static gboolean fw_iso_resource_once_create_source(HinokoFwIsoResource *inst, GSource **source,
						   GError **error);

static void hinoko_fw_iso_resource_once_class_init(HinokoFwIsoResourceOnceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	HinokoFwIsoResourceClass *parent_class = HINOKO_FW_ISO_RESOURCE_CLASS(klass);

	gobject_class->finalize = fw_iso_resource_once_finalize;

	parent_class->open = fw_iso_resource_once_open;
	parent_class->create_source = fw_iso_resource_once_create_source;
}

static void hinoko_fw_iso_resource_once_init(HinokoFwIsoResourceOnce *self)
{
	HinokoFwIsoResourceOncePrivate *priv =
		hinoko_fw_iso_resource_once_get_instance_private(self);

	priv->fd = -1;
}

static gboolean fw_iso_resource_once_open(HinokoFwIsoResource *inst, const gchar *path,
					  gint open_flag, GError **error)
{
	HinokoFwIsoResourceOnce *self;
	HinokoFwIsoResourceOncePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(inst), FALSE);
	self = HINOKO_FW_ISO_RESOURCE_ONCE(inst);
	priv = hinoko_fw_iso_resource_once_get_instance_private(self);

	return fw_iso_resource_open(&priv->fd, path, open_flag, error);
}

void fw_iso_resource_once_handle_event(HinokoFwIsoResource *inst, const char *signal_name,
				       guint channel, guint bandwidth, const GError *error)
{
	g_signal_emit_by_name(inst, signal_name, channel, bandwidth, error);
}

static gboolean fw_iso_resource_once_create_source(HinokoFwIsoResource *inst, GSource **source,
						   GError **error)
{
	HinokoFwIsoResourceOnce *self;
	HinokoFwIsoResourceOncePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(inst), FALSE);
	self = HINOKO_FW_ISO_RESOURCE_ONCE(inst);
	priv = hinoko_fw_iso_resource_once_get_instance_private(self);

	return fw_iso_resource_create_source(priv->fd, inst, fw_iso_resource_once_handle_event,
					     source, error);
}

/**
 * hinoko_fw_iso_resource_once_new:
 *
 * Allocate and return an instance of [class@FwIsoResourceOnce].
 *
 * Returns: A [class@FwIsoResourceOnce].
 *
 * Sine: 0.7.
 */
HinokoFwIsoResourceOnce *hinoko_fw_iso_resource_once_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE_ONCE, NULL);
}

/**
 * hinoko_fw_iso_resource_once_allocate_async:
 * @self: A [class@FwIsoResourceOnce].
 * @channel_candidates: (array length=channel_candidates_count): The array with elements for
 *			numeric number for isochronous channel to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate allocation of isochronous resource without any wait. When the allocation finishes,
 * [signal@FwIsoResource::allocated] signal is emit to notify the result, channel, and bandwidth.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_once_allocate_async(HinokoFwIsoResourceOnce *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth, GError **error)
{
	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	g_return_if_fail(channel_candidates != NULL);
	g_return_if_fail(channel_candidates_count > 0);
	g_return_if_fail(bandwidth > 0);

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE, &res, error);
}

/**
 * hinoko_fw_iso_resource_once_deallocate_async:
 * @self: A [class@FwIsoResourceOnce].
 * @channel: The channel number to be deallocated.
 * @bandwidth: The amount of bandwidth to be deallocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate deallocation of isochronous resource without any wait. When the
 * deallocation finishes, [signal@FwIsoResource::deallocated] signal is emit to notify the result,
 * channel, and bandwidth.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_once_deallocate_async(HinokoFwIsoResourceOnce *self, guint channel,
						  guint bandwidth, GError **error)
{
	struct fw_cdev_allocate_iso_resource res = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	g_return_if_fail(channel < 64);
	g_return_if_fail(bandwidth > 0);

	res.channels = 1ull << channel;
	res.bandwidth = bandwidth;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE, &res, error);
}

/**
 * hinoko_fw_iso_resource_once_allocate_sync:
 * @self: A [class@FwIsoResourceOnce].
 * @channel_candidates: (array length=channel_candidates_count): The array with elements for
 *			numeric number for isochronous channel to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @timeout_ms: The timeout to wait for allocated event.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate allocation of isochronous resource and wait for [signal@FwIsoResource::allocated]
 * signal.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_once_allocate_sync(HinokoFwIsoResourceOnce *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth, guint timeout_ms,
					       GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	fw_iso_resource_waiter_init(HINOKO_FW_ISO_RESOURCE(self), &w, ALLOCATED_SIGNAL_NAME,
				    timeout_ms);

	hinoko_fw_iso_resource_once_allocate_async(self, channel_candidates,
						   channel_candidates_count, bandwidth, error);

	fw_iso_resource_waiter_wait(HINOKO_FW_ISO_RESOURCE(self), &w, error);
}

/**
 * hinoko_fw_iso_resource_once_deallocate_sync:
 * @self: A [class@FwIsoResourceOnce].
 * @channel: The channel number to be deallocated.
 * @bandwidth: The amount of bandwidth to be deallocated.
 * @timeout_ms: The timeout to wait for deallocated event.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinoko.FwIsoResourceError.
 *
 * Initiate deallocation of isochronous resource and wait for [signal@FwIsoResource::deallocated]
 * signal.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_once_deallocate_sync(HinokoFwIsoResourceOnce *self, guint channel,
						 guint bandwidth, guint timeout_ms,
						 GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	fw_iso_resource_waiter_init(HINOKO_FW_ISO_RESOURCE(self), &w, DEALLOCATED_SIGNAL_NAME,
				    timeout_ms);

	hinoko_fw_iso_resource_once_deallocate_async(self, channel, bandwidth, error);

	fw_iso_resource_waiter_wait(HINOKO_FW_ISO_RESOURCE(self), &w, error);
}
