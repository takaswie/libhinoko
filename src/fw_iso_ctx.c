// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/firewire-cdev.h>

/**
 * SECTION:fw_iso_ctx
 * @Title: HinokoFwIsoCtx
 * @Short_description: An abstract object to maintain isochronous context.
 *
 * A #HinokoFwIsoCtx is an abstract object to maintain isochronous context by
 * UAPI of Linux FireWire subsystem. All of operations utilize ioctl(2) with
 * subsystem specific request commands. This object is designed for internal
 * use, therefore a few method and properties are available for applications.
 */
struct _HinokoFwIsoCtxPrivate {
	int fd;
	guint handle;

	HinokoFwIsoCtxMode mode;
	guint header_size;
};
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(HinokoFwIsoCtx, hinoko_fw_iso_ctx,
				    G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoCtx", hinoko_fw_iso_ctx)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_ctx_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void fw_iso_ctx_finalize(GObject *obj)
{
	HinokoFwIsoCtx *self = HINOKO_FW_ISO_CTX(obj);

	hinoko_fw_iso_ctx_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_ctx_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_ctx_class_init(HinokoFwIsoCtxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_ctx_finalize;
}

static void hinoko_fw_iso_ctx_init(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv =
				hinoko_fw_iso_ctx_get_instance_private(self);

	priv->fd = -1;
}

/**
 * hinoko_fw_iso_ctx_allocate:
 * @self: A #HinokoFwIsoCtx.
 * @path: A path to any Linux FireWire character device.
 * @mode: The mode of context, one of #HinokoFwIsoCtxMode enumerations.
 * @scode: The speed of context, one of #HinokoFwScode enumerations.
 * @channel: The numerical channel of context up to 64.
 * @header_size: The number of bytes for header of isochronous context.
 * @exception: A #GError.
 *
 * Allocate a isochronous context to 1394 OHCI controller. A local node of the
 * node corresponding to the given path is used as the controller, thus any
 * path is accepted as long as process has enough permission for the path.
 */
void hinoko_fw_iso_ctx_allocate(HinokoFwIsoCtx *self, const char *path,
				HinokoFwIsoCtxMode mode, HinokoFwScode scode,
				guint channel, guint header_size,
				GError **exception)
{
	HinokoFwIsoCtxPrivate *priv;
	struct fw_cdev_create_iso_context create = {0};

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (path == NULL || strlen(path) == 0) {
		raise (exception, EINVAL);
		return;
	}

	// Linux firewire stack supports three types of isochronous context
	// described in 1394 OHCI specification.
	if (mode != HINOKO_FW_ISO_CTX_MODE_TX &&
	    mode != HINOKO_FW_ISO_CTX_MODE_RX_SINGLE &&
	    mode != HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		raise(exception, EINVAL);
		return;
	}

	if (scode != HINOKO_FW_SCODE_S100 &&
	    scode != HINOKO_FW_SCODE_S200 &&
	    scode != HINOKO_FW_SCODE_S400 &&
	    scode != HINOKO_FW_SCODE_S800 &&
	    scode != HINOKO_FW_SCODE_S1600 &&
	    scode != HINOKO_FW_SCODE_S3200 &&
	    scode != HINOKO_FW_SCODE_BETA) {
		raise(exception, EINVAL);
		return;
	}

	// IEEE 1394 specification supports isochronous channel up to 64.
	if (channel > 64) {
		raise(exception, EINVAL);
		return;
	}

	// Headers should be aligned to quadlet.
	if (header_size % 4 > 0) {
		raise(exception, EINVAL);
		return;
	}

	if (mode == HINOKO_FW_ISO_CTX_MODE_RX_SINGLE) {
		// At least, 1 quadlet is required for iso_header.
		if (header_size < 4) {
			raise(exception, EINVAL);
			return;
		}
		if (channel > 64) {
			raise(exception, EINVAL);
			return;
		}
	} else if (mode == HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE) {
		// Needless.
		if (header_size > 0) {
			raise(exception, EINVAL);
			return;
		}

		// Needless.
		if (channel > 0) {
			raise(exception, EINVAL);
			return;
		}
	}

	if (priv->fd >= 0) {
		raise(exception, EBUSY);
		return;
	}

	priv->fd = open(path, O_RDWR);
	if  (priv->fd < 0) {
		raise(exception, errno);
		return;
	}

	create.type = mode;
	create.channel = channel;
	create.speed = scode;
	create.closure = (__u64)self;
	create.header_size = header_size;

	if (ioctl(priv->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create) < 0) {
		raise(exception, errno);
		close(priv->fd);
		priv->fd = -1;
		return;
	}

	priv->handle = create.handle;
	priv->mode = mode;
	priv->header_size = header_size;
}

/**
 * hinoko_fw_iso_ctx_release:
 * @self: A #HinokoFwIsoCtx.
 *
 * Release allocated isochronous context from 1394 OHCI controller.
 */
void hinoko_fw_iso_ctx_release(HinokoFwIsoCtx *self)
{
	HinokoFwIsoCtxPrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_CTX(self));
	priv = hinoko_fw_iso_ctx_get_instance_private(self);

	if (priv->fd >= 0)
		close(priv->fd);

	priv->fd = -1;
}
