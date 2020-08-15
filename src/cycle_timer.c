// SPDX-License-Identifier: LGPL-2.1-or-later
#include "cycle_timer.h"

/**
 * SECTION:cycle_timer
 * @Title: HinokoCycleTimer
 * @Short_description: A boxed object to represent data for cycle timer.
 * @include: cycle_timer.h
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
	return g_malloc0(sizeof(HinokoCycleTimer));
}

/**
 * hinoko_cycle_timer_get_timestamp:
 * @self: A #HinokoCycleTimer.
 * @tv_sec: (out): The second part of timestamp.
 * @tv_nsec: (out): The nanosecond part of timestamp.
 *
 * Get timestamp with enough sizee of strorage. The timestamp refers to
 * clock_id available by hinoko_cycle_timer_get_clock_id().
 */
void hinoko_cycle_timer_get_timestamp(HinokoCycleTimer *self, gint64 *tv_sec,
				      gint32 *tv_nsec)
{
	*tv_sec = self->tv_sec;
	*tv_nsec = self->tv_nsec;
}

/**
 * hinoko_cycle_timer_get_clock_id:
 * @self: A #HinokoCycleTimer.
 * @clock_id: (out): The numerical ID of clock source for the reference
 *            timestamp. One CLOCK_REALTIME(0), CLOCK_MONOTONIC(1), and
 *            CLOCK_MONOTONIC_RAW(4) is available in UAPI of Linux kernel.
 *
 * Get the ID of clock for timestamp.
 */
void hinoko_cycle_timer_get_clock_id(HinokoCycleTimer *self, gint *clock_id)
{
	*clock_id = self->clk_id;
}

/**
 * hinoko_cycle_timer_get_cycle_timer:
 * @self: A #HinokoCycleTimer.
 * @cycle_timer: (array fixed-size=3)(out caller-allocates): The value of cycle
 * 		 timer register of 1394 OHCI, including three elements; second,
 * 		 cycle and offset.
 *
 * Get the value of cycle timer in 1394 OHCI controller.
 */
void hinoko_cycle_timer_get_cycle_timer(HinokoCycleTimer *self,
					guint16 cycle_timer[3])
{
	cycle_timer[0] = (self->cycle_timer & 0xfe000000) >> 25;
	cycle_timer[1] = (self->cycle_timer & 0x01fff000) >> 12;
	cycle_timer[2] = self->cycle_timer & 0x00000fff;
}
