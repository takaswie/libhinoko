// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RX_SINGLE_H__
#define __HINOKO_FW_ISO_RX_SINGLE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RX_SINGLE	(hinoko_fw_iso_rx_single_get_type())

#define HINOKO_FW_ISO_RX_SINGLE(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),				\
				    HINOKO_TYPE_FW_ISO_RX_SINGLE,	\
				    HinokoFwIsoRxSingle))
#define HINOKO_IS_FW_ISO_RX_SINGLE(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),				\
				    HINOKO_TYPE_FW_ISO_RX_SINGLE))

#define HINOKO_FW_ISO_RX_SINGLE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),				\
				 HINOKO_TYPE_FW_ISO_RX_SINGLE,		\
				 HinokoFwIsoRxSingleClass))
#define HINOKO_IS_FW_ISO_RX_SINGLE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),				\
				 HINOKO_TYPE_FW_ISO_RX_SINGLE))
#define HINOKO_FW_ISO_RX_SINGLE_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),				\
				   HINOKO_TYPE_FW_ISO_RX_SINGLE,	\
				   HinokoFwIsoRxSingleClass))

typedef struct _HinokoFwIsoRxSingle		HinokoFwIsoRxSingle;
typedef struct _HinokoFwIsoRxSingleClass	HinokoFwIsoRxSingleClass;
typedef struct _HinokoFwIsoRxSinglePrivate	HinokoFwIsoRxSinglePrivate;

struct _HinokoFwIsoRxSingle {
	HinokoFwIsoCtx parent_instance;

	HinokoFwIsoRxSinglePrivate *priv;
};

struct _HinokoFwIsoRxSingleClass {
	HinokoFwIsoCtxClass parent_class;

	/**
	 * HinokoFwIsoRxSingleClass::interrupted:
	 * @self: A #HinokoFwIsoRxSingle.
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @header: (array length=header_length) (element-type guint8): The
	 * 	    headers of IR context for handled packets.
	 * @header_length: the number of bytes for @header.
	 * @count: the number of packets to handle.
	 *
	 * In detail, please refer to documentation about #HinokoFwIsoRxSingle::interrupted.
	 */
	void (*interrupted)(HinokoFwIsoRxSingle *self, guint sec, guint cycle,
			    const guint8 *header, guint header_length,
			    guint count);
};

GType hinoko_fw_iso_rx_single_get_type(void) G_GNUC_CONST;

HinokoFwIsoRxSingle *hinoko_fw_iso_rx_single_new(void);

void hinoko_fw_iso_rx_single_allocate(HinokoFwIsoRxSingle *self,
				      const char *path,
				      guint channel, guint header_size,
				      GError **exception);
void hinoko_fw_iso_rx_single_release(HinokoFwIsoRxSingle *self);

void hinoko_fw_iso_rx_single_map_buffer(HinokoFwIsoRxSingle *self,
					guint maximum_bytes_per_payload,
					guint payloads_per_buffer,
					GError **exception);
void hinoko_fw_iso_rx_single_unmap_buffer(HinokoFwIsoRxSingle *self);

void hinoko_fw_iso_rx_single_register_packet(HinokoFwIsoRxSingle *self, gboolean schedule_interrupt,
					     GError **exception);

void hinoko_fw_iso_rx_single_start(HinokoFwIsoRxSingle *self, const guint16 *cycle_match,
				   guint32 sync, HinokoFwIsoCtxMatchFlag tags, GError **exception);
void hinoko_fw_iso_rx_single_stop(HinokoFwIsoRxSingle *self);

void hinoko_fw_iso_rx_single_get_payload(HinokoFwIsoRxSingle *self, guint index,
					 const guint8 **payload, guint *length,
					 GError **exception);

G_END_DECLS

#endif
