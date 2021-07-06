//! @file interrupts.h
//! @brief File containing interrupt descriptor table functions

#pragma once

#include <lib/target.h>
#include <mem/rc.h>
#include <misc/types.h>

//! @brief Number of interrupt vectors
#define INTERRUPT_VECTORS_MAX 256

//! @brief Interrupt frame
struct interrupt_frame {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;
	uint64_t intno;
	uint64_t errcode;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

//! @brief Interrupt callback type
//! @param frame Interrupt frame
//! @param intno Interrupt number
typedef void (*interrupt_callback_t)(struct interrupt_frame *frame, void *ctx);

//! @brief Initialize IDT on this core
void idt_init(void);

//! @brief Create and register interrupt handler
//! @param intno Interrupt vector
//! @param callback Interrupt callback
//! @param ctx Opaque pointer
//! @param dpl Maximum privelege level
//! @param ist IST value
//! @param noints True if ints should be disabled on entry
void interrupt_register_handler(uint8_t intno, interrupt_callback_t callback, void *ctx,
                                uint8_t dpl, uint8_t ist, bool noints);

//! @brief Target for IDT initialization
EXPORT_TARGET(idt_available);
