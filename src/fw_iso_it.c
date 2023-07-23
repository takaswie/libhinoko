// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

/**
 * HinokoFwIsoIt:
 * An object to transmit isochronous packet for single channel.
 *
 * A [class@FwIsoIt] transmits isochronous packets for single channel by IT context in 1394 OHCI.
 * The content of packet is split to two parts; context header and context payload in a manner of
 * Linux FireWire subsystem.
 */
typedef struct {
	struct fw_iso_ctx_state state;
	guint offset;
} HinokoFwIsoItPrivate;

static void fw_iso_ctx_iface_init(HinokoFwIsoCtxInterface *iface);

G_DEFINE_TYPE_WITH_CODE(HinokoFwIsoIt, hinoko_fw_iso_it, G_TYPE_OBJECT,
			G_ADD_PRIVATE(HinokoFwIsoIt)
			G_IMPLEMENT_INTERFACE(HINOKO_TYPE_FW_ISO_CTX, fw_iso_ctx_iface_init))

enum fw_iso_it_sig_type {
	FW_ISO_IT_SIG_TYPE_IRQ = 1,
	FW_ISO_IT_SIG_TYPE_COUNT,
};
static guint fw_iso_it_sigs[FW_ISO_IT_SIG_TYPE_COUNT] = { 0 };

static void fw_iso_it_get_property(GObject *obj, guint id, GValue *val, GParamSpec *spec)
{
	HinokoFwIsoIt *self = HINOKO_FW_ISO_IT(obj);
	HinokoFwIsoItPrivate *priv = hinoko_fw_iso_it_get_instance_private(self);

	fw_iso_ctx_state_get_property(&priv->state, obj, id, val, spec);
}

static void fw_iso_it_finalize(GObject *obj)
{
	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(obj));

	G_OBJECT_CLASS(hinoko_fw_iso_it_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_it_class_init(HinokoFwIsoItClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_iso_it_get_property;
	gobject_class->finalize = fw_iso_it_finalize;

	fw_iso_ctx_class_override_properties(gobject_class);

	/**
	 * HinokoFwIsoIt::interrupted:
	 * @self: A [class@FwIsoIt].
	 * @sec: sec part of isochronous cycle when interrupt occurs, up to 7.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs, up to 7999.
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
	fw_iso_it_sigs[FW_ISO_IT_SIG_TYPE_IRQ] =
		g_signal_new("interrupted",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(HinokoFwIsoItClass, interrupted),
			NULL, NULL,
			hinoko_sigs_marshal_VOID__UINT_UINT_POINTER_UINT_UINT,
			G_TYPE_NONE,
			5, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER,
			G_TYPE_UINT, G_TYPE_UINT);
}

static void hinoko_fw_iso_it_init(HinokoFwIsoIt *self)
{
	HinokoFwIsoItPrivate *priv = hinoko_fw_iso_it_get_instance_private(self);

	fw_iso_ctx_state_init(&priv->state);
}

static void fw_iso_it_stop(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;
	gboolean running;

	g_return_if_fail(HINOKO_IS_FW_ISO_IT(inst));
	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	running = priv->state.running;

	fw_iso_ctx_state_stop(&priv->state);

	if (priv->state.running != running)
		g_signal_emit_by_name(G_OBJECT(inst), STOPPED_SIGNAL_NAME, NULL);
}

void fw_iso_it_unmap_buffer(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_IT(inst));
	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	fw_iso_ctx_state_unmap_buffer(&priv->state);
}

static void fw_iso_it_release(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_IT(inst));
	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	fw_iso_ctx_state_release(&priv->state);
}

static gboolean fw_iso_it_read_cycle_time(HinokoFwIsoCtx *inst, gint clock_id,
					  HinawaCycleTime *const *cycle_time, GError **error)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(inst), FALSE);
	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	return fw_iso_ctx_state_read_cycle_time(&priv->state, clock_id, cycle_time, error);
}

static gboolean fw_iso_it_flush_completions(HinokoFwIsoCtx *inst, GError **error)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(inst), FALSE);
	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	return fw_iso_ctx_state_flush_completions(&priv->state, error);
}

gboolean fw_iso_it_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				GError **error)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;

	const struct fw_cdev_event_iso_interrupt *ev;
	guint sec;
	guint cycle;
	unsigned int pkt_count;

	g_return_val_if_fail(HINOKO_FW_ISO_IT(inst), FALSE);
	g_return_val_if_fail(event->common.type == FW_CDEV_EVENT_ISO_INTERRUPT, FALSE);

	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	ev = &event->iso_interrupt;

	sec = ohci1394_isoc_desc_tstamp_to_sec(ev->cycle);
	cycle = ohci1394_isoc_desc_tstamp_to_cycle(ev->cycle);
	pkt_count = ev->header_length / 4;

	g_signal_emit(inst, fw_iso_it_sigs[FW_ISO_IT_SIG_TYPE_IRQ], 0, sec, cycle, ev->header,
		      ev->header_length, pkt_count);

	return fw_iso_ctx_state_queue_chunks(&priv->state, error);
}

gboolean fw_iso_it_create_source(HinokoFwIsoCtx *inst, GSource **source, GError **error)
{
	HinokoFwIsoIt *self;
	HinokoFwIsoItPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(inst), FALSE);
	self = HINOKO_FW_ISO_IT(inst);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	return fw_iso_ctx_state_create_source(&priv->state, inst, fw_iso_it_handle_event, source,
					      error);
}

static void fw_iso_ctx_iface_init(HinokoFwIsoCtxInterface *iface)
{
	iface->stop = fw_iso_it_stop;
	iface->unmap_buffer = fw_iso_it_unmap_buffer;
	iface->release = fw_iso_it_release;
	iface->read_cycle_time = fw_iso_it_read_cycle_time;
	iface->flush_completions = fw_iso_it_flush_completions;
	iface->create_source = fw_iso_it_create_source;
}

/**
 * hinoko_fw_iso_it_new:
 *
 * Instantiate [class@FwIsoIt] object and return the instance.
 *
 * Returns: an instance of [class@FwIsoIt].
 */
HinokoFwIsoIt *hinoko_fw_iso_it_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_IT, NULL);
}

/**
 * hinoko_fw_iso_it_allocate:
 * @self: A [class@FwIsoIt].
 * @path: A path to any Linux FireWire character device.
 * @scode: A [enum@FwScode] to indicate speed of isochronous communication.
 * @channel: An isochronous channel to transfer, up to 63.
 * @header_size: The number of bytes for header of IT context.
 * @error: A [struct@GLib.Error].
 *
 * Allocate an IT context to 1394 OHCI controller. A local node of the node corresponding to the
 * given path is used as the controller, thus any path is accepted as long as process has enough
 * permission for the path.
 *
 * Returns: TRUE if the overall operation finishes successful, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_it_allocate(HinokoFwIsoIt *self, const char *path, HinokoFwScode scode,
				   guint channel, guint header_size, GError **error)
{
	HinokoFwIsoItPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(self), FALSE);
	g_return_val_if_fail(channel <= IEEE1394_MAX_CHANNEL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	return fw_iso_ctx_state_allocate(&priv->state, path, HINOKO_FW_ISO_CTX_MODE_IT, scode,
					 channel, header_size, error);
}

/**
 * hinoko_fw_iso_it_map_buffer:
 * @self: A [class@FwIsoIt].
 * @maximum_bytes_per_payload: The number of bytes for payload of IT context.
 * @payloads_per_buffer: The number of payloads of IT context in buffer.
 * @error: A [struct@GLib.Error].
 *
 * Map intermediate buffer to share payload of IT context with 1394 OHCI controller.
 *
 * Returns: TRUE if the overall operation finishes successful, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_it_map_buffer(HinokoFwIsoIt *self, guint maximum_bytes_per_payload,
				     guint payloads_per_buffer, GError **error)
{
	HinokoFwIsoItPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	return fw_iso_ctx_state_map_buffer(&priv->state, maximum_bytes_per_payload,
					   payloads_per_buffer, error);
}

/**
 * hinoko_fw_iso_it_start:
 * @self: A [class@FwIsoIt].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @error: A [struct@GLib.Error].
 *
 * Start IT context.
 *
 * Returns: TRUE if the overall operation finishes successful, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_it_start(HinokoFwIsoIt *self, const guint16 *cycle_match, GError **error)
{
	HinokoFwIsoItPrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(self), FALSE);
	g_return_val_if_fail(cycle_match == NULL ||
			     cycle_match[0] <= OHCI1394_IT_contextControl_cycleMatch_MAX_SEC ||
			     cycle_match[1] <= OHCI1394_IT_contextControl_cycleMatch_MAX_CYCLE,
			     FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
	priv = hinoko_fw_iso_it_get_instance_private(self);

	return fw_iso_ctx_state_start(&priv->state, cycle_match, 0, 0, error);
}

/**
 * hinoko_fw_iso_it_register_packet:
 * @self: A [class@FwIsoIt].
 * @tags: The value of tag field for isochronous packet to register.
 * @sync_code: The value of sync field in isochronous packet header for packet processing, up to 15.
 * @header: (array length=header_length) (nullable): The header of IT context for isochronous
 *	    packet. The length of header should be the same as the size of header indicated in
 *	    allocation if it's not null.
 * @header_length: The number of bytes for the @header.
 * @payload: (array length=payload_length)(nullable): The payload of IT context for isochronous
 *	     packet.
 * @payload_length: The number of bytes for the @payload.
 * @schedule_interrupt: Whether to schedule hardware interrupt at isochronous cycle for the packet.
 * @error: A [struct@GLib.Error].
 *
 * Register packet data with header and payload for IT context. The content of given header and
 * payload is appended into data field of isochronous packet to be sent. The caller can schedule
 * hardware interrupt to generate interrupt event. In detail, please refer to documentation about
 * [signal@FwIsoIt::interrupted].
 *
 * Returns: TRUE if the overall operation finishes successful, otherwise FALSE.
 *
 * Since: 0.7
 */
gboolean hinoko_fw_iso_it_register_packet(HinokoFwIsoIt *self, HinokoFwIsoCtxMatchFlag tags,
					  guint sync_code,
					  const guint8 *header, guint header_length,
					  const guint8 *payload, guint payload_length,
					  gboolean schedule_interrupt, GError **error)
{
	HinokoFwIsoItPrivate *priv;
	const guint8 *frames;
	guint frame_size;
	gboolean skip;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_IT(self), FALSE);
	g_return_val_if_fail(sync_code <= IEEE1394_MAX_SYNC_CODE, FALSE);
	g_return_val_if_fail((header != NULL && header_length > 0) ||
			     (header == NULL && header_length == 0), FALSE);
	g_return_val_if_fail((payload != NULL && payload_length > 0) ||
			     (payload == NULL && payload_length == 0), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinoko_fw_iso_it_get_instance_private(self);

	skip = FALSE;
	if (header_length == 0 && payload_length == 0)
		skip = TRUE;

	if (!fw_iso_ctx_state_register_chunk(&priv->state, skip, tags, sync_code,
					     header, header_length, payload_length,
					     schedule_interrupt, error))
		return FALSE;

	fw_iso_ctx_state_read_frame(&priv->state, priv->offset, payload_length, &frames,
				    &frame_size);
	memcpy((void *)frames, payload, frame_size);
	priv->offset += frame_size;

	if (frame_size != payload_length) {
		guint rest = payload_length - frame_size;

		payload += frame_size;
		fw_iso_ctx_state_read_frame(&priv->state, 0, rest, &frames, &frame_size);
		memcpy((void *)frames, payload, frame_size);

		priv->offset = frame_size;
	}

	return TRUE;
}
