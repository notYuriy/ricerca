//! @file spinlock.h
//! @brief File containing declarations of spinlock functions

#pragma once

#include <lib/callback.h>
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

//! @brief Grab spinlock
//! @param spinlock Pointer to the spinlock
void thread_spinlock_grab(struct thread_spinlock *spinlock);

//! @brief Ungrab spinlock
//! @param spinlock Pointer to the spinlock
void thread_spinlock_ungrab(struct thread_spinlock *spinlock);

//! @brief Grab spinlock and disable interrupts
//! @param spinlock Pointer to the spinlock
//! @return New interrupt state
bool thread_spinlock_lock(struct thread_spinlock *spinlock);

//! @brief Ungrab spinlock and return to prev int state
//! @param spinlock Pointer to the spinlock
//! @param state Previous interrupt state
void thread_spinlock_unlock(struct thread_spinlock *spinlock, bool state);
