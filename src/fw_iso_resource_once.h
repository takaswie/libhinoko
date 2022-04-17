// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_ONCE_H__
#define __HINOKO_FW_ISO_RESOURCE_ONCE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE_ONCE	(hinoko_fw_iso_resource_once_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoResourceOnce, hinoko_fw_iso_resource_once, HINOKO,
			 FW_ISO_RESOURCE_ONCE, HinokoFwIsoResource);

struct _HinokoFwIsoResourceOnceClass {
	HinokoFwIsoResourceClass parent_class;
};

HinokoFwIsoResourceOnce *hinoko_fw_iso_resource_once_new();

void hinoko_fw_iso_resource_once_allocate_async(HinokoFwIsoResourceOnce *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth, GError **error);

void hinoko_fw_iso_resource_once_deallocate_async(HinokoFwIsoResourceOnce *self, guint channel,
						  guint bandwidth, GError **error);

void hinoko_fw_iso_resource_once_allocate_sync(HinokoFwIsoResourceOnce *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth, guint timeout_ms,
					       GError **error);

void hinoko_fw_iso_resource_once_deallocate_sync(HinokoFwIsoResourceOnce *self, guint channel,
						 guint bandwidth, guint timeout_ms,
						 GError **error);

G_END_DECLS

#endif
