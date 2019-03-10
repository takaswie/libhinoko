// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <errno.h>

/**
 * SECTION:fw_iso_rx_multiple
 * @Title: HinokoFwIsoRxMultiple
 * @Short_description: An object to receive isochronous packet for several
 *		       channels.
 *
 * A #HinokoFwIsoRxMultiple receives isochronous packets for several channels by
 * IR context for buffer-fill mode in 1394 OHCI.
 */
struct _HinokoFwIsoRxMultiplePrivate {
	int tmp;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoRxMultiple, hinoko_fw_iso_rx_multiple,
			   HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoRxMultiple", hinoko_fw_iso_rx_multiple)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_rx_multiple_quark(), errno, \
		    "%d: %s", __LINE__, strerror(errno))

static void fw_iso_rx_multiple_finalize(GObject *obj)
{
	HinokoFwIsoRxMultiple *self = HINOKO_FW_ISO_RX_MULTIPLE(obj);

	hinoko_fw_iso_rx_multiple_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_multiple_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_rx_multiple_class_init(HinokoFwIsoRxMultipleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_rx_multiple_finalize;
}

static void hinoko_fw_iso_rx_multiple_init(HinokoFwIsoRxMultiple *self)
{
	return;
}

/**
 * hinoko_fw_iso_rx_multiple_new:
 *
 * Instantiate #HinokoFwIsoRxMultiple object and return the instance.
 *
 * Returns: an instance of #HinokoFwIsoRxMultiple.
 */
HinokoFwIsoRxMultiple *hinoko_fw_iso_rx_multiple_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RX_MULTIPLE, NULL);
}

/**
 * hinoko_fw_iso_rx_multiple_allocate:
 * @self: A #HinokoFwIsoRxMultiple.
 * @path: A path to any Linux FireWire character device.
 * @scode: A #HinokoFwScode to indicate speed of isochronous communication.
 * @channels: (array length=channels_length) (element-type guint8): an array
 *	      for channels to listen to.
 * @channels_length: The length of @channels.
 * @exception: A #GError.
 *
 * Allocate an IR context to 1394 OHCI controller for buffer-fill mode.
 * A local node of the node corresponding to the given path is used as the
 * controller, thus any path is accepted as long as process has enough
 * permission for the path.
 */
void hinoko_fw_iso_rx_multiple_allocate(HinokoFwIsoRxMultiple *self,
					const char *path, HinokoFwScode scode,
					const guint8 *channels,
					guint channels_length,
					GError **exception)
{
	guint64 channel_flags;
	int i;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));

	if (channels_length == 0) {
		raise(exception, EINVAL);
		return;
	}

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				   HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE, scode, 0,
				   0, exception);
	if (*exception != NULL)
		return;

	channel_flags = 0;
	for (i = 0; i < channels_length; ++i) {
		if (channels[i] > 64) {
			raise(exception, EINVAL);
			return;
		}

		channel_flags |= G_GUINT64_CONSTANT(1) << channels[i];
	}

	hinoko_fw_iso_ctx_set_rx_channels(HINOKO_FW_ISO_CTX(self),
					  &channel_flags, exception);
	if (*exception != NULL)
		return;
}

/**
 * hinoko_fw_iso_rx_multiple_release:
 * @self: A #HinokoFwIsoRxMultiple.
 *
 * Release allocated IR context from 1394 OHCI controller.
 */
void hinoko_fw_iso_rx_multiple_release(HinokoFwIsoRxMultiple *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_MULTIPLE(self));

	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(self));
}
