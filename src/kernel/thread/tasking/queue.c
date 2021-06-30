//! @file queue.c
//! @brief Implementation of task queue functions

#include <lib/containerof.h>
#include <lib/panic.h>
#include <misc/atomics.h>
#include <sys/arch/arch.h>
#include <sys/arch/interrupts.h>
#include <sys/arch/tss.h>
#include <sys/ic.h>
#include <thread/tasking/queue.h>

MODULE("thread/tasking/queue")
TARGET(thread_tasking_queues_available, thread_tasking_queues_init,
       {ic_bsp_available, arch_available})

//! @brief Interrupt vector for enqueue IPIs
static uint8_t thread_task_queue_ipi_vec = 0x69;

//! @brief Dummy IPI callback
static void thread_task_ipi_dummy(struct interrupt_frame *frame, void *ctx) {
	(void)frame;
	(void)ctx;
}

//! @brief Task unfairness comparator for heap insertions/deletitions
//! @param left Left handside
//! @param right Right handside
//! @return True if left unfairness is smaller than that of right
static bool thread_compare_unfairness(struct pairing_heap_hook *left,
                                      struct pairing_heap_hook *right) {
	struct thread_task *ltask = CONTAINER_OF(left, struct thread_task, hook);
	struct thread_task *rtask = CONTAINER_OF(right, struct thread_task, hook);
	return ltask->unfairness < rtask->unfairness;
}

//! @brief Initialize task queue
//! @param queue Queue to initialize
//! @param apic_id CPU APIC id
void thread_task_queue_init(struct thread_task_queue *queue, uint32_t apic_id) {
	queue->apic_id = apic_id;
	pairing_heap_init(&queue->heap, thread_compare_unfairness);
	queue->lock = THREAD_SPINLOCK_INIT;
}

//! @brief Enqueue task in the queue
//! @param queue Queue to enqueue task in
//! @param task Task to enqueue
void thread_task_queue_enqueue(struct thread_task_queue *queue, struct thread_task *task) {
	const bool int_state = thread_spinlock_lock(&queue->lock);
	pairing_heap_insert(&queue->heap, &task->hook);
	thread_spinlock_unlock(&queue->lock, int_state);
	if (ATOMIC_ACQUIRE_LOAD(&queue->idle)) {
		ic_send_ipi(queue->apic_id, thread_task_queue_ipi_vec);
	}
}

//! @brief Try to dequeue task from the queue
//! @param queue Queue to dequeue the task from
//! @return Dequeued task or NULL if task queue is empty
struct thread_task *thread_task_queue_try_dequeue(struct thread_task_queue *queue) {
	const bool int_state = thread_spinlock_lock(&queue->lock);
	struct pairing_heap_hook *res = pairing_heap_remove_min(&queue->heap);
	thread_spinlock_unlock(&queue->lock, int_state);
	return res != NULL ? CONTAINER_OF(res, struct thread_task, hook) : NULL;
}

//! @brief Dequeue task from the queue or wait until such task becomes available
//! @param queue Queue to dequeue the task
//! @return Dequeued task
struct thread_task *thread_task_queue_dequeue(struct thread_task_queue *queue) {
	ATOMIC_RELEASE_STORE(&queue->idle, true);
	while (true) {
		struct thread_task *result = thread_task_queue_try_dequeue(queue);
		if (result != NULL) {
			ATOMIC_RELEASE_STORE(&queue->idle, true);
		}
		asm volatile("sti\n\r"
		             "hlt\n\r"
		             "cli\n\r");
	}
}

//! @brief Initialize task queues subsystem
void thread_tasking_queues_init(void) {
	interrupt_register_handler(thread_task_queue_ipi_vec, thread_task_ipi_dummy, NULL, 0,
	                           TSS_INT_IST, true);
}
