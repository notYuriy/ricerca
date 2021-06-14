//! @brief timer.c
//! @file File containing mplementations of generic timing functions

#include <lib/log.h>
#include <lib/panic.h>
#include <sys/timers/acpi.h>
#include <sys/timers/timer.h>

MODULE("sys/timer")
TARGET(timers_target, META_DUMMY, {acpi_timer_detection_target})
META_DEFINE_DUMMY()

//! @brief Singly linked list of timers
static struct timer timer_list_head = {0, NULL, NULL};

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

//! @brief Wait for a given number of milliseconds
//! @param ms Number of milliseconds
void timer_wait_ms(uint32_t ms) {
	struct timer *current = &timer_list_head;
	while (current->next != NULL) {
		if (current->next->wait_callback(current->next, ms)) {
			return;
		}
	}
	PANIC("No timer available to perform wait operaiton");
}
