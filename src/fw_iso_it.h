// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINOKO_FW_ISO_IT_H__
#define __ORG_KERNEL_HINOKO_FW_ISO_IT_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_FW_ISO_IT	(hinoko_fw_iso_it_get_type())

G_DECLARE_DERIVABLE_TYPE(HinokoFwIsoIt, hinoko_fw_iso_it, HINOKO, FW_ISO_IT, GObject);

struct _HinokoFwIsoItClass {
	GObjectClass parent_class;

	/**
	 * HinokoFwIsoItClass::interrupted:
	 * @self: A [class@FwIsoIt].
	 * @sec: The sec part of isochronous cycle when interrupt occurs, up to 7.
	 * @cycle: The cycle part of of isochronous cycle when interrupt occurs, up to 7999.
	 * @tstamp: (array length=tstamp_length) (element-type guint8): A series of timestamps for
	 *	    packets already handled.
	 * @tstamp_length: the number of bytes for @tstamp.
	 * @count: the number of handled packets.
	 *
	 * Class closure for the [signal@FwIsoIt::interrupted] signal.
	 */
	void (*interrupted)(HinokoFwIsoIt *self, guint sec, guint cycle,
			    const guint8 *tstamp, guint tstamp_length,
			    guint count);
};

HinokoFwIsoIt *hinoko_fw_iso_it_new(void);

gboolean hinoko_fw_iso_it_allocate(HinokoFwIsoIt *self, const char *path, HinokoFwScode scode,
				   guint channel, guint header_size, GError **error);

gboolean hinoko_fw_iso_it_map_buffer(HinokoFwIsoIt *self, guint maximum_bytes_per_payload,
				     guint payloads_per_buffer, GError **error);

gboolean hinoko_fw_iso_it_start(HinokoFwIsoIt *self, const guint16 *cycle_match, GError **error);

gboolean hinoko_fw_iso_it_register_packet(HinokoFwIsoIt *self, HinokoFwIsoCtxMatchFlag tags,
					  guint sync_code,
					  const guint8 *header, guint header_length,
					  const guint8 *payload, guint payload_length,
					  gboolean schedule_interrupt, GError **error);

G_END_DECLS

#endif
