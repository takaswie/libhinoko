// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RX_MULTIPLE_H__
#define __HINOKO_FW_ISO_RX_MULTIPLE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RX_MULTIPLE	(hinoko_fw_iso_rx_multiple_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoRxMultiple, hinoko_fw_iso_rx_multiple, HINOKO,
			 FW_ISO_RX_MULTIPLE, HinokoFwIsoCtx);

struct _HinokoFwIsoRxMultipleClass {
	HinokoFwIsoCtxClass parent_class;

	/**
	 * HinokoFwIsoRxMultipleClass::interrupted:
	 * @self: A #HinokoFwIsoRxMultiple.
	 * @count: The number of packets available in this interrupt.
	 *
	 * In detail, please refer to documentation about #HinokoFwIsoRxMultiple::interrupted.
	 */
	void (*interrupted)(HinokoFwIsoRxMultiple *self, guint count);
};

HinokoFwIsoRxMultiple *hinoko_fw_iso_rx_multiple_new(void);

void hinoko_fw_iso_rx_multiple_allocate(HinokoFwIsoRxMultiple *self,
					const char *path,
					const guint8 *channels,
					guint channels_length,
					GError **exception);
void hinoko_fw_iso_rx_multiple_release(HinokoFwIsoRxMultiple *self);

void hinoko_fw_iso_rx_multiple_map_buffer(HinokoFwIsoRxMultiple *self,
					  guint bytes_per_chunk,
					  guint chunks_per_buffer,
					  GError **exception);
void hinoko_fw_iso_rx_multiple_unmap_buffer(HinokoFwIsoRxMultiple *self);

void hinoko_fw_iso_rx_multiple_start(HinokoFwIsoRxMultiple *self,
				     const guint16 *cycle_match, guint32 sync,
				     HinokoFwIsoCtxMatchFlag tags,
				     guint chunks_per_irq, GError **exception);
void hinoko_fw_iso_rx_multiple_stop(HinokoFwIsoRxMultiple *self);

void hinoko_fw_iso_rx_multiple_get_payload(HinokoFwIsoRxMultiple *self,
					guint index, const guint8 **payload,
					guint *length, GError **exception);

G_END_DECLS

#endif
