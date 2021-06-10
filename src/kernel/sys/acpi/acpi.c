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
TARGET(acpi_target, acpi_init)

//! @brief ACPI RSDP revisions
enum {
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
			bool hotplug = (mem->flags & 2U) != 0;
			uint32_t domain_id = mem->domain;
			uint64_t base = ((uint64_t)(mem->base_high) << 32ULL) + (uint64_t)(mem->base_low);
			uint64_t len = ((uint64_t)(mem->length_high) << 32ULL) + (uint64_t)(mem->length_low);
			LOG_INFO("Memory range (%p:%p, domain = %u. hotplug: %s) detected", base, base + len,
			         domain_id, hotplug ? "true" : "false");
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
	while (address < end_address) {
		struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
		switch (entry->type) {
		case ACPI_MADT_XAPIC_ENTRY: {
			struct acpi_madt_xapic_entry *xapic = (struct acpi_madt_xapic_entry *)entry;
			if ((xapic->flags & 0b11U) == 0) {
				// Disabled core
				break;
			}
			LOG_INFO("CPU with ACPI ID %U has APIC ID %U ", (uint32_t)xapic->acpi_id,
			         (uint32_t)xapic->apic_id);
			break;
		}
		case ACPI_MADT_X2APIC_ENTRY: {
			struct acpi_madt_x2apic_entry *x2apic = (struct acpi_madt_x2apic_entry *)entry;
			if ((x2apic->flags & 0b11U) == 0) {
				// Disabled core
				break;
			}
			LOG_INFO("CPU with ACPI ID %U has APIC ID %U", x2apic->acpi_id, x2apic->apic_id);
			break;
		}
		default:
			break;
		}
		address += entry->length;
	}
}

//! @brief Visit a given ACPI table
//! @param table_phys Physical table address
static void acpi_visit_table(uint64_t table_phys) {
	const struct acpi_sdt_header *header = (struct acpi_sdt_header *)(table_phys + HIGH_PHYS_VMA);
	// Check a few common tables
	if (memcmp(header->signature, "SRAT", 4) == 0) {
		if (acpi_boot_srat != NULL) {
			PANIC("Duplicate SRAT");
		}
		LOG_SUCCESS("SRAT ACPI table found");
		acpi_boot_srat = (struct acpi_srat *)header;
	} else if (memcmp(header->signature, "APIC", 4) == 0) {
		if (acpi_boot_madt != NULL) {
			PANIC("Duplicate MADT");
		}
		LOG_SUCCESS("MADT (APIC) ACPI table found");
		acpi_boot_madt = (struct acpi_madt *)header;
	} else if (memcmp(header->signature, "SLIT", 4) == 0) {
		if (acpi_boot_slit != NULL) {
			PANIC("Duplicate SLIT");
		}
		LOG_SUCCESS("SLIT ACPI table found");
		struct acpi_slit *slit = (struct acpi_slit *)header;
		if (acpi_validate_slit(slit)) {
			acpi_boot_slit = slit;
		} else {
			LOG_ERR("SLIT is of a poor quality, discarding");
		}
	}
}

//! @brief Walk RSDT
//! @param rsdt_phys Physical address of RSDT
static void acpi_walk_rsdt(uint64_t rsdt_phys) {
	struct acpi_rsdt *rsdt = (struct acpi_rsdt *)(rsdt_phys + HIGH_PHYS_VMA);
	if (!acpi_validate_checksum(rsdt, rsdt->hdr.length)) {
		LOG_ERR("RSDT checksum validation failed");
	}
	// Get tables count
	const size_t tables_count = (rsdt->hdr.length - sizeof(struct acpi_sdt_header)) / 4;
	LOG_INFO("%U tables found in RSDT", tables_count);
	// Visit tables
	for (size_t i = 0; i < tables_count; ++i) {
		acpi_visit_table((uint64_t)rsdt->tables[i]);
	}
}

//! @brief Walk XSDT
//! @param rsdt_phys Physical address of XSDT
static void acpi_walk_xsdt(uint64_t xsdt_phys) {
	struct acpi_xsdt *xsdt = (struct acpi_xsdt *)(xsdt_phys + HIGH_PHYS_VMA);
	if (!acpi_validate_checksum(xsdt, xsdt->hdr.length)) {
		LOG_ERR("XSDT checksum validation failed");
	}
	// Get tables count
	const size_t tables_count = (xsdt->hdr.length - sizeof(struct acpi_sdt_header)) / 8;
	LOG_INFO("%U tables found in XSDT", tables_count);
	// Visit tables
	for (size_t i = 0; i < tables_count; ++i) {
		acpi_visit_table(xsdt->tables[i]);
	}
}

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
		const uint64_t rsdt_addr = (uint64_t)rsdp->rsdt_addr;
		LOG_INFO("RSDT detected at %p", rsdt_addr);
		acpi_walk_rsdt(rsdt_addr);
	} else if (rsdp->rev == ACPI_RSDP_REV2) {
		// RSDPv2 and hence XSDT detected
		struct acpi_rsdpv2 *rsdpv2 = (struct acpi_rsdpv2 *)rsdp;
		if (!acpi_validate_checksum(rsdp, sizeof(struct acpi_rsdp))) {
			LOG_ERR("RSDPv2 checksum validation failed");
		}
		const uint64_t xsdt_addr = (uint64_t)rsdpv2->xsdt_addr;
		LOG_INFO("XSDT detected at %p", xsdt_addr);
		acpi_walk_xsdt(xsdt_addr);
	} else {
		PANIC("Unknown XSDT revision");
	}
}

//! @brief Lookup APIC CPU id from ACPI CPU id
//! @param acpi_id ACPI CPU id
uint32_t acpi_acpi2apic_id(uint32_t acpi_id) {
	if (acpi_boot_madt == NULL) {
		return 0;
	}
	// Enumerate MADT entries
	// Calculate entries starting and ending address
	uint64_t address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt);
	const uint64_t end_address = (uint64_t)acpi_boot_madt + acpi_boot_madt->hdr.length;
	while (address < end_address) {
		struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
		address += entry->length;
		switch (entry->type) {
		case ACPI_MADT_XAPIC_ENTRY: {
			struct acpi_madt_xapic_entry *xapic = (struct acpi_madt_xapic_entry *)entry;
			if ((xapic->flags & 0b11U) == 0) {
				break;
			}
			if ((uint32_t)(xapic->acpi_id) == acpi_id) {
				return (uint32_t)xapic->apic_id;
			}
			break;
		}
		case ACPI_MADT_X2APIC_ENTRY: {
			struct acpi_madt_x2apic_entry *x2apic = (struct acpi_madt_x2apic_entry *)entry;
			if ((x2apic->flags & 0b11U) == 0) {
				break;
			}
			if (x2apic->acpi_id == acpi_id) {
				return x2apic->apic_id;
			}
			break;
		}
		default:
			break;
		}
	}
	PANIC("ACPI ID %u not found", acpi_id);
}

//! @brief Lookup ACPI CPI id from APIC CPI id
//! @param apic_id APIC CPU id
uint32_t acpi_apic2acpi_id(uint32_t apic_id) {
	// Enumerate MADT entries
	if (acpi_boot_madt == NULL) {
		return 0;
	}
	// Enumerate MADT entries
	// Calculate entries starting and ending address
	uint64_t address = (uint64_t)acpi_boot_madt + sizeof(struct acpi_madt);
	const uint64_t end_address = (uint64_t)acpi_boot_madt + acpi_boot_madt->hdr.length;
	while (address < end_address) {
		struct acpi_madt_entry *entry = (struct acpi_madt_entry *)address;
		address += entry->length;
		switch (entry->type) {
		case ACPI_MADT_XAPIC_ENTRY: {
			struct acpi_madt_xapic_entry *xapic = (struct acpi_madt_xapic_entry *)entry;
			if ((xapic->flags & 0b11U) == 0) {
				break;
			}
			if ((uint32_t)(xapic->apic_id) == apic_id) {
				return (uint32_t)xapic->acpi_id;
			}
			break;
		}
		case ACPI_MADT_X2APIC_ENTRY: {
			struct acpi_madt_x2apic_entry *x2apic = (struct acpi_madt_x2apic_entry *)entry;
			if ((x2apic->flags & 0b11U) == 0) {
				break;
			}
			if (x2apic->apic_id == apic_id) {
				return x2apic->acpi_id;
			}
			break;
		}
		default:
			break;
		}
	}
	PANIC("APIC ID %u not found", apic_id);
}
