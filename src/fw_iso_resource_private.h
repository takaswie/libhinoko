// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__
#define __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__

#include "hinoko.h"

#include <unistd.h>
#include <errno.h>

#define ALLOCATED_SIGNAL_NAME		"allocated"
#define DEALLOCATED_SIGNAL_NAME		"deallocated"

gboolean fw_iso_resource_open(int *fd, const gchar *path, gint open_flag, GError **error);

gboolean fw_iso_resource_create_source(int fd, HinokoFwIsoResource *inst,
				       void (*handle_event)(HinokoFwIsoResource *self,
							    const char *signal_name, guint channel,
							    guint bandwidth, const GError *error),
				       GSource **source, GError **error);

void hinoko_fw_iso_resource_ioctl(HinokoFwIsoResource *self,
				  unsigned long request, void *argp,
				  GError **error);

void fw_iso_resource_auto_handle_event(HinokoFwIsoResource *inst, const char *signal_name,
				       guint channel, guint bandwidth, const GError *error);

void fw_iso_resource_once_handle_event(HinokoFwIsoResource *inst, const char *signal_name,
				       guint channel, guint bandwidth, const GError *error);

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
