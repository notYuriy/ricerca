//! @file panic.c
//! @brief Implementation of abort functions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/target.h>
#include <thread/smp/core.h>

//! @brief Panic handler
//! @param fmt Panic message format string
//! @param ... Fmt arguments
attribute_noreturn void panic_handler(const char *subsystem, const char *fmt, ...) {
	// Prevent other messages from popping up
	log_lock();
	// Log panic message
	va_list args;
	va_start(args, fmt);
	log_valogf_lockless(LOG_TYPE_PANIC, subsystem, fmt, args);
	va_end(args);
	// Print CPU id if panic occured after SMP init
	if (TARGET_IS_REACHED(thread_smp_core_available)) {
		log_logf_lockless(LOG_TYPE_PANIC, subsystem, "Panic originated from core %u",
		                  PER_CPU(logical_id));
	}
	// Hang forever
	hang();
}
