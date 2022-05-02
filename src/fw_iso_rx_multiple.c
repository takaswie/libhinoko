// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

/**
 * HinokoFwIsoRxMultiple:
 * An object to receive isochronous packet for several channels.
 *
 * A [class@FwIsoRxMultiple] receives isochronous packets for several channels by IR context for
 * buffer-fill mode in 1394 OHCI.
 */
struct ctx_payload {
	unsigned int offset;
	unsigned int length;
};
typedef struct {
	GByteArray *channels;

	guint prev_offset;

	struct ctx_payload *ctx_payloads;
	unsigned int ctx_payload_count;
	guint8 *concat_frames;

	guint chunks_per_irq;
	guint accumulated_chunk_count;
} HinokoFwIsoRxMultiplePrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoRxMultiple, hinoko_fw_iso_rx_multiple, HINOKO_TYPE_FW_ISO_CTX)

enum fw_iso_rx_multiple_prop_type {
	FW_ISO_RX_MULTIPLE_PROP_TYPE_CHANNELS = 1,
	FW_ISO_RX_MULTIPLE_PROP_TYPE_COUNT,
};
static GParamSpec *fw_iso_rx_multiple_props[FW_ISO_RX_MULTIPLE_PROP_TYPE_COUNT] = { NULL, };

enum fw_iso_rx_multiple_sig_type {
	FW_ISO_RX_MULTIPLE_SIG_TYPE_IRQ = 1,
	FW_ISO_RX_MULTIPLE_SIG_TYPE_COUNT,
};
static guint fw_iso_rx_multiple_sigs[FW_ISO_RX_MULTIPLE_SIG_TYPE_COUNT] = { 0 };

static void fw_iso_rx_multiple_finalize(GObject *obj)
{
	HinokoFwIsoRxMultiple *self = HINOKO_FW_ISO_RX_MULTIPLE(obj);

	hinoko_fw_iso_rx_multiple_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_multiple_parent_class)->finalize(obj);
}

static void fw_iso_rx_multiple_get_property(GObject *obj, guint id, GValue *val,
					    GParamSpec *spec)
{
	HinokoFwIsoRxMultiple *self = HINOKO_FW_ISO_RX_MULTIPLE(obj);
	HinokoFwIsoRxMultiplePrivate *priv =
			hinoko_fw_iso_rx_multiple_get_instance_private(self);

	switch (id) {
	case FW_ISO_RX_MULTIPLE_PROP_TYPE_CHANNELS:
		g_value_set_static_boxed(val, priv->channels);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_iso_rx_multiple_set_property(GObject *obj, guint id,
					    const GValue *val, GParamSpec *spec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
}

static void hinoko_fw_iso_rx_multiple_class_init(HinokoFwIsoRxMultipleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_rx_multiple_finalize;
	gobject_class->get_property = fw_iso_rx_multiple_get_property;
	gobject_class->set_property = fw_iso_rx_multiple_set_property;

	fw_iso_rx_multiple_props[FW_ISO_RX_MULTIPLE_PROP_TYPE_CHANNELS] =
		g_param_spec_boxed("channels", "channels",
				   "The array with elements to represent "
				   "channels to be listened to",
				   G_TYPE_BYTE_ARRAY,
				   G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_ISO_RX_MULTIPLE_PROP_TYPE_COUNT,
					  fw_iso_rx_multiple_props);

	/**
	 * HinokoFwIsoRxMultiple::interrupted:
	 * @self: A [class@FwIsoRxMultiple].
	 * @count: The number of packets available in this interrupt.
	 *
	 * Emitted when Linux FireWire subsystem generates interrupt event. There are two cases
	 * for Linux FireWire subsystem to generate the event:
	 *
	 * - When OHCI 1394 controller generates hardware interrupt as a result to process the
	 *   isochronous packet for the buffer chunk marked to generate hardware interrupt.
	 * - When application calls [method@FwIsoCtx.flush_completions] explicitly.
	 *
	 * The handler of signal can retrieve the content of packet by call of
	 * [method@FwIsoRxMultiple.get_payload].
	 */
	fw_iso_rx_multiple_sigs[FW_ISO_RX_MULTIPLE_SIG_TYPE_IRQ] =
		g_signal_new("interrupted",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(HinokoFwIsoRxMultipleClass, interrupted),
			NULL, NULL,
			g_cclosure_marshal_VOID__UINT,
			G_TYPE_NONE,
			1, G_TYPE_UINT);
}

static void hinoko_fw_iso_rx_multiple_init(HinokoFwIsoRxMultiple *self)
{
	return;
}

/**
 * hinoko_fw_iso_rx_multiple_new:
 *
 * Instantiate [class@FwIsoRxMultiple] object and return the instance.
 *
 * Returns: an instance of [class@FwIsoRxMultiple].
 */
HinokoFwIsoRxMultiple *hinoko_fw_iso_rx_multiple_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RX_MULTIPLE, NULL);
}

/**
 * hinoko_fw_iso_rx_multiple_allocate:
 * @self: A [class@FwIsoRxMultiple].
 * @path: A path to any Linux FireWire character device.
 * @channels: (array length=channels_length) (element-type guint8): an array for channels to listen
 *	      to.
 * @channels_length: The length of channels.
 * @error: A [struct@GLib.Error].
 *
 * Allocate an IR context to 1394 OHCI controller for buffer-fill mode. A local node of the node
 * corresponding to the given path is used as the controller, thus any path is accepted as long as
 * process has enough permission for the path.
 */
void hinoko_fw_iso_rx_multiple_allocate(HinokoFwIsoRxMultiple *self,
					const char *path,
					const guint8 *channels,
					guint channels_length,
					GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv;

	guint64 channel_flags;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	g_return_if_fail(channels_length > 0);

	channel_flags = 0;
	for (i = 0; i < channels_length; ++i) {
		g_return_if_fail(channels[i] < 64);

		channel_flags |= G_GUINT64_CONSTANT(1) << channels[i];
	}

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				   HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE, 0, 0,
				   0, error);
	if (*error != NULL)
		return;

	hinoko_fw_iso_ctx_set_rx_channels(HINOKO_FW_ISO_CTX(self),
					  &channel_flags, error);
	if (*error != NULL)
		return;
	if (channel_flags == 0) {
		g_set_error_literal(error, HINOKO_FW_ISO_CTX_ERROR,
				    HINOKO_FW_ISO_CTX_ERROR_NO_ISOC_CHANNEL,
				    "No isochronous channel is available");
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
 * @self: A [class@FwIsoRxMultiple].
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
 * @self: A [class@FwIsoRxMultiple].
 * @bytes_per_chunk: The maximum number of bytes for payload of isochronous packet (not payload for
 *		     isochronous context).
 * @chunks_per_buffer: The number of chunks in buffer.
 * @error: A [struct@GLib.Error].
 *
 * Map an intermediate buffer to share payload of IR context with 1394 OHCI
 * controller.
 */
void hinoko_fw_iso_rx_multiple_map_buffer(HinokoFwIsoRxMultiple *self,
					  guint bytes_per_chunk,
					  guint chunks_per_buffer,
					  GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	if (priv->channels == NULL) {
		g_set_error_literal(error, HINOKO_FW_ISO_CTX_ERROR,
				    HINOKO_FW_ISO_CTX_ERROR_NO_ISOC_CHANNEL,
				    "No isochronous channel is available");
		return;
	}

	// The size of each chunk should be aligned to quadlet.
	bytes_per_chunk = (bytes_per_chunk + 3) / 4;
	bytes_per_chunk *= 4;

	priv->concat_frames = g_malloc_n(4, bytes_per_chunk);

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self), bytes_per_chunk,
				     chunks_per_buffer, error);

	g_object_get(G_OBJECT(self),
		     "bytes-per-chunk", &bytes_per_chunk,
		     "chunks-per-buffer", &chunks_per_buffer, NULL);

	priv->ctx_payloads = g_malloc_n(bytes_per_chunk * chunks_per_buffer / 8 / 2,
					sizeof(*priv->ctx_payloads));
}

/**
 * hinoko_fw_iso_rx_multiple_unmap_buffer:
 * @self: A [class@FwIsoRxMultiple].
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload of IR context.
 */
void hinoko_fw_iso_rx_multiple_unmap_buffer(HinokoFwIsoRxMultiple *self)
{
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	hinoko_fw_iso_rx_multiple_stop(self);

	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));

	if (priv->concat_frames != NULL)
		free(priv->concat_frames);

	priv->concat_frames = NULL;
}

static void fw_iso_rx_multiple_register_chunk(HinokoFwIsoRxMultiple *self,
					      GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);
	gboolean schedule_irq = FALSE;

	if (priv->chunks_per_irq > 0) {
		if (++priv->accumulated_chunk_count % priv->chunks_per_irq == 0)
			schedule_irq = TRUE;
		if (priv->accumulated_chunk_count >= G_MAXINT)
			priv->accumulated_chunk_count %= priv->chunks_per_irq;
	}

	hinoko_fw_iso_ctx_register_chunk(HINOKO_FW_ISO_CTX(self), FALSE, 0, 0, NULL, 0, 0,
					 schedule_irq, error);
}

/**
 * hinoko_fw_iso_rx_multiple_start:
 * @self: A [class@FwIsoRxMultiple].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing, up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @chunks_per_irq: The number of chunks per interval of interrupt. When 0 is given, application
 *		    should call [method@FwIsoCtx.flush_completions] voluntarily to generate
 *		    [signal@FwIsoRxMultiple::interrupted] event.
 * @error: A [struct@GLib.Error]
 *
 * Start IR context.
 */
void hinoko_fw_iso_rx_multiple_start(HinokoFwIsoRxMultiple *self,
				     const guint16 *cycle_match, guint32 sync,
				     HinokoFwIsoCtxMatchFlag tags,
				     guint chunks_per_irq, GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv;
	guint chunks_per_buffer;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	g_object_get(G_OBJECT(self), "chunks-per-buffer", &chunks_per_buffer, NULL);
	g_return_if_fail(chunks_per_irq < chunks_per_buffer);

	priv->chunks_per_irq = chunks_per_irq;
	priv->accumulated_chunk_count = 0;

	for (i = 0; i < chunks_per_buffer; ++i) {
		fw_iso_rx_multiple_register_chunk(self, error);
		if (*error != NULL)
			return;
	}

	priv->prev_offset = 0;
	hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, sync, tags, error);
}

/**
 * hinoko_fw_iso_rx_multiple_stop:
 * @self: A [class@FwIsoRxMultiple].
 *
 * Stop IR context.
 */
void hinoko_fw_iso_rx_multiple_stop(HinokoFwIsoRxMultiple *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));

	hinoko_fw_iso_ctx_stop(HINOKO_FW_ISO_CTX(self));
}

void hinoko_fw_iso_rx_multiple_handle_event(HinokoFwIsoRxMultiple *self,
				const struct fw_cdev_event_iso_interrupt_mc *event,
				GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv;
	unsigned int bytes_per_chunk;
	unsigned int chunks_per_buffer;
	unsigned int bytes_per_buffer;
	unsigned int accum_end;
	unsigned int accum_length;
	unsigned int chunk_pos;
	unsigned int chunk_end;
	struct ctx_payload *ctx_payload;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	g_object_get(G_OBJECT(self),
		     "bytes-per-chunk", &bytes_per_chunk,
		     "chunks-per-buffer", &chunks_per_buffer, NULL);

	bytes_per_buffer = bytes_per_chunk * chunks_per_buffer;

	accum_end = event->completed;
	if (accum_end < priv->prev_offset)
		accum_end += bytes_per_buffer;

	accum_length = 0;
	ctx_payload = priv->ctx_payloads;
	priv->ctx_payload_count = 0;
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

		g_debug("%3d: %6d %4d %6d",
			priv->ctx_payload_count, offset, length,
			event->completed);

		ctx_payload->offset = offset;
		ctx_payload->length = length;
		++ctx_payload;
		++priv->ctx_payload_count;

		// For next position.
		accum_length += length;
	}

	g_signal_emit(self,
		fw_iso_rx_multiple_sigs[FW_ISO_RX_MULTIPLE_SIG_TYPE_IRQ],
		0, priv->ctx_payload_count);

	chunk_pos = priv->prev_offset / bytes_per_chunk;
	chunk_end = (priv->prev_offset + accum_length) / bytes_per_chunk;
	for (; chunk_pos < chunk_end; ++chunk_pos) {
		fw_iso_rx_multiple_register_chunk(self, error);
		if (*error != NULL)
			return;
	}

	priv->prev_offset += accum_length;
	priv->prev_offset %= bytes_per_buffer;
}

/**
 * hinoko_fw_iso_rx_multiple_get_payload:
 * @self: A [class@FwIsoRxMultiple].
 * @index: the index of packet available in this interrupt.
 * @payload: (array length=length)(out)(transfer none): The array with data frame for payload of
 *	     IR context.
 * @length: The number of bytes in the above @payload.
 * @error: A [struct@GLib.Error].
 */
void hinoko_fw_iso_rx_multiple_get_payload(HinokoFwIsoRxMultiple *self,
					guint index, const guint8 **payload,
					guint *length, GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv;
	struct ctx_payload *ctx_payload;
	guint frame_size;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	g_return_if_fail(error != NULL && *error == NULL);

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	g_return_if_fail(index < priv->ctx_payload_count);
	ctx_payload = &priv->ctx_payloads[index];

	hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self),
				      ctx_payload->offset, ctx_payload->length,
				      payload, &frame_size);

	if (frame_size < ctx_payload->length) {
		unsigned int done = frame_size;
		unsigned int rest = ctx_payload->length - frame_size;

		memcpy(priv->concat_frames, *payload, done);
		hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self), 0, rest,
					      payload, &frame_size);
		g_return_if_fail(frame_size == rest);
		memcpy(priv->concat_frames + done, *payload, rest);

		*payload = priv->concat_frames;
	}

	*length = ctx_payload->length;
}
