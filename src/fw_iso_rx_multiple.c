// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_rx_multiple.h"

/**
 * SECTION:fw_iso_rx_multiple
 * @Title: HinokoFwIsoRxMultiple
 * @Short_description: An object to receive isochronous packet for several
 *		       channels.
 *
 * A #HinokoFwIsoRxMultiple receives isochronous packets for several channels by
 * IR context for buffer-fill mode in 1394 OHCI.
 */
G_DEFINE_TYPE(HinokoFwIsoRxMultiple, hinoko_fw_iso_rx_multiple,
	      HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoRxMultiple", hinoko_fw_iso_rx_multiple)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_rx_multiple_quark(), errno, \
		    "%d: %s", __LINE__, strerror(errno))

static void hinoko_fw_iso_rx_multiple_class_init(HinokoFwIsoRxMultipleClass *klass)
{
	return;
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
