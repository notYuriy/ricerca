//! @brief cpuid.h
//! @file Wrapper of assembly cpuid.h instruction

#pragma once

#include <misc/types.h>

//! @brief CPUID info
struct cpuid {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

//! @brief Run CPUID assembly instruction
//! @param leaf CPUID leaf
//! @param subleaf CPU subleaf
//! @param cpuid Buffer to store CPUID information
static inline void cpuid(uint32_t leaf, uint32_t subleaf, struct cpuid *cpuid) {
	asm volatile("cpuid"
	             : "=a"(cpuid->eax), "=b"(cpuid->ebx), "=c"(cpuid->ecx), "=d"(cpuid->edx)
	             : "a"(leaf), "c"(subleaf));
}
