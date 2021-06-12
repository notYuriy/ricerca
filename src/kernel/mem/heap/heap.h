//! @file heap.h
//! @brief File containing declarations for heap allocation functions

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <sys/numa/numa.h>

//! @brief Allocate memory on behalf of a given node
//! @param size Size of the memory to be allocated
//! @param id Locality to which memory will belong
//! @return NULL pointer if allocation failed, pointer to the virtual memory of size "size"
//! otherwise
void *mem_heap_alloc(size_t size, numa_id_t id);

//! @brief Free memory on behalf of a given node
//! @param ptr Pointer to the previously allocated memory
//! @param size Size of the allocated memory
void *mem_heap_free(void *mem, size_t size);

//! @brief Target to initialize kernel heap
EXPORT_TARGET(mem_heap_target)