//! @brief msr.h
//! @file File containing MSR functions

#pragma once

#include <misc/types.h>

//! @brief Read from MSR
//! @param msr MSR to read form
static inline uint64_t rdmsr(uint32_t msr) {
	uint32_t low, high;
	asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (((uint64_t)high) << 32ULL) | (uint64_t)low;
}

//! @brief Write to MSR
//! @param msr MSR to write to
//! @param val Value to write
static inline void wrmsr(uint32_t msr, uint64_t val) {
	uint32_t low = val & 0xffffffff;
	uint32_t high = val >> 32;
	asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}
