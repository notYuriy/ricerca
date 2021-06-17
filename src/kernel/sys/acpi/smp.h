//! @file smp.h
//! @brief File containing declarations for enumeration of CPUs available on the system using ACPI

#pragma once

#include <lib/target.h>
#include <misc/types.h>

//! @brief Discovered CPU
struct acpi_smp_cpu {
	//! @brief APIC ID
	uint32_t apic_id;
	//! @brief ACPI ID
	uint32_t acpi_id;
	//! @brief Logical ID (madt index)
	uint32_t logical_id;
};

//! @brief Platform CPU iterator
struct acpi_smp_cpu_iterator {
	//! @brief MADT offset
	size_t offset;
	//! @brief Logical CPU ID to be returned
	uint32_t logical_id;
};

//! @brief Static CPU iterator init
#define ACPI_SMP_CPU_ITERATOR_INIT                                                                 \
	(struct acpi_smp_cpu_iterator) {                                                               \
		0, 0                                                                                       \
	}

//! @brief Calculate max CPUs count
//! @return Maximum possible number of CPUs
uint32_t acpi_smp_get_max_cpus(void);

//! @brief Iterate over one more CPU
//! @param iter Pointer to the iterator
//! @param buf Pointer to acpi_smp_cpu structure to store info about discovered CPU
//! @return True if one more CPU was discovered and buffer was filled with valid data
bool acpi_smp_iterate_over_cpus(struct acpi_smp_cpu_iterator *iter, struct acpi_smp_cpu *buf);

//! @brief Target for enabling CPU enumeration
EXPORT_TARGET(acpi_smp_available)
