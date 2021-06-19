//! @file gdt.h
//! @brief File containing global descriptor table functions

#pragma once

#include <misc/types.h>

//! @brief Number of GDT descriptors
#define GDT_DESCRIPTORS 9

//! @brief 64-bit code desciptor
#define GDT_CODE64 0x28

//! @brief 64-bit data desciptor
#define GDT_DATA64 0x30

struct gdt {
	uint64_t descrs[GDT_DESCRIPTORS];
};

//! @brief Initialize and load GDT
//! @param gdt Pointer to uninitialized GDT
void gdt_init(struct gdt *gdt);
