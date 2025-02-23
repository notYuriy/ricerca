//! @file numa.c
//! @brief File containing definitions of NUMA proximities enumeration wrappers

#include <lib/panic.h>
#include <mem/misc.h>
#include <misc/misc.h>
#include <misc/types.h>
#include <sys/acpi/acpi.h>
#include <sys/acpi/numa.h>
#include <sys/ic.h>

MODULE("sys/acpi/numa")
TARGET(acpi_numa_available, acpi_numa_init, {ic_bsp_available, acpi_available})

//! @brief Proximity domain of boot CPU
numa_id_t acpi_numa_boot_domain = 0;

//! @brief Enumerate NUMA proximities
//! @param iter Pointer to acpi_numa_proximities_iter
//! @param buf Buffer to store NUMA proxmities IDs in
//! @return False if enumeration has ended, true otherwise
//! @note Caller should guard against duplicate proximities on its own
bool acpi_numa_enumerate_at_boot(struct acpi_numa_proximities_iter *iter, numa_id_t *buf) {
	// Check if SRAT is there
	if (acpi_boot_srat == NULL) {
		// If not, return 0 and return false later on
		if (iter->enumeration_finished) {
			return false;
		}
		iter->enumeration_finished = true;
		*buf = 0;
		return true;
	} else {
		// Enumerate SRAT
		const uintptr_t starting_address = ((uintptr_t)acpi_boot_srat) + sizeof(struct acpi_srat);
		const uintptr_t entries_len = acpi_boot_srat->hdr.length - sizeof(struct acpi_srat);
		while (iter->srat_offset < entries_len) {
			// Get pointer to current SRAT entry and increment offset
			struct acpi_srat_entry *entry =
			    (struct acpi_srat_entry *)(starting_address + iter->srat_offset);
			iter->srat_offset += entry->length;
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
		return acpi_boot_slit->lengths[id1 * acpi_boot_slit->localities + id2];
	} else {
		// Assume that resources from the current node are the closest,
		// Also assume that all nodes are reachable from each other
		return (id1 == id2) ? 0 : 1;
	}
}

//! @brief Try to get one more memory range from the iterator
//! @param iter Pointer to iterator
//! @param buf Buffer to store info in
//! @return True if new entry is available, false otherwise
bool acpi_numa_get_memory_range(struct acpi_numa_phys_range_iter *iter,
                                struct acpi_numa_memory_range *buf) {
	// Check if SRAT is available
	if (acpi_boot_srat == NULL) {
		// Use "srat_offset" field of the iterator as a flag to see if this routine was already
		// called
		if (iter->srat_offset == 0) {
			iter->srat_offset = 1;
			buf->node_id = 0;
			buf->end = iter->range_end;
			buf->start = iter->range_start;
			// Align start and end
			buf->start = align_up(buf->start, PAGE_SIZE);
			buf->end = align_down(buf->end, PAGE_SIZE);
			return true;
		} else {
			return false;
		}
	} else {
		// Enumerate SRAT
		const uintptr_t starting_address = ((uintptr_t)acpi_boot_srat) + sizeof(struct acpi_srat);
		const uintptr_t entries_len = acpi_boot_srat->hdr.length - sizeof(struct acpi_srat);
		while (iter->srat_offset < entries_len) {
			// Get pointer to current SRAT entry and increment offset
			struct acpi_srat_entry *entry =
			    (struct acpi_srat_entry *)(starting_address + iter->srat_offset);
			iter->srat_offset += entry->length;
			// We only need memory entries
			if (entry->type != ACPI_SRAT_MEM_ENTRY) {
				continue;
			}
			struct acpi_srat_mem_entry *mem = (struct acpi_srat_mem_entry *)entry;
			// Check that entry is active
			if ((mem->flags & 1U) == 0) {
				continue;
			}
			uint32_t domain_id = mem->domain;
			uint64_t base = ((uint64_t)(mem->base_high) << 32ULL) + (uint64_t)(mem->base_low);
			uint64_t len = ((uint64_t)(mem->length_high) << 32ULL) + (uint64_t)(mem->length_low);
			uint64_t end = base + len;
			// Check if any part of this range belongs to enumerable region
			if (end <= iter->range_start || base >= iter->range_end) {
				continue;
			}
			buf->end = (end > iter->range_end) ? iter->range_end : end;
			buf->start = (base < iter->range_start) ? iter->range_start : base;
			buf->node_id = domain_id;
			// Align start and end
			buf->start = align_up(buf->start, PAGE_SIZE);
			buf->end = align_down(buf->end, PAGE_SIZE);
			return true;
		}
	}
	return false;
}

numa_id_t acpi_numa_apic2numa_id(uint32_t apic_id) {
	if (acpi_boot_srat == NULL) {
		return 0;
	}
	// Enumerate SRAT and find proximity domain for a given APIC id
	const uintptr_t starting_address = ((uintptr_t)acpi_boot_srat) + sizeof(struct acpi_srat);
	const uintptr_t entries_len = acpi_boot_srat->hdr.length - sizeof(struct acpi_srat);
	uintptr_t current_offset = 0;
	while (current_offset < entries_len) {
		// Get pointer to current SRAT entry and increment offset
		struct acpi_srat_entry *entry =
		    (struct acpi_srat_entry *)(starting_address + current_offset);
		current_offset += entry->length;
		// Check entry type
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
			// Check if apic ID matches requested apic ID
			if (xapic->apic_id == apic_id) {
				return domain_id;
			}
			break;
		}
		case ACPI_SRAT_X2APIC_ENTRY: {
			// Processor with APIC id and ACPI id >= 256
			struct acpi_srat_x2apic_entry *x2apic = (struct acpi_srat_x2apic_entry *)entry;
			// Check that entry is active
			if ((x2apic->flags & 1U) == 0) {
				break;
			}
			// Check if apic ID matches requested apic ID
			if (x2apic->apic_id == apic_id) {
				return x2apic->domain;
			}
		}
		default:
			break;
		}
	}
	PANIC("Can't find APIC ID %u in SRAT", apic_id);
}

//! @brief Initialize NUMA ACPI wrappers
static void acpi_numa_init(void) {
	// Get boot CPU APIC id
	uint32_t boot_apic_id = ic_get_apic_id();
	LOG_INFO("APIC ID of boot CPU is %u", boot_apic_id);
	// Get boot CPU proximity domain
	uint32_t boot_prox_domain = acpi_numa_apic2numa_id(boot_apic_id);
	LOG_INFO("NUMA domain of boot CPU is %u", boot_prox_domain);
	acpi_numa_boot_domain = boot_prox_domain;
	LOG_SUCCESS("Initialization finished!");
}
