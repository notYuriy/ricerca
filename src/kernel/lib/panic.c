//! @file panic.c
//! @brief Implementation of abort functions

#include <lib/log.h>
#include <lib/panic.h>
#include <lib/symmap.h>
#include <lib/target.h>
#include <thread/smp/core.h>

//! @brief Maximum backtrace depth
#define PANIC_BACKTRACE_MAX 20

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
		log_printf_lockless("Panic originated from core %u\n\n", PER_CPU(logical_id));
	}
	log_printf_lockless("Stacktrace:\n");
	// Print stacktrace
	uintptr_t rbp = (uintptr_t)__builtin_frame_address(0);
	if (rbp != 0) {
		for (int i = 0; i < PANIC_BACKTRACE_MAX; ++i) {
			if (*(uintptr_t *)rbp == 0) {
				break;
			}
			uintptr_t addr = ((uintptr_t *)rbp)[1];
			struct symmap_addr_info info;
			if (symmap_query_addr_info(addr, &info)) {
				log_printf_lockless("* 0x%p <%s+0x%X>\n", addr, info.name, info.offset);
			} else {
				log_printf_lockless("* 0x%p\n", addr);
			}
			rbp = *(uintptr_t *)rbp;
		}
	}
	// Hang forever
	hang();
}
