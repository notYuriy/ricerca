//! @file physregion.h
//! @brief File containing interface for physical memory slubs

#include <lib/panic.h>     // For ASSERT
#include <mem/misc.h>      // For memory management config access
#include <mem/phys/slub.h> // For declarations of functions below

// Module name
#define MODULE "mem/phys/slub"

//! @brief Initialize mem_phys_slub for allocations from a given memory range
//! @param slub Slub object
//! @param base Physical memory range base
//! @param length Physical memory range length
void mem_phys_slub_init_stage1(struct mem_phys_slub *slub, uintptr_t base, size_t length) {
	// Separate memory region in two areas: first area for allocations and second area for busy
	// block metadata
	static const size_t gran_unit_size = 1ULL << PHYS_SLUB_GRAN;
	// per_granularity is how much memory in slub is used per each granular block
	static const size_t per_granularity = gran_unit_size + sizeof(struct mem_phys_busy_info);
	// Calculate how much blocks of this size can fit
	const size_t gran_unit_count = length / per_granularity;
	// Allocate place for those blocks in the end of the memory range we are given
	slub->base = base;
	slub->length = length;
	slub->brk_length = gran_unit_size * gran_unit_count;
	slub->brk_bytes = 0;
	// Zero out free-lists
	for (size_t i = 0; i < 64; ++i) {
		slub->free_lists[i] = PHYS_NULL;
	}
	slub->max_freed_order = 0;
	// Do not keep any busy info yet
	slub->busy_list = NULL;
	slub->busy_info = NULL;
	slub->keep_busy_info = false;
}

//! @brief Physical NULL
#define PHYS_NULL ((uintptr_t)0)

//! @brief Enqueue block in the free list of the given slub object
//! @param slub Slub object
//! @param order Block order
//! @param block Physical address of the block
static void mem_phys_slub_enqueue(struct mem_phys_slub *slub, size_t order, uintptr_t block) {
	if (slub->max_freed_order < order) {
		slub->max_freed_order = order;
	}
	uintptr_t *block_next_ptr = (uintptr_t *)(HIGH_PHYS_VMA + block);
	*block_next_ptr = slub->free_lists[order];
	slub->free_lists[order] = block;
}

//! @brief Dequeue block from the free list
//! @param slub Slub object
//! @param order Block order
//! @return Physical address of the block
static uintptr_t mem_phys_slub_dequeue(struct mem_phys_slub *slub, size_t order) {
	uintptr_t block = slub->free_lists[order];
	uintptr_t next = *(uintptr_t *)(HIGH_PHYS_VMA + block);
	slub->free_lists[order] = next;
	if (next == PHYS_NULL && slub->max_freed_order == order) {
		size_t i = slub->max_freed_order - 1;
		while (i-- > 0) {
			if (slub->free_lists[order] != PHYS_NULL) {
				slub->max_freed_order = order;
			}
		}
	}
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

//! @brief Add busy block info for the memory range
//! @param slub Slub object
//! @param base Block physical base
//! @param size Block size
void mem_phys_slub_add_busy_block(struct mem_phys_slub *slub, uintptr_t base, size_t size) {
	struct mem_phys_busy_info *info = mem_phys_slub_get_range_info(slub, base);
	info->prev = NULL;
	info->next = slub->busy_list;
	slub->busy_list = info;
	slub->busy_list->size = size;
}

//! @brief Calculate size order
//! @param size Size of the memory block requested
static size_t mem_phys_slub_get_order(size_t size) {
	size_t current = (1ULL << PHYS_SLUB_GRAN);
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
	ASSERT(size % (1ULL << PHYS_SLUB_GRAN) == 0,
	       "Attempt to allocate %U bytes (not granular allocation)", size);
	// Get block order
	size_t order = mem_phys_slub_get_order(size);
	if (order == MEM_PHYS_SLUB_ORDERS_COUNT) {
		// TOO large
		return PHYS_NULL;
	}
	// If block is larger than all orders we provide, return PHYS_NULL
	if (order == MEM_PHYS_SLUB_ORDERS_COUNT) {
		return PHYS_NULL;
	}
	// 2 and 3. Iterate free from order to slub->max_freed_order
	for (size_t i = order; i < slub->max_freed_order; ++i) {
		if (slub->free_lists[i] != PHYS_NULL) {
			uintptr_t block = mem_phys_slub_dequeue(slub, i);
			mem_phys_slub_split_until_target(slub, block, i, order);
			// If busy info feature is active, add busy info about just allocated block
			if (slub->keep_busy_info) {
				mem_phys_slub_add_busy_block(slub, block, (1ULL << order));
			}
		}
	}
	// 4. Allocate with brk
	size_t new_brk_bytes = slub->brk_bytes + (1ULL << order);
	if (new_brk_bytes <= slub->brk_length) {
		uintptr_t base = slub->brk_bytes + slub->base;
		slub->brk_bytes = new_brk_bytes;
		// If busy info feature is active, add busy info about just allocated block
		if (slub->keep_busy_info) {
			mem_phys_slub_add_busy_block(slub, base, (1ULL << order));
		}
		return base;
	}
	return PHYS_NULL;
}

//! @brief Deallocate physical memory
//! @param slub Slub object
//! @param addr Addess of the previously allocated physical memory range
//! @param size Size of the memory to be allocated
void mem_phys_slub_free(struct mem_phys_slub *slub, uintptr_t addr, size_t size) {
	// If keep busy info feature is active, we need to remove busy info block
	if (slub->keep_busy_info) {
		struct mem_phys_busy_info *info = mem_phys_slub_get_range_info(slub, addr);
		ASSERT(slub->busy_list != NULL, "Busy list is empty on attempt to free memory range");
		if (info->next != NULL) {
			info->next->prev = info->prev;
		}
		if (info->prev != NULL) {
			info->prev->next = info->next;
		} else {
			slub->busy_list = info->next;
		}
	}
	// Add block to the free list
	size_t order = mem_phys_slub_get_order(size);
	mem_phys_slub_enqueue(slub, order, addr);
}

//! @brief Get access to physical memory allocated block metadata
//! @param slub Slub object
//! @param addr Address of the block allocated
//! @return Pointer to information about the block
//! @note Subsystem should have been notified about phys direct map being set up using
//! mem_phys_slub_init_stage2 call. Otherwise, physical memory management subsystem will
//! not maintain busy blocks metadata, since access to busy blocks relies on phys window being
//! set up
struct mem_phys_busy_info *mem_phys_slub_get_range_info(struct mem_phys_slub *slub,
                                                        uintptr_t addr) {
	ASSERT(slub->keep_busy_info, "Attempt to get busy info while not enabled");
	const uintptr_t relative_offset = addr - slub->base;
	const size_t index_in_busy_table = relative_offset / (1ULL << PHYS_SLUB_GRAN);
	return slub->busy_info + index_in_busy_table;
}

//! @brief Second stage of mem_phys_slub initialization. Called when physical direct map is there
//! @param slub Slub object
//! @param bool Keep info about busy blocks
void mem_phys_slub_init_stage2(struct mem_phys_slub *slub, bool keep_busy_info) {
	if (keep_busy_info) {
		// Allocate space for busy info
		slub->busy_info =
		    (struct mem_phys_busy_info *)(slub->base + slub->brk_length + HIGH_PHYS_VMA);
		slub->keep_busy_info = true;
	} else {
		// Give remaining space to brk
		slub->brk_length = slub->length;
	}
}
