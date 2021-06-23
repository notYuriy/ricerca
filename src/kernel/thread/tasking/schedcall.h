//! @file schedcall.h
//! @brief File containing declarations of functions for running code on scheduler stack

#pragma once

#include <lib/target.h>
#include <sys/arch/interrupts.h>

//! @brief Call routine on scheduler stack
//! @param func Pointer to the function to be called
//! @param arg Pointer to the function argument
void thread_sched_call(interrupt_callback_t func, void *arg);

//! @brief Initialize schedule call subsystem
EXPORT_TARGET(thread_sched_call_available)
