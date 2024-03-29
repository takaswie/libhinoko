// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

/**
 * HinokoFwIsoIrSingle:
 * An object to receive isochronous packet for single channel.
 *
 * [class@FwIsoIrSingle] receives isochronous packets for single channel by packet-per-buffer
 * mode of IR context in 1394 OHCI. The content of packet is split to two parts; context header and
 * context payload in a manner of Linux FireWire subsystem.
 *
 */
typedef struct {
	struct fw_iso_ctx_state state;

	guint header_size;
	guint chunk_cursor;

	const struct fw_cdev_event_iso_interrupt *ev;
} HinokoFwIsoIrSinglePrivate;

static void fw_iso_ctx_iface_init(HinokoFwIsoCtxInterface *iface);

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoIrSingle, hinoko_fw_iso_ir_single, G_TYPE_OBJECT,
			G_ADD_PRIVATE(HinokoFwIsoIrSingle)
			G_IMPLEMENT_INTERFACE(HINOKO_TYPE_FW_ISO_CTX, fw_iso_ctx_iface_init))

enum fw_iso_ir_single_sig_type {
	FW_ISO_IR_SINGLE_SIG_TYPE_IRQ = 1,
	FW_ISO_IR_SINGLE_SIG_TYPE_COUNT,
};
static guint fw_iso_ir_single_sigs[FW_ISO_IR_SINGLE_SIG_TYPE_COUNT] = { 0 };

static void fw_iso_ir_single_get_property(GObject *obj, guint id, GValue *val, GParamSpec *spec)
{
	HinokoFwIsoIrSingle *self = HINOKO_FW_ISO_IR_SINGLE(obj);
	HinokoFwIsoIrSinglePrivate *priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	fw_iso_ctx_state_get_property(&priv->state, obj, id, val, spec);
}

static void fw_iso_ir_single_finalize(GObject *obj)
{
	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(obj));

	G_OBJECT_CLASS(hinoko_fw_iso_ir_single_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_ir_single_class_init(HinokoFwIsoIrSingleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_ir_single_get_property;
	gobject_class->finalize = fw_iso_ir_single_finalize;

	fw_iso_ctx_class_override_properties(gobject_class);

	/**
	 * HinokoFwIsoIrSingle::interrupted:
	 * @self: A [class@FwIsoIrSingle]
	 * @sec: sec part of isochronous cycle when interrupt occurs, up to 7.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs, up to 7999.
	 * @header: (array length=header_length) (element-type guint8): The headers of IR context
	 *	    for packets handled in the event of interrupt. The content is different
	 *	    depending on header_size parameter of [method@FwIsoIrSingle.allocate].
	 * @header_length: the number of bytes for @header.
	 * @count: the number of packets to handle.
	 *
	 * Emitted when Linux FireWire subsystem generates interrupt event. There are three cases
	 * for Linux FireWire subsystem to generate the event:
	 *
	 * - When 1394 OHCI hardware generates hardware interrupt as a result to process the
	 *   isochronous packet for the buffer chunk marked to generate hardware interrupt.
	 * - When the size of accumulated context header for packets since the last event reaches
	 *   the size of memory page (usually 4,096 bytes).
	 * - When application calls [method@FwIsoCtx.flush_completions] explicitly.
	 *
	 * The handler of signal can retrieve context payload of received packet by call of
	 * [method@FwIsoIrSingle.get_payload].
	 */
	fw_iso_ir_single_sigs[FW_ISO_IR_SINGLE_SIG_TYPE_IRQ] =
		g_signal_new("interrupted",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(HinokoFwIsoIrSingleClass, interrupted),
			NULL, NULL,
			hinoko_sigs_marshal_VOID__UINT_UINT_POINTER_UINT_UINT,
			G_TYPE_NONE,
			5, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER,
			G_TYPE_UINT, G_TYPE_UINT);
}

static void hinoko_fw_iso_ir_single_init(HinokoFwIsoIrSingle *self)
{
	HinokoFwIsoIrSinglePrivate *priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	fw_iso_ctx_state_init(&priv->state);
}

static void fw_iso_ir_single_stop(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;
	gboolean running;

	g_return_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst));
	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	running = priv->state.running;

	fw_iso_ctx_state_stop(&priv->state);

	if (priv->state.running != running)
		g_signal_emit_by_name(G_OBJECT(inst), STOPPED_SIGNAL_NAME, NULL);
}

static void fw_iso_ir_single_unmap_buffer(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst));
	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	fw_iso_ctx_state_unmap_buffer(&priv->state);
}

static void fw_iso_ir_single_release(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst));
	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	fw_iso_ctx_state_release(&priv->state);
}

static gboolean fw_iso_ir_single_read_cycle_time(HinokoFwIsoCtx *inst, gint clock_id,
						 HinawaCycleTime **cycle_time,
						 GError **error)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst), FALSE);
	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	return fw_iso_ctx_state_read_cycle_time(&priv->state, clock_id, cycle_time, error);
}

static gboolean fw_iso_ir_single_flush_completions(HinokoFwIsoCtx *inst, GError **error)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst), FALSE);
	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	return fw_iso_ctx_state_flush_completions(&priv->state, error);
}

gboolean fw_iso_ir_single_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				       GError **error)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;

	const struct fw_cdev_event_iso_interrupt *ev;
	guint sec;
	guint cycle;
	guint count;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst), FALSE);
	g_return_val_if_fail(event->common.type == FW_CDEV_EVENT_ISO_INTERRUPT, FALSE);

	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	ev = &event->iso_interrupt;
	sec = ohci1394_isoc_desc_tstamp_to_sec(ev->cycle);
	cycle = ohci1394_isoc_desc_tstamp_to_cycle(ev->cycle);
	count = ev->header_length / priv->header_size;

	// TODO; handling error?
	priv->ev = ev;
	g_signal_emit(self, fw_iso_ir_single_sigs[FW_ISO_IR_SINGLE_SIG_TYPE_IRQ], 0,
		      sec, cycle, ev->header, ev->header_length, count);
	priv->ev = NULL;

	priv->chunk_cursor += count;
	if (priv->chunk_cursor >= G_MAXINT)
		priv->chunk_cursor %= priv->state.chunks_per_buffer;

	return fw_iso_ctx_state_queue_chunks(&priv->state, error);
}

gboolean fw_iso_ir_single_create_source(HinokoFwIsoCtx *inst, GSource **source, GError **error)
{
	HinokoFwIsoIrSingle *self;
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(inst), FALSE);
	self = HINOKO_FW_ISO_IR_SINGLE(inst);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	return fw_iso_ctx_state_create_source(&priv->state, inst, fw_iso_ir_single_handle_event,
					      source, error);
}

static void fw_iso_ctx_iface_init(HinokoFwIsoCtxInterface *iface)
{
	iface->stop = fw_iso_ir_single_stop;
	iface->unmap_buffer = fw_iso_ir_single_unmap_buffer;
	iface->release = fw_iso_ir_single_release;
	iface->read_cycle_time = fw_iso_ir_single_read_cycle_time;
	iface->flush_completions = fw_iso_ir_single_flush_completions;
	iface->create_source = fw_iso_ir_single_create_source;
}

/**
 * hinoko_fw_iso_ir_single_new:
 *
 * Instantiate [class@FwIsoIrSingle] object and return the instance.
 *
 * Returns: an instance of [class@FwIsoIrSingle].
 */
HinokoFwIsoIrSingle *hinoko_fw_iso_ir_single_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_IR_SINGLE, NULL);
}

/**
 * hinoko_fw_iso_ir_single_allocate:
 * @self: A [class@FwIsoIrSingle].
 * @path: A path to any Linux FireWire character device.
 * @channel: An isochronous channel to listen, up to 63.
 * @header_size: The number of bytes for header of IR context, greater than 4 at least to include
 *		 isochronous packet header in header parameter of [signal@FwIsoIrSingle::interrupted].
 * @error: A [struct@GLib.Error].
 *
 * Allocate an IR context to 1394 OHCI hardware for packet-per-buffer mode. A local node of the
 * node corresponding to the given path is used as the hardware, thus any path is accepted as
 * long as process has enough permission for the path.
 *
 * The header_size parameter has an effect for the content of header parameter in
 * [signal@FwIsoIrSingle::interrupted]. When it's greater than 8, header includes the series of two
 * quadlets for isochronous packet header and timestamp per isochronous packet. When it's greater
 * than 12, header includes the part of isochronous packet data per packet.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_ir_single_allocate(HinokoFwIsoIrSingle *self, const char *path,
					  guint channel, guint header_size, GError **error)
{
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(self), FALSE);
	g_return_val_if_fail(channel <= IEEE1394_MAX_CHANNEL, FALSE);
	g_return_val_if_fail(header_size >= 4, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	if (!fw_iso_ctx_state_allocate(&priv->state, path, HINOKO_FW_ISO_CTX_MODE_IR_SINGLE, 0,
				       channel, header_size, error))
		return FALSE;

	priv->header_size = header_size;

	return TRUE;
}

/**
 * hinoko_fw_iso_ir_single_map_buffer:
 * @self: A [class@FwIsoIrSingle].
 * @maximum_bytes_per_payload: The maximum number of bytes per payload of IR context.
 * @payloads_per_buffer: The number of payload in buffer.
 * @error: A [struct@GLib.Error].
 *
 * Map intermediate buffer to share payload of IR context with 1394 OHCI hardware.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_ir_single_map_buffer(HinokoFwIsoIrSingle *self,
					    guint maximum_bytes_per_payload,
					    guint payloads_per_buffer, GError **error)
{
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	return fw_iso_ctx_state_map_buffer(&priv->state, maximum_bytes_per_payload,
					   payloads_per_buffer, error);
}

/**
 * hinoko_fw_iso_ir_single_register_packet:
 * @self: A [class@FwIsoIrSingle].
 * @schedule_interrupt: Whether to schedule hardware interrupt at isochronous cycle for the packet.
 * @error: A [struct@GLib.Error].
 *
 * Register chunk of buffer to process packet for future isochronous cycle. The caller can schedule
 * hardware interrupt to generate interrupt event. In detail, please refer to documentation about
 * [signal@FwIsoIrSingle::interrupted] signal.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_ir_single_register_packet(HinokoFwIsoIrSingle *self,
						 gboolean schedule_interrupt, GError **error)
{
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(self), FALSE);
	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	return fw_iso_ctx_state_register_chunk(&priv->state, FALSE, 0, 0, NULL, 0, 0,
					       schedule_interrupt, error);
}

/**
 * hinoko_fw_iso_ir_single_start:
 * @self: A [class@FwIsoIrSingle].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @sync_code: The value of sy field in isochronous packet header for packet processing, up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @error: A [struct@GLib.Error].
 *
 * Start IR context.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_ir_single_start(HinokoFwIsoIrSingle *self, const guint16 *cycle_match,
				       guint32 sync_code, HinokoFwIsoCtxMatchFlag tags, GError **error)
{
	HinokoFwIsoIrSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(self), FALSE);
	g_return_val_if_fail(cycle_match == NULL ||
			     cycle_match[0] <= OHCI1394_IR_contextMatch_cycleMatch_MAX_SEC ||
			     cycle_match[1] <= OHCI1394_IR_contextMatch_cycleMatch_MAX_CYCLE,
			     FALSE);
	g_return_val_if_fail(sync_code <= IEEE1394_MAX_SYNC_CODE, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	priv->chunk_cursor = 0;

	return fw_iso_ctx_state_start(&priv->state, cycle_match, sync_code, tags, error);
}

/**
 * hinoko_fw_iso_ir_single_get_payload:
 * @self: A [class@FwIsoIrSingle].
 * @index: the index inner available packets at the event of interrupt.
 * @payload: (element-type guint8)(array length=length)(out)(transfer none): The array with data
 *	     frame for payload of IR context.
 * @length: The number of bytes in the above payload.
 *
 * Retrieve payload of IR context for a handled packet corresponding to index at the event of
 * interrupt.
 *
 * Since: 0.7
 */
void hinoko_fw_iso_ir_single_get_payload(HinokoFwIsoIrSingle *self, guint index,
					 const guint8 **payload, guint *length)
{
	HinokoFwIsoIrSinglePrivate *priv;
	unsigned int bytes_per_chunk;
	unsigned int chunks_per_buffer;
	unsigned int pos;
	guint32 iso_header;
	guint offset;
	guint frame_size;

	g_return_if_fail(HINOKO_IS_FW_ISO_IR_SINGLE(self));

	priv = hinoko_fw_iso_ir_single_get_instance_private(self);

	g_return_if_fail(priv->ev != NULL);
	g_return_if_fail(index * priv->header_size <= priv->ev->header_length);

	bytes_per_chunk = priv->state.bytes_per_chunk;
	chunks_per_buffer = priv->state.chunks_per_buffer;

	pos = index * priv->header_size / 4;
	iso_header = GUINT32_FROM_BE(priv->ev->header[pos]);
	*length = ieee1394_iso_header_to_data_length(iso_header);
	if (priv->header_size > 8)
		*length -= priv->header_size - 8;
	if (*length > bytes_per_chunk)
		*length = bytes_per_chunk;

	index = (priv->chunk_cursor + index) % chunks_per_buffer;
	offset = index * bytes_per_chunk;
	fw_iso_ctx_state_read_frame(&priv->state, offset, *length, payload, &frame_size);
	g_return_if_fail(frame_size == *length);
}
