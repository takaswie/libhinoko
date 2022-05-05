// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RX_SINGLE_H__
#define __HINOKO_FW_ISO_RX_SINGLE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RX_SINGLE	(hinoko_fw_iso_rx_single_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoRxSingle, hinoko_fw_iso_rx_single, HINOKO, FW_ISO_RX_SINGLE,
			 HinokoFwIsoCtx);

struct _HinokoFwIsoRxSingleClass {
	HinokoFwIsoCtxClass parent_class;

	/**
	 * HinokoFwIsoRxSingleClass::interrupted:
	 * @self: A [class@FwIsoRxSingle].
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @header: (array length=header_length) (element-type guint8): The headers of IR context
	 *	    for handled packets.
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

void hinoko_fw_iso_rx_single_allocate(HinokoFwIsoRxSingle *self,
				      const char *path,
				      guint channel, guint header_size,
				      GError **error);
void hinoko_fw_iso_rx_single_release(HinokoFwIsoRxSingle *self);

void hinoko_fw_iso_rx_single_map_buffer(HinokoFwIsoRxSingle *self,
					guint maximum_bytes_per_payload,
					guint payloads_per_buffer,
					GError **error);
void hinoko_fw_iso_rx_single_unmap_buffer(HinokoFwIsoRxSingle *self);

void hinoko_fw_iso_rx_single_register_packet(HinokoFwIsoRxSingle *self, gboolean schedule_interrupt,
					     GError **error);

void hinoko_fw_iso_rx_single_start(HinokoFwIsoRxSingle *self, const guint16 *cycle_match,
				   guint32 sync, HinokoFwIsoCtxMatchFlag tags, GError **error);

void hinoko_fw_iso_rx_single_get_payload(HinokoFwIsoRxSingle *self, guint index,
					 const guint8 **payload, guint *length,
					 GError **error);

G_END_DECLS

#endif
