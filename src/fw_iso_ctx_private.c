// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
