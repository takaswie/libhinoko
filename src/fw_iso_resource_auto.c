// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_auto.h"

struct _HinokoFwIsoResourceAutoPrivate {
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto, HINOKO_TYPE_FW_ISO_RESOURCE)

static void hinoko_fw_iso_resource_auto_class_init(HinokoFwIsoResourceAutoClass *klass)
{
	return;
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
