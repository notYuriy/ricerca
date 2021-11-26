//! @file rwlock.h
//! @brief File containing mutex-related function declarations
//! @note This particular read-write lock implementation ensures that writers would not starve

#pragma once

#include <lib/queue.h>
#include <thread/locking/spinlock.h>

//! @brief Read-write lock
struct thread_rwlock {
	//! @brief Spinlock
	struct thread_spinlock lock;
	//! @brief Sleep queue
	struct queue sleep_queue;
	//! @brief Read-write lock state
	enum
	{
		//! @brief Read-write lock is not taken
		THREAD_RWLOCK_FREE = 0,
		//! @brief Read-write lock is taken for reading only
		THREAD_RWLOCK_TAKEN_READ = 1,
		//! @brief Read-write lock is taken for writing only
		THREAD_RWLOCK_TAKEN_WRITE = 2,
	} state;
	//! @brief Number of readers
	size_t readers;
};

//! @brief Read-write lock static initializer
#define THREAD_RWLOCK_INIT                                                                         \
	(struct thread_rwlock) {                                                                       \
		.lock = THREAD_SPINLOCK_INIT, .sleep_queue = QUEUE_INIT, .state = THREAD_RWLOCK_FREE,      \
		.readers = 0,                                                                              \
	}

//! @brief Lock rwlock for reading
void thread_rwlock_read(struct thread_rwlock *rwlock);

//! @brief Lock rwlock for writing
void thread_rwlock_write(struct thread_rwlock *rwlock);

//! @brief Unlock rwlock
void thread_rwlock_unlock(struct thread_rwlock *rwlock);