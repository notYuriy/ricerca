//! @file tsc.c
//! @brief File containing TSC calibration functions implementation

#include <sys/tsc.h>
#include <thread/smp/core.h>
#include <thread/smp/trampoline.h>

//! @brief Initiate TSC calibration process on this core
void tsc_begin_calibration(void) {
	PER_CPU(tsc_freq) = tsc_read();
}

//! @brief End TSC calibration process on this core
//! @note Should be called after THREAD_TRAMPOLINE_CALIBRATION_PERIOD ms have passed since
//! tsc_begin_calibration() call
void tsc_end_calibration(void) {
	uint64_t passed = tsc_read() - PER_CPU(tsc_freq);
	PER_CPU(tsc_freq) = passed / THREAD_TRAMPOLINE_CALIBRATION_PERIOD;
}
