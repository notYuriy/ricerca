//! @file locals.h
//! @brief File declaring CPU-local data structures and functions to access them

#pragma once

#include <lib/target.h>
#include <sys/numa/numa.h>

//! @brief CPU-local area
struct thread_smp_locals {
	//! @brief NUMA id
	numa_id_t numa_id;
};

//! @brief Get pointer to CPU local storage
//! @return Pointer to the thread_smp_locals data structure
struct thread_smp_locals *thread_smp_locals_get(void);

//! @brief Initialize CPU local storage on AP
//! @note To be used on AP bootup
//! @note Requires LAPIC to be enabled
void thread_smp_locals_set(void);

//! @brief Export target to initialize CPU local areas
EXPORT_TARGET(thread_smp_locals_target)
