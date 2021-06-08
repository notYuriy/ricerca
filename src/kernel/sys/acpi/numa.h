//! @file numa.h
//! @brief File containing declarations of NUMA proximities enumeration wrappers

#pragma once

#include <misc/types.h>    // For bool
#include <sys/numa/numa.h> // For numa_id_t and numa_distance_t

#define ACPI_NUMA_DISTANCE_UNREACHABLE (numa_distance_t)0xff

//! @brief Enumerate NUMA proximities at boot time
//! @param buf Buffer to store NUMA proxmities IDs in
//! @return False if enumeration has ended, true otherwise
//! @note Caller should guard against duplicate proximities on its own
bool acpi_numa_enumerate_at_boot(numa_id_t *buf);

//! @brief Get relative distance between two given NUMA proximities
//! @param id1 ID of proximity 1
//! @param id2 ID of proximity 2
numa_distance_t acpi_numa_get_distance(numa_id_t id1, numa_id_t id2);
