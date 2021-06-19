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

//! @brief Data passed to the trampoline
struct thread_smp_trampoline_args {
	//! @brief CR3
	uint64_t cr3;
	//! @brief CPU locals addr
	uint64_t cpu_locals;
	//! @brief CPU locals size
	uint64_t cpu_locals_size;
	//! @brief Max cpus
	uint64_t max_cpus;
	//! @brief thread_smp_trampoline_ap_init location
	uint64_t callback;
};

//! @brief Physical address of trampoline args
#define THREAD_SMP_TRAMPOLINE_ARGS_PHYS 0x70000

//! @brief Long mode trampoline code
//! @param logical_id Logical ID of the CPU
static void thread_smp_trampoline_ap_init(uint32_t logical_id) {
	thread_smp_locals_init_on_ap(logical_id);
	// If kernel gave up on us, hang
	struct thread_smp_locals *locals = thread_smp_locals_get();
	if (ATOMIC_ACQUIRE_LOAD(&locals->status) == THREAD_SMP_LOCALS_STATUS_GAVE_UP) {
		hang();
	}
	// Update status
	ATOMIC_RELEASE_STORE(&locals->status, THREAD_SMP_LOCALS_STATUS_WAITING);
	LOG_INFO("Hello from core %u", logical_id);
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
	// Assert that cr3 will be accessible from booted cores
	uint64_t cr3 = rdcr3();
	if ((cr3 & 0xffffffffULL) != cr3) {
		PANIC("CR3 won't be accessible from booted cores");
	}
	// Load trampoline args
	struct thread_smp_trampoline_args *args =
	    (struct thread_smp_trampoline_args *)(mem_wb_phys_win_base +
	                                          THREAD_SMP_TRAMPOLINE_ARGS_PHYS);
	args->cr3 = cr3;
	args->cpu_locals = (uint64_t)thread_smp_locals_array;
	args->cpu_locals_size = (uint64_t)sizeof(struct thread_smp_locals);
	args->max_cpus = (uint64_t)thread_smp_locals_max_cpus;
	args->callback = (uint64_t)thread_smp_trampoline_ap_init;
}
