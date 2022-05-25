// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__
#define __HINOKO_FW_ISO_RESOURCE_PRIVATE_H__

#include "hinoko.h"

#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#define GENERATION_PROP_NAME		"generation"

#define ALLOCATED_SIGNAL_NAME		"allocated"
#define DEALLOCATED_SIGNAL_NAME		"deallocated"

// NOTE: make it public in later releases.
void hinoko_fw_iso_resource_error_to_label(HinokoFwIsoResourceError code, const char **label);

static inline void generate_fw_iso_resource_error_coded(GError **error, HinokoFwIsoResourceError code)
{
	const char *label;

	hinoko_fw_iso_resource_error_to_label(code, &label);
	g_set_error_literal(error, HINOKO_FW_ISO_RESOURCE_ERROR, code, label);
}

#define generate_syscall_error(error, errno, format, arg)		\
	g_set_error(error, HINOKO_FW_ISO_RESOURCE_ERROR,		\
		    HINOKO_FW_ISO_RESOURCE_ERROR_FAILED,		\
		    format " %d(%s)", arg, errno, strerror(errno))

#define generate_ioctl_error(error, errno, request)			\
	generate_syscall_error(error, errno, "ioctl(%s)", #request)

struct fw_iso_resource_state {
	int fd;
	struct fw_cdev_event_bus_reset bus_state;
};

enum fw_iso_resource_prop_type {
	FW_ISO_RESOURCE_PROP_TYPE_GENERATION = 1,
	FW_ISO_RESOURCE_PROP_TYPE_COUNT,
};

void fw_iso_resource_class_override_properties(GObjectClass *gobject_class);

void fw_iso_resource_state_get_property(const struct fw_iso_resource_state *state, GObject *obj,
					guint id, GValue *val, GParamSpec *spec);

void fw_iso_resource_state_init(struct fw_iso_resource_state *state);

void fw_iso_resource_state_release(struct fw_iso_resource_state *state);

gboolean fw_iso_resource_state_open(struct fw_iso_resource_state *state, const gchar *path,
				    gint open_flag, GError **error);

gboolean fw_iso_resource_state_create_source(struct fw_iso_resource_state *state,
					     HinokoFwIsoResource *inst,
					     void (*handle_event)(HinokoFwIsoResource *self,
								  const union fw_cdev_event *event),
					     GSource **source, GError **error);

gboolean fw_iso_resource_state_cache_bus_state(struct fw_iso_resource_state *state, GError **error);

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

gboolean fw_iso_resource_waiter_wait(struct fw_iso_resource_waiter *w, HinokoFwIsoResource *self,
				     GError **error);

void parse_iso_resource_event(const struct fw_cdev_event_iso_resource *ev, guint *channel,
			      guint *bandwidth, const char **signal_name, GError **error);

#endif
