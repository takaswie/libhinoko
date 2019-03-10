// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx.h"

/**
 * SECTION:fw_iso_ctx
 * @Title: HinokoFwIsoCtx
 * @Short_description: An abstract object to maintain isochronous context.
 *
 * A #HinokoFwIsoCtx is an abstract object to maintain isochronous context by
 * UAPI of Linux FireWire subsystem. All of operations utilize ioctl(2) with
 * subsystem specific request commands. This object is designed for internal
 * use, therefore no method is available for applications.
 */
struct _HinokoFwIsoCtxPrivate {
	int tmp;
};
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(HinokoFwIsoCtx, hinoko_fw_iso_ctx,
				    G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoCtx", hinoko_fw_iso_ctx)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_ctx_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void hinoko_fw_iso_ctx_class_init(HinokoFwIsoCtxClass *klass)
{
	return;
}

static void hinoko_fw_iso_ctx_init(HinokoFwIsoCtx *self)
{
	return;
}
