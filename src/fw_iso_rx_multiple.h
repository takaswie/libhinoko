// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RX_MULTIPLE_H__
#define __HINOKO_FW_ISO_RX_MULTIPLE_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_iso_ctx.h>
#include <hinoko_enums.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RX_MULTIPLE	(hinoko_fw_iso_rx_multiple_get_type())

#define HINOKO_FW_ISO_RX_MULTIPLE(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),				\
				    HINOKO_TYPE_FW_ISO_RX_MULTIPLE,	\
				    HinokoFwIsoRxMultiple))
#define HINOKO_IS_FW_ISO_RX_MULTIPLE(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),				\
				    HINOKO_TYPE_FW_ISO_RX_MULTIPLE))

#define HINOKO_FW_ISO_RX_MULTIPLE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),				\
				 HINOKO_TYPE_FW_ISO_RX_MULTIPLE,	\
				 HinokoFwIsoRxMultipleClass))
#define HINOKO_IS_FW_ISO_RX_MULTIPLE_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),				\
				 HINOKO_TYPE_FW_ISO_RX_MULTIPLE))
#define HINOKO_FW_ISO_RX_MULTIPLE_GET_CLASS(obj)			\
	(G_TYPE_INSTANCE_GET_CLASS((obj),				\
				   HINOKO_TYPE_FW_ISO_RX_MULTIPLE,	\
				   HinokoFwIsoRxMultipleClass))

typedef struct _HinokoFwIsoRxMultiple		HinokoFwIsoRxMultiple;
typedef struct _HinokoFwIsoRxMultipleClass	HinokoFwIsoRxMultipleClass;
typedef struct _HinokoFwIsoRxMultiplePrivate	HinokoFwIsoRxMultiplePrivate;

struct _HinokoFwIsoRxMultiple {
	HinokoFwIsoCtx parent_instance;

	HinokoFwIsoRxMultiplePrivate *priv;
};

struct _HinokoFwIsoRxMultipleClass {
	HinokoFwIsoCtxClass parent_class;

	/**
	 * HinokoFwIsoRxMultipleClass::interrupted:
	 * @self: A #HinokoFwIsoRxMultiple.
	 * @count: The number of packets available in this interrupt.
	 *
	 * When any packet is available, the handler of
	 * #HinokoFwIsoRxMultipleClass::interrupted is called with the number of
	 * available packets. In the handler, payload of received packet is
	 * available by a call of #hinoko_fw_iso_rx_multiple_get_payload().
	 */
	void (*interrupted)(HinokoFwIsoRxMultiple *self, guint count);
};

GType hinoko_fw_iso_rx_multiple_get_type(void) G_GNUC_CONST;

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
