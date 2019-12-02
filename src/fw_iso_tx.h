// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_TX_H__
#define __HINOKO_FW_ISO_TX_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_iso_ctx.h>
#include <hinoko_enums.h>
#include <hinoko_sigs_marshal.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_TX	(hinoko_fw_iso_tx_get_type())

#define HINOKO_FW_ISO_TX(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINOKO_TYPE_FW_ISO_TX,	\
				    HinokoFwIsoTx))
#define HINOKO_IS_FW_ISO_TX(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINOKO_TYPE_FW_ISO_TX))

#define HINOKO_FW_ISO_TX_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINOKO_TYPE_FW_ISO_TX,		\
				 HinokoFwIsoTxClass))
#define HINOKO_IS_FW_ISO_TX_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINOKO_TYPE_FW_ISO_TX))
#define HINOKO_FW_ISO_TX_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINOKO_TYPE_FW_ISO_TX,	\
				   HinokoFwIsoTxClass))

typedef struct _HinokoFwIsoTx		HinokoFwIsoTx;
typedef struct _HinokoFwIsoTxClass	HinokoFwIsoTxClass;
typedef struct _HinokoFwIsoTxPrivate	HinokoFwIsoTxPrivate;

struct _HinokoFwIsoTx {
	HinokoFwIsoCtx parent_instance;

	HinokoFwIsoTxPrivate *priv;
};

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
	 * When registered packets are handled, #HinokoFwIsoTxClass::interrupted
	 * handler is called with timestamps of the packet.
	 */
	void (*interrupted)(HinokoFwIsoTx *self, guint sec, guint cycle,
			    const guint8 *tstamp, guint tstamp_length,
			    guint count);
};

GType hinoko_fw_iso_tx_get_type(void) G_GNUC_CONST;

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

void hinoko_fw_iso_tx_start(HinokoFwIsoTx *self, const guint16 *cycle_match,
			    guint packets_per_irq, GError **exception);
void hinoko_fw_iso_tx_stop(HinokoFwIsoTx *self);

void hinoko_fw_iso_tx_register_packet(HinokoFwIsoTx *self,
				HinokoFwIsoCtxMatchFlag tags, guint sy,
				const guint8 *header, guint header_length,
				const guint8 *payload, guint payload_length,
				GError **exception);

G_END_DECLS

#endif
