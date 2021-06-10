//! @brief msr.h
//! @file File containing MSR functions

#pragma once

#include <misc/types.h>

//! @brief Read from MSR
//! @param msr MSR to read form
static inline uint64_t rdmsr(uint32_t msr) {
	uint64_t val;
	asm volatile("rdmsr" : "=A"(val) : "c"(msr));
	return val;
}

//! @brief Write to MSR
//! @param msr MSR to write to
static inline void wrmsr(uint32_t msr, uint64_t val) {
	asm volatile("wrmsr" : : "c"(msr), "A"(val));
}
