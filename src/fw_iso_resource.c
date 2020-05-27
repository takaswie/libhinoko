// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource.h"

struct _HinokoFwIsoResourcePrivate {
	int fd;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoResource, hinoko_fw_iso_resource, G_TYPE_OBJECT)

static void hinoko_fw_iso_resource_class_init(HinokoFwIsoResourceClass *klass)
{
	return;
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
