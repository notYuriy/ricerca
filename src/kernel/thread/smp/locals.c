//! @file locals.c
//! @brief File containing implementations of functions related to CPU-local storage

#include <lib/panic.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <sys/acpi/acpi.h>
#include <sys/acpi/numa.h>
#include <sys/lapic.h>
#include <sys/msr.h>
#include <thread/smp/locals.h>

MODULE("thread/smp/locals")
TARGET(thread_smp_locals_target, thread_smp_locals_init,
       {acpi_target, acpi_numa_target, mem_phys_target, mem_misc_collect_info_target})

//! @brief Pointer to CPU local data structures
static struct thread_smp_locals *thread_smp_locals_array = NULL;

//! @brief CPU local storage MSR
#define KERNEL_GS_BASE 0xC0000102

//! @brief Read raw address of CPU local storage
static uintptr_t thread_smp_locals_get_raw(void) {
	return rdmsr(KERNEL_GS_BASE);
}

//! @brief Write raw address of CPU local storage
//! @param addr New address
static void thread_smp_locals_set_raw(uintptr_t addr) {
	wrmsr(KERNEL_GS_BASE, addr);
}

//! @brief Get pointer to CPU local storage
//! @return Pointer to the thread_smp_locals data structure
struct thread_smp_locals *thread_smp_locals_get(void) {
	return (struct thread_smp_locals *)thread_smp_locals_get_raw();
}

//! @brief Initialize CPU local storage for a given ACPI ID
//! @note To be used on AP bootup
void thread_smp_locals_set(void) {
	// Get APIC ID
	uint32_t apic_id = lapic_get_apic_id();
	// Query logical id
	uint32_t logical_id = acpi_madt_convert_ids(ACPI_MADT_LAPIC_PROP_APIC_ID,
	                                            ACPI_MADT_LAPIC_PROP_LOGICAL_ID, apic_id);
	// Set pointer to local storage
	thread_smp_locals_set_raw((uintptr_t)(thread_smp_locals_array + logical_id));
}

//! @brief Initialize thread-local storage
void thread_smp_locals_init(void) {
	// Get maximum CPUs count
	uint32_t count = acpi_get_max_cpus();
	// Allocate memory for CPU-local data
	uintptr_t backing_physmem = mem_phys_perm_alloc_on_behalf(
	    sizeof(struct thread_smp_locals) * count, acpi_numa_boot_domain);
	if (backing_physmem == PHYS_NULL) {
		PANIC("Failed to allocate memory for CPU locals");
	}
	thread_smp_locals_array = (struct thread_smp_locals *)(mem_wb_phys_win_base + backing_physmem);
	LOG_INFO("CPU-local structures allocated at %p", thread_smp_locals_array);
	// Set thread-local regs on BSP
	thread_smp_locals_set();
	thread_smp_locals_get()->numa_id = acpi_numa_boot_domain;
}
