//! @file log.c
//! @brief File containing definitions for ACPI high level functions

#include <lib/log.h>       // For logging
#include <sys/acpi/acpi.h> // For declarations of functions used here

//! @brief Module name
#define MODULE "acpi"

void acpi_early_init(struct stivale2_struct_tag_rsdp *rsdp_tag) {
	uint64_t rsdp = rsdp_tag->rsdp;
	LOG_INFO("RSDP physical address: %p", rsdp);
}
