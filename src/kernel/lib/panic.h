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

// clang-format off
#ifdef DEBUG
#define ASSERT_IMPL(file, line, cond, template, ...)                                               \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			PANIC("Assertion failed (%s:%U): " template, file, (size_t)line __VA_OPT__(,) __VA_ARGS__);\
		}                                                                                          \
	} while (0);
#else
#define ASSERT_IMPL(file, line, cond, template, ...) (void)(cond);
#endif
// clang-format on

//! @brief ASSERT macro
//! @param cond Condition to be asserted (should be == true)
//! @note Requires MODULE to be defined
#define ASSERT(cond, ...) ASSERT_IMPL(__FILE__, __LINE__, cond, __VA_ARGS__)
