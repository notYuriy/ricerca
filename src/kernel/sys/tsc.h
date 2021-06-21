//! @file tsc.h
//! @brief File containing TSC related functions

#pragma once

#include <misc/types.h>
#include <sys/cpuid.h>

//! @brief Read TSC counter
inline static uint64_t tsc_read() {
	uint32_t lo, hi;
	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

//! @brief Check if invariant TSC is supported
//! @note If TSC is not supported at all, this function will return false
static inline bool tsc_is_invariant_supported() {
	struct cpuid buf;
	// Check that highest CPUID function is at least 0x80000007
	cpuid(0x80000000, 0, &buf);
	if (buf.eax < 0x80000007) {
		return false;
	}
	// Check if TSC is supported
	cpuid(0x80000001, 0, &buf);
	if ((buf.edx & (1 << 4)) != 0) {
		return false;
	}
	// Check if invariant TSC is supported
	cpuid(0x80000008, 0, &buf);
	return (buf.edx & (1 << 8)) != 0;
}
