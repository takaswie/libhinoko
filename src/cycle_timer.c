// SPDX-License-Identifier: LGPL-2.1-or-later
#include "fw_iso_ctx_private.h"

/**
 * HinokoCycleTimer:
 * A boxed object to represent data for cycle timer.
 *
 * A [struct@CycleTimer] is an boxed object to represent the value of cycle timer and timestamp
 * referring to clock_id.
 */
HinokoCycleTimer *hinoko_cycle_timer_copy(const HinokoCycleTimer *self)
{
#ifdef g_memdup2
	return g_memdup2(self, sizeof(*self));
#else
	// GLib v2.68 deprecated g_memdup() with concern about overflow by narrow conversion from
	// size_t to unsigned int in ABI for LP64 data model.
	gpointer ptr = g_malloc(sizeof(*self));
	memcpy(ptr, self, sizeof(*self));
	return ptr;
#endif
}

G_DEFINE_BOXED_TYPE(HinokoCycleTimer, hinoko_cycle_timer, hinoko_cycle_timer_copy, g_free)

/**
 * hinoko_cycle_timer_new:
 *
 * Allocate and return an instance of [struct@CycleTimer].
 *
 * Returns: (transfer none): An instance of [struct@CycleTimer].
 */
HinokoCycleTimer *hinoko_cycle_timer_new()
{
	return g_malloc0(sizeof(HinokoCycleTimer));
}

/**
 * hinoko_cycle_timer_get_timestamp:
 * @self: A [struct@CycleTimer].
 * @tv_sec: (out): The second part of timestamp.
 * @tv_nsec: (out): The nanosecond part of timestamp.
 *
 * Get timestamp with enough sizee of strorage. The timestamp refers to clock_id available by
 * [method@CycleTimer.get_clock_id].
 */
void hinoko_cycle_timer_get_timestamp(HinokoCycleTimer *self, gint64 *tv_sec,
				      gint32 *tv_nsec)
{
	*tv_sec = self->tv_sec;
	*tv_nsec = self->tv_nsec;
}

/**
 * hinoko_cycle_timer_get_clock_id:
 * @self: A [struct@CycleTimer].
 * @clock_id: (out): The numerical ID of clock source for the reference timestamp. One of
 *	      CLOCK_REALTIME(0), CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(4) is available in
 *	      UAPI of Linux kernel.
 *
 * Get the ID of clock for timestamp.
 */
void hinoko_cycle_timer_get_clock_id(HinokoCycleTimer *self, gint *clock_id)
{
	*clock_id = self->clk_id;
}

#define IEEE1394_CYCLE_TIMER_SEC_MASK		0xfe000000
#define IEEE1394_CYCLE_TIMER_SEC_SHIFT		25
#define IEEE1394_CYCLE_TIMER_CYCLE_MASK		0x01fff000
#define IEEE1394_CYCLE_TIMER_CYCLE_SHIFT	12
#define IEEE1394_CYCLE_TIMER_OFFSET_MASK	0x00000fff

static guint ieee1394_cycle_timer_to_sec(guint32 cycle_timer)
{
	return (cycle_timer & IEEE1394_CYCLE_TIMER_SEC_MASK) >> IEEE1394_CYCLE_TIMER_SEC_SHIFT;
}

static guint ieee1394_cycle_timer_to_cycle(guint32 cycle_timer)
{
	return (cycle_timer & IEEE1394_CYCLE_TIMER_CYCLE_MASK) >> IEEE1394_CYCLE_TIMER_CYCLE_SHIFT;
}

static guint ieee1394_cycle_timer_to_offset(guint32 cycle_timer)
{
	return cycle_timer & IEEE1394_CYCLE_TIMER_OFFSET_MASK;
}

/**
 * hinoko_cycle_timer_get_cycle_timer:
 * @self: A [struct@CycleTimer].
 * @cycle_timer: (array fixed-size=3)(out caller-allocates): The value of cycle timer register of
 *		 1394 OHCI, including three elements; second, cycle, and offset.
 *
 * Get the value of cycle timer in 1394 OHCI controller. The first element of array expresses the
 * value of sec field, up to 127. The second element of array expresses the value of cycle field,
 * up to 7999. The third element of array expresses the value of offset field, up to 3071.
 */
void hinoko_cycle_timer_get_cycle_timer(HinokoCycleTimer *self,
					guint16 cycle_timer[3])
{
	cycle_timer[0] = ieee1394_cycle_timer_to_sec(self->cycle_timer);
	cycle_timer[1] = ieee1394_cycle_timer_to_cycle(self->cycle_timer);
	cycle_timer[2] = ieee1394_cycle_timer_to_offset(self->cycle_timer);
}
