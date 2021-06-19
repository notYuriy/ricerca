//! @file physregion.h
//! @brief File containing interface for physical memory slabs

#pragma once

#include <mem/misc.h>
#include <mem/rc.h>
#include <misc/types.h>

//! @brief One physical memory slab on a given range of physical memory
struct mem_phys_slab {
	//! @brief Refcounted base
	struct mem_rc rc_base;
	//! @brief Pointer to the next memory slab available
	struct mem_phys_slab *next;
	//! @brief Physical memory region base
	uintptr_t base;
	//! @brief Bytes allocated with brk
	size_t brk_bytes;
	//! @brief Length of the memory region
	size_t length;

#define MEM_PHYS_SLAB_ORDERS_COUNT 64
	//! @brief Free-lists
	uintptr_t free_lists[MEM_PHYS_SLAB_ORDERS_COUNT];
	//! @brief Max freed order
	size_t max_freed_order;
};

//! @brief Initialize mem_phys_slab for allocations from a given memory range
//! @param slab Slab object
//! @param base Physical memory range base
//! @param length Physical memory range length
void mem_phys_slab_init(struct mem_phys_slab *slab, uintptr_t base, size_t length);

//! @brief Physical NULL
#define PHYS_NULL ((uintptr_t)0)

//! @brief Allocate physical memory
//! @param slab Slab object
//! @param size Size of the memory to be allocated
//! @return Allocated memory or PHYS_NULL if there is not enough memory
uintptr_t mem_phys_slab_alloc(struct mem_phys_slab *slab, size_t size);

//! @brief Deallocate physical memory
//! @param slab Slab object
//! @param addr Addess of the previously allocated physical memory range
//! @param size Size of the memory to be allocated
void mem_phys_slab_free(struct mem_phys_slab *slab, uintptr_t addr, size_t size);
