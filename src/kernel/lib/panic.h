//! @file panic.h
//! @brief File containing declaration of the panic macro

#include <lib/log.h>
#include <misc/attributes.h>

//! @brief Hang forever
static inline attribute_noreturn void hang() {
	asm volatile("cli");
	while (true) {
		asm volatile("pause");
	}
}
#define FAIL_IMPL(...)                                                                             \
	do {                                                                                           \
		LOG_PANIC(__VA_ARGS__);                                                                    \
		hang();                                                                                    \
	} while (0)

// clang-format off
#ifdef DEBUG
#define ASSERT_IMPL(file, line, cond, template, ...)                                               \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			FAIL_IMPL("Assertion \"%s\" failed at %s:%U: " template, #cond, file, (size_t)line __VA_OPT__(,) __VA_ARGS__);\
		}                                                                                          \
	} while (0)
#else
#define ASSERT_IMPL(file, line, cond, template, ...) (void)(cond)
#endif
// clang-format on

#define PANIC_IMPL(file, line, template, ...)                                                      \
	do {                                                                                           \
		FAIL_IMPL("Panic at %s:%U: " template, file, (size_t)line __VA_OPT__(, ) __VA_ARGS__);     \
	} while (0)

//! @brief PANIC macro
//! @note Requires MODULE to be defined
#define PANIC(...) PANIC_IMPL(__FILE__, __LINE__, __VA_ARGS__)

//! @brief ASSERT macro
//! @param cond Condition to be asserted (should be == true)
//! @note Requires MODULE to be defined
#define ASSERT(cond, ...) ASSERT_IMPL(__FILE__, __LINE__, cond, __VA_ARGS__)

//! @brief TODO macro
#define TODO() FAIL_IMPL("TODO encountered at %s:%U", __FILE__, (size_t)__LINE__)

//! @brief UNREACHABLE macro
#define UNREACHABLE FAIL_IMPL("Unreachable reached at %s:%U", __FILE__, (size_t)__LINE__)
