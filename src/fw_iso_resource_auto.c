// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <errno.h>

/**
 * SECTION:fw_iso_resource_auto
 * @Title: HinokoFwIsoResourceAuto
 * @Short_description: An object to maintain allocated isochronous resource.
 * @include: fw_iso_resource_auto.h
 *
 * A #HinokoFwIsoResourceAuto is an object to maintain isochronous resource
 * during the lifetime of the object. The allocated isochronous resource is
 * kept even if the generation of the bus updates. The maintenance of allocated
 * isochronous resource is done by Linux FireWire subsystem.
 */
typedef struct {
	gboolean allocated;
	guint channel;
	guint bandwidth;

	GMutex mutex;
	guint handle;
} HinokoFwIsoResourceAutoPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto,
			   HINOKO_TYPE_FW_ISO_RESOURCE)

/**
 * hinoko_fw_iso_resource_auto_error_quark:
 *
 * Return the GQuark for error domain of GError which has code in #HinokoFwIsoResourceAutoError.
 *
 * Returns: A #GQuark.
 */
G_DEFINE_QUARK(hinoko-fw-iso-resource-auto-error-quark, hinoko_fw_iso_resource_auto_error)

static const char *const err_msgs[] = {
	[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED] =
		"The instance is already associated to allocated isochronous resources",
	[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED] =
		"The instance is not associated to allocated isochronous resources",
	[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_TIMEOUT] =
		"No event to the request arrives within timeout.",
};

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR, code, err_msgs[code])

enum fw_iso_resource_auto_prop_type {
	FW_ISO_RESOURCE_AUTO_PROP_ALLOCATED = 1,
	FW_ISO_RESOURCE_AUTO_PROP_CHANNEL,
	FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH,
	FW_ISO_RESOURCE_AUTO_PROP_COUNT,
};
static GParamSpec *fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_COUNT] = { NULL, };

static void fw_iso_resource_auto_get_property(GObject *obj, guint id,
					      GValue *val, GParamSpec *spec)
{
	HinokoFwIsoResourceAuto *self = HINOKO_FW_ISO_RESOURCE_AUTO(obj);
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	switch (id) {
	case FW_ISO_RESOURCE_AUTO_PROP_ALLOCATED:
		g_value_set_boolean(val, priv->allocated);
		break;
	case FW_ISO_RESOURCE_AUTO_PROP_CHANNEL:
		g_value_set_uint(val, priv->channel);
		break;
	case FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH:
		g_value_set_uint(val, priv->bandwidth);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}

	g_mutex_unlock(&priv->mutex);
}

static void hinoko_fw_iso_resource_auto_class_init(HinokoFwIsoResourceAutoClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_resource_auto_get_property;

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_ALLOCATED] =
		g_param_spec_boolean("allocated", "allocated",
				     "Whether to allocated or not.",
				     FALSE, G_PARAM_READABLE);

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_CHANNEL] =
		g_param_spec_uint("channel", "channel",
				  "The allocated channel number.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);

	fw_iso_resource_auto_props[FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH] =
		g_param_spec_uint("bandwidth", "bandwidth",
				  "The allocated amount of bandwidth.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_ISO_RESOURCE_AUTO_PROP_COUNT,
					  fw_iso_resource_auto_props);
}

static void hinoko_fw_iso_resource_auto_init(HinokoFwIsoResourceAuto *self)
{
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_init(&priv->mutex);
}

/**
 * hinoko_fw_iso_resource_auto_new:
 *
 * Allocate and return an instance of #HinokoFwIsoResourceAuto object.
 *
 * Returns: A #HinokoFwIsoResourceAuto.
 */
HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE_AUTO, NULL);
}

/**
 * hinoko_fw_iso_resource_auto_allocate_async:
 * @self: A #HinokoFwIsoResourceAuto.
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A #GError. Error can be generated with domain of
 *	       #hinoko_fw_iso_resource_error_quark(), and #hinoko_fw_iso_resource_auto_error_quark().
 *
 * Initiate allocation of isochronous resource. When the allocation is done,
 * #HinokoFwIsoResource::allocated signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_auto_allocate_async(HinokoFwIsoResourceAuto *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **error)
{
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_return_if_fail(channel_candidates != NULL);
	g_return_if_fail(channel_candidates_count > 0);
	g_return_if_fail(bandwidth > 0);

	g_mutex_lock(&priv->mutex);

	if (priv->allocated) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED);
		goto end;
	}

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE, &res,
				     error);
	if (*error == NULL)
		priv->handle = res.handle;
end:
	g_mutex_unlock(&priv->mutex);
}

/**
 * hinoko_fw_iso_resource_auto_deallocate_async:
 * @self: A #HinokoFwIsoResourceAuto.
 * @error: A #GError. Error can be generated with domain of
 *	       #hinoko_fw_iso_resource_error_quark(), and #hinoko_fw_iso_resource_auto_error_quark().
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * #HinokoFwIsoResource::deallocated signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_auto_deallocate_async(HinokoFwIsoResourceAuto *self,
						  GError **error)
{
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_deallocate dealloc = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	if (!priv->allocated) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED);
		goto end;
	}

	dealloc.handle = priv->handle;

	hinoko_fw_iso_resource_ioctl(HINOKO_FW_ISO_RESOURCE(self),
				     FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE,
				     &dealloc, error);
end:
	g_mutex_unlock(&priv->mutex);
}

struct waiter {
	GMutex mutex;
	GCond cond;
	GError *error;
	gboolean handled;
};

static void handle_event_signal(HinokoFwIsoResourceAuto *self, guint channel,
				guint bandwidth, const GError *error,
				gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	g_mutex_lock(&w->mutex);
	if (error != NULL)
		w->error = g_error_copy(error);
	w->handled = TRUE;
	g_cond_signal(&w->cond);
	g_mutex_unlock(&w->mutex);
}

/**
 * hinoko_fw_iso_resource_auto_allocate_sync:
 * @self: A #HinokoFwIsoResourceAuto.
 * @channel_candidates: (array length=channel_candidates_count): The array with
 *			elements for numerical number for isochronous channel
 *			to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A #GError. Error can be generated with domain of
 *	       #hinoko_fw_iso_resource_error_quark(), and #hinoko_fw_iso_resource_auto_error_quark().
 *
 * Initiate allocation of isochronous resource and wait for #HinokoFwIsoResource::allocated signal.
 * When the call is successful, #HinokoFwIsoResourceAuto:channel and #HinokoFwIsoResourceAuto:bandwidth
 * properties are available.
 */
void hinoko_fw_iso_resource_auto_allocate_sync(HinokoFwIsoResourceAuto *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth,
					       GError **error)
{
	struct waiter w;
	guint64 expiration;
	gulong handler_id;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error != NULL && *error == NULL);

	g_mutex_init(&w.mutex);
	g_cond_init(&w.cond);
	w.error = NULL;
	w.handled = FALSE;

	// For safe, use 100 msec for timeout.
	expiration = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;

	handler_id = g_signal_connect(self, "allocated",
				      (GCallback)handle_event_signal, &w);

	hinoko_fw_iso_resource_auto_allocate_async(self, channel_candidates,
						   channel_candidates_count,
						   bandwidth, error);
	if (*error != NULL) {
		g_signal_handler_disconnect(self, handler_id);
		return;
	}

	g_mutex_lock(&w.mutex);
	while (w.handled == FALSE) {
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_mutex_unlock(&w.mutex);

	if (w.handled == FALSE)
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_TIMEOUT);
	else if (w.error != NULL)
		*error = w.error;	// Delegate ownership.
}

/**
 * hinoko_fw_iso_resource_auto_deallocate_sync:
 * @self: A #HinokoFwIsoResourceAuto.
 * @error: A #GError. Error can be generated with domain of
 *	       #hinoko_fw_iso_resource_error_quark(), and #hinoko_fw_iso_resource_auto_error_quark().
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * #HinokoFwIsoResource::deallocated signal is emit to notify the result, channel, and bandwidth.
 */
void hinoko_fw_iso_resource_auto_deallocate_sync(HinokoFwIsoResourceAuto *self,
						 GError **error)
{
	struct waiter w;
	guint64 expiration;
	gulong handler_id;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self));
	g_return_if_fail(error != NULL && *error == NULL);

	g_mutex_init(&w.mutex);
	g_cond_init(&w.cond);
	w.error = NULL;
	w.handled = FALSE;

	// For safe, use 100 msec for timeout.
	expiration = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;

	handler_id = g_signal_connect(self, "deallocated",
				      (GCallback)handle_event_signal, &w);

	hinoko_fw_iso_resource_auto_deallocate_async(self, error);
	if (*error != NULL) {
		g_signal_handler_disconnect(self, handler_id);
		return;
	}

	g_mutex_lock(&w.mutex);
	while (w.handled == FALSE) {
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_mutex_unlock(&w.mutex);

	if (w.handled == FALSE)
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_TIMEOUT);
	else if (w.error != NULL)
		*error = w.error;	// Delegate ownership.
}

void hinoko_fw_iso_resource_auto_handle_event(HinokoFwIsoResourceAuto *self,
					struct fw_cdev_event_iso_resource *ev)
{
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	if (ev->type == FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED) {
		if (ev->channel >= 0) {
			g_mutex_lock(&priv->mutex);
			priv->channel = ev->channel;
			priv->bandwidth = ev->bandwidth;
			priv->allocated = TRUE;
			g_mutex_unlock(&priv->mutex);
		}
	} else {
		if (ev->channel >= 0) {
			g_mutex_lock(&priv->mutex);
			priv->channel = 0;
			priv->bandwidth -= ev->bandwidth;
			priv->allocated = FALSE;
			g_mutex_unlock(&priv->mutex);
		}
	}
}
