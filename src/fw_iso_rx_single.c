// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

/**
 * HinokoFwIsoRxSingle:
 * An object to receive isochronous packet for single channel.
 *
 * A [class@FwIsoRxSingle] receives isochronous packets for single channel by IR
 * context for packet-per-buffer mode in 1394 OHCI. The content of packet is
 * split to two parts; context header and context payload in a manner of Linux
 * FireWire subsystem.
 *
 */
typedef struct {
	struct fw_iso_ctx_state state;

	guint header_size;
	guint chunk_cursor;

	const struct fw_cdev_event_iso_interrupt *ev;
} HinokoFwIsoRxSinglePrivate;

static void fw_iso_ctx_class_init(HinokoFwIsoCtxClass *parent_class);

G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoRxSingle, hinoko_fw_iso_rx_single, HINOKO_TYPE_FW_ISO_CTX)

static void fw_iso_rx_single_finalize(GObject *obj)
{
	HinokoFwIsoRxSingle *self = HINOKO_FW_ISO_RX_SINGLE(obj);

	hinoko_fw_iso_rx_single_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_single_parent_class)->finalize(obj);
}

enum fw_iso_rx_single_sig_type {
	FW_ISO_RX_SINGLE_SIG_TYPE_IRQ = 1,
	FW_ISO_RX_SINGLE_SIG_TYPE_COUNT,
};
static guint fw_iso_rx_single_sigs[FW_ISO_RX_SINGLE_SIG_TYPE_COUNT] = { 0 };

static void hinoko_fw_iso_rx_single_class_init(HinokoFwIsoRxSingleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	HinokoFwIsoCtxClass *parent_class = HINOKO_FW_ISO_CTX_CLASS(klass);

	gobject_class->finalize = fw_iso_rx_single_finalize;

	fw_iso_ctx_class_init(parent_class);

	/**
	 * HinokoFwIsoRxSingle::interrupted:
	 * @self: A [class@FwIsoRxSingle]
	 * @sec: sec part of isochronous cycle when interrupt occurs.
	 * @cycle: cycle part of of isochronous cycle when interrupt occurs.
	 * @header: (array length=header_length) (element-type guint8): The headers of IR context
	 *	    for handled packets.
	 * @header_length: the number of bytes for @header.
	 * @count: the number of packets to handle.
	 *
	 * Emitted when Linux FireWire subsystem generates interrupt event. There are three cases
	 * for Linux FireWire subsystem to generate the event:
	 *
	 * - When OHCI 1394 controller generates hardware interrupt as a result to process the
	 *   isochronous packet for the buffer chunk marked to generate hardware interrupt.
	 * - When the size of accumulated context header for packets since the last event reaches
	 *   the size of memory page (usually 4,096 bytes).
	 * - When application calls [method@FwIsoCtx.flush_completions] explicitly.
	 *
	 * The handler of signal can retrieve context payload of received packet by call of
	 * [method@FwIsoRxSingle.get_payload].
	 */
	fw_iso_rx_single_sigs[FW_ISO_RX_SINGLE_SIG_TYPE_IRQ] =
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
	HinokoFwIsoRxSinglePrivate *priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	fw_iso_ctx_state_init(&priv->state);
}

static void fw_iso_rx_single_stop(HinokoFwIsoCtx *inst)
{
	HinokoFwIsoRxSingle *self;
	HinokoFwIsoRxSinglePrivate *priv;
	gboolean running;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(inst));
	self = HINOKO_FW_ISO_RX_SINGLE(inst);
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	running = priv->state.running;

	fw_iso_ctx_state_stop(&priv->state);

	if (priv->state.running != running)
		g_signal_emit_by_name(G_OBJECT(inst), "stopped", NULL);
}

static gboolean fw_iso_rx_single_get_cycle_timer(HinokoFwIsoCtx *inst, gint clock_id,
						 HinokoCycleTimer *const *cycle_timer,
						 GError **error)
{
	HinokoFwIsoRxSingle *self;
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(inst), FALSE);
	self = HINOKO_FW_ISO_RX_SINGLE(inst);
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	return fw_iso_ctx_state_get_cycle_timer(&priv->state, clock_id, cycle_timer, error);
}

static gboolean fw_iso_rx_single_flush_completions(HinokoFwIsoCtx *inst, GError **error)
{
	HinokoFwIsoRxSingle *self;
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(inst), FALSE);
	self = HINOKO_FW_ISO_RX_SINGLE(inst);
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	return fw_iso_ctx_state_flush_completions(&priv->state, error);
}

gboolean fw_iso_rx_single_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				       GError **error)
{
	HinokoFwIsoRxSingle *self;
	HinokoFwIsoRxSinglePrivate *priv;

	const struct fw_cdev_event_iso_interrupt *ev;
	guint sec;
	guint cycle;
	guint count;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(inst), FALSE);
	g_return_val_if_fail(event->common.type == FW_CDEV_EVENT_ISO_INTERRUPT, FALSE);

	self = HINOKO_FW_ISO_RX_SINGLE(inst);
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	ev = &event->iso_interrupt;
	sec = (ev->cycle & 0x0000e000) >> 13;
	cycle = ev->cycle & 0x00001fff;
	count = ev->header_length / priv->header_size;

	// TODO; handling error?
	priv->ev = ev;
	g_signal_emit(self, fw_iso_rx_single_sigs[FW_ISO_RX_SINGLE_SIG_TYPE_IRQ], 0,
		      sec, cycle, ev->header, ev->header_length, count);
	priv->ev = NULL;

	priv->chunk_cursor += count;
	if (priv->chunk_cursor >= G_MAXINT) {
		guint chunks_per_buffer;

		g_object_get(G_OBJECT(self),
			     "chunks-per-buffer", &chunks_per_buffer, NULL);

		priv->chunk_cursor %= chunks_per_buffer;
	}

	return TRUE;
}

gboolean fw_iso_rx_single_create_source(HinokoFwIsoCtx *inst, GSource **source, GError **error)
{
	HinokoFwIsoRxSingle *self;
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(inst), FALSE);
	self = HINOKO_FW_ISO_RX_SINGLE(inst);
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	return fw_iso_ctx_state_create_source(&priv->state, inst, fw_iso_rx_single_handle_event,
					      source, error);
}

static void fw_iso_ctx_class_init(HinokoFwIsoCtxClass *parent_class)
{
	parent_class->stop = fw_iso_rx_single_stop;
	parent_class->get_cycle_timer = fw_iso_rx_single_get_cycle_timer;
	parent_class->flush_completions = fw_iso_rx_single_flush_completions;
	parent_class->create_source = fw_iso_rx_single_create_source;
}

/**
 * hinoko_fw_iso_rx_single_new:
 *
 * Instantiate [class@FwIsoRxSingle] object and return the instance.
 *
 * Returns: an instance of [class@FwIsoRxSingle].
 */
HinokoFwIsoRxSingle *hinoko_fw_iso_rx_single_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RX_SINGLE, NULL);
}

/**
 * hinoko_fw_iso_rx_single_allocate:
 * @self: A [class@FwIsoRxSingle].
 * @path: A path to any Linux FireWire character device.
 * @channel: An isochronous channel to listen.
 * @header_size: The number of bytes for header of IR context.
 * @error: A [struct@GLib.Error].
 *
 * Allocate an IR context to 1394 OHCI controller for packet-per-buffer mode. A local node of the
 * node corresponding to the given path is used as the controller, thus any path is accepted as
 * long as process has enough permission for the path.
 */
void hinoko_fw_iso_rx_single_allocate(HinokoFwIsoRxSingle *self,
				      const char *path,
				      guint channel, guint header_size,
				      GError **error)
{
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				 HINOKO_FW_ISO_CTX_MODE_RX_SINGLE, 0,
				 channel, header_size, error);
	if (*error != NULL)
		return;

	priv->header_size = header_size;
}

/**
 * hinoko_fw_iso_rx_single_release:
 * @self: A [class@FwIsoRxSingle].
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
 * @self: A [class@FwIsoRxSingle].
 * @maximum_bytes_per_payload: The maximum number of bytes per payload of IR context.
 * @payloads_per_buffer: The number of payload in buffer.
 * @error: A [struct@GLib.Error].
 *
 * Map intermediate buffer to share payload of IR context with 1394 OHCI controller.
 */
void hinoko_fw_iso_rx_single_map_buffer(HinokoFwIsoRxSingle *self,
					guint maximum_bytes_per_payload,
					guint payloads_per_buffer,
					GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self),
				     maximum_bytes_per_payload,
				     payloads_per_buffer, error);
}

/**
 * hinoko_fw_iso_rx_single_unmap_buffer:
 * @self: A [class@FwIsoRxSingle].
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload of IR context.
 */
void hinoko_fw_iso_rx_single_unmap_buffer(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_rx_single_stop(self);
	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));
}

/**
 * hinoko_fw_iso_rx_single_register_packet:
 * @self: A [class@FwIsoRxSingle].
 * @schedule_interrupt: Whether to schedule hardware interrupt at isochronous cycle for the packet.
 * @error: A [struct@GLib.Error].
 *
 * Register chunk of buffer to process packet for future isochronous cycle. The caller can schedule
 * hardware interrupt to generate interrupt event. In detail, please refer to documentation about
 * [signal@FwIsoRxSingle::interrupted] signal.
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_rx_single_register_packet(HinokoFwIsoRxSingle *self, gboolean schedule_interrupt,
					     GError **error)
{
	hinoko_fw_iso_ctx_register_chunk(HINOKO_FW_ISO_CTX(self), FALSE, 0, 0, NULL, 0, 0,
					 schedule_interrupt, error);
}

/**
 * hinoko_fw_iso_rx_single_start:
 * @self: A [class@FwIsoRxSingle].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing, up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @error: A [struct@GLib.Error].
 *
 * Start IR context.
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_rx_single_start(HinokoFwIsoRxSingle *self, const guint16 *cycle_match,
				   guint32 sync, HinokoFwIsoCtxMatchFlag tags, GError **error)
{
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	priv->chunk_cursor = 0;

	hinoko_fw_iso_ctx_start(HINOKO_FW_ISO_CTX(self), cycle_match, sync, tags, error);
}

/**
 * hinoko_fw_iso_rx_single_stop:
 * @self: A [class@FwIsoRxSingle].
 *
 * Stop IR context.
 */
void hinoko_fw_iso_rx_single_stop(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_ctx_stop(HINOKO_FW_ISO_CTX(self));
}

/**
 * hinoko_fw_iso_rx_single_get_payload:
 * @self: A [class@FwIsoRxSingle].
 * @index: the index inner available packets.
 * @payload: (element-type guint8)(array length=length)(out)(transfer none): The array with data
 *	     frame for payload of IR context.
 * @length: The number of bytes in the above payload.
 * @error: A [struct@GLib.Error].
 *
 * Retrieve payload of IR context for a handled packet corresponding to index.
 */
void hinoko_fw_iso_rx_single_get_payload(HinokoFwIsoRxSingle *self, guint index,
					 const guint8 **payload, guint *length,
					 GError **error)
{
	HinokoFwIsoRxSinglePrivate *priv;
	unsigned int bytes_per_chunk;
	unsigned int chunks_per_buffer;
	unsigned int pos;
	guint32 iso_header;
	guint offset;
	guint frame_size;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	g_return_if_fail(priv->ev != NULL);
	g_return_if_fail(index * priv->header_size <= priv->ev->header_length);

	g_object_get(G_OBJECT(self),
		     "bytes-per-chunk", &bytes_per_chunk,
		     "chunks-per-buffer", &chunks_per_buffer, NULL);

	pos = index * priv->header_size / 4;
	iso_header = GUINT32_FROM_BE(priv->ev->header[pos]);
	*length = (iso_header & 0xffff0000) >> 16;
	if (*length > bytes_per_chunk)
		*length = bytes_per_chunk;

	index = (priv->chunk_cursor + index) % chunks_per_buffer;
	offset = index * bytes_per_chunk;
	hinoko_fw_iso_ctx_read_frames(HINOKO_FW_ISO_CTX(self), offset, *length,
				      payload, &frame_size);
	g_return_if_fail(frame_size == *length);
}
