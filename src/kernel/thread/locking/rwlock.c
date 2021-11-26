//! @file rwlock.c
//! @brief Implementation of read-write lock

#include <thread/locking/rwlock.h>
#include <thread/tasking/localsched.h>

//! @brief Wait queue node
struct thread_rwlock_wait_queue_node {
	//! @brief Queue node
	struct queue_node node;
	//! @brief Pointer to the task
	struct thread_task *task;
	//! @brief True if locked for writing
	bool writing;
};

//! @brief Lock rwlock for reading
void thread_rwlock_read(struct thread_rwlock *rwlock) {
	bool int_state = thread_spinlock_lock(&rwlock->lock);
	if (rwlock->state == THREAD_RWLOCK_FREE) {
		rwlock->state = THREAD_RWLOCK_TAKEN_READ;
		thread_spinlock_unlock(&rwlock->lock, int_state);
		return;
	} else if (rwlock->state == THREAD_RWLOCK_TAKEN_READ && rwlock->sleep_queue.head == NULL) {
		rwlock->readers++;
		thread_spinlock_unlock(&rwlock->lock, int_state);
		return;
	}
	struct thread_rwlock_wait_queue_node node;
	node.task = thread_localsched_get_current_task();
	node.writing = false;
	QUEUE_ENQUEUE(&rwlock->sleep_queue, &node, node);
	thread_localsched_suspend_current(CALLBACK_VOID(thread_spinlock_ungrab, &rwlock->lock));
	intlevel_recover(int_state);
}

//! @brief Lock rwlock for writing
void thread_rwlock_write(struct thread_rwlock *rwlock) {
	const bool int_state = thread_spinlock_lock(&rwlock->lock);
	if (rwlock->state == THREAD_RWLOCK_FREE) {
		rwlock->state = THREAD_RWLOCK_TAKEN_WRITE;
		thread_spinlock_unlock(&rwlock->lock, int_state);
		return;
	}
	struct thread_rwlock_wait_queue_node node;
	node.task = thread_localsched_get_current_task();
	node.writing = true;
	QUEUE_ENQUEUE(&rwlock->sleep_queue, &node, node);
	thread_localsched_suspend_current(CALLBACK_VOID(thread_spinlock_ungrab, &rwlock->lock));
	intlevel_recover(int_state);
}

//! @brief Unlock rwlock
void thread_rwlock_unlock(struct thread_rwlock *rwlock) {
	const bool int_state = thread_spinlock_lock(&rwlock->lock);
	if (rwlock->state == THREAD_RWLOCK_TAKEN_READ) {
		rwlock->readers--;
		if (rwlock->readers != 0) {
			// We can let some other reader do our job of waking up other threads
			thread_spinlock_unlock(&rwlock->lock, int_state);
			return;
		}
	}
	struct thread_rwlock_wait_queue_node *node =
	    QUEUE_DEQUEUE(&rwlock->sleep_queue, struct thread_rwlock_wait_queue_node, node);
	if (node == NULL) {
		rwlock->state = THREAD_RWLOCK_FREE;
	} else if (node->writing) {
		rwlock->state = THREAD_RWLOCK_TAKEN_WRITE;
		thread_localsched_wake_up(node->task);
	} else {
		rwlock->state = THREAD_RWLOCK_TAKEN_READ;
		// Assemble queue of readers
		struct queue readers_queue = QUEUE_INIT;
		size_t new_readers_count = 0;
		do {
			new_readers_count++;
			QUEUE_ENQUEUE(&readers_queue, node, node);
			node = QUEUE_DEQUEUE(&rwlock->sleep_queue, struct thread_rwlock_wait_queue_node, node);
		} while (node != NULL && !node->writing);
		rwlock->readers = new_readers_count;
		// The town wakes up.
		while ((node = QUEUE_DEQUEUE(&readers_queue, struct thread_rwlock_wait_queue_node, node)) !=
		       NULL) {
			thread_localsched_wake_up(node->task);
		}
	}
	thread_spinlock_unlock(&rwlock->lock, int_state);
}