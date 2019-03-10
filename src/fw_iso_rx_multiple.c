// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <errno.h>

/**
 * SECTION:fw_iso_rx_multiple
 * @Title: HinokoFwIsoRxMultiple
 * @Short_description: An object to receive isochronous packet for several
 *		       channels.
 *
 * A #HinokoFwIsoRxMultiple receives isochronous packets for several channels by
 * IR context for buffer-fill mode in 1394 OHCI.
 */
struct _HinokoFwIsoRxMultiplePrivate {
	GByteArray *channels;
	guint chunks_per_irq;
	guint accumulate_chunk_count;
	guint maximum_chunk_count;

	guint prev_offset;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoRxMultiple, hinoko_fw_iso_rx_multiple,
			   HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoRxMultiple", hinoko_fw_iso_rx_multiple)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_rx_multiple_quark(), errno, \
		    "%d: %s", __LINE__, strerror(errno))

static void fw_iso_rx_multiple_finalize(GObject *obj)
{
	HinokoFwIsoRxMultiple *self = HINOKO_FW_ISO_RX_MULTIPLE(obj);

	hinoko_fw_iso_rx_multiple_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_multiple_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_rx_multiple_class_init(HinokoFwIsoRxMultipleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_rx_multiple_finalize;
}

static void hinoko_fw_iso_rx_multiple_init(HinokoFwIsoRxMultiple *self)
{
	return;
}

/**
 * hinoko_fw_iso_rx_multiple_new:
 *
 * Instantiate #HinokoFwIsoRxMultiple object and return the instance.
 *
 * Returns: an instance of #HinokoFwIsoRxMultiple.
 */
HinokoFwIsoRxMultiple *hinoko_fw_iso_rx_multiple_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RX_MULTIPLE, NULL);
}

/**
 * hinoko_fw_iso_rx_multiple_allocate:
 * @self: A #HinokoFwIsoRxMultiple.
 * @path: A path to any Linux FireWire character device.
 * @scode: A #HinokoFwScode to indicate speed of isochronous communication.
 * @channels: (array length=channels_length) (element-type guint8): an array
 *	      for channels to listen to.
 * @channels_length: The length of @channels.
 * @exception: A #GError.
 *
 * Allocate an IR context to 1394 OHCI controller for buffer-fill mode.
 * A local node of the node corresponding to the given path is used as the
 * controller, thus any path is accepted as long as process has enough
 * permission for the path.
 */
void hinoko_fw_iso_rx_multiple_allocate(HinokoFwIsoRxMultiple *self,
					const char *path, HinokoFwScode scode,
					const guint8 *channels,
					guint channels_length,
					GError **exception)
{
	HinokoFwIsoRxMultiplePrivate *priv;

	guint64 channel_flags;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	if (channels_length == 0) {
		raise(exception, EINVAL);
		return;
	}

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				   HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE, scode, 0,
				   0, exception);
	if (*exception != NULL)
		return;

	channel_flags = 0;
	for (i = 0; i < channels_length; ++i) {
		if (channels[i] > 64) {
			raise(exception, EINVAL);
			return;
		}

		channel_flags |= G_GUINT64_CONSTANT(1) << channels[i];
	}

	hinoko_fw_iso_ctx_set_rx_channels(HINOKO_FW_ISO_CTX(self),
					  &channel_flags, exception);
	if (*exception != NULL)
		return;
	if (channel_flags == 0) {
		raise(exception, ENODATA);
		return;
	}

	priv->channels = g_byte_array_new();
	for (i = 0; i < 64; ++i) {
		if (!(channel_flags & (G_GUINT64_CONSTANT(1) << i)))
			continue;

		g_byte_array_append(priv->channels, (const guint8 *)&i, 1);
	}
}

/**
 * hinoko_fw_iso_rx_multiple_release:
 * @self: A #HinokoFwIsoRxMultiple.
 *
 * Release allocated IR context from 1394 OHCI controller.
 */
void hinoko_fw_iso_rx_multiple_release(HinokoFwIsoRxMultiple *self)
{
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	hinoko_fw_iso_rx_multiple_unmap_buffer(self);

	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(self));

	if (priv->channels != NULL)
		g_byte_array_unref(priv->channels);
	priv->channels = NULL;
}

/**
 * hinoko_fw_iso_rx_multiple_map_buffer:
 * @self: A #HinokoFwIsoRxMultiple.
 * @bytes_per_chunk: The maximum number of bytes for payload of isochronous
 *		     packet (not payload for isochronous context).
 * @chunks_per_buffer: The number of chunks in buffer.
 * @exception: A #GError.
 *
 * Map an intermediate buffer to share payload of IR context with 1394 OHCI
 * controller.
 */
void hinoko_fw_iso_rx_multiple_map_buffer(HinokoFwIsoRxMultiple *self,
					  guint bytes_per_chunk,
					  guint chunks_per_buffer,
					  GError **exception)
{
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	if (priv->channels == NULL) {
		raise(exception, ENODATA);
		return;
	}

	// The size of each chunk should be aligned to quadlet.
	bytes_per_chunk = (bytes_per_chunk + 3) / 4;
	bytes_per_chunk *= 4;

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self), bytes_per_chunk,
				     chunks_per_buffer, exception);
}

/**
 * hinoko_fw_iso_rx_multiple_unmap_buffer:
 * @self: A #HinokoFwIsoRxMultiple.
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload
 * of IR context.
 */
void hinoko_fw_iso_rx_multiple_unmap_buffer(HinokoFwIsoRxMultiple *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));

	hinoko_fw_iso_rx_multiple_stop(self);

	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));
}

static void fw_iso_rx_multiple_register_chunk(HinokoFwIsoRxMultiple *self,
					      gboolean skip, GError **exception)
{
	HinokoFwIsoRxMultiplePrivate *priv;
	gboolean irq;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

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
 * hinoko_fw_iso_rx_multiple_start:
 * @self: A #HinokoFwIsoRxMultiple.
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable):
 * 		 The isochronous cycle to start packet processing. The first
 * 		 element should be the second part of isochronous cycle, up to
 * 		 3. The second element should be the cycle part of isochronous
 * 		 cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing,
 * 	  up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @chunks_per_irq: The number of chunks per interval of interrupt.
 * @exception: A #GError.
 *
 * Start IR context.
 */
void hinoko_fw_iso_rx_multiple_start(HinokoFwIsoRxMultiple *self,
				     const guint16 *cycle_match, guint32 sync,
				     HinokoFwIsoCtxMatchFlag tags,
				     guint chunks_per_irq, GError **exception)
{
	HinokoFwIsoRxMultiplePrivate *priv;
	GValue val = G_VALUE_INIT;
	unsigned int chunks_per_buffer;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	priv->chunks_per_irq = chunks_per_irq;
	priv->maximum_chunk_count = G_MAXUINT / 2 / chunks_per_irq;
	priv->maximum_chunk_count *= chunks_per_irq;
	priv->accumulate_chunk_count = 0;

	g_value_init(&val, G_TYPE_UINT);
	g_object_get_property(G_OBJECT(self), "chunks-per-buffer", &val);
	chunks_per_buffer = g_value_get_uint(&val);

	for (i = 0; i < chunks_per_buffer; ++i) {
		fw_iso_rx_multiple_register_chunk(self, FALSE, exception);
		if (*exception != NULL)
			return;
	}

	priv->prev_offset = 0;
	hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, sync,
				tags, exception);
}

/**
 * hinoko_fw_iso_rx_multiple_stop:
 * @self: A #HinokoFwIsoRxMultiple.
 *
 * Stop IR context.
 */
void hinoko_fw_iso_rx_multiple_stop(HinokoFwIsoRxMultiple *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));

	hinoko_fw_iso_ctx_stop(HINOKO_FW_ISO_CTX(self));
}

void hinoko_fw_iso_rx_multiple_handle_event(HinokoFwIsoRxMultiple *self,
				struct fw_cdev_event_iso_interrupt_mc *event,
				GError **exception)
{
	HinokoFwIsoRxMultiplePrivate *priv;
	GValue val = G_VALUE_INIT;
	unsigned int bytes_per_chunk;
	unsigned int chunks_per_buffer;
	unsigned int bytes_per_buffer;
	unsigned int accum_end;
	unsigned int accum_length;
	unsigned int chunk_pos;
	unsigned int chunk_end;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	g_value_init(&val, G_TYPE_UINT);
	g_object_get_property(G_OBJECT(self), "bytes-per-chunk", &val);
	bytes_per_chunk = g_value_get_uint(&val);
	g_value_unset(&val);

	g_value_init(&val, G_TYPE_UINT);
	g_object_get_property(G_OBJECT(self), "chunks-per-buffer", &val);
	chunks_per_buffer = g_value_get_uint(&val);

	bytes_per_buffer = bytes_per_chunk * chunks_per_buffer;

	accum_end = event->completed;
	if (accum_end < priv->prev_offset)
		accum_end += bytes_per_buffer;

	accum_length = 0;
	while (TRUE) {
		unsigned int avail = accum_end - priv->prev_offset - accum_length;
		guint offset;
		const guint8 *frames;
		const guint32 *buf;
		guint32 iso_header;
		guint frame_size;
		unsigned int length;

		if (avail < 4)
			break;

		offset = (priv->prev_offset + accum_length) % bytes_per_buffer;
		hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self), offset,
					      4, &frames, &frame_size);

		buf = (const guint32 *)frames;
		iso_header = GUINT32_FROM_LE(buf[0]);
		length = (iso_header & 0xffff0000) >> 16;

		// In buffer-fill mode, payload is sandwitched by heading
		// isochronous header and trailing timestamp.
		length += 8;

		if (avail < length)
			break;

		// For next position.
		accum_length += length;
	}

	chunk_pos = priv->prev_offset / bytes_per_chunk;
	chunk_end = (priv->prev_offset + accum_length) / bytes_per_chunk;
	for (; chunk_pos < chunk_end; ++chunk_pos) {
		fw_iso_rx_multiple_register_chunk(self, FALSE, exception);
		if (*exception != NULL)
			return;
	}

	priv->prev_offset += accum_length;
	priv->prev_offset %= bytes_per_buffer;
}
