//! @file heap.h
//! @brief File containing declarations for heap allocation functions

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <sys/numa/numa.h>

//! @brief Allocate memory
//! @param size Size of the memory to be allocated
//! @return NULL pointer if allocation failed, pointer to the virtual memory of size "size"
//! otherwise
void *mem_heap_alloc(size_t size);

//! @brief Free memory on behalf of a given node
//! @param ptr Pointer to the previously allocated memory
//! @param size Size of the allocated memory
void mem_heap_free(void *mem, size_t size);

//! @brief Reallocate memory to a new region with new size
//! @param mem Pointer to the memory
//! @param newsize New size
//! @param oldsize Old size
//! @return Pointer to the new memory or NULL if realloc failed
void *mem_heap_realloc(void *mem, size_t newsize, size_t oldsize);

//! @brief Target to initialize kernel heap
EXPORT_TARGET(mem_heap_target)
