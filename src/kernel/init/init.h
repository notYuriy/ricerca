//! @file init.h
//! @brief File containing externs for info collected on early kernel init

#pragma once

#include <init/stivale2.h>

//! @brief RSDP tag or NULL if not found
extern struct stivale2_struct_tag_rsdp *init_rsdp_tag;

//! @brief Memory map tag or NULL if not found
extern struct stivale2_struct_tag_memmap *init_memmap_tag;

//! @brief Modules tag or NULL if not found
extern struct stivale2_struct_tag_modules *init_modules_tag;
