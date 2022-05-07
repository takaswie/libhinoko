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
	struct fw_iso_resource_state state;
} HinokoFwIsoResourceOncePrivate;

static void fw_iso_resource_iface_init(HinokoFwIsoResourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoResourceOnce, hinoko_fw_iso_resource_once, G_TYPE_OBJECT,
			G_ADD_PRIVATE(HinokoFwIsoResourceOnce)
                        G_IMPLEMENT_INTERFACE(HINOKO_TYPE_FW_ISO_RESOURCE, fw_iso_resource_iface_init))

static void fw_iso_resource_once_get_property(GObject *obj, guint id, GValue *val, GParamSpec *spec)
{
	HinokoFwIsoResourceOnce *self = HINOKO_FW_ISO_RESOURCE_ONCE(obj);
	HinokoFwIsoResourceOncePrivate *priv =
			hinoko_fw_iso_resource_once_get_instance_private(self);

	fw_iso_resource_state_get_property(&priv->state, obj, id, val, spec);
}

static void fw_iso_resource_once_finalize(GObject *obj)
{
	HinokoFwIsoResourceOnce *self = HINOKO_FW_ISO_RESOURCE_ONCE(obj);
	HinokoFwIsoResourceOncePrivate *priv =
		hinoko_fw_iso_resource_once_get_instance_private(self);

	fw_iso_resource_state_release(&priv->state);

	G_OBJECT_CLASS(hinoko_fw_iso_resource_once_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_resource_once_class_init(HinokoFwIsoResourceOnceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_resource_once_get_property;
	gobject_class->finalize = fw_iso_resource_once_finalize;

	fw_iso_resource_class_override_properties(gobject_class);
}

static void hinoko_fw_iso_resource_once_init(HinokoFwIsoResourceOnce *self)
{
	HinokoFwIsoResourceOncePrivate *priv =
		hinoko_fw_iso_resource_once_get_instance_private(self);

	fw_iso_resource_state_init(&priv->state);
}

static gboolean fw_iso_resource_once_open(HinokoFwIsoResource *inst, const gchar *path,
					  gint open_flag, GError **error)
{
	HinokoFwIsoResourceOnce *self;
	HinokoFwIsoResourceOncePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(inst), FALSE);
	self = HINOKO_FW_ISO_RESOURCE_ONCE(inst);
	priv = hinoko_fw_iso_resource_once_get_instance_private(self);

	return fw_iso_resource_state_open(&priv->state, path, open_flag, error);
}

static gboolean fw_iso_resource_once_allocate_async(HinokoFwIsoResource *inst,
						    guint8 *channel_candidates,
						    gsize channel_candidates_count,
						    guint bandwidth, GError **error)
{
	HinokoFwIsoResourceOnce *self;
	HinokoFwIsoResourceOncePrivate *priv;

	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(inst), FALSE);
	g_return_val_if_fail(channel_candidates != NULL, FALSE);
	g_return_val_if_fail(channel_candidates_count > 0, FALSE);
	g_return_val_if_fail(bandwidth > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	self = HINOKO_FW_ISO_RESOURCE_ONCE(inst);
	priv = hinoko_fw_iso_resource_once_get_instance_private(self);
	if (priv->state.fd < 0) {
		generate_coded_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return FALSE;
	}

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	if (ioctl(priv->state.fd, FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE, &res) < 0) {
		generate_ioctl_error(error, errno, FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE_ONCE);
		return FALSE;
	}

	return TRUE;
}
static void handle_iso_resource_event(HinokoFwIsoResourceOnce *self,
				      const struct fw_cdev_event_iso_resource *ev)
{
	const char *signal_name;
	guint channel;
	guint bandwidth;
	GError *error;

	parse_iso_resource_event(ev, &channel, &bandwidth, &signal_name, &error);

	g_signal_emit_by_name(self, signal_name, channel, bandwidth, error);

	if (error != NULL)
		g_clear_error(&error);
}

static void handle_bus_reset_event(HinokoFwIsoResourceOnce *self,
				   const struct fw_cdev_event_bus_reset *ev)
{
	HinokoFwIsoResourceOncePrivate *priv =
		hinoko_fw_iso_resource_once_get_instance_private(self);
	gboolean need_notify = (priv->state.bus_state.generation != ev->generation);

	memcpy(&priv->state.bus_state, ev, sizeof(*ev));

	if (need_notify)
		g_object_notify(G_OBJECT(self), GENERATION_PROP_NAME);
}

void fw_iso_resource_once_handle_event(HinokoFwIsoResource *inst, const union fw_cdev_event *event)
{
	HinokoFwIsoResourceOnce *self;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(inst));
	self = HINOKO_FW_ISO_RESOURCE_ONCE(inst);

	switch (event->common.type) {
	case FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED:
	case FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED:
		handle_iso_resource_event(self, &event->iso_resource);
		break;
	case FW_CDEV_EVENT_BUS_RESET:
		handle_bus_reset_event(self, &event->bus_reset);
		break;
	default:
		break;
	}
}

static gboolean fw_iso_resource_once_create_source(HinokoFwIsoResource *inst, GSource **source,
						   GError **error)
{
	HinokoFwIsoResourceOnce *self;
	HinokoFwIsoResourceOncePrivate *priv;
	guint generation;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(inst), FALSE);
	self = HINOKO_FW_ISO_RESOURCE_ONCE(inst);
	priv = hinoko_fw_iso_resource_once_get_instance_private(self);

	generation = priv->state.bus_state.generation;

	if (!fw_iso_resource_state_create_source(&priv->state, inst,
						 fw_iso_resource_once_handle_event, source, error))
		return FALSE;

	if (generation != priv->state.bus_state.generation)
		g_object_notify(G_OBJECT(self), GENERATION_PROP_NAME);

	return TRUE;
}

static void fw_iso_resource_iface_init(HinokoFwIsoResourceInterface *iface)
{
	iface->open = fw_iso_resource_once_open;
	iface->allocate_async = fw_iso_resource_once_allocate_async;
	iface->create_source = fw_iso_resource_once_create_source;

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
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 0.7.
 */
gboolean hinoko_fw_iso_resource_once_deallocate_async(HinokoFwIsoResourceOnce *self, guint channel,
						      guint bandwidth, GError **error)
{
	HinokoFwIsoResourceOncePrivate *priv;

	struct fw_cdev_allocate_iso_resource res = {0};

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	g_return_val_if_fail(channel < 64, FALSE);
	g_return_val_if_fail(bandwidth > 0, FALSE);

	priv = hinoko_fw_iso_resource_once_get_instance_private(self);
	if (priv->state.fd < 0) {
		generate_coded_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return FALSE;
	}

	res.channels = 1ull << channel;
	res.bandwidth = bandwidth;

	if (ioctl(priv->state.fd, FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE, &res) < 0) {
		generate_ioctl_error(error, errno, FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE_ONCE);
		return FALSE;
	}

	return TRUE;
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
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 0.7.
 */
gboolean hinoko_fw_iso_resource_once_deallocate_sync(HinokoFwIsoResourceOnce *self, guint channel,
						     guint bandwidth, guint timeout_ms,
						     GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_ONCE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	fw_iso_resource_waiter_init(&w, HINOKO_FW_ISO_RESOURCE(self), DEALLOCATED_SIGNAL_NAME,
				    timeout_ms);

	(void)hinoko_fw_iso_resource_once_deallocate_async(self, channel, bandwidth, error);

	return fw_iso_resource_waiter_wait(&w, HINOKO_FW_ISO_RESOURCE(self), error);
}
