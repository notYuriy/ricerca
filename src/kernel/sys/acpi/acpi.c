//! @file log.c
//! @brief File containing definitions for ACPI high level functions

#include <init/init.h>
#include <lib/log.h>
#include <lib/panic.h>
#include <lib/string.h>
#include <mem/misc.h>
#include <misc/attributes.h>
#include <sys/acpi/acpi.h>

MODULE("sys/acpi")
TARGET(acpi_available, acpi_init, {mem_misc_collect_info_available})

//! @brief ACPI RSDP revisions
enum
{
	//! @brief First ACPI RSDP revision with pointer to RSDT
	ACPI_RSDP_REV1 = 0,
	//! @brief Second ACPI RSDP revision with pointer to XSDT
	ACPI_RSDP_REV2 = 2,
};

//! @brief First revision ACPI RSDP
struct acpi_rsdp {
	char sign[8];
	uint8_t checksum;
	char oemid[6];
	uint8_t rev;
	uint32_t rsdt_addr;
} attribute_packed;

//! @brief Second revision ACPI RSDP
struct acpi_rsdpv2 {
	struct acpi_rsdp base;
	uint32_t length;
	uint64_t xsdt_addr;
	uint8_t ext_checksum;
	uint8_t reserved[3];
} attribute_packed;

//! @brief RSDT table
struct acpi_rsdt {
	struct acpi_sdt_header hdr;
	uint32_t tables[];
} attribute_packed;

//! @brief XSDT table
struct acpi_xsdt {
	struct acpi_sdt_header hdr;
	uint64_t tables[];
} attribute_packed;

//! @brief Validate ACPI table checksum
//! @param table ACPI table
//! @param len ACPI table length
bool acpi_validate_checksum(void *table, size_t len) {
	uint8_t mod = 0;
	for (size_t i = 0; i < len; ++i) {
		mod += ((uint8_t *)table)[i];
	}
	return mod == 0;
}

//! @brief Nullable pointer to SRAT. Set by acpi_init if found
struct acpi_srat *acpi_boot_srat = NULL;

//! @brief Nullable pointer to MADT. Set by acpi_init if found
struct acpi_madt *acpi_boot_madt = NULL;

//! @brief Nullable pointer to SLIT. Set by acpi_init if found
struct acpi_slit *acpi_boot_slit = NULL;

//! @brief Nullable pointer to RSDT. Set by acpi_init if fount
static struct acpi_rsdt *acpi_boot_rsdt = NULL;

//! @brief Nullable pointer to XSDT. Set by acpi_init if found
static struct acpi_xsdt *acpi_boot_xsdt = NULL;

//! @brief Nullable pointer to FADT. Set by acpi_init if found
struct acpi_fadt *acpi_boot_fadt = NULL;

//! @brief Validate SLIT
//! @param slit Pointer to the SLIT table
//! @note Used to guard against bioses that fill SLIT with 10s everywhere
static bool acpi_validate_slit(struct acpi_slit *slit) {
	for (size_t i = 0; i < slit->localities; ++i) {
		for (size_t j = 0; j < slit->localities; ++j) {
			if (i == j) {
				if (slit->lengths[i * slit->localities + j] != 10) {
					LOG_WARN("slit: lenghts[%U][%U] is not 10", i, j);
					return false;
				}
			} else if (slit->lengths[i * slit->localities + j] <= 10) {
				LOG_WARN("slit: length[%U][%U] = %u is <= 10", i, j,
				         (uint32_t)slit->lengths[i * slit->localities + j]);
				return false;
			}
			if (slit->lengths[i * slit->localities + j] == 255) {
				PANIC("Unreachable nodes.");
			}
		}
	}
	return true;
}

//! @brief Dump SRAT
//! @brief srat Pointer to SRAT table
void acpi_dump_srat(struct acpi_srat *srat) {
	LOG_INFO("Dumping SRAT:");

	// Calculate entries starting and ending address
	uint64_t address = (uint64_t)srat + sizeof(struct acpi_srat);
	const uint64_t end_address = (uint64_t)srat + srat->hdr.length;

	// Iterate over all srat enties
	while (address < end_address) {
		struct acpi_srat_entry *entry = (struct acpi_srat_entry *)address;
		switch (entry->type) {
		case ACPI_SRAT_XAPIC_ENTRY: {
			// Processor with APIC id and ACPI id < 256
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
			LOG_INFO("CPU with apic_id %u in domain = %u detected", (uint32_t)xapic->apic_id,
			         domain_id);
			break;
		}
		case ACPI_SRAT_MEM_ENTRY: {
			// Memory range
			struct acpi_srat_mem_entry *mem = (struct acpi_srat_mem_entry *)address;
			// Check that entry is active
			if ((mem->flags & 1U) == 0) {
				break;
			}
			uint32_t domain_id = mem->domain;
			uint64_t base = ((uint64_t)(mem->base_high) << 32ULL) + (uint64_t)(mem->base_low);
			uint64_t len = ((uint64_t)(mem->length_high) << 32ULL) + (uint64_t)(mem->length_low);
			LOG_INFO("Memory range (%p:%p, domain = %u) detected", base, base + len, domain_id);
			break;
		}
		case ACPI_SRAT_X2APIC_ENTRY: {
			// Processor with APIC id and ACPI id >= 256
			struct acpi_srat_x2apic_entry *x2apic = (struct acpi_srat_x2apic_entry *)entry;
			// Check that entry is active
			if ((x2apic->flags & 1U) == 0) {
				break;
			}
			LOG_INFO("CPU with apic_id %u in domain = %u detected", x2apic->apic_id,
			         x2apic->domain);
			break;
		}
		default:
			break;
		}
		// Move to next entry
		address += entry->length;
	}
}

//! @brief Dump SLIT
//! @brief srat Pointer to SLIT table
void acpi_dump_slit(struct acpi_slit *slit) {
	LOG_INFO("Number of localities (obtained from SLIT): %u", slit->localities);
	LOG_INFO("Dumping localities distances matrix");
	// Print table header
	log_write("/\t", 2);
	for (size_t i = 0; i < slit->localities; ++i) {
		log_printf("\033[36m%U\033[0m\t", i);
	}
	log_write("\n", 1);
	for (size_t i = 0; i < slit->localities; ++i) {
		log_printf("\033[31m%U\033[0m\t", i);
		for (size_t j = 0; j < slit->localities; ++j) {
			log_printf("%U\t", slit->lengths[i * slit->localities + j]);
		}
		log_write("\n", 1);
	}
}

//! @brief Dump MADT
//! @brief madt Pointer to MADT table
void acpi_dump_madt(struct acpi_madt *madt) {
	LOG_INFO("Dumping MADT:");

	// Calculate entries starting and ending address
	uint64_t address = (uint64_t)madt + sizeof(struct acpi_madt);
	const uint64_t end_address = (uint64_t)madt + madt->hdr.length;

	// Iterate over all entries
	uint32_t logical_id = 0;
	while (address < end_address) {
		struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
		switch (entry->type) {
		case ACPI_MADT_XAPIC_ENTRY: {
			struct acpi_madt_xapic_entry *xapic = (struct acpi_madt_xapic_entry *)entry;
			if ((xapic->flags & 0b11U) == 0) {
				LOG_INFO("Disabled core with ACPI ID %u, APIC ID %u, LID %u",
				         (uint32_t)xapic->acpi_id, (uint32_t)xapic->apic_id, logical_id++);
				break;
			}
			LOG_INFO("CPU with ACPI ID %u, APIC ID %u, LID %u", (uint32_t)xapic->acpi_id,
			         (uint32_t)xapic->apic_id, logical_id++);
			break;
		}
		case ACPI_MADT_X2APIC_ENTRY: {
			struct acpi_madt_x2apic_entry *x2apic = (struct acpi_madt_x2apic_entry *)entry;
			if ((x2apic->flags & 0b11U) == 0) {
				LOG_INFO("Disabled core with ACPI ID %U, APIC ID %U, LID %u",
				         (uint32_t)x2apic->acpi_id, (uint32_t)x2apic->apic_id, logical_id++);
				break;
			}
			LOG_INFO("CPU with ACPI ID %u, APIC ID %u, LID %uU", x2apic->acpi_id, x2apic->apic_id,
			         logical_id++);
			break;
		}
		default:
			break;
		}
		address += entry->length;
	}
}

//! @brief Walk RSDT in search of a specific table
//! @param rsdt Pointer to RSDT
//! @param name Name of the table
//! @param index Index of the table
//! @return Pointer to the table or NULL if not found
static struct acpi_sdt_header *acpi_find_in_rsdt(struct acpi_rsdt *rsdt, const char *name,
                                                 size_t index) {
	// Get tables count
	const size_t tables_count = (rsdt->hdr.length - sizeof(struct acpi_sdt_header)) / 4;
	// Visit tables
	size_t to_skip = index;
	for (size_t i = 0; i < tables_count; ++i) {
		uintptr_t table_phys = (uintptr_t)rsdt->tables[i];
		struct acpi_sdt_header *header =
		    (struct acpi_sdt_header *)(table_phys + mem_wb_phys_win_base);
		if (memcmp(header->signature, name, 4) == 0) {
			if (to_skip == 0) {
				return header;
			}
			to_skip--;
		}
	}
	return NULL;
}

//! @brief Walk XSDT in search of a specific table
//! @param rsdt Pointer to XSDT
//! @param name Name of the table
//! @param index Index of the table
//! @return Pointer to the table or NULL if not found
static struct acpi_sdt_header *acpi_find_in_xsdt(struct acpi_xsdt *xsdt, const char *name,
                                                 size_t index) {
	// Get tables count
	const size_t tables_count = (xsdt->hdr.length - sizeof(struct acpi_sdt_header)) / 8;
	// Visit tables
	size_t to_skip = index;
	for (size_t i = 0; i < tables_count; ++i) {
		uintptr_t table_phys = xsdt->tables[i];
		struct acpi_sdt_header *header =
		    (struct acpi_sdt_header *)(table_phys + mem_wb_phys_win_base);
		if (memcmp(header->signature, name, 4) == 0) {
			if (to_skip == 0) {
				return header;
			}
			to_skip--;
		}
	}
	return NULL;
}

//! @brief Search for the specific table
//! @param name Name of the table
//! @param index of the table
struct acpi_sdt_header *acpi_find_table(const char *name, size_t index) {
	// Handle a few special cases
	if (memcmp(name, "DSDT", 4) == 0) {
		ASSERT(index == 0, "Attempt to request DSDT with non-zero index");
		if (acpi_boot_fadt == NULL) {
			return NULL;
		}
		if (acpi_boot_fadt->dsdt_ex != 0 && acpi_revision >= 2) {
			return (struct acpi_sdt_header *)(mem_wb_phys_win_base + acpi_boot_fadt->dsdt_ex);
		}
		if (acpi_boot_fadt->dsdt != 0) {
			return (struct acpi_sdt_header *)(mem_wb_phys_win_base +
			                                  (uintptr_t)acpi_boot_fadt->dsdt);
		}
		return NULL;
	}
	if (acpi_boot_xsdt == NULL) {
		// Assert RSDT presence
		ASSERT(acpi_boot_rsdt != NULL, "Attempt to query tables in non-acpi mode");
		return acpi_find_in_rsdt(acpi_boot_rsdt, name, index);
	} else {
		return acpi_find_in_xsdt(acpi_boot_xsdt, name, index);
	}
}

//! @brief Find size of physical address space according to SRAT
//! @return Size of physical memory space
size_t acpi_query_phys_space_size(void) {
	if (acpi_boot_srat == NULL) {
		return 0;
	}
	size_t result = 0;
	// Enumerate SRAT
	const uintptr_t starting_address = ((uintptr_t)acpi_boot_srat) + sizeof(struct acpi_srat);
	const uintptr_t entries_len = acpi_boot_srat->hdr.length - sizeof(struct acpi_srat);
	uintptr_t current_offset = 0;
	while (current_offset < entries_len) {
		// Get pointer to current SRAT entry and increment offset
		struct acpi_srat_entry *entry =
		    (struct acpi_srat_entry *)(starting_address + current_offset);
		current_offset += entry->length;
		// We only need memory entries
		if (entry->type != ACPI_SRAT_MEM_ENTRY) {
			continue;
		}
		struct acpi_srat_mem_entry *mem = (struct acpi_srat_mem_entry *)entry;
		// Check that entry is active
		if ((mem->flags & 1U) == 0) {
			continue;
		}
		uint64_t base = ((uint64_t)(mem->base_high) << 32ULL) + (uint64_t)(mem->base_low);
		uint64_t len = ((uint64_t)(mem->length_high) << 32ULL) + (uint64_t)(mem->length_low);
		if (base + len > result) {
			result = base + len;
		}
	}
	return result;
}

//! @brief ACPI revision or 0 if ACPI is not supported
size_t acpi_revision = 0;

//! @brief Early ACPI subsystem init
static void acpi_init(void) {
	struct stivale2_struct_tag_rsdp *rsdp_tag = init_rsdp_tag;
	if (rsdp_tag == NULL) {
		LOG_WARN("Machine does not support ACPI");
		return;
	}

	struct acpi_rsdp *rsdp = (struct acpi_rsdp *)rsdp_tag->rsdp;
	LOG_INFO("RSDP at %p", rsdp);

	// Validate RSDP
	if (!acpi_validate_checksum(rsdp, sizeof(struct acpi_rsdp))) {
		LOG_ERR("Legacy RSDP checksum validation failed");
	}

	// Walk boot ACPI tables
	if (rsdp->rev == ACPI_RSDP_REV1) {
		// RSDPv1 and hence RSDT detected
		acpi_revision = 1;
		const uint64_t rsdt_addr = (uint64_t)rsdp->rsdt_addr;
		acpi_boot_rsdt = (struct acpi_rsdt *)(mem_wb_phys_win_base + rsdt_addr);
	} else {
		// RSDPv2 and hence XSDT detected
		acpi_revision = rsdp->rev;
		struct acpi_rsdpv2 *rsdpv2 = (struct acpi_rsdpv2 *)rsdp;
		if (!acpi_validate_checksum(rsdp, sizeof(struct acpi_rsdp))) {
			LOG_ERR("RSDPv2 checksum validation failed");
		}
		const uint64_t xsdt_addr = (uint64_t)rsdpv2->xsdt_addr;
		acpi_boot_xsdt = (struct acpi_xsdt *)(mem_wb_phys_win_base + xsdt_addr);
	}

	// Find a few useful tables
	acpi_boot_madt = (struct acpi_madt *)acpi_find_table("APIC", 0);
	acpi_boot_slit = (struct acpi_slit *)acpi_find_table("SLIT", 0);
	acpi_boot_srat = (struct acpi_srat *)acpi_find_table("SRAT", 0);
	acpi_boot_fadt = (struct acpi_fadt *)acpi_find_table("FACP", 0);

	// Validate SLIT
	if (acpi_boot_slit != NULL) {
		acpi_dump_slit(acpi_boot_slit);
		if (!acpi_validate_slit(acpi_boot_slit)) {
			LOG_ERR("SLIT is of a poor quality. Discarding");
			acpi_boot_slit = NULL;
		}
	}
}

//! @brief Load a given property from xAPIC ACPI MADT entry
//! @param entry MADT xAPIC entry
//! @param prop Property to load
//! @param lid Local ID
static uint32_t acpi_madt_xapic_load_prop(struct acpi_madt_xapic_entry *entry,
                                          enum acpi_madt_lapic_entry_prop prop, uint32_t lid) {
	switch (prop) {
	case ACPI_MADT_LAPIC_PROP_LOGICAL_ID:
		return lid;
	case ACPI_MADT_LAPIC_PROP_ACPI_ID:
		return (uint32_t)entry->acpi_id;
	case ACPI_MADT_LAPIC_PROP_APIC_ID:
		return (uint32_t)entry->apic_id;
	default:
		PANIC("Unknown property");
	}
}

//! @brief Load a given property from x2APIC ACPI MADT entry
//! @param entry MADT x2APIC entry
//! @param prop Property to load
//! @param lid Local ID
static uint32_t acpi_madt_x2apic_load_prop(struct acpi_madt_x2apic_entry *entry,
                                           enum acpi_madt_lapic_entry_prop prop, uint32_t lid) {
	switch (prop) {
	case ACPI_MADT_LAPIC_PROP_LOGICAL_ID:
		return lid;
	case ACPI_MADT_LAPIC_PROP_ACPI_ID:
		return entry->acpi_id;
	case ACPI_MADT_LAPIC_PROP_APIC_ID:
		return entry->apic_id;
	default:
		PANIC("Unknown property");
	}
}

//! @brief Find MADT lapic entry that satisifies given criteria
//! @param matched Property of MADT lapic entries to match with expected
//! @param returned Property of MADT lapic entries to return
//! @param expected Expected value of matched property
//! @return Value of returned property
uint32_t acpi_madt_convert_ids(enum acpi_madt_lapic_entry_prop matched,
                               enum acpi_madt_lapic_entry_prop returned, uint32_t expected) {
	if (acpi_boot_madt == NULL) {
		// 0 is valid for all props anyway
		return 0;
	}
	uint64_t address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt);
	const uint64_t end_address = (uint64_t)acpi_boot_madt + acpi_boot_madt->hdr.length;
	uint32_t logical_id = 0;
	while (address < end_address) {
		struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
		address += entry->length;
		switch (entry->type) {
		case ACPI_MADT_XAPIC_ENTRY: {
			struct acpi_madt_xapic_entry *xapic = (struct acpi_madt_xapic_entry *)entry;
			if ((xapic->flags & 0b11U) == 0) {
				break;
			}
			uint32_t matched_val = acpi_madt_xapic_load_prop(xapic, matched, logical_id);
			if (matched_val == expected) {
				return acpi_madt_xapic_load_prop(xapic, returned, logical_id);
			}
			logical_id++;
			break;
		}
		case ACPI_MADT_X2APIC_ENTRY: {
			struct acpi_madt_x2apic_entry *x2apic = (struct acpi_madt_x2apic_entry *)entry;
			if ((x2apic->flags & 0b11U) == 0) {
				break;
			}
			uint32_t matched_val = acpi_madt_x2apic_load_prop(x2apic, matched, logical_id);
			if (matched_val == expected) {
				return acpi_madt_x2apic_load_prop(x2apic, returned, logical_id);
			}
			logical_id++;
			break;
		}
		default:
			break;
		}
	}
	PANIC("Search for MADT LAPIC entry failed. Params: {%u, %u, %u}", (uint32_t)matched,
	      (uint32_t)returned, expected);
}
