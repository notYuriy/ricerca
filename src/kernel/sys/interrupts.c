//! @file interrupts.c
//! @brief File containing definitions for interrupts disable/enable functions

#include <misc/types.h> // For uint32_t

//! @brief Disable interrupts
//! @return True if interrupts were enabled
bool interrupts_disable() {
	// Get interrupt enable flag
	uint64_t flags;
	asm volatile("pushf; pop %0; cli" : "=g"(flags));
	return flags & (1 << 9);
}

//! @brief Enable interrupts if true is passed
//! @param status True if interrupts should be enabled
void interrupts_enable(bool status) {
	if (status) {
		asm volatile("sti");
	}
}
