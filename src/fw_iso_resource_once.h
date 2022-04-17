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

G_END_DECLS

#endif
