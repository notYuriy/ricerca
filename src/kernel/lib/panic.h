//! @file panic.h
//! @brief File containing declaration of the panic macro

#include <lib/log.h>

//! @brief Hang forever
static inline _Noreturn void hang() {
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

#ifdef DEBUG
//! @brief ASSERT macro
//! @note Requires MODULE to be defined
#define ASSERT(cond, ...)                                                                          \
	if (!(cond)) {                                                                                 \
		PANIC(__VA_ARGS__);                                                                        \
	}
#else
//! @brief ASSERT macro
//! @note Requires MODULE to be defined
#define ASSERT(cond, ...) (void)(cond);
#endif
