//! @file profiling.c
//! @brief Implementation of mcount function

#include <lib/profiling.h>
#include <mem/heap/heap.h>
#include <thread/smp/core.h>

TARGET(profiling_available, profiling_init, {mem_heap_available, thread_smp_core_available})

//! @brief True if profiling is enabled
static bool profiling_is_enabled = false;

//! @brief mcount hook called on every function call
attribute_no_instrument void mcount() {
	if (!profiling_is_enabled) {
		return;
	}
}

//! @brief Initialize profiling
static void profiling_init() {
	profiling_is_enabled = true;
}
