// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINOKO_FW_ISO_RESOURCE_AUTO_H__
#define __ORG_KERNEL_HINOKO_FW_ISO_RESOURCE_AUTO_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE_AUTO	(hinoko_fw_iso_resource_auto_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto, HINOKO,
			 FW_ISO_RESOURCE_AUTO, GObject);

#define HINOKO_FW_ISO_RESOURCE_AUTO_ERROR	hinoko_fw_iso_resource_auto_error_quark()

GQuark hinoko_fw_iso_resource_auto_error_quark();

struct _HinokoFwIsoResourceAutoClass {
	GObjectClass parent_class;
};

HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new(void);

gboolean hinoko_fw_iso_resource_auto_deallocate(HinokoFwIsoResourceAuto *self, GError **error);
gboolean hinoko_fw_iso_resource_auto_deallocate_wait(HinokoFwIsoResourceAuto *self,
						     guint timeout_ms, GError **error);

G_END_DECLS

#endif
