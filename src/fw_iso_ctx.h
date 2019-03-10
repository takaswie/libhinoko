// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_CTX_H__
#define __HINOKO_FW_ISO_CTX_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_CTX	(hinoko_fw_iso_ctx_get_type())

#define HINOKO_FW_ISO_CTX(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINOKO_TYPE_FW_ISO_CTX,	\
				    HinokoFwIsoCtx))
#define HINOKO_IS_FW_ISO_CTX(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINOKO_TYPE_FW_ISO_CTX))

#define HINOKO_FW_ISO_CTX_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINOKO_TYPE_FW_ISO_CTX,	\
				 HinokoFwIsoCtxClass))
#define HINOKO_IS_FW_ISO_CTX_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINOKO_TYPE_FW_ISO_CTX))
#define HINOKO_FW_ISO_CTX_GET_CLASS(obj)			\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINOKO_TYPE_FW_ISO_CTX,	\
				   HinokoFwIsoCtxClass))

typedef struct _HinokoFwIsoCtx		HinokoFwIsoCtx;
typedef struct _HinokoFwIsoCtxClass	HinokoFwIsoCtxClass;
typedef struct _HinokoFwIsoCtxPrivate	HinokoFwIsoCtxPrivate;

struct _HinokoFwIsoCtx {
	GObject parent_instance;

	HinokoFwIsoCtxPrivate *priv;
};

struct _HinokoFwIsoCtxClass {
	GObjectClass parent_class;
};

GType hinoko_fw_iso_ctx_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif
