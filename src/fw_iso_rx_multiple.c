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
	struct fw_iso_ctx_state state;

	GByteArray *channels;

	guint prev_offset;

	struct ctx_payload *ctx_payloads;
	unsigned int ctx_payload_count;
	guint8 *concat_frames;

	guint chunks_per_irq;
	guint accumulated_chunk_count;
} HinokoFwIsoRxMultiplePrivate;

static void fw_iso_ctx_iface_init(HinokoFwIsoCtxInterface *iface);

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoRxMultiple, hinoko_fw_iso_rx_multiple, G_TYPE_OBJECT,
			G_ADD_PRIVATE(HinokoFwIsoRxMultiple)
			G_IMPLEMENT_INTERFACE(HINOKO_TYPE_FW_ISO_CTX, fw_iso_ctx_iface_init))

enum fw_iso_rx_multiple_prop_type {
	FW_ISO_RX_MULTIPLE_PROP_TYPE_CHANNELS = FW_ISO_CTX_PROP_TYPE_COUNT,
	FW_ISO_RX_MULTIPLE_PROP_TYPE_COUNT,
};

enum fw_iso_rx_multiple_sig_type {
	FW_ISO_RX_MULTIPLE_SIG_TYPE_IRQ = 1,
	FW_ISO_RX_MULTIPLE_SIG_TYPE_COUNT,
};
static guint fw_iso_rx_multiple_sigs[FW_ISO_RX_MULTIPLE_SIG_TYPE_COUNT] = { 0 };

static void fw_iso_rx_multiple_get_property(GObject *obj, guint id, GValue *val, GParamSpec *spec)
{
	HinokoFwIsoRxMultiple *self = HINOKO_FW_ISO_RX_MULTIPLE(obj);
	HinokoFwIsoRxMultiplePrivate *priv =
			hinoko_fw_iso_rx_multiple_get_instance_private(self);

	switch (id) {
	case FW_ISO_RX_MULTIPLE_PROP_TYPE_CHANNELS:
		g_value_set_static_boxed(val, priv->channels);
		break;
	default:
		fw_iso_ctx_state_get_property(&priv->state, obj, id, val, spec);
		break;
	}
}

static void fw_iso_rx_multiple_finalize(GObject *obj)
{
	HinokoFwIsoRxMultiple *self = HINOKO_FW_ISO_RX_MULTIPLE(obj);

	hinoko_fw_iso_rx_multiple_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_multiple_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_rx_multiple_class_init(HinokoFwIsoRxMultipleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_rx_multiple_get_property;
	gobject_class->finalize = fw_iso_rx_multiple_finalize;

	fw_iso_ctx_class_override_properties(gobject_class);

	g_object_class_install_property(gobject_class, FW_ISO_RX_MULTIPLE_PROP_TYPE_CHANNELS,
		g_param_spec_boxed("channels", "channels",
				   "The array with elements to represent "
				   "channels to be listened to",
				   G_TYPE_BYTE_ARRAY,
				   G_PARAM_READABLE));

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
	HinokoFwIsoRxMultiplePrivate *priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	fw_iso_ctx_state_init(&priv->state);
}

static void fw_iso_rx_multiple_stop(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoRxMultiple *self;
	HinokoFwIsoRxMultiplePrivate *priv;
	gboolean running;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(inst));
	self = HINOKO_FW_ISO_RX_MULTIPLE(inst);
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	running = priv->state.running;

	fw_iso_ctx_state_stop(&priv->state);

	if (priv->state.running != running)
		g_signal_emit_by_name(G_OBJECT(inst), "stopped", NULL);
}

static gboolean fw_iso_rx_multiple_get_cycle_timer(HinokoFwIsoCtx *inst, gint clock_id,
						 HinokoCycleTimer *const *cycle_timer,
						 GError **error)
{
	HinokoFwIsoRxMultiple *self;
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(inst), FALSE);
	self = HINOKO_FW_ISO_RX_MULTIPLE(inst);
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	return fw_iso_ctx_state_get_cycle_timer(&priv->state, clock_id, cycle_timer, error);
}

static gboolean fw_iso_rx_multiple_flush_completions(HinokoFwIsoCtx *inst, GError **error)
{
	HinokoFwIsoRxMultiple *self;
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(inst), FALSE);
	self = HINOKO_FW_ISO_RX_MULTIPLE(inst);
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	return fw_iso_ctx_state_flush_completions(&priv->state, error);
}

static gboolean fw_iso_rx_multiple_register_chunk(HinokoFwIsoRxMultiple *self, GError **error)
{
	HinokoFwIsoRxMultiplePrivate *priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);
	gboolean schedule_irq = FALSE;

	if (priv->chunks_per_irq > 0) {
		if (++priv->accumulated_chunk_count % priv->chunks_per_irq == 0)
			schedule_irq = TRUE;
		if (priv->accumulated_chunk_count >= G_MAXINT)
			priv->accumulated_chunk_count %= priv->chunks_per_irq;
	}

	return fw_iso_ctx_state_register_chunk(&priv->state, FALSE, 0, 0, NULL, 0, 0, schedule_irq,
					       error);
}

gboolean fw_iso_rx_multiple_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
					 GError **error)
{
	HinokoFwIsoRxMultiple *self;
	HinokoFwIsoRxMultiplePrivate *priv;

	const struct fw_cdev_event_iso_interrupt_mc *ev;
	unsigned int bytes_per_chunk;
	unsigned int chunks_per_buffer;
	unsigned int bytes_per_buffer;
	unsigned int accum_end;
	unsigned int accum_length;
	unsigned int chunk_pos;
	unsigned int chunk_end;
	struct ctx_payload *ctx_payload;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(inst), FALSE);
	g_return_val_if_fail(event->common.type == FW_CDEV_EVENT_ISO_INTERRUPT_MULTICHANNEL, FALSE);

	self = HINOKO_FW_ISO_RX_MULTIPLE(inst);
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	ev = &event->iso_interrupt_mc;

	bytes_per_chunk = priv->state.bytes_per_chunk;
	chunks_per_buffer = priv->state.chunks_per_buffer;
	bytes_per_buffer = bytes_per_chunk * chunks_per_buffer;

	accum_end = ev->completed;
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
		fw_iso_ctx_state_read_frame(&priv->state, offset, 4, &frames, &frame_size);

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
			ev->completed);

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
		if (!fw_iso_rx_multiple_register_chunk(self, error))
			return FALSE;
	}

	priv->prev_offset += accum_length;
	priv->prev_offset %= bytes_per_buffer;

	return fw_iso_ctx_state_queue_chunks(&priv->state, error);
}

gboolean fw_iso_rx_multiple_create_source(HinokoFwIsoCtx *inst, GSource **source, GError **error)
{
	HinokoFwIsoRxMultiple *self;
	HinokoFwIsoRxMultiplePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(inst), FALSE);
	self = HINOKO_FW_ISO_RX_MULTIPLE(inst);
	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	return fw_iso_ctx_state_create_source(&priv->state, inst, fw_iso_rx_multiple_handle_event,
					      source, error);
}

static void fw_iso_ctx_iface_init(HinokoFwIsoCtxInterface *iface)
{
	iface->stop = fw_iso_rx_multiple_stop;
	iface->get_cycle_timer = fw_iso_rx_multiple_get_cycle_timer;
	iface->flush_completions = fw_iso_rx_multiple_flush_completions;
	iface->create_source = fw_iso_rx_multiple_create_source;
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

	int i;
	struct fw_cdev_set_iso_channels set = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	g_return_if_fail(channels_length > 0);

	for (i = 0; i < channels_length; ++i) {
		g_return_if_fail(channels[i] < 64);

		set.channels |= G_GUINT64_CONSTANT(1) << channels[i];
	}

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	if (!fw_iso_ctx_state_allocate(&priv->state, path, HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE, 0,
				       0, 0, error))
		return;

	set.handle = priv->state.handle;
	if (ioctl(priv->state.fd, FW_CDEV_IOC_SET_ISO_CHANNELS, &set) < 0) {
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_SET_ISO_CHANNELS");
		hinoko_fw_iso_rx_multiple_release(self);
		return;
	} else if (set.channels == 0) {
		g_set_error_literal(error, HINOKO_FW_ISO_CTX_ERROR,
				    HINOKO_FW_ISO_CTX_ERROR_NO_ISOC_CHANNEL,
				    "No isochronous channel is available");
		hinoko_fw_iso_rx_multiple_release(self);
		return;
	}

	priv->channels = g_byte_array_new();
	for (i = 0; i < 64; ++i) {
		if (set.channels & (G_GUINT64_CONSTANT(1) << i))
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

	fw_iso_ctx_state_unmap_buffer(&priv->state);
	fw_iso_ctx_state_release(&priv->state);

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
	g_return_if_fail(error == NULL || *error == NULL);

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

	if (!fw_iso_ctx_state_map_buffer(&priv->state, bytes_per_chunk, chunks_per_buffer, error))
		return;

	bytes_per_chunk = priv->state.bytes_per_chunk;
	chunks_per_buffer = priv->state.chunks_per_buffer;

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

	hinoko_fw_iso_ctx_stop(HINOKO_FW_ISO_CTX(self));
	fw_iso_ctx_state_unmap_buffer(&priv->state);

	if (priv->concat_frames != NULL)
		free(priv->concat_frames);

	priv->concat_frames = NULL;
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
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	chunks_per_buffer = priv->state.chunks_per_buffer;
	g_return_if_fail(chunks_per_irq < chunks_per_buffer);

	priv->chunks_per_irq = chunks_per_irq;
	priv->accumulated_chunk_count = 0;

	for (i = 0; i < chunks_per_buffer; ++i) {
		if (!fw_iso_rx_multiple_register_chunk(self, error))
			return;
	}

	priv->prev_offset = 0;
	(void)fw_iso_ctx_state_start(&priv->state, cycle_match, sync, tags, error);
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
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_rx_multiple_get_instance_private(self);

	g_return_if_fail(index < priv->ctx_payload_count);
	ctx_payload = &priv->ctx_payloads[index];

	fw_iso_ctx_state_read_frame(&priv->state, ctx_payload->offset, ctx_payload->length, payload,
				    &frame_size);

	if (frame_size < ctx_payload->length) {
		unsigned int done = frame_size;
		unsigned int rest = ctx_payload->length - frame_size;

		memcpy(priv->concat_frames, *payload, done);
		fw_iso_ctx_state_read_frame(&priv->state, 0, rest, payload, &frame_size);
		g_return_if_fail(frame_size == rest);
		memcpy(priv->concat_frames + done, *payload, rest);

		*payload = priv->concat_frames;
	}

	*length = ctx_payload->length;
}
