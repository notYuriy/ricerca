//! @file smp.c
//! @brief File containing definitions for enumeration of CPUs available on the system using ACPI

#include <sys/acpi/acpi.h>
#include <sys/acpi/smp.h>

MODULE("sys/acpi/smp")
TARGET(acpi_smp_available, META_DUMMY, {acpi_available})
META_DEFINE_DUMMY()

//! @brief Calculate max CPUs count
//! @return Maximum possible number of CPUs
uint32_t acpi_smp_get_max_cpus(void) {
	if (acpi_boot_madt == NULL) {
		return 1;
	}
	uint64_t address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt);
	const uint64_t end_address = (uint64_t)acpi_boot_madt + acpi_boot_madt->hdr.length;
	uint32_t count = 0;
	while (address < end_address) {
		struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
		address += entry->length;
		switch (entry->type) {
		case ACPI_MADT_XAPIC_ENTRY:
			count++;
			break;
		case ACPI_MADT_X2APIC_ENTRY:
			count++;
			break;
		default:
			break;
		}
	}
	return count;
}

//! @brief Iterate over one more CPU
//! @param buf Pointer to acpi_smp_cpu structure to store info about discovered CPU
//! @return True if one more CPU was discovered and buffer was filled with valid data
bool acpi_smp_iterate_over_cpus(struct acpi_smp_cpu_iterator *iter, struct acpi_smp_cpu *buf) {
	// Detect if MADT is present
	if (acpi_boot_madt == NULL) {
		// If not, only return BSP
		if (iter->offset == 1) {
			return false;
		}
		iter->offset = 1;
		buf->acpi_id = 0;
		buf->apic_id = 0;
		buf->logical_id = 0;
		return false;
	} else {
		// Walk MADT
		uint64_t address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt) + iter->offset;
		const uint64_t end_address = (uint64_t)acpi_boot_madt + acpi_boot_madt->hdr.length;
		while (address < end_address) {
			// Move on to the next entry in advance
			struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
			iter->offset += entry->length;
			address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt) + iter->offset;
			// Handle MADT table types. TODO: handle disabled entries
			switch (entry->type) {
			case ACPI_MADT_XAPIC_ENTRY: {
				struct acpi_madt_xapic_entry *xapic = (struct acpi_madt_xapic_entry *)entry;
				if ((xapic->flags & 0b11U) == 0) {
					break;
				}
				buf->acpi_id = xapic->acpi_id;
				buf->apic_id = xapic->apic_id;
				buf->logical_id = iter->logical_id;
				iter->logical_id++;
				return true;
			}
			case ACPI_MADT_X2APIC_ENTRY: {
				struct acpi_madt_x2apic_entry *x2apic = (struct acpi_madt_x2apic_entry *)entry;
				if ((x2apic->flags & 0b11U) == 0) {
					break;
				}
				buf->acpi_id = x2apic->acpi_id;
				buf->apic_id = x2apic->apic_id;
				buf->logical_id = iter->logical_id;
				iter->logical_id++;
				return true;
			}
			default:
				break;
			}
		}
		return false;
	}
}
