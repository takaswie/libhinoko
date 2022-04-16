// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_H__
#define __HINOKO_FW_ISO_RESOURCE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE	(hinoko_fw_iso_resource_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoResource, hinoko_fw_iso_resource, HINOKO, FW_ISO_RESOURCE,
			 GObject);

#define HINOKO_FW_ISO_RESOURCE_ERROR		hinoko_fw_iso_resource_error_quark()

GQuark hinoko_fw_iso_resource_error_quark();

struct _HinokoFwIsoResourceClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoResourceClass::allocated:
	 * @self: A [class@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: A [struct@GLib.Error]. Error can be generated with domain of
	 *	   Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Class closure for the [signal@FwIsoResource::allocated] signal.
	 */
	void (*allocated)(HinokoFwIsoResource *self, guint channel,
			  guint bandwidth, const GError *error);

	/**
	 * HinokoFwIsoResourceClass::deallocated:
	 * @self: A [class@FwIsoResource].
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @error: A [struct@GLib.Error]. Error can be generated with domain of
	 *	   Hinoko.FwIsoResourceError and its EVENT code.
	 *
	 * Class closure for the [signal@FwIsoResource::deallocated] signal.
	 */
	void (*deallocated)(HinokoFwIsoResource *self, guint channel,
			    guint bandwidth, const GError *error);
};

HinokoFwIsoResource *hinoko_fw_iso_resource_new();

void hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path,
				 gint open_flag, GError **error);

void hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self,
					  GSource **gsrc, GError **error);

guint hinoko_fw_iso_resource_calculate_bandwidth(guint bytes_per_payload,
						 HinokoFwScode scode);

void hinoko_fw_iso_resource_allocate_once_async(HinokoFwIsoResource *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **error);

void hinoko_fw_iso_resource_deallocate_once_async(HinokoFwIsoResource *self,
						  guint channel,
						  guint bandwidth,
						  GError **error);

void hinoko_fw_iso_resource_allocate_once_sync(HinokoFwIsoResource *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth,
					       GError **error);

void hinoko_fw_iso_resource_deallocate_once_sync(HinokoFwIsoResource *self,
						 guint channel,
						 guint bandwidth,
						 GError **error);

G_END_DECLS

#endif
