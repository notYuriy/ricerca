//! @file physregion.h
//! @brief File containing interface for physical memory slabs

#include <lib/panic.h>
#include <lib/target.h>
#include <mem/misc.h>
#include <mem/phys/slab.h>

MODULE("mem/phys/slab")

//! @brief Initialize mem_phys_slab for allocations from a given memory range
//! @param slab Slab object
//! @param base Physical memory range base
//! @param length Physical memory range length
void mem_phys_slab_init(struct mem_phys_slab *slab, uintptr_t base, size_t length) {
	slab->base = base;
	slab->length = length;
	slab->brk_bytes = 0;
	// Zero out free-lists
	for (size_t i = 0; i < 64; ++i) {
		slab->free_lists[i] = PHYS_NULL;
	}
	slab->max_freed_order = 0;
}

//! @brief Physical NULL
#define PHYS_NULL ((uintptr_t)0)

//! @brief Enqueue block in the free list of the given slab object
//! @param slab Slab object
//! @param order Block order
//! @param block Physical address of the block
static void mem_phys_slab_enqueue(struct mem_phys_slab *slab, size_t order, uintptr_t block) {
	ASSERT(block < mem_wb_phys_win_base, "Block in higher half");
	if (slab->max_freed_order < order) {
		slab->max_freed_order = order;
	}
	uintptr_t *block_next_ptr = (uintptr_t *)(mem_wb_phys_win_base + block);
	*block_next_ptr = slab->free_lists[order];
	slab->free_lists[order] = block;
}

//! @brief Dequeue block from the free list
//! @param slab Slab object
//! @param order Block order
//! @return Physical address of the block
static uintptr_t mem_phys_slab_dequeue(struct mem_phys_slab *slab, size_t order) {
	uintptr_t block = slab->free_lists[order];
	uintptr_t next = *(uintptr_t *)(mem_wb_phys_win_base + block);
	slab->free_lists[order] = next;
	if (next == PHYS_NULL && slab->max_freed_order == order) {
		size_t i = slab->max_freed_order - 1;
		slab->max_freed_order = 0;
		while (i-- > 0) {
			if (slab->free_lists[i] != PHYS_NULL) {
				slab->max_freed_order = i;
			}
		}
	}
	ASSERT(block < mem_wb_phys_win_base, "Block in higher half");
	return block;
}

//! @brief Split block of physical memory of a given order to the order needed
//! @param slab Slab object
//! @param base Block physical base
//! @param order Block order
//! @param target Target order needed
static void mem_phys_slab_split_until_target(struct mem_phys_slab *slab, uintptr_t base,
                                             size_t order, size_t target) {
	while ((order--) > target) {
		const uintptr_t neighbour = base + (1ULL << order);
		mem_phys_slab_enqueue(slab, order, neighbour);
	}
}

//! @brief Calculate size order
//! @param size Size of the memory block requested
static size_t mem_phys_slab_get_order(size_t size) {
	size_t current = PAGE_SIZE;
	size_t order = PHYS_SLAB_GRAN;
	for (; order < MEM_PHYS_SLAB_ORDERS_COUNT; ++order, current *= 2) {
		if (size <= current) {
			return order;
		}
	}
	return MEM_PHYS_SLAB_ORDERS_COUNT;
}

//! @brief Allocate physical memory
//! @param slab Slab object
//! @param size Size of the memory to be allocated
//! @return Allocated memory or PHYS_NULL if there is not enough memory
uintptr_t mem_phys_slab_alloc(struct mem_phys_slab *slab, size_t size) {
	// Allocation algorithm is very simple
	// 1. Find which free list we need to query
	// 2. Return block from that free list if its not empty
	// 3. Split blocks from higher order free lists if those are not empty
	// 4. If all free lists are empty, attempt to allocate by incerasing slab->brk_base by size
	// Get block order
	size_t order = mem_phys_slab_get_order(size);
	// If block is larger than all orders we provide, return PHYS_NULL
	if (order == MEM_PHYS_SLAB_ORDERS_COUNT) {
		return PHYS_NULL;
	}
	// 2 and 3. Iterate free from order to slab->max_freed_order
	for (size_t i = order; i <= slab->max_freed_order; ++i) {
		if (slab->free_lists[i] != PHYS_NULL) {
			uintptr_t block = mem_phys_slab_dequeue(slab, i);
			mem_phys_slab_split_until_target(slab, block, i, order);
			return block;
		}
	}
	// 4. Allocate with brk
	size_t new_brk_bytes = slab->brk_bytes + (1ULL << order);
	if (new_brk_bytes <= slab->length) {
		uintptr_t base = slab->brk_bytes + slab->base;
		slab->brk_bytes = new_brk_bytes;
		return base;
	}
	return PHYS_NULL;
}

//! @brief Deallocate physical memory
//! @param slab Slab object
//! @param addr Addess of the previously allocated physical memory range
//! @param size Size of the memory to be allocated
void mem_phys_slab_free(struct mem_phys_slab *slab, uintptr_t addr, size_t size) {
	// Add block to the free list
	size_t order = mem_phys_slab_get_order(size);
	mem_phys_slab_enqueue(slab, order, addr);
}
