//! @file haep.h
//! @brief File containing definition for heap slub structure

#pragma once

//! @brief Slub orders count
#define MEM_HEAP_SLUB_ORDERS 12

//! @brief Heap slub's data
struct mem_heap_slub_data {
	//! @brief Free lists
	struct mem_heap_obj *free_lists[MEM_HEAP_SLUB_ORDERS];
	//! @brief Allocated and not-yet used slubs
	struct mem_heap_slub_hdr *slubs;
};

//! @brief Static slub data init
#define MEM_HEAP_SLUB_DATA_INIT                                                                    \
	(struct mem_heap_slub_data) {                                                                  \
		.free_lists = {0}, .slubs = 0                                                              \
	}
