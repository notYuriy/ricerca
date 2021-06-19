//! @file tables.c
//! @brief File containing definitions of routines for initialization of amd64 tables

#include <lib/panic.h>
#include <lib/string.h>
#include <mem/heap/heap.h>
#include <sys/arch/arch.h>
#include <sys/arch/gdt.h>
#include <sys/arch/interrupts.h>
#include <thread/smp/locals.h>

MODULE("arch")
TARGET(arch_available, arch_bsp_init,
       {mem_heap_available, thread_smp_locals_available, idt_available})

//! @brief Preallocate arch state for the given core before bootup
//! @param logical_id Core logical id
//! @return True if allocation of core state succeeded
bool arch_prealloc(uint32_t logical_id) {
	struct thread_smp_locals *locals = thread_smp_locals_array + logical_id;
	// Allocate GDT and initialize it with template values
	locals->arch_state.gdt = mem_heap_alloc(sizeof(struct gdt));
	if (locals->arch_state.gdt == NULL) {
		return false;
	}
	return true;
}

//! @brief Initialize amd64 tables on this core
void arch_init(void) {
	// Initialize GDT
	struct thread_smp_locals *locals = thread_smp_locals_get();
	gdt_init(locals->arch_state.gdt);
	// Initialize IDT
	idt_init();
}

//! @brief Initialize amd64 tables on BSP
static void arch_bsp_init(void) {
	ASSERT(arch_prealloc(thread_smp_locals_get()->logical_id),
	       "Failed to allocate arch core state for BSP");
	arch_init();
}
