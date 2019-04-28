// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"
#include "hinoko_sigs_marshal.h"

#include <unistd.h>
#include <errno.h>

/**
 * SECTION:fw_iso_rx_single
 * @Title: HinokoFwIsoRxSingle
 * @Short_description: An object to receive isochronous packet for single
 *		       channel.
 *
 * A #HinokoFwIsoRxSingle receives isochronous packets for single channel by IR
 * context for packet-per-buffer mode in 1394 OHCI. The content of packet is
 * split to two parts; context header and context payload in a manner of Linux
 * FireWire subsystem.
 *
 */
struct _HinokoFwIsoRxSinglePrivate {
	guint header_size;
	guint chunks_per_irq;
	guint accumulate_chunk_count;
	guint maximum_chunk_count;

	const struct fw_cdev_event_iso_interrupt *ev;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoRxSingle, hinoko_fw_iso_rx_single,
			   HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoRxSingle", hinoko_fw_iso_rx_single)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_rx_single_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void fw_iso_rx_single_finalize(GObject *obj)
{
	HinokoFwIsoRxSingle *self = HINOKO_FW_ISO_RX_SINGLE(obj);

	hinoko_fw_iso_rx_single_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_single_parent_class)->finalize(obj);
}

enum fw_iso_rx_single_sig_type {
	FW_ISO_RX_SINGLE_SIG_TYPE_INTERRUPTED = 1,
	FW_ISO_RX_SINGLE_SIG_TYPE_COUNT,
};
static guint fw_iso_rx_single_sigs[FW_ISO_RX_SINGLE_SIG_TYPE_COUNT] = { 0 };

static void hinoko_fw_iso_rx_single_class_init(HinokoFwIsoRxSingleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_rx_single_finalize;

	/**
	 * HinokoFwIsoRxSingle::interrupted:
	 * @self: A #HinokoFwIsoRxSingle.
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @header: (array length=header_length) (element-type guint8): The
	 * 	    headers of IR context for handled packets.
	 * @header_length: the number of bytes for @header.
	 * @count: the number of packets to handle.
	 *
	 * When any packet is available, the #HinokoFwIsoRxSingle::interrupted
	 * signal is emitted with header of the packet. In a handler of the
	 * signal, payload of received packet is available by a call of
	 * #hinoko_fw_iso_rx_single_get_payload().
	 */
	fw_iso_rx_single_sigs[FW_ISO_RX_SINGLE_SIG_TYPE_INTERRUPTED] =
		g_signal_new("interrupted",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(HinokoFwIsoRxSingleClass, interrupted),
			NULL, NULL,
			hinoko_sigs_marshal_VOID__UINT_UINT_POINTER_UINT_UINT,
			G_TYPE_NONE,
			5, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER,
			G_TYPE_UINT, G_TYPE_UINT);
}

static void hinoko_fw_iso_rx_single_init(HinokoFwIsoRxSingle *self)
{
	return;
}

/**
 * hinoko_fw_iso_rx_single_new:
 *
 * Instantiate #HinokoFwIsoRxSingle object and return the instance.
 *
 * Returns: an instance of #HinokoFwIsoRxSingle.
 */
HinokoFwIsoRxSingle *hinoko_fw_iso_rx_single_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RX_SINGLE, NULL);
}

/**
 * hinoko_fw_iso_rx_single_allocate:
 * @self: A #HinokoFwIsoRxSingle.
 * @path: A path to any Linux FireWire character device.
 * @scode: A #HinokoFwScode to indicate speed of isochronous communication.
 * @channel: An isochronous channel to listen.
 * @header_size: The number of bytes for header of IR context.
 * @exception: A #GError.
 *
 * Allocate an IR context to 1394 OHCI controller for packet-per-buffer mode.
 * A local node of the node corresponding to the given path is used as the
 * controller, thus any path is accepted as long as process has enough
 * permission for the path.
 */
void hinoko_fw_iso_rx_single_allocate(HinokoFwIsoRxSingle *self,
				      const char *path, HinokoFwScode scode,
				      guint channel, guint header_size,
				      GError **exception)
{
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				 HINOKO_FW_ISO_CTX_MODE_RX_SINGLE, scode,
				 channel, header_size, exception);
	if (*exception != NULL)
		return;

	priv->header_size = header_size;
}

/**
 * hinoko_fw_iso_rx_single_release:
 * @self: A #HinokoFwIsoRxSingle.
 *
 * Release allocated IR context from 1394 OHCI controller.
 */
void hinoko_fw_iso_rx_single_release(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_rx_single_unmap_buffer(self);
	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(self));
}

/*
 * hinoko_fw_iso_rx_single_map_buffer:
 * @self: A #HinokoFwIsoRxSingle.
 * @maximum_bytes_per_payload: The maximum number of bytes per payload of IR
 *			       context.
 * @payloads_per_buffer: The number of payload in buffer.
 * @payloads_per_irq: The number of payload per interval of interrupt.
 * @exception: A #GError.
 *
 * Map intermediate buffer to share payload of IR context with 1394 OHCI
 * controller.
 */
void hinoko_fw_iso_rx_single_map_buffer(HinokoFwIsoRxSingle *self,
					guint maximum_bytes_per_payload,
					guint payloads_per_buffer,
					GError **exception)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self),
				     maximum_bytes_per_payload,
				     payloads_per_buffer, exception);
}

/**
 * hinoko_fw_iso_rx_single_unmap_buffer:
 * @self: A #HinokoFwIsoRxSingle.
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload
 * of IR context.
 */
void hinoko_fw_iso_rx_single_unmap_buffer(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_rx_single_stop(self);
	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));
}

static void fw_iso_rx_single_register_chunk(HinokoFwIsoRxSingle *self,
					    gboolean skip, GError **exception)
{
	HinokoFwIsoRxSinglePrivate *priv;
	gboolean irq;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	++priv->accumulate_chunk_count;
	priv->accumulate_chunk_count %= priv->maximum_chunk_count;
	if (priv->accumulate_chunk_count % priv->chunks_per_irq == 0)
		irq = TRUE;
	else
		irq = FALSE;

	hinoko_fw_iso_ctx_register_chunk(HINOKO_FW_ISO_CTX(self), skip, 0, 0,
					 NULL, 0, 0, irq, exception);
}

/**
 * hinoko_fw_iso_rx_single_start:
 * @self: A #HinokoFwIsoRxSingle.
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable):
 * 		 The isochronous cycle to start packet processing. The first
 * 		 element should be the second part of isochronous cycle, up to
 * 		 3. The second element should be the cycle part of isochronous
 * 		 cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing,
 * 	  up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @packets_per_irq: The number of packets as interval of event. Skip cycles are
 *		     ignored.
 * @exception: A #GError.
 *
 * Start IR context.
 */
void hinoko_fw_iso_rx_single_start(HinokoFwIsoRxSingle *self,
				   const guint16 *cycle_match, guint32 sync,
				   HinokoFwIsoCtxMatchFlag tags,
				   guint packets_per_irq, GError **exception)
{
	HinokoFwIsoRxSinglePrivate *priv;
	unsigned int chunks_per_buffer;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	// MEMO: Linux FireWire subsystem queues isochronous event independently
	// of interrupt flag when the same number of bytes as one page is
	// stored in the buffer of header. To avoid unexpected wakeup, check
	// the interval.
	if (priv->header_size * packets_per_irq > sysconf(_SC_PAGESIZE)) {
		raise(exception, EINVAL);
		return;
	}

	priv->chunks_per_irq = packets_per_irq;
	priv->maximum_chunk_count = G_MAXUINT / 2 / packets_per_irq;
	priv->maximum_chunk_count *= packets_per_irq;
	priv->accumulate_chunk_count = 0;

	g_object_get(G_OBJECT(self), "chunks-per-buffer", &chunks_per_buffer,
		     NULL);

	for (i = 0; i < chunks_per_buffer; ++i) {
		fw_iso_rx_single_register_chunk(self, FALSE, exception);
		if (*exception != NULL)
			return;
	}

	hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, sync,
				tags, exception);
}

/**
 * hinoko_fw_iso_rx_single_stop:
 * @self: A #HinokoFwIsoRxSingle.
 *
 * Stop IR context.
 */
void hinoko_fw_iso_rx_single_stop(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_ctx_stop(HINOKO_FW_ISO_CTX(self));
}

void hinoko_fw_iso_rx_single_handle_event(HinokoFwIsoRxSingle *self,
				struct fw_cdev_event_iso_interrupt *event,
				GError **exception)
{
	HinokoFwIsoRxSinglePrivate *priv;
	guint sec;
	guint cycle;
	guint count;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	sec = (event->cycle & 0x0000e000) >> 13;
	cycle = event->cycle & 0x00001fff;
	count = event->header_length / priv->header_size;

	// TODO; handling error?
	priv->ev = event;
	g_signal_emit(self,
		fw_iso_rx_single_sigs[FW_ISO_RX_SINGLE_SIG_TYPE_INTERRUPTED],
		0, sec, cycle, event->header, event->header_length, count);
	priv->ev = NULL;

	for (i = 0; i < priv->chunks_per_irq; ++i) {
		// Register consumed chunks to reuse.
		fw_iso_rx_single_register_chunk(self, FALSE, exception);
		if (*exception != NULL)
			return;
	}
}

/**
 * hinoko_fw_iso_rx_single_get_payload:
 * @self: A #HinokoFwIsoRxSingle.
 * @index: the index inner available packets.
 * @payload: (element-type guint8) (array length=length) (out callee-allocates):
 *	     An array with bytes for payload of IR context.
 * @length: (out): The number of bytes in the above @payload.
 * @exception: A #GError.
 *
 * Retrieve payload of IR context for a handled packet corresponding to index.
 */
void hinoko_fw_iso_rx_single_get_payload(HinokoFwIsoRxSingle *self, guint index,
					 const guint8 **payload, guint *length,
					 GError **exception)
{
	HinokoFwIsoRxSinglePrivate *priv;
	unsigned int bytes_per_chunk;
	unsigned int chunks_per_buffer;
	unsigned int pos;
	guint32 iso_header;
	guint offset;
	guint frame_size;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	if (priv->ev == NULL) {
		raise(exception, ENODATA);
		return;
	}

	if (index * priv->header_size >= priv->ev->header_length) {
		raise(exception, EINVAL);
		return;
	}

	g_object_get(G_OBJECT(self), "bytes-per-chunk", &bytes_per_chunk, NULL);
	g_object_get(G_OBJECT(self), "chunks-per-buffer", &chunks_per_buffer,
		     NULL);

	pos = index * priv->header_size / 4;
	iso_header = GUINT32_FROM_BE(priv->ev->header[pos]);
	*length = (iso_header & 0xffff0000) >> 16;
	if (*length > bytes_per_chunk)
		*length = bytes_per_chunk;

	index = (priv->accumulate_chunk_count + index) % chunks_per_buffer;
	offset = index * bytes_per_chunk;
	hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self), offset, *length,
				      payload, &frame_size);
	if (frame_size != *length) {
		raise(exception, EIO);
		return;
	}
}
