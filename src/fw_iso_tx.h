// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_TX_H__
#define __HINOKO_FW_ISO_TX_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_iso_ctx.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_TX	(hinoko_fw_iso_tx_get_type())

#define HINOKO_FW_ISO_TX(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINOKO_TYPE_FW_ISO_TX,	\
				    HinokoFwIsoTx))
#define HINOKO_IS_FW_ISO_TX(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINOKO_TYPE_FW_ISO_TX))

#define HINOKO_FW_ISO_TX_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINOKO_TYPE_FW_ISO_TX,		\
				 HinokoFwIsoTxClass))
#define HINOKO_IS_FW_ISO_TX_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINOKO_TYPE_FW_ISO_TX))
#define HINOKO_FW_ISO_TX_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINOKO_TYPE_FW_ISO_TX,	\
				   HinokoFwIsoTxClass))

typedef struct _HinokoFwIsoTx		HinokoFwIsoTx;
typedef struct _HinokoFwIsoTxClass	HinokoFwIsoTxClass;

struct _HinokoFwIsoTx {
	HinokoFwIsoCtx parent_instance;
};

struct _HinokoFwIsoTxClass {
	HinokoFwIsoCtxClass parent_class;
};

GType hinoko_fw_iso_tx_get_type(void) G_GNUC_CONST;

HinokoFwIsoTx *hinoko_fw_iso_tx_new(void);

G_END_DECLS

#endif
