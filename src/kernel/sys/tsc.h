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
