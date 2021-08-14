//! @file stkguard.c
//! @brief Implementation of stack smashing protector runtime

#include <lib/panic.h>
#include <lib/target.h>
#include <misc/types.h>

uintptr_t __stack_chk_guard = 0xaaaaaaaaaaaaaaaa;

MODULE("lib/stkguard");

__attribute__((noreturn)) void __stack_chk_fail(void) {
	PANIC("Stack smash protector failure");
}
