// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_RESOURCE_H__
#define __HINOKO_FW_ISO_RESOURCE_H__

#include <glib.h>
#include <glib-object.h>

#include <hinoko_sigs_marshal.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_RESOURCE	(hinoko_fw_iso_resource_get_type())

#define HINOKO_FW_ISO_RESOURCE(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),				\
					HINOKO_TYPE_FW_ISO_RESOURCE,	\
					HinokoFwIsoResource))
#define HINOKO_IS_FW_ISO_RESOURCE(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),				\
				    HINOKO_TYPE_FW_ISO_RESOURCE))

#define HINOKO_FW_ISO_RESOURCE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),				\
				 HINOKO_TYPE_FW_ISO_RESOURCE,		\
				 HinokoFwIsoResourceClass))
#define HINOKO_IS_FW_ISO_RESOURCE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),				\
				 HINOKO_TYPE_FW_ISO_RESOURCE))
#define HINOKO_FW_ISO_RESOURCE_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),				\
				   HINOKO_TYPE_FW_ISO_RESOURCE,		\
				   HinokoFwIsoResourceClass))

typedef struct _HinokoFwIsoResource		HinokoFwIsoResource;
typedef struct _HinokoFwIsoResourceClass	HinokoFwIsoResourceClass;
typedef struct _HinokoFwIsoResourcePrivate	HinokoFwIsoResourcePrivate;

struct _HinokoFwIsoResource {
	GObject parent_instance;

	HinokoFwIsoResourcePrivate *priv;
};

struct _HinokoFwIsoResourceClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoResourceClass::allocated:
	 * @self: A #HinokoFwIsoResource.
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @err_code: 0 if successful, else any error code.
	 *
	 * When allocation of isochronous resource finishes, the ::allocated
	 * handler is called to notify the result, channel, and bandwidth.
	 */
	void (*allocated)(HinokoFwIsoResource *self, guint channel,
			  guint bandwidth, guint err_code);

	/**
	 * HinokoFwIsoResourceClass::deallocated:
	 * @self: A #HinokoFwIsoResource.
	 * @channel: The deallocated channel number.
	 * @bandwidth: The deallocated amount of bandwidth.
	 * @err_code: 0 if successful, else any error code.
	 *
	 * When deallocation of isochronous resource finishes, the ::deallocated
	 * handler is called to notify the result, channel, and bandwidth.
	 */
	void (*deallocated)(HinokoFwIsoResource *self, guint channel,
			    guint bandwidth, guint err_code);
};

GType hinoko_fw_iso_resource_get_type(void) G_GNUC_CONST;

HinokoFwIsoResource *hinoko_fw_iso_resource_new();

void hinoko_fw_iso_resource_open(HinokoFwIsoResource *self, const gchar *path,
				 gint open_flag, GError **exception);

void hinoko_fw_iso_resource_create_source(HinokoFwIsoResource *self,
					  GSource **gsrc, GError **exception);

#endif
