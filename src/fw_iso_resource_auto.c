// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_private.h"

/**
 * HinokoFwIsoResourceAuto:
 * An object to maintain allocated isochronous resource.
 *
 * A [class@FwIsoResourceAuto]is an object to maintain isochronous resource during the lifetime of
 * the object. The allocated isochronous resource is kept even if the generation of the bus
 * updates. The maintenance of allocated isochronous resource is done by Linux FireWire subsystem.
 */
typedef struct {
	gboolean is_allocated;
	guint channel;
	guint bandwidth;

	GMutex mutex;
	guint handle;
} HinokoFwIsoResourceAutoPrivate;

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto, HINOKO_TYPE_FW_ISO_RESOURCE,
			G_ADD_PRIVATE(HinokoFwIsoResourceAuto))

/**
 * hinoko_fw_iso_resource_auto_error_quark:
 *
 * Return the [alias@GLib.Quark] for error domain of [struct@GLib.Error] which has code in
 * Hinoko.FwIsoResourceAutoError.
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinoko-fw-iso-resource-auto-error-quark, hinoko_fw_iso_resource_auto_error)

static const char *const err_msgs[] = {
	[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED] =
		"The instance is already associated to allocated isochronous resources",
	[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED] =
		"The instance is not associated to allocated isochronous resources",
};

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR, code, err_msgs[code])

enum fw_iso_resource_auto_prop_type {
	FW_ISO_RESOURCE_AUTO_PROP_IS_ALLOCATED = 1,
	FW_ISO_RESOURCE_AUTO_PROP_CHANNEL,
	FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH,
	FW_ISO_RESOURCE_AUTO_PROP_COUNT,
};
static GParamSpec *fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_COUNT] = { NULL, };

static void fw_iso_resource_auto_get_property(GObject *obj, guint id,
					      GValue *val, GParamSpec *spec)
{
	HinokoFwIsoResourceAuto *self = HINOKO_FW_ISO_RESOURCE_AUTO(obj);
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	switch (id) {
	case FW_ISO_RESOURCE_AUTO_PROP_IS_ALLOCATED:
		g_value_set_boolean(val, priv->is_allocated);
		break;
	case FW_ISO_RESOURCE_AUTO_PROP_CHANNEL:
		g_value_set_uint(val, priv->channel);
		break;
	case FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH:
		g_value_set_uint(val, priv->bandwidth);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}

	g_mutex_unlock(&priv->mutex);
}

static void hinoko_fw_iso_resource_auto_class_init(HinokoFwIsoResourceAutoClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_resource_auto_get_property;

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_IS_ALLOCATED] =
		g_param_spec_boolean("is-allocated", "is-allocated",
				     "Whether to allocated or not.",
				     FALSE, G_PARAM_READABLE);

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_CHANNEL] =
		g_param_spec_uint("channel", "channel",
				  "The allocated channel number.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH] =
		g_param_spec_uint("bandwidth", "bandwidth",
				  "The allocated amount of bandwidth.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_ISO_RESOURCE_AUTO_PROP_COUNT,
					  fw_iso_resource_auto_props);
}

static void hinoko_fw_iso_resource_auto_init(HinokoFwIsoResourceAuto *self)
{
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_init(&priv->mutex);
}

void fw_iso_resource_auto_handle_event(HinokoFwIsoResource *inst, const char *signal_name,
				       guint channel, guint bandwidth, const GError *error)
{
	HinokoFwIsoResourceAuto *self;
	HinokoFwIsoResourceAutoPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(inst));
	self = HINOKO_FW_ISO_RESOURCE_AUTO(inst);
	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	if (!strcmp(signal_name, ALLOCATED_SIGNAL_NAME)) {
		if (error == NULL) {
			g_mutex_lock(&priv->mutex);
			priv->channel = channel;
			priv->bandwidth = bandwidth;
			priv->is_allocated = TRUE;
			g_mutex_unlock(&priv->mutex);
		}
	} else {
		if (error == NULL) {
			g_mutex_lock(&priv->mutex);
			priv->channel = 0;
			priv->bandwidth -= bandwidth;
			priv->is_allocated = FALSE;
			g_mutex_unlock(&priv->mutex);
		}
	}

	g_signal_emit_by_name(self, signal_name, channel, bandwidth, error);
}

/**
 * hinoko_fw_iso_resource_auto_new:
 *
 * Allocate and return an instance of [class@FwIsoResourceAuto]object.
 *
 * Returns: A [class@FwIsoResourceAuto]
 */
HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE_AUTO, NULL);
}

/**
 * hinoko_fw_iso_resource_auto_allocate_async:
 * @self: A [class@FwIsoResourceAuto]
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of
 *	   Hinoko.FwIsoResourceError and Hinoko.FwIsoResourceAutoError.
 *
 * Initiate allocation of isochronous resource. When the allocation is done,
 * [signal@FwIsoResource::allocated] signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_auto_allocate_async(HinokoFwIsoResourceAuto *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **error)
{
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_return_if_fail(channel_candidates != NULL);
	g_return_if_fail(channel_candidates_count > 0);
	g_return_if_fail(bandwidth > 0);

	g_mutex_lock(&priv->mutex);

	if (priv->is_allocated) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED);
		goto end;
	}

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE, &res,
				     error);
	if (*error == NULL)
		priv->handle = res.handle;
end:
	g_mutex_unlock(&priv->mutex);
}

/**
 * hinoko_fw_iso_resource_auto_deallocate_async:
 * @self: A [class@FwIsoResourceAuto]
 * @error: A [struct@GLib.Error]. Error can be generated with domain of
 *	   Hinoko.FwIsoResourceError, and Hinoko.FwIsoResourceAutoError.
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * [signal@FwIsoResource::deallocated] signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_auto_deallocate_async(HinokoFwIsoResourceAuto *self,
						  GError **error)
{
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_deallocate dealloc = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	if (!priv->is_allocated) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED);
		goto end;
	}

	dealloc.handle = priv->handle;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE,
				     &dealloc, error);
end:
	g_mutex_unlock(&priv->mutex);
}

/**
 * hinoko_fw_iso_resource_auto_allocate_sync:
 * @self: A [class@FwIsoResourceAuto]
 * @channel_candidates: (array length=channel_candidates_count): The array with elements for
 *			numerical number for isochronous channel to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @timeout_ms: The timeout to wait for allocated event.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of
 *	   Hinoko.FwIsoResourceError, and Hinoko.FwIsoResourceAutoError.
 *
 * Initiate allocation of isochronous resource and wait for [signal@FwIsoResource::allocated]
 * signal. When the call is successful, [property@FwIsoResourceAuto:channel] and
 * [property@FwIsoResourceAuto:bandwidth] properties are available.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_auto_allocate_sync(HinokoFwIsoResourceAuto *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth, guint timeout_ms,
					       GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error == NULL || *error == NULL);

	fw_iso_resource_waiter_init(HINOKO_FW_ISO_RESOURCE(self), &w, ALLOCATED_SIGNAL_NAME,
				    timeout_ms);

	hinoko_fw_iso_resource_auto_allocate_async(self, channel_candidates,
						   channel_candidates_count,
						   bandwidth, error);

	fw_iso_resource_waiter_wait(HINOKO_FW_ISO_RESOURCE(self), &w, error);
}

/**
 * hinoko_fw_iso_resource_auto_deallocate_sync:
 * @self: A [class@FwIsoResourceAuto]
 * @timeout_ms: The timeout to wait for allocated event by milli second unit.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of
 *	   Hinoko.FwIsoResourceError, and Hinoko.FwIsoResourceAutoError.
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * [signal@FwIsoResource::deallocated] signal is emit to notify the result, channel, and bandwidth.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_resource_auto_deallocate_sync(HinokoFwIsoResourceAuto *self, guint timeout_ms,
						 GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error == NULL || *error == NULL);

	fw_iso_resource_waiter_init(HINOKO_FW_ISO_RESOURCE(self), &w, DEALLOCATED_SIGNAL_NAME,
				    timeout_ms);

	hinoko_fw_iso_resource_auto_deallocate_async(self, error);

	fw_iso_resource_waiter_wait(HINOKO_FW_ISO_RESOURCE(self), &w, error);
}
