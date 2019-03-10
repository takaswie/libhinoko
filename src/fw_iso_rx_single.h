// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RX_SINGLE_H__
#define __HINOKO_FW_ISO_RX_SINGLE_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_iso_ctx.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RX_SINGLE	(hinoko_fw_iso_rx_single_get_type())

#define HINOKO_FW_ISO_RX_SINGLE(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),				\
				    HINOKO_TYPE_FW_ISO_RX_SINGLE,	\
				    HinokoFwIsoRxSingle))
#define HINOKO_IS_FW_ISO_RX_SINGLE(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),				\
				    HINOKO_TYPE_FW_ISO_RX_SINGLE))

#define HINOKO_FW_ISO_RX_SINGLE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),				\
				 HINOKO_TYPE_FW_ISO_RX_SINGLE,		\
				 HinokoFwIsoRxSingleClass))
#define HINOKO_IS_FW_ISO_RX_SINGLE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),				\
				 HINOKO_TYPE_FW_ISO_RX_SINGLE))
#define HINOKO_FW_ISO_RX_SINGLE_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),				\
				   HINOKO_TYPE_FW_ISO_RX_SINGLE,	\
				   HinokoFwIsoRxSingleClass))

typedef struct _HinokoFwIsoRxSingle		HinokoFwIsoRxSingle;
typedef struct _HinokoFwIsoRxSingleClass	HinokoFwIsoRxSingleClass;

struct _HinokoFwIsoRxSingle {
	HinokoFwIsoCtx parent_instance;
};

struct _HinokoFwIsoRxSingleClass {
	HinokoFwIsoCtxClass parent_class;
};

GType hinoko_fw_iso_rx_single_get_type(void) G_GNUC_CONST;

HinokoFwIsoRxSingle *hinoko_fw_iso_rx_single_new(void);

G_END_DECLS

#endif
