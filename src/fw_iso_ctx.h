// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_CTX_H__
#define __HINOKO_FW_ISO_CTX_H__

#include <hinoko.h>

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

#define HINOKO_FW_ISO_CTX_ERROR		hinoko_fw_iso_ctx_error_quark()

GQuark hinoko_fw_iso_ctx_error_quark();

typedef struct _HinokoFwIsoCtx		HinokoFwIsoCtx;
typedef struct _HinokoFwIsoCtxClass	HinokoFwIsoCtxClass;
typedef struct _HinokoFwIsoCtxPrivate	HinokoFwIsoCtxPrivate;

struct _HinokoFwIsoCtx {
	GObject parent_instance;

	HinokoFwIsoCtxPrivate *priv;
};

struct _HinokoFwIsoCtxClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoCtxClass::stopped:
	 * @self: A #HinokoFwIsoCtx.
	 * @error: (nullable): A #GError.
	 *
	 * When isochronous context is stopped, #HinokoFwIsoCtxClass::stopped
	 * handler is called. When any error occurs, it's reported.
	 */
	void (*stopped)(HinokoFwIsoCtx *self, GError *error);
};

GType hinoko_fw_iso_ctx_get_type(void) G_GNUC_CONST;

void hinoko_fw_iso_ctx_get_cycle_timer(HinokoFwIsoCtx *self, gint clock_id,
				       HinokoCycleTimer *const *cycle_timer,
				       GError **exception);

void hinoko_fw_iso_ctx_create_source(HinokoFwIsoCtx *self, GSource **gsrc,
				     GError **exception);

void hinoko_fw_iso_ctx_flush_completions(HinokoFwIsoCtx *self, GError **exception);

G_END_DECLS

#endif
