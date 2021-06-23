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
enum
{
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

enum
{
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

//! @brief GAS address space types
enum
{
	//! @brief Memory mapped IO address space
	ACPI_GAS_MMIO_ADDRESS_SPACE = 0,
	//! @brief Port IO address space
	ACPI_GAS_PORT_IO_ADDRESS_SPACE = 1,
};

//! @brief FADT Generic address structure
struct acpi_fadt_gas {
	uint8_t address_space;
	uint8_t bit_width;
	uint8_t bit_offset;
	uint8_t access_size;
	uint64_t address;
} attribute_packed;

//! @brief FADT table
struct acpi_fadt {
	struct acpi_sdt_header hdr;
	uint32_t firmware_ctl;
	uint32_t dsdt;

	uint8_t reserved;

	uint8_t preffered_pm_profile;
	uint16_t sci_interrupt;
	uint32_t smi_cmd_port;
	uint8_t acpi_enable;
	uint8_t acpi_disable;
	uint8_t s4bios_req;
	uint8_t pstate_ctrl;
	uint32_t pm1a_ev_blk;
	uint32_t pm1b_ev_blk;
	uint32_t pm1a_ctrl_blk;
	uint32_t pm1b_ctrl_blk;
	uint32_t pm2_ctrl_blk;
	uint32_t pm_timer_blk;
	uint32_t gpe0_blk;
	uint32_t gpe1_blk;
	uint8_t pm1_ev_len;
	uint8_t pm1_ctrl_len;
	uint8_t pm2_ctrl_len;
	uint8_t pm_timer_len;
	uint8_t gpe0_len;
	uint8_t gpe1_len;
	uint8_t gpe1_base;
	uint8_t cstate_ctrl;
	uint16_t worst_c2_latency;
	uint16_t worst_c3_latency;
	uint16_t flush_size;
	uint16_t flush_stride;
	uint8_t duty_offset;
	uint8_t duty_width;
	uint8_t day_alarm;
	uint8_t month_alarm;
	uint8_t century;

	uint16_t boot_arch_flags;

	uint8_t reserved2;
	uint32_t flags;

	struct acpi_fadt_gas reset_reg;

	uint8_t reset_value;
	uint8_t reserved3[3];

	uint64_t firmware_ctl_ex;
	uint64_t dsdt_ex;

	struct acpi_fadt_gas pm1a_ev_blk_ex;
	struct acpi_fadt_gas pm1b_ev_blk_ex;
	struct acpi_fadt_gas pm1a_ctrl_blk_ex;
	struct acpi_fadt_gas pm1b_ctrl_blk_ex;
	struct acpi_fadt_gas pm2_ctrl_blk_ex;
	struct acpi_fadt_gas pm_timer_blk_ex;
	struct acpi_fadt_gas gpe0_blk_ex;
	struct acpi_fadt_gas gpe1_blk_ex;
} attribute_packed;

//! @brief Nullable pointer to FADT. Set by acpi_init if found
extern struct acpi_fadt *acpi_boot_fadt;

//! @brief DMAR table
struct acpi_dmar {
	struct acpi_sdt_header hdr;
	uint8_t host_addr_width;
	uint8_t flags;
	uint8_t reserved[10];
	uintptr_t remapping_structures;
} attribute_packed;

//! @brief Validate ACPI table checksum
//! @param table ACPI table
//! @param len ACPI table length
bool acpi_validate_checksum(void *table, size_t len);

//! @brief Find size of physical address space according to SRAT
//! @return Size of physical memory space
size_t acpi_query_phys_space_size(void);

//! @brief MADT lapic entry property
enum acpi_madt_lapic_entry_prop
{
	// Local ID
	ACPI_MADT_LAPIC_PROP_LOGICAL_ID = 0,
	// ACPI ID
	ACPI_MADT_LAPIC_PROP_ACPI_ID = 1,
	// APIC ID
	ACPI_MADT_LAPIC_PROP_APIC_ID = 2,
};

//! @brief Find MADT lapic entry that satisifies given criteria
//! @param matched Property of MADT lapic entries to match with expected
//! @param returned Property of MADT lapic entries to return
//! @param expected Expected value of matched property
//! @param ignore_disabled Ignore disabled MADT entries
//! @return Value of returned property
uint32_t acpi_madt_convert_ids(enum acpi_madt_lapic_entry_prop matched,
                               enum acpi_madt_lapic_entry_prop returned, uint32_t expected,
                               bool ignore_disabled);

//! @brief Search for the specific table
//! @param name Name of the table
//! @param index of the table
struct acpi_sdt_header *acpi_find_table(const char *name, size_t index);

//! @brief ACPI revision or 0 if ACPI is not supported
extern size_t acpi_revision;

//! @brief Export ACPI init target
EXPORT_TARGET(acpi_available);
