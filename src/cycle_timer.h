// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_CYCLE_TIMER_H__
#define __HINOKO_CYCLE_TIMER_H__

#include <hinoko.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_CYCLE_TIMER	(hinoko_cycle_timer_get_type())

typedef struct fw_cdev_get_cycle_timer2 HinokoCycleTimer;

GType hinoko_cycle_timer_get_type() G_GNUC_CONST;

HinokoCycleTimer *hinoko_cycle_timer_new();

void hinoko_cycle_timer_get_timestamp(HinokoCycleTimer *self, gint64 *tv_sec,
				      gint32 *tv_nsec);

void hinoko_cycle_timer_get_clock_id(HinokoCycleTimer *self, gint *clock_id);

void hinoko_cycle_timer_get_cycle_timer(HinokoCycleTimer *self,
					guint16 cycle_timer[3]);

G_END_DECLS

#endif
