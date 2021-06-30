//! @file task.h
//! @brief File containing definition of a task structure

#pragma once

#include <lib/pairing_heap.h>
#include <misc/types.h>
#include <sys/arch/interrupts.h>

struct thread_task {
	//! @brief General registers
	struct interrupt_frame frame;
	//! @brief Pairing heap hook
	struct pairing_heap_hook hook;
	//! @brief Task unfairness
	uint64_t unfairness;
	//! @brief Task stack
	uintptr_t stack;
	//! @brief ID of the core task was allocated to
	uint32_t core_id;
};
