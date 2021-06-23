//! @file arch.h
//! @brief File containing declarations of routines for initialization of amd64 tables

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <sys/numa/numa.h>

//! @brief Core architecture state
struct arch_core_state {
	//! @brief Pointer to the GDT
	struct gdt *gdt;
	//! @brief Pointer to the TSS
	struct tss *tss;
};

//! @brief Preallocate arch state for the given core before bootup
//! @param logical_id Core logical id
//! @param numa_id Core proximity domain ID
//! @return True if allocation of core state succeeded
bool arch_prealloc(uint32_t logical_id, numa_id_t numa_id);

//! @brief Initialize amd64 tables on this core
void arch_init(void);

//! @brief Export target for arch initialization
EXPORT_TARGET(arch_available)
