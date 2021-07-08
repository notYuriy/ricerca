//! @file intlevel.c
//! @brief File containing definitions for interrupts disable/enable functions

#include <misc/types.h>

//! @brief Disable interrupts
//! @return True if interrupts were enabled
bool intlevel_elevate(void) {
	// Get interrupt enable flag
	uint64_t flags;
	asm volatile("pushf; pop %0; cli" : "=g"(flags));
	return flags & (1 << 9);
}

//! @brief Enable interrupts if true is passed
//! @param status True if interrupts should be enabled
void intlevel_recover(bool status) {
	if (status) {
		asm volatile("sti");
	}
}
