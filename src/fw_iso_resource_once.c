// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

/**
 * HinokoFwIsoResourceOnce:
 * An object to initiate requests and listen events of isochronous resource allocation/deallocation
 * by one shot.
 *
 * The [class@FwIsoResourceOnce] is an object to initiate requests and listen events of isochronous
 * resource allocation/deallocation by file descriptor owned internally. The allocated resource
 * is left even if this object is destroyed, thus application is responsible for deallocation.
 */
G_DEFINE_TYPE(HinokoFwIsoResourceOnce, hinoko_fw_iso_resource_once, HINOKO_TYPE_FW_ISO_RESOURCE)

static void hinoko_fw_iso_resource_once_class_init(HinokoFwIsoResourceOnceClass *klass)
{
	return;
}

static void hinoko_fw_iso_resource_once_init(HinokoFwIsoResourceOnce *self)
{
	return;
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
