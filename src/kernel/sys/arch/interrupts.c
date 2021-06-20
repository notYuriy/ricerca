//! @file interrupts.c
//! @brief File containing implementations of interrupt descriptor table functions

#include <lib/panic.h>
#include <mem/heap/heap.h>
#include <misc/attributes.h>
#include <sys/arch/gdt.h>
#include <sys/arch/interrupts.h>
#include <sys/arch/tss.h>
#include <sys/cr.h>
#include <thread/spinlock.h>

MODULE("sys/arch/interrupts")
TARGET(idt_available, idt_fill, {})

//! @brief IDT descriptor
struct idt_descriptor {
	uint16_t offset_low;
	uint16_t segment_selector;
	uint16_t flags;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t reserved;
} attribute_packed;

//! @brief IDT descriptors array
struct idt_descriptor idt_descriptors[INTERRUPT_VECTORS_MAX];

//! @brief Array of interrupt callbacks
static interrupt_callback_t interrupt_callbacks[INTERRUPT_VECTORS_MAX] = {0};

//! @brief Array of contexts
static void *interrupt_contexts[INTERRUPT_VECTORS_MAX] = {0};

//! @brief Handle interrupt
void interrupt_handle(struct interrupt_frame *frame) {
	// If handle is registered, call it
	if (interrupt_callbacks[frame->intno] != NULL) {
		interrupt_callbacks[frame->intno](frame, interrupt_contexts[frame->intno]);
	} else {
		PANIC("Unhandled interrupt v=%U, e=0x%p, rip=0x%p, cr2=0x%p", frame->intno, frame->errcode,
		      frame->rip, rdcr2());
	}
}

//! @brief Encode IDT gate
//! @param descr Pointer to the IDT descriptor
//! @param callback Pointer to the callback
//! @param dpl Maximum privelege level
//! @param ist IST value
//! @param noints True if interrupts should be disabled
static void interrupt_encode_idt_gate(struct idt_descriptor *descr, interrupt_callback_t callback,
                                      uint8_t dpl, uint8_t ist, bool noints) {
	uint16_t flags = 0;
	flags |= (uint16_t)ist;
	flags |= ((uint16_t)(noints ? 0b1110 : 0b1111) << 8);
	flags |= (((uint16_t)dpl) << 13);
	flags |= ((uint16_t)1 << 15);

	descr->offset_low = (uint16_t)((uintptr_t)callback & 0xffffULL);
	descr->segment_selector = GDT_CODE64;
	descr->flags = flags;
	descr->offset_mid = (uint16_t)(((uintptr_t)callback >> 16) & 0xffffULL);
	descr->offset_high = (uint32_t)(((uintptr_t)callback >> 32) & 0xffffffffULL);
	descr->reserved = 0;
}

//! @brief Array of raw interrupt callbacks defined in sys/arch/isrs.asm
extern void *interrupt_raw_callbacks[INTERRUPT_VECTORS_MAX];

//! @brief Create and register interrupt handler
//! @param intno Interrupt vector
//! @param callback Interrupt callback
//! @param ctx Opaque pointer
//! @param dpl Maximum privelege level
//! @param ist IST value
//! @param noints True if ints should be disabled on entry
void interrupt_register_handler(uint8_t intno, interrupt_callback_t callback, void *ctx,
                                uint8_t dpl, uint8_t ist, bool noints) {
	ASSERT(interrupt_callbacks[intno] == NULL, "Attempt to register overlapping interrupt handler");
	interrupt_callbacks[intno] = callback;
	interrupt_contexts[intno] = ctx;
	interrupt_encode_idt_gate(idt_descriptors + intno, interrupt_raw_callbacks[intno], dpl, ist,
	                          noints);
}

static void idt_fill() {
	for (size_t i = 0; i < INTERRUPT_VECTORS_MAX; ++i) {
		interrupt_encode_idt_gate(idt_descriptors + i, interrupt_raw_callbacks[i], 0, 0, false);
	}
}

//! @brief Initialize IDT on this core
void idt_init(void) {
	struct {
		uint16_t length;
		uint64_t base;
	} attribute_packed idtr = {INTERRUPT_VECTORS_MAX * 8 - 1, (uint64_t)idt_descriptors};
	asm volatile("lidt %0" : : "m"(idtr));
}
