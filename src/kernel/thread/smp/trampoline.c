//! @file trampoline.c
//! @brief File containing code for copying AP trampoline

#include <lib/panic.h>
#include <lib/string.h>
#include <mem/misc.h>
#include <misc/attributes.h>
#include <misc/types.h>
#include <sys/cr.h>
#include <sys/ic.h>
#include <sys/tsc.h>
#include <thread/smp/core.h>
#include <thread/smp/trampoline.h>
#include <thread/tasking/localsched.h>

MODULE("thread/smp/trampoline")
TARGET(thread_smp_trampoline_available, thread_smp_trampoline_init,
       {mem_misc_collect_info_available, thread_smp_core_available})

//! @brief Trampoline code start symbol
extern char thread_smp_trampoline_code_start[1];

//! @brief Trampoline code end symbol
extern char thread_smp_trampoline_code_end[1];

//! @brief Trampoline state
enum thread_smp_trampoline_state thread_smp_trampoline_state;

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
//! @note attribute_no_instrument as CPU local storage is not enabled yet => profiling wont' work
//! properly
attribute_no_instrument static void thread_smp_trampoline_ap_init(uint32_t logical_id) {
	thread_smp_core_init_on_ap(logical_id);
	// If kernel gave up on us, hang
	struct thread_smp_core *locals = thread_smp_core_get();
	if (ATOMIC_ACQUIRE_LOAD(&locals->status) == THREAD_SMP_CORE_STATUS_GAVE_UP) {
		hang();
	}
	// Initialize architecture layer
	arch_init();
	// Update status
	ATOMIC_RELEASE_STORE(&locals->status, THREAD_SMP_CORE_WAITING_FOR_CALIBRATION);
	// Wait for trampoline status to update
	enum thread_smp_trampoline_state state;
	while ((state = ATOMIC_ACQUIRE_LOAD(&thread_smp_trampoline_state)) !=
	       THREAD_SMP_TRAMPOLINE_BEGIN_CALIBRATION) {
		if (state == THREAD_SMP_TRAMPOLINE_END_CALIBRATION) {
			LOG_WARN("CPU is late to calibration interval! Hanging");
			hang();
		}
		asm volatile("pause");
	}
	// Begin TSC calibration process
	tsc_begin_calibration();
	// Begin IC timer calibration process
	ic_timer_start_calibration();
	// Wait for trampoline status to update
	while (ATOMIC_ACQUIRE_LOAD(&thread_smp_trampoline_state) !=
	       THREAD_SMP_TRAMPOLINE_END_CALIBRATION) {
		asm volatile("pause");
	}
	// End TSC calibration process
	tsc_end_calibration();
	// End IC timer calibration process
	ic_timer_end_calibration();
	LOG_INFO("Hello from AP %u. Local TSC frequency is %U MHz", logical_id, PER_CPU(tsc_freq));
	// Initialize local scheduler
	thread_localsched_init();
	// Bootstrap local scheduler
	thread_localsched_bootstrap();
	UNREACHABLE;
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
	args->cpu_locals = (uint64_t)thread_smp_core_array;
	args->cpu_locals_size = (uint64_t)sizeof(struct thread_smp_core);
	args->max_cpus = (uint64_t)thread_smp_core_max_cpus;
	args->callback = (uint64_t)thread_smp_trampoline_ap_init;
}
