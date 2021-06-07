//! @file panic.h
//! @brief File containing declaration of the panic macro

#include <lib/log.h> // For LOG_* macros

//! @brief Hang forever
static inline void hang() {
	asm volatile("cli");
	while (true) {
		asm volatile("pause");
	}
}

//! @brief PANIC macro
//! @note Requires MODULE to be defined
#define PANIC(...)                                                                                 \
	do {                                                                                           \
		LOG_PANIC(__VA_ARGS__);                                                                    \
		hang();                                                                                    \
	} while (0)
