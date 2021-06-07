//! @file acpi.h
//! @brief File containing declarations for ACPI high level functions

#pragma once

#include <init/stivale2.h> // For RSDP stivale2 tag

//! @brief Early ACPI subsystem init
void acpi_early_init(struct stivale2_struct_tag_rsdp *rsdp_tag);
