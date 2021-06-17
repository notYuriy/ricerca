//! @file timer.h
//! @brief File containing declarations of timing functions

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <thread/spinlock.h>

//! @brief Coolness value for ACPI timer
#define TIMER_ACPI_COOLNESS 1

//! @brief Generic timer interface
struct timer {
	//! @brief Timer coolness value (timer layer picks the best timer using this value)
	uint64_t coolness;
	//! @brief Callback to wait a given number of milliseconds
	//! @note If wait is unsupported, false is returned
	bool (*wait_callback)(struct timer *self, uint32_t ms);
	//! @brief Next timer
	struct timer *next;
};

//! @brief Register timer
//! @param timer Pointer to the timer
void timer_register(struct timer *timer);

//! @brief Wait for a given number of milliseconds
//! @param ms Number of milliseconds
void timer_busy_wait_ms(uint32_t ms);

//! @brief Timer subsystem lock
extern struct thread_spinlock lock;

//! @brief Timer initialization target
EXPORT_TARGET(timers_available)
