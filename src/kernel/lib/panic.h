//! @file panic.h
//! @brief File containing declaration of the panic macro

#pragma once

#include <lib/log.h>
#include <misc/attributes.h>

//! @brief Hang forever
static inline attribute_noreturn void hang() {
	asm volatile("cli");
	while (true) {
		asm volatile("pause");
	}
}

//! @brief Panic handler
//! @param subsystem Subsystem from which panic originates
//! @param fmt Panic message format string
//! @param ... Fmt arguments
attribute_noreturn void panic_handler(const char *subsystem, const char *fmt, ...);

#ifdef DEBUG
#define ASSERT_IMPL(file, line, cond, template, ...)                                               \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			panic_handler(module_name, "Assertion \"%s\" failed at %s:%U: " template, #cond, file, \
			              (size_t)line __VA_OPT__(, ) __VA_ARGS__);                                \
		}                                                                                          \
	} while (0)
#else
#define ASSERT_IMPL(file, line, cond, template, ...) (void)(cond)
#endif

#define PANIC_IMPL(file, line, template, ...)                                                      \
	do {                                                                                           \
		panic_handler(module_name, "Panic at %s:%U: " template, file,                              \
		              (size_t)line __VA_OPT__(, ) __VA_ARGS__);                                    \
	} while (0)

//! @brief PANIC macro
//! @note Requires MODULE to be defined
#define PANIC(...) PANIC_IMPL(__FILE__, __LINE__, __VA_ARGS__)

//! @brief ASSERT macro
//! @param cond Condition to be asserted (should be == true)
//! @note Requires MODULE to be defined
#define ASSERT(cond, ...) ASSERT_IMPL(__FILE__, __LINE__, cond, __VA_ARGS__)

//! @brief TODO macro
#define TODO() panic_handler(module_name, "TODO encountered at %s:%U", __FILE__, (size_t)__LINE__)

//! @brief UNREACHABLE macro
#define UNREACHABLE                                                                                \
	panic_handler(module_name, "Unreachable reached at %s:%U", __FILE__, (size_t)__LINE__)
