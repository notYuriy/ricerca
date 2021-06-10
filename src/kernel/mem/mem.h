//! @file mem.h
//! @brief File containing declaration of memory management init function

#pragma once

#include <init/stivale2.h>
#include <lib/target.h>
#include <mem/phys/slub.h>
#include <mem/rc.h>

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

//! @brief Export "add memory ranges to NUMA nodes" target
EXPORT_TARGET(mem_add_numa_ranges_target)

//! @brief Export kernel memory management init target
EXPORT_TARGET(mem_kern_init_target)
