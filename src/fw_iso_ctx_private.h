// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_CTX_PRIVATE_H__
#define __HINOKO_FW_ISO_CTX_PRIVATE_H__

#include "hinoko.h"

#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

extern const char *const fw_iso_ctx_err_msgs[7];

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINOKO_FW_ISO_CTX_ERROR, code, fw_iso_ctx_err_msgs[code])

#define generate_syscall_error(error, errno, format, arg)		\
	g_set_error(error, HINOKO_FW_ISO_CTX_ERROR,			\
		    HINOKO_FW_ISO_CTX_ERROR_FAILED,			\
		    format " %d(%s)", arg, errno, strerror(errno))

#define generate_ioctl_error(error, errno, request)			\
	generate_syscall_error(error, errno, "ioctl(%s)", #request)

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

#define IEEE1394_MAX_CHANNEL			63
#define IEEE1394_MAX_SYNC_CODE			15
#define IEEE1394_ISO_HEADER_DATA_LENGTH_MASK	0xffff0000
#define IEEE1394_ISO_HEADER_DATA_LENGTH_SHIFT	16

static inline guint ieee1394_iso_header_to_data_length(guint iso_header)
{
	return (iso_header & IEEE1394_ISO_HEADER_DATA_LENGTH_MASK) >>
		IEEE1394_ISO_HEADER_DATA_LENGTH_SHIFT;
}

#define OHCI1394_ISOC_DESC_timeStamp_SEC_MASK		0x0000e000
#define OHCI1394_ISOC_DESC_timeStamp_SEC_SHIFT		13
#define OHCI1394_ISOC_DESC_timeStmap_CYCLE_MASK		0x00001fff

static inline guint ohci1394_isoc_desc_tstamp_to_sec(guint32 tstamp)
{
	return (tstamp & OHCI1394_ISOC_DESC_timeStamp_SEC_MASK) >>
							OHCI1394_ISOC_DESC_timeStamp_SEC_SHIFT;
}

static inline guint ohci1394_isoc_desc_tstamp_to_cycle(guint32 tstamp)
{
	return (tstamp & OHCI1394_ISOC_DESC_timeStmap_CYCLE_MASK);
}

#define OHCI1394_IR_contextMatch_cycleMatch_MAX_SEC		3
#define OHCI1394_IR_contextMatch_cycleMatch_MAX_CYCLE		7999

#define OHCI1394_IT_contextControl_cycleMatch_MAX_SEC		3
#define OHCI1394_IT_contextControl_cycleMatch_MAX_CYCLE		7999

struct fw_iso_ctx_state {
	int fd;
	guint handle;

	HinokoFwIsoCtxMode mode;
	guint header_size;
	guchar *addr;
	guint bytes_per_chunk;
	guint chunks_per_buffer;

	// The number of entries equals to the value of chunks_per_buffer.
	guint8 *data;
	guint data_length;
	guint alloc_data_length;
	guint registered_chunk_count;

	guint curr_offset;
	gboolean running;
};

enum fw_iso_ctx_prop_type {
	FW_ISO_CTX_PROP_TYPE_BYTES_PER_CHUNK = 1,
	FW_ISO_CTX_PROP_TYPE_CHUNKS_PER_BUFFER,
	FW_ISO_CTX_PROP_TYPE_COUNT,
};

#define BYTES_PER_CHUNK_PROP_NAME		"bytes-per-chunk"
#define CHUNKS_PER_BUFFER_PROP_NAME		"chunks-per-buffer"

#define STOPPED_SIGNAL_NEME		"stopped"

void fw_iso_ctx_class_override_properties(GObjectClass *gobject_class);

void fw_iso_ctx_state_get_property(const struct fw_iso_ctx_state *state, GObject *obj, guint id,
				   GValue *val, GParamSpec *spec);

void fw_iso_ctx_state_init(struct fw_iso_ctx_state *state);

gboolean fw_iso_ctx_state_allocate(struct fw_iso_ctx_state *state, const char *path,
				   HinokoFwIsoCtxMode mode, HinokoFwScode scode, guint channel,
				   guint header_size, GError **error);
void fw_iso_ctx_state_release(struct fw_iso_ctx_state *state);

gboolean fw_iso_ctx_state_map_buffer(struct fw_iso_ctx_state *state, guint bytes_per_chunk,
				     guint chunks_per_buffer, GError **error);
void fw_iso_ctx_state_unmap_buffer(struct fw_iso_ctx_state *state);

gboolean fw_iso_ctx_state_register_chunk(struct fw_iso_ctx_state *state, gboolean skip,
					 HinokoFwIsoCtxMatchFlag tags, guint sync_code,
					 const guint8 *header, guint header_length,
					 guint payload_length, gboolean schedule_interrupt,
					 GError **error);
gboolean fw_iso_ctx_state_queue_chunks(struct fw_iso_ctx_state *state, GError **error);

gboolean fw_iso_ctx_state_start(struct fw_iso_ctx_state *state, const guint16 *cycle_match,
				guint32 sync_code, HinokoFwIsoCtxMatchFlag tags, GError **error);
void fw_iso_ctx_state_stop(struct fw_iso_ctx_state *state);

void fw_iso_ctx_state_read_frame(struct fw_iso_ctx_state *state, guint offset, guint length,
				  const guint8 **frame, guint *frame_size);

gboolean fw_iso_ctx_state_flush_completions(struct fw_iso_ctx_state *state, GError **error);

gboolean fw_iso_ctx_state_get_cycle_timer(struct fw_iso_ctx_state *state, gint clock_id,
					  HinokoCycleTimer *const *cycle_timer, GError **error);

gboolean fw_iso_ctx_state_create_source(struct fw_iso_ctx_state *state, HinokoFwIsoCtx *inst,
					gboolean (*handle_event)(HinokoFwIsoCtx *inst,
								 const union fw_cdev_event *event,
								 GError **error),
					GSource **source, GError **error);

#endif
