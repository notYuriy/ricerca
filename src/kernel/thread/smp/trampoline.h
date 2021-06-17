//! @file trampoline.h
//! @brief File containing wrappers for accessing trampoline code

#pragma once

#include <lib/target.h>

#define THREAD_SMP_TRAMPOLINE_ADDR 0x71000

//! @brief Export trampoline copying target
EXPORT_TARGET(thread_smp_trampoline_available)
