//! @file mutex.c
//! @brief File containing implementations of mutex functions

#include <thread/locking/mutex.h>
#include <thread/tasking/localsched.h>

//! @brief Wait queue node
struct thread_mutex_wait_queue_node {
	//! @brief Queue node
	struct queue_node node;
	//! @brief Pointer to the task
	struct thread_task *task;
};

//! @brief Lock mutex
void thread_mutex_lock(struct thread_mutex *mutex) {
	const bool int_state = thread_spinlock_lock(&mutex->lock);
	if (!mutex->taken) {
		mutex->taken = true;
		thread_spinlock_unlock(&mutex->lock, int_state);
		return;
	}
	struct thread_mutex_wait_queue_node node;
	node.task = thread_localsched_get_current_task();
	QUEUE_ENQUEUE(&mutex->sleep_queue, &node, node);
	thread_localsched_suspend_current(CALLBACK_VOID(thread_spinlock_ungrab, &mutex->lock));
	intlevel_recover(int_state);
}

//! @brief Unlock mutex
void thread_mutex_unlock(struct thread_mutex *mutex) {
	const bool int_state = thread_spinlock_lock(&mutex->lock);
	struct thread_mutex_wait_queue_node *node =
	    QUEUE_DEQUEUE(&mutex->sleep_queue, struct thread_mutex_wait_queue_node, node);
	if (node == NULL) {
		mutex->taken = false;
	} else {
		thread_localsched_wake_up(node->task);
	}
	thread_spinlock_unlock(&mutex->lock, int_state);
}
