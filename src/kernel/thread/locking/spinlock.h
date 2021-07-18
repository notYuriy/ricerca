//! @file spinlock.h
//! @brief File containing declarations of spinlock functions

#pragma once

#include <lib/panic.h>
#include <misc/atomics.h>
#include <sys/cr.h>
#include <sys/intlevel.h>

//! @brief Spinlock
struct thread_spinlock {
	//! @brief Current ticket for which access to the resource is allowed
	size_t current;
	//! @brief Last allocated ticket
	size_t allocated;
};

//! @brief Spinlock initialization value. Can be assigned to a spinlock variable to initialize it
#define THREAD_SPINLOCK_INIT                                                                       \
	(struct thread_spinlock) {                                                                     \
		0, 0                                                                                       \
	}

//! @brief Lock spinlock
//! @param spinlock Pointer to the spinlock
//! @return Interrupts state
static inline bool thread_spinlock_lock(struct thread_spinlock *spinlock) {
	const bool state = intlevel_elevate();
	const size_t ticket = ATOMIC_FETCH_INCREMENT_REL(&spinlock->allocated);
	while (ATOMIC_ACQUIRE_LOAD(&spinlock->current) != ticket) {
		asm volatile("pause");
	}
	return state;
}

//! @brief Unlock spinlock
//! @param spinlock Pointer to the spinlock
//! @param state Interrupts state
static inline void thread_spinlock_unlock(struct thread_spinlock *spinlock, bool state) {
	const size_t current = ATOMIC_RELAXED_LOAD(&spinlock->current);
	ATOMIC_RELEASE_STORE(&spinlock->current, current + 1);
	intlevel_recover(state);
}
