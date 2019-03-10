// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_rx_single.h"

/**
 * SECTION:fw_iso_rx_single
 * @Title: HinokoFwIsoRxSingle
 * @Short_description: An object to receive isochronous packet for single
 *		       channel.
 *
 * A #HinokoFwIsoRxSingle receives isochronous packets for single channel by IR
 * context for packet-per-buffer mode in 1394 OHCI. The content of packet is
 * split to two parts; context header and context payload in a manner of Linux
 * FireWire subsystem.
 *
 */
G_DEFINE_TYPE(HinokoFwIsoRxSingle, hinoko_fw_iso_rx_single,
	      HINOKO_TYPE_FW_ISO_CTX)

static void hinoko_fw_iso_rx_single_class_init(HinokoFwIsoRxSingleClass *klass)
{
	return;
}

static void hinoko_fw_iso_rx_single_init(HinokoFwIsoRxSingle *self)
{
	return;
}

/**
 * hinoko_fw_iso_rx_single_new:
 *
 * Instantiate #HinokoFwIsoRxSingle object and return the instance.
 *
 * Returns: an instance of #HinokoFwIsoRxSingle.
 */
HinokoFwIsoRxSingle *hinoko_fw_iso_rx_single_new(void)
{
	return g_object_new(HINOKO_TYPE_FW_ISO_RX_SINGLE, NULL);
}
