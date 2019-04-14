/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __HINOKO_ENUM_TYPES_H__
#define __HINOKO_ENUM_TYPES_H__

#include <glib-object.h>
#include <linux/firewire-cdev.h>

/**
 * HinokoFwIsoCtxMode:
 * @HINOKO_FW_ISO_CTX_MODE_TX:		The mode of IT context of 1394 OHCI.
 * @HINOKO_FW_ISO_CTX_MODE_RX_SINGLE:	The mode of IR context of 1394 OHCI with
 *					packer-per-buffer protocol
 * @HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE: The mode of IR context of 1394 OHCI with
 *					buffer-fill protocol.
 *
 * A representation of mode for isochronous context of Linux FireWire subsystem.
 */
typedef enum {
	HINOKO_FW_ISO_CTX_MODE_TX = FW_CDEV_ISO_CONTEXT_TRANSMIT,
	HINOKO_FW_ISO_CTX_MODE_RX_SINGLE = FW_CDEV_ISO_CONTEXT_RECEIVE,
	HINOKO_FW_ISO_CTX_MODE_RX_MULTIPLE = FW_CDEV_ISO_CONTEXT_RECEIVE_MULTICHANNEL,
} HinokoFwIsoCtxMode;

/**
 * HinokoFwScode:
 * @HINOKO_FW_SCODE_S100:	100 bps.
 * @HINOKO_FW_SCODE_S200:	200 bps.
 * @HINOKO_FW_SCODE_S400:	400 bps.
 * @HINOKO_FW_SCODE_S800:	800 bps.
 * @HINOKO_FW_SCODE_S1600:	1600 bps.
 * @HINOKO_FW_SCODE_S3200:	3200 bps.
 * @HINOKO_FW_SCODE_BETA:	The speed defined in beta specification.
 *
 * A representation of speed for isochronous context on IEEE 1394 bus.
 */
typedef enum {
	HINOKO_FW_SCODE_S100	= SCODE_100,
	HINOKO_FW_SCODE_S200	= SCODE_200,
	HINOKO_FW_SCODE_S400	= SCODE_400,
	HINOKO_FW_SCODE_S800	= SCODE_800,
	HINOKO_FW_SCODE_S1600	= SCODE_1600,
	HINOKO_FW_SCODE_S3200	= SCODE_3200,
	HINOKO_FW_SCODE_BETA	= SCODE_BETA,
} HinokoFwScode;

#endif
