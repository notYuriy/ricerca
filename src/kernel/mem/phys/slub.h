//! @file physregion.h
//! @brief File containing interface for physical memory slubs

#pragma once

#include <mem/misc.h>
#include <mem/rc.h>
#include <misc/types.h>

//! @brief One physical memory slub on a given range of physical memory
struct mem_phys_slub {
	//! @brief Refcounted base
	struct mem_rc rc_base;
	//! @brief Pointer to the next memory slub available
	struct mem_phys_slub *next;
	//! @brief Physical memory region base
	uintptr_t base;
	//! @brief Bytes allocated with brk
	size_t brk_bytes;
	//! @brief Length of the memory region
	size_t length;

#define MEM_PHYS_SLUB_ORDERS_COUNT 64
	//! @brief Free-lists
	uintptr_t free_lists[MEM_PHYS_SLUB_ORDERS_COUNT];
	//! @brief Max freed order
	size_t max_freed_order;
};

//! @brief Initialize mem_phys_slub for allocations from a given memory range
//! @param slub Slub object
//! @param base Physical memory range base
//! @param length Physical memory range length
void mem_phys_slub_init(struct mem_phys_slub *slub, uintptr_t base, size_t length);

//! @brief Physical NULL
#define PHYS_NULL ((uintptr_t)0)

//! @brief Allocate physical memory
//! @param slub Slub object
//! @param size Size of the memory to be allocated
//! @return Allocated memory or PHYS_NULL if there is not enough memory
uintptr_t mem_phys_slub_alloc(struct mem_phys_slub *slub, size_t size);

//! @brief Deallocate physical memory
//! @param slub Slub object
//! @param addr Addess of the previously allocated physical memory range
//! @param size Size of the memory to be allocated
void mem_phys_slub_free(struct mem_phys_slub *slub, uintptr_t addr, size_t size);
