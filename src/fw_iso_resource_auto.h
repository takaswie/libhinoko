// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_AUTO_AUTO_H__
#define __HINOKO_FW_ISO_RESOURCE_AUTO_AUTO_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_iso_resource.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE_AUTO	(hinoko_fw_iso_resource_auto_get_type())

#define HINOKO_FW_ISO_RESOURCE_AUTO(obj)				\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),				\
				    HINOKO_TYPE_FW_ISO_RESOURCE_AUTO,	\
				    HinokoFwIsoResourceAuto))
#define HINOKO_IS_FW_ISO_RESOURCE_AUTO(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),				\
				    HINOKO_TYPE_FW_ISO_RESOURCE_AUTO))

#define HINOKO_FW_ISO_RESOURCE_AUTO_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_CAST((klass),				\
				 HINOKO_TYPE_FW_ISO_RESOURCE_AUTO,	\
				 HinokoFwIsoResourceAutoClass))
#define HINOKO_IS_FW_ISO_RESOURCE_AUTO_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),				\
				 HINOKO_TYPE_FW_ISO_RESOURCE_AUTO))
#define HINOKO_FW_ISO_RESOURCE_AUTO_GET_CLASS(obj)			\
	(G_TYPE_INSTANCE_GET_CLASS((obj),				\
				   HINOKO_TYPE_FW_ISO_RESOURCE_AUTO,	\
				   HinokoFwIsoResourceAutoClass))

typedef struct _HinokoFwIsoResourceAuto		HinokoFwIsoResourceAuto;
typedef struct _HinokoFwIsoResourceAutoClass	HinokoFwIsoResourceAutoClass;
typedef struct _HinokoFwIsoResourceAutoPrivate	HinokoFwIsoResourceAutoPrivate;

struct _HinokoFwIsoResourceAuto {
	HinokoFwIsoResource parent_instance;

	HinokoFwIsoResourceAutoPrivate *priv;
};

struct _HinokoFwIsoResourceAutoClass {
	HinokoFwIsoResourceClass parent_class;
};

GType hinoko_fw_iso_resource_auto_get_type(void) G_GNUC_CONST;

HinokoFwIsoResourceAuto *hinoko_fw_iso_resource_auto_new();

void hinoko_fw_iso_resource_auto_allocate_async(HinokoFwIsoResourceAuto *self,
						guint8 *channel_candidates,
						gsize channel_candidates_count,
						guint bandwidth,
						GError **exception);
void hinoko_fw_iso_resource_auto_allocate_sync(HinokoFwIsoResourceAuto *self,
					       guint8 *channel_candidates,
					       gsize channel_candidates_count,
					       guint bandwidth,
					       GError **exception);

void hinoko_fw_iso_resource_auto_deallocate_async(HinokoFwIsoResourceAuto *self,
						  GError **exception);

#endif
