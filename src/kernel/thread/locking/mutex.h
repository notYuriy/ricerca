//! @file mutex.h
//! @brief File containing mutex-related function declarations

#pragma once

#include <lib/queue.h>
#include <thread/locking/spinlock.h>

//! @brief Mutex
struct thread_mutex {
	//! @brief Mutex lock
	struct thread_spinlock lock;
	//! @brief Sleep queue
	struct queue sleep_queue;
	//! @brief True if taken
	bool taken;
};

//! @brief Initialize mutex
void thread_mutex_init(struct thread_mutex *mutex);

//! @brief Lock mutex
void thread_mutex_lock(struct thread_mutex *mutex);

//! @brief Unlock mutex
void thread_mutex_unlock(struct thread_mutex *mutex);
