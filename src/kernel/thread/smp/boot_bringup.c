//! @file boot_bringup.c
//! @brief File containing code for bringing up APs on boot

#include <lib/log.h>
#include <sys/ic.h>
#include <sys/timers/timer.h>
#include <thread/smp/boot_bringup.h>
#include <thread/smp/locals.h>
#include <thread/smp/trampoline.h>

MODULE("thread/smp/boot_bringup")
TARGET(thread_smp_ap_boot_bringup_available, thread_smp_ap_boot_bringup,
       {ic_bsp_available, thread_smp_locals_available, timers_available,
        thread_smp_trampoline_available})

//! @brief Bring up CPUs plugged on boot
void thread_smp_ap_boot_bringup() {
	// Send init IPIs
	for (size_t i = 0; i < thread_smp_locals_max_cpus; ++i) {
		struct thread_smp_locals *locals = thread_smp_locals_array + i;
		if (locals->status == THREAD_SMP_LOCALS_STATUS_ASLEEP) {
			ic_send_init_ipi(locals->apic_id);
			timer_busy_wait_ms(10);
			ic_send_startup_ipi(locals->apic_id, THREAD_SMP_TRAMPOLINE_ADDR);
			timer_busy_wait_ms(10);
		}
	}
	/*
	timer_busy_wait_ms(10);
	// Send startup IPIs
	for (size_t i = 0; i < thread_smp_locals_max_cpus; ++i) {
	    struct thread_smp_locals *locals = thread_smp_locals_array + i;
	    if (locals->status == THREAD_SMP_LOCALS_STATUS_ASLEEP) {
	        ic_send_startup_ipi(locals->apic_id, THREAD_SMP_TRAMPOLINE_ADDR);
	    }
	}
	*/
}
