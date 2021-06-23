//! @file mem.h
//! @brief File containing declaration of memory management init function

#pragma once

#include <init/stivale2.h>
#include <lib/target.h>
#include <mem/phys/slab.h>
#include <mem/rc.h>

//! @brief Physical memory range
struct mem_range {
	//! @brief Next memory range in numa node ranges list
	struct mem_range *next_range;
	//! @brief Physical memory slab
	struct mem_phys_slab slab;
};

//! @brief Export "add memory ranges to NUMA nodes" target
EXPORT_TARGET(mem_add_numa_ranges_available)

//! @brief Export kernel memory management init target
EXPORT_TARGET(mem_all_available)
