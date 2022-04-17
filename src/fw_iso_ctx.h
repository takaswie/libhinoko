// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_CTX_H__
#define __HINOKO_FW_ISO_CTX_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_CTX		(hinoko_fw_iso_ctx_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoCtx, hinoko_fw_iso_ctx, HINOKO, FW_ISO_CTX, GObject);

#define HINOKO_FW_ISO_CTX_ERROR		hinoko_fw_iso_ctx_error_quark()

GQuark hinoko_fw_iso_ctx_error_quark();

struct _HinokoFwIsoCtxClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoCtxClass::stopped:
	 * @self: A [class@FwIsoCtx].
	 * @error: (transfer none) (nullable): A [struct@GLib.Error].
	 *
	 * Class closure for the [signal@FwIsoCtx::stopped] signal.
	 */
	void (*stopped)(HinokoFwIsoCtx *self, const GError *error);
};


void hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self, gint clock_id,
				       HinokoCycleTimer *const *cycle_timer,
				       GError **error);

void hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **gsrc,
				     GError **error);

void hinoko_fw_iso_ctx_flush_completions(HinokoFwIsoCtx *self, GError **error);

G_END_DECLS

#endif
