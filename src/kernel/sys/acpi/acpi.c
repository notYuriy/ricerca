//! @file log.c
//! @brief File containing definitions for ACPI high level functions

#include <lib/log.h>         // For logging
#include <lib/panic.h>       // For PANIC
#include <lib/string.h>      // For memcpy
#include <mem/misc.h>        // For HIGH_PHYS_VMA
#include <misc/attributes.h> // For packed
#include <sys/acpi/acpi.h>   // For declarations of functions used here

//! @brief Module name
#define MODULE "acpi"

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

//! @brief Walk RSDT
//! @param rsdt_phys Physical address of RSDT
void acpi_walk_rsdt(uint64_t rsdt_phys) {
	struct acpi_rsdt *rsdt = (struct acpi_rsdt *)(rsdt_phys + HIGH_PHYS_VMA);
	if (!acpi_validate_checksum(rsdt, rsdt->hdr.length)) {
		LOG_ERR("RSDT checksum validation failed");
	}

	const size_t tables_count = (rsdt->hdr.length - sizeof(struct acpi_sdt_header)) / 4;
	LOG_INFO("%U tables found in RSDT", tables_count);

	for (size_t i = 0; i < tables_count; ++i) {
		const uint64_t table_phys = (uint64_t)rsdt->tables[i];
		const struct acpi_sdt_header *header =
		    (struct acpi_sdt_header *)(table_phys + HIGH_PHYS_VMA);
		char name_buf[5] = {0};
		memcpy(name_buf, header->signature, 4);
		LOG_INFO("Table \"%s\" found in RSDT", name_buf);
	}
}

//! @brief Walk XSDT
//! @param rsdt_phys Physical address of XSDT
void acpi_walk_xsdt(uint64_t xsdt_phys) {
	struct acpi_xsdt *xsdt = (struct acpi_xsdt *)(xsdt_phys + HIGH_PHYS_VMA);
	if (!acpi_validate_checksum(xsdt, xsdt->hdr.length)) {
		LOG_ERR("XSDT checksum validation failed");
	}

	const size_t tables_count = (xsdt->hdr.length - sizeof(struct acpi_sdt_header)) / 8;
	LOG_INFO("%U tables found in XSDT", tables_count);

	for (size_t i = 0; i < tables_count; ++i) {
		const uint64_t table_phys = (uint64_t)xsdt->tables[i];
		const struct acpi_sdt_header *header =
		    (struct acpi_sdt_header *)(table_phys + HIGH_PHYS_VMA);
		char name_buf[5] = {0};
		memcpy(name_buf, header->signature, 4);
		LOG_INFO("Table \"%s\" found in XSDT", name_buf);
	}
}

//! @brief Early ACPI subsystem init
//! @param rsdp_tag Stivale2 RSDP tag
void acpi_early_init(struct stivale2_struct_tag_rsdp *rsdp_tag) {
	struct acpi_rsdp *rsdp = (struct acpi_rsdp *)rsdp_tag->rsdp;
	LOG_INFO("RSDP at %p", rsdp);

	if (!acpi_validate_checksum(rsdp, sizeof(struct acpi_rsdp))) {
		LOG_ERR("Legacy RSDP checksum validation failed");
	}

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
