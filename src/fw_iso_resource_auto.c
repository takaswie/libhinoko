// SPDX-License-Identifier: LGPL-2.1-or-later
#include <errno.h>

#include "internal.h"

#include <linux/firewire-cdev.h>

struct _HinokoFwIsoResourceAutoPrivate {
	gboolean allocated;
	guint channel;
	guint bandwidth;

	GMutex mutex;
	guint handle;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto, HINOKO_TYPE_FW_ISO_RESOURCE)

enum fw_iso_resource_auto_prop_type {
	FW_ISO_RESOURCE_AUTO_PROP_ALLOCATED = 1,
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
	case FW_ISO_RESOURCE_AUTO_PROP_ALLOCATED:
		g_value_set_boolean(val, priv->allocated);
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

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_ALLOCATED] =
		g_param_spec_boolean("allocated", "allocated",
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

/**
 * hinoko_fw_iso_resource_auto:
 *
 * Allocate and return an instance of #HinokoFwIsoResourceAuto object.
 *
 * Returns: A #HinokoFwIsoResourceAuto.
 */
HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE_AUTO, NULL);
}

/**
 * hinoko_fw_iso_resource_auto_allocate_async:
 * @self: A #HinokoFwIsoResourceAuto.
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @exception: A #GError.
 *
 * Initiate allocation of isochronous resource. When the allocation is done,
 * ::allocated signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_auto_allocate_async(HinokoFwIsoResourceAuto *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **exception)
{
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_return_if_fail(channel_candidates != NULL);
	g_return_if_fail(channel_candidates_count > 0);
	g_return_if_fail(bandwidth > 0);

	g_mutex_lock(&priv->mutex);

	if (priv->allocated) {
		generate_error(exception, EBUSY);
		goto end;
	}

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE, &res,
				     exception);
	if (*exception == NULL)
		priv->handle = res.handle;
end:
	g_mutex_unlock(&priv->mutex);
}

void hinoko_fw_iso_resource_auto_handle_event(HinokoFwIsoResourceAuto *self,
					struct fw_cdev_event_iso_resource *ev)
{
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	if (ev->type == FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED) {
		if (ev->channel >= 0) {
			g_mutex_lock(&priv->mutex);
			priv->channel = ev->channel;
			priv->bandwidth = ev->bandwidth;
			priv->allocated = TRUE;
			g_mutex_unlock(&priv->mutex);
		}
	}
}
