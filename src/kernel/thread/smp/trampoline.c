//! @file trampoline.c
//! @brief File containing code for copying AP trampoline

#include <lib/panic.h>
#include <lib/string.h>
#include <mem/misc.h>
#include <misc/types.h>
#include <sys/cr.h>
#include <thread/smp/locals.h>
#include <thread/smp/trampoline.h>

MODULE("thread/smp/trampoline")
TARGET(thread_smp_trampoline_available, thread_smp_trampoline_init,
       {mem_misc_collect_info_available, thread_smp_locals_available})

//! @brief Trampoline code start symbol
extern char thread_smp_trampoline_code_start[1];

//! @brief Trampoline code end symbol
extern char thread_smp_trampoline_code_end[1];

//! @brief Trampoline code max size
#define THREAD_SMP_TRAMPOLINE_MAX_SIZE 0x7000

//! @brief CR3 pointer physical location
#define THREAD_SMP_TRAMPOLINE_CR3_PPHYS 0x70998

//! @brief Long mode trampoline code
void thread_smp_trampoline_ap_init() {
	LOG_INFO("Booted");
	hang();
}

//! @brief Initialize SMP trampoline
void thread_smp_trampoline_init() {
	// Copy trampoline code
	uintptr_t code_start = (uintptr_t)thread_smp_trampoline_code_start;
	uintptr_t code_end = (uintptr_t)thread_smp_trampoline_code_end;
	size_t code_size = code_end - code_start;
	if (code_size > THREAD_SMP_TRAMPOLINE_MAX_SIZE) {
		PANIC("Trampoline code is too big");
	}
	void *code_src = (void *)(code_start);
	void *code_dst = (void *)(mem_wb_phys_win_base + THREAD_SMP_TRAMPOLINE_ADDR);
	memcpy(code_dst, code_src, code_size);
	// Write stivale2 cr3 to THREAD_SMP_TRAMPOLINE_CR3_PPHYS
	uintptr_t *cr3 = (uintptr_t *)(mem_wb_phys_win_base + THREAD_SMP_TRAMPOLINE_CR3_PPHYS);
	*cr3 = rdcr3();
}
