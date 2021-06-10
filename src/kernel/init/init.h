//! @file init.h
//! @brief File containing stivale2 loaded tags

#pragma once

#include <init/stivale2.h>

//! @brief RSDP tag or NULL if not found
extern struct stivale2_struct_tag_rsdp *init_rsdp_tag;

//! @brief Memory map tag or NULL if not found
extern struct stivale2_struct_tag_memmap *init_memmap_tag;
