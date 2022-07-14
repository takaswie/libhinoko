// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_CTX_H__
#define __HINOKO_FW_ISO_CTX_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_CTX		(hinoko_fw_iso_ctx_get_type())

G_DECLARE_INTERFACE(HinokoFwIsoCtx, hinoko_fw_iso_ctx, HINOKO, FW_ISO_CTX, GObject);

#define HINOKO_FW_ISO_CTX_ERROR		hinoko_fw_iso_ctx_error_quark()

GQuark hinoko_fw_iso_ctx_error_quark();

struct _HinokoFwIsoCtxInterface {
	GTypeInterface parent_iface;

	/**
	 * HinokoFwIsoCtxInterface::stop:
	 * @self: A [iface@FwIsoCtx].
	 *
	 * Virtual function to stop isochronous context.
	 *
	 * Since: 0.7.
	 */
	void (*stop)(HinokoFwIsoCtx *self);

	/**
	 *HinokoFwIsoCtxInterface::unmap_buffer:
	 * @self: A [iface@FwIsoCtx].
	 *
	 * Virtual function to unmap intermediate buffer shared with 1394 OHCI controller for the
	 * context.
	 *
	 * Since: 0.7.
	 */
	void (*unmap_buffer)(HinokoFwIsoCtx *self);

	/**
	 * HinokoFwIsoCtxInterface::release:
	 * @self: A [iface@FwIsoCtx].
	 *
	 * Virtual function to release the contest from 1394 OHCI controller.
	 *
	 * Since: 0.7.
	 */
	void (*release)(HinokoFwIsoCtx *self);

	/**
	 * HinokoFwIsoCtxInterface::get_cycle_timer:
	 * @self: A [iface@FwIsoCtx].
	 * @clock_id: The numeric ID of clock source for the reference timestamp. One CLOCK_REALTIME(0),
	 *	      CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(4) is available in UAPI of Linux kernel.
	 * @cycle_timer: (inout): A [struct@CycleTimer] to store data of cycle timer.
	 * @error: A [struct@GLib.Error].
	 *
	 * Virtual function to tetrieve the value of cycle timer register. This method call is
	 * available once any isochronous context is created.
	 *
	 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
	 *
	 * Since: 0.7.
	 */
	gboolean (*get_cycle_timer)(HinokoFwIsoCtx *self, gint clock_id,
				    HinokoCycleTimer *const *cycle_timer, GError **error);

	/**
	 * HinokoFwIsoCtxInterface::flush_completions:
	 * @self: A [iface@FwIsoCtx].
	 * @error: A [struct@GLib.Error].
	 *
	 * Virtual function to flush isochronous context until recent isochronous cycle. The call
	 * of function forces the context to queue any type of interrupt event for the recent
	 * isochronous cycle. Application can process the content of isochronous packet without
	 * waiting for actual hardware interrupt.
	 *
	 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
	 *
	 * Since: 0.7.
	 */
	gboolean (*flush_completions)(HinokoFwIsoCtx *self, GError **error);

	/**
	 * HinokoFwIsoCtxInterface::create_source:
	 * @self: A [iface@FwIsoCtx].
	 * @source: (out): A [struct@GLib.Source].
	 * @error: A [struct@GLib.Error].
	 *
	 * Virtual function to create [struct@GLib.Source] for [struct@GLib.MainContext] to
	 * dispatch events for isochronous context.
	 *
	 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
	 *
	 * Since: 0.7.
	 */
	gboolean (*create_source)(HinokoFwIsoCtx *self, GSource **source, GError **error);

	/**
	 * HinokoFwIsoCtxInterface::stopped:
	 * @self: A [iface@FwIsoCtx].
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error].
	 *
	 * Closure for the [signal@FwIsoCtx::stopped] signal.
	 */
	void (*stopped)(HinokoFwIsoCtx *self, const GError *error);
};

void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self);

void hinoko_fw_iso_ctx_unmap_buffer(HinokoFwIsoCtx *self);

void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self);

gboolean hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self, gint clock_id,
					   HinokoCycleTimer *const *cycle_timer, GError **error);

gboolean hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **source, GError **error);

gboolean hinoko_fw_iso_ctx_flush_completions(HinokoFwIsoCtx *self, GError **error);

G_END_DECLS

#endif
