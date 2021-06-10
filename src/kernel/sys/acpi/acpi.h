//! @file acpi.h
//! @brief File containing declarations for ACPI high level functions

#pragma once

#include <init/stivale2.h>
#include <lib/target.h>
#include <misc/attributes.h>
#include <misc/types.h>

//! @brief ACPI SDT tables header
struct acpi_sdt_header {
	char signature[4];
	uint32_t length;
	uint8_t rev;
	uint8_t checksum;
	char oemid[6];
	char oem_table_id[8];
	uint32_t oem_rev;
	uint32_t creator_id;
	uint32_t creator_rev;
} attribute_packed;

//! @brief SRAT ACPI table
struct acpi_srat {
	struct acpi_sdt_header hdr;
	char reserved[12];
} attribute_packed;

//! @brief SRAT entry type
enum {
	//! @brief SRAT xAPIC entry
	ACPI_SRAT_XAPIC_ENTRY = 0,
	//! @brief SRAT x2APIC entry
	ACPI_SRAT_X2APIC_ENTRY = 2,
	//! @brief SRAT memory entry
	ACPI_SRAT_MEM_ENTRY = 1,
};

//! @brief SRAT entry
struct acpi_srat_entry {
	uint8_t type;
	uint8_t length;
} attribute_packed;

//! @brief SRAT xAPIC affinity structure
struct acpi_srat_xapic_entry {
	struct acpi_srat_entry base;
	uint8_t domain_low;
	uint8_t apic_id;
	uint32_t flags;
	uint8_t sapic_eid;
	uint8_t domain_high[3];
	uint32_t clock_domain;
} attribute_packed;

//! @brief SRAT x2APIC affinity structure
struct acpi_srat_x2apic_entry {
	struct acpi_srat_entry base;
	uint8_t reserved[2];
	uint32_t domain;
	uint32_t apic_id;
	uint32_t flags;
	uint32_t clock_domain;
	uint8_t reserved2[4];
} attribute_packed;

//! @brief SRAT memory affinity structure
struct acpi_srat_mem_entry {
	struct acpi_srat_entry base;
	uint32_t domain;
	uint8_t reserved[2];
	uint32_t base_low;
	uint32_t base_high;
	uint32_t length_low;
	uint32_t length_high;
	uint8_t reserved2[4];
	uint32_t flags;
	uint8_t reserved3[8];
} attribute_packed;

//! @brief Nullable pointer to SRAT. Set by acpi_init if found
extern struct acpi_srat *acpi_boot_srat;

//! @brief MADT ACPI table
struct acpi_madt {
	struct acpi_sdt_header hdr;
	uint32_t lapic_addr;
	uint32_t pic_installed;
} attribute_packed;

enum {
	//! @brief MADT Processor xAPIC entry type
	ACPI_MADT_XAPIC_ENTRY = 0,
	//! @brief MADT Processor x2APIC entry type
	ACPI_MADT_X2APIC_ENTRY = 9,
	//! @brief MADT LAPIC address override
	ACPI_MADT_LAPIC_ADDR_OVERRIDE_ENTRY = 5,
};

//! @brief MADT entry
struct acpi_madt_entry {
	uint8_t type;
	uint8_t length;
} attribute_packed;

//! @brief MADT Processor xAPIC entry
struct acpi_madt_xapic_entry {
	struct acpi_madt_entry entry;
	uint8_t acpi_id;
	uint8_t apic_id;
	uint32_t flags;
} attribute_packed;

//! @brief MADT Processor x2APIC entry
struct acpi_madt_x2apic_entry {
	struct acpi_madt_entry entry;
	char reserved[2];
	uint32_t apic_id;
	uint32_t flags;
	uint32_t acpi_id;
} attribute_packed;

//! @brief ACPI LAPIC address override entry
struct acpi_madt_lapic_addr_override_entry {
	struct acpi_madt_entry entry;
	uint64_t override;
} attribute_packed;

//! @brief Nullable pointer to MADT. Set by acpi_init if found
extern struct acpi_madt *acpi_boot_madt;

//! @brief SLIT ACPI table
struct acpi_slit {
	struct acpi_sdt_header hdr;
	uint64_t localities;
	uint8_t lengths[];
} attribute_packed;

//! @brief Nullable pointer to SLIT. Set by acpi_init if found
extern struct acpi_slit *acpi_boot_slit;

//! @brief Validate ACPI table checksum
//! @param table ACPI table
//! @param len ACPI table length
bool acpi_validate_checksum(void *table, size_t len);

//! @brief Lookup APIC CPU id from ACPI CPU id
//! @param acpi_id ACPI CPU id
//! @return APIC CPU id
uint32_t acpi_acpi2apic_id(uint32_t acpi_id);

//! @brief Lookup ACPI CPI id from APIC CPI id
//! @param apic_id APIC CPU id
//! @return ACPI CPU id
uint32_t acpi_apic2acpi_id(uint32_t apic_id);

//! @brief Export ACPI init target
EXPORT_TARGET(acpi_target);
