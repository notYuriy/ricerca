//! @brief timer.c
//! @file File containing mplementations of generic timing functions

#include <lib/log.h>
#include <lib/panic.h>
#include <sys/timers/acpi.h>
#include <sys/timers/timer.h>
#include <thread/locking/spinlock.h>

MODULE("sys/timer")
TARGET(timers_available, META_DUMMY, {acpi_timer_available})
META_DEFINE_DUMMY()

//! @brief Singly linked list of timers
static struct timer timer_list_head = {0, NULL, NULL, NULL};

//! @brief Register timer
//! @param timer Pointer to the timer
void timer_register(struct timer *timer) {
	timer->next = NULL;
	struct timer *current = &timer_list_head;
	while (current->next != NULL) {
		if (current->next->coolness < timer->coolness) {
			timer->next = current->next;
			current->next = timer;
			return;
		}
	}
	current->next = timer;
}

//! @brief Create timer goal
//! @param ms Number of milliseconds to wait
//! @param goal Buffer to store timer goal in
void timer_make_goal(uint32_t ms, struct timer_goal *goal) {
	struct timer *prev = &timer_list_head;
	while (prev->next != NULL) {
		struct timer *current = prev->next;
		if (!(current->make_goal(current, ms))) {
			continue;
		}
		goal->timer = current;
		return;
	}
	PANIC("No timer available to perform wait operaiton");
}

//! @brief Busy wait on the goal
//! @param goal Pointer to the timer goal
void timer_busy_wait_on_goal(struct timer_goal *goal) {
	while (!timer_poll_goal(goal)) {
		asm volatile("pause");
	}
}

//! @brief Wait for a given number of milliseconds
//! @param ms Number of milliseconds
void timer_busy_wait_ms(uint32_t ms) {
	struct timer_goal goal;
	timer_make_goal(ms, &goal);
	timer_busy_wait_on_goal(&goal);
}

//! @brief Check if timer goal has been reached
//! @param goal Pointer to the timer goal
//! @return True if goal has been reached
bool timer_poll_goal(struct timer_goal *goal) {
	return goal->timer->is_goal_reached(goal->timer);
}
