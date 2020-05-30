// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_auto.h"

struct _HinokoFwIsoResourceAutoPrivate {
	gboolean allocated;
	guint channel;
	guint bandwidth;
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
	return;
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
