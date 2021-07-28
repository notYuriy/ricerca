//! @file topology.h
//! @brief File containing CPU Topology related declarations

#pragma once

#include <lib/target.h>
#include <thread/smp/core.h>

//! @brief CPU scheduling group
struct thread_smp_sched_group {
	//! @brief Next CPU scheduling group (should form a circular list)
	struct thread_smp_sched_group *next;
	//! @brief Number of CPUs in the scheduling group
	size_t cpu_count;
	//! @brief Number of tasks scheduled to the group
	size_t tasks_count;
	//! @brief Inline array of CPU ids
	uint32_t cpus[];
};

//! @brief CPU scheduling domain tree node
//! @note Populated for every CPU
struct thread_smp_sched_domain {
	//! @brief Parent domain
	struct thread_smp_sched_domain *parent;
	//! @brief CPU scheduling groups circular list
	//! @note As domain trees are populated for each CPU, `group` pointer in domain tree nodes for
	//! CPU N should point to the group which CPU N belongs to
	struct thread_smp_sched_group *group;
	//! @brief CPU timestamp at last rebalancing attempt
	uint64_t last_rebalance_tsc;
};

//! @brief Update groups load after inserting task on this CPU
//! @param id ID of the allocated CPU
void thread_smp_topology_update_on_insert(uint32_t id);

//! @brief Update groups load after removing task on this CPU
//! @param id ID of the allocated CPU
void thread_smp_topology_update_on_remove(uint32_t id);

//! @brief Export target for topology initialization
EXPORT_TARGET(thread_smp_topology_available)
