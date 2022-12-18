// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_ENUM_TYPES_H__
#define __HINOKO_ENUM_TYPES_H__

G_BEGIN_DECLS

/**
 * HinokoFwIsoCtxMode:
 * @HINOKO_FW_ISO_CTX_MODE_IT:		The mode of IT context of 1394 OHCI.
 * @HINOKO_FW_ISO_CTX_MODE_IR_SINGLE:	The mode of IR context of 1394 OHCI with
 *					packer-per-buffer protocol
 * @HINOKO_FW_ISO_CTX_MODE_IR_MULTIPLE: The mode of IR context of 1394 OHCI with
 *					buffer-fill protocol.
 *
 * A set of mode for isochronous context of Linux FireWire subsystem.
 */
typedef enum {
	HINOKO_FW_ISO_CTX_MODE_IT = FW_CDEV_ISO_CONTEXT_TRANSMIT,
	HINOKO_FW_ISO_CTX_MODE_IR_SINGLE = FW_CDEV_ISO_CONTEXT_RECEIVE,
	HINOKO_FW_ISO_CTX_MODE_IR_MULTIPLE = FW_CDEV_ISO_CONTEXT_RECEIVE_MULTICHANNEL,
} HinokoFwIsoCtxMode;

/**
 * HinokoFwScode:
 * @HINOKO_FW_SCODE_S100:	100 Mbps.
 * @HINOKO_FW_SCODE_S200:	200 Mbps.
 * @HINOKO_FW_SCODE_S400:	400 Mbps.
 * @HINOKO_FW_SCODE_S800:	800 Mbps.
 * @HINOKO_FW_SCODE_S1600:	1600 Mbps.
 * @HINOKO_FW_SCODE_S3200:	3200 Mbps.
 *
 * A set of speed for isochronous context on IEEE 1394 bus.
 */
typedef enum {
	HINOKO_FW_SCODE_S100	= SCODE_100,
	HINOKO_FW_SCODE_S200	= SCODE_200,
	HINOKO_FW_SCODE_S400	= SCODE_400,
	HINOKO_FW_SCODE_S800	= SCODE_800,
	HINOKO_FW_SCODE_S1600	= SCODE_1600,
	HINOKO_FW_SCODE_S3200	= SCODE_3200,
} HinokoFwScode;

/**
 * HinokoFwIsoCtxMatchFlag:
 * @HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0: The value of tag0 in 1394 OHCI.
 * @HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1: The value of tag1 in 1394 OHCI.
 * @HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2: The value of tag2 in 1394 OHCI.
 * @HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3: The value of tag3 in 1394 OHCI.
 *
 * A set of tag field of isochronous packet on IEEE 1394 bus.
 */
typedef enum /*< flags >*/
{
	HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG0 = FW_CDEV_ISO_CONTEXT_MATCH_TAG0,
	HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG1 = FW_CDEV_ISO_CONTEXT_MATCH_TAG1,
	HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG2 = FW_CDEV_ISO_CONTEXT_MATCH_TAG2,
	HINOKO_FW_ISO_CTX_MATCH_FLAG_TAG3 = FW_CDEV_ISO_CONTEXT_MATCH_TAG3,
} HinokoFwIsoCtxMatchFlag;

/**
 * HinokoFwIsoResourceError:
 * @HINOKO_FW_ISO_RESOURCE_ERROR_FAILED:	The system call fails.
 * @HINOKO_FW_ISO_RESOURCE_ERROR_OPENED:	The instance is already associated to any firewire
 *						character device.
 * @HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED:	The instance is not associated to any firewire
 *						character device.
 * @HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT:	No event to the request arrives within timeout.
 * @HINOKO_FW_ISO_RESOURCE_ERROR_EVENT:		Event for the request arrives but includes error code.
 *
 * A set of error code for [iface@FwIsoResource].
 */
typedef enum {
	HINOKO_FW_ISO_RESOURCE_ERROR_FAILED,
	HINOKO_FW_ISO_RESOURCE_ERROR_OPENED,
	HINOKO_FW_ISO_RESOURCE_ERROR_NOT_OPENED,
	HINOKO_FW_ISO_RESOURCE_ERROR_TIMEOUT,
	HINOKO_FW_ISO_RESOURCE_ERROR_EVENT,
} HinokoFwIsoResourceError;

/**
 * HinokoFwIsoResourceAutoError:
 * @HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_FAILED:		The system call fails.
 * @HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED:	The instance is already associated to
 *							allocated isochronous resources.
 * @HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED:	The instance is not associated to allocated
 *							isochronous resources.
 *
 * A set of error code for [class@FwIsoResourceAuto].
 */
typedef enum {
	HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_FAILED,
	HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_ALLOCATED,
	HINOKO_FW_ISO_RESOURCE_AUTO_ERROR_NOT_ALLOCATED,
} HinokoFwIsoResourceAutoError;

/**
 * HinokoFwIsoCtxError:
 * @HINOKO_FW_ISO_CTX_ERROR_FAILED:		The system call fails.
 * @HINOKO_FW_ISO_CTX_ERROR_ALLOCATED:		The instance is already associated to any firewire
 *		        			character device.
 * @HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED:	The instance is not associated to any firewire
 *						character device.
 * @HINOKO_FW_ISO_CTX_ERROR_MAPPED:		The intermediate buffer is already mapped to the
 *						process.
 * @HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED:		The intermediate buffer is not mapped to the
 *						process.
 * @HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED:	No chunk registered before starting.
 * @HINOKO_FW_ISO_CTX_ERROR_NO_ISOC_CHANNEL:	No isochronous channel is available.
 *
 * A set of error code for [iface@FwIsoCtx].
 */
typedef enum {
	HINOKO_FW_ISO_CTX_ERROR_FAILED,
	HINOKO_FW_ISO_CTX_ERROR_ALLOCATED,
	HINOKO_FW_ISO_CTX_ERROR_NOT_ALLOCATED,
	HINOKO_FW_ISO_CTX_ERROR_MAPPED,
	HINOKO_FW_ISO_CTX_ERROR_NOT_MAPPED,
	HINOKO_FW_ISO_CTX_ERROR_CHUNK_UNREGISTERED,
	HINOKO_FW_ISO_CTX_ERROR_NO_ISOC_CHANNEL,
} HinokoFwIsoCtxError;

G_END_DECLS

#endif
