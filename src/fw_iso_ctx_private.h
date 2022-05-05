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

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

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


gboolean fw_iso_ctx_state_allocate(struct fw_iso_ctx_state *state, const char *path,
				   HinokoFwIsoCtxMode mode, HinokoFwScode scode, guint channel,
				   guint header_size, GError **error);
void fw_iso_ctx_state_release(struct fw_iso_ctx_state *state);

#define STOPPED_SIGNAL_NEME		"stopped"

void hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				HinokoFwIsoCtxMode mode, HinokoFwScode scode,
				guint channel, guint header_size,
				GError **error);
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self);
void hinoko_fw_iso_ctx_map_buffer(HinokoFwIsoCtx *self, guint bytes_per_chunk,
				  guint chunks_per_buffer, GError **error);
void hinoko_fw_iso_ctx_unmap_buffer(HinokoFwIsoCtx *self);
void hinoko_fw_iso_ctx_register_chunk(HinokoFwIsoCtx *self, gboolean skip,
				      HinokoFwIsoCtxMatchFlag tags, guint sy,
				      const guint8 *header, guint header_length,
				      guint payload_length, gboolean schedule_interrupt,
				      GError **error);
void hinoko_fw_iso_ctx_set_rx_channels(HinokoFwIsoCtx *self,
				       guint64 *channel_flags,
				       GError **error);
void hinoko_fw_iso_ctx_start(HinokoFwIsoCtx *self, const guint16 *cycle_match, guint32 sync,
			     HinokoFwIsoCtxMatchFlag tags, GError **error);
void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self);
void hinoko_fw_iso_ctx_read_frames(HinokoFwIsoCtx *self, guint offset,
				   guint length, const guint8 **frames,
				   guint *frame_size);

gboolean fw_iso_rx_single_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				       GError **error);

gboolean fw_iso_rx_multiple_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
					 GError **error);

gboolean fw_iso_tx_handle_event(HinokoFwIsoCtx *inst, const union fw_cdev_event *event,
				GError **error);

#endif
