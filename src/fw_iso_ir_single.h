// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINOKO_FW_ISO_IR_SINGLE_H__
#define __ORG_KERNEL_HINOKO_FW_ISO_IR_SINGLE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_IR_SINGLE	(hinoko_fw_iso_ir_single_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoIrSingle, hinoko_fw_iso_ir_single, HINOKO, FW_ISO_IR_SINGLE,
			 GObject);

struct _HinokoFwIsoIrSingleClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoIrSingleClass::interrupted:
	 * @self: A [class@FwIsoIrSingle].
	 * @sec: The sec part of isochronous cycle when interrupt occurs, up to 7.
	 * @cycle: The cycle part of of isochronous cycle when interrupt occurs, up to 7999.
	 * @header: (array length=header_length) (element-type guint8): The headers of IR context
	 *	    for packets handled in the event of interrupt. The content is different
	 *	    depending on header_size parameter of [method@FwIsoIrSingle.allocate].
	 * @header_length: the number of bytes for header.
	 * @count: the number of packets to handle.
	 *
	 * Class closure for the [signal@FwIsoIrSingle::interrupted] signal.
	 */
	void (*interrupted)(HinokoFwIsoIrSingle *self, guint sec, guint cycle,
			    const guint8 *header, guint header_length,
			    guint count);
};

HinokoFwIsoIrSingle *hinoko_fw_iso_ir_single_new(void);

gboolean hinoko_fw_iso_ir_single_allocate(HinokoFwIsoIrSingle *self, const char *path,
					  guint channel, guint header_size, GError **error);

gboolean hinoko_fw_iso_ir_single_map_buffer(HinokoFwIsoIrSingle *self,
					    guint maximum_bytes_per_payload,
					    guint payloads_per_buffer, GError **error);

gboolean hinoko_fw_iso_ir_single_register_packet(HinokoFwIsoIrSingle *self,
						 gboolean schedule_interrupt, GError **error);

gboolean hinoko_fw_iso_ir_single_start(HinokoFwIsoIrSingle *self, const guint16 *cycle_match,
				       guint32 sync_code, HinokoFwIsoCtxMatchFlag tags,
				       GError **error);

void hinoko_fw_iso_ir_single_get_payload(HinokoFwIsoIrSingle *self, guint index,
					 const guint8 **payload, guint *length);

G_END_DECLS

#endif
