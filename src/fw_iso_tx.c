// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"
#include "hinoko_sigs_marshal.h"

#include <unistd.h>
#include <errno.h>

/**
 * SECTION:fw_iso_tx
 * @Title: HinokoFwIsoTx
 * @Short_description: An object to transmit isochronous packet for single
 *		       channel.
 *
 * A #HinokoFwIsoTx transmits isochronous packets for single channel by IT
 * context in 1394 OHCI. The content of packet is split to two parts; context
 * header and context payload in a manner of Linux FireWire subsystem.
 */
struct _HinokoFwIsoTxPrivate {
	guint offset;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoTx, hinoko_fw_iso_tx,
			   HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoTx", hinoko_fw_iso_tx)
#define raise(exception, errno)					\
	g_set_error(exception, hinoko_fw_iso_tx_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

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
	 * When registered packets are handled, #HinokoFwIsoTx::interrupted
	 * signal is emitted with timestamps of the packet.
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
 */
void hinoko_fw_iso_tx_allocate(HinokoFwIsoTx *self, const char *path,
			       HinokoFwScode scode, guint channel,
			       guint header_size, GError **exception)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				   HINOKO_FW_ISO_CTX_MODE_TX, scode, channel,
				   header_size, exception);
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
 */
void hinoko_fw_iso_tx_map_buffer(HinokoFwIsoTx *self,
				 guint maximum_bytes_per_payload,
				 guint payloads_per_buffer,
				 GError **exception)
{
	HinokoFwIsoTxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	priv = hinoko_fw_iso_tx_get_instance_private(self);

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self),
				     maximum_bytes_per_payload,
				     payloads_per_buffer, exception);

	priv->offset = 0;
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

static void fw_iso_tx_register_chunk(HinokoFwIsoTx *self,
				     HinokoFwIsoCtxMatchFlag tags, guint sy,
				     const guint8 *header, guint header_length,
				     guint payload_length, GError **exception)
{
	gboolean skip = FALSE;

	if (header_length == 0 && payload_length == 0)
		skip = TRUE;

	hinoko_fw_iso_ctx_register_chunk(HINOKO_FW_ISO_CTX(self), skip, tags,
					 sy, header, header_length,
					 payload_length, exception);
}

/**
 * hinoko_fw_iso_tx_start:
 * @self: A #HinokoFwIsoTx.
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable):
 * 		 The isochronous cycle to start packet processing. The first
 * 		 element should be the second part of isochronous cycle, up to
 * 		 3. The second element should be the cycle part of isochronous
 * 		 cycle, up to 7999.
 * @packets_per_irq: The number of packets as interval of event. This value
 *		     should be up to (pagesize / 4) due to implementation of
 *		     Linux FireWire subsystem.
 * @exception: A #GError.
 *
 * Start IT context.
 */
void hinoko_fw_iso_tx_start(HinokoFwIsoTx *self, const guint16 *cycle_match,
			    guint packets_per_irq, GError **exception)
{
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	// MEMO: Linux FireWire subsystem queues isochronous event independently
	// of interrupt flag when the same number of bytes as one page is
	// stored in the buffer of header. To avoid unexpected wakeup, check
	// the interval.
	if (packets_per_irq == 0 || packets_per_irq * 4 > sysconf(_SC_PAGESIZE)) {
		raise(exception, EINVAL);
		return;
	}

	for (i = 0; i < packets_per_irq * 2; ++i) {
		fw_iso_tx_register_chunk(self,
					 HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0, 0,
					 NULL, 0, 0, exception);
		if (*exception != NULL)
			return;
	}

	hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, 0, 0,
				packets_per_irq, exception);

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
 * @header: (array length=header_length) (element-type guint8) (in) (nullable):
 * 	    The header of IT context for isochronous packet.
 * @header_length: The number of bytes for the @header.
 * @payload: (array length=payload_length) (element-type guint8) (in) (nullable):
 * 	     The payload of IT context for isochronous packet.
 * @payload_length: The number of bytes for the @payload.
 * @exception: A #GError.
 *
 * Register packet data in a shape of header and payload of IT context.
 */
void hinoko_fw_iso_tx_register_packet(HinokoFwIsoTx *self,
				HinokoFwIsoCtxMatchFlag tags, guint sy,
				const guint8 *header, guint header_length,
				const guint8 *payload, guint payload_length,
				GError **exception)
{
	HinokoFwIsoTxPrivate *priv;
	const guint8 *frames;
	guint frame_size;

	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));
	priv = hinoko_fw_iso_tx_get_instance_private(self);

	if ((header == NULL && header_length > 0) ||
	    (header != NULL && header_length == 0)) {
		raise(exception, EINVAL);
		return;
	}

	if ((payload == NULL && payload_length > 0) ||
	    (payload != NULL && payload_length == 0)) {
		raise(exception, EINVAL);
		return;
	}

	fw_iso_tx_register_chunk(self, tags, sy, header, header_length,
				 payload_length, exception);
	if (*exception != NULL)
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

void hinoko_fw_iso_tx_handle_event(HinokoFwIsoTx *self,
				   struct fw_cdev_event_iso_interrupt *event,
				   GError **exception)
{
	guint sec = (event->cycle & 0x0000e000) >> 13;
	guint cycle = event->cycle & 0x00001fff;
	unsigned int pkt_count = event->header_length / 4;
	guint registered_chunk_count;
	int i;

	g_signal_emit(self, fw_iso_tx_sigs[FW_ISO_TX_SIG_TYPE_IRQ],
		0, sec, cycle, event->header, event->header_length, pkt_count);

	g_object_get(G_OBJECT(self), "registered-chunk-count",
		     &registered_chunk_count, NULL);

	for (i = registered_chunk_count; i < pkt_count; ++i) {
		fw_iso_tx_register_chunk(self,
					 HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1, 0,
					 NULL, 0, 0, exception);
		if (*exception != NULL)
			return;
	}
}
