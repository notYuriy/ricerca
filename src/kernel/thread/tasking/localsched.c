//! @file local_sched.c
//! @brief File containing implementation of the local scheduler

#include <lib/pairing_heap.h>
#include <lib/panic.h>
#include <lib/string.h>
#include <sys/ic.h>
#include <sys/interrupts.h>
#include <sys/tsc.h>
#include <thread/smp/core.h>
#include <thread/tasking/localsched.h>
#include <thread/tasking/schedcall.h>

MODULE("thread/tasking/localsched")
TARGET(thread_localsched_available, thread_localsched_init_target,
       {thread_sched_call_available, thread_tasking_queues_available})

//! @brief Length of the minimal timeslice in us
#define THREAD_LOCAL_TIMESLICE_MIN 10000

//! @brief Length of the default timeslice in us
#define THREAD_LOCAL_TIMESLICE_DEFAULT 20000

//! @brief Copy task state frame to the interrupt frame
//! @param task Task to copy state from
//! @param frame Frame to copy state to
static void thread_localsched_task_to_frame(struct thread_task *task,
                                            struct interrupt_frame *frame) {
	memcpy(frame, &task->frame, sizeof(struct interrupt_frame));
}

//! @brief Copy interrupt frame to the task state frame
//! @param frame Frame to copy state from
//! @param task Task to copy state to
static void thread_localsched_frame_to_task(struct interrupt_frame *frame,
                                            struct thread_task *task) {
	memcpy(&task->frame, frame, sizeof(struct interrupt_frame));
}

//! @brief Pick the length of the timeslice on bootstrap/timer interrupt
//! @param current_unfairness Unfairness of the thread to be run
//! @return Timeslice in us
//! @note Queue should be locked on this call
static uint64_t thread_localsched_pick_timeslice_len(uint64_t current_unfairness) {
	struct thread_localsched_data *data = &PER_CPU(localsched);
	struct thread_task *alternative = thread_task_queue_try_get_nolock(&data->queue);
	if (alternative == NULL) {
		return THREAD_LOCAL_TIMESLICE_DEFAULT;
	}
	uint64_t unfairness_diff = alternative->unfairness - current_unfairness;
	uint64_t us = unfairness_diff / PER_CPU(tsc_freq);
	return us > THREAD_LOCAL_TIMESLICE_MIN ? us : THREAD_LOCAL_TIMESLICE_MIN;
}

//! @brief Waits for the first task to run
//! @param frame Interrupt frame
//! @param ctx Unused
static void thread_localsched_wait_on_boostrap(struct interrupt_frame *frame, void *ctx) {
	(void)ctx;
	struct thread_localsched_data *data = &PER_CPU(localsched);
	// Wait until we have task to run
	bool exited_idle;
	const bool int_state = thread_task_queue_lock(&data->queue);
	struct thread_task *task = thread_task_queue_dequeue(&data->queue, &exited_idle);
	// Calculate optimal timeslice
	uint64_t us = thread_localsched_pick_timeslice_len(task->unfairness);
	thread_task_queue_unlock(&data->queue, int_state);
	// Set up timer event interrup
	ic_timer_one_shot(us);
	// Preempt to the task
	thread_localsched_task_to_frame(task, frame);
	task->timestamp = tsc_read();
	data->current_task = task;
}

//! @brief Initialize local scheduler on this AP
void thread_localsched_init(void) {
	struct thread_localsched_data *data = &PER_CPU(localsched);
	// Initialize task queue
	thread_task_queue_init(&data->queue, PER_CPU(apic_id));
	// Initialize queue fields
	data->current_task = NULL;
	data->tasks_count = 0;
	data->idle_unfairness = 0;
	// Online CPU
	ATOMIC_RELEASE_STORE(&PER_CPU(status), THREAD_SMP_CORE_STATUS_ONLINE);
}

//! @brief Bootstrap local scheduler on this AP
void thread_localsched_bootstrap(void) {
	// Wait for the first task to run on the scheduler stack
	thread_sched_call(thread_localsched_wait_on_boostrap, NULL);
	UNREACHABLE;
}

//! @brief Update unfairness values
//! @param task Task that was running
static void thread_localsched_update_unfairness(struct thread_task *task) {
	struct thread_localsched_data *data = &PER_CPU(localsched);
	// Calculate clock cycle difference
	uint64_t diff = tsc_read() - task->timestamp;
	// Adjust unfairness values
	task->unfairness += diff;
	data->idle_unfairness += diff / data->tasks_count;
}

//! @brief Timer interrupt handler
//! @param frame Interrupt frame
//! @param ctx Ignored
static void thread_localsched_timer_int_handler(struct interrupt_frame *frame, void *ctx) {
	(void)ctx;
	// Save old task data
	struct thread_localsched_data *data = &PER_CPU(localsched);
	struct thread_task *old_task = data->current_task;
	thread_localsched_frame_to_task(frame, old_task);
	// Lock CPU queue
	const bool int_state = thread_task_queue_lock(&data->queue);
	// Update unfairness values
	ASSERT(old_task != NULL, "No active task");
	thread_localsched_update_unfairness(old_task);
	// Put task back in the queue
	thread_task_queue_enqueue_nolock(&data->queue, old_task);
	// Grab a new task to run
	struct thread_task *new_task = thread_task_queue_try_dequeue_nolock(&data->queue);
	if (new_task == NULL) {
		// No other task to run, continue existing one
		new_task = old_task;
	}
	// Pick timeslice length and create new one-shot timer event
	uint64_t us = thread_localsched_pick_timeslice_len(new_task->unfairness);
	ic_timer_one_shot(us);
	// Unlock queue and preempt to the new task
	thread_task_queue_unlock(&data->queue, int_state);
	thread_localsched_task_to_frame(new_task, frame);
	new_task->timestamp = tsc_read();
	data->current_task = new_task;
	// Acknowledge timer interrupt
	ic_timer_ack();
}

//! @brief Preemption callback
//! @param frame Interrupt frame
//! @param ctx If ctx != (void *)0, current task is enqueued back
static void thread_localsched_preemption_handler(struct interrupt_frame *frame, void *ctx) {
	struct thread_localsched_data *data = &PER_CPU(localsched);
	// Save old task data
	struct thread_task *old_task = data->current_task;
	thread_localsched_frame_to_task(frame, old_task);
	// Update unfairness values
	ASSERT(old_task != NULL, "No active task");
	thread_localsched_update_unfairness(old_task);
	bool int_state = thread_task_queue_lock(&data->queue);
	// Put task back in the queue if ctx is not NULL
	if (ctx != NULL) {
		thread_task_queue_enqueue_nolock(&data->queue, old_task);
	} else {
		// Task is not coming back, so store current idle unfairness in acc_unfairness_idle field
		old_task->acc_unfairness_idle = data->idle_unfairness;
	}
	// Grab a new task to run
	data->current_task = NULL;
	bool exited_idle;
	struct thread_task *new_task = thread_task_queue_dequeue(&data->queue, &exited_idle);
	// If exited_idle is true, we get to decide the length of the new timeslice
	if (exited_idle) {
		uint64_t us = thread_localsched_pick_timeslice_len(new_task->unfairness);
		ic_timer_one_shot(us);
	}
	// Unlock queue and preempt to the new task
	thread_task_queue_unlock(&data->queue, int_state);
	thread_localsched_task_to_frame(new_task, frame);
	new_task->timestamp = tsc_read();
	data->current_task = new_task;
}

//! @brief Suspend current task
void thread_localsched_suspend_current(void) {
	thread_sched_call(thread_localsched_preemption_handler, NULL);
}

//! @brief Yield current task
void thread_localsched_yield(void) {
	// Any non-null value would work as ctx
	thread_sched_call(thread_localsched_preemption_handler, (void *)(1));
}

//! @brief Associate task with the local scheduler on the given CPU
//! @param logical_id ID of the core
//! @param task Pointer to the task
void thread_localsched_associate(uint32_t logical_id, struct thread_task *task) {
	// Setting acc_unfairness_idle and unfairness to 0 will make enqueue function add
	// data->idle_unfairness to the unfairness of the just woken up task
	task->unfairness = 0;
	task->acc_unfairness_idle = 0;
	task->core_id = logical_id;
	thread_localsched_wake_up(task);
}

//! @brief Wake up task
//! @param task Pointer to the task to wake up
void thread_localsched_wake_up(struct thread_task *task) {
	struct thread_localsched_data *data = &thread_smp_core_array[task->core_id].localsched;
	const bool int_state = thread_task_queue_lock(&data->queue);
	// Increment task unfairness
	task->unfairness += data->idle_unfairness - task->acc_unfairness_idle;
	// Enqueue task
	data->tasks_count++;
	thread_task_queue_enqueue_signal_nolock(&data->queue, task);
	thread_task_queue_unlock(&data->queue, int_state);
}

//! @brief Termination sched call handler
//! @param frame Interrupt frame
//! @param ctx Ignored
static void thread_localsched_termination_handler(struct interrupt_frame *frame, void *ctx) {
	(void)ctx;
	struct thread_localsched_data *data = &PER_CPU(localsched);
	struct thread_task *old_task = data->current_task;
	// Update unfairness values
	thread_localsched_update_unfairness(old_task);
	// Free task data
	thread_task_dispose(old_task);
	// Lock the queue
	bool int_state = thread_task_queue_lock(&data->queue);
	// Grab a new task to run
	data->current_task = NULL;
	bool exited_idle;
	struct thread_task *new_task = thread_task_queue_dequeue(&data->queue, &exited_idle);
	// If exited_idle is true, we get to decide the length of the new timeslice
	if (exited_idle) {
		uint64_t us = thread_localsched_pick_timeslice_len(new_task->unfairness);
		ic_timer_one_shot(us);
	}
	// Unlock queue and preempt to the new task
	thread_task_queue_unlock(&data->queue, int_state);
	thread_localsched_task_to_frame(new_task, frame);
	new_task->timestamp = tsc_read();
	data->current_task = new_task;
}

//! @brief Terminate current task
attribute_noreturn void thread_localsched_terminate(void) {
	thread_sched_call(thread_localsched_termination_handler, NULL);
	UNREACHABLE;
}

//! @brief Initialize local scheduler
static void thread_localsched_init_target(void) {
	interrupt_register_handler(ic_timer_vec, thread_localsched_timer_int_handler, NULL, 0, 0, true);
}
