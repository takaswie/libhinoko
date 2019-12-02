// SPDX-License-Identifier: LGPL-2.1-or-later
#include "cycle_timer.h"

/**
 * SECTION:cycle_timer
 * @Title: HinokoCycleTimer
 * @Short_description: A boxed object to represent data for cycle timer.
 *
 * A #HinokoCycleTimer is an boxed object to represent the value of cycle
 * timer and timestamp referring to clock_id.
 */
HinokoCycleTimer *hinoko_cycle_timer_copy(const HinokoCycleTimer *self)
{
	return g_memdup(self, sizeof(*self));
}

G_DEFINE_BOXED_TYPE(HinokoCycleTimer, hinoko_cycle_timer, hinoko_cycle_timer_copy, g_free)

/**
 * hinoko_cycle_timer_new:
 *
 * Allocate and return an instance of HinokoCycleTimer.
 *
 * Returns: (transfer none): An instance of HinokoCycleTimer.
 */
HinokoCycleTimer *hinoko_cycle_timer_new()
{
	return g_try_malloc0(sizeof(HinokoCycleTimer));
}
