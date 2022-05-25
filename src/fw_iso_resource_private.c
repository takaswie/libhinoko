// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

#define generate_event_error(error, errno)			\
	g_set_error(error, HINOKO_FW_ISO_RESOURCE_ERROR,	\
		    HINOKO_FW_ISO_RESOURCE_ERROR_EVENT,		\
		    "%d %s", errno, strerror(errno))

typedef struct {
	GSource src;
	HinokoFwIsoResource *self;
	gpointer tag;
	gsize len;
	guint8 *buf;
	int fd;
	void (*handle_event)(HinokoFwIsoResource *self, const union fw_cdev_event *event);
} FwIsoResourceSource;

void fw_iso_resource_class_override_properties(GObjectClass *gobject_class)
{
	g_object_class_override_property(gobject_class, FW_ISO_RESOURCE_PROP_TYPE_GENERATION,
					 GENERATION_PROP_NAME);
}

void fw_iso_resource_state_get_property(const struct fw_iso_resource_state *state, GObject *obj,
					guint id, GValue *val, GParamSpec *spec)
{
	switch (id) {
	case FW_ISO_RESOURCE_PROP_TYPE_GENERATION:
		g_value_set_uint(val, state->bus_state.generation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

void fw_iso_resource_state_init(struct fw_iso_resource_state *state)
{
	state->fd = -1;
}

void fw_iso_resource_state_release(struct fw_iso_resource_state *state)
{
	if (state->fd >= 0)
		close(state->fd);
	state->fd = -1;
}

gboolean fw_iso_resource_state_open(struct fw_iso_resource_state *state, const gchar *path,
				    gint open_flag, GError **error)
{
	g_return_val_if_fail(path != NULL && strlen(path) > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd >= 0) {
		generate_coded_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_OPENED);
		return FALSE;
	}

	open_flag |= O_RDONLY;
	state->fd = open(path, open_flag);
	if (state->fd < 0) {
		GFileError code = g_file_error_from_errno(errno);
		if (code != G_FILE_ERROR_FAILED)
			generate_file_error(error, code, "open(%s)", path);
		else
			generate_syscall_error(error, errno, "open(%s)", path);
		return FALSE;
	}

	if (!fw_iso_resource_state_cache_bus_state(state, error)) {
		fw_iso_resource_state_release(state);
		return FALSE;
	}

	return TRUE;
}

static gboolean check_src(GSource *source)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)source;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(source, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *source, GSourceFunc cb, gpointer user_data)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)source;
	GIOCondition condition;
	ssize_t len;
	const union fw_cdev_event *ev;

	if (src->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(source, src->tag);
	if (condition & G_IO_ERR)
		return G_SOURCE_REMOVE;

	len = read(src->fd, src->buf, src->len);
	if (len <= 0) {
		if (errno == EAGAIN)
			return G_SOURCE_CONTINUE;

		return G_SOURCE_REMOVE;
	}

	ev = (const union fw_cdev_event *)src->buf;
	switch (ev->common.type) {
	case FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED:
	case FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED:
	case FW_CDEV_EVENT_BUS_RESET:
		src->handle_event(src->self, ev);
		break;
	default:
		break;
	}

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *source)
{
	FwIsoResourceSource *src = (FwIsoResourceSource *)source;

	g_free(src->buf);
	g_object_unref(src->self);
}

gboolean fw_iso_resource_state_create_source(struct fw_iso_resource_state *state,
					     HinokoFwIsoResource *inst,
					     void (*handle_event)(HinokoFwIsoResource *self,
								  const union fw_cdev_event *event),
					     GSource **source, GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	long page_size = sysconf(_SC_PAGESIZE);
	FwIsoResourceSource *src;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE(inst), FALSE);
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(error != NULL && *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_coded_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return FALSE;
	}

	*source = g_source_new(&funcs, sizeof(FwIsoResourceSource));

	g_source_set_name(*source, "HinokoFwIsoResource");

	src = (FwIsoResourceSource *)(*source);

	src->buf = g_malloc0(page_size);

	src->len = (gsize)page_size;
	src->tag = g_source_add_unix_fd(*source, state->fd, G_IO_IN);
	src->fd = state->fd;
	src->self = g_object_ref(inst);
	src->handle_event = handle_event;

	if (!fw_iso_resource_state_cache_bus_state(state, error)) {
		g_source_unref(*source);
		return FALSE;
	}

	return TRUE;
}

gboolean fw_iso_resource_state_cache_bus_state(struct fw_iso_resource_state *state, GError **error)
{
	struct fw_cdev_get_info get_info = {0};

	get_info.version = 5;
	get_info.bus_reset = (__u64)&state->bus_state;

	if (ioctl(state->fd, FW_CDEV_IOC_GET_INFO, &get_info) < 0) {
		generate_ioctl_error(error, errno, FW_CDEV_IOC_GET_INFO);
		return FALSE;
	}

	return TRUE;
}

static void handle_event_signal(HinokoFwIsoResource *self, guint channel, guint bandwidth,
				const GError *error, gpointer user_data)
{
	struct fw_iso_resource_waiter *w = (struct fw_iso_resource_waiter *)user_data;

	g_mutex_lock(&w->mutex);
	if (error != NULL)
		w->error = g_error_copy(error);
	w->handled = TRUE;
	g_cond_signal(&w->cond);
	g_mutex_unlock(&w->mutex);
}

void fw_iso_resource_waiter_init(struct fw_iso_resource_waiter *w, HinokoFwIsoResource *self,
				 const char *signal_name, guint timeout_ms)
{
	g_mutex_init(&w->mutex);
	g_cond_init(&w->cond);
	w->error = NULL;
	w->handled = FALSE;
	w->expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;
	w->handler_id = g_signal_connect(self, signal_name, G_CALLBACK(handle_event_signal), w);
}

gboolean fw_iso_resource_waiter_wait(struct fw_iso_resource_waiter *w, HinokoFwIsoResource *self,
				     GError **error)
{
	gboolean result;

	if (*error != NULL) {
		g_signal_handler_disconnect(self, w->handler_id);
		return FALSE;
	}

	g_mutex_lock(&w->mutex);
	while (w->handled == FALSE) {
		if (!g_cond_wait_until(&w->cond, &w->mutex, w->expiration))
			break;
	}
	g_signal_handler_disconnect(self, w->handler_id);
	g_mutex_unlock(&w->mutex);

	if (w->handled == FALSE) {
		generate_coded_error(error, HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT);
		result = FALSE;
	} else if (w->error != NULL) {
		*error = w->error;	// Delegate ownership.
		result = FALSE;
	} else {
		result = TRUE;
	}

	return result;
}

void parse_iso_resource_event(const struct fw_cdev_event_iso_resource *ev, guint *channel,
			      guint *bandwidth, const char **signal_name, GError **error)
{
	*error = NULL;

	if (ev->channel >= 0) {
		*channel = ev->channel;
		*bandwidth = ev->bandwidth;
	} else {
		*channel = 0;
		*bandwidth = 0;
		generate_event_error(error, -ev->channel);
	}

	if (ev->type == FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED)
		*signal_name = ALLOCATED_SIGNAL_NAME;
	else
		*signal_name = DEALLOCATED_SIGNAL_NAME;
}
