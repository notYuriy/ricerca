//! @file task.h
//! @brief File containing definition of a task structure

#pragma once

#include <lib/pairing_heap.h>
#include <misc/types.h>
#include <sys/arch/interrupts.h>

//! @brief Task stack size
#define THREAD_TASK_STACK_SIZE 0x10000

struct thread_task {
	//! @brief General registers
	struct interrupt_frame frame;
	//! @brief Pairing heap hook
	struct pairing_heap_hook hook;
	//! @brief Task unfairness (in clock cycles)
	uint64_t unfairness;
	//! @brief Accumulated idle unfairness (in clock cycles)
	uint64_t acc_unfairness_idle;
	//! @brief Timestamp
	uint64_t timestamp;
	//! @brief Task stack
	uintptr_t stack;
	//! @brief ID of the core task was allocated to
	uint32_t core_id;
};

//! @brief Create task with a given entrypoint
//! @param func Function to be called
//! @param arg First argument
//! @return Pointer to the created task or NULL on failure
struct thread_task *thread_task_create_call(void *func, void *arg);

//! @brief Dispose task
//! @param task Pointer to the task
void thread_task_dispose(struct thread_task *task);
