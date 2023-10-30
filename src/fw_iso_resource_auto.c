// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_private.h"

/**
 * HinokoFwIsoResourceAuto:
 * An object to maintain allocated isochronous resource.
 *
 * A [class@FwIsoResourceAuto]is an object to maintain isochronous resource during the lifetime of
 * the object. The allocated isochronous resource is kept even if the generation of the bus
 * updates. The maintenance of allocated isochronous resource is done by Linux FireWire subsystem.
 */
typedef struct {
	struct fw_iso_resource_state state;

	gboolean is_allocated;
	guint channel;
	guint bandwidth;

	GMutex mutex;
	guint handle;
} HinokoFwIsoResourceAutoPrivate;

static void fw_iso_resource_iface_init(HinokoFwIsoResourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoResourceAuto, hinoko_fw_iso_resource_auto, G_TYPE_OBJECT,
			G_ADD_PRIVATE(HinokoFwIsoResourceAuto)
                        G_IMPLEMENT_INTERFACE(HINOKO_TYPE_FW_ISO_RESOURCE, fw_iso_resource_iface_init))

/**
 * hinoko_fw_iso_resource_auto_error_quark:
 *
 * Return the [alias@GLib.Quark] for error domain of [struct@GLib.Error] which has code in
 * [error@FwIsoResourceAutoError].
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinoko-fw-iso-resource-auto-error-quark, hinoko_fw_iso_resource_auto_error)

/**
 * fw_iso_resource_auto_error_to_label:
 * @code: One of [error@FwIsoResourceAutoError].
 * @label: (out) (transfer none): The label of error code.
 *
 * Retrieve the label of error code.
 */
static void fw_iso_resource_auto_error_to_label(HinokoFwIsoResourceAutoError code,
						const char **label)
{
	static const char *const labels[] = {
		[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_FAILED] = "The system call fails",
		[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED] =
			"The instance is already associated to allocated isochronous resources",
		[HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED] =
			"The instance is not associated to allocated isochronous resources",
	};

	switch (code) {
	case HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_FAILED:
	case HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED:
	case HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED:
		break;
	default:
		code = HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_FAILED;
		break;
	}

	*label = labels[code];
}

static inline void generate_local_error(GError **error, HinokoFwIsoResourceAutoError code)
{
	const char *label;

	fw_iso_resource_auto_error_to_label(code, &label);
	g_set_error_literal(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR, code, label);
}

enum fw_iso_resource_auto_prop_type {
	FW_ISO_RESOURCE_AUTO_PROP_IS_ALLOCATED = FW_ISO_RESOURCE_PROP_TYPE_COUNT,
	FW_ISO_RESOURCE_AUTO_PROP_CHANNEL,
	FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH,
	FW_ISO_RESOURCE_AUTO_PROP_COUNT,
};

static void fw_iso_resource_auto_get_property(GObject *obj, guint id,
					      GValue *val, GParamSpec *spec)
{
	HinokoFwIsoResourceAuto *self = HINOKO_FW_ISO_RESOURCE_AUTO(obj);
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	switch (id) {
	case FW_ISO_RESOURCE_AUTO_PROP_IS_ALLOCATED:
		g_value_set_boolean(val, priv->is_allocated);
		break;
	case FW_ISO_RESOURCE_AUTO_PROP_CHANNEL:
		g_value_set_uint(val, priv->channel);
		break;
	case FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH:
		g_value_set_uint(val, priv->bandwidth);
		break;
	default:
		fw_iso_resource_state_get_property(&priv->state, obj, id, val, spec);
		break;
	}

	g_mutex_unlock(&priv->mutex);
}

static void fw_iso_resource_auto_finalize(GObject *obj)
{
	HinokoFwIsoResourceAuto *self = HINOKO_FW_ISO_RESOURCE_AUTO(obj);
	HinokoFwIsoResourceAutoPrivate *priv =
		hinoko_fw_iso_resource_auto_get_instance_private(self);

	fw_iso_resource_state_release(&priv->state);

	G_OBJECT_CLASS(hinoko_fw_iso_resource_auto_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_resource_auto_class_init(HinokoFwIsoResourceAutoClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_resource_auto_get_property;
	gobject_class->finalize = fw_iso_resource_auto_finalize;

	fw_iso_resource_class_override_properties(gobject_class);

	/**
	 * HinokoFwIsoResourceAuto:is-allocated:
	 *
	 * Whether to be allocate isochronous resource or not.
	 *
	 * Since: 0.7
	 */
	g_object_class_install_property(gobject_class, FW_ISO_RESOURCE_AUTO_PROP_IS_ALLOCATED,
		g_param_spec_boolean("is-allocated", "is-allocated",
				     "Whether to allocate or not.",
				     FALSE, G_PARAM_READABLE));

	/**
	 * HinokoFwIsoResourceAuto:channel:
	 *
	 * The allocated channel number.
	 *
	 * Since: 0.7
	 */
	g_object_class_install_property(gobject_class, FW_ISO_RESOURCE_AUTO_PROP_CHANNEL,
		g_param_spec_uint("channel", "channel",
				  "The allocated channel number.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));

	/**
	 * HinokoFwIsoResourceAuto:bandwidth:
	 *
	 * The allocated amount of bandwidth.
	 *
	 * Since: 0.7
	 */
	g_object_class_install_property(gobject_class, FW_ISO_RESOURCE_AUTO_PROP_BANDWIDTH,
		g_param_spec_uint("bandwidth", "bandwidth",
				  "The allocated amount of bandwidth.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));
}

static void hinoko_fw_iso_resource_auto_init(HinokoFwIsoResourceAuto *self)
{
	HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

	fw_iso_resource_state_init(&priv->state);
	g_mutex_init(&priv->mutex);
}

static gboolean fw_iso_resource_auto_open(HinokoFwIsoResource *inst, const gchar *path,
					  gint open_flag, GError **error)
{
	HinokoFwIsoResourceAuto *self;
	HinokoFwIsoResourceAutoPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(inst), FALSE);
	self = HINOKO_FW_ISO_RESOURCE_AUTO(inst);
	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	return fw_iso_resource_state_open(&priv->state, path, open_flag, error);
}

static gboolean fw_iso_resource_auto_allocate(HinokoFwIsoResource *inst,
					      const guint8 *channel_candidates,
					      gsize channel_candidates_count, guint bandwidth,
					      GError **error)
{
	HinokoFwIsoResourceAuto *self;
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_allocate_iso_resource res = {0};
	gboolean result;
	int i;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(inst), FALSE);
	g_return_val_if_fail(channel_candidates != NULL, FALSE);
	g_return_val_if_fail(channel_candidates_count > 0, FALSE);
	g_return_val_if_fail(bandwidth > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	self = HINOKO_FW_ISO_RESOURCE_AUTO(inst);
	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);
	if (priv->state.fd < 0) {
		generate_fw_iso_resource_error_coded(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return FALSE;
	}

	g_mutex_lock(&priv->mutex);

	if (priv->is_allocated) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED);
		result = FALSE;
		goto end;
	}

	for (i = 0; i < channel_candidates_count; ++i) {
		if (channel_candidates[i] < 64)
			res.channels |= 1ull << channel_candidates[i];
	}
	res.bandwidth = bandwidth;

	if (ioctl(priv->state.fd, FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE, &res) < 0) {
		generate_fw_iso_resource_error_ioctl(error, errno,
						     FW_CDEV_IOC_ALLOCATE_ISO_RESOURCE);
		result = FALSE;
	} else {
		priv->handle = res.handle;
		result = TRUE;
	}
end:
	g_mutex_unlock(&priv->mutex);

	return result;
}

static void handle_iso_resource_event(HinokoFwIsoResourceAuto *self,
				      const struct fw_cdev_event_iso_resource *ev)
{
	const char *signal_name;
	guint channel;
	guint bandwidth;
	GError *error;

	parse_iso_resource_event(ev, &channel, &bandwidth, &signal_name, &error);

	if (error == NULL) {
		HinokoFwIsoResourceAutoPrivate *priv =
			hinoko_fw_iso_resource_auto_get_instance_private(self);

		g_mutex_lock(&priv->mutex);

		switch (ev->type) {
		case FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED:
			priv->channel = channel;
			priv->bandwidth = bandwidth;
			priv->is_allocated = TRUE;
			break;
		case FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED:
			priv->channel = 0;
			priv->bandwidth -= bandwidth;
			priv->is_allocated = FALSE;
			break;
		default:
			break;
		}

		g_mutex_unlock(&priv->mutex);
	}

	g_signal_emit_by_name(self, signal_name, channel, bandwidth, error);

	if (error != NULL)
		g_clear_error(&error);
}

static void handle_bus_reset_event(HinokoFwIsoResourceAuto *self,
				   const struct fw_cdev_event_bus_reset *ev)
{
	HinokoFwIsoResourceAutoPrivate *priv =
		hinoko_fw_iso_resource_auto_get_instance_private(self);
	gboolean need_notify = (priv->state.bus_state.generation != ev->generation);

	memcpy(&priv->state.bus_state, ev, sizeof(*ev));

	if (need_notify)
		g_object_notify(G_OBJECT(self), GENERATION_PROP_NAME);
}

void fw_iso_resource_auto_handle_event(HinokoFwIsoResource *inst, const union fw_cdev_event *event)
{
	HinokoFwIsoResourceAuto *self;

	g_return_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(inst));
	self = HINOKO_FW_ISO_RESOURCE_AUTO(inst);

	switch (event->common.type) {
	case FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED:
	case FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED:
		handle_iso_resource_event(self, &event->iso_resource);
		break;
	case FW_CDEV_EVENT_BUS_RESET:
		handle_bus_reset_event(self, &event->bus_reset);
		break;
	default:
		break;
	}
}

static gboolean fw_iso_resource_auto_create_source(HinokoFwIsoResource *inst, GSource **source,
						   GError **error)
{
	HinokoFwIsoResourceAuto *self;
	HinokoFwIsoResourceAutoPrivate *priv;
	guint generation;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(inst), FALSE);
	self = HINOKO_FW_ISO_RESOURCE_AUTO(inst);
	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);

	generation = priv->state.bus_state.generation;

	if (!fw_iso_resource_state_create_source(&priv->state, inst,
						 fw_iso_resource_auto_handle_event, source, error))
		return FALSE;

	if (generation != priv->state.bus_state.generation)
		g_object_notify(G_OBJECT(self), GENERATION_PROP_NAME);

	return TRUE;
}

static void fw_iso_resource_iface_init(HinokoFwIsoResourceInterface *iface)
{
	iface->open = fw_iso_resource_auto_open;
	iface->allocate = fw_iso_resource_auto_allocate;
	iface->create_source = fw_iso_resource_auto_create_source;

}

/**
 * hinoko_fw_iso_resource_auto_new:
 *
 * Allocate and return an instance of [class@FwIsoResourceAuto]object.
 *
 * Returns: A [class@FwIsoResourceAuto]
 */
HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new()
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RESOURCE_AUTO, NULL);
}

/**
 * hinoko_fw_iso_resource_auto_deallocate:
 * @self: A [class@FwIsoResourceAuto]
 * @error: A [struct@GLib.Error]. Error can be generated with domains of [error@FwIsoResourceError],
 *	   and [error@FwIsoResourceAutoError].
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * [signal@FwIsoResource::deallocated] signal is emit to notify the result, channel, and bandwidth.
 *
 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
 *
 * Since: 1.0
 */
gboolean hinoko_fw_iso_resource_auto_deallocate(HinokoFwIsoResourceAuto *self, GError **error)
{
	HinokoFwIsoResourceAutoPrivate *priv;
	struct fw_cdev_deallocate dealloc = {0};
	gboolean result;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinoko_fw_iso_resource_auto_get_instance_private(self);
	if (priv->state.fd < 0) {
		generate_fw_iso_resource_error_coded(error, HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED);
		return FALSE;
	}

	g_mutex_lock(&priv->mutex);

	if (!priv->is_allocated) {
		generate_local_error(error, HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED);
		result = FALSE;
		goto end;
	}

	dealloc.handle = priv->handle;

	if (ioctl(priv->state.fd, FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE, &dealloc) < 0) {
		generate_fw_iso_resource_error_ioctl(error, errno,
						     FW_CDEV_IOC_DEALLOCATE_ISO_RESOURCE);
		result = FALSE;
	} else {
		result = TRUE;
	}
end:
	g_mutex_unlock(&priv->mutex);

	return result;
}

/**
 * hinoko_fw_iso_resource_auto_deallocate_wait:
 * @self: A [class@FwIsoResourceAuto]
 * @timeout_ms: The timeout to wait for allocated event by milli second unit.
 * @error: A [struct@GLib.Error]. Error can be generated with domains of [error@FwIsoResourceError],
 *	   and [error@FwIsoResourceAutoError].
 *
 * Initiate deallocation of isochronous resource. When the deallocation is done,
 * [signal@FwIsoResource::deallocated] signal is emit to notify the result, channel, and bandwidth.
 *
 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
 *
 * Since: 1.0
 */
gboolean hinoko_fw_iso_resource_auto_deallocate_wait(HinokoFwIsoResourceAuto *self,
						     guint timeout_ms, GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE_AUTO(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	fw_iso_resource_waiter_init(&w, HINOKO_FW_ISO_RESOURCE(self), DEALLOCATED_SIGNAL_NAME,
				    timeout_ms);

	(void)hinoko_fw_iso_resource_auto_deallocate(self, error);

	return fw_iso_resource_waiter_wait(&w, HINOKO_FW_ISO_RESOURCE(self), error);
}
