//! @file tsc.h
//! @brief File containing TSC related functions

#pragma once

#include <misc/types.h>
#include <sys/cpuid.h>

//! @brief Read TSC counter
inline static uint64_t tsc_read(void) {
	uint32_t lo, hi;
	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

//! @brief Initiate TSC calibration process on this core
void tsc_begin_calibration(void);

//! @brief End TSC calibration process on this core
//! @note Should be called after THREAD_TRAMPOLINE_CALIBRATION_PERIOD ms have passed since
//! tsc_begin_calibration() call
void tsc_end_calibration(void);
