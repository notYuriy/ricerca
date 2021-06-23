//! @file queue.h
//! @brief Declarations of task queue functions

#pragma once

#include <lib/target.h>
#include <thread/locking/spinlock.h>
#include <thread/tasking/task.h>

//! @brief Task queue
struct thread_task_queue {
	//! @brief Task queue head
	struct thread_task *head;
	//! @brief Task queue tail
	struct thread_task *tail;
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

//! @brief Enqueue task in the queue
//! @param queue Queue to enqueue task in
//! @param task Task to enqueue
void thread_task_queue_enqueue(struct thread_task_queue *queue, struct thread_task *task);

//! @brief Try to dequeue task from the queue
//! @param queue Queue to dequeue the task from
//! @return Dequeued task or NULL if task queue is empty
struct thread_task *thread_task_queue_try_dequeue(struct thread_task_queue *queue);

//! @brief Dequeue task from the queue or wait until such task becomes available
//! @param queue Queue to dequeue the task
//! @return Dequeued task
struct thread_task *thread_task_queue_dequeue(struct thread_task_queue *queue);

//! @brief Export target for initializing task queue support
EXPORT_TARGET(thread_tasking_queues_available)
