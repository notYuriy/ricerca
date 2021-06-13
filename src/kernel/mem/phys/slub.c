//! @file physregion.h
//! @brief File containing interface for physical memory slubs

#include <lib/panic.h>
#include <lib/target.h>
#include <mem/misc.h>
#include <mem/phys/slub.h>

MODULE("mem/phys/slub")

//! @brief Initialize mem_phys_slub for allocations from a given memory range
//! @param slub Slub object
//! @param base Physical memory range base
//! @param length Physical memory range length
void mem_phys_slub_init(struct mem_phys_slub *slub, uintptr_t base, size_t length) {
	slub->base = base;
	slub->length = length;
	slub->brk_bytes = 0;
	// Zero out free-lists
	for (size_t i = 0; i < 64; ++i) {
		slub->free_lists[i] = PHYS_NULL;
	}
	slub->max_freed_order = 0;
}

//! @brief Physical NULL
#define PHYS_NULL ((uintptr_t)0)

//! @brief Enqueue block in the free list of the given slub object
//! @param slub Slub object
//! @param order Block order
//! @param block Physical address of the block
static void mem_phys_slub_enqueue(struct mem_phys_slub *slub, size_t order, uintptr_t block) {
	ASSERT(block < mem_wb_phys_win_base, "Block in higher half");
	if (slub->max_freed_order < order) {
		slub->max_freed_order = order;
	}
	uintptr_t *block_next_ptr = (uintptr_t *)(mem_wb_phys_win_base + block);
	*block_next_ptr = slub->free_lists[order];
	slub->free_lists[order] = block;
}

//! @brief Dequeue block from the free list
//! @param slub Slub object
//! @param order Block order
//! @return Physical address of the block
static uintptr_t mem_phys_slub_dequeue(struct mem_phys_slub *slub, size_t order) {
	uintptr_t block = slub->free_lists[order];
	uintptr_t next = *(uintptr_t *)(mem_wb_phys_win_base + block);
	slub->free_lists[order] = next;
	if (next == PHYS_NULL && slub->max_freed_order == order) {
		size_t i = slub->max_freed_order - 1;
		slub->max_freed_order = 0;
		while (i-- > 0) {
			if (slub->free_lists[i] != PHYS_NULL) {
				slub->max_freed_order = i;
			}
		}
	}
	ASSERT(block < mem_wb_phys_win_base, "Block in higher half");
	return block;
}

//! @brief Split block of physical memory of a given order to the order needed
//! @param slub Slub object
//! @param base Block physical base
//! @param order Block order
//! @param target Target order needed
static void mem_phys_slub_split_until_target(struct mem_phys_slub *slub, uintptr_t base,
                                             size_t order, size_t target) {
	while ((order--) > target) {
		const uintptr_t neighbour = base + (1ULL << order);
		mem_phys_slub_enqueue(slub, order, neighbour);
	}
}

//! @brief Calculate size order
//! @param size Size of the memory block requested
static size_t mem_phys_slub_get_order(size_t size) {
	size_t current = PAGE_SIZE;
	size_t order = PHYS_SLUB_GRAN;
	for (; order < MEM_PHYS_SLUB_ORDERS_COUNT; ++order, current *= 2) {
		if (size <= current) {
			return order;
		}
	}
	return MEM_PHYS_SLUB_ORDERS_COUNT;
}

//! @brief Allocate physical memory
//! @param slub Slub object
//! @param size Size of the memory to be allocated
//! @return Allocated memory or PHYS_NULL if there is not enough memory
uintptr_t mem_phys_slub_alloc(struct mem_phys_slub *slub, size_t size) {
	// Allocation algorithm is very simple
	// 1. Find which free list we need to query
	// 2. Return block from that free list if its not empty
	// 3. Split blocks from higher order free lists if those are not empty
	// 4. If all free lists are empty, attempt to allocate by incerasing slub->brk_base by size

	// Find free list to query. Check that size is granular
	ASSERT(size % PAGE_SIZE == 0, "Attempt to allocate %U bytes (not granular allocation)", size);
	// Get block order
	size_t order = mem_phys_slub_get_order(size);
	// If block is larger than all orders we provide, return PHYS_NULL
	if (order == MEM_PHYS_SLUB_ORDERS_COUNT) {
		return PHYS_NULL;
	}
	// 2 and 3. Iterate free from order to slub->max_freed_order
	for (size_t i = order; i <= slub->max_freed_order; ++i) {
		if (slub->free_lists[i] != PHYS_NULL) {
			uintptr_t block = mem_phys_slub_dequeue(slub, i);
			mem_phys_slub_split_until_target(slub, block, i, order);
			return block;
		}
	}
	// 4. Allocate with brk
	size_t new_brk_bytes = slub->brk_bytes + (1ULL << order);
	if (new_brk_bytes <= slub->length) {
		uintptr_t base = slub->brk_bytes + slub->base;
		slub->brk_bytes = new_brk_bytes;
		return base;
	}
	return PHYS_NULL;
}

//! @brief Deallocate physical memory
//! @param slub Slub object
//! @param addr Addess of the previously allocated physical memory range
//! @param size Size of the memory to be allocated
void mem_phys_slub_free(struct mem_phys_slub *slub, uintptr_t addr, size_t size) {
	// Add block to the free list
	size_t order = mem_phys_slub_get_order(size);
	mem_phys_slub_enqueue(slub, order, addr);
}
