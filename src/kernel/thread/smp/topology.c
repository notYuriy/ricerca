//! @file topology.c
//! @brief File containing code to build CPU topology

#include <lib/log.h>
#include <lib/panic.h>
#include <mem/heap/heap.h>
#include <thread/smp/core.h>
#include <thread/smp/topology.h>

MODULE("thread/smp/topology")
TARGET(thread_smp_topology_available, thread_smp_build_topology,
       {thread_smp_core_available, mem_heap_available})

static void thread_smp_build_topology_flat(void) {
	// Build CPU groups ring
	struct thread_smp_sched_group *root = NULL, *tail = NULL;
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		struct thread_smp_sched_group *group =
		    mem_heap_alloc(sizeof(struct thread_smp_sched_group) + 4);
		if (group == NULL) {
			PANIC("Failed to allocate CPU group");
		}
		group->cpus[0] = i;
		group->cpu_count = 1;
		group->tasks_count = 0;
		if (root == NULL) {
			root = group;
			group->next = group;
			tail = group;
		} else {
			tail->next = group;
			tail = group;
			group->next = root;
		}
	}
	// Build flat CPU domains
	struct thread_smp_sched_group *group = root;
	for (size_t i = 0; i < thread_smp_core_max_cpus; ++i) {
		struct thread_smp_sched_domain *domain =
		    mem_heap_alloc(sizeof(struct thread_smp_sched_domain));
		if (domain == NULL) {
			PANIC("Failed to allocate CPU group");
		}
		domain->group = group;
		domain->last_rebalance_tsc = 0;
		domain->parent = NULL;
		group = group->next;
		thread_smp_core_array[i].domain = thread_smp_core_array[i].root = domain;
	}
	// Account for BSP task
	thread_smp_topology_update_on_insert(PER_CPU(logical_id));
}

//! @brief Update groups load after inserting task on this CPU
//! @param id ID of the allocated CPU
void thread_smp_topology_update_on_insert(uint32_t id) {
	struct thread_smp_sched_domain *domain = thread_smp_core_array[id].domain;
	while (domain != NULL) {
		ATOMIC_FETCH_INCREMENT(&domain->group->tasks_count);
		domain = domain->parent;
	}
}

//! @brief Update groups load after removing task on this CPU
//! @param id ID of the allocated CPU
void thread_smp_topology_update_on_remove(uint32_t id) {
	struct thread_smp_sched_domain *domain = thread_smp_core_array[id].domain;
	while (domain != NULL) {
		ATOMIC_FETCH_DECREMENT(&domain->group->tasks_count);
		domain = domain->parent;
	}
}

static void thread_smp_build_topology(void) {
	thread_smp_build_topology_flat();
}
