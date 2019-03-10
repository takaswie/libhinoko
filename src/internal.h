/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __HINOKO_INTERNAL_H__
#define __HINOKO_INTERNAL_H__

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

#include "fw_iso_ctx.h"

void hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				HinokoFwIsoCtxMode mode, HinokoFwScode scode,
				guint channel, guint header_size,
				GError **exception);
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self);

#endif
