// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_tx.h"

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
G_DEFINE_TYPE(HinokoFwIsoTx, hinoko_fw_iso_tx, HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoTx", hinoko_fw_iso_tx)
#define raise(exception, errno)					\
	g_set_error(exception, hinoko_fw_iso_tx_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void hinoko_fw_iso_tx_class_init(HinokoFwIsoTxClass *klass)
{
	return;
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
