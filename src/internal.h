// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_INTERNAL_H__
#define __HINOKO_INTERNAL_H__

#include "hinoko.h"

gboolean hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				HinokoFwIsoCtxMode mode, HinokoFwScode scode, guint channel,
				guint header_size, GError **exception);
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self);
gboolean hinoko_fw_iso_ctx_map_buffer(HinokoFwIsoCtx *self, guint bytes_per_chunk,
				      guint chunks_per_buffer, GError **exception);
void hinoko_fw_iso_ctx_unmap_buffer(HinokoFwIsoCtx *self);
gboolean hinoko_fw_iso_ctx_register_chunk(HinokoFwIsoCtx *self, gboolean skip,
					  HinokoFwIsoCtxMatchFlag tags, guint sy,
					  const guint8 *header, guint header_length,
					  guint payload_length, gboolean schedule_interrupt,
					  GError **exception);
gboolean hinoko_fw_iso_ctx_set_rx_channels(HinokoFwIsoCtx *self, guint64 *channel_flags,
					   GError **exception);
void hinoko_fw_iso_ctx_start(HinokoFwIsoCtx *self, const guint16 *cycle_match, guint32 sync,
			     HinokoFwIsoCtxMatchFlag tags, GError **exception);
void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self);
void hinoko_fw_iso_ctx_read_frames(HinokoFwIsoCtx *self, guint offset,
				   guint length, const guint8 **frames,
				   guint *frame_size);

void hinoko_fw_iso_rx_single_handle_event(HinokoFwIsoRxSingle *self,
				struct fw_cdev_event_iso_interrupt *event,
				GError **exception);

void hinoko_fw_iso_rx_multiple_handle_event(HinokoFwIsoRxMultiple *self,
				struct fw_cdev_event_iso_interrupt_mc *event,
				GError **exception);

void hinoko_fw_iso_tx_handle_event(HinokoFwIsoTx *self,
				   struct fw_cdev_event_iso_interrupt *event,
				   GError **exception);

void hinoko_fw_iso_resource_ioctl(HinokoFwIsoResource *self,
				  unsigned long request, void *argp,
				  GError **exception);

void hinoko_fw_iso_resource_auto_handle_event(HinokoFwIsoResourceAuto *self,
					struct fw_cdev_event_iso_resource *ev);

#endif
