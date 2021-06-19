//! @file locals.h
//! @brief File declaring CPU-local data structures and functions to access them

#pragma once

#include <lib/target.h>
#include <sys/arch/arch.h>
#include <sys/numa/numa.h>

//! @brief Per-CPU stack size
#define THREAD_SMP_LOCALS_CPU_STACK_SIZE 0x10000

//! @brief CPU status
enum
{
	//! @brief CPU is not there
	THREAD_SMP_LOCALS_STATUS_NOT_PRESENT = 0,
	//! @brief CPU is asleep
	THREAD_SMP_LOCALS_STATUS_ASLEEP = 1,
	//! @brief Startup IPI was sent
	THREAD_SMP_LOCALS_STATUS_WAKEUP_INITIATED = 2,
	//! @brief CPU is online, waiting for tasks to run
	THREAD_SMP_LOCALS_STATUS_WAITING = 3,
	//! @brief CPU is online and runs a task
	THREAD_SMP_LOCALS_STATUS_RUNNING_TASK = 4,
	//! @brief CPU was put asleep forcefully
	THREAD_SMP_LOCALS_STATUS_PUT_ASLEEP = 5,
	//! @brief CPU boot timeouted, so kernel gave up on it
	THREAD_SMP_LOCALS_STATUS_GAVE_UP = 6,
};

//! @brief CPU-local area
struct thread_smp_locals {
	//! @brief Data accessible from SMP trampolie
	//! @note Keep in sync with thread/smp/trampoline.real
	struct {
		//! @brief NUMA ID
		numa_id_t numa_id;
		//! @brief ACPI ID
		uint32_t acpi_id;
		//! @brief APIC ID
		uint32_t apic_id;
		//! @brief Logical ID
		uint32_t logical_id;
		//! @brief CPU status
		uint64_t status;
		//! @brief Top of the interrupt stack for this CPU
		uintptr_t interrupt_stack_top;
		//! @brief Top of the scheduler stack for this CPU
		uintptr_t scheduler_stack_top;
	} attribute_packed;
	// Architecture-specific CPU state
	struct arch_core_state arch_state;
};

//! @brief Pointer to CPU local data structures
extern struct thread_smp_locals *thread_smp_locals_array;

//! @brief Size of CPU local structures array
extern size_t thread_smp_locals_max_cpus;

//! @brief Get pointer to CPU local storage
//! @return Pointer to the thread_smp_locals data structure
struct thread_smp_locals *thread_smp_locals_get(void);

//! @brief Initialize CPU local storage on AP
//! @note To be used on AP bootup
//! @note Requires LAPIC to be enabled
//! @param logical_id Logical id of the current AP
void thread_smp_locals_init_on_ap(uint32_t logical_id);

//! @brief Export target to initialize CPU local areas
EXPORT_TARGET(thread_smp_locals_available)
