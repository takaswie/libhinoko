// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINOKO_FW_ISO_RESOURCE_H__
#define __ORG_KERNEL_HINOKO_FW_ISO_RESOURCE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE	(hinoko_fw_iso_resource_get_type())

G_DECLARE_INTERFACE(HinokoFwIsoResource, hinoko_fw_iso_resource, HINOKO, FW_ISO_RESOURCE, GObject);

#define HINOKO_FW_ISO_RESOURCE_ERROR		hinoko_fw_iso_resource_error_quark()

GQuark hinoko_fw_iso_resource_error_quark();

struct _HinokoFwIsoResourceInterface {
	GTypeInterface parent_iface;

	/**
	 * HinokoFwIsoResourceInterface::open:
	 * @self: A [iface@FwIsoResource].
	 * @path: A path of any Linux FireWire character device.
	 * @open_flag: The flag of open(2) system call. O_RDONLY is forced to fulfil internally.
	 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.FileError
	 *	   and Hinoko.FwIsoResourceError.
	 *
	 * Virtual function to open Linux FireWire character device to delegate any request for
	 * isochronous resource management to Linux FireWire subsystem.
	 *
	 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
	 *
	 * Since: 0.7
	 */
	gboolean (*open)(HinokoFwIsoResource *self, const gchar *path, gint open_flag,
			 GError **error);

	/**
	 * HinokoFwIsoResourceInterface::allocate_async:
	 * @self: A [iface@FwIsoResource].
	 * @channel_candidates: (array length=channel_candidates_count): The array with elements for
	 *			numeric number of isochronous channel to be allocated.
	 * @channel_candidates_count: The number of channel candidates.
	 * @bandwidth: The amount of bandwidth to be allocated.
	 * @error: A [struct@GLib.Error].
	 *
	 * Virtual function to create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch
	 * events for isochronous resource.
	 *
	 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
	 *
	 * Since: 0.7
	 */
	gboolean (*allocate_async)(HinokoFwIsoResource *self, const guint8 *channel_candidates,
				   gsize channel_candidates_count, guint bandwidth, GError **error);

	/**
	 * HinokoFwIsoResourceInterface::create_source:
	 * @self: A [iface@FwIsoResource].
	 * @source: (out): A [struct@GLib.Source]
	 * @error: A [struct@GLib.Error].
	 *
	 * Virtual function to create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch
	 * events for isochronous resource.
	 *
	 * Returns: TRUE if the overall operation finished successfully, otherwise FALSE.
	 *
	 * Since: 0.7
	 */
	gboolean (*create_source)(HinokoFwIsoResource *self, GSource **source, GError **error);

	/**
	 * HinokoFwIsoResourceInterface::allocated:
	 * @self: A [iface@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error]. Error can be generated
	 *	   with domain of Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Closure for the [signal@FwIsoResource::allocated] signal.
	 *
	 * Since: 0.7
	 */
	void (*allocated)(HinokoFwIsoResource *self, guint channel,
			  guint bandwidth, const GError *error);

	/**
	 * HinokoFwIsoResourceInterface::deallocated:
	 * @self: A [iface@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error]. Error can be generated
	 *	   with domain of Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Closure for the [signal@FwIsoResource::deallocated] signal.
	 *
	 * Since: 0.7
	 */
	void (*deallocated)(HinokoFwIsoResource *self, guint channel,
			    guint bandwidth, const GError *error);
};

gboolean hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path, gint open_flag,
				     GError **error);

gboolean hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self, GSource **source,
					      GError **error);

gboolean hinoko_fw_iso_resource_allocate_async(HinokoFwIsoResource *self,
					       const guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth, GError **error);

gboolean hinoko_fw_iso_resource_allocate_sync(HinokoFwIsoResource *self,
					      const guint8 *channel_candidates,
				              gsize channel_candidates_count, guint bandwidth,
					      guint timeout_ms, GError **error);

guint hinoko_fw_iso_resource_calculate_bandwidth(guint bytes_per_payload,
						 HinokoFwScode scode);

G_END_DECLS

#endif
