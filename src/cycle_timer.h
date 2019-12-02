// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __HINOKO_CYCLE_TIMER__H__
#define __HINOKO_CYCLE_TIMER__H__

#include <glib.h>
#include <glib-object.h>

#include <linux/firewire-cdev.h>

G_BEGIN_DECLS

#define HINOKO_TYPE_CYCLE_TIMER	(hinoko_cycle_timer_get_type())

typedef struct fw_cdev_get_cycle_timer2 HinokoCycleTimer;

GType hinoko_cycle_timer_get_type() G_GNUC_CONST;

HinokoCycleTimer *hinoko_cycle_timer_new();

G_END_DECLS

#endif
