//! @file mem.h
//! @brief File containing declaration of memory management init function

#pragma once

#include <init/stivale2.h> // For stivale2 memory map

//! @brief Initialize memory management
//! @param memmap Stivale2 memory map tag
void mem_init(struct stivale2_struct_tag_memmap *memmap);
