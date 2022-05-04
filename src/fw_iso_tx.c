// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

/**
 * HinokoFwIsoTx:
 * An object to transmit isochronous packet for single channel.
 *
 * A [class@FwIsoTx] transmits isochronous packets for single channel by IT context in 1394 OHCI.
 * The content of packet is split to two parts; context header and context payload in a manner of
 * Linux FireWire subsystem.
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
	 * @self: A [class@FwIsoTx].
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @tstamp: (array length=tstamp_length) (element-type guint8): A series of timestamps for
	 *	    packets already handled.
	 * @tstamp_length: the number of bytes for @tstamp.
	 * @count: the number of handled packets.
	 *
	 * Emitted when Linux FireWire subsystem generates interrupt event. There are three cases
	 * for Linux FireWire subsystem to generate the event:
	 *
	 * - When OHCI 1394 controller generates hardware interrupt as a result of processing the
	 *   isochronous packet for the buffer chunk marked to generate hardware interrupt.
	 * - When the number of isochronous packets sent since the last interrupt event reaches
	 *   one quarter of memory page size (usually 4,096 / 4 = 1,024 packets).
	 * - When application calls [method@FwIsoCtx.flush_completions] explicitly.
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

gboolean fw_iso_tx_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				GError **error)
{
	const struct fw_cdev_event_iso_interrupt *ev;

	g_return_val_if_fail(HINOKO_FW_ISO_TX(inst), FALSE);
	g_return_val_if_fail(event->common.type == FW_CDEV_EVENT_ISO_INTERRUPT, FALSE);

	ev = &event->iso_interrupt;

	guint sec = (ev->cycle & 0x0000e000) >> 13;
	guint cycle = ev->cycle & 0x00001fff;
	unsigned int pkt_count = ev->header_length / 4;

	g_signal_emit(inst, fw_iso_tx_sigs[FW_ISO_TX_SIG_TYPE_IRQ], 0, sec, cycle, ev->header,
		      ev->header_length, pkt_count);

	return TRUE;
}

/**
 * hinoko_fw_iso_tx_new:
 *
 * Instantiate [class@FwIsoTx] object and return the instance.
 *
 * Returns: an instance of [class@FwIsoTx].
 */
HinokoFwIsoTx *hinoko_fw_iso_tx_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_TX, NULL);
}

/**
 * hinoko_fw_iso_tx_allocate:
 * @self: A [class@FwIsoTx].
 * @path: A path to any Linux FireWire character device.
 * @scode: A [enum@FwScode] to indicate speed of isochronous communication.
 * @channel: An isochronous channel to transfer.
 * @header_size: The number of bytes for header of IT context.
 * @error: A [struct@GLib.Error].
 *
 * Allocate an IT context to 1394 OHCI controller. A local node of the node corresponding to the
 * given path is used as the controller, thus any path is accepted as long as process has enough
 * permission for the path.
 */
void hinoko_fw_iso_tx_allocate(HinokoFwIsoTx *self, const char *path,
			       HinokoFwScode scode, guint channel,
			       guint header_size, GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	g_return_if_fail(error == NULL || *error == NULL);

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				   HINOKO_FW_ISO_CTX_MODE_TX, scode, channel,
				   header_size, error);
}

/**
 * hinoko_fw_iso_tx_release:
 * @self: A [class@FwIsoTx].
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
 * @self: A [class@FwIsoTx].
 * @maximum_bytes_per_payload: The number of bytes for payload of IT context.
 * @payloads_per_buffer: The number of payloads of IT context in buffer.
 * @error: A [struct@GLib.Error].
 *
 * Map intermediate buffer to share payload of IT context with 1394 OHCI controller.
 */
void hinoko_fw_iso_tx_map_buffer(HinokoFwIsoTx *self,
				 guint maximum_bytes_per_payload,
				 guint payloads_per_buffer,
				 GError **error)
{
	HinokoFwIsoTxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	g_return_if_fail(error == NULL || *error == NULL);
	priv = hinoko_fw_iso_tx_get_instance_private(self);

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self),
				     maximum_bytes_per_payload,
				     payloads_per_buffer, error);

	priv->offset = 0;
}

/**
 * hinoko_fw_iso_tx_unmap_buffer:
 * @self: A [class@FwIsoTx].
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload of IT context.
 */
void hinoko_fw_iso_tx_unmap_buffer(HinokoFwIsoTx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	hinoko_fw_iso_tx_stop(self);

	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));
}

/**
 * hinoko_fw_iso_tx_start:
 * @self: A [class@FwIsoTx].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @error: A [struct@GLib.Error].
 *
 * Start IT context.
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_tx_start(HinokoFwIsoTx *self, const guint16 *cycle_match, GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	g_return_if_fail(error == NULL || *error == NULL);

	hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, 0, 0, error);

}

/**
 * hinoko_fw_iso_tx_stop:
 * @self: A [class@FwIsoTx].
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
 * @self: A [class@FwIsoTx].
 * @tags: The value of tag field for isochronous packet to register.
 * @sy: The value of sy field for isochronous packet to register.
 * @header: (array length=header_length)(nullable): The header of IT context for isochronous packet.
 * @header_length: The number of bytes for the @header.
 * @payload: (array length=payload_length)(nullable): The payload of IT context for isochronous
 *	     packet.
 * @payload_length: The number of bytes for the @payload.
 * @schedule_interrupt: Whether to schedule hardware interrupt at isochronous cycle for the packet.
 * @error: A [struct@GLib.Error].
 *
 * Register packet data with header and payload for IT context. The caller can schedule hardware
 * interrupt to generate interrupt event. In detail, please refer to documentation about
 * [signal@FwIsoTx::interrupted].
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_tx_register_packet(HinokoFwIsoTx *self,
				HinokoFwIsoCtxMatchFlag tags, guint sy,
				const guint8 *header, guint header_length,
				const guint8 *payload, guint payload_length,
				gboolean schedule_interrupt, GError **error)
{
	HinokoFwIsoTxPrivate *priv;
	const guint8 *frames;
	guint frame_size;
	gboolean skip;

	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	g_return_if_fail((header != NULL && header_length > 0) ||
			 (header == NULL && header_length == 0));
	g_return_if_fail((payload != NULL && payload_length > 0) ||
			 (payload == NULL && payload_length == 0));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_tx_get_instance_private(self);

	skip = FALSE;
	if (header_length == 0 && payload_length == 0)
		skip = TRUE;

	hinoko_fw_iso_ctx_register_chunk(HINOKO_FW_ISO_CTX(self), skip, tags, sy, header,
					 header_length, payload_length, schedule_interrupt,
					 error);
	if (*error != NULL)
		return;

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
}
