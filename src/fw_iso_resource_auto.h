// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_AUTO_H__
#define __HINOKO_FW_ISO_RESOURCE_AUTO_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE_AUTO	(hinoko_fw_iso_resource_auto_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto, HINOKO,
			 FW_ISO_RESOURCE_AUTO, HinokoFwIsoResource);

#define HINOKO_FW_ISO_RESOURCE_AUTO_ERROR	hinoko_fw_iso_resource_auto_error_quark()

GQuark hinoko_fw_iso_resource_auto_error_quark();

struct _HinokoFwIsoResourceAutoClass {
	HinokoFwIsoResourceClass parent_class;
};

HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new();

void hinoko_fw_iso_resource_auto_allocate_async(HinokoFwIsoResourceAuto *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **error);
void hinoko_fw_iso_resource_auto_allocate_sync(HinokoFwIsoResourceAuto *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth, guint timeout_ms,
					       GError **error);

void hinoko_fw_iso_resource_auto_deallocate_async(HinokoFwIsoResourceAuto *self,
						  GError **error);
void hinoko_fw_iso_resource_auto_deallocate_sync(HinokoFwIsoResourceAuto *self, guint timeout_ms,
						 GError **error);

G_END_DECLS

#endif
