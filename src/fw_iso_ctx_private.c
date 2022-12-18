// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define generate_file_error(error, code, format, arg)		\
	g_set_error(error, G_FILE_ERROR, code, format, arg)

typedef struct {
	GSource src;
	gpointer tag;
	unsigned int len;
	void *buf;
	HinokoFwIsoCtx *self;
	int fd;
	gboolean (*handle_event)(HinokoFwIsoCtx *self, const union fw_cdev_event *event,
				 GError **error);
} FwIsoCtxSource;

void fw_iso_ctx_class_override_properties(GObjectClass *gobject_class)
{
	g_object_class_override_property(gobject_class, FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK,
					 BYTES_PER_CHUNK_PROP_NAME);

	g_object_class_override_property(gobject_class, FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER,
					 CHUNKS_PER_BUFFER_PROP_NAME);
}

void fw_iso_ctx_state_get_property(const struct fw_iso_ctx_state *state, GObject *obj, guint id,
				   GValue *val, GParamSpec *spec)
{
	switch (id) {
	case FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK:
		g_value_set_uint(val, state->bytes_per_chunk);
		break;
	case FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER:
		g_value_set_uint(val, state->chunks_per_buffer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

/**
 * fw_iso_ctx_state_init:
 * @state: A [struct@FwIsoCtxState].
 *
 * Initialize structure for state of isochronous context.
 */
void fw_iso_ctx_state_init(struct fw_iso_ctx_state *state)
{
	state->fd = -1;
}

/**
 * fw_iso_ctx_state_allocate:
 * @state: A [struct@FwIsoCtxState].
 * @path: A path to any Linux FireWire character device.
 * @mode: The mode of context, one of [enum@FwIsoCtxMode] enumerations.
 * @scode: The speed of context, one of [enum@FwScode] enumerations.
 * @channel: The numeric channel of context up to 64.
 * @header_size: The number of bytes for header of isochronous context.
 * @error: A [struct@GLib.Error].
 *
 * Allocate a isochronous context to 1394 OHCI controller. A local node of the
 * node corresponding to the given path is used as the controller, thus any
 * path is accepted as long as process has enough permission for the path.
 */
gboolean fw_iso_ctx_state_allocate(struct fw_iso_ctx_state *state, const char *path,
				   HinokoFwIsoCtxMode mode, HinokoFwScode scode, guint channel,
				   guint header_size, GError **error)
{
	struct fw_cdev_get_info info = {0};
	struct fw_cdev_create_iso_context create = {0};

	g_return_val_if_fail(path != NULL && strlen(path) > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	// Linux firewire stack supports three types of isochronous context
	// described in 1394 OHCI specification.
	g_return_val_if_fail(mode == HINOKO_FW_ISO_CTX_MODE_IT ||
			     mode == HINOKO_FW_ISO_CTX_MODE_IR_SINGLE ||
			     mode == HINOKO_FW_ISO_CTX_MODE_IR_MULTIPLE, FALSE);

	g_return_val_if_fail(scode == HINOKO_FW_SCODE_S100 ||
			     scode == HINOKO_FW_SCODE_S200 ||
			     scode == HINOKO_FW_SCODE_S400 ||
			     scode == HINOKO_FW_SCODE_S800 ||
			     scode == HINOKO_FW_SCODE_S1600 ||
			     scode == HINOKO_FW_SCODE_S3200, FALSE);

	// IEEE 1394 specification supports isochronous channel up to 64.
	g_return_val_if_fail(channel <= IEEE1394_MAX_CHANNEL, FALSE);

	// Headers should be aligned to quadlet.
	g_return_val_if_fail(header_size % 4 == 0, FALSE);

	if (mode == HINOKO_FW_ISO_CTX_MODE_IR_SINGLE) {
		// At least, 1 quadlet is required for iso_header.
		g_return_val_if_fail(header_size >= 4, FALSE);
	} else if (mode == HINOKO_FW_ISO_CTX_MODE_IR_MULTIPLE) {
		// Needless.
		g_return_val_if_fail(header_size == 0, FALSE);
		g_return_val_if_fail(channel == 0, FALSE);
	}

	if (state->fd >= 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_ALLOCATED);
		return FALSE;
	}

	state->fd = open(path, O_RDWR);
	if  (state->fd < 0) {
		GFileError code = g_file_error_from_errno(errno);
		if (code != G_FILE_ERROR_FAILED)
			generate_file_error(error, code, "open(%s)", path);
		else
			generate_syscall_error(error, errno, "open(%s)", path);
		return FALSE;
	}

	// Support FW_CDEV_VERSION_AUTO_FLUSH_ISO_OVERFLOW.
	info.version = 5;
	if (ioctl(state->fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		generate_fw_iso_ctx_error_ioctl(error, errno, FW_CDEV_IOC_GET_INFO);
		fw_iso_ctx_state_release(state);
		return FALSE;
	}

	create.type = mode;
	create.channel = channel;
	create.speed = scode;
	create.header_size = header_size;

	if (ioctl(state->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create) < 0) {
		generate_fw_iso_ctx_error_ioctl(error, errno, FW_CDEV_IOC_CREATE_ISO_CONTEXT);
		fw_iso_ctx_state_release(state);
		return FALSE;
	}

	state->handle = create.handle;
	state->mode = mode;
	state->header_size = header_size;

	return TRUE;
}

/**
 * fw_iso_ctx_state_release:
 * @state: A [struct@FwIsoCtxState].
 *
 * Release allocated isochronous context from 1394 OHCI controller.
 */
void fw_iso_ctx_state_release(struct fw_iso_ctx_state *state)
{
	if (state->fd >= 0)
		close(state->fd);

	state->fd = -1;
}

/**
 * fw_iso_ctx_state_map_buffer:
 * @state: A [struct@FwIsoCtxState].
 * @bytes_per_chunk: The number of bytes per chunk in buffer going to be allocated.
 * @chunks_per_buffer: The number of chunks in buffer going to be allocated.
 * @error: A [struct@GLib.Error].
 *
 * Map intermediate buffer to share payload of isochronous context with 1394 OHCI controller.
 */
gboolean fw_iso_ctx_state_map_buffer(struct fw_iso_ctx_state *state, guint bytes_per_chunk,
				     guint chunks_per_buffer, GError **error)
{
	unsigned int datum_size;
	int prot;

	g_return_val_if_fail(bytes_per_chunk > 0, FALSE);
	g_return_val_if_fail(chunks_per_buffer > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (state->addr != NULL) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_MAPPED);
		return FALSE;
	}

	datum_size = sizeof(struct fw_cdev_iso_packet);
	if (state->mode == HINOKO_FW_ISO_CTX_MODE_IT)
		datum_size += state->header_size;

	state->data = g_malloc_n(chunks_per_buffer, datum_size);

	state->alloc_data_length = chunks_per_buffer * datum_size;

	prot = PROT_READ;
	if (state->mode == HINOKO_FW_ISO_CTX_MODE_IT)
		prot |= PROT_WRITE;

	// Align to size of page.
	state->addr = mmap(NULL, bytes_per_chunk * chunks_per_buffer, prot,
			  MAP_SHARED, state->fd, 0);
	if (state->addr == MAP_FAILED) {
		generate_syscall_error(error, errno,
				       "mmap(%d)", bytes_per_chunk * chunks_per_buffer);
		return FALSE;
	}

	state->bytes_per_chunk = bytes_per_chunk;
	state->chunks_per_buffer = chunks_per_buffer;

	return TRUE;
}

/**
 * hinoko_fw_iso_ctx_unmap_buffer:
 * @state: A [struct@FwIsoCtxState].
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload of isochronous context.
 */
void fw_iso_ctx_state_unmap_buffer(struct fw_iso_ctx_state *state)
{
	if (state->addr != NULL) {
		munmap(state->addr, state->bytes_per_chunk * state->chunks_per_buffer);
	}

	if (state->data != NULL)
		free(state->data);

	state->addr = NULL;
	state->data = NULL;
}

/**
 * fw_iso_ctx_state_register_chunk:
 * @state: A [struct@FwIsoCtxState].
 * @skip: Whether to skip packet transmission or not.
 * @tags: The value of tag field for isochronous packet to handle.
 * @sync_code: The value of sy field in isochronous packet header for packet processing, up to 15.
 * @header: (array length=header_length) (element-type guint8): The content of header for IT
 *	    context, nothing for IR context.
 * @header_length: The number of bytes for @header.
 * @payload_length: The number of bytes for payload of isochronous context.
 * @schedule_interrupt: schedule hardware interrupt at isochronous cycle for the chunk.
 * @error: A [struct@GLib.Error].
 *
 * Register data on buffer for payload of isochronous context.
 */
gboolean fw_iso_ctx_state_register_chunk(struct fw_iso_ctx_state *state, gboolean skip,
					 HinokoFwIsoCtxMatchFlag tags, guint sync_code,
					 const guint8 *header, guint header_length,
					 guint payload_length, gboolean schedule_interrupt,
					 GError **error)
{
	struct fw_cdev_iso_packet *datum;

	g_return_val_if_fail(skip == TRUE || skip == FALSE, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	g_return_val_if_fail(tags == 0 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 ||
			     tags == HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3, FALSE);

	g_return_val_if_fail(sync_code <= IEEE1394_MAX_SYNC_CODE, FALSE);

	if (state->mode == HINOKO_FW_ISO_CTX_MODE_IT) {
		if (!skip) {
			g_return_val_if_fail(header_length == state->header_size, FALSE);
			g_return_val_if_fail(payload_length <= state->bytes_per_chunk, FALSE);
		} else {
			g_return_val_if_fail(payload_length == 0, FALSE);
			g_return_val_if_fail(header_length == 0, FALSE);
			g_return_val_if_fail(header == NULL, FALSE);
		}
	} else if (state->mode == HINOKO_FW_ISO_CTX_MODE_IR_SINGLE ||
		   state->mode == HINOKO_FW_ISO_CTX_MODE_IR_MULTIPLE) {
		g_return_val_if_fail(tags == 0, FALSE);
		g_return_val_if_fail(sync_code == 0, FALSE);
		g_return_val_if_fail(header == NULL, FALSE);
		g_return_val_if_fail(header_length == 0, FALSE);
		g_return_val_if_fail(payload_length == 0, FALSE);
	}

	g_return_val_if_fail(
		state->data_length + sizeof(*datum) + header_length <= state->alloc_data_length,
		FALSE);

	if (state->fd < 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (state->addr == NULL) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED);
		return FALSE;
	}

	datum = (struct fw_cdev_iso_packet *)(state->data + state->data_length);
	state->data_length += sizeof(*datum) + header_length;
	++state->registered_chunk_count;

	if (state->mode == HINOKO_FW_ISO_CTX_MODE_IT) {
		if (!skip)
			memcpy(datum->header, header, header_length);
	} else {
		payload_length = state->bytes_per_chunk;

		if (state->mode == HINOKO_FW_ISO_CTX_MODE_IR_SINGLE)
			header_length = state->header_size;
	}

	datum->control =
		FW_CDEV_ISO_PAYLOAD_LENGTH(payload_length) |
		FW_CDEV_ISO_TAG(tags) |
		FW_CDEV_ISO_SY(sync_code) |
		FW_CDEV_ISO_HEADER_LENGTH(header_length);

	if (skip)
		datum->control |= FW_CDEV_ISO_SKIP;

	if (schedule_interrupt)
		datum->control |= FW_CDEV_ISO_INTERRUPT;

	return TRUE;
}

#define FW_CDEV_ISO_PACKET_CONTROL_HEADER_LENGTH_MASK	0xff000000
#define FW_CDEV_ISO_PACKET_CONTROL_HEADER_LENGTH_SHIFT	24
#define FW_CDEV_ISO_PACKET_CONTROL_PAYLOAD_MASK		0x0000ffff

static guint fw_cdev_iso_packet_control_to_header_length(guint control)
{
	return (control & FW_CDEV_ISO_PACKET_CONTROL_HEADER_LENGTH_MASK) >>
		FW_CDEV_ISO_PACKET_CONTROL_HEADER_LENGTH_SHIFT;
}

static guint fw_cdev_iso_packet_control_to_payload_length(guint control)
{
	return (control & FW_CDEV_ISO_PACKET_CONTROL_PAYLOAD_MASK);
}

/**
 * fw_iso_ctx_state_queue_chunks:
 * @state: A [struct@FwIsoCtxState].
 * @error: A [struct@GLib.Error].
 *
 * Queue registered chunks to 1394 OHCI controller.
 */
gboolean fw_iso_ctx_state_queue_chunks(struct fw_iso_ctx_state *state, GError **error)
{
	guint data_offset = 0;
	int chunk_count = 0;
	unsigned int bytes_per_buffer;
	guint buf_offset;

	bytes_per_buffer = state->bytes_per_chunk * state->chunks_per_buffer;
	buf_offset = state->curr_offset;

	while (data_offset < state->data_length) {
		struct fw_cdev_queue_iso arg = {0};
		guint buf_length = 0;
		guint data_length = 0;

		while (buf_offset + buf_length < bytes_per_buffer &&
		       data_offset + data_length < state->data_length) {
			struct fw_cdev_iso_packet *datum;
			guint payload_length;
			guint header_length;
			guint datum_length;

			datum = (struct fw_cdev_iso_packet *)
				(state->data + data_offset + data_length);
			payload_length = fw_cdev_iso_packet_control_to_payload_length(datum->control);
			header_length = fw_cdev_iso_packet_control_to_header_length(datum->control);

			if (buf_offset + buf_length + payload_length >
							bytes_per_buffer) {
				buf_offset = 0;
				break;
			}

			datum_length = sizeof(*datum);
			if (state->mode == HINOKO_FW_ISO_CTX_MODE_IT)
				datum_length += header_length;

			g_debug("%3d: %3d-%3d/%3d: %6d-%6d/%6d: %d",
				chunk_count,
				data_offset + data_length,
				data_offset + data_length + datum_length,
				state->alloc_data_length,
				buf_offset + buf_length,
				buf_offset + buf_length + payload_length,
				bytes_per_buffer,
				!!(datum->control & FW_CDEV_ISO_INTERRUPT));

			buf_length += payload_length;
			data_length += datum_length;
			++chunk_count;
		}

		arg.packets = (__u64)(state->data + data_offset);
		arg.size = data_length;
		arg.data = (__u64)(state->addr + buf_offset);
		arg.handle = state->handle;
		if (ioctl(state->fd, FW_CDEV_IOC_QUEUE_ISO, &arg) < 0) {
			generate_fw_iso_ctx_error_ioctl(error, errno, FW_CDEV_IOC_QUEUE_ISO);
			return FALSE;
		}

		g_debug("%3d: %3d-%3d/%3d: %6d-%6d/%6d",
			chunk_count,
			data_offset, data_offset + data_length,
			state->alloc_data_length,
			buf_offset, buf_offset + buf_length,
			bytes_per_buffer);

		buf_offset += buf_length;
		buf_offset %= bytes_per_buffer;

		data_offset += data_length;
	}

	g_warn_if_fail(chunk_count == state->registered_chunk_count);

	state->curr_offset = buf_offset;

	state->data_length = 0;
	state->registered_chunk_count = 0;

	return TRUE;
}

#define FW_CDEV_CYCLE_MATCH_SEC_MASK				0x00007000
#define FW_CDEV_CYCLE_MATCH_SEC_SHIFT				13
#define FW_CDEV_CYCLE_MATCH_CYCLE_MASK				0x00001fff

static guint fw_cdev_cycle_match_from_fields(guint sec, guint cycle)
{
	return ((sec << FW_CDEV_CYCLE_MATCH_SEC_SHIFT) & FW_CDEV_CYCLE_MATCH_SEC_MASK) |
	       (cycle & FW_CDEV_CYCLE_MATCH_CYCLE_MASK);
}

/**
 * fw_iso_ctx_state_start:
 * @state: A [struct@FwIsoCtxState].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @sync_code: The value of sy field in isochronous packet header for packet processing, up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @error: A [struct@GLib.Error].
 *
 * Start isochronous context.
 */
gboolean fw_iso_ctx_state_start(struct fw_iso_ctx_state *state, const guint16 *cycle_match,
				guint32 sync_code, HinokoFwIsoCtxMatchFlag tags, GError **error)
{
	struct fw_cdev_start_iso arg = {0};
	gint cycle;

	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (state->addr == NULL) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED);
		return FALSE;
	}

	if (state->mode == HINOKO_FW_ISO_CTX_MODE_IT) {
		g_return_val_if_fail(cycle_match == NULL ||
				cycle_match[0] <= OHCI1394_IT_contextControl_cycleMatch_MAX_SEC ||
				cycle_match[1] <= OHCI1394_IT_contextControl_cycleMatch_MAX_CYCLE,
				FALSE);
		g_return_val_if_fail(sync_code == 0, FALSE);
		g_return_val_if_fail(tags == 0, FALSE);
	} else {
		g_return_val_if_fail(cycle_match == NULL ||
				cycle_match[0] <= OHCI1394_IR_contextMatch_cycleMatch_MAX_SEC ||
				cycle_match[1] <= OHCI1394_IR_contextMatch_cycleMatch_MAX_CYCLE,
				FALSE);
		g_return_val_if_fail(sync_code <= IEEE1394_MAX_SYNC_CODE, FALSE);
		g_return_val_if_fail(tags <= (HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 |
					      HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 |
					      HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 |
					      HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3), FALSE);
	}

	if (cycle_match == NULL)
		cycle = -1;
	else
		cycle = fw_cdev_cycle_match_from_fields(cycle_match[0], cycle_match[1]);

	// Not prepared.
	if (state->data_length == 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED);
		return FALSE;
	}

	if (!fw_iso_ctx_state_queue_chunks(state, error))
		return FALSE;

	arg.sync = sync_code;
	arg.cycle = cycle;
	arg.tags = tags;
	arg.handle = state->handle;
	if (ioctl(state->fd, FW_CDEV_IOC_START_ISO, &arg) < 0) {
		generate_fw_iso_ctx_error_ioctl(error, errno, FW_CDEV_IOC_START_ISO);
		return FALSE;
	}

	state->running = TRUE;

	return TRUE;
}

/**
 * fw_iso_ctx_state_stop:
 * @state: A [struct@FwIsoCtxState].
 *
 * Stop isochronous context.
 */
void fw_iso_ctx_state_stop(struct fw_iso_ctx_state *state)
{
	struct fw_cdev_stop_iso arg = {0};

	if (!state->running)
		return;

	arg.handle = state->handle;
	ioctl(state->fd, FW_CDEV_IOC_STOP_ISO, &arg);

	state->running = FALSE;
	state->registered_chunk_count = 0;
	state->data_length = 0;
	state->curr_offset = 0;
}

/**
 * fw_iso_ctx_state_read_frames:
 * @state: A [struct@FwIsoCtxState].
 * @offset: offset from head of buffer.
 * @length: the number of bytes to read.
 * @frames: (array length=frame_size)(out)(transfer none)(nullable): The array to fill the same
 *	    data frame as @frame_size.
 * @frame_size: this value is for a case to truncate due to the end of buffer.
 *
 * Read frames to given buffer.
 */
void fw_iso_ctx_state_read_frame(struct fw_iso_ctx_state *state, guint offset, guint length,
				  const guint8 **frame, guint *frame_size)
{
	unsigned int bytes_per_buffer;

	bytes_per_buffer = state->bytes_per_chunk * state->chunks_per_buffer;
	if (offset > bytes_per_buffer) {
		*frame = NULL;
		*frame_size = 0;
		return;
	}

	*frame = state->addr + offset;

	if (offset + length < bytes_per_buffer)
		*frame_size = length;
	else
		*frame_size = bytes_per_buffer - offset;
}

/**
 * fw_iso_ctx_state_flush_completions:
 * @state: A [struct@FwIsoCtxState].
 * @error: A [struct@GLib.Error].
 *
 * Flush isochronous context until recent isochronous cycle. The call of function forces the
 * context to queue any type of interrupt event for the recent isochronous cycle. Application can
 * process the content of isochronous packet without waiting for actual hardware interrupt.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 */
gboolean fw_iso_ctx_state_flush_completions(struct fw_iso_ctx_state *state, GError **error)
{
	struct fw_cdev_flush_iso arg = {
		.handle = state->handle,
	};

	if (ioctl(state->fd, FW_CDEV_IOC_FLUSH_ISO, &arg) < 0) {
		generate_fw_iso_ctx_error_ioctl(error, errno, FW_CDEV_IOC_FLUSH_ISO);
		return FALSE;
	}

	return TRUE;
}

/**
 * fw_iso_ctx_state_get_cycle_timer:
 * @state: A [struct@FwIsoCtxState].
 * @clock_id: The numeric ID of clock source for the reference timestamp. One CLOCK_REALTIME(0),
 *	      CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(4) is available in UAPI of Linux kernel.
 * @cycle_timer: (inout): A [struct@CycleTimer] to store data of cycle timer.
 * @error: A [struct@GLib.Error].
 *
 * Retrieve the value of cycle timer register. This method call is available
 * once any isochronous context is created.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 */
gboolean fw_iso_ctx_state_get_cycle_timer(struct fw_iso_ctx_state *state, gint clock_id,
					  HinokoCycleTimer *const *cycle_timer, GError **error)
{
	g_return_val_if_fail(cycle_timer != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	(*cycle_timer)->clk_id = clock_id;
	if (ioctl(state->fd, FW_CDEV_IOC_GET_CYCLE_TIMER2, *cycle_timer) < 0) {
		generate_fw_iso_ctx_error_ioctl(error, errno, FW_CDEV_IOC_GET_CYCLE_TIMER2);
		return FALSE;
	}

	return TRUE;
}

static gboolean check_src(GSource *source)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)source;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(source, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *source, GSourceFunc cb, gpointer user_data)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)source;
	GIOCondition condition;
	GError *error = NULL;
	int len;
	const union fw_cdev_event *event;

	condition = g_source_query_unix_fd(source, src->tag);
	if (condition & G_IO_ERR)
		return G_SOURCE_REMOVE;

	len = read(src->fd, src->buf, src->len);
	if (len < 0) {
		if (errno != EAGAIN) {
			generate_file_error(&error, g_file_error_from_errno(errno),
					    "read %s", strerror(errno));
			goto error;
		}

		return G_SOURCE_CONTINUE;
	}

	event = (const union fw_cdev_event *)src->buf;
	if (!src->handle_event(src->self, event, &error))
		goto error;

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
error:
	hinoko_fw_iso_ctx_stop(src->self);
	g_clear_error(&error);
	return G_SOURCE_REMOVE;
}

static void finalize_src(GSource *source)
{
	FwIsoCtxSource *src = (FwIsoCtxSource *)source;

	g_free(src->buf);
	g_object_unref(src->self);
}

/**
 * fw_iso_ctx_state_create_source:
 * @state: A [struct@FwIsoCtxState].
 * @source: (out): A [struct@GLib.Source].
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * context.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 */
gboolean fw_iso_ctx_state_create_source(struct fw_iso_ctx_state *state, HinokoFwIsoCtx *inst,
					gboolean (*handle_event)(HinokoFwIsoCtx *inst,
								 const union fw_cdev_event *event,
								 GError **error),
					GSource **source, GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	FwIsoCtxSource *src;

	g_return_val_if_fail(HINOKO_IS_FW_ISO_CTX(inst), FALSE);
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_fw_iso_ctx_error_coded(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	*source = g_source_new(&funcs, sizeof(FwIsoCtxSource));

	g_source_set_name(*source, "HinokoFwIsoCtx");

	src = (FwIsoCtxSource *)(*source);

	if (state->mode != HINOKO_FW_ISO_CTX_MODE_IR_MULTIPLE) {
		// MEMO: Linux FireWire subsystem queues isochronous event
		// independently of interrupt flag when the same number of
		// bytes as one page is stored in the buffer of header. To
		// avoid truncated read, keep enough size.
		src->len = sizeof(struct fw_cdev_event_iso_interrupt) +
			   sysconf(_SC_PAGESIZE);
	} else {
		src->len = sizeof(struct fw_cdev_event_iso_interrupt_mc);
	}
	src->buf = g_malloc0(src->len);

	src->tag = g_source_add_unix_fd(*source, state->fd, G_IO_IN);
	src->fd = state->fd;
	src->self = g_object_ref(inst);
	src->handle_event = handle_event;

	return TRUE;
}
