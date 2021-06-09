//! @file log.c
//! @brief File containing miscelanious memory management related declarations

#pragma once

//! @brief Higher-half physical direct map virtual address
#define HIGH_PHYS_VMA (0xffff800000000000ULL)

//! @brief Physical slub granularity as power of 2
#define PHYS_SLUB_GRAN 12ULL

//! @brief Page size
#define PAGE_SIZE (1ULL << PHYS_SLUB_GRAN)

//! @brief End of low physical memory
#define PHYS_LOW (2 * 1024 * 1024)

//! @brief Maximum number of static memory range
#define MEM_MAX_RANGES_STATIC 1024
