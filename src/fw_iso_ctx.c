// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

#include <sys/mman.h>

/**
 * HinokoFwIsoCtx
 * An basic interface to operate isochronous context on 1394 OHCI controller.
 *
 * A [iface@FwIsoCtx] is an basic interface to use UAPI of Linux FireWire subsystem to operate
 * 1394 OHCI controller.
 */

G_DEFINE_INTERFACE(HinokoFwIsoCtx, hinoko_fw_iso_ctx, G_TYPE_OBJECT)

/**
 * hinoko_fw_iso_ctx_error_quark:
 *
 * Return the [alias@GLib.Quark] for error domain of [struct@GLib.Error] which has code in
 * Hinoko.FwIsoCtxError.
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinoko-fw-iso-ctx-error-quark, hinoko_fw_iso_ctx_error)

const char *const fw_iso_ctx_err_msgs[7] = {
	[HINOKO_FW_ISO_CTX_ERROR_ALLOCATED] =
		"The instance is already associated to any firewire character device",
	[HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED] =
		"The instance is not associated to any firewire character device",
	[HINOKO_FW_ISO_CTX_ERROR_MAPPED] =
		"The intermediate buffer is already mapped to the process",
	[HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED] =
		"The intermediate buffer is not mapped to the process",
	[HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED] = "No chunk registered before starting",
};

static void hinoko_fw_iso_ctx_default_init(HinokoFwIsoCtxInterface *iface)
{
	g_object_interface_install_property(iface,
		g_param_spec_uint(BYTES_PER_CHUNK_PROP_NAME, "bytes-per-chunk",
				  "The number of bytes for chunk in buffer.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));

	g_object_interface_install_property(iface,
		g_param_spec_uint(CHUNKS_PER_BUFFER_PROP_NAME, "chunks-per-buffer",
				  "The number of chunks in buffer.",
				  0, G_MAXUINT, 0,
				  G_PARAM_READABLE));

	/**
	 * HinokoFwIsoCtx::stopped:
	 * @self: A [iface@FwIsoCtx].
	 * @error: (transfer none) (nullable) (in): A [struct@GLib.Error].
	 *
	 * Emitted when isochronous context is stopped.
	 */
	g_signal_new(STOPPED_SIGNAL_NEME,
		G_TYPE_FROM_INTERFACE(iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(HinokoFwIsoCtxInterface, stopped),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1, G_TYPE_ERROR);
}

/**
 * hinoko_fw_iso_ctx_get_cycle_timer:
 * @self: A [iface@FwIsoCtx].
 * @clock_id: The numeric ID of clock source for the reference timestamp. One CLOCK_REALTIME(0),
 *	      CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(2) is available in UAPI of Linux kernel.
 * @cycle_timer: (inout): A [struct@CycleTimer] to store data of cycle timer.
 * @error: A [struct@GLib.Error].
 *
 * Retrieve the value of cycle timer register. This method call is available
 * once any isochronous context is created.
 */
void hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self, gint clock_id,
				       HinokoCycleTimer *const *cycle_timer,
				       GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(cycle_timer != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	HINOKO_FW_ISO_CTX_GET_IFACE(self)->get_cycle_timer(self, clock_id, cycle_timer, error);
}

/**
 * hinoko_fw_iso_ctx_create_source:
 * @self: A [iface@FwIsoCtx].
 * @source: (out): A [struct@GLib.Source].
 * @error: A [struct@GLib.Error].
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for isochronous
 * context.
 */
void hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **source, GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(source != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	(void)HINOKO_FW_ISO_CTX_GET_IFACE(self)->create_source(self, source, error);
}

/**
 * hinoko_fw_iso_ctx_stop:
 * @self: A [iface@FwIsoCtx].
 *
 * Stop isochronous context.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_ctx_stop(HinokoFwIsoCtx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));

	HINOKO_FW_ISO_CTX_GET_IFACE(self)->stop(self);
}

/**
 * hinoko_fw_iso_ctx_unmap_buffer:
 * @self: A [iface@FwIsoCtx].
 *
 * Unmap intermediate buffer shared with 1394 OHCI controller for the context.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_ctx_unmap_buffer(HinokoFwIsoCtx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));

	hinoko_fw_iso_ctx_stop(self);
	HINOKO_FW_ISO_CTX_GET_IFACE(self)->unmap_buffer(self);
}

/**
 * hinoko_fw_iso_ctx_release:
 * @self: A [iface@FwIsoCtx].
 *
 * Release the contest from 1394 OHCI controller.
 *
 * Since: 0.7.
 */
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));

	hinoko_fw_iso_ctx_unmap_buffer(self);
	HINOKO_FW_ISO_CTX_GET_IFACE(self)->release(self);
}

/**
 * hinoko_fw_iso_ctx_flush_completions:
 * @self: A [iface@FwIsoCtx].
 * @error: A [struct@GLib.Error].
 *
 * Flush isochronous context until recent isochronous cycle. The call of function forces the
 * context to queue any type of interrupt event for the recent isochronous cycle. Application can
 * process the content of isochronous packet without waiting for actual hardware interrupt.
 *
 * Since: 0.6.
 */
void hinoko_fw_iso_ctx_flush_completions(HinokoFwIsoCtx *self, GError **error)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	g_return_if_fail(error == NULL || *error == NULL);

	(void)HINOKO_FW_ISO_CTX_GET_IFACE(self)->flush_completions(self, error);
}
