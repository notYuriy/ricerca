//! @file log.c
//! @brief File containing miscelanious memory management related declarations

#pragma once

#include <lib/target.h>

//! @brief Physical slab granularity as power of 2
#define PHYS_SLAB_GRAN 12ULL

//! @brief Page size
#define PAGE_SIZE (1ULL << PHYS_SLAB_GRAN)

//! @brief End of low physical memory
#define PHYS_LOW (2 * 1024 * 1024)

//! @brief Size of initial direct phys mapping
#define INIT_PHYS_MAPPING_SIZE 0x100000000

//! @brief Maximum number of static memory range
#define MEM_MAX_RANGES_STATIC 16384

//! @brief Writeback physical window base
extern uintptr_t mem_wb_phys_win_base;

//! @brief True if 5 level paging is enabled
extern bool mem_5level_paging_enabled;

//! @brief Physical space size
extern size_t mem_phys_space_size;

//! @brief Export boot memory-related info detection routine
//! @note Required as a dependency to use mem_wb_phys_win_base
//! @note Required as a dependency to use mem_5level_paging_enabled
EXPORT_TARGET(mem_misc_collect_info_available)

//! @brief Export phys space size calculation target
//! @note Required as a dependency to use mem_phys_space_size
EXPORT_TARGET(mem_phys_space_size_available)
