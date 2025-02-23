//! @file locals.c
//! @brief File containing implementations of functions related to CPU-local storage

#include <lib/panic.h>
#include <mem/misc.h>
#include <mem/phys/phys.h>
#include <misc/types.h>
#include <sys/acpi/acpi.h>
#include <sys/acpi/numa.h>
#include <sys/acpi/smp.h>
#include <sys/arch/arch.h>
#include <sys/ic.h>
#include <sys/msr.h>
#include <thread/smp/core.h>

MODULE("thread/smp/locals")
TARGET(thread_smp_core_available, thread_smp_core_init,
       {acpi_available, acpi_numa_available, mem_phys_available, mem_misc_collect_info_available,
        acpi_smp_available})

//! @brief Pointer to CPU local data structures
struct thread_smp_core *thread_smp_core_array = NULL;

//! @brief Size of CPU local structures array
size_t thread_smp_core_max_cpus;

//! @brief CPU GS hidden value MSR
#define IA32_GS_BASE 0xC0000101

//! @brief CPU kernel GS MSR
#define IA32_KERNEL_GS_BASE 0xC0000102

//! @brief Read raw address of the CPU local storage structure
static uintptr_t thread_smp_core_get_raw(void) {
	uintptr_t res;
	asm volatile("mov %%gs:0, %0" : "=R"(res));
	return res;
}

//! @brief Write raw address of the CPU local storage structure
//! @param addr New address
static void thread_smp_core_set_raw(uintptr_t addr) {
	wrmsr(IA32_GS_BASE, addr);
	// Write 0 to kernel GS msr to not expose address of cpu-local storage to userspace after swapgs
	wrmsr(IA32_KERNEL_GS_BASE, 0);
}

//! @brief Get pointer to CPU local storage
//! @return Pointer to the thread_smp_core data structure
struct thread_smp_core *thread_smp_core_get(void) {
	return (struct thread_smp_core *)thread_smp_core_get_raw();
}

//! @brief Get CPU locals of a CPU with a given logical ID
//! @return Pointer to the thread_smp_core structure
struct thread_smp_core *thread_smp_core_get_for(uint32_t id) {
	ASSERT(id < thread_smp_core_max_cpus,
	       "Attempt to access cpu local storage of CPU with invalid ID %u (max_cpus = %u)", id,
	       thread_smp_core_max_cpus);
	return thread_smp_core_array + id;
}

//! @brief Initialize CPU local storage on AP
//! @note To be used on AP bootup
//! @note Requires LAPIC to be enabled
//! @param logical_id Logical ID of the current AP
//! @note attribute_no_instrument as CPU local storage is not enabled yet => profiling wont' work
//! properly
attribute_no_instrument void thread_smp_core_init_on_ap(uint32_t logical_id) {
	// Get point to CPU local data
	struct thread_smp_core *this_cpu_data = thread_smp_core_array + logical_id;
	// Set pointer to local storage
	thread_smp_core_set_raw((uintptr_t)(this_cpu_data));
}

//! @brief Initialize thread-local storage
void thread_smp_core_init(void) {
	// Get maximum CPUs count
	thread_smp_core_max_cpus = acpi_smp_get_max_cpus();
	// Allocate memory for CPU-local data
	uintptr_t backing_physmem = mem_phys_alloc_on_behalf(
	    align_up(sizeof(struct thread_smp_core) * thread_smp_core_max_cpus, PAGE_SIZE),
	    acpi_numa_boot_domain);
	if (backing_physmem == PHYS_NULL) {
		PANIC("Failed to allocate memory for CPU locals");
	}
	thread_smp_core_array = (struct thread_smp_core *)(mem_wb_phys_win_base + backing_physmem);
	LOG_INFO("CPU-local structures allocated at %p", thread_smp_core_array);
	// Set asleep statuses
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		thread_smp_core_array[i].self = thread_smp_core_array + i;
		ATOMIC_RELEASE_STORE(&thread_smp_core_array[i].status, THREAD_SMP_CORE_STATUS_ASLEEP);
	}
	// Iterate over CPUs and initialize their stae
	struct acpi_smp_cpu_iterator iter = ACPI_SMP_CPU_ITERATOR_INIT;
	struct acpi_smp_cpu buf;
	while (acpi_smp_iterate_over_cpus(&iter, &buf)) {
		ASSERT(buf.logical_id < thread_smp_core_max_cpus, "CPU logical ID out of range");
		struct thread_smp_core *this_cpu_data = thread_smp_core_array + buf.logical_id;
		// Set IDs
		this_cpu_data->acpi_id = buf.acpi_id;
		this_cpu_data->apic_id = buf.apic_id;
		this_cpu_data->logical_id = buf.logical_id;
		this_cpu_data->numa_id = acpi_numa_apic2numa_id(this_cpu_data->apic_id);
		// Allocate CPU stacks
		uintptr_t interrupt_stack =
		    mem_phys_alloc_on_behalf(THREAD_SMP_CORE_CPU_STACK_SIZE, this_cpu_data->numa_id);
		uintptr_t scheduler_stack =
		    mem_phys_alloc_on_behalf(THREAD_SMP_CORE_CPU_STACK_SIZE, this_cpu_data->numa_id);
		if (interrupt_stack == PHYS_NULL || scheduler_stack == PHYS_NULL) {
			PANIC("Failed to allocate CPU stacks");
		}
		LOG_INFO("Core %u stacks at %p %p", buf.logical_id, interrupt_stack, scheduler_stack);
		this_cpu_data->interrupt_stack_top =
		    mem_wb_phys_win_base + interrupt_stack + THREAD_SMP_CORE_CPU_STACK_SIZE;
		this_cpu_data->scheduler_stack_top =
		    mem_wb_phys_win_base + scheduler_stack + THREAD_SMP_CORE_CPU_STACK_SIZE;
		// Allocate archittecture state
		if (!arch_prealloc(this_cpu_data->logical_id, this_cpu_data->numa_id)) {
			PANIC("Failed to allocate arch state for the CPU");
		}
	}
	// Get APIC ID
	uint32_t apic_id = ic_get_apic_id();
	// Query logical id
	uint32_t logical_id = acpi_madt_convert_ids(ACPI_MADT_LAPIC_PROP_APIC_ID,
	                                            ACPI_MADT_LAPIC_PROP_LOGICAL_ID, apic_id);
	// Initialize thread-local storage on BSP
	thread_smp_core_init_on_ap(logical_id);
	PER_CPU(status) = THREAD_SMP_CORE_STATUS_ONLINE;
	// Initalize tables on BSP
	arch_init();
}
