//! @file boot_bringup.c
//! @brief File containing code for bringing up APs on boot

#include <lib/log.h>
#include <sys/arch/arch.h>
#include <sys/ic.h>
#include <sys/timers/timer.h>
#include <sys/tsc.h>
#include <thread/smp/boot_bringup.h>
#include <thread/smp/core.h>
#include <thread/smp/trampoline.h>

MODULE("thread/smp/boot_bringup")
TARGET(thread_smp_ap_boot_bringup_available, thread_smp_ap_boot_bringup,
       {ic_bsp_available, thread_smp_core_available, timers_available,
        thread_smp_trampoline_available, arch_available})

//! @brief Bring up CPUs plugged on boot
void thread_smp_ap_boot_bringup() {
	// Preallocate arch state and send init IPIs
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		struct thread_smp_core *locals = thread_smp_core_array + i;
		if (locals->status == THREAD_SMP_CORE_STATUS_ASLEEP) {
			ATOMIC_RELEASE_STORE(&locals->status, THREAD_SMP_CORE_STATUS_WAKEUP_INITIATED);
			ic_send_init_ipi(locals->apic_id);
		}
	}
	// Wait 10 ms
	timer_busy_wait_ms(10);
	// Send startup IPIs
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		struct thread_smp_core *locals = thread_smp_core_array + i;
		if (locals->status == THREAD_SMP_CORE_STATUS_WAKEUP_INITIATED) {
			ic_send_startup_ipi(locals->apic_id, THREAD_SMP_TRAMPOLINE_ADDR);
		}
	}
	// Wait for 10 ms more
	timer_busy_wait_ms(10);
	// Check if CPUs have booted up
	bool everyone_started = true;
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		struct thread_smp_core *locals = thread_smp_core_array + i;
		uint64_t status = ATOMIC_ACQUIRE_LOAD(&(locals->status));
		if (status == THREAD_SMP_CORE_STATUS_WAKEUP_INITIATED) {
			// Resend startup IPI
			ic_send_startup_ipi(locals->apic_id, THREAD_SMP_TRAMPOLINE_ADDR);
			everyone_started = false;
		}
	}
	if (everyone_started) {
		goto calibration;
	}
	LOG_WARN("Failed to boot CPUs from the first SIPI round. Waiting for 100ms to give CPUs a "
	         "second chance");
	// Wait for 100 ms
	timer_busy_wait_ms(100);
	// Ok, now everyone should have started,
	everyone_started = true;
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		struct thread_smp_core *locals = thread_smp_core_array + i;
		uint64_t status = ATOMIC_ACQUIRE_LOAD(&locals->status);
		if (status == THREAD_SMP_CORE_STATUS_WAKEUP_INITIATED) {
			// Give up on this CPU, it takes too long to start
			ATOMIC_RELEASE_STORE(&locals->status, THREAD_SMP_CORE_STATUS_GAVE_UP);
			everyone_started = false;
		}
	}
	if (!everyone_started) {
		LOG_ERR("Some CPUS have not booted up, giving up on them");
	}
calibration:
	LOG_INFO("Calibration process initiated");
	// Begin TSC calibration process
	tsc_begin_calibration();
	// Begin IC timer calibration process
	ic_timer_start_calibration();
	// Signal APs to begin calibration
	ATOMIC_RELEASE_STORE(&thread_smp_trampoline_state, THREAD_SMP_TRAMPOLINE_BEGIN_CALIBRATION);
	// Wait for calibration period to end
	timer_busy_wait_ms(THREAD_TRAMPOLINE_CALIBRATION_PERIOD);
	// Signal APs to end calibration
	ATOMIC_RELEASE_STORE(&thread_smp_trampoline_state, THREAD_SMP_TRAMPOLINE_END_CALIBRATION);
	// End TSC calibration process
	tsc_end_calibration();
	// End IC timer calibration process
	ic_timer_end_calibration();
	LOG_INFO("Calibration process finished. BSP TSC frequency = %U KHz", PER_CPU(tsc_freq));
}
