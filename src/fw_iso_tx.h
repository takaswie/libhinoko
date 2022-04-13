// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_TX_H__
#define __HINOKO_FW_ISO_TX_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_TX	(hinoko_fw_iso_tx_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoTx, hinoko_fw_iso_tx, HINOKO, FW_ISO_TX, HinokoFwIsoCtx);

struct _HinokoFwIsoTxClass {
	HinokoFwIsoCtxClass parent_class;

	/**
	 * HinokoFwIsoTxClass::interrupted:
	 * @self: A #HinokoFwIsoTx.
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @tstamp: (array length=tstamp_length) (element-type guint8): A series
	 *	    of timestamps for packets already handled.
	 * @tstamp_length: the number of bytes for @tstamp.
	 * @count: the number of handled packets.
	 *
	 * In detail, please refer to documentation about #HinokoFwIsoTx::interrupted.
	 */
	void (*interrupted)(HinokoFwIsoTx *self, guint sec, guint cycle,
			    const guint8 *tstamp, guint tstamp_length,
			    guint count);
};

HinokoFwIsoTx *hinoko_fw_iso_tx_new(void);

void hinoko_fw_iso_tx_allocate(HinokoFwIsoTx *self, const char *path,
			       HinokoFwScode scode, guint channel,
			       guint header_size, GError **exception);
void hinoko_fw_iso_tx_release(HinokoFwIsoTx *self);

void hinoko_fw_iso_tx_map_buffer(HinokoFwIsoTx *self,
				 guint maximum_bytes_per_payload,
				 guint payloads_per_buffer,
				 GError **exception);
void hinoko_fw_iso_tx_unmap_buffer(HinokoFwIsoTx *self);

void hinoko_fw_iso_tx_start(HinokoFwIsoTx *self, const guint16 *cycle_match, GError **exception);
void hinoko_fw_iso_tx_stop(HinokoFwIsoTx *self);

void hinoko_fw_iso_tx_register_packet(HinokoFwIsoTx *self,
				HinokoFwIsoCtxMatchFlag tags, guint sy,
				const guint8 *header, guint header_length,
				const guint8 *payload, guint payload_length,
				gboolean schedule_interrupt, GError **exception);

G_END_DECLS

#endif
