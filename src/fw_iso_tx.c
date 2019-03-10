// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

/**
 * SECTION:fw_iso_tx
 * @Title: HinokoFwIsoTx
 * @Short_description: An object to transmit isochronous packet for single
 *		       channel.
 *
 * A #HinokoFwIsoTx transmits isochronous packets for single channel by IT
 * context in 1394 OHCI. The content of packet is split to two parts; context
 * header and context payload in a manner of Linux FireWire subsystem.
 */
struct _HinokoFwIsoTxPrivate {
	int tmp;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoTx, hinoko_fw_iso_tx,
			   HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoTx", hinoko_fw_iso_tx)
#define raise(exception, errno)					\
	g_set_error(exception, hinoko_fw_iso_tx_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void fw_iso_tx_finalize(GObject *obj)
{
	HinokoFwIsoTx *self = HINOKO_FW_ISO_TX(obj);

	hinoko_fw_iso_tx_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_tx_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_tx_class_init(HinokoFwIsoTxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_tx_finalize;
}

static void hinoko_fw_iso_tx_init(HinokoFwIsoTx *self)
{
	return;
}

/**
 * hinoko_fw_iso_tx_new:
 *
 * Instantiate #HinokoFwIsoTx object and return the instance.
 *
 * Returns: an instance of #HinokoFwIsoTx.
 */
HinokoFwIsoTx *hinoko_fw_iso_tx_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_TX, NULL);
}

/**
 * hinoko_fw_iso_tx_allocate:
 * @self: A #HinokoFwIsoTx.
 * @path: A path to any Linux FireWire character device.
 * @scode: A #HinokoFwScode to indicate speed of isochronous communication.
 * @channel: An isochronous channel to transfer.
 * @header_size: The number of bytes for header of IT context.
 * @exception: A #GError.
 *
 * Allocate an IT context to 1394 OHCI controller. A local node of the node
 * corresponding to the given path is used as the controller, thus any path is
 * accepted as long as process has enough permission for the path.
 */
void hinoko_fw_iso_tx_allocate(HinokoFwIsoTx *self, const char *path,
			       HinokoFwScode scode, guint channel,
			       guint header_size, GError **exception)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				   HINOKO_FW_ISO_CTX_MODE_TX, scode, channel,
				   header_size, exception);
}

/**
 * hinoko_fw_iso_tx_release:
 * @self: A #HinokoFwIsoTx.
 *
 * Release allocated IT context from 1394 OHCI controller.
 */
void hinoko_fw_iso_tx_release(HinokoFwIsoTx *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_TX(self));

	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(self));
}
