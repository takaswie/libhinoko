// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <unistd.h>
#include <errno.h>

/**
 * SECTION:fw_iso_tx
 * @Title: HinokoFwIsoTx
 * @Short_description: An object to transmit isochronous packet for single
 *		       channel.
 * @include: fw_iso_tx.h
 *
 * A #HinokoFwIsoTx transmits isochronous packets for single channel by IT
 * context in 1394 OHCI. The content of packet is split to two parts; context
 * header and context payload in a manner of Linux FireWire subsystem.
 */
typedef struct {
	guint offset;
} HinokoFwIsoTxPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoTx, hinoko_fw_iso_tx, HINOKO_TYPE_FW_ISO_CTX)

static void fw_iso_tx_finalize(GObject *obj)
{
	HinokoFwIsoTx *self = HINOKO_FW_ISO_TX(obj);

	hinoko_fw_iso_tx_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_tx_parent_class)->finalize(obj);
}

enum fw_iso_tx_sig_type {
	FW_ISO_TX_SIG_TYPE_IRQ = 1,
	FW_ISO_TX_SIG_TYPE_COUNT,
};
static guint fw_iso_tx_sigs[FW_ISO_TX_SIG_TYPE_COUNT] = { 0 };

static void hinoko_fw_iso_tx_class_init(HinokoFwIsoTxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_tx_finalize;

	/**
	 * HinokoFwIsoTx::interrupted:
	 * @self: A #HinokoFwIsoTx.
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @tstamp: (array length=tstamp_length) (element-type guint8): A series
	 *	    of timestamps for packets already handled.
	 * @tstamp_length: the number of bytes for @tstamp.
	 * @count: the number of handled packets.
	 *
	 * When Linux FireWire subsystem generates interrupt event, the #HinokoFwIsoTx::interrupted
	 * signal is emitted. There are three cases for Linux FireWire subsystem to generate the
	 * event:
	 *
	 * - When OHCI 1394 controller generates hardware interrupt as a result of processing the
	 *   isochronous packet for the buffer chunk marked to generate hardware interrupt.
	 * - When the number of isochronous packets sent since the last interrupt event reaches
	 *   one quarter of memory page size (usually 4,096 / 4 = 1,024 packets).
	 * - When application calls #hinoko_fw_iso_ctx_flush_completions() explicitly.
	 */
	fw_iso_tx_sigs[FW_ISO_TX_SIG_TYPE_IRQ] =
		g_signal_new("interrupted",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(HinokoFwIsoTxClass, interrupted),
			NULL, NULL,
			hinoko_sigs_marshal_VOID__UINT_UINT_POINTER_UINT_UINT,
			G_TYPE_NONE,
			5, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER,
			G_TYPE_UINT, G_TYPE_UINT);
}

static void hinoko_fw_iso_tx_init(HinokoFwIsoTx *self)
{
	return;
}

/**
 * hinoko_fw_iso_tx_new:
 *
 * Instantiate #HinokoFwIsoTx object and return the instance.
 *
 * Returns: an instance of #HinokoFwIsoTx.
 */
HinokoFwIsoTx *hinoko_fw_iso_tx_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_TX, NULL);
}

/**
 * hinoko_fw_iso_tx_allocate:
 * @self: A #HinokoFwIsoTx.
 * @path: A path to any Linux FireWire character device.
 * @scode: A #HinokoFwScode to indicate speed of isochronous communication.
 * @channel: An isochronous channel to transfer.
 * @header_size: The number of bytes for header of IT context.
 * @exception: A #GError.
 *
 * Allocate an IT context to 1394 OHCI controller. A local node of the node
 * corresponding to the given path is used as the controller, thus any path is
 * accepted as long as process has enough permission for the path.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 *
 * Since: 0.7.
 */
gboolean hinoko_fw_iso_tx_allocate(HinokoFwIsoTx *self, const char *path, HinokoFwScode scode,
				   guint channel, guint header_size, GError **exception)
{
	g_return_val_if_fail(HINOKO_IS_FW_ISO_TX(self), FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);

	return hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path, HINOKO_FW_ISO_CTX_MODE_TX,
					  scode, channel, header_size, exception);
}

/**
 * hinoko_fw_iso_tx_release:
 * @self: A #HinokoFwIsoTx.
 *
 * Release allocated IT context from 1394 OHCI controller.
 */
void hinoko_fw_iso_tx_release(HinokoFwIsoTx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	hinoko_fw_iso_tx_unmap_buffer(self);

	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(self));
}

/**
 * hinoko_fw_iso_tx_map_buffer:
 * @self: A #HinokoFwIsoTx.
 * @maximum_bytes_per_payload: The number of bytes for payload of IT context.
 * @payloads_per_buffer: The number of payloads of IT context in buffer.
 * @exception: A #GError.
 *
 * Map intermediate buffer to share payload of IT context with 1394 OHCI
 * controller.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 *
 * Since: 0.7.
 */
gboolean hinoko_fw_iso_tx_map_buffer(HinokoFwIsoTx *self, guint maximum_bytes_per_payload,
				     guint payloads_per_buffer, GError **exception)
{
	HinokoFwIsoTxPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_TX(self), FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);
	priv = hinoko_fw_iso_tx_get_instance_private(self);

	if (!hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self), maximum_bytes_per_payload,
					  payloads_per_buffer, exception))
		return FALSE;

	priv->offset = 0;

	return TRUE;
}

/**
 * hinoko_fw_iso_tx_unmap_buffer:
 * @self: A #HinokoFwIsoTx.
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload
 * of IT context.
 */
void hinoko_fw_iso_tx_unmap_buffer(HinokoFwIsoTx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	hinoko_fw_iso_tx_stop(self);

	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));
}

/**
 * hinoko_fw_iso_tx_start:
 * @self: A #HinokoFwIsoTx.
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable):
 * 		 The isochronous cycle to start packet processing. The first
 * 		 element should be the second part of isochronous cycle, up to
 * 		 3. The second element should be the cycle part of isochronous
 * 		 cycle, up to 7999.
 * @exception: A #GError.
 *
 * Start IT context.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 *
 * Since: 0.7.
 */
gboolean hinoko_fw_iso_tx_start(HinokoFwIsoTx *self, const guint16 *cycle_match, GError **exception)
{
	g_return_val_if_fail(HINOKO_IS_FW_ISO_TX(self), FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);

	return hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, 0, 0, exception);

}

/**
 * hinoko_fw_iso_tx_stop:
 * @self: A #HinokoFwIsoTx.
 *
 * Stop IT context.
 */
void hinoko_fw_iso_tx_stop(HinokoFwIsoTx *self)
{
	HinokoFwIsoTxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	priv = hinoko_fw_iso_tx_get_instance_private(self);

	hinoko_fw_iso_ctx_stop(HINOKO_FW_ISO_CTX(self));

	priv->offset = 0;
}

/**
 * hinoko_fw_iso_tx_register_packet:
 * @self: A #HinokoFwIsoTx.
 * @tags: The value of tag field for isochronous packet to register.
 * @sy: The value of sy field for isochronous packet to register.
 * @header: (array length=header_length)(nullable): The header of IT context
 * 	    for isochronous packet.
 * @header_length: The number of bytes for the @header.
 * @payload: (array length=payload_length)(nullable): The payload of IT context
 * 	     for isochronous packet.
 * @payload_length: The number of bytes for the @payload.
 * @schedule_interrupt: Whether to schedule hardware interrupt at isochronous cycle for the packet.
 * @exception: A #GError.
 *
 * Register packet data with header and payload for IT context. The caller can schedule hardware
 * interrupt to generate interrupt event. In detail, please refer to documentation about
 * #HinokoFwIsoTx::interrupted.
 *
 * Returns: %TRUE if the overall operation finished successfully, otherwise %FALSE.
 *
 * Since: 0.7.
 */
gboolean hinoko_fw_iso_tx_register_packet(HinokoFwIsoTx *self,HinokoFwIsoCtxMatchFlag tags,
					  guint sy, const guint8 *header, guint header_length,
					  const guint8 *payload, guint payload_length,
					  gboolean schedule_interrupt, GError **exception)
{
	HinokoFwIsoTxPrivate *priv;
	const guint8 *frames;
	guint frame_size;
	gboolean skip;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_TX(self), FALSE);
	g_return_val_if_fail((header != NULL && header_length > 0) ||
			     (header == NULL && header_length == 0), FALSE);
	g_return_val_if_fail((payload != NULL && payload_length > 0) ||
			     (payload == NULL && payload_length == 0), FALSE);
	g_return_val_if_fail(exception != NULL && *exception == NULL, FALSE);

	priv = hinoko_fw_iso_tx_get_instance_private(self);

	skip = FALSE;
	if (header_length == 0 && payload_length == 0)
		skip = TRUE;

	if (!hinoko_fw_iso_ctx_register_chunk(HINOKO_FW_ISO_CTX(self), skip, tags, sy, header,
					      header_length, payload_length, schedule_interrupt,
					      exception))
		return FALSE;

	hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self), priv->offset,
				      payload_length, &frames, &frame_size);
	memcpy((void *)frames, payload, frame_size);
	priv->offset += frame_size;

	if (frame_size != payload_length) {
		guint rest = payload_length - frame_size;

		payload += frame_size;
		hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self), 0,
					      rest, &frames, &frame_size);
		memcpy((void *)frames, payload, frame_size);

		priv->offset = frame_size;
	}

	return TRUE;
}

void hinoko_fw_iso_tx_handle_event(HinokoFwIsoTx *self,
				   struct fw_cdev_event_iso_interrupt *event,
				   GError **exception)
{
	guint sec = (event->cycle & 0x0000e000) >> 13;
	guint cycle = event->cycle & 0x00001fff;
	unsigned int pkt_count = event->header_length / 4;

	g_signal_emit(self, fw_iso_tx_sigs[FW_ISO_TX_SIG_TYPE_IRQ], 0, sec, cycle, event->header,
		      event->header_length, pkt_count);
}
