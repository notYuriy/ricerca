//! @file locals.h
//! @brief File declaring CPU-local data structures and functions to access them

#pragma once

#include <lib/target.h>
#include <sys/arch/arch.h>
#include <sys/ic.h>
#include <sys/numa/numa.h>
#include <thread/tasking/queue.h>

//! @brief Per-CPU stack size
#define THREAD_SMP_CORE_CPU_STACK_SIZE 0x10000

//! @brief CPU status
enum
{
	//! @brief CPU is asleep
	THREAD_SMP_CORE_STATUS_ASLEEP = 1,
	//! @brief Startup IPI was sent
	THREAD_SMP_CORE_STATUS_WAKEUP_INITIATED = 2,
	//! @brief CPU is online, waiting for tasks to run
	THREAD_SMP_CORE_STATUS_ONLINE = 3,
	//! @brief Gave up status
	THREAD_SMP_CORE_STATUS_GAVE_UP = 4,
};

//! @brief CPU-local area
struct thread_smp_core {
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
	// IC timer state
	struct ic_core_state ic_state;
	// Task queue
	struct thread_task_queue queue;
};

//! @brief Macro to access per-cpu data
#define PER_CPU(member) (thread_smp_core_get()->member)

//! @brief Pointer to CPU local data structures
extern struct thread_smp_core *thread_smp_core_array;

//! @brief Size of CPU local structures array
extern size_t thread_smp_core_max_cpus;

//! @brief Get pointer to CPU local storage
//! @return Pointer to the thread_smp_core data structure
struct thread_smp_core *thread_smp_core_get(void);

//! @brief Initialize CPU local storage on AP
//! @note To be used on AP bootup
//! @note Requires LAPIC to be enabled
//! @param logical_id Logical id of the current AP
void thread_smp_core_init_on_ap(uint32_t logical_id);

//! @brief Export target to initialize CPU local areas
EXPORT_TARGET(thread_smp_core_available)
