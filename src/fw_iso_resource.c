// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_resource_private.h"

/**
 * HinokoFwIsoResource:
 * An interface object to listen events of isochronous resource allocation and deallocation.
 *
 * The [iface@FwIsoResource] should be implemented in GObject-derived object to listen events of
 * isochronous resource allocation and deallocation.
 *
 * Since: 0.7
 */

G_DEFINE_INTERFACE(HinokoFwIsoResource, hinoko_fw_iso_resource, G_TYPE_OBJECT)

/**
 * hinoko_fw_iso_resource_error_quark:
 *
 * Return the [alias@GLib.Quark] for [struct@GLib.Error] with which has code in
 * [error@FwIsoResourceError].
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinoko-fw-iso-resource-error-quark, hinoko_fw_iso_resource_error)

/**
 * hinoko_fw_iso_resource_error_to_label:
 * @code: One of [error@FwIsoResourceError].
 * @label: (out) (transfer none): The label of error code.
 *
 * Retrieve the label of error code.
 */
void hinoko_fw_iso_resource_error_to_label(HinokoFwIsoResourceError code, const char **label)
{
	const char *const labels[] = {
		[HINOKO_FW_ISO_RESOURCE_ERROR_FAILED] = "The system call fails",
		[HINOKO_FW_ISO_RESOURCE_ERROR_OPENED] =
			"The instance is already associated to any firewire character device",
		[HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED] =
			"The instance is not associated to any firewire character device",
		[HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT] =
			"No event to the request arrives within timeout.",
		[HINOKO_FW_ISO_RESOURCE_ERROR_EVENT] =
			"Event for the request arrives but includes error code",
	};

	switch (code) {
	case HINOKO_FW_ISO_RESOURCE_ERROR_FAILED:
	case HINOKO_FW_ISO_RESOURCE_ERROR_OPENED:
	case HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED:
	case HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT:
	case HINOKO_FW_ISO_RESOURCE_ERROR_EVENT:
		break;
	default:
		code = HINOKO_FW_ISO_RESOURCE_ERROR_FAILED;
		break;
	}

	*label = labels[code];
}

static void hinoko_fw_iso_resource_default_init(HinokoFwIsoResourceInterface *iface)
{
	/**
	 * HinokoFwIsoResource::generation:
	 *
	 * The numeric value of current generation for bus topology.
	 *
	 * Since: 0.7
	 */
	g_object_interface_install_property(iface,
		g_param_spec_uint(GENERATION_PROP_NAME, "generation",
				  "The numeric value of current generation for bus topology",
				  0, G_MAXUINT,
				  0,
				  G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));

	/**
	 * HinokoFwIsoResource::allocated:
	 * @self: A [iface@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error]. Error can be generated
	 *	   with domain of [error@FwIsoResourceError] and its EVENT code.
	 *
	 * Emitted when allocation of isochronous resource finishes.
	 *
	 * Since: 0.7
	 */
	g_signal_new(ALLOCATED_SIGNAL_NAME,
		     G_TYPE_FROM_INTERFACE(iface),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		     G_STRUCT_OFFSET(HinokoFwIsoResourceInterface, allocated),
		     NULL, NULL,
		     hinoko_sigs_marshal_VOID__UINT_UINT_BOXED,
		     G_TYPE_NONE,
		     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_ERROR);

	/**
	 * HinokoFwIsoResource::deallocated:
	 * @self: A [iface@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error]. Error can be generated
	 *	   with domain of [error@FwIsoResourceError] and its EVENT code.
	 *
	 * Emitted when deallocation of isochronous resource finishes.
	 *
	 * Since: 0.7
	 */
	g_signal_new(DEALLOCATED_SIGNAL_NAME,
		     G_TYPE_FROM_INTERFACE(iface),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		     G_STRUCT_OFFSET(HinokoFwIsoResourceInterface, deallocated),
		     NULL, NULL,
		     hinoko_sigs_marshal_VOID__UINT_UINT_BOXED,
		     G_TYPE_NONE,
		     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_ERROR);
}

/**
 * hinoko_fw_iso_resource_open:
 * @self: A [iface@FwIsoResource].
 * @path: A path of any Linux FireWire character device.
 * @open_flag: The flag of open(2) system call. O_RDONLY is forced to fulfil
 *	       internally.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; [error@GLib.FileError]
 *	   and [error@FwIsoResourceError].
 *
 * Open Linux FireWire character device to delegate any request for isochronous
 * resource management to Linux FireWire subsystem.
 *
 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path, gint open_flag,
				     GError **error)
{
	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self), FALSE);
	g_return_val_if_fail(path != NULL && strlen(path) > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	return HINOKO_FW_ISO_RESOURCE_GET_IFACE(self)->open(self, path, open_flag, error);
}

/**
 * hinoko_fw_iso_resource_create_source:
 * @self: A [iface@FwIsoResource].
 * @source: (out): A [struct@GLib.Source]
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * resource.
 *
 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self, GSource **source,
					      GError **error)
{
	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self), FALSE);
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	return HINOKO_FW_ISO_RESOURCE_GET_IFACE(self)->create_source(self, source, error);
}

/**
 * hinoko_fw_iso_resource_allocate:
 * @self: A [iface@FwIsoResource].
 * @channel_candidates: (array length=channel_candidates_count): The array with elements for
 *			numeric number of isochronous channel to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of [error@FwIsoResourceError]
 *	   as well as domain depending on each implementation.
 *
 * Initiate allocation of isochronous resource without any wait. One of the candidates is actually
 * allocated for channel. When the allocation finishes, [signal@FwIsoResource::allocated] signal is
 * emitted to notify the result, channel, and bandwidth.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 1.0
 */
gboolean hinoko_fw_iso_resource_allocate(HinokoFwIsoResource *self,
					 const guint8 *channel_candidates,
					 gsize channel_candidates_count, guint bandwidth,
					 GError **error)
{
	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self), FALSE);
	g_return_val_if_fail(channel_candidates != NULL, FALSE);
	g_return_val_if_fail(channel_candidates_count > 0, FALSE);
	g_return_val_if_fail(bandwidth > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	return HINOKO_FW_ISO_RESOURCE_GET_IFACE(self)->allocate(self, channel_candidates,
								channel_candidates_count,
								bandwidth, error);
}

/**
 * hinoko_fw_iso_resource_allocate_wait:
 * @self: A [iface@FwIsoResource].
 * @channel_candidates: (array length=channel_candidates_count): The array with elements for
 *			numeric number for isochronous channel to be allocated.
 * @channel_candidates_count: The number of channel candidates.
 * @bandwidth: The amount of bandwidth to be allocated.
 * @timeout_ms: The timeout to wait for allocated event.
 * @error: A [struct@GLib.Error]. Error can be generated with domain of [error@FwIsoResourceError]
 *	   as well as domain depending on each implementation.
 *
 * Initiate allocation of isochronous resource and wait for [signal@FwIsoResource::allocated]
 * signal. One of the candidates is actually allocated for channel.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 1.0
 */
gboolean hinoko_fw_iso_resource_allocate_wait(HinokoFwIsoResource *self,
					      const guint8 *channel_candidates,
				              gsize channel_candidates_count, guint bandwidth,
					      guint timeout_ms, GError **error)
{
	struct fw_iso_resource_waiter w;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RESOURCE(self), FALSE);
	g_return_val_if_fail(channel_candidates != NULL, FALSE);
	g_return_val_if_fail(channel_candidates_count > 0, FALSE);
	g_return_val_if_fail(bandwidth > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	fw_iso_resource_waiter_init(&w, self, ALLOCATED_SIGNAL_NAME, timeout_ms);

	(void)hinoko_fw_iso_resource_allocate(self, channel_candidates, channel_candidates_count,
					      bandwidth, error);

	return fw_iso_resource_waiter_wait(&w, self, error);
}

/**
 * hinoko_fw_iso_resource_calculate_bandwidth:
 * @bytes_per_payload: The number of bytes in payload of isochronous packet.
 * @scode: The speed of transmission.
 *
 * Calculate the amount of bandwidth expected to be consumed in allocation unit
 * by given parameters.
 *
 * Returns: The amount of bandwidth expected to be consumed.
 */
guint hinoko_fw_iso_resource_calculate_bandwidth(guint bytes_per_payload,
						 HinokoFwScode scode)
{
	guint bytes_per_packet;
	guint s400_bytes;

	// iso packets have three header quadlets and quadlet-aligned payload.
	bytes_per_packet = 3 * 4 + ((bytes_per_payload + 3) / 4) * 4;

	// convert to bandwidth units (quadlets at S1600 = bytes at S400).
	if (scode <= HINOKO_FW_SCODE_S400) {
		s400_bytes = bytes_per_packet *
			     (1 << (HINOKO_FW_SCODE_S400 - scode));
	} else {
		s400_bytes = bytes_per_packet /
			     (1 << (scode - HINOKO_FW_SCODE_S400));
	}

	return s400_bytes;
}
