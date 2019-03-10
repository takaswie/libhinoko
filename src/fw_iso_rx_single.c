// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <errno.h>

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
struct _HinokoFwIsoRxSinglePrivate {
	guint header_size;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinokoFwIsoRxSingle, hinoko_fw_iso_rx_single,
			   HINOKO_TYPE_FW_ISO_CTX)

// For error handling.
G_DEFINE_QUARK("HinokoFwIsoRxSingle", hinoko_fw_iso_rx_single)
#define raise(exception, errno)						\
	g_set_error(exception, hinoko_fw_iso_rx_single_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void fw_iso_rx_single_finalize(GObject *obj)
{
	HinokoFwIsoRxSingle *self = HINOKO_FW_ISO_RX_SINGLE(obj);

	hinoko_fw_iso_rx_single_release(self);

	G_OBJECT_CLASS(hinoko_fw_iso_rx_single_parent_class)->finalize(obj);
}

static void hinoko_fw_iso_rx_single_class_init(HinokoFwIsoRxSingleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_iso_rx_single_finalize;
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

/**
 * hinoko_fw_iso_rx_single_allocate:
 * @self: A #HinokoFwIsoRxSingle.
 * @path: A path to any Linux FireWire character device.
 * @scode: A #HinokoFwScode to indicate speed of isochronous communication.
 * @channel: An isochronous channel to listen.
 * @header_size: The number of bytes for header of IR context.
 * @exception: A #GError.
 *
 * Allocate an IR context to 1394 OHCI controller for packet-per-buffer mode.
 * A local node of the node corresponding to the given path is used as the
 * controller, thus any path is accepted as long as process has enough
 * permission for the path.
 */
void hinoko_fw_iso_rx_single_allocate(HinokoFwIsoRxSingle *self,
				      const char *path, HinokoFwScode scode,
				      guint channel, guint header_size,
				      GError **exception)
{
	HinokoFwIsoRxSinglePrivate *priv;

	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));
	priv = hinoko_fw_iso_rx_single_get_instance_private(self);

	hinoko_fw_iso_ctx_allocate(HINOKO_FW_ISO_CTX(self), path,
				 HINOKO_FW_ISO_CTX_MODE_RX_SINGLE, scode,
				 channel, header_size, exception);
	if (*exception != NULL)
		return;

	priv->header_size = header_size;
}

/**
 * hinoko_fw_iso_rx_single_release:
 * @self: A #HinokoFwIsoRxSingle.
 *
 * Release allocated IR context from 1394 OHCI controller.
 */
void hinoko_fw_iso_rx_single_release(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_rx_single_unmap_buffer(self);
	hinoko_fw_iso_ctx_release(HINOKO_FW_ISO_CTX(self));
}

/*
 * hinoko_fw_iso_rx_single_map_buffer:
 * @self: A #HinokoFwIsoRxSingle.
 * @maximum_bytes_per_payload: The maximum number of bytes per payload of IR
 *			       context.
 * @payloads_per_buffer: The number of payload in buffer.
 * @payloads_per_irq: The number of payload per interval of interrupt.
 * @exception: A #GError.
 *
 * Map intermediate buffer to share payload of IR context with 1394 OHCI
 * controller.
 */
void hinoko_fw_iso_rx_single_map_buffer(HinokoFwIsoRxSingle *self,
					guint maximum_bytes_per_payload,
					guint payloads_per_buffer,
					GError **exception)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_ctx_map_buffer(HINOKO_FW_ISO_CTX(self),
				     maximum_bytes_per_payload,
				     payloads_per_buffer, exception);
}

/**
 * hinoko_fw_iso_rx_single_unmap_buffer:
 * @self: A #HinokoFwIsoRxSingle.
 *
 * Unmap intermediate buffer shard with 1394 OHCI controller for payload
 * of IR context.
 */
void hinoko_fw_iso_rx_single_unmap_buffer(HinokoFwIsoRxSingle *self)
{
	g_return_if_fail(HINOKO_IS_FW_ISO_RX_SINGLE(self));

	hinoko_fw_iso_ctx_unmap_buffer(HINOKO_FW_ISO_CTX(self));
}
