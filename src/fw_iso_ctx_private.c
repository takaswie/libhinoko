// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

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
	g_return_val_if_fail(mode == HINOKO_FW_ISO_CTX_MODE_TX ||
			     mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE ||
			     mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE, FALSE);

	g_return_val_if_fail(scode == HINOKO_FW_SCODE_S100 ||
			     scode == HINOKO_FW_SCODE_S200 ||
			     scode == HINOKO_FW_SCODE_S400 ||
			     scode == HINOKO_FW_SCODE_S800 ||
			     scode == HINOKO_FW_SCODE_S1600 ||
			     scode == HINOKO_FW_SCODE_S3200, FALSE);

	// IEEE 1394 specification supports isochronous channel up to 64.
	g_return_val_if_fail(channel < 64, FALSE);

	// Headers should be aligned to quadlet.
	g_return_val_if_fail(header_size % 4 == 0, FALSE);

	if (mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE) {
		// At least, 1 quadlet is required for iso_header.
		g_return_val_if_fail(header_size >= 4, FALSE);
		g_return_val_if_fail(channel < 64, FALSE);
	} else if (mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		// Needless.
		g_return_val_if_fail(header_size == 0, FALSE);
		g_return_val_if_fail(channel == 0, FALSE);
	}

	if (state->fd >= 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_ALLOCATED);
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
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_INFO");
		fw_iso_ctx_state_release(state);
		return FALSE;
	}

	create.type = mode;
	create.channel = channel;
	create.speed = scode;
	create.header_size = header_size;

	if (ioctl(state->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create) < 0) {
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_CREATE_ISO_CONTEXT");
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
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (state->addr != NULL) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_MAPPED);
		return FALSE;
	}

	datum_size = sizeof(struct fw_cdev_iso_packet);
	if (state->mode == HINOKO_FW_ISO_CTX_MODE_TX)
		datum_size += state->header_size;

	state->data = g_malloc_n(chunks_per_buffer, datum_size);

	state->alloc_data_length = chunks_per_buffer * datum_size;

	prot = PROT_READ;
	if (state->mode == HINOKO_FW_ISO_CTX_MODE_TX)
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
 * @sy: The value of sy field for isochronous packet to handle.
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
					 HinokoFwIsoCtxMatchFlag tags, guint sy,
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

	g_return_val_if_fail(sy < 16, FALSE);

	if (state->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		if (!skip) {
			g_return_val_if_fail(header_length == state->header_size, FALSE);
			g_return_val_if_fail(payload_length <= state->bytes_per_chunk, FALSE);
		} else {
			g_return_val_if_fail(payload_length == 0, FALSE);
			g_return_val_if_fail(header_length == 0, FALSE);
			g_return_val_if_fail(header == NULL, FALSE);
		}
	} else if (state->mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE ||
		   state->mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		g_return_val_if_fail(tags == 0, FALSE);
		g_return_val_if_fail(sy == 0, FALSE);
		g_return_val_if_fail(header == NULL, FALSE);
		g_return_val_if_fail(header_length == 0, FALSE);
		g_return_val_if_fail(payload_length == 0, FALSE);
	}

	g_return_val_if_fail(
		state->data_length + sizeof(*datum) + header_length <= state->alloc_data_length,
		FALSE);

	if (state->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (state->addr == NULL) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED);
		return FALSE;
	}

	datum = (struct fw_cdev_iso_packet *)(state->data + state->data_length);
	state->data_length += sizeof(*datum) + header_length;
	++state->registered_chunk_count;

	if (state->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		if (!skip)
			memcpy(datum->header, header, header_length);
	} else {
		payload_length = state->bytes_per_chunk;

		if (state->mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE)
			header_length = state->header_size;
	}

	datum->control =
		FW_CDEV_ISO_PAYLOAD_LENGTH(payload_length) |
		FW_CDEV_ISO_TAG(tags) |
		FW_CDEV_ISO_SY(sy) |
		FW_CDEV_ISO_HEADER_LENGTH(header_length);

	if (skip)
		datum->control |= FW_CDEV_ISO_SKIP;

	if (schedule_interrupt)
		datum->control |= FW_CDEV_ISO_INTERRUPT;

	return TRUE;
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
			payload_length = datum->control & 0x0000ffff;
			header_length = (datum->control & 0xff000000) >> 24;

			if (buf_offset + buf_length + payload_length >
							bytes_per_buffer) {
				buf_offset = 0;
				break;
			}

			datum_length = sizeof(*datum);
			if (state->mode == HINOKO_FW_ISO_CTX_MODE_TX)
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
			generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_QUEUE_ISO");
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

/**
 * fw_iso_ctx_state_start:
 * @state: A [struct@FwIsoCtxState].
 * @cycle_match: (array fixed-size=2) (element-type guint16) (in) (nullable): The isochronous cycle
 *		 to start packet processing. The first element should be the second part of
 *		 isochronous cycle, up to 3. The second element should be the cycle part of
 *		 isochronous cycle, up to 7999.
 * @sync: The value of sync field in isochronous header for packet processing, up to 15.
 * @tags: The value of tag field in isochronous header for packet processing.
 * @error: A [struct@GLib.Error].
 *
 * Start isochronous context.
 */
gboolean fw_iso_ctx_state_start(struct fw_iso_ctx_state *state, const guint16 *cycle_match,
				guint32 sync, HinokoFwIsoCtxMatchFlag tags, GError **error)
{
	struct fw_cdev_start_iso arg = {0};
	gint cycle;

	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	if (state->addr == NULL) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED);
		return FALSE;
	}

	if (cycle_match == NULL) {
		cycle = -1;
	} else {
		g_return_val_if_fail(cycle_match[0] < 4, FALSE);
		g_return_val_if_fail(cycle_match[1] < 8000, FALSE);

		cycle = (cycle_match[0] << 13) | cycle_match[1];
	}

	if (state->mode == HINOKO_FW_ISO_CTX_MODE_TX) {
		g_return_val_if_fail(sync == 0, FALSE);
		g_return_val_if_fail(tags == 0, FALSE);
	} else {
		g_return_val_if_fail(sync < 16, FALSE);
		g_return_val_if_fail(tags <= (HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 |
					  HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 |
					  HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 |
					  HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3), FALSE);
	}

	// Not prepared.
	if (state->data_length == 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED);
		return FALSE;
	}

	if (!fw_iso_ctx_state_queue_chunks(state, error))
		return FALSE;

	arg.sync = sync;
	arg.cycle = cycle;
	arg.tags = tags;
	arg.handle = state->handle;
	if (ioctl(state->fd, FW_CDEV_IOC_START_ISO, &arg) < 0) {
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_START_ISO");
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
 */
gboolean fw_iso_ctx_state_flush_completions(struct fw_iso_ctx_state *state, GError **error)
{
	if (ioctl(state->fd, FW_CDEV_IOC_FLUSH_ISO) < 0) {
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_FLUSH_ISO");
		return FALSE;
	}

	return TRUE;
}

/**
 * fw_iso_ctx_state_get_cycle_timer:
 * @state: A [struct@FwIsoCtxState].
 * @clock_id: The numeric ID of clock source for the reference timestamp. One CLOCK_REALTIME(0),
 *	      CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(2) is available in UAPI of Linux kernel.
 * @cycle_timer: (inout): A [struct@CycleTimer] to store data of cycle timer.
 * @error: A [struct@GLib.Error].
 *
 * Retrieve the value of cycle timer register. This method call is available
 * once any isochronous context is created.
 */
gboolean fw_iso_ctx_state_get_cycle_timer(struct fw_iso_ctx_state *state, gint clock_id,
					  HinokoCycleTimer *const *cycle_timer, GError **error)
{
	g_return_val_if_fail(cycle_timer != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	if (state->fd < 0) {
		generate_local_error(error, HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED);
		return FALSE;
	}

	(*cycle_timer)->clk_id = clock_id;
	if (ioctl(state->fd, FW_CDEV_IOC_GET_CYCLE_TIMER2, *cycle_timer) < 0) {
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_CYCLE_TIMER2");
		return FALSE;
	}

	return TRUE;
}
