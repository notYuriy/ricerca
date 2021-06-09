//! @file mem.h
//! @brief File containing declaration of memory management init function

#pragma once

#include <init/stivale2.h> // For stivale2 memory map
#include <mem/phys/slub.h> // For physical memory slub
#include <mem/rc.h>        // For reference counting

//! @brief Physical memory range
struct mem_range {
	//! @brief Refcounted base
	struct mem_rc rc_base;
	//! @brief Next memory range for this node
	struct mem_range *next_range;
	//! @brief Previous memory range for this ndoe
	struct mem_range *prev_range;
	//! @brief Physical memory slub
	struct mem_phys_slub slub;
	//! @brief True if memory range was offlined
	bool offlined;
};

//! @brief Initialize memory management
//! @param memmap Stivale2 memory map tag
void mem_init(struct stivale2_struct_tag_memmap *memmap);
