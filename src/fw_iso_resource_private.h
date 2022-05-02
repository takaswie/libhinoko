// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__
#define __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__

#include "hinoko.h"

#include <unistd.h>
#include <errno.h>

void hinoko_fw_iso_resource_ioctl(HinokoFwIsoResource *self,
				  unsigned long request, void *argp,
				  GError **error);

void hinoko_fw_iso_resource_auto_handle_event(HinokoFwIsoResourceAuto *self, guint channel,
					      guint bandwidth, const char *signal_name,
					      const GError *error);

struct fw_iso_resource_waiter {
	GMutex mutex;
	GCond cond;
	GError *error;
	gboolean handled;
	guint64 expiration;
	gulong handler_id;
};

void fw_iso_resource_waiter_init(HinokoFwIsoResource *self, struct fw_iso_resource_waiter *w,
				 const char *signal_name, guint timeout_ms);

void fw_iso_resource_waiter_wait(HinokoFwIsoResource *self, struct fw_iso_resource_waiter *w,
				 GError **error);

#endif
