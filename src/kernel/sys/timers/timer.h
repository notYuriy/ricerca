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
	//! @brief Callback to initialize timer for waiting for a given amount of milliseconds
	//! @param self Pointer to this structure
	//! @param ms Number of milliseconds to wait
	//! @return True if operation is supported
	bool (*make_goal)(struct timer *self, uint32_t ms);
	//! @brief Callback to check if goal was reached
	//! @param self Pointer to this structure
	//! @return True if goal was reached
	bool (*is_goal_reached)(struct timer *self);
	//! @brief Next timer
	struct timer *next;
};

//! @brief Register timer
//! @param timer Pointer to the timer
void timer_register(struct timer *timer);

//! @brief Wait for a given number of milliseconds
//! @param ms Number of milliseconds
void timer_busy_wait_ms(uint32_t ms);

//! @brief Timer goal
struct timer_goal {
	//! @brief Reference to the timer being used
	struct timer *timer;
};

//! @brief Create timer goal
//! @param ms Number of milliseconds to wait
//! @param goal Buffer to store timer goal in
void timer_make_goal(uint32_t ms, struct timer_goal *goal);

//! @brief Busy wait on the goal
//! @param goal Pointer to the timer goal
void timer_busy_wait_on_goal(struct timer_goal *goal);

//! @brief Check if timer goal has been reached
//! @param goal Pointer to the timer goal
//! @return True if goal has been reached
bool timer_poll_goal(struct timer_goal *goal);

//! @brief Timer initialization target
EXPORT_TARGET(timers_available)
