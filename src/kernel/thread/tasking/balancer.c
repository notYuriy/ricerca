//! @file balancer.c
//! @brief Implementation of load balancing

#include <lib/panic.h>
#include <misc/atomics.h>
#include <thread/smp/core.h>
#include <thread/smp/topology.h>
#include <thread/tasking/balancer.h>
#include <thread/tasking/localsched.h>

MODULE("thread/tasking/balancer.c")
TARGET(thread_balancer_available, META_DUMMY,
       {thread_smp_core_available, thread_smp_topology_available, thread_localsched_available})
META_DEFINE_DUMMY()

//! @brief Find least busy core in CPU group
//! @param group Pointer to the group
//! @return ID of the least busy CPU
static uint32_t thread_balancer_least_busy_core(struct thread_smp_sched_group *group) {
	struct thread_smp_core *result = NULL;
	size_t result_load = 0;
	ASSERT(group->cpu_count > 0, "CPU groups should not be empty");
	for (size_t i = 0; i < group->cpu_count; ++i) {
		struct thread_smp_core *current = thread_smp_core_array + (group->cpus[i]);
		size_t current_load = ATOMIC_ACQUIRE_LOAD(&current->localsched.tasks_count);
		if (result == NULL || result_load > current_load) {
			result = current;
			result_load = current_load;
		}
	}
	ASSERT(result != NULL, "CPU groups should not be empty");
	return result->logical_id;
}

//! @brief Find least busy CPU group in domain
//! @param domain Pointer to the domain
//! @return Pointer to the least busy group
static struct thread_smp_sched_group *thread_balancer_least_busy_group(
    struct thread_smp_sched_domain *domain) {
	struct thread_smp_sched_group *result = NULL;
	size_t result_load = 0;
	struct thread_smp_sched_group *root = domain->group, *current = domain->group;
	do {
		size_t current_load = ATOMIC_ACQUIRE_LOAD(&current->tasks_count);
		if (result == NULL || result_load > current_load) {
			result = current;
			result_load = current_load;
		}
		current = current->next;
	} while (current != root);
	return result;
}

//! @brief Run task on any core
//! @param task Pointer to the task
void thread_balancer_allocate_to_any(struct thread_task *task) {
	struct thread_smp_sched_domain *root = PER_CPU(root);
	struct thread_smp_sched_group *group = thread_balancer_least_busy_group(root);
	uint32_t id = thread_balancer_least_busy_core(group);
	thread_localsched_associate(id, task);
}
