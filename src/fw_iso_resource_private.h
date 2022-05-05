// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__
#define __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__

#include "hinoko.h"

#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#define ALLOCATED_SIGNAL_NAME		"allocated"
#define DEALLOCATED_SIGNAL_NAME		"deallocated"

#define generate_syscall_error(error, errno, format, arg)		\
	g_set_error(error, HINOKO_FW_ISO_RESOURCE_ERROR,		\
		    HINOKO_FW_ISO_RESOURCE_ERROR_FAILED,		\
		    format " %d(%s)", arg, errno, strerror(errno))

gboolean fw_iso_resource_open(int *fd, const gchar *path, gint open_flag, GError **error);

gboolean fw_iso_resource_create_source(int fd, HinokoFwIsoResource *inst,
				       void (*handle_event)(HinokoFwIsoResource *self,
							    const union fw_cdev_event *event),
				       GSource **source, GError **error);

struct fw_iso_resource_waiter {
	GMutex mutex;
	GCond cond;
	GError *error;
	gboolean handled;
	guint64 expiration;
	gulong handler_id;
};

void fw_iso_resource_waiter_init(struct fw_iso_resource_waiter *w, HinokoFwIsoResource *self,
				 const char *signal_name, guint timeout_ms);

void fw_iso_resource_waiter_wait(struct fw_iso_resource_waiter *w, HinokoFwIsoResource *self,
				 GError **error);

void parse_iso_resource_event(const struct fw_cdev_event_iso_resource *ev, guint *channel,
			      guint *bandwidth, const char **signal_name, GError **error);

#endif
