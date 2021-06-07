//! @file acpi.h
//! @brief File containing declarations for ACPI high level functions

#pragma once

#include <init/stivale2.h>   // For RSDP stivale2 tag
#include <misc/attributes.h> // For packed
#include <misc/types.h>      // For uint_t types

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

//! @brief Validate ACPI table checksum
//! @param table ACPI table
//! @param len ACPI table length
bool acpi_validate_checksum(void *table, size_t len);

//! @brief Early ACPI subsystem init
//! @param rsdp_tag Stivale2 RSDP tag
void acpi_early_init(struct stivale2_struct_tag_rsdp *rsdp_tag);
