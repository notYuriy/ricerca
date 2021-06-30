//! @file trampoline.h
//! @brief File containing wrappers for accessing trampoline code

#pragma once

#include <lib/target.h>

//! @brief Physical address of the trampoline code
#define THREAD_SMP_TRAMPOLINE_ADDR 0x71000

//! @brief Trampoline calibration window in duration (in ms)
#define THREAD_TRAMPOLINE_CALIBRATION_PERIOD 200

//! @brief Trampoline state enum. Set by core that boots up APs and watched by cores that are booted
enum thread_smp_trampoline_state
{
	//! @brief Core should wait for trampoline state update
	THREAD_SMP_TRAMPOLINE_WAIT = 0,
	//! @brief Core should begin calibration process
	THREAD_SMP_TRAMPOLINE_BEGIN_CALIBRATION = 1,
	//! @brief Core should end calibration process
	THREAD_SMP_TRAMPOLINE_END_CALIBRATION = 2,
};

//! @brief Current trampoline state
extern enum thread_smp_trampoline_state thread_smp_trampoline_state;

//! @brief Export trampoline copying target
EXPORT_TARGET(thread_smp_trampoline_available)
