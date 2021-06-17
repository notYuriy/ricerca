//! @file numa.h
//! @brief File containing declarations of NUMA proximities enumeration wrappers

#pragma once

#include <lib/target.h>
#include <misc/types.h>
#include <sys/numa/numa.h>

//! @brief Proximity domain of boot CPU
extern numa_id_t acpi_numa_boot_domain;

//! @brief Enumerate NUMA proximities at boot time
//! @param buf Buffer to store NUMA proxmities IDs in
//! @return False if enumeration has ended, true otherwise
//! @note Caller should guard against duplicate proximities on its own
bool acpi_numa_enumerate_at_boot(numa_id_t *buf);

//! @brief Get relative distance between two given NUMA proximities
//! @param id1 ID of proximity 1
//! @param id2 ID of proximity 2
numa_distance_t acpi_numa_get_distance(numa_id_t id1, numa_id_t id2);

//! @brief Memory range areas iterator
struct acpi_numa_phys_range_iter {
	//! @brief Offset in SRAT
	uintptr_t srat_offset;
	//! @brief Start of the physical range
	uintptr_t range_start;
	//! @brief End of the physical range
	uintptr_t range_end;
};

//! @brief Memory range
struct acpi_numa_memory_range {
	//! @brief Range start
	uintptr_t start;
	//! @brief Range end
	uintptr_t end;
	//! @brief Is hotpluggable?
	bool hotpluggable;
	//! @brief NUMA node this range belongs to
	numa_id_t node_id;
};

//! @brief Memory range iterator initializer
//! @param start Start of the physical range
//! @param end End of the physical range
#define ACPI_NUMA_PHYS_RANGE_ITER_INIT(start, end)                                                 \
	{ 0, start, end }

//! @brief Try to get one more memory range from the iterator
//! @param iter Pointer to iterator
//! @param buf Buffer to store info in
//! @return True if new entry is available, false otherwise
bool acpi_numa_get_memory_range(struct acpi_numa_phys_range_iter *iter,
                                struct acpi_numa_memory_range *buf);

//! @brief Translate APIC id to proximity domain
//! @param apic_id APIC id
//! @return NUMA domain ID
numa_id_t acpi_numa_apic2numa_id(uint32_t apic_id);

//! @brief Export ACPI NUMA wrappers init target
EXPORT_TARGET(acpi_numa_available)
