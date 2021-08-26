//! @file bootstrap.h
//! @brief File containing declarations of bootstrap allocator functions

#pragma once

#include <lib/target.h>
#include <mem/rc.h>
#include <misc/types.h>

//! @brief Allocate memory on bootstrap
//! @param size Size of the allocation
//! @return Pointer to the allocated memory. Can be assumed to be non-null
void *mem_bootstrap_alloc(size_t size);

//! @brief Terminate bootstrap allocator
//! @return Physical address beyond which no bootstrap allocations are placed
uintptr_t mem_bootstrap_terminate_allocator(void);

//! @brief Allocate reference-counted object on bootstrap
//! @param size Size of the allocation
//! @return Pointer to the allocated memory
static inline void *mem_bootstrap_alloc_rc(size_t size) {
	struct mem_rc *result = mem_bootstrap_alloc(size);
	MEM_REF_INIT(result, NULL);
	return result;
}

//! @brief Allocate reference counted object on bootstrap
#define MEM_BOOTSTRAP_OBJ_ALLOC(T) (T *)(mem_bootstrap_alloc_rc(sizeof(T)))

//! @brief Export target for bootstrap allocation initialization
EXPORT_TARGET(mem_bootstrap_alloc_available)
