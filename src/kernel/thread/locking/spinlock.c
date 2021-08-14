//! @file spinlock.h
//! @brief File containing definitions of spinlock functions

#include <thread/locking/spinlock.h>

//! @brief Grab spinlock
//! @param spinlock Pointer to the spinlock
void thread_spinlock_grab(struct thread_spinlock *spinlock) {
	const size_t ticket = ATOMIC_FETCH_INCREMENT_REL(&spinlock->allocated);
	while (ATOMIC_ACQUIRE_LOAD(&spinlock->current) != ticket) {
		asm volatile("pause");
	}
}

//! @brief Ungrab spinlock
//! @param spinlock Pointer to the spinlock
void thread_spinlock_ungrab(struct thread_spinlock *spinlock) {
	const size_t current = ATOMIC_RELAXED_LOAD(&spinlock->current);
	ATOMIC_RELEASE_STORE(&spinlock->current, current + 1);
}

//! @brief Grab spinlock and disable interrupts
//! @param spinlock Pointer to the spinlock
//! @return New interrupt state
bool thread_spinlock_lock(struct thread_spinlock *spinlock) {
	const bool state = intlevel_elevate();
	thread_spinlock_grab(spinlock);
	return state;
}

//! @brief Ungrab spinlock and return to prev int state
//! @param spinlock Pointer to the spinlock
//! @param state Previous interrupt state
void thread_spinlock_unlock(struct thread_spinlock *spinlock, bool state) {
	thread_spinlock_ungrab(spinlock);
	intlevel_recover(state);
}
