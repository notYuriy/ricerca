//! @file numa.c
//! @brief File containing definitions of NUMA proximities enumeration wrappers

#include <lib/panic.h>     // For PANIC
#include <misc/types.h>    // For bool
#include <sys/acpi/acpi.h> // For ACPI tables
#include <sys/acpi/numa.h> // For declarations of functions below

//! @brief Module name
#define MODULE "acpi/numa"

//! @brief Current SRAT offset
static uintptr_t acpi_numa_current_srat_offset = 0;

//! @brief True if enumeration has been ended (used when SRAT is not available)
static bool acpi_numa_enum_ended = false;

//! @brief Enumerate NUMA proximities at boot time
//! @param buf Buffer to store NUMA proxmities IDs in
//! @return False if enumeration has ended, true otherwise
//! @note Caller should guard against duplicate proximities on its own
bool acpi_numa_enumerate_at_boot(numa_id_t *buf) {
	// Check if SRAT is there
	if (acpi_boot_srat == NULL) {
		// If not, return 0 and return false later on
		if (acpi_numa_enum_ended) {
			return false;
		}
		acpi_numa_enum_ended = true;
		*buf = 0;
		return true;
	} else {
		// Enumerate SRAT
		const uintptr_t starting_address = ((uintptr_t)acpi_boot_srat) + sizeof(struct acpi_srat);
		const uintptr_t entries_len = acpi_boot_srat->hdr.length - sizeof(struct acpi_srat);
		while (acpi_numa_current_srat_offset < entries_len) {
			// Get pointer to current SRAT entry and increment offset
			struct acpi_srat_entry *entry =
			    (struct acpi_srat_entry *)(starting_address + acpi_numa_current_srat_offset);
			acpi_numa_current_srat_offset += entry->length;
			// Check SRAT entry type
			switch (entry->type) {
			case ACPI_SRAT_XAPIC_ENTRY: {
				struct acpi_srat_xapic_entry *xapic = (struct acpi_srat_xapic_entry *)entry;
				// Check that entry is active
				if ((xapic->flags & 1U) == 0) {
					break;
				}
				// Calculate domain ID
				uint32_t domain_id = (uint32_t)xapic->domain_low;
				domain_id += (uint32_t)(xapic->domain_high[0]) << 8U;
				domain_id += (uint32_t)(xapic->domain_high[1]) << 16U;
				domain_id += (uint32_t)(xapic->domain_high[2]) << 24U;
				if (domain_id > NUMA_MAX_NODES) {
					PANIC("domain_id = %u is greater than NUMA_MAX_NODES=%u", domain_id,
					      NUMA_MAX_NODES);
				}
				*buf = (numa_id_t)domain_id;
				return true;
			}
			case ACPI_SRAT_X2APIC_ENTRY: {
				// Processor with APIC id and ACPI id >= 256
				struct acpi_srat_x2apic_entry *x2apic = (struct acpi_srat_x2apic_entry *)entry;
				// Check that entry is active
				if ((x2apic->flags & 1U) == 0) {
					break;
				}
				if (x2apic->domain > NUMA_MAX_NODES) {
					PANIC("domain_id = %u is greater than NUMA_MAX_NODES=%u", x2apic->domain,
					      NUMA_MAX_NODES);
				}
				*buf = (numa_id_t)x2apic->domain;
				return true;
			}
			case ACPI_SRAT_MEM_ENTRY: {
				// Memory range
				struct acpi_srat_mem_entry *mem = (struct acpi_srat_mem_entry *)entry;
				// Check that entry is active
				if ((mem->flags & 1U) == 0) {
					break;
				}
				if (mem->domain > NUMA_MAX_NODES) {
					PANIC("domain_id = %u is greater than NUMA_MAX_NODES=%u", mem->domain,
					      NUMA_MAX_NODES);
				}
				*buf = (numa_id_t)mem->domain;
				return true;
			}
			default:
				break;
			}
		}
	}
	return false;
}

//! @brief Get relative distance between two given NUMA proximities
//! @param id1 ID of proximity 1
//! @param id2 ID of proximity 2
numa_distance_t acpi_numa_get_distance(numa_id_t id1, numa_id_t id2) {
	// Check if SLIT is available
	if (acpi_boot_slit != NULL) {
		// Take distance from SLIT path matrix
		uint8_t dist = acpi_boot_slit->lengths[id1 * acpi_boot_slit->localities + id2];
		if (dist == 0xff) {
			return ACPI_NUMA_DISTANCE_UNREACHABLE;
		}
		return dist;
	} else {
		// Assume that resources from the current node are the closest,
		// Also assume that all nodes are reachable from each other
		return (id1 == id2) ? 0 : 1;
	}
}
