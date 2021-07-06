//! @file queue.h
//! @brief Declarations of task queue functions

#pragma once

#include <lib/pairing_heap.h>
#include <lib/target.h>
#include <thread/locking/spinlock.h>
#include <thread/tasking/task.h>

//! @brief Task queue
struct thread_task_queue {
	//! @brief Tasks heap
	struct pairing_heap heap;
	//! @brief Task lock
	struct thread_spinlock lock;
	//! @brief APIC id of the owner
	uint32_t apic_id;
	//! @brief True if CPU is idle (waiting for new tasks to run)
	bool idle;
};

//! @brief Initialize task queue
//! @param queue Queue to initialize
//! @param apic_id CPU APIC id
void thread_task_queue_init(struct thread_task_queue *queue, uint32_t apic_id);

//! @brief Lock queue
//! @param queue Queue to lock
//! @return Interrupt state prior to thread_task_queue_lock call
bool thread_task_queue_lock(struct thread_task_queue *queue);

//! @brief Unlock queue
//! @param queue Queue to unlock
//! @param state Interrupt state returned from thread_task_queue_lock
void thread_task_queue_unlock(struct thread_task_queue *queue, bool int_state);

//! @brief Enqueue task in the queue without locking
//! @param queue Queue to enqueue task in
//! @param task Task to enqueue
void thread_task_queue_enqueue_nolock(struct thread_task_queue *queue, struct thread_task *task);

//! @brief Enqueue task in the queue without locking and signal with interprocessor interrupt
//! @param queue Queue to enqueue task in
//! @param task Task to enqueue
void thread_task_queue_enqueue_signal_nolock(struct thread_task_queue *queue,
                                             struct thread_task *task);

//! @brief Get next task to run without locking
//! @param queue Queue to dequeue the task from
//! @return Dequeued task or NULL if task queue is empty
struct thread_task *thread_task_queue_try_get_nolock(struct thread_task_queue *queue);

//! @brief Dequeue task from the queue without locking
//! @param queue Queue to dequeue the task from
//! @return Dequeued task or NULL if task queue is empty
struct thread_task *thread_task_queue_try_dequeue_nolock(struct thread_task_queue *queue);

//! @brief Dequeue task from the queue or wait until such task becomes available
//! @param queue Queue to dequeue the task
//! @param exited_idle Set to true if core had entered idle state while waiting for the new task
//! @return Dequeued task
//! @note Task queue should be locked before the call to this function and interrupts should be
//! disabled
struct thread_task *thread_task_queue_dequeue(struct thread_task_queue *queue, bool *exited_idle);

//! @brief Export target for initializing task queue support
EXPORT_TARGET(thread_tasking_queues_available)
