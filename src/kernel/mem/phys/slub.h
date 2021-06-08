//! @file physregion.h
//! @brief File containing interface for physical memory slubs

#pragma once

#include <mem/misc.h>   // For config
#include <mem/rc.h>     // For refcounted object
#include <misc/types.h> // For uint_* types

//! @brief Information about busy blocks
struct mem_phys_busy_info {
	//! @brief Next busy block in the slub
	struct mem_phys_busy_info *next;
	//! @brief Prev busy block in the slub
	struct mem_phys_busy_info *prev;
	//! @brief Size of the block
	size_t size;
};

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
	//! @brief Length available to the brk
	size_t brk_length;

#define MEM_PHYS_SLUB_ORDERS_COUNT 64
	//! @brief Free-lists
	uintptr_t free_lists[MEM_PHYS_SLUB_ORDERS_COUNT];
	//! @brief Pointer to blocks
	struct mem_phys_busy_info *busy_list;
	//! @brief True if busy info is saved
	bool keep_busy_info;
	//! @brief Pointer to busy block info array
	struct mem_phys_busy_info *busy_info;
	//! @brief Max freed order
	size_t max_freed_order;
};

//! @brief Initialize mem_phys_slub for allocations from a given memory range
//! @param slub Slub object
//! @param base Physical memory range base
//! @param length Physical memory range length
void mem_phys_slub_init_stage1(struct mem_phys_slub *slub, uintptr_t base, size_t length);

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

//! @brief Second stage of mem_phys_slub initialization. Called when physical direct map is there
//! @param slub Slub object
//! @param bool Keep info about busy blocks
void mem_phys_slub_init_stage2(struct mem_phys_slub *slub, bool keep_busy_info);

//! @brief Get access to physical memory allocated block metadata
//! @param slub Slub object
//! @param addr Address of the block allocated
//! @return Pointer to information about the block
//! @note Subsystem should have been notified about phys direct map being set up using
//! mem_phys_slub_phys_window_ready call. Otherwise, physical memory management subsystem will
//! not maintain busy blocks metadata, since access to busy blocks relies on phys window being
//! set up
struct mem_phys_busy_info *mem_phys_slub_get_range_info(struct mem_phys_slub *slub, uintptr_t addr);
