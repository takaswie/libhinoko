// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RX_SINGLE_H__
#define __HINOKO_FW_ISO_RX_SINGLE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RX_SINGLE	(hinoko_fw_iso_rx_single_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoRxSingle, hinoko_fw_iso_rx_single, HINOKO, FW_ISO_RX_SINGLE,
			 GObject);

struct _HinokoFwIsoRxSingleClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoRxSingleClass::interrupted:
	 * @self: A [class@FwIsoRxSingle].
	 * @sec: The sec part of isochronous cycle when interrupt occurs, up to 7.
	 * @cycle: The cycle part of of isochronous cycle when interrupt occurs, up to 7999.
	 * @header: (array length=header_length) (element-type guint8): The headers of IR context
	 *	    for packets handled in the event of interrupt. The content is different
	 *	    depending on header_size parameter of [method@FwIsoRxSingle.allocate].
	 * @header_length: the number of bytes for header.
	 * @count: the number of packets to handle.
	 *
	 * Class closure for the [signal@FwIsoRxSingle::interrupted] signal.
	 */
	void (*interrupted)(HinokoFwIsoRxSingle *self, guint sec, guint cycle,
			    const guint8 *header, guint header_length,
			    guint count);
};

HinokoFwIsoRxSingle *hinoko_fw_iso_rx_single_new(void);

gboolean hinoko_fw_iso_rx_single_allocate(HinokoFwIsoRxSingle *self, const char *path,
					  guint channel, guint header_size, GError **error);

gboolean hinoko_fw_iso_rx_single_map_buffer(HinokoFwIsoRxSingle *self,
					    guint maximum_bytes_per_payload,
					    guint payloads_per_buffer, GError **error);

gboolean hinoko_fw_iso_rx_single_register_packet(HinokoFwIsoRxSingle *self,
						 gboolean schedule_interrupt, GError **error);

gboolean hinoko_fw_iso_rx_single_start(HinokoFwIsoRxSingle *self, const guint16 *cycle_match,
				       guint32 sync_code, HinokoFwIsoCtxMatchFlag tags,
				       GError **error);

void hinoko_fw_iso_rx_single_get_payload(HinokoFwIsoRxSingle *self, guint index,
					 const guint8 **payload, guint *length);

G_END_DECLS

#endif
