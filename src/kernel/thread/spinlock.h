//! @file spinlock.h
//! @brief File containing declarations of spinlock functions

#pragma once

#include <misc/atomics.h>   // For atomic operations
#include <sys/interrupts.h> // For interrupts functions

//! @brief Spinlock
struct thread_spinlock {
	//! @brief Current ticket for which access to the resource is allowed
	size_t current;
	//! @brief Last allocated ticket
	size_t allocated;
};

//! @brief Spinlock initialization value. Can be assigned to a spinlock variable to initialize it
#define THREAD_SPINLOCK_INIT                                                                       \
	{ 0, 0 }

//! @brief Lock spinlock
//! @param spinlock Pointer to the spinlock
//! @return Interrupts state
static inline bool thread_spinlock_lock(struct thread_spinlock *spinlock) {
	const bool state = interrupts_disable();
	const size_t ticket = ATOMIC_FETCH_INCREMENT(&spinlock->allocated);
	while (ATOMIC_ACQUIRE_LOAD(&spinlock->current) < ticket) {
		asm volatile("pause");
	}
	return state;
}

//! @brief Unlock spinlock
//! @param spinlock Pointer to the spinlock
//! @param state Interrupts state
static inline void thread_spinlock_unlock(struct thread_spinlock *spinlock, bool state) {
	ATOMIC_FETCH_INCREMENT(&spinlock->current);
	interrupts_enable(state);
}
