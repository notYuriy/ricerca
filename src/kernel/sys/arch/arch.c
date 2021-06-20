//! @file tables.c
//! @brief File containing definitions of routines for initialization of amd64 tables

#include <lib/panic.h>
#include <lib/string.h>
#include <mem/heap/heap.h>
#include <sys/arch/arch.h>
#include <sys/arch/gdt.h>
#include <sys/arch/interrupts.h>
#include <sys/ic.h>
#include <thread/smp/locals.h>

MODULE("arch")
TARGET(arch_available, arch_bsp_init,
       {mem_heap_available, thread_smp_locals_available, idt_available, ic_bsp_available})

//! @brief Preallocate arch state for the given core before bootup
//! @param logical_id Core logical id
//! @return True if allocation of core state succeeded
bool arch_prealloc(uint32_t logical_id) {
	struct thread_smp_locals *locals = thread_smp_locals_array + logical_id;
	// Allocate space for GDT
	locals->arch_state.gdt = mem_heap_alloc(sizeof(struct gdt));
	if (locals->arch_state.gdt == NULL) {
		return false;
	}
	// Allocate space for TSS
	locals->arch_state.tss = mem_heap_alloc(sizeof(struct tss));
	if (locals->arch_state.tss == NULL) {
		return false;
	}
	return true;
}

//! @brief Dummy interrupt vector
static void arch_dummy_int_vec(struct interrupt_frame *frame, void *ctx) {
	(void)frame;
	(void)ctx;
	ic_handle_spur_irq();
}

//! @brief Initialize amd64 tables on this core
void arch_init(void) {
	// Initialize GDT
	struct thread_smp_locals *locals = thread_smp_locals_get();
	gdt_init(locals->arch_state.gdt);
	// Initialize IDT
	idt_init();
	// Fill TSS
	tss_fill(locals->arch_state.tss);
	// Load TSS
	gdt_load_tss(locals->arch_state.gdt, locals->arch_state.tss);
	// Set CPU stacks
	tss_set_int_stack(locals->arch_state.tss, locals->interrupt_stack_top);
	tss_set_int_stack(locals->arch_state.tss, locals->scheduler_stack_top);
	// Register spurious interrupt vector
	interrupt_register_handler(ic_spur_vec, arch_dummy_int_vec, NULL, 0, TSS_INT_IST, true);
}

//! @brief Initialize amd64 tables on BSP
static void arch_bsp_init(void) {
	ASSERT(arch_prealloc(thread_smp_locals_get()->logical_id),
	       "Failed to allocate arch core state for BSP");
	arch_init();
}
