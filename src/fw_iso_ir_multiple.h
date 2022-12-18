// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_FW_ISO_IR_MULTIPLE_H__
#define __HINOKO_FW_ISO_IR_MULTIPLE_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_IR_MULTIPLE	(hinoko_fw_iso_ir_multiple_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoIrMultiple, hinoko_fw_iso_ir_multiple, HINOKO,
			 FW_ISO_IR_MULTIPLE, GObject);

struct _HinokoFwIsoIrMultipleClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoIrMultipleClass::interrupted:
	 * @self: A [class@FwIsoIrMultiple].
	 * @count: The number of packets available in this interrupt.
	 *
	 * Class closure for the [signal@FwIsoIrMultiple::interrupted].
	 */
	void (*interrupted)(HinokoFwIsoIrMultiple *self, guint count);
};

HinokoFwIsoIrMultiple *hinoko_fw_iso_ir_multiple_new(void);

gboolean hinoko_fw_iso_ir_multiple_allocate(HinokoFwIsoIrMultiple *self, const char *path,
					    const guint8 *channels, guint channels_length,
					    GError **error);

gboolean hinoko_fw_iso_ir_multiple_map_buffer(HinokoFwIsoIrMultiple *self, guint bytes_per_chunk,
					      guint chunks_per_buffer, GError **error);

gboolean hinoko_fw_iso_ir_multiple_start(HinokoFwIsoIrMultiple *self, const guint16 *cycle_match,
					 guint32 sync_code, HinokoFwIsoCtxMatchFlag tags,
					 guint chunks_per_irq, GError **error);

void hinoko_fw_iso_ir_multiple_get_payload(HinokoFwIsoIrMultiple *self, guint index,
					   const guint8 **payload, guint *length);

G_END_DECLS

#endif
