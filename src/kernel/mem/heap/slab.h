//! @file haep.h
//! @brief File containing definition for heap slab structure

#pragma once

//! @brief Slab orders count
#define MEM_HEAP_SLAB_ORDERS 12

//! @brief Heap slab's data
struct mem_heap_slab_data {
	//! @brief Free lists
	struct mem_heap_obj *free_lists[MEM_HEAP_SLAB_ORDERS];
	//! @brief Allocated and not-yet used slabs
	struct mem_heap_slab_hdr *slabs;
};

//! @brief Static slab data init
#define MEM_HEAP_SLAB_DATA_INIT                                                                    \
	(struct mem_heap_slab_data) {                                                                  \
		.free_lists = {0}, .slabs = 0                                                              \
	}
