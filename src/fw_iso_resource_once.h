// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINOKO_FW_ISO_RESOURCE_ONCE_H__
#define __ORG_KERNEL_HINOKO_FW_ISO_RESOURCE_ONCE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE_ONCE	(hinoko_fw_iso_resource_once_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoResourceOnce, hinoko_fw_iso_resource_once, HINOKO,
			 FW_ISO_RESOURCE_ONCE, GObject);

struct _HinokoFwIsoResourceOnceClass {
	GObjectClass parent_class;
};

HinokoFwIsoResourceOnce *hinoko_fw_iso_resource_once_new();

gboolean hinoko_fw_iso_resource_once_deallocate(HinokoFwIsoResourceOnce *self, guint channel,
						guint bandwidth, GError **error);

gboolean hinoko_fw_iso_resource_once_deallocate_wait(HinokoFwIsoResourceOnce *self, guint channel,
						     guint bandwidth, guint timeout_ms,
						     GError **error);

G_END_DECLS

#endif
